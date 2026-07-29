#ifndef R4_PROTOCOL_H
#define R4_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "r4_controller.h"
#include "r4_service_buttons.h"

#define R4_PROTOCOL_MAX_LINE_LENGTH 255U
#define R4_PROTOCOL_PAYLOAD_CAPACITY 224U

typedef enum {
    R4_COMMAND_INVALID = 0,
    R4_COMMAND_UNKNOWN,
    R4_COMMAND_PING,
    R4_COMMAND_VERSION,
    R4_COMMAND_INPUT,
    R4_COMMAND_STATUS,
    R4_COMMAND_HELP,
    R4_COMMAND_LED_SET,
    R4_COMMAND_LED_FLASH,
    R4_COMMAND_LED_OFF,
    R4_COMMAND_EVENT_NEXT,
    R4_COMMAND_FRAMEBUFFER_INFO,
    R4_COMMAND_FRAMEBUFFER_CHUNK,
    R4_COMMAND_UPDATE_ARM,
    R4_COMMAND_UPDATE_CONFIRM,
    R4_COMMAND_UPDATE_STATUS,
    R4_COMMAND_HOST_HEARTBEAT,
    R4_COMMAND_HOST_STATE,
    R4_COMMAND_HOST_GAME,
    R4_COMMAND_HOST_RA,
    R4_COMMAND_HOST_ACHIEVEMENT,
    R4_COMMAND_HOST_CAPTURE,
    R4_COMMAND_HOST_CARD,
    R4_COMMAND_HOST_TELEMETRY,
    R4_COMMAND_TEST_TRIGGERS,
    R4_COMMAND_TEST_BUTTON
} r4_command_type_t;

typedef enum {
    R4_PARSE_OK = 0,
    R4_PARSE_EMPTY,
    R4_PARSE_TOO_LONG,
    R4_PARSE_INCOMPLETE,
    R4_PARSE_INVALID_VALUE,
    R4_PARSE_UNKNOWN
} r4_parse_status_t;

typedef struct {
    r4_command_type_t type;
    int values[4];
    char payload[R4_PROTOCOL_PAYLOAD_CAPACITY];
} r4_command_t;

r4_parse_status_t r4_protocol_parse(
    const char *line,
    size_t length,
    r4_command_t *command
);

bool r4_protocol_format_input(
    char *buffer,
    size_t capacity,
    const r4_controller_state_t *state
);

bool r4_protocol_format_status(
    char *buffer,
    size_t capacity,
    const char *firmware_version,
    uint8_t led_red,
    uint8_t led_green,
    uint8_t led_blue,
    uint8_t base_red,
    uint8_t base_green,
    uint8_t base_blue,
    bool flash_active,
    const r4_controller_state_t *state
);

bool r4_protocol_format_event(
    char *buffer,
    size_t capacity,
    const r4_service_event_t *event
);

bool r4_protocol_find_field(
    const char *payload,
    const char *key,
    char *value,
    size_t value_capacity
);

bool r4_protocol_decode_hex(
    const char *encoded,
    char *decoded,
    size_t decoded_capacity
);

const char *r4_parse_status_name(r4_parse_status_t status);

#endif
