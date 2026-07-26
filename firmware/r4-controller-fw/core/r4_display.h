#ifndef R4_DISPLAY_H
#define R4_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define R4_DISPLAY_TEXT_CAPACITY 48U
#define R4_DISPLAY_ACHIEVEMENT_DURATION_MS 5000U
#define R4_DISPLAY_HOST_WATCHDOG_TIMEOUT_MS 7000U

typedef enum {
    R4_DISPLAY_BOOT = 0,
    R4_DISPLAY_WAITING,
    R4_DISPLAY_HOME,
    R4_DISPLAY_GAME,
    R4_DISPLAY_DIAGNOSTIC,
    R4_DISPLAY_ERROR
} r4_display_screen_t;

typedef struct {
    r4_display_screen_t screen;
    char system[R4_DISPLAY_TEXT_CAPACITY];
    char game[R4_DISPLAY_TEXT_CAPACITY];
    char time[R4_DISPLAY_TEXT_CAPACITY];
    char diagnostic[R4_DISPLAY_TEXT_CAPACITY];
    char error[R4_DISPLAY_TEXT_CAPACITY];
    char error_before_host_error[R4_DISPLAY_TEXT_CAPACITY];
    char achievement[R4_DISPLAY_TEXT_CAPACITY];
    char firmware_version[R4_DISPLAY_TEXT_CAPACITY];
    uint32_t achievement_hide_at_ms;
    uint32_t game_started_at_ms;
    uint32_t game_elapsed_seconds;
    uint32_t remaining_runtime_minutes;
    uint32_t host_last_seen_ms;
    int32_t temperature_millicelsius;
    uint8_t battery_percent;
    uint8_t volume_percent;
    r4_display_screen_t screen_before_host_error;
    bool temperature_available;
    bool battery_available;
    bool volume_available;
    bool external_power;
    bool network_connected;
    bool retroachievements_active;
    bool achievement_visible;
    bool orange_pi_connected;
    bool game_timer_running;
    bool remaining_runtime_available;
    bool host_watchdog_armed;
    bool host_link_lost;
    bool clock_colon_visible;
} r4_display_model_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t *pixels;
    size_t pixel_capacity;
} r4_framebuffer_t;

void r4_display_model_init(r4_display_model_t *model);

void r4_display_show_achievement(
    r4_display_model_t *model,
    uint32_t now_ms
);

void r4_display_start_game(
    r4_display_model_t *model,
    uint32_t now_ms
);

void r4_display_stop_game(r4_display_model_t *model);

void r4_display_arm_host_watchdog(
    r4_display_model_t *model,
    uint32_t now_ms
);

void r4_display_host_activity(
    r4_display_model_t *model,
    uint32_t now_ms
);

void r4_display_tick(
    r4_display_model_t *model,
    uint32_t now_ms
);

bool r4_display_render(
    const r4_display_model_t *model,
    r4_framebuffer_t *framebuffer
);

bool r4_display_write_pbm(
    const char *path,
    const r4_framebuffer_t *framebuffer
);

uint32_t r4_display_hash(const r4_framebuffer_t *framebuffer);

#endif
