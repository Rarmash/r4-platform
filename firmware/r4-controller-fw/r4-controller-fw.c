#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/r4_controller.h"
#include "core/r4_display.h"
#include "core/r4_framebuffer_transport.h"
#include "core/r4_protocol.h"
#include "core/r4_service_buttons.h"
#include "pico/stdlib.h"
#include "rgb_led.h"
#include "rp2040_input.h"
#include "tusb.h"
#include "version.h"

#ifndef R4_ENABLE_TEST_INPUT
#define R4_ENABLE_TEST_INPUT 0
#endif

#define PIN_RGB_LED 16
#define CDC_COMMAND_BUFFER_SIZE \
    (R4_PROTOCOL_MAX_LINE_LENGTH + 1U)
#define CDC_WRITE_TIMEOUT_MS 1000

static char cdc_command_buffer[CDC_COMMAND_BUFFER_SIZE];
static size_t cdc_command_length;
static bool cdc_discarding_line;

static r4_rp2040_input_t rp2040_input;
static r4_input_backend_t input_backend;
static r4_controller_state_t controller_state;
static r4_service_buttons_t service_buttons;
static r4_display_model_t display_model;
static uint8_t framebuffer_pixels[
    R4_OLED_PROFILE_WIDTH * R4_OLED_PROFILE_HEIGHT
];
static uint8_t framebuffer_packed[R4_FRAMEBUFFER_PACKED_SIZE];
static uint32_t framebuffer_snapshot_id;
static bool framebuffer_snapshot_valid;

#if R4_ENABLE_TEST_INPUT
static bool test_triggers_active;
static uint16_t test_left_trigger;
static uint16_t test_right_trigger;
static bool test_service_buttons_active;
#endif

static uint8_t led_base_red;
static uint8_t led_base_green;
static uint8_t led_base_blue;
static uint8_t led_output_red;
static uint8_t led_output_green;
static uint8_t led_output_blue;
static bool led_flash_active;
static absolute_time_t led_flash_deadline;

static bool is_valid_color_component(int value) {
    return value >= 0 && value <= 255;
}

static void apply_led_output(
    uint8_t red,
    uint8_t green,
    uint8_t blue
) {
    led_output_red = red;
    led_output_green = green;
    led_output_blue = blue;
    rgb_led_set(red, green, blue);
}

static void set_base_led(
    uint8_t red,
    uint8_t green,
    uint8_t blue
) {
    led_base_red = red;
    led_base_green = green;
    led_base_blue = blue;

    if (!led_flash_active) {
        apply_led_output(red, green, blue);
    }
}

static void start_led_flash(
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    uint32_t duration_ms
) {
    led_flash_active = true;
    led_flash_deadline = make_timeout_time_ms(duration_ms);
    apply_led_output(red, green, blue);
}

static void led_task(void) {
    if (
        led_flash_active &&
        time_reached(led_flash_deadline)
    ) {
        led_flash_active = false;
        apply_led_output(
            led_base_red,
            led_base_green,
            led_base_blue
        );
    }
}

static void update_controller_state(void) {
    if (!r4_input_backend_poll(&input_backend, &controller_state)) {
        r4_controller_state_reset(&controller_state);
    }

#if R4_ENABLE_TEST_INPUT
    if (test_triggers_active) {
        controller_state.left_trigger = test_left_trigger;
        controller_state.right_trigger = test_right_trigger;
        controller_state.left_trigger_status = R4_ANALOG_OK;
        controller_state.right_trigger_status = R4_ANALOG_OK;
    }
#endif

#if R4_ENABLE_TEST_INPUT
    if (test_service_buttons_active) {
        return;
    }
#endif

    const bool r4_pressed =
        (controller_state.buttons & R4_BUTTON_R4) != 0;
    const bool r4_chord_active =
        r4_pressed &&
        (
            (
                controller_state.buttons &
                ~R4_BUTTON_R4
            ) != 0 ||
            controller_state.hat != R4_HAT_CENTERED
        );

    r4_service_buttons_update_with_chord(
        &service_buttons,
        R4_SERVICE_BUTTON_R4,
        r4_pressed,
        r4_chord_active,
        to_ms_since_boot(get_absolute_time())
    );
}

static void send_gamepad_report(void) {
    update_controller_state();

    if (!tud_hid_ready()) {
        return;
    }

    r4_hid_report_t report;
    r4_build_hid_report(&controller_state, &report);
    tud_hid_report(0, &report, sizeof(report));
}

uint16_t tud_hid_get_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t *buffer,
    uint16_t requested_length
) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)requested_length;
    return 0;
}

void tud_hid_set_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t const *buffer,
    uint16_t buffer_size
) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)buffer_size;
}

static bool cdc_write_all(
    const void *data,
    size_t length
) {
    const uint8_t *bytes = data;
    size_t offset = 0;
    const absolute_time_t deadline =
        make_timeout_time_ms(CDC_WRITE_TIMEOUT_MS);

    while (offset < length) {
        const uint32_t available =
            tud_cdc_write_available();

        if (available == 0) {
            tud_cdc_write_flush();
            tud_task();

            if (time_reached(deadline)) {
                return false;
            }

            tight_loop_contents();
            continue;
        }

        const size_t remaining = length - offset;
        const uint32_t chunk_size =
            remaining < available
                ? (uint32_t)remaining
                : available;
        const uint32_t written =
            tud_cdc_write(bytes + offset, chunk_size);

        offset += written;
        tud_cdc_write_flush();
        tud_task();

        if (written == 0 && time_reached(deadline)) {
            return false;
        }
    }

    tud_cdc_write_flush();
    return true;
}

static void cdc_write_line(const char *text) {
    if (text == NULL) {
        return;
    }

    const size_t text_length = strlen(text);

    if (!cdc_write_all(text, text_length)) {
        return;
    }

    static const char line_ending[] = "\r\n";
    cdc_write_all(line_ending, sizeof(line_ending) - 1U);
}

static void copy_decoded_field(
    const char *payload,
    const char *key,
    char *target,
    size_t target_capacity
) {
    char encoded[R4_PROTOCOL_PAYLOAD_CAPACITY];

    if (
        r4_protocol_find_field(
            payload,
            key,
            encoded,
            sizeof(encoded)
        )
    ) {
        r4_protocol_decode_hex(
            encoded,
            target,
            target_capacity
        );
    }
}

static void process_host_state(const char *payload) {
    char mode[24];

    if (
        !r4_protocol_find_field(
            payload,
            "MODE",
            mode,
            sizeof(mode)
        )
    ) {
        cdc_write_line("ERR HOST_STATE_USAGE");
        return;
    }

    if (strcmp(mode, "BOOT") == 0) {
        display_model.screen = R4_DISPLAY_BOOT;
    } else if (strcmp(mode, "WAITING") == 0) {
        display_model.screen = R4_DISPLAY_WAITING;
    } else if (strcmp(mode, "HOME") == 0) {
        display_model.screen = R4_DISPLAY_HOME;
    } else if (strcmp(mode, "DIAGNOSTIC") == 0) {
        display_model.screen = R4_DISPLAY_DIAGNOSTIC;
    } else if (strcmp(mode, "ERROR") == 0) {
        display_model.screen = R4_DISPLAY_ERROR;
    } else {
        cdc_write_line("ERR HOST_STATE_VALUE");
        return;
    }

    display_model.orange_pi_connected = true;
    copy_decoded_field(
        payload,
        "TEXT_HEX",
        display_model.diagnostic,
        sizeof(display_model.diagnostic)
    );
    copy_decoded_field(
        payload,
        "ERROR_HEX",
        display_model.error,
        sizeof(display_model.error)
    );
    cdc_write_line("OK HOST STATE");
}

static void process_host_game(const char *payload) {
    char action[16];

    if (
        !r4_protocol_find_field(
            payload,
            "ACTION",
            action,
            sizeof(action)
        )
    ) {
        cdc_write_line("ERR HOST_GAME_USAGE");
        return;
    }

    if (strcmp(action, "START") == 0) {
        display_model.replay_buffering = false;
        copy_decoded_field(
            payload,
            "SYSTEM_HEX",
            display_model.system,
            sizeof(display_model.system)
        );
        copy_decoded_field(
            payload,
            "GAME_HEX",
            display_model.game,
            sizeof(display_model.game)
        );
        display_model.screen = R4_DISPLAY_GAME;
        r4_display_start_game(
            &display_model,
            to_ms_since_boot(get_absolute_time())
        );
    } else if (strcmp(action, "STOP") == 0) {
        display_model.screen = R4_DISPLAY_HOME;
        display_model.replay_buffering = false;
        display_model.system[0] = '\0';
        display_model.game[0] = '\0';
        r4_display_stop_game(&display_model);
    } else {
        cdc_write_line("ERR HOST_GAME_VALUE");
        return;
    }

    display_model.orange_pi_connected = true;
    cdc_write_line("OK HOST GAME");
}

static void process_host_ra(const char *payload) {
    char active[8];

    if (
        !r4_protocol_find_field(
            payload,
            "ACTIVE",
            active,
            sizeof(active)
        ) ||
        (
            strcmp(active, "0") != 0 &&
            strcmp(active, "1") != 0
        )
    ) {
        cdc_write_line("ERR HOST_RA_USAGE");
        return;
    }

    display_model.retroachievements_active =
        strcmp(active, "1") == 0;
    cdc_write_line("OK HOST RA");
}

static void process_host_achievement(const char *payload) {
    copy_decoded_field(
        payload,
        "TITLE_HEX",
        display_model.achievement,
        sizeof(display_model.achievement)
    );
    r4_display_show_achievement(
        &display_model,
        to_ms_since_boot(get_absolute_time())
    );
    cdc_write_line("OK HOST ACHIEVEMENT");
}

static void process_host_capture(const char *payload) {
    char status[16];
    char type[16] = "SCREENSHOT";
    const char *notification;

    if (
        r4_protocol_find_field(
            payload,
            "TYPE",
            type,
            sizeof(type)
        ) &&
        strcmp(type, "SCREENSHOT") != 0 &&
        strcmp(type, "CLIP") != 0
    ) {
        cdc_write_line("ERR HOST_CAPTURE_VALUE");
        return;
    }

    if (
        !r4_protocol_find_field(
            payload,
            "STATUS",
            status,
            sizeof(status)
        )
    ) {
        cdc_write_line("ERR HOST_CAPTURE_USAGE");
        return;
    }

    if (strcmp(type, "SCREENSHOT") == 0) {
        if (strcmp(status, "BUSY") == 0) {
            notification = "CAPTURING";
        } else if (strcmp(status, "SAVED") == 0) {
            notification = "CAPTURE SAVED";
        } else if (strcmp(status, "ERROR") == 0) {
            notification = "CAPTURE ERROR";
        } else {
            cdc_write_line("ERR HOST_CAPTURE_VALUE");
            return;
        }
    } else {
        if (strcmp(status, "BUFFERING") == 0) {
            display_model.replay_buffering = true;
            cdc_write_line("OK HOST CAPTURE");
            return;
        } else if (strcmp(status, "SAVING") == 0) {
            notification = "CLIP SAVING";
        } else if (strcmp(status, "SAVED") == 0) {
            notification = "CLIP SAVED";
        } else if (strcmp(status, "ERROR") == 0) {
            display_model.replay_buffering = false;
            notification = "CLIP ERROR";
        } else if (strcmp(status, "UNAVAILABLE") == 0) {
            display_model.replay_buffering = false;
            notification = "CLIP UNAVAILABLE";
        } else {
            cdc_write_line("ERR HOST_CAPTURE_VALUE");
            return;
        }
    }

    r4_display_show_notification(
        &display_model,
        notification,
        to_ms_since_boot(get_absolute_time())
    );
    cdc_write_line("OK HOST CAPTURE");
}

static void process_host_card(const char *payload) {
    char state[16];
    char notification[24];

    if (
        !r4_protocol_find_field(
            payload,
            "STATE",
            state,
            sizeof(state)
        )
    ) {
        cdc_write_line("ERR HOST_CARD_USAGE");
        return;
    }

    if (
        strcmp(state, "INSERTED") != 0 &&
        strcmp(state, "READY") != 0 &&
        strcmp(state, "BUSY") != 0 &&
        strcmp(state, "EJECTED") != 0 &&
        strcmp(state, "ERROR") != 0
    ) {
        cdc_write_line("ERR HOST_CARD_VALUE");
        return;
    }

    snprintf(
        display_model.card_state,
        sizeof(display_model.card_state),
        "%s",
        state
    );
    snprintf(
        notification,
        sizeof(notification),
        "CARD %s",
        state
    );
    r4_display_show_notification(
        &display_model,
        notification,
        to_ms_since_boot(get_absolute_time())
    );
    cdc_write_line("OK HOST CARD");
}

static void process_host_telemetry(const char *payload) {
    char value[16];
    char *value_end;

    if (
        r4_protocol_find_field(
            payload,
            "BATTERY",
            value,
            sizeof(value)
        )
    ) {
        const long battery = strtol(value, &value_end, 10);

        if (
            value_end != value &&
            *value_end == '\0' &&
            battery >= 0 &&
            battery <= 100
        ) {
            display_model.battery_percent = (uint8_t)battery;
            display_model.battery_available = true;
        } else if (strcmp(value, "NA") == 0) {
            display_model.battery_available = false;
        } else {
            cdc_write_line("ERR HOST_TELEMETRY_BATTERY");
            return;
        }
    }

    if (
        r4_protocol_find_field(
            payload,
            "RUNTIME_MIN",
            value,
            sizeof(value)
        )
    ) {
        const long runtime_minutes = strtol(value, &value_end, 10);

        if (
            value_end != value &&
            *value_end == '\0' &&
            runtime_minutes >= 0 &&
            runtime_minutes <= 5999
        ) {
            display_model.remaining_runtime_minutes =
                (uint32_t)runtime_minutes;
            display_model.remaining_runtime_available = true;
        } else if (strcmp(value, "NA") == 0) {
            display_model.remaining_runtime_available = false;
        } else {
            cdc_write_line("ERR HOST_TELEMETRY_RUNTIME");
            return;
        }
    }

    if (
        r4_protocol_find_field(
            payload,
            "VOLUME",
            value,
            sizeof(value)
        )
    ) {
        const long volume = strtol(value, &value_end, 10);

        if (
            value_end != value &&
            *value_end == '\0' &&
            volume >= 0 &&
            volume <= 100
        ) {
            display_model.volume_percent = (uint8_t)volume;
            display_model.volume_available = true;
        } else if (strcmp(value, "NA") == 0) {
            display_model.volume_available = false;
        } else {
            cdc_write_line("ERR HOST_TELEMETRY_VOLUME");
            return;
        }
    }

    if (
        r4_protocol_find_field(
            payload,
            "POWER",
            value,
            sizeof(value)
        )
    ) {
        if (strcmp(value, "EXTERNAL") == 0) {
            display_model.external_power = true;
        } else if (strcmp(value, "BATTERY") == 0) {
            display_model.external_power = false;
        } else {
            cdc_write_line("ERR HOST_TELEMETRY_POWER");
            return;
        }
    }

    if (
        r4_protocol_find_field(
            payload,
            "NETWORK",
            value,
            sizeof(value)
        )
    ) {
        if (strcmp(value, "UP") == 0) {
            display_model.network_connected = true;
        } else if (strcmp(value, "DOWN") == 0) {
            display_model.network_connected = false;
        } else {
            cdc_write_line("ERR HOST_TELEMETRY_NETWORK");
            return;
        }
    }

    if (
        r4_protocol_find_field(
            payload,
            "TEMP_MILLIC",
            value,
            sizeof(value)
        )
    ) {
        const long temperature = strtol(value, &value_end, 10);

        if (
            value_end != value &&
            *value_end == '\0' &&
            temperature >= INT32_MIN &&
            temperature <= INT32_MAX
        ) {
            display_model.temperature_millicelsius =
                (int32_t)temperature;
            display_model.temperature_available = true;
        } else if (strcmp(value, "NA") == 0) {
            display_model.temperature_available = false;
        } else {
            cdc_write_line("ERR HOST_TELEMETRY_TEMPERATURE");
            return;
        }
    }

    copy_decoded_field(
        payload,
        "TIME_HEX",
        display_model.time,
        sizeof(display_model.time)
    );
    cdc_write_line("OK HOST TELEMETRY");
}

static void process_test_triggers(const r4_command_t *command) {
#if R4_ENABLE_TEST_INPUT
    if (
        command->values[0] < 0 ||
        command->values[0] > UINT16_MAX ||
        command->values[1] < 0 ||
        command->values[1] > UINT16_MAX
    ) {
        cdc_write_line("ERR TEST_TRIGGER_RANGE");
        return;
    }

    test_left_trigger = (uint16_t)command->values[0];
    test_right_trigger = (uint16_t)command->values[1];
    test_triggers_active = true;
    cdc_write_line("OK TEST TRIGGERS");
#else
    (void)command;
    cdc_write_line("ERR TEST_MODE_DISABLED");
#endif
}

#if R4_ENABLE_TEST_INPUT
static bool parse_test_button(
    const char *payload,
    r4_service_button_id_t *button,
    bool *pressed,
    uint32_t *timestamp_ms
) {
    char button_name[16];
    char action[8];
    char timestamp[16];

    if (
        !r4_protocol_find_field(
            payload,
            "BUTTON",
            button_name,
            sizeof(button_name)
        ) ||
        !r4_protocol_find_field(
            payload,
            "ACTION",
            action,
            sizeof(action)
        ) ||
        !r4_protocol_find_field(
            payload,
            "TIME_MS",
            timestamp,
            sizeof(timestamp)
        )
    ) {
        return false;
    }

    if (strcmp(button_name, "CAPTURE") == 0) {
        *button = R4_SERVICE_BUTTON_CAPTURE;
    } else if (strcmp(button_name, "R4") == 0) {
        *button = R4_SERVICE_BUTTON_R4;
    } else if (strcmp(button_name, "TROPHY") == 0) {
        *button = R4_SERVICE_BUTTON_TROPHY;
    } else {
        return false;
    }

    if (strcmp(action, "DOWN") == 0) {
        *pressed = true;
    } else if (strcmp(action, "UP") == 0) {
        *pressed = false;
    } else if (strcmp(action, "TICK") == 0) {
        *pressed = false;
    } else {
        return false;
    }

    *timestamp_ms = (uint32_t)strtoul(timestamp, NULL, 10);
    return true;
}
#endif

static void process_test_button(const r4_command_t *command) {
#if R4_ENABLE_TEST_INPUT
    r4_service_button_id_t button;
    bool pressed;
    uint32_t timestamp_ms;

    if (
        !parse_test_button(
            command->payload,
            &button,
            &pressed,
            &timestamp_ms
        )
    ) {
        cdc_write_line("ERR TEST_BUTTON_USAGE");
        return;
    }

    test_service_buttons_active = true;

    char action[8];
    r4_protocol_find_field(
        command->payload,
        "ACTION",
        action,
        sizeof(action)
    );

    if (strcmp(action, "TICK") != 0) {
        r4_service_buttons_update(
            &service_buttons,
            button,
            pressed,
            timestamp_ms
        );
    }

    r4_service_buttons_tick(&service_buttons, timestamp_ms);
    cdc_write_line("OK TEST BUTTON");
#else
    (void)command;
    cdc_write_line("ERR TEST_MODE_DISABLED");
#endif
}

static bool parse_uint32_field(
    const char *payload,
    const char *key,
    uint32_t *result
) {
    char value[16];

    if (
        result == NULL ||
        !r4_protocol_find_field(
            payload,
            key,
            value,
            sizeof(value)
        ) ||
        value[0] == '\0'
    ) {
        return false;
    }

    uint32_t parsed = 0;

    for (size_t index = 0; value[index] != '\0'; ++index) {
        if (value[index] < '0' || value[index] > '9') {
            return false;
        }

        const uint32_t digit = (uint32_t)(value[index] - '0');

        if (parsed > (UINT32_MAX - digit) / 10U) {
            return false;
        }

        parsed = parsed * 10U + digit;
    }

    *result = parsed;
    return true;
}

static bool capture_framebuffer(
    char *response,
    size_t response_capacity
) {
    r4_framebuffer_t framebuffer = {
        .width = R4_OLED_PROFILE_WIDTH,
        .height = R4_OLED_PROFILE_HEIGHT,
        .pixels = framebuffer_pixels,
        .pixel_capacity = sizeof(framebuffer_pixels)
    };

    if (
        !r4_display_render(&display_model, &framebuffer) ||
        !r4_framebuffer_pack_mono1_msb(
            framebuffer_pixels,
            sizeof(framebuffer_pixels),
            framebuffer_packed,
            sizeof(framebuffer_packed)
        )
    ) {
        framebuffer_snapshot_valid = false;
        return false;
    }

    ++framebuffer_snapshot_id;

    if (framebuffer_snapshot_id == 0) {
        ++framebuffer_snapshot_id;
    }

    framebuffer_snapshot_valid = true;
    return r4_framebuffer_format_info(
        response,
        response_capacity,
        framebuffer_snapshot_id,
        r4_display_hash(&framebuffer)
    );
}

static void process_framebuffer_chunk(
    const char *payload,
    char *response,
    size_t response_capacity
) {
    uint32_t snapshot_id;
    uint32_t offset;
    uint32_t length;

    if (
        !parse_uint32_field(payload, "ID", &snapshot_id) ||
        !parse_uint32_field(payload, "OFFSET", &offset) ||
        !parse_uint32_field(payload, "LENGTH", &length)
    ) {
        cdc_write_line("ERR FRAMEBUFFER_USAGE");
        return;
    }

    if (!framebuffer_snapshot_valid) {
        cdc_write_line("ERR FRAMEBUFFER_NOT_READY");
        return;
    }

    if (snapshot_id != framebuffer_snapshot_id) {
        cdc_write_line("ERR FRAMEBUFFER_STALE");
        return;
    }

    if (
        length == 0 ||
        length > R4_FRAMEBUFFER_CHUNK_MAX ||
        offset > sizeof(framebuffer_packed) ||
        length > sizeof(framebuffer_packed) - offset
    ) {
        cdc_write_line("ERR FRAMEBUFFER_RANGE");
        return;
    }

    if (
        !r4_framebuffer_format_chunk(
            response,
            response_capacity,
            snapshot_id,
            offset,
            framebuffer_packed,
            sizeof(framebuffer_packed),
            length
        )
    ) {
        cdc_write_line("ERR RESPONSE_TOO_LONG");
        return;
    }

    cdc_write_line(response);
}

static void process_parsed_command(
    const r4_command_t *command
) {
    char response[384];

    switch (command->type) {
        case R4_COMMAND_PING:
            cdc_write_line("PONG");
            break;

        case R4_COMMAND_VERSION:
            cdc_write_line(
                "R4_CONTROLLER_FW " R4_FIRMWARE_VERSION
            );
            break;

        case R4_COMMAND_INPUT:
            if (
                r4_protocol_format_input(
                    response,
                    sizeof(response),
                    &controller_state
                )
            ) {
                cdc_write_line(response);
            } else {
                cdc_write_line("ERR RESPONSE_TOO_LONG");
            }
            break;

        case R4_COMMAND_STATUS:
            if (
                r4_protocol_format_status(
                    response,
                    sizeof(response),
                    R4_FIRMWARE_VERSION,
                    led_output_red,
                    led_output_green,
                    led_output_blue,
                    led_base_red,
                    led_base_green,
                    led_base_blue,
                    led_flash_active,
                    &controller_state
                )
            ) {
                cdc_write_line(response);
            } else {
                cdc_write_line("ERR RESPONSE_TOO_LONG");
            }
            break;

        case R4_COMMAND_LED_OFF:
            led_base_red = 0;
            led_base_green = 0;
            led_base_blue = 0;
            led_flash_active = false;
            apply_led_output(0, 0, 0);
            cdc_write_line("OK LED OFF");
            break;

        case R4_COMMAND_LED_SET:
            if (
                !is_valid_color_component(command->values[0]) ||
                !is_valid_color_component(command->values[1]) ||
                !is_valid_color_component(command->values[2])
            ) {
                cdc_write_line("ERR LED_RANGE");
                break;
            }

            set_base_led(
                (uint8_t)command->values[0],
                (uint8_t)command->values[1],
                (uint8_t)command->values[2]
            );
            snprintf(
                response,
                sizeof(response),
                "OK LED %d %d %d",
                command->values[0],
                command->values[1],
                command->values[2]
            );
            cdc_write_line(response);
            break;

        case R4_COMMAND_LED_FLASH:
            if (
                !is_valid_color_component(command->values[0]) ||
                !is_valid_color_component(command->values[1]) ||
                !is_valid_color_component(command->values[2])
            ) {
                cdc_write_line("ERR LED_RANGE");
                break;
            }

            if (
                command->values[3] < 1 ||
                command->values[3] > 10000
            ) {
                cdc_write_line("ERR LED_DURATION_RANGE");
                break;
            }

            start_led_flash(
                (uint8_t)command->values[0],
                (uint8_t)command->values[1],
                (uint8_t)command->values[2],
                (uint32_t)command->values[3]
            );
            snprintf(
                response,
                sizeof(response),
                "OK LED FLASH %d %d %d %d",
                command->values[0],
                command->values[1],
                command->values[2],
                command->values[3]
            );
            cdc_write_line(response);
            break;

        case R4_COMMAND_EVENT_NEXT: {
            r4_service_event_t event;

            if (!r4_service_buttons_pop(&service_buttons, &event)) {
                cdc_write_line("EVENT NONE");
            } else if (
                r4_protocol_format_event(
                    response,
                    sizeof(response),
                    &event
                )
            ) {
                cdc_write_line(response);
            } else {
                cdc_write_line("ERR RESPONSE_TOO_LONG");
            }
            break;
        }

        case R4_COMMAND_FRAMEBUFFER_INFO:
            if (
                capture_framebuffer(
                    response,
                    sizeof(response)
                )
            ) {
                cdc_write_line(response);
            } else {
                cdc_write_line("ERR FRAMEBUFFER_RENDER");
            }
            break;

        case R4_COMMAND_FRAMEBUFFER_CHUNK:
            process_framebuffer_chunk(
                command->payload,
                response,
                sizeof(response)
            );
            break;

        case R4_COMMAND_HOST_HEARTBEAT:
            r4_display_arm_host_watchdog(
                &display_model,
                to_ms_since_boot(get_absolute_time())
            );
            cdc_write_line("OK HOST HEARTBEAT");
            break;

        case R4_COMMAND_HOST_STATE:
            r4_display_host_activity(
                &display_model,
                to_ms_since_boot(get_absolute_time())
            );
            process_host_state(command->payload);
            break;

        case R4_COMMAND_HOST_GAME:
            r4_display_host_activity(
                &display_model,
                to_ms_since_boot(get_absolute_time())
            );
            process_host_game(command->payload);
            break;

        case R4_COMMAND_HOST_RA:
            r4_display_host_activity(
                &display_model,
                to_ms_since_boot(get_absolute_time())
            );
            process_host_ra(command->payload);
            break;

        case R4_COMMAND_HOST_ACHIEVEMENT:
            r4_display_host_activity(
                &display_model,
                to_ms_since_boot(get_absolute_time())
            );
            process_host_achievement(command->payload);
            break;

        case R4_COMMAND_HOST_CAPTURE:
            r4_display_host_activity(
                &display_model,
                to_ms_since_boot(get_absolute_time())
            );
            process_host_capture(command->payload);
            break;

        case R4_COMMAND_HOST_CARD:
            r4_display_host_activity(
                &display_model,
                to_ms_since_boot(get_absolute_time())
            );
            process_host_card(command->payload);
            break;

        case R4_COMMAND_HOST_TELEMETRY:
            r4_display_host_activity(
                &display_model,
                to_ms_since_boot(get_absolute_time())
            );
            process_host_telemetry(command->payload);
            break;

        case R4_COMMAND_TEST_TRIGGERS:
            process_test_triggers(command);
            break;

        case R4_COMMAND_TEST_BUTTON:
            process_test_button(command);
            break;

        case R4_COMMAND_HELP:
            cdc_write_line(
                "COMMANDS PING VERSION INPUT STATUS "
                "LED <R> <G> <B> "
                "LED FLASH <R> <G> <B> <MS> "
                "LED OFF EVENT NEXT FRAMEBUFFER INFO "
                "FRAMEBUFFER CHUNK ... HOST HEARTBEAT "
                "HOST CAPTURE TYPE=... STATUS=... "
                "HOST CARD STATE=... HELP"
            );
            break;

        case R4_COMMAND_INVALID:
        case R4_COMMAND_UNKNOWN:
            cdc_write_line("ERR UNKNOWN_COMMAND");
            break;
    }
}

static void process_cdc_command(void) {
    r4_command_t command;
    const r4_parse_status_t status =
        r4_protocol_parse(
            cdc_command_buffer,
            cdc_command_length,
            &command
        );

    if (status == R4_PARSE_OK) {
        process_parsed_command(&command);
    } else if (
        strncmp(cdc_command_buffer, "HOST CAPTURE", 12) == 0
    ) {
        cdc_write_line("ERR HOST_CAPTURE_USAGE");
    } else if (
        strncmp(cdc_command_buffer, "HOST CARD", 9) == 0
    ) {
        cdc_write_line("ERR HOST_CARD_USAGE");
    } else if (
        strncmp(cdc_command_buffer, "LED FLASH", 9) == 0
    ) {
        cdc_write_line("ERR LED_FLASH_USAGE");
    } else if (
        strncmp(cdc_command_buffer, "LED", 3) == 0
    ) {
        cdc_write_line("ERR LED_USAGE");
    } else if (status == R4_PARSE_TOO_LONG) {
        cdc_write_line("ERR LINE_TOO_LONG");
    } else {
        cdc_write_line("ERR UNKNOWN_COMMAND");
    }

    cdc_command_length = 0;
}

static void process_cdc_character(uint8_t character) {
    if (character == '\r' || character == '\n') {
        if (cdc_discarding_line) {
            cdc_discarding_line = false;
            cdc_command_length = 0;
            return;
        }

        if (cdc_command_length > 0) {
            process_cdc_command();
        }
        return;
    }

    if (cdc_discarding_line) {
        return;
    }

    if (character == '\b' || character == 0x7F) {
        if (cdc_command_length > 0) {
            --cdc_command_length;
        }
        return;
    }

    if (
        cdc_command_length <
        CDC_COMMAND_BUFFER_SIZE - 1U
    ) {
        cdc_command_buffer[cdc_command_length++] =
            (char)character;
        return;
    }

    cdc_command_length = 0;
    cdc_discarding_line = true;
    cdc_write_line("ERR LINE_TOO_LONG");
}

static void cdc_service_task(void) {
    while (tud_cdc_available()) {
        uint8_t input_buffer[64];
        const uint32_t received =
            tud_cdc_read(input_buffer, sizeof(input_buffer));

        for (uint32_t index = 0; index < received; ++index) {
            process_cdc_character(input_buffer[index]);
        }
    }
}

int main(void) {
    r4_controller_state_reset(&controller_state);
    r4_display_model_init(&display_model);
    snprintf(
        display_model.firmware_version,
        sizeof(display_model.firmware_version),
        "%s",
        R4_FIRMWARE_VERSION
    );
    r4_service_buttons_init(
        &service_buttons,
        (r4_service_button_config_t){
            .debounce_ms = 30,
            .long_press_ms = 700,
            .double_press_ms = 300
        }
    );

    r4_rp2040_input_init(&rp2040_input);
    input_backend = r4_rp2040_input_backend(&rp2040_input);

    rgb_led_init(PIN_RGB_LED);
    led_base_red = 0;
    led_base_green = 0;
    led_base_blue = 0;
    led_flash_active = false;

    apply_led_output(0, 0, 16);
    sleep_ms(200);
    apply_led_output(0, 0, 0);

    const tusb_rhport_init_t usb_configuration = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL
    };

    if (!tud_rhport_init(0, &usb_configuration)) {
        led_flash_active = false;
        set_base_led(16, 0, 0);

        while (true) {
            tight_loop_contents();
        }
    }

    uint32_t previous_report_time_ms = 0;

    while (true) {
        tud_task();
        cdc_service_task();
        led_task();

        const uint32_t current_time_ms =
            to_ms_since_boot(get_absolute_time());
        r4_service_buttons_tick(
            &service_buttons,
            current_time_ms
        );
        r4_display_tick(
            &display_model,
            current_time_ms
        );

        if (
            current_time_ms -
            previous_report_time_ms >= 5
        ) {
            previous_report_time_ms = current_time_ms;
            send_gamepad_report();
        }
    }
}
