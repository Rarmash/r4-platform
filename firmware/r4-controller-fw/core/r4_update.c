#include "r4_update.h"

#include <stddef.h>
#include <string.h>

static int hex_digit_value(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return -1;
}

void r4_update_init(r4_update_manager_t *manager) {
    if (manager == NULL) {
        return;
    }
    memset(manager, 0, sizeof(*manager));
}

void r4_update_arm(
    r4_update_manager_t *manager,
    uint32_t token,
    uint64_t now_ms
) {
    if (manager == NULL) {
        return;
    }
    manager->token = token;
    manager->expires_at_ms = now_ms + R4_UPDATE_TOKEN_TTL_MS;
    manager->armed = true;
}

r4_update_state_t r4_update_status(
    r4_update_manager_t *manager,
    uint64_t now_ms,
    uint32_t *remaining_ms
) {
    if (remaining_ms != NULL) {
        *remaining_ms = 0;
    }
    if (manager == NULL || !manager->armed) {
        return R4_UPDATE_IDLE;
    }
    if (now_ms >= manager->expires_at_ms) {
        manager->armed = false;
        return R4_UPDATE_IDLE;
    }
    if (remaining_ms != NULL) {
        const uint64_t remaining =
            manager->expires_at_ms - now_ms;
        *remaining_ms =
            remaining > UINT32_MAX
                ? UINT32_MAX
                : (uint32_t)remaining;
    }
    return R4_UPDATE_ARMED;
}

r4_update_confirm_result_t r4_update_confirm(
    r4_update_manager_t *manager,
    uint32_t token,
    uint64_t now_ms
) {
    if (manager == NULL) {
        return R4_UPDATE_CONFIRM_NOT_ARMED;
    }
    if (!manager->armed) {
        if (
            manager->has_used_token &&
            token == manager->last_used_token
        ) {
            return R4_UPDATE_CONFIRM_ALREADY_USED;
        }
        return R4_UPDATE_CONFIRM_NOT_ARMED;
    }
    if (now_ms >= manager->expires_at_ms) {
        manager->armed = false;
        return R4_UPDATE_CONFIRM_EXPIRED;
    }
    if (token != manager->token) {
        return R4_UPDATE_CONFIRM_INVALID_TOKEN;
    }

    manager->armed = false;
    manager->has_used_token = true;
    manager->last_used_token = token;
    return R4_UPDATE_CONFIRM_OK;
}

bool r4_update_parse_token(const char *text, uint32_t *token) {
    if (text == NULL || token == NULL || strlen(text) != 8U) {
        return false;
    }
    uint32_t parsed = 0;
    for (size_t index = 0; index < 8U; ++index) {
        const int digit = hex_digit_value(text[index]);
        if (digit < 0) {
            return false;
        }
        parsed = (parsed << 4U) | (uint32_t)digit;
    }
    *token = parsed;
    return true;
}
