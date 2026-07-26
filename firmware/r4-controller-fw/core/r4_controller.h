#ifndef R4_CONTROLLER_H
#define R4_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define R4_HAT_CENTERED 0U
#define R4_HAT_UP 1U
#define R4_HAT_UP_RIGHT 2U
#define R4_HAT_RIGHT 3U
#define R4_HAT_DOWN_RIGHT 4U
#define R4_HAT_DOWN 5U
#define R4_HAT_DOWN_LEFT 6U
#define R4_HAT_LEFT 7U
#define R4_HAT_UP_LEFT 8U

#define R4_BUTTON_A (UINT32_C(1) << 0)
#define R4_BUTTON_B (UINT32_C(1) << 1)
#define R4_BUTTON_X (UINT32_C(1) << 3)
#define R4_BUTTON_Y (UINT32_C(1) << 4)
#define R4_BUTTON_L1 (UINT32_C(1) << 6)
#define R4_BUTTON_R1 (UINT32_C(1) << 7)
#define R4_BUTTON_SELECT (UINT32_C(1) << 10)
#define R4_BUTTON_START (UINT32_C(1) << 11)
#define R4_BUTTON_R4 (UINT32_C(1) << 12)
/* Compatibility name for the unchanged HID/CDC button bit. */
#define R4_BUTTON_HOTKEY R4_BUTTON_R4
#define R4_BUTTON_LEFT_STICK (UINT32_C(1) << 13)
#define R4_BUTTON_RIGHT_STICK (UINT32_C(1) << 14)

typedef enum {
    R4_ANALOG_OK = 0,
    R4_ANALOG_SOURCE_UNAVAILABLE,
    R4_ANALOG_ADC_UNAVAILABLE,
    R4_ANALOG_INVALID_CALIBRATION
} r4_analog_status_t;

typedef struct {
    uint32_t raw_min;
    uint32_t raw_max;
    uint32_t dead_zone;
    bool inverted;
} r4_analog_calibration_t;

typedef struct {
    uint32_t raw_value;
    bool source_available;
    bool adc_available;
} r4_analog_sample_t;

typedef struct {
    uint16_t value;
    r4_analog_status_t status;
} r4_analog_result_t;

typedef struct {
    int8_t left_x;
    int8_t left_y;
    int8_t right_x;
    int8_t right_y;
    uint16_t left_trigger;
    uint16_t right_trigger;
    r4_analog_status_t left_trigger_status;
    r4_analog_status_t right_trigger_status;
    uint8_t hat;
    uint32_t buttons;
} r4_controller_state_t;

#if defined(_MSC_VER) || defined(__TINYC__)
#pragma pack(push, 1)
#endif

typedef struct
#if defined(__GNUC__) && !defined(__TINYC__)
    __attribute__((packed))
#endif
{
    int8_t x;
    int8_t y;
    uint8_t left_trigger;
    uint8_t right_trigger;
    int8_t right_x;
    int8_t right_y;
    uint8_t hat;
    uint32_t buttons;
} r4_hid_report_t;

#if defined(_MSC_VER) || defined(__TINYC__)
#pragma pack(pop)
#endif

typedef enum {
    R4_INPUT_CAP_DIRECT_GPIO = UINT32_C(1) << 0,
    R4_INPUT_CAP_BUILTIN_ADC = UINT32_C(1) << 1,
    R4_INPUT_CAP_GPIO_EXPANDER = UINT32_C(1) << 2,
    R4_INPUT_CAP_EXTERNAL_ADC = UINT32_C(1) << 3,
    R4_INPUT_CAP_MOCK = UINT32_C(1) << 4
} r4_input_capability_t;

typedef struct {
    void *context;
    uint32_t capabilities;
    bool (*poll)(void *context, r4_controller_state_t *state);
} r4_input_backend_t;

r4_analog_result_t r4_normalize_analog(
    r4_analog_sample_t sample,
    r4_analog_calibration_t calibration
);

int8_t r4_map_centered_axis(
    uint16_t raw_value,
    uint16_t center,
    uint16_t raw_max,
    uint16_t dead_zone
);

void r4_controller_state_reset(r4_controller_state_t *state);

void r4_build_hid_report(
    const r4_controller_state_t *state,
    r4_hid_report_t *report
);

bool r4_input_backend_poll(
    const r4_input_backend_t *backend,
    r4_controller_state_t *state
);

const char *r4_analog_status_name(r4_analog_status_t status);

#endif
