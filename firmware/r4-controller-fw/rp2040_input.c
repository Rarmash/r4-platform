#include "rp2040_input.h"

#include <stdbool.h>
#include <stdint.h>

#include "hardware/adc.h"
#include "pico/stdlib.h"

#define PIN_BUTTON_L1 0
#define PIN_BUTTON_R1 1
#define PIN_DPAD_UP 2
#define PIN_DPAD_DOWN 3
#define PIN_DPAD_LEFT 4
#define PIN_DPAD_RIGHT 5
#define PIN_BUTTON_X 6
#define PIN_BUTTON_Y 7
#define PIN_BUTTON_SELECT 8
#define PIN_BUTTON_START 9
#define PIN_BUTTON_R4 10
#define PIN_LEFT_STICK_BUTTON 11
#define PIN_RIGHT_STICK_BUTTON 12
#define PIN_BUTTON_A 13
#define PIN_BUTTON_B 14

#define PIN_LEFT_STICK_X 26
#define PIN_LEFT_STICK_Y 27
#define PIN_RIGHT_STICK_X 28
#define PIN_RIGHT_STICK_Y 29

#define ADC_LEFT_STICK_X 0
#define ADC_LEFT_STICK_Y 1
#define ADC_RIGHT_STICK_X 2
#define ADC_RIGHT_STICK_Y 3

#define ADC_MAX_VALUE 4095
#define AXIS_DEADZONE 100

static void initialize_button(uint gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}

static bool is_button_pressed(uint gpio) {
    return !gpio_get(gpio);
}

static uint16_t read_adc_input(uint input) {
    adc_select_input(input);
    sleep_us(5);
    return adc_read();
}

static void calibrate_stick_centers(r4_rp2040_input_t *input) {
    uint32_t left_sum_x = 0;
    uint32_t left_sum_y = 0;
    uint32_t right_sum_x = 0;
    uint32_t right_sum_y = 0;

    sleep_ms(250);

    for (uint32_t index = 0; index < 128; ++index) {
        left_sum_x += read_adc_input(ADC_LEFT_STICK_X);
        left_sum_y += read_adc_input(ADC_LEFT_STICK_Y);
        right_sum_x += read_adc_input(ADC_RIGHT_STICK_X);
        right_sum_y += read_adc_input(ADC_RIGHT_STICK_Y);
        sleep_ms(2);
    }

    input->left_center_x = (uint16_t)(left_sum_x / 128U);
    input->left_center_y = (uint16_t)(left_sum_y / 128U);
    input->right_center_x = (uint16_t)(right_sum_x / 128U);
    input->right_center_y = (uint16_t)(right_sum_y / 128U);
}

static uint8_t read_dpad_hat(void) {
    bool up = is_button_pressed(PIN_DPAD_UP);
    bool down = is_button_pressed(PIN_DPAD_DOWN);
    bool left = is_button_pressed(PIN_DPAD_LEFT);
    bool right = is_button_pressed(PIN_DPAD_RIGHT);

    if (up && down) {
        up = false;
        down = false;
    }

    if (left && right) {
        left = false;
        right = false;
    }

    if (up) {
        if (right) {
            return R4_HAT_UP_RIGHT;
        }
        if (left) {
            return R4_HAT_UP_LEFT;
        }
        return R4_HAT_UP;
    }

    if (down) {
        if (right) {
            return R4_HAT_DOWN_RIGHT;
        }
        if (left) {
            return R4_HAT_DOWN_LEFT;
        }
        return R4_HAT_DOWN;
    }

    if (right) {
        return R4_HAT_RIGHT;
    }
    if (left) {
        return R4_HAT_LEFT;
    }
    return R4_HAT_CENTERED;
}

static void update_button(
    r4_controller_state_t *state,
    uint gpio,
    uint32_t mask
) {
    if (is_button_pressed(gpio)) {
        state->buttons |= mask;
    }
}

void r4_rp2040_input_init(r4_rp2040_input_t *input) {
    if (input == NULL) {
        return;
    }

    const uint button_pins[] = {
        PIN_BUTTON_L1,
        PIN_BUTTON_R1,
        PIN_DPAD_UP,
        PIN_DPAD_DOWN,
        PIN_DPAD_LEFT,
        PIN_DPAD_RIGHT,
        PIN_BUTTON_X,
        PIN_BUTTON_Y,
        PIN_BUTTON_SELECT,
        PIN_BUTTON_START,
        PIN_BUTTON_R4,
        PIN_LEFT_STICK_BUTTON,
        PIN_RIGHT_STICK_BUTTON,
        PIN_BUTTON_A,
        PIN_BUTTON_B
    };

    for (
        size_t index = 0;
        index < sizeof(button_pins) / sizeof(button_pins[0]);
        ++index
    ) {
        initialize_button(button_pins[index]);
    }

    adc_init();
    adc_gpio_init(PIN_LEFT_STICK_X);
    adc_gpio_init(PIN_LEFT_STICK_Y);
    adc_gpio_init(PIN_RIGHT_STICK_X);
    adc_gpio_init(PIN_RIGHT_STICK_Y);

    input->trigger_source =
        r4_input_source_unavailable(R4_SOURCE_EXTERNAL_ADC);
    input->left_trigger_calibration =
        (r4_analog_calibration_t){
            .raw_min = 0,
            .raw_max = 4095,
            .dead_zone = 64,
            .inverted = false
        };
    input->right_trigger_calibration =
        input->left_trigger_calibration;

    calibrate_stick_centers(input);
}

bool r4_rp2040_input_poll(
    void *context,
    r4_controller_state_t *state
) {
    r4_rp2040_input_t *input = context;

    if (input == NULL || state == NULL) {
        return false;
    }

    r4_controller_state_reset(state);

    state->left_x = r4_map_centered_axis(
        read_adc_input(ADC_LEFT_STICK_X),
        input->left_center_x,
        ADC_MAX_VALUE,
        AXIS_DEADZONE
    );
    state->left_y = r4_map_centered_axis(
        read_adc_input(ADC_LEFT_STICK_Y),
        input->left_center_y,
        ADC_MAX_VALUE,
        AXIS_DEADZONE
    );
    state->right_x = r4_map_centered_axis(
        read_adc_input(ADC_RIGHT_STICK_X),
        input->right_center_x,
        ADC_MAX_VALUE,
        AXIS_DEADZONE
    );
    state->right_y = r4_map_centered_axis(
        read_adc_input(ADC_RIGHT_STICK_Y),
        input->right_center_y,
        ADC_MAX_VALUE,
        AXIS_DEADZONE
    );
    state->hat = read_dpad_hat();

    update_button(state, PIN_BUTTON_A, R4_BUTTON_A);
    update_button(state, PIN_BUTTON_B, R4_BUTTON_B);
    update_button(state, PIN_BUTTON_X, R4_BUTTON_X);
    update_button(state, PIN_BUTTON_Y, R4_BUTTON_Y);
    update_button(state, PIN_BUTTON_L1, R4_BUTTON_L1);
    update_button(state, PIN_BUTTON_R1, R4_BUTTON_R1);
    update_button(state, PIN_BUTTON_SELECT, R4_BUTTON_SELECT);
    update_button(state, PIN_BUTTON_START, R4_BUTTON_START);
    update_button(state, PIN_BUTTON_R4, R4_BUTTON_R4);
    update_button(
        state,
        PIN_LEFT_STICK_BUTTON,
        R4_BUTTON_LEFT_STICK
    );
    update_button(
        state,
        PIN_RIGHT_STICK_BUTTON,
        R4_BUTTON_RIGHT_STICK
    );

    const r4_analog_result_t left_trigger =
        r4_normalize_analog(
            r4_input_source_read_analog(
                &input->trigger_source,
                0
            ),
            input->left_trigger_calibration
        );
    const r4_analog_result_t right_trigger =
        r4_normalize_analog(
            r4_input_source_read_analog(
                &input->trigger_source,
                1
            ),
            input->right_trigger_calibration
        );

    state->left_trigger = left_trigger.value;
    state->right_trigger = right_trigger.value;
    state->left_trigger_status = left_trigger.status;
    state->right_trigger_status = right_trigger.status;
    return true;
}

r4_input_backend_t r4_rp2040_input_backend(
    r4_rp2040_input_t *input
) {
    return (r4_input_backend_t){
        .context = input,
        .capabilities =
            R4_INPUT_CAP_DIRECT_GPIO |
            R4_INPUT_CAP_BUILTIN_ADC,
        .poll = r4_rp2040_input_poll
    };
}
