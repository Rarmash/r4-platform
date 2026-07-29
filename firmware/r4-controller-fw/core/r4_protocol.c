#include "r4_protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void reset_command(r4_command_t *command) {
    memset(command, 0, sizeof(*command));
    command->type = R4_COMMAND_INVALID;
}

static bool copy_payload(
    r4_command_t *command,
    const char *payload
) {
    const size_t length = strlen(payload);

    if (length >= sizeof(command->payload)) {
        return false;
    }

    memcpy(command->payload, payload, length + 1U);
    return true;
}

static bool parse_exact_ints(
    const char *text,
    const char *format,
    int *first,
    int *second,
    int *third,
    int *fourth,
    int expected
) {
    char trailing;
    int parsed;

    switch (expected) {
        case 2:
            parsed = sscanf(
                text,
                format,
                first,
                second,
                &trailing
            );
            break;

        case 3:
            parsed = sscanf(
                text,
                format,
                first,
                second,
                third,
                &trailing
            );
            break;

        case 4:
            parsed = sscanf(
                text,
                format,
                first,
                second,
                third,
                fourth,
                &trailing
            );
            break;

        default:
            return false;
    }

    return parsed == expected;
}

r4_parse_status_t r4_protocol_parse(
    const char *line,
    size_t length,
    r4_command_t *command
) {
    if (line == NULL || command == NULL) {
        return R4_PARSE_INVALID_VALUE;
    }

    reset_command(command);

    if (length == 0) {
        return R4_PARSE_EMPTY;
    }

    if (length > R4_PROTOCOL_MAX_LINE_LENGTH) {
        return R4_PARSE_TOO_LONG;
    }

    char text[R4_PROTOCOL_MAX_LINE_LENGTH + 1U];
    memcpy(text, line, length);
    text[length] = '\0';

    if (strcmp(text, "PING") == 0) {
        command->type = R4_COMMAND_PING;
    } else if (strcmp(text, "VERSION") == 0) {
        command->type = R4_COMMAND_VERSION;
    } else if (strcmp(text, "INPUT") == 0) {
        command->type = R4_COMMAND_INPUT;
    } else if (strcmp(text, "STATUS") == 0) {
        command->type = R4_COMMAND_STATUS;
    } else if (strcmp(text, "HELP") == 0) {
        command->type = R4_COMMAND_HELP;
    } else if (strcmp(text, "LED OFF") == 0) {
        command->type = R4_COMMAND_LED_OFF;
    } else if (strcmp(text, "EVENT NEXT") == 0) {
        command->type = R4_COMMAND_EVENT_NEXT;
    } else if (strcmp(text, "FRAMEBUFFER INFO") == 0) {
        command->type = R4_COMMAND_FRAMEBUFFER_INFO;
    } else if (strncmp(text, "FRAMEBUFFER CHUNK ", 18) == 0) {
        command->type = R4_COMMAND_FRAMEBUFFER_CHUNK;
        if (!copy_payload(command, text + 18)) {
            return R4_PARSE_TOO_LONG;
        }
    } else if (strcmp(text, "HOST HEARTBEAT") == 0) {
        command->type = R4_COMMAND_HOST_HEARTBEAT;
    } else if (strncmp(text, "LED FLASH", 9) == 0) {
        if (
            !parse_exact_ints(
                text,
                "LED FLASH %d %d %d %d %c",
                &command->values[0],
                &command->values[1],
                &command->values[2],
                &command->values[3],
                4
            )
        ) {
            return R4_PARSE_INCOMPLETE;
        }

        command->type = R4_COMMAND_LED_FLASH;
    } else if (strncmp(text, "LED", 3) == 0) {
        if (
            !parse_exact_ints(
                text,
                "LED %d %d %d %c",
                &command->values[0],
                &command->values[1],
                &command->values[2],
                NULL,
                3
            )
        ) {
            return R4_PARSE_INCOMPLETE;
        }

        command->type = R4_COMMAND_LED_SET;
    } else if (strncmp(text, "HOST STATE ", 11) == 0) {
        command->type = R4_COMMAND_HOST_STATE;
        if (!copy_payload(command, text + 11)) {
            return R4_PARSE_TOO_LONG;
        }
    } else if (strncmp(text, "HOST GAME ", 10) == 0) {
        command->type = R4_COMMAND_HOST_GAME;
        if (!copy_payload(command, text + 10)) {
            return R4_PARSE_TOO_LONG;
        }
    } else if (strncmp(text, "HOST RA ", 8) == 0) {
        command->type = R4_COMMAND_HOST_RA;
        if (!copy_payload(command, text + 8)) {
            return R4_PARSE_TOO_LONG;
        }
    } else if (strncmp(text, "HOST ACHIEVEMENT ", 17) == 0) {
        command->type = R4_COMMAND_HOST_ACHIEVEMENT;
        if (!copy_payload(command, text + 17)) {
            return R4_PARSE_TOO_LONG;
        }
    } else if (strncmp(text, "HOST CAPTURE ", 13) == 0) {
        command->type = R4_COMMAND_HOST_CAPTURE;
        if (
            strcmp(text, "HOST CAPTURE STATUS=BUSY") != 0 &&
            strcmp(text, "HOST CAPTURE STATUS=SAVED") != 0 &&
            strcmp(text, "HOST CAPTURE STATUS=ERROR") != 0 &&
            strcmp(
                text,
                "HOST CAPTURE TYPE=SCREENSHOT STATUS=BUSY"
            ) != 0 &&
            strcmp(
                text,
                "HOST CAPTURE TYPE=SCREENSHOT STATUS=SAVED"
            ) != 0 &&
            strcmp(
                text,
                "HOST CAPTURE TYPE=SCREENSHOT STATUS=ERROR"
            ) != 0 &&
            strcmp(
                text,
                "HOST CAPTURE TYPE=CLIP STATUS=BUFFERING"
            ) != 0 &&
            strcmp(
                text,
                "HOST CAPTURE TYPE=CLIP STATUS=SAVING"
            ) != 0 &&
            strcmp(
                text,
                "HOST CAPTURE TYPE=CLIP STATUS=SAVED"
            ) != 0 &&
            strcmp(
                text,
                "HOST CAPTURE TYPE=CLIP STATUS=ERROR"
            ) != 0 &&
            strcmp(
                text,
                "HOST CAPTURE TYPE=CLIP STATUS=UNAVAILABLE"
            ) != 0
        ) {
            return R4_PARSE_INVALID_VALUE;
        }
        if (!copy_payload(command, text + 13)) {
            return R4_PARSE_TOO_LONG;
        }
    } else if (strncmp(text, "HOST CARD ", 10) == 0) {
        command->type = R4_COMMAND_HOST_CARD;
        if (
            strcmp(text, "HOST CARD STATE=INSERTED") != 0 &&
            strcmp(text, "HOST CARD STATE=READY") != 0 &&
            strcmp(text, "HOST CARD STATE=BUSY") != 0 &&
            strcmp(text, "HOST CARD STATE=EJECTED") != 0 &&
            strcmp(text, "HOST CARD STATE=ERROR") != 0
        ) {
            return R4_PARSE_INVALID_VALUE;
        }
        if (!copy_payload(command, text + 10)) {
            return R4_PARSE_TOO_LONG;
        }
    } else if (strncmp(text, "HOST TELEMETRY ", 15) == 0) {
        command->type = R4_COMMAND_HOST_TELEMETRY;
        if (!copy_payload(command, text + 15)) {
            return R4_PARSE_TOO_LONG;
        }
    } else if (strncmp(text, "TEST TRIGGERS", 13) == 0) {
        if (
            !parse_exact_ints(
                text,
                "TEST TRIGGERS %d %d %c",
                &command->values[0],
                &command->values[1],
                NULL,
                NULL,
                2
            )
        ) {
            return R4_PARSE_INCOMPLETE;
        }

        command->type = R4_COMMAND_TEST_TRIGGERS;
    } else if (strncmp(text, "TEST BUTTON ", 12) == 0) {
        command->type = R4_COMMAND_TEST_BUTTON;
        if (!copy_payload(command, text + 12)) {
            return R4_PARSE_TOO_LONG;
        }
    } else {
        command->type = R4_COMMAND_UNKNOWN;
        return R4_PARSE_UNKNOWN;
    }

    return R4_PARSE_OK;
}

bool r4_protocol_format_input(
    char *buffer,
    size_t capacity,
    const r4_controller_state_t *state
) {
    if (buffer == NULL || capacity == 0 || state == NULL) {
        return false;
    }

    const int written = snprintf(
        buffer,
        capacity,
        "LX=%d LY=%d RX=%d RY=%d "
        "HAT=%u BUTTONS=0x%08lX "
        "LT=%u RT=%u "
        "LT_STATUS=%s RT_STATUS=%s",
        (int)state->left_x,
        (int)state->left_y,
        (int)state->right_x,
        (int)state->right_y,
        (unsigned int)state->hat,
        (unsigned long)state->buttons,
        (unsigned int)state->left_trigger,
        (unsigned int)state->right_trigger,
        r4_analog_status_name(state->left_trigger_status),
        r4_analog_status_name(state->right_trigger_status)
    );

    return written >= 0 && (size_t)written < capacity;
}

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
) {
    if (
        buffer == NULL ||
        capacity == 0 ||
        firmware_version == NULL ||
        state == NULL
    ) {
        return false;
    }

    const int written = snprintf(
        buffer,
        capacity,
        "FW=%s "
        "LED=%u,%u,%u "
        "BASE=%u,%u,%u "
        "FLASH=%u "
        "LX=%d LY=%d RX=%d RY=%d "
        "HAT=%u BUTTONS=0x%08lX "
        "LT=%u RT=%u "
        "LT_STATUS=%s RT_STATUS=%s",
        firmware_version,
        (unsigned int)led_red,
        (unsigned int)led_green,
        (unsigned int)led_blue,
        (unsigned int)base_red,
        (unsigned int)base_green,
        (unsigned int)base_blue,
        flash_active ? 1U : 0U,
        (int)state->left_x,
        (int)state->left_y,
        (int)state->right_x,
        (int)state->right_y,
        (unsigned int)state->hat,
        (unsigned long)state->buttons,
        (unsigned int)state->left_trigger,
        (unsigned int)state->right_trigger,
        r4_analog_status_name(state->left_trigger_status),
        r4_analog_status_name(state->right_trigger_status)
    );

    return written >= 0 && (size_t)written < capacity;
}

bool r4_protocol_format_event(
    char *buffer,
    size_t capacity,
    const r4_service_event_t *event
) {
    if (buffer == NULL || capacity == 0 || event == NULL) {
        return false;
    }

    const int written = snprintf(
        buffer,
        capacity,
        "EVENT BUTTON=%s ACTION=%s TIME_MS=%lu SEQ=%lu",
        r4_service_button_name(event->button),
        r4_service_action_name(event->action),
        (unsigned long)event->timestamp_ms,
        (unsigned long)event->sequence
    );

    return written >= 0 && (size_t)written < capacity;
}

bool r4_protocol_find_field(
    const char *payload,
    const char *key,
    char *value,
    size_t value_capacity
) {
    if (
        payload == NULL ||
        key == NULL ||
        value == NULL ||
        value_capacity == 0
    ) {
        return false;
    }

    const size_t key_length = strlen(key);
    const char *cursor = payload;

    while (*cursor != '\0') {
        while (*cursor == ' ') {
            ++cursor;
        }

        const char *token_end = strchr(cursor, ' ');

        if (token_end == NULL) {
            token_end = cursor + strlen(cursor);
        }

        if (
            (size_t)(token_end - cursor) > key_length + 1U &&
            strncmp(cursor, key, key_length) == 0 &&
            cursor[key_length] == '='
        ) {
            const char *field = cursor + key_length + 1U;
            const size_t field_length =
                (size_t)(token_end - field);

            if (field_length >= value_capacity) {
                return false;
            }

            memcpy(value, field, field_length);
            value[field_length] = '\0';
            return true;
        }

        cursor = token_end;
    }

    return false;
}

static int hex_value(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }

    character = (char)tolower((unsigned char)character);

    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }

    return -1;
}

bool r4_protocol_decode_hex(
    const char *encoded,
    char *decoded,
    size_t decoded_capacity
) {
    if (
        encoded == NULL ||
        decoded == NULL ||
        decoded_capacity == 0
    ) {
        return false;
    }

    const size_t encoded_length = strlen(encoded);

    if (
        encoded_length % 2U != 0 ||
        encoded_length / 2U >= decoded_capacity
    ) {
        return false;
    }

    for (size_t index = 0; index < encoded_length; index += 2U) {
        const int high = hex_value(encoded[index]);
        const int low = hex_value(encoded[index + 1U]);

        if (high < 0 || low < 0) {
            return false;
        }

        const char value = (char)((high << 4) | low);

        if (value == '\0') {
            return false;
        }

        decoded[index / 2U] = value;
    }

    decoded[encoded_length / 2U] = '\0';
    return true;
}

const char *r4_parse_status_name(r4_parse_status_t status) {
    switch (status) {
        case R4_PARSE_OK:
            return "OK";
        case R4_PARSE_EMPTY:
            return "EMPTY";
        case R4_PARSE_TOO_LONG:
            return "TOO_LONG";
        case R4_PARSE_INCOMPLETE:
            return "INCOMPLETE";
        case R4_PARSE_INVALID_VALUE:
            return "INVALID_VALUE";
        case R4_PARSE_UNKNOWN:
            return "UNKNOWN";
    }

    return "INVALID_VALUE";
}
