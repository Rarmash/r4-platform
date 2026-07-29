#ifndef R4_UPDATE_H
#define R4_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#define R4_UPDATE_TOKEN_TTL_MS 10000U

typedef enum {
    R4_UPDATE_IDLE = 0,
    R4_UPDATE_ARMED
} r4_update_state_t;

typedef enum {
    R4_UPDATE_CONFIRM_OK = 0,
    R4_UPDATE_CONFIRM_NOT_ARMED,
    R4_UPDATE_CONFIRM_EXPIRED,
    R4_UPDATE_CONFIRM_INVALID_TOKEN,
    R4_UPDATE_CONFIRM_ALREADY_USED
} r4_update_confirm_result_t;

typedef struct {
    uint32_t token;
    uint32_t last_used_token;
    uint64_t expires_at_ms;
    bool armed;
    bool has_used_token;
} r4_update_manager_t;

void r4_update_init(r4_update_manager_t *manager);

void r4_update_arm(
    r4_update_manager_t *manager,
    uint32_t token,
    uint64_t now_ms
);

r4_update_state_t r4_update_status(
    r4_update_manager_t *manager,
    uint64_t now_ms,
    uint32_t *remaining_ms
);

r4_update_confirm_result_t r4_update_confirm(
    r4_update_manager_t *manager,
    uint32_t token,
    uint64_t now_ms
);

bool r4_update_parse_token(const char *text, uint32_t *token);

#endif
