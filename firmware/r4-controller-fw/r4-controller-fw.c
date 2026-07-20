#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware/adc.h"
#include "pico/stdlib.h"
#include "tusb.h"

// -----------------------------------------------------------------------------
// Firmware information
// -----------------------------------------------------------------------------

#define R4_FIRMWARE_VERSION "0.2.0"

// -----------------------------------------------------------------------------
// Current breadboard pinout
// -----------------------------------------------------------------------------

#define PIN_BUTTON_1     13
#define PIN_BUTTON_2     14
#define PIN_STICK_BUTTON 11

#define PIN_STICK_X      26
#define PIN_STICK_Y      27

#define ADC_STICK_X      0
#define ADC_STICK_Y      1

#define ADC_MAX_VALUE    4095
#define AXIS_DEADZONE    100

// -----------------------------------------------------------------------------
// CDC service protocol
// -----------------------------------------------------------------------------

#define CDC_COMMAND_BUFFER_SIZE 64

static char cdc_command_buffer[CDC_COMMAND_BUFFER_SIZE];
static size_t cdc_command_length;
static bool cdc_discarding_line;

// -----------------------------------------------------------------------------
// Controller state
// -----------------------------------------------------------------------------

static uint16_t stick_center_x;
static uint16_t stick_center_y;

static hid_gamepad_report_t latest_gamepad_report;

// -----------------------------------------------------------------------------
// Hardware input
// -----------------------------------------------------------------------------

static void initialize_button(uint gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);

    // Released = HIGH, pressed and connected to GND = LOW.
    gpio_pull_up(gpio);
}

static uint16_t read_adc_input(uint input) {
    adc_select_input(input);

    // Allow the ADC multiplexer to settle after changing channel.
    sleep_us(5);

    return adc_read();
}

static void calibrate_stick_center(void) {
    uint32_t sum_x = 0;
    uint32_t sum_y = 0;

    // The stick must remain released during startup calibration.
    sleep_ms(250);

    for (uint32_t i = 0; i < 128; ++i) {
        sum_x += read_adc_input(ADC_STICK_X);
        sum_y += read_adc_input(ADC_STICK_Y);

        sleep_ms(2);
    }

    stick_center_x = (uint16_t)(sum_x / 128);
    stick_center_y = (uint16_t)(sum_y / 128);
}

static int8_t map_axis(uint16_t raw_value, uint16_t center) {
    int32_t delta =
        (int32_t)raw_value - (int32_t)center;

    if (
        delta > -AXIS_DEADZONE &&
        delta < AXIS_DEADZONE
    ) {
        return 0;
    }

    int32_t mapped_value;

    if (delta > 0) {
        const int32_t available_range =
            ADC_MAX_VALUE - center - AXIS_DEADZONE;

        if (available_range <= 0) {
            return 127;
        }

        mapped_value =
            ((delta - AXIS_DEADZONE) * 127) /
            available_range;
    } else {
        const int32_t available_range =
            center - AXIS_DEADZONE;

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
// USB HID gamepad
// -----------------------------------------------------------------------------

static void send_gamepad_report(void) {
    if (!tud_hid_ready()) {
        return;
    }

    hid_gamepad_report_t report = {
        .x = map_axis(
            read_adc_input(ADC_STICK_X),
            stick_center_x
        ),

        .y = map_axis(
            read_adc_input(ADC_STICK_Y),
            stick_center_y
        ),

        .z = 0,
        .rz = 0,
        .rx = 0,
        .ry = 0,

        .hat = GAMEPAD_HAT_CENTERED,
        .buttons = 0
    };

    if (!gpio_get(PIN_BUTTON_1)) {
        report.buttons |= GAMEPAD_BUTTON_A;
    }

    if (!gpio_get(PIN_BUTTON_2)) {
        report.buttons |= GAMEPAD_BUTTON_B;
    }

    if (!gpio_get(PIN_STICK_BUTTON)) {
        report.buttons |= GAMEPAD_BUTTON_THUMBL;
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
// USB CDC service channel
// -----------------------------------------------------------------------------

static void cdc_write_line(const char *text) {
    tud_cdc_write(text, strlen(text));
    tud_cdc_write("\r\n", 2);
    tud_cdc_write_flush();
}

static void process_cdc_command(void) {
    cdc_command_buffer[cdc_command_length] = '\0';

    if (strcmp(cdc_command_buffer, "PING") == 0) {
        cdc_write_line("PONG");
    } else if (
        strcmp(cdc_command_buffer, "VERSION") == 0
    ) {
        cdc_write_line(
            "R4_CONTROLLER_FW " R4_FIRMWARE_VERSION
        );
    } else if (
        strcmp(cdc_command_buffer, "INPUT") == 0
    ) {
        char response[96];

        snprintf(
            response,
            sizeof(response),
            "X=%d Y=%d BUTTONS=0x%08lX",
            (int)latest_gamepad_report.x,
            (int)latest_gamepad_report.y,
            (unsigned long)latest_gamepad_report.buttons
        );

        cdc_write_line(response);
    } else if (
        strcmp(cdc_command_buffer, "HELP") == 0
    ) {
        cdc_write_line(
            "COMMANDS PING VERSION INPUT HELP"
        );
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
        CDC_COMMAND_BUFFER_SIZE - 1
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
            tud_cdc_read(
                input_buffer,
                sizeof(input_buffer)
            );

        for (uint32_t i = 0; i < received; ++i) {
            process_cdc_character(input_buffer[i]);
        }
    }
}

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------

int main(void) {
    initialize_button(PIN_BUTTON_1);
    initialize_button(PIN_BUTTON_2);
    initialize_button(PIN_STICK_BUTTON);

    adc_init();
    adc_gpio_init(PIN_STICK_X);
    adc_gpio_init(PIN_STICK_Y);

    calibrate_stick_center();

    const tusb_rhport_init_t usb_configuration = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL
    };

    if (!tud_rhport_init(0, &usb_configuration)) {
        while (true) {
            tight_loop_contents();
        }
    }

    uint32_t previous_report_time_ms = 0;

    while (true) {
        tud_task();

        // Process commands received through R4 Service.
        cdc_service_task();

        const uint32_t current_time_ms =
            to_ms_since_boot(get_absolute_time());

        // Send the gamepad state approximately 200 times per second.
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