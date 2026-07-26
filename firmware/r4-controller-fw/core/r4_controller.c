#include "r4_controller.h"

#include <string.h>

#if defined(__TINYC__)
typedef char r4_hid_report_must_remain_11_bytes[
    sizeof(r4_hid_report_t) == 11 ? 1 : -1
];
#else
_Static_assert(
    sizeof(r4_hid_report_t) == 11,
    "The HID report layout must remain 11 bytes"
);
#endif

r4_analog_result_t r4_normalize_analog(
    r4_analog_sample_t sample,
    r4_analog_calibration_t calibration
) {
    r4_analog_result_t result = {
        .value = 0,
        .status = R4_ANALOG_OK
    };

    if (!sample.source_available) {
        result.status = R4_ANALOG_SOURCE_UNAVAILABLE;
        return result;
    }

    if (!sample.adc_available) {
        result.status = R4_ANALOG_ADC_UNAVAILABLE;
        return result;
    }

    if (calibration.raw_min >= calibration.raw_max) {
        result.status = R4_ANALOG_INVALID_CALIBRATION;
        return result;
    }

    uint32_t clamped = sample.raw_value;

    if (clamped < calibration.raw_min) {
        clamped = calibration.raw_min;
    } else if (clamped > calibration.raw_max) {
        clamped = calibration.raw_max;
    }

    const uint32_t span =
        calibration.raw_max - calibration.raw_min;

    uint32_t position = calibration.inverted
        ? calibration.raw_max - clamped
        : clamped - calibration.raw_min;

    uint32_t dead_zone = calibration.dead_zone;

    if (dead_zone >= span) {
        result.status = R4_ANALOG_INVALID_CALIBRATION;
        return result;
    }

    if (position <= dead_zone) {
        return result;
    }

    position -= dead_zone;
    const uint32_t active_span = span - dead_zone;

    const uint64_t scaled =
        (uint64_t)position * UINT16_MAX +
        active_span / 2U;

    result.value = (uint16_t)(scaled / active_span);
    return result;
}

int8_t r4_map_centered_axis(
    uint16_t raw_value,
    uint16_t center,
    uint16_t raw_max,
    uint16_t dead_zone
) {
    const int32_t delta =
        (int32_t)raw_value - (int32_t)center;

    if (
        delta > -(int32_t)dead_zone &&
        delta < (int32_t)dead_zone
    ) {
        return 0;
    }

    int32_t mapped_value;

    if (delta > 0) {
        const int32_t available_range =
            (int32_t)raw_max -
            (int32_t)center -
            (int32_t)dead_zone;

        if (available_range <= 0) {
            return 127;
        }

        mapped_value =
            ((delta - (int32_t)dead_zone) * 127) /
            available_range;
    } else {
        const int32_t available_range =
            (int32_t)center -
            (int32_t)dead_zone;

        if (available_range <= 0) {
            return -127;
        }

        mapped_value =
            ((delta + (int32_t)dead_zone) * 127) /
            available_range;
    }

    if (mapped_value > 127) {
        mapped_value = 127;
    } else if (mapped_value < -127) {
        mapped_value = -127;
    }

    return (int8_t)mapped_value;
}

void r4_controller_state_reset(r4_controller_state_t *state) {
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->hat = R4_HAT_CENTERED;
    state->left_trigger_status =
        R4_ANALOG_SOURCE_UNAVAILABLE;
    state->right_trigger_status =
        R4_ANALOG_SOURCE_UNAVAILABLE;
}

void r4_build_hid_report(
    const r4_controller_state_t *state,
    r4_hid_report_t *report
) {
    if (state == NULL || report == NULL) {
        return;
    }

    report->x = state->left_x;
    report->y = state->left_y;
    report->left_trigger =
        (uint8_t)(((uint32_t)state->left_trigger + 128U) / 257U);
    report->right_trigger =
        (uint8_t)(((uint32_t)state->right_trigger + 128U) / 257U);
    report->right_x = state->right_x;
    report->right_y = state->right_y;
    report->hat = state->hat;
    report->buttons = state->buttons;
}

bool r4_input_backend_poll(
    const r4_input_backend_t *backend,
    r4_controller_state_t *state
) {
    if (
        backend == NULL ||
        backend->poll == NULL ||
        state == NULL
    ) {
        return false;
    }

    return backend->poll(backend->context, state);
}

const char *r4_analog_status_name(r4_analog_status_t status) {
    switch (status) {
        case R4_ANALOG_OK:
            return "OK";

        case R4_ANALOG_SOURCE_UNAVAILABLE:
            return "NO_SOURCE";

        case R4_ANALOG_ADC_UNAVAILABLE:
            return "ADC_UNAVAILABLE";

        case R4_ANALOG_INVALID_CALIBRATION:
            return "INVALID_CAL";
    }

    return "UNKNOWN";
}
