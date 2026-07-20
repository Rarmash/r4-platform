#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware/adc.h"
#include "pico/stdlib.h"
#include "rgb_led.h"
#include "tusb.h"

// -----------------------------------------------------------------------------
// Firmware information
// -----------------------------------------------------------------------------

#define R4_FIRMWARE_VERSION "0.5.1"

// -----------------------------------------------------------------------------
// Current breadboard pinout
// -----------------------------------------------------------------------------

#define PIN_BUTTON_1 13
#define PIN_BUTTON_2 14

#define PIN_LEFT_STICK_BUTTON 11
#define PIN_RIGHT_STICK_BUTTON 12

#define PIN_LEFT_STICK_X 26
#define PIN_LEFT_STICK_Y 27

#define PIN_RIGHT_STICK_X 28
#define PIN_RIGHT_STICK_Y 29

#define PIN_RGB_LED 16

#define ADC_LEFT_STICK_X 0
#define ADC_LEFT_STICK_Y 1

#define ADC_RIGHT_STICK_X 2
#define ADC_RIGHT_STICK_Y 3

#define ADC_MAX_VALUE 4095
#define AXIS_DEADZONE 100

// -----------------------------------------------------------------------------
// CDC service protocol
// -----------------------------------------------------------------------------

#define CDC_COMMAND_BUFFER_SIZE 96
#define CDC_WRITE_TIMEOUT_MS 1000

static char cdc_command_buffer[CDC_COMMAND_BUFFER_SIZE];
static size_t cdc_command_length;
static bool cdc_discarding_line;

// -----------------------------------------------------------------------------
// Controller state
// -----------------------------------------------------------------------------

static uint16_t left_stick_center_x;
static uint16_t left_stick_center_y;

static uint16_t right_stick_center_x;
static uint16_t right_stick_center_y;

static hid_gamepad_report_t latest_gamepad_report;

// -----------------------------------------------------------------------------
// RGB LED state
// -----------------------------------------------------------------------------

static uint8_t led_base_red;
static uint8_t led_base_green;
static uint8_t led_base_blue;

static uint8_t led_output_red;
static uint8_t led_output_green;
static uint8_t led_output_blue;

static bool led_flash_active;
static absolute_time_t led_flash_deadline;

// -----------------------------------------------------------------------------
// Hardware input
// -----------------------------------------------------------------------------

static void initialize_button(uint gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);

    // Released = HIGH.
    // Pressed and connected to GND = LOW.
    gpio_pull_up(gpio);
}

static uint16_t read_adc_input(uint input) {
    adc_select_input(input);

    // Allow the ADC multiplexer to settle after changing channels.
    sleep_us(5);

    return adc_read();
}

static void calibrate_stick_centers(void) {
    uint32_t left_sum_x = 0;
    uint32_t left_sum_y = 0;

    uint32_t right_sum_x = 0;
    uint32_t right_sum_y = 0;

    // Both sticks must remain released during startup calibration.
    sleep_ms(250);

    for (uint32_t i = 0; i < 128; ++i) {
        left_sum_x += read_adc_input(ADC_LEFT_STICK_X);
        left_sum_y += read_adc_input(ADC_LEFT_STICK_Y);

        right_sum_x += read_adc_input(ADC_RIGHT_STICK_X);
        right_sum_y += read_adc_input(ADC_RIGHT_STICK_Y);

        sleep_ms(2);
    }

    left_stick_center_x =
        (uint16_t)(left_sum_x / 128);

    left_stick_center_y =
        (uint16_t)(left_sum_y / 128);

    right_stick_center_x =
        (uint16_t)(right_sum_x / 128);

    right_stick_center_y =
        (uint16_t)(right_sum_y / 128);
}

static int8_t map_axis(
    uint16_t raw_value,
    uint16_t center
) {
    const int32_t delta =
        (int32_t)raw_value -
        (int32_t)center;

    if (
        delta > -AXIS_DEADZONE &&
        delta < AXIS_DEADZONE
    ) {
        return 0;
    }

    int32_t mapped_value;

    if (delta > 0) {
        const int32_t available_range =
            ADC_MAX_VALUE -
            center -
            AXIS_DEADZONE;

        if (available_range <= 0) {
            return 127;
        }

        mapped_value =
            ((delta - AXIS_DEADZONE) * 127) /
            available_range;
    } else {
        const int32_t available_range =
            center -
            AXIS_DEADZONE;

        if (available_range <= 0) {
            return -127;
        }

        mapped_value =
            ((delta + AXIS_DEADZONE) * 127) /
            available_range;
    }

    if (mapped_value > 127) {
        mapped_value = 127;
    }

    if (mapped_value < -127) {
        mapped_value = -127;
    }

    return (int8_t)mapped_value;
}

// -----------------------------------------------------------------------------
// RGB status LED
// -----------------------------------------------------------------------------

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

    led_flash_deadline =
        make_timeout_time_ms(duration_ms);

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

// -----------------------------------------------------------------------------
// USB HID gamepad
// -----------------------------------------------------------------------------

static void send_gamepad_report(void) {
    if (!tud_hid_ready()) {
        return;
    }

    hid_gamepad_report_t report = {
        .x = map_axis(
            read_adc_input(ADC_LEFT_STICK_X),
            left_stick_center_x
        ),

        .y = map_axis(
            read_adc_input(ADC_LEFT_STICK_Y),
            left_stick_center_y
        ),

        .rx = map_axis(
            read_adc_input(ADC_RIGHT_STICK_X),
            right_stick_center_x
        ),

        .ry = map_axis(
            read_adc_input(ADC_RIGHT_STICK_Y),
            right_stick_center_y
        ),

        .z = 0,
        .rz = 0,

        .hat = GAMEPAD_HAT_CENTERED,
        .buttons = 0
    };

    if (!gpio_get(PIN_BUTTON_1)) {
        report.buttons |= GAMEPAD_BUTTON_A;
    }

    if (!gpio_get(PIN_BUTTON_2)) {
        report.buttons |= GAMEPAD_BUTTON_B;
    }

    if (!gpio_get(PIN_LEFT_STICK_BUTTON)) {
        report.buttons |= GAMEPAD_BUTTON_THUMBL;
    }

    if (!gpio_get(PIN_RIGHT_STICK_BUTTON)) {
        report.buttons |= GAMEPAD_BUTTON_THUMBR;
    }

    latest_gamepad_report = report;

    tud_hid_report(
        0,
        &report,
        sizeof(report)
    );
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

// -----------------------------------------------------------------------------
// USB CDC output
// -----------------------------------------------------------------------------

static bool cdc_write_all(
    const void *data,
    size_t length
) {
    const uint8_t *bytes =
        (const uint8_t *)data;

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

        const size_t remaining =
            length - offset;

        const uint32_t chunk_size =
            remaining < available
                ? (uint32_t)remaining
                : available;

        const uint32_t written =
            tud_cdc_write(
                bytes + offset,
                chunk_size
            );

        offset += written;

        tud_cdc_write_flush();
        tud_task();

        if (
            written == 0 &&
            time_reached(deadline)
        ) {
            return false;
        }
    }

    tud_cdc_write_flush();

    return true;
}

static void cdc_write_line(const char *text) {
    const size_t text_length =
        strlen(text);

    if (!cdc_write_all(text, text_length)) {
        return;
    }

    static const char line_ending[] = "\r\n";

    cdc_write_all(
        line_ending,
        sizeof(line_ending) - 1
    );
}

// -----------------------------------------------------------------------------
// CDC command handlers
// -----------------------------------------------------------------------------

static void process_ping_command(void) {
    cdc_write_line("PONG");
}

static void process_version_command(void) {
    cdc_write_line(
        "R4_CONTROLLER_FW " R4_FIRMWARE_VERSION
    );
}

static void process_input_command(void) {
    char response[128];

    snprintf(
        response,
        sizeof(response),
        "LX=%d LY=%d RX=%d RY=%d BUTTONS=0x%08lX",
        (int)latest_gamepad_report.x,
        (int)latest_gamepad_report.y,
        (int)latest_gamepad_report.rx,
        (int)latest_gamepad_report.ry,
        (unsigned long)latest_gamepad_report.buttons
    );

    cdc_write_line(response);
}

static void process_status_command(void) {
    char response[256];

    snprintf(
        response,
        sizeof(response),
        "FW=%s "
        "LED=%u,%u,%u "
        "BASE=%u,%u,%u "
        "FLASH=%u "
        "LX=%d LY=%d RX=%d RY=%d "
        "BUTTONS=0x%08lX",
        R4_FIRMWARE_VERSION,

        (unsigned int)led_output_red,
        (unsigned int)led_output_green,
        (unsigned int)led_output_blue,

        (unsigned int)led_base_red,
        (unsigned int)led_base_green,
        (unsigned int)led_base_blue,

        led_flash_active ? 1U : 0U,

        (int)latest_gamepad_report.x,
        (int)latest_gamepad_report.y,
        (int)latest_gamepad_report.rx,
        (int)latest_gamepad_report.ry,

        (unsigned long)latest_gamepad_report.buttons
    );

    cdc_write_line(response);
}

static void process_led_off_command(void) {
    led_base_red = 0;
    led_base_green = 0;
    led_base_blue = 0;

    led_flash_active = false;

    apply_led_output(0, 0, 0);

    cdc_write_line("OK LED OFF");
}

static void process_led_command(
    const char *command
) {
    int red;
    int green;
    int blue;
    char trailing_character;

    const int parsed_items = sscanf(
        command,
        "LED %d %d %d %c",
        &red,
        &green,
        &blue,
        &trailing_character
    );

    if (parsed_items != 3) {
        cdc_write_line("ERR LED_USAGE");
        return;
    }

    if (
        !is_valid_color_component(red) ||
        !is_valid_color_component(green) ||
        !is_valid_color_component(blue)
    ) {
        cdc_write_line("ERR LED_RANGE");
        return;
    }

    set_base_led(
        (uint8_t)red,
        (uint8_t)green,
        (uint8_t)blue
    );

    char response[64];

    snprintf(
        response,
        sizeof(response),
        "OK LED %d %d %d",
        red,
        green,
        blue
    );

    cdc_write_line(response);
}

static void process_led_flash_command(
    const char *command
) {
    int red;
    int green;
    int blue;

    unsigned long duration_ms;
    char trailing_character;

    const int parsed_items = sscanf(
        command,
        "LED FLASH %d %d %d %lu %c",
        &red,
        &green,
        &blue,
        &duration_ms,
        &trailing_character
    );

    if (parsed_items != 4) {
        cdc_write_line(
            "ERR LED_FLASH_USAGE"
        );

        return;
    }

    if (
        !is_valid_color_component(red) ||
        !is_valid_color_component(green) ||
        !is_valid_color_component(blue)
    ) {
        cdc_write_line("ERR LED_RANGE");
        return;
    }

    if (
        duration_ms < 1 ||
        duration_ms > 10000
    ) {
        cdc_write_line(
            "ERR LED_DURATION_RANGE"
        );

        return;
    }

    start_led_flash(
        (uint8_t)red,
        (uint8_t)green,
        (uint8_t)blue,
        (uint32_t)duration_ms
    );

    char response[96];

    snprintf(
        response,
        sizeof(response),
        "OK LED FLASH %d %d %d %lu",
        red,
        green,
        blue,
        duration_ms
    );

    cdc_write_line(response);
}

static void process_help_command(void) {
    cdc_write_line(
        "COMMANDS PING VERSION INPUT STATUS "
        "LED <R> <G> <B> "
        "LED FLASH <R> <G> <B> <MS> "
        "LED OFF HELP"
    );
}

static void process_cdc_command(void) {
    cdc_command_buffer[cdc_command_length] = '\0';

    if (
        strcmp(cdc_command_buffer, "PING") == 0
    ) {
        process_ping_command();
    } else if (
        strcmp(cdc_command_buffer, "VERSION") == 0
    ) {
        process_version_command();
    } else if (
        strcmp(cdc_command_buffer, "INPUT") == 0
    ) {
        process_input_command();
    } else if (
        strcmp(cdc_command_buffer, "STATUS") == 0
    ) {
        process_status_command();
    } else if (
        strcmp(cdc_command_buffer, "LED OFF") == 0
    ) {
        process_led_off_command();
    } else if (
        strcmp(cdc_command_buffer, "HELP") == 0
    ) {
        process_help_command();
    } else if (
        strncmp(
            cdc_command_buffer,
            "LED FLASH ",
            10
        ) == 0
    ) {
        process_led_flash_command(
            cdc_command_buffer
        );
    } else if (
        strncmp(
            cdc_command_buffer,
            "LED",
            3
        ) == 0
    ) {
        process_led_command(
            cdc_command_buffer
        );
    } else {
        cdc_write_line(
            "ERR UNKNOWN_COMMAND"
        );
    }

    cdc_command_length = 0;
}

// -----------------------------------------------------------------------------
// CDC input parser
// -----------------------------------------------------------------------------

static void process_cdc_character(
    uint8_t character
) {
    if (
        character == '\r' ||
        character == '\n'
    ) {
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

    if (
        character == '\b' ||
        character == 0x7F
    ) {
        if (cdc_command_length > 0) {
            --cdc_command_length;
        }

        return;
    }

    if (
        cdc_command_length <
        CDC_COMMAND_BUFFER_SIZE - 1
    ) {
        cdc_command_buffer[cdc_command_length++] =
            (char)character;

        return;
    }

    cdc_command_length = 0;
    cdc_discarding_line = true;

    cdc_write_line(
        "ERR LINE_TOO_LONG"
    );
}

static void cdc_service_task(void) {
    while (tud_cdc_available()) {
        uint8_t input_buffer[64];

        const uint32_t received =
            tud_cdc_read(
                input_buffer,
                sizeof(input_buffer)
            );

        for (uint32_t i = 0; i < received; ++i) {
            process_cdc_character(
                input_buffer[i]
            );
        }
    }
}

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------

int main(void) {
    initialize_button(PIN_BUTTON_1);
    initialize_button(PIN_BUTTON_2);

    initialize_button(
        PIN_LEFT_STICK_BUTTON
    );

    initialize_button(
        PIN_RIGHT_STICK_BUTTON
    );

    adc_init();

    adc_gpio_init(PIN_LEFT_STICK_X);
    adc_gpio_init(PIN_LEFT_STICK_Y);

    adc_gpio_init(PIN_RIGHT_STICK_X);
    adc_gpio_init(PIN_RIGHT_STICK_Y);

    rgb_led_init(PIN_RGB_LED);

    led_base_red = 0;
    led_base_green = 0;
    led_base_blue = 0;

    led_flash_active = false;

    // Short blue startup indication.
    apply_led_output(0, 0, 16);
    sleep_ms(200);
    apply_led_output(0, 0, 0);

    calibrate_stick_centers();

    const tusb_rhport_init_t usb_configuration = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL
    };

    if (
        !tud_rhport_init(
            0,
            &usb_configuration
        )
    ) {
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
            to_ms_since_boot(
                get_absolute_time()
            );

        if (
            current_time_ms -
            previous_report_time_ms >= 5
        ) {
            previous_report_time_ms =
                current_time_ms;

            send_gamepad_report();
        }
    }
}