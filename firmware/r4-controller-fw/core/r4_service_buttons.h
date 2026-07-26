#ifndef R4_SERVICE_BUTTONS_H
#define R4_SERVICE_BUTTONS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define R4_SERVICE_EVENT_QUEUE_CAPACITY 16U

typedef enum {
    R4_SERVICE_BUTTON_CAPTURE = 0,
    R4_SERVICE_BUTTON_R4,
    R4_SERVICE_BUTTON_TROPHY,
    R4_SERVICE_BUTTON_COUNT
} r4_service_button_id_t;

typedef enum {
    R4_SERVICE_ACTION_PRESS = 0,
    R4_SERVICE_ACTION_RELEASE,
    R4_SERVICE_ACTION_SHORT,
    R4_SERVICE_ACTION_LONG,
    R4_SERVICE_ACTION_DOUBLE
} r4_service_action_t;

typedef struct {
    uint32_t debounce_ms;
    uint32_t long_press_ms;
    uint32_t double_press_ms;
} r4_service_button_config_t;

typedef struct {
    r4_service_button_id_t button;
    r4_service_action_t action;
    uint32_t timestamp_ms;
    uint32_t sequence;
} r4_service_event_t;

typedef struct {
    bool raw_pressed;
    bool stable_pressed;
    bool long_emitted;
    bool short_pending;
    bool chord_used;
    uint32_t raw_changed_at_ms;
    uint32_t pressed_at_ms;
    uint32_t short_deadline_ms;
} r4_service_button_state_t;

typedef struct {
    r4_service_button_config_t config;
    r4_service_button_state_t buttons[R4_SERVICE_BUTTON_COUNT];
    r4_service_event_t events[R4_SERVICE_EVENT_QUEUE_CAPACITY];
    size_t event_head;
    size_t event_count;
    uint32_t next_sequence;
    uint32_t dropped_events;
} r4_service_buttons_t;

void r4_service_buttons_init(
    r4_service_buttons_t *buttons,
    r4_service_button_config_t config
);

void r4_service_buttons_update(
    r4_service_buttons_t *buttons,
    r4_service_button_id_t button,
    bool pressed,
    uint32_t now_ms
);

void r4_service_buttons_update_with_chord(
    r4_service_buttons_t *buttons,
    r4_service_button_id_t button,
    bool pressed,
    bool chord_active,
    uint32_t now_ms
);

void r4_service_buttons_tick(
    r4_service_buttons_t *buttons,
    uint32_t now_ms
);

bool r4_service_buttons_pop(
    r4_service_buttons_t *buttons,
    r4_service_event_t *event
);

const char *r4_service_button_name(r4_service_button_id_t button);
const char *r4_service_action_name(r4_service_action_t action);

#endif
