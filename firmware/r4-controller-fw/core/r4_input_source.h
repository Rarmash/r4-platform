#ifndef R4_INPUT_SOURCE_H
#define R4_INPUT_SOURCE_H

#include <stdbool.h>
#include <stdint.h>

#include "r4_controller.h"

typedef enum {
    R4_SOURCE_DIRECT_GPIO = 0,
    R4_SOURCE_BUILTIN_ADC,
    R4_SOURCE_GPIO_EXPANDER,
    R4_SOURCE_EXTERNAL_ADC,
    R4_SOURCE_MOCK
} r4_input_source_kind_t;

typedef struct {
    r4_input_source_kind_t kind;
    void *context;
    bool (*read_digital)(
        void *context,
        uint16_t channel,
        bool *pressed
    );
    r4_analog_sample_t (*read_analog)(
        void *context,
        uint16_t channel
    );
} r4_input_source_t;

r4_analog_sample_t r4_input_source_read_analog(
    const r4_input_source_t *source,
    uint16_t channel
);

bool r4_input_source_read_digital(
    const r4_input_source_t *source,
    uint16_t channel,
    bool *pressed
);

r4_input_source_t r4_input_source_unavailable(
    r4_input_source_kind_t kind
);

#endif
