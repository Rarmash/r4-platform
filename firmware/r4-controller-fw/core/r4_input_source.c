#include "r4_input_source.h"

#include <stddef.h>

r4_analog_sample_t r4_input_source_read_analog(
    const r4_input_source_t *source,
    uint16_t channel
) {
    if (source == NULL || source->read_analog == NULL) {
        return (r4_analog_sample_t){
            .raw_value = 0,
            .source_available = false,
            .adc_available = false
        };
    }

    return source->read_analog(source->context, channel);
}

bool r4_input_source_read_digital(
    const r4_input_source_t *source,
    uint16_t channel,
    bool *pressed
) {
    if (
        source == NULL ||
        source->read_digital == NULL ||
        pressed == NULL
    ) {
        return false;
    }

    return source->read_digital(
        source->context,
        channel,
        pressed
    );
}

r4_input_source_t r4_input_source_unavailable(
    r4_input_source_kind_t kind
) {
    return (r4_input_source_t){
        .kind = kind,
        .context = NULL,
        .read_digital = NULL,
        .read_analog = NULL
    };
}
