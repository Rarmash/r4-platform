#ifndef R4_RP2040_INPUT_H
#define R4_RP2040_INPUT_H

#include "core/r4_controller.h"
#include "core/r4_input_source.h"

typedef struct {
    uint16_t left_center_x;
    uint16_t left_center_y;
    uint16_t right_center_x;
    uint16_t right_center_y;
    r4_input_source_t trigger_source;
    r4_analog_calibration_t left_trigger_calibration;
    r4_analog_calibration_t right_trigger_calibration;
} r4_rp2040_input_t;

void r4_rp2040_input_init(r4_rp2040_input_t *input);

bool r4_rp2040_input_poll(
    void *context,
    r4_controller_state_t *state
);

r4_input_backend_t r4_rp2040_input_backend(
    r4_rp2040_input_t *input
);

#endif
