#include "r4_service_buttons.h"

#include <string.h>

static bool time_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static void emit_event(
    r4_service_buttons_t *buttons,
    r4_service_button_id_t button,
    r4_service_action_t action,
    uint32_t now_ms
) {
    if (buttons->event_count >= R4_SERVICE_EVENT_QUEUE_CAPACITY) {
        ++buttons->dropped_events;
        return;
    }

    const size_t index =
        (buttons->event_head + buttons->event_count) %
        R4_SERVICE_EVENT_QUEUE_CAPACITY;

    r4_service_event_t *event = &buttons->events[index];
    event->button = button;
    event->action = action;
    event->timestamp_ms = now_ms;
    event->sequence = ++buttons->next_sequence;
    ++buttons->event_count;
}

void r4_service_buttons_init(
    r4_service_buttons_t *buttons,
    r4_service_button_config_t config
) {
    if (buttons == NULL) {
        return;
    }

    memset(buttons, 0, sizeof(*buttons));
    buttons->config = config;
}

static void apply_stable_transition(
    r4_service_buttons_t *buttons,
    r4_service_button_id_t button,
    uint32_t now_ms
) {
    r4_service_button_state_t *state = &buttons->buttons[button];
    state->stable_pressed = state->raw_pressed;

    if (state->stable_pressed) {
        state->pressed_at_ms = now_ms;
        state->long_emitted = false;
        emit_event(
            buttons,
            button,
            R4_SERVICE_ACTION_PRESS,
            now_ms
        );
        return;
    }

    emit_event(
        buttons,
        button,
        R4_SERVICE_ACTION_RELEASE,
        now_ms
    );

    if (state->long_emitted) {
        state->short_pending = false;
        return;
    }

    if (state->chord_used) {
        state->short_pending = false;
        return;
    }

    if (
        state->short_pending &&
        !time_reached(now_ms, state->short_deadline_ms)
    ) {
        state->short_pending = false;
        emit_event(
            buttons,
            button,
            R4_SERVICE_ACTION_DOUBLE,
            now_ms
        );
        return;
    }

    state->short_pending = true;
    state->short_deadline_ms =
        now_ms + buttons->config.double_press_ms;
}

void r4_service_buttons_update(
    r4_service_buttons_t *buttons,
    r4_service_button_id_t button,
    bool pressed,
    uint32_t now_ms
) {
    if (
        buttons == NULL ||
        button < 0 ||
        button >= R4_SERVICE_BUTTON_COUNT
    ) {
        return;
    }

    r4_service_button_state_t *state = &buttons->buttons[button];

    if (pressed != state->raw_pressed) {
        state->raw_pressed = pressed;
        state->raw_changed_at_ms = now_ms;

        if (pressed) {
            state->chord_used = false;
        }
    }

    if (
        state->raw_pressed != state->stable_pressed &&
        time_reached(
            now_ms,
            state->raw_changed_at_ms + buttons->config.debounce_ms
        )
    ) {
        apply_stable_transition(buttons, button, now_ms);
    }
}

void r4_service_buttons_update_with_chord(
    r4_service_buttons_t *buttons,
    r4_service_button_id_t button,
    bool pressed,
    bool chord_active,
    uint32_t now_ms
) {
    r4_service_buttons_update(
        buttons,
        button,
        pressed,
        now_ms
    );

    if (
        buttons == NULL ||
        button < 0 ||
        button >= R4_SERVICE_BUTTON_COUNT ||
        !pressed ||
        !chord_active
    ) {
        return;
    }

    r4_service_button_state_t *state = &buttons->buttons[button];
    state->chord_used = true;
    state->short_pending = false;
}

void r4_service_buttons_tick(
    r4_service_buttons_t *buttons,
    uint32_t now_ms
) {
    if (buttons == NULL) {
        return;
    }

    for (
        r4_service_button_id_t button = R4_SERVICE_BUTTON_CAPTURE;
        button < R4_SERVICE_BUTTON_COUNT;
        button = (r4_service_button_id_t)(button + 1)
    ) {
        r4_service_button_state_t *state = &buttons->buttons[button];

        if (
            state->raw_pressed != state->stable_pressed &&
            time_reached(
                now_ms,
                state->raw_changed_at_ms +
                    buttons->config.debounce_ms
            )
        ) {
            apply_stable_transition(buttons, button, now_ms);
        }

        if (
            state->stable_pressed &&
            !state->long_emitted &&
            !state->chord_used &&
            time_reached(
                now_ms,
                state->pressed_at_ms +
                    buttons->config.long_press_ms
            )
        ) {
            state->long_emitted = true;
            state->short_pending = false;
            emit_event(
                buttons,
                button,
                R4_SERVICE_ACTION_LONG,
                now_ms
            );
        }

        if (
            state->short_pending &&
            time_reached(now_ms, state->short_deadline_ms)
        ) {
            state->short_pending = false;
            emit_event(
                buttons,
                button,
                R4_SERVICE_ACTION_SHORT,
                now_ms
            );
        }
    }
}

bool r4_service_buttons_pop(
    r4_service_buttons_t *buttons,
    r4_service_event_t *event
) {
    if (
        buttons == NULL ||
        event == NULL ||
        buttons->event_count == 0
    ) {
        return false;
    }

    *event = buttons->events[buttons->event_head];
    buttons->event_head =
        (buttons->event_head + 1U) %
        R4_SERVICE_EVENT_QUEUE_CAPACITY;
    --buttons->event_count;
    return true;
}

const char *r4_service_button_name(r4_service_button_id_t button) {
    switch (button) {
        case R4_SERVICE_BUTTON_CAPTURE:
            return "CAPTURE";

        case R4_SERVICE_BUTTON_R4:
            return "R4";

        case R4_SERVICE_BUTTON_TROPHY:
            return "TROPHY";

        case R4_SERVICE_BUTTON_COUNT:
            break;
    }

    return "UNKNOWN";
}

const char *r4_service_action_name(r4_service_action_t action) {
    switch (action) {
        case R4_SERVICE_ACTION_PRESS:
            return "PRESS";

        case R4_SERVICE_ACTION_RELEASE:
            return "RELEASE";

        case R4_SERVICE_ACTION_SHORT:
            return "SHORT";

        case R4_SERVICE_ACTION_LONG:
            return "LONG";

        case R4_SERVICE_ACTION_DOUBLE:
            return "DOUBLE";
    }

    return "UNKNOWN";
}
