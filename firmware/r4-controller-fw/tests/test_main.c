#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "r4_controller.h"
#include "r4_display.h"
#include "r4_framebuffer_transport.h"
#include "r4_input_source.h"
#include "r4_protocol.h"
#include "r4_service_buttons.h"

static int failures;
static int checks;

#define CHECK(condition) \
    do { \
        ++checks; \
        if (!(condition)) { \
            fprintf( \
                stderr, \
                "FAIL %s:%d: %s\n", \
                __FILE__, \
                __LINE__, \
                #condition \
            ); \
            ++failures; \
        } \
    } while (0)

static r4_analog_calibration_t normal_calibration(void) {
    return (r4_analog_calibration_t){
        .raw_min = 100,
        .raw_max = 1100,
        .dead_zone = 100,
        .inverted = false
    };
}

static r4_analog_sample_t sample(uint32_t value) {
    return (r4_analog_sample_t){
        .raw_value = value,
        .source_available = true,
        .adc_available = true
    };
}

static void test_analog_normalization(void) {
    r4_analog_calibration_t calibration = normal_calibration();

    CHECK(r4_normalize_analog(sample(0), calibration).value == 0);
    CHECK(r4_normalize_analog(sample(200), calibration).value == 0);
    CHECK(
        r4_normalize_analog(sample(1100), calibration).value ==
        UINT16_MAX
    );
    CHECK(
        r4_normalize_analog(sample(9999), calibration).value ==
        UINT16_MAX
    );

    const r4_analog_result_t midpoint =
        r4_normalize_analog(sample(650), calibration);
    CHECK(midpoint.status == R4_ANALOG_OK);
    CHECK(midpoint.value >= 32767 && midpoint.value <= 32768);

    calibration.inverted = true;
    CHECK(r4_normalize_analog(sample(1100), calibration).value == 0);
    CHECK(
        r4_normalize_analog(sample(100), calibration).value ==
        UINT16_MAX
    );

    calibration = normal_calibration();
    calibration.raw_min = calibration.raw_max;
    CHECK(
        r4_normalize_analog(sample(100), calibration).status ==
        R4_ANALOG_INVALID_CALIBRATION
    );

    calibration = normal_calibration();
    calibration.dead_zone = 1000;
    CHECK(
        r4_normalize_analog(sample(100), calibration).status ==
        R4_ANALOG_INVALID_CALIBRATION
    );

    r4_analog_sample_t unavailable = sample(500);
    unavailable.source_available = false;
    CHECK(
        r4_normalize_analog(unavailable, calibration).status ==
        R4_ANALOG_SOURCE_UNAVAILABLE
    );

    unavailable.source_available = true;
    unavailable.adc_available = false;
    CHECK(
        r4_normalize_analog(unavailable, calibration).status ==
        R4_ANALOG_ADC_UNAVAILABLE
    );
}

static void test_centered_axis(void) {
    CHECK(r4_map_centered_axis(2048, 2048, 4095, 100) == 0);
    CHECK(r4_map_centered_axis(2100, 2048, 4095, 100) == 0);
    CHECK(r4_map_centered_axis(4095, 2048, 4095, 100) == 127);
    CHECK(r4_map_centered_axis(0, 2048, 4095, 100) == -127);
}

static void test_hid_report(void) {
    r4_controller_state_t state;
    r4_controller_state_reset(&state);
    state.left_x = -127;
    state.left_y = 127;
    state.right_x = -64;
    state.right_y = 64;
    state.left_trigger = 0;
    state.right_trigger = UINT16_MAX;
    state.hat = R4_HAT_DOWN_RIGHT;
    state.buttons = R4_BUTTON_A | R4_BUTTON_R4;

    r4_hid_report_t report;
    r4_build_hid_report(&state, &report);

    CHECK(sizeof(report) == 11);
    CHECK(report.x == -127);
    CHECK(report.y == 127);
    CHECK(report.left_trigger == 0);
    CHECK(report.right_trigger == UINT8_MAX);
    CHECK(report.right_x == -64);
    CHECK(report.right_y == 64);
    CHECK(report.hat == R4_HAT_DOWN_RIGHT);
    CHECK(report.buttons == state.buttons);
}

typedef struct {
    uint32_t values[2];
    bool adc_available;
} mock_analog_context_t;

static r4_analog_sample_t mock_read_analog(
    void *context,
    uint16_t channel
) {
    mock_analog_context_t *mock = context;

    return (r4_analog_sample_t){
        .raw_value = channel < 2 ? mock->values[channel] : 0,
        .source_available = channel < 2,
        .adc_available = mock->adc_available
    };
}

static void test_input_sources(void) {
    const r4_input_source_t unavailable =
        r4_input_source_unavailable(R4_SOURCE_EXTERNAL_ADC);
    CHECK(
        r4_input_source_read_analog(&unavailable, 0)
            .source_available == false
    );

    mock_analog_context_t context = {
        .values = {123, 987},
        .adc_available = true
    };
    const r4_input_source_t mock = {
        .kind = R4_SOURCE_MOCK,
        .context = &context,
        .read_digital = NULL,
        .read_analog = mock_read_analog
    };

    CHECK(r4_input_source_read_analog(&mock, 0).raw_value == 123);
    CHECK(r4_input_source_read_analog(&mock, 1).raw_value == 987);
    CHECK(r4_input_source_read_analog(&mock, 2).source_available == false);
}

static void test_protocol_parser(void) {
    r4_command_t command;

    CHECK(
        r4_protocol_parse("PING", 4, &command) == R4_PARSE_OK
    );
    CHECK(command.type == R4_COMMAND_PING);

    const char led[] = "LED 1 2 3";
    CHECK(
        r4_protocol_parse(
            led,
            sizeof(led) - 1U,
            &command
        ) == R4_PARSE_OK
    );
    CHECK(command.type == R4_COMMAND_LED_SET);
    CHECK(command.values[0] == 1);
    CHECK(command.values[2] == 3);

    const char incomplete[] = "LED FLASH 1 2";
    CHECK(
        r4_protocol_parse(
            incomplete,
            sizeof(incomplete) - 1U,
            &command
        ) == R4_PARSE_INCOMPLETE
    );

    const char unknown[] = "FUTURE FIELD=1";
    CHECK(
        r4_protocol_parse(
            unknown,
            sizeof(unknown) - 1U,
            &command
        ) == R4_PARSE_UNKNOWN
    );
    CHECK(command.type == R4_COMMAND_UNKNOWN);

    char too_long[R4_PROTOCOL_MAX_LINE_LENGTH + 2U];
    memset(too_long, 'A', sizeof(too_long));
    CHECK(
        r4_protocol_parse(
            too_long,
            sizeof(too_long),
            &command
        ) == R4_PARSE_TOO_LONG
    );

    const char framebuffer_info[] = "FRAMEBUFFER INFO";
    CHECK(
        r4_protocol_parse(
            framebuffer_info,
            sizeof(framebuffer_info) - 1U,
            &command
        ) == R4_PARSE_OK
    );
    CHECK(command.type == R4_COMMAND_FRAMEBUFFER_INFO);

    const char framebuffer_chunk[] =
        "FRAMEBUFFER CHUNK ID=7 OFFSET=96 LENGTH=32";
    CHECK(
        r4_protocol_parse(
            framebuffer_chunk,
            sizeof(framebuffer_chunk) - 1U,
            &command
        ) == R4_PARSE_OK
    );
    CHECK(command.type == R4_COMMAND_FRAMEBUFFER_CHUNK);
    CHECK(strstr(command.payload, "ID=7") == command.payload);

    const char heartbeat[] = "HOST HEARTBEAT";
    CHECK(
        r4_protocol_parse(
            heartbeat,
            sizeof(heartbeat) - 1U,
            &command
        ) == R4_PARSE_OK
    );
    CHECK(command.type == R4_COMMAND_HOST_HEARTBEAT);

    const char host[] =
        "HOST GAME ACTION=START SYSTEM_HEX=4E4553 "
        "GAME_HEX=4D4152494F EXTRA=ignored";
    CHECK(
        r4_protocol_parse(
            host,
            sizeof(host) - 1U,
            &command
        ) == R4_PARSE_OK
    );
    CHECK(command.type == R4_COMMAND_HOST_GAME);

    const char capture[] = "HOST CAPTURE STATUS=SAVED";
    CHECK(
        r4_protocol_parse(
            capture,
            sizeof(capture) - 1U,
            &command
        ) == R4_PARSE_OK
    );
    CHECK(command.type == R4_COMMAND_HOST_CAPTURE);
    CHECK(strcmp(command.payload, "STATUS=SAVED") == 0);
    const char clip_saving[] =
        "HOST CAPTURE TYPE=CLIP STATUS=SAVING";
    CHECK(
        r4_protocol_parse(
            clip_saving,
            sizeof(clip_saving) - 1U,
            &command
        ) == R4_PARSE_OK
    );
    CHECK(command.type == R4_COMMAND_HOST_CAPTURE);
    CHECK(
        strcmp(
            command.payload,
            "TYPE=CLIP STATUS=SAVING"
        ) == 0
    );
    const char clip_unavailable[] =
        "HOST CAPTURE TYPE=CLIP STATUS=UNAVAILABLE";
    CHECK(
        r4_protocol_parse(
            clip_unavailable,
            sizeof(clip_unavailable) - 1U,
            &command
        ) == R4_PARSE_OK
    );
    const char invalid_capture[] = "HOST CAPTURE STATUS=DONE";
    CHECK(
        r4_protocol_parse(
            invalid_capture,
            sizeof(invalid_capture) - 1U,
            &command
        ) == R4_PARSE_INVALID_VALUE
    );
    const char invalid_clip[] =
        "HOST CAPTURE TYPE=CLIP STATUS=BUSY";
    CHECK(
        r4_protocol_parse(
            invalid_clip,
            sizeof(invalid_clip) - 1U,
            &command
        ) == R4_PARSE_INVALID_VALUE
    );

    const char card[] = "HOST CARD STATE=READY";
    CHECK(
        r4_protocol_parse(
            card,
            sizeof(card) - 1U,
            &command
        ) == R4_PARSE_OK
    );
    CHECK(command.type == R4_COMMAND_HOST_CARD);
    CHECK(strcmp(command.payload, "STATE=READY") == 0);
    const char invalid_card[] = "HOST CARD STATE=ABSENT";
    CHECK(
        r4_protocol_parse(
            invalid_card,
            sizeof(invalid_card) - 1U,
            &command
        ) == R4_PARSE_INVALID_VALUE
    );

    CHECK(
        r4_protocol_parse(
            host,
            sizeof(host) - 1U,
            &command
        ) == R4_PARSE_OK
    );

    char field[32];
    CHECK(
        r4_protocol_find_field(
            command.payload,
            "GAME_HEX",
            field,
            sizeof(field)
        )
    );
    CHECK(strcmp(field, "4D4152494F") == 0);

    char decoded[16];
    CHECK(
        r4_protocol_decode_hex(
            field,
            decoded,
            sizeof(decoded)
        )
    );
    CHECK(strcmp(decoded, "MARIO") == 0);
    CHECK(
        !r4_protocol_decode_hex(
            "123",
            decoded,
            sizeof(decoded)
        )
    );
}

static void test_protocol_formatting(void) {
    r4_controller_state_t state;
    r4_controller_state_reset(&state);
    state.left_x = 1;
    state.left_y = 2;
    state.right_x = 3;
    state.right_y = 4;
    state.hat = 5;
    state.buttons = UINT32_C(0x1234);
    state.left_trigger = 1000;
    state.right_trigger = 2000;
    state.left_trigger_status = R4_ANALOG_OK;
    state.right_trigger_status = R4_ANALOG_SOURCE_UNAVAILABLE;

    char output[384];
    CHECK(
        r4_protocol_format_input(
            output,
            sizeof(output),
            &state
        )
    );
    CHECK(
        strstr(
            output,
            "LX=1 LY=2 RX=3 RY=4 HAT=5 BUTTONS=0x00001234"
        ) == output
    );
    CHECK(strstr(output, " LT=1000 RT=2000 ") != NULL);
    CHECK(strstr(output, "RT_STATUS=NO_SOURCE") != NULL);

    CHECK(
        r4_protocol_format_status(
            output,
            sizeof(output),
            "0.10.0",
            1,
            2,
            3,
            4,
            5,
            6,
            true,
            &state
        )
    );
    CHECK(
        strstr(output, "FW=0.10.0 LED=1,2,3") == output
    );
}

static void drain_events(r4_service_buttons_t *buttons) {
    r4_service_event_t event;
    while (r4_service_buttons_pop(buttons, &event)) {
    }
}

static r4_service_buttons_t new_service_buttons(void) {
    r4_service_buttons_t buttons;
    r4_service_buttons_init(
        &buttons,
        (r4_service_button_config_t){
            .debounce_ms = 30,
            .long_press_ms = 700,
            .double_press_ms = 300
        }
    );
    return buttons;
}

static void test_r4_short(void) {
    r4_service_buttons_t buttons = new_service_buttons();
    r4_service_event_t event;

    r4_service_buttons_update(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        true,
        0
    );
    r4_service_buttons_tick(&buttons, 30);
    CHECK(r4_service_buttons_pop(&buttons, &event));
    CHECK(event.action == R4_SERVICE_ACTION_PRESS);

    r4_service_buttons_update(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        false,
        100
    );
    r4_service_buttons_tick(&buttons, 130);
    CHECK(r4_service_buttons_pop(&buttons, &event));
    CHECK(event.action == R4_SERVICE_ACTION_RELEASE);
    CHECK(!r4_service_buttons_pop(&buttons, &event));

    r4_service_buttons_tick(&buttons, 430);
    CHECK(r4_service_buttons_pop(&buttons, &event));
    CHECK(event.action == R4_SERVICE_ACTION_SHORT);
}

static void test_r4_long(void) {
    r4_service_buttons_t buttons = new_service_buttons();
    r4_service_event_t event;

    r4_service_buttons_update(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        true,
        0
    );
    r4_service_buttons_tick(&buttons, 30);
    drain_events(&buttons);
    r4_service_buttons_tick(&buttons, 730);
    CHECK(r4_service_buttons_pop(&buttons, &event));
    CHECK(event.action == R4_SERVICE_ACTION_LONG);
    r4_service_buttons_tick(&buttons, 1000);
    CHECK(!r4_service_buttons_pop(&buttons, &event));

    r4_service_buttons_update(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        false,
        1100
    );
    r4_service_buttons_tick(&buttons, 1130);
    CHECK(r4_service_buttons_pop(&buttons, &event));
    CHECK(event.action == R4_SERVICE_ACTION_RELEASE);
    r4_service_buttons_tick(&buttons, 1500);
    CHECK(!r4_service_buttons_pop(&buttons, &event));
}

static void test_r4_double_and_debounce(void) {
    r4_service_buttons_t buttons = new_service_buttons();
    r4_service_event_t event;

    r4_service_buttons_update(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        true,
        0
    );
    r4_service_buttons_update(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        false,
        10
    );
    r4_service_buttons_tick(&buttons, 40);
    CHECK(!r4_service_buttons_pop(&buttons, &event));

    r4_service_buttons_update(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        true,
        100
    );
    r4_service_buttons_tick(&buttons, 130);
    drain_events(&buttons);
    r4_service_buttons_update(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        false,
        180
    );
    r4_service_buttons_tick(&buttons, 210);
    drain_events(&buttons);
    r4_service_buttons_update(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        true,
        260
    );
    r4_service_buttons_tick(&buttons, 290);
    drain_events(&buttons);
    r4_service_buttons_update(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        false,
        340
    );
    r4_service_buttons_tick(&buttons, 370);

    CHECK(r4_service_buttons_pop(&buttons, &event));
    CHECK(event.action == R4_SERVICE_ACTION_RELEASE);
    CHECK(r4_service_buttons_pop(&buttons, &event));
    CHECK(event.action == R4_SERVICE_ACTION_DOUBLE);
    r4_service_buttons_tick(&buttons, 1000);
    CHECK(!r4_service_buttons_pop(&buttons, &event));
}

static void test_r4_chord_suppresses_standalone_action(
    uint32_t other_button
) {
    r4_service_buttons_t buttons = new_service_buttons();
    r4_service_event_t event;

    r4_service_buttons_update_with_chord(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        true,
        false,
        0
    );
    r4_service_buttons_tick(&buttons, 30);
    CHECK(r4_service_buttons_pop(&buttons, &event));
    CHECK(event.action == R4_SERVICE_ACTION_PRESS);

    r4_service_buttons_update_with_chord(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        true,
        true,
        100
    );

    r4_controller_state_t state;
    r4_controller_state_reset(&state);
    state.buttons = R4_BUTTON_R4 | other_button;

    r4_hid_report_t report;
    r4_build_hid_report(&state, &report);
    CHECK((report.buttons & R4_BUTTON_R4) != 0);
    CHECK((report.buttons & other_button) != 0);

    r4_service_buttons_update_with_chord(
        &buttons,
        R4_SERVICE_BUTTON_R4,
        false,
        false,
        150
    );
    r4_service_buttons_tick(&buttons, 180);
    CHECK(r4_service_buttons_pop(&buttons, &event));
    CHECK(event.action == R4_SERVICE_ACTION_RELEASE);

    r4_service_buttons_tick(&buttons, 1000);
    CHECK(!r4_service_buttons_pop(&buttons, &event));
}

static void test_r4_chords(void) {
    test_r4_chord_suppresses_standalone_action(
        R4_BUTTON_START
    );
    test_r4_chord_suppresses_standalone_action(
        R4_BUTTON_A
    );
}

static void test_framebuffer_transport(void) {
    uint8_t pixels[16] = {0};
    uint8_t packed[2] = {0};
    pixels[0] = 1;
    pixels[7] = 1;

    for (size_t index = 8; index < 16; ++index) {
        pixels[index] = 1;
    }

    CHECK(
        r4_framebuffer_pack_mono1_msb(
            pixels,
            sizeof(pixels),
            packed,
            sizeof(packed)
        )
    );
    CHECK(packed[0] == UINT8_C(0x81));
    CHECK(packed[1] == UINT8_C(0xFF));

    char response[R4_FRAMEBUFFER_RESPONSE_MAX + 1U];
    CHECK(
        r4_framebuffer_format_info(
            response,
            sizeof(response),
            7,
            UINT32_C(0x1234ABCD)
        )
    );
    CHECK(strstr(response, "FRAMEBUFFER INFO ID=7") == response);
    CHECK(strstr(response, "FORMAT=MONO1_MSB") != NULL);
    CHECK(strlen(response) <= R4_FRAMEBUFFER_RESPONSE_MAX);

    uint8_t chunk[R4_FRAMEBUFFER_CHUNK_MAX];

    for (size_t index = 0; index < sizeof(chunk); ++index) {
        chunk[index] = (uint8_t)index;
    }

    CHECK(
        r4_framebuffer_format_chunk(
            response,
            sizeof(response),
            7,
            0,
            chunk,
            sizeof(chunk),
            sizeof(chunk)
        )
    );
    CHECK(strstr(response, "FRAMEBUFFER DATA ID=7 OFFSET=0") == response);
    CHECK(strlen(response) <= R4_FRAMEBUFFER_RESPONSE_MAX);
    CHECK(
        !r4_framebuffer_format_chunk(
            response,
            sizeof(response),
            7,
            0,
            chunk,
            sizeof(chunk),
            R4_FRAMEBUFFER_CHUNK_MAX + 1U
        )
    );
}

static void test_display_snapshots(void) {
    uint8_t pixels[128U * 64U];
    r4_framebuffer_t framebuffer = {
        .width = 128,
        .height = 64,
        .pixels = pixels,
        .pixel_capacity = sizeof(pixels)
    };
    r4_display_model_t model;
    r4_display_model_init(&model);

    CHECK(r4_display_render(&model, &framebuffer));
    const uint32_t boot_hash = r4_display_hash(&framebuffer);
    CHECK(boot_hash != 0);

    model.screen = R4_DISPLAY_GAME;
    strcpy(model.system, "NES");
    strcpy(model.game, "MARIO");
    strcpy(model.time, "12:34");
    model.network_connected = true;
    model.battery_available = true;
    model.battery_percent = 75;
    model.retroachievements_active = true;
    CHECK(r4_display_render(&model, &framebuffer));
    const uint32_t game_hash = r4_display_hash(&framebuffer);
    CHECK(game_hash != 0);
    CHECK(game_hash != boot_hash);

    model.replay_buffering = true;
    CHECK(r4_display_render(&model, &framebuffer));
    const uint32_t replay_hash = r4_display_hash(&framebuffer);
    CHECK(replay_hash != game_hash);
    model.replay_buffering = false;

    r4_display_tick(&model, 499U);
    CHECK(model.clock_colon_visible);
    CHECK(r4_display_render(&model, &framebuffer));
    CHECK(r4_display_hash(&framebuffer) == game_hash);

    r4_display_tick(&model, 500U);
    CHECK(!model.clock_colon_visible);
    CHECK(r4_display_render(&model, &framebuffer));
    const uint32_t hidden_colon_hash =
        r4_display_hash(&framebuffer);
    CHECK(hidden_colon_hash != game_hash);

    r4_display_tick(&model, 1000U);
    CHECK(model.clock_colon_visible);
    CHECK(r4_display_render(&model, &framebuffer));
    CHECK(r4_display_hash(&framebuffer) == game_hash);

    strcpy(
        model.game,
        "Classic NES Series - Super Mario Bros."
    );
    CHECK(
        strcmp(
            model.game,
            "Classic NES Series - Super Mario Bros."
        ) == 0
    );

    r4_display_start_game(&model, UINT32_MAX - 500U);
    r4_display_tick(&model, 1500U);
    CHECK(model.game_timer_running);
    CHECK(model.game_elapsed_seconds == 2U);
    r4_display_stop_game(&model);
    CHECK(!model.game_timer_running);
    CHECK(model.game_elapsed_seconds == 0U);

    r4_display_show_notification(
        &model,
        "CAPTURE SAVED",
        UINT32_MAX - 500U
    );
    CHECK(model.notification_visible);
    CHECK(r4_display_render(&model, &framebuffer));
    const uint32_t notification_hash =
        r4_display_hash(&framebuffer);
    CHECK(notification_hash != game_hash);
    r4_display_tick(
        &model,
        R4_DISPLAY_NOTIFICATION_DURATION_MS - 502U
    );
    CHECK(model.notification_visible);
    r4_display_tick(
        &model,
        R4_DISPLAY_NOTIFICATION_DURATION_MS - 501U
    );
    CHECK(!model.notification_visible);

    strcpy(model.achievement, "FIRST WIN");
    r4_display_show_notification(&model, "CARD READY", 1000U);
    r4_display_show_achievement(&model, 1000);
    CHECK(model.achievement_visible);
    CHECK(model.notification_visible);
    CHECK(r4_display_render(&model, &framebuffer));
    CHECK(r4_display_hash(&framebuffer) != game_hash);
    const uint32_t achievement_over_notification_hash =
        r4_display_hash(&framebuffer);
    model.notification_visible = false;
    CHECK(r4_display_render(&model, &framebuffer));
    CHECK(
        r4_display_hash(&framebuffer) ==
            achievement_over_notification_hash
    );
    r4_display_tick(
        &model,
        1000 + R4_DISPLAY_ACHIEVEMENT_DURATION_MS - 1U
    );
    CHECK(model.achievement_visible);
    r4_display_tick(
        &model,
        1000 + R4_DISPLAY_ACHIEVEMENT_DURATION_MS
    );
    CHECK(!model.achievement_visible);

    r4_display_show_achievement(&model, UINT32_MAX - 1000U);
    r4_display_tick(&model, 3998U);
    CHECK(model.achievement_visible);
    r4_display_tick(&model, 3999U);
    CHECK(!model.achievement_visible);
}

static void test_display_host_watchdog(void) {
    r4_display_model_t model;
    r4_display_model_init(&model);
    model.screen = R4_DISPLAY_HOME;

    r4_display_tick(&model, 100000U);
    CHECK(model.screen == R4_DISPLAY_HOME);
    CHECK(!model.host_watchdog_armed);

    r4_display_arm_host_watchdog(
        &model,
        UINT32_MAX - 3000U
    );
    CHECK(model.host_watchdog_armed);
    CHECK(model.orange_pi_connected);

    r4_display_tick(&model, 3998U);
    CHECK(model.screen == R4_DISPLAY_HOME);
    CHECK(!model.host_link_lost);

    r4_display_tick(&model, 3999U);
    CHECK(model.screen == R4_DISPLAY_ERROR);
    CHECK(model.host_link_lost);
    CHECK(!model.orange_pi_connected);
    CHECK(strcmp(model.error, "HOST LINK LOST") == 0);

    r4_display_host_activity(&model, 4000U);
    CHECK(model.screen == R4_DISPLAY_HOME);
    CHECK(!model.host_link_lost);
    CHECK(model.orange_pi_connected);

    model.screen = R4_DISPLAY_ERROR;
    strcpy(model.error, "LOW BATTERY");
    r4_display_tick(&model, 11000U);
    CHECK(model.host_link_lost);
    CHECK(strcmp(model.error, "HOST LINK LOST") == 0);
    r4_display_host_activity(&model, 11001U);
    CHECK(model.screen == R4_DISPLAY_ERROR);
    CHECK(strcmp(model.error, "LOW BATTERY") == 0);
}

int main(void) {
    test_analog_normalization();
    test_centered_axis();
    test_hid_report();
    test_input_sources();
    test_protocol_parser();
    test_protocol_formatting();
    test_r4_short();
    test_r4_long();
    test_r4_double_and_debounce();
    test_r4_chords();
    test_framebuffer_transport();
    test_display_snapshots();
    test_display_host_watchdog();

    if (failures != 0) {
        fprintf(
            stderr,
            "%d of %d checks failed\n",
            failures,
            checks
        );
        return EXIT_FAILURE;
    }

    printf("%d checks passed\n", checks);
    return EXIT_SUCCESS;
}
