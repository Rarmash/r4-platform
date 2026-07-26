#include "r4_display.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const uint8_t *glyph_rows(char character) {
    static const uint8_t digits[][7] = {
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
        {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
        {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
        {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
        {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
        {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}
    };

    static const uint8_t letters[][7] = {
        {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
        {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
        {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
        {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
        {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
        {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
        {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C},
        {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
        {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
        {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
        {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
        {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
        {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
        {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
        {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
        {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
        {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
        {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04},
        {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},
        {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
        {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}
    };

    static const uint8_t blank[7] =
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t unknown[7] =
        {0x1F, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
    static const uint8_t dash[7] =
        {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    static const uint8_t colon[7] =
        {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
    static const uint8_t period[7] =
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06};
    static const uint8_t slash[7] =
        {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    static const uint8_t percent[7] =
        {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13};
    static const uint8_t underscore[7] =
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F};

    character = (char)toupper((unsigned char)character);

    if (character >= '0' && character <= '9') {
        return digits[character - '0'];
    }

    if (character >= 'A' && character <= 'Z') {
        return letters[character - 'A'];
    }

    switch (character) {
        case ' ':
            return blank;
        case '-':
            return dash;
        case ':':
            return colon;
        case '.':
            return period;
        case '/':
            return slash;
        case '%':
            return percent;
        case '_':
            return underscore;
        default:
            return unknown;
    }
}

static void set_pixel(
    r4_framebuffer_t *framebuffer,
    int x,
    int y,
    bool value
) {
    if (
        x < 0 ||
        y < 0 ||
        x >= framebuffer->width ||
        y >= framebuffer->height
    ) {
        return;
    }

    framebuffer->pixels[
        (size_t)y * framebuffer->width + (size_t)x
    ] = value ? 1U : 0U;
}

static void draw_rect(
    r4_framebuffer_t *framebuffer,
    int x,
    int y,
    int width,
    int height,
    bool filled
) {
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            if (
                filled ||
                row == 0 ||
                column == 0 ||
                row == height - 1 ||
                column == width - 1
            ) {
                set_pixel(
                    framebuffer,
                    x + column,
                    y + row,
                    true
                );
            }
        }
    }
}

static void draw_character(
    r4_framebuffer_t *framebuffer,
    int x,
    int y,
    char character,
    int scale
) {
    const uint8_t *rows = glyph_rows(character);

    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
            if ((rows[row] & (UINT8_C(0x10) >> column)) == 0) {
                continue;
            }

            draw_rect(
                framebuffer,
                x + column * scale,
                y + row * scale,
                scale,
                scale,
                true
            );
        }
    }
}

static void draw_text_range(
    r4_framebuffer_t *framebuffer,
    int x,
    int y,
    const char *text,
    int scale,
    size_t start,
    size_t maximum_characters
) {
    if (text == NULL) {
        return;
    }

    for (
        size_t index = 0;
        text[start + index] != '\0' &&
            index < maximum_characters;
        ++index
    ) {
        draw_character(
            framebuffer,
            x + (int)index * 6 * scale,
            y,
            text[start + index],
            scale
        );
    }
}

static void draw_text(
    r4_framebuffer_t *framebuffer,
    int x,
    int y,
    const char *text,
    int scale,
    size_t maximum_characters
) {
    draw_text_range(
        framebuffer,
        x,
        y,
        text,
        scale,
        0,
        maximum_characters
    );
}

static void draw_wrapped_text(
    r4_framebuffer_t *framebuffer,
    int x,
    int y,
    const char *text,
    size_t line_characters
) {
    if (text == NULL || text[0] == '\0') {
        return;
    }

    const size_t length = strlen(text);
    size_t split = length < line_characters
        ? length
        : line_characters;

    if (length > line_characters) {
        for (size_t index = split; index > 0; --index) {
            if (text[index] == ' ') {
                split = index;
                break;
            }
        }
    }

    draw_text_range(
        framebuffer,
        x,
        y,
        text,
        1,
        0,
        split
    );

    size_t second_start = split;

    while (text[second_start] == ' ') {
        ++second_start;
    }

    if (text[second_start] != '\0') {
        draw_text_range(
            framebuffer,
            x,
            y + 9,
            text,
            1,
            second_start,
            line_characters
        );
    }
}

void r4_display_model_init(r4_display_model_t *model) {
    if (model == NULL) {
        return;
    }

    memset(model, 0, sizeof(*model));
    model->screen = R4_DISPLAY_BOOT;
    model->clock_colon_visible = true;
}

void r4_display_show_achievement(
    r4_display_model_t *model,
    uint32_t now_ms
) {
    if (model == NULL) {
        return;
    }

    model->achievement_visible = true;
    model->achievement_hide_at_ms =
        now_ms + R4_DISPLAY_ACHIEVEMENT_DURATION_MS;
}

void r4_display_start_game(
    r4_display_model_t *model,
    uint32_t now_ms
) {
    if (model == NULL) {
        return;
    }

    model->game_started_at_ms = now_ms;
    model->game_elapsed_seconds = 0;
    model->game_timer_running = true;
}

void r4_display_stop_game(r4_display_model_t *model) {
    if (model == NULL) {
        return;
    }

    model->game_timer_running = false;
    model->game_elapsed_seconds = 0;
}

void r4_display_arm_host_watchdog(
    r4_display_model_t *model,
    uint32_t now_ms
) {
    if (model == NULL) {
        return;
    }

    model->host_watchdog_armed = true;
    r4_display_host_activity(model, now_ms);
}

void r4_display_host_activity(
    r4_display_model_t *model,
    uint32_t now_ms
) {
    if (model == NULL || !model->host_watchdog_armed) {
        return;
    }

    model->host_last_seen_ms = now_ms;
    model->orange_pi_connected = true;

    if (model->host_link_lost) {
        model->host_link_lost = false;
        model->screen = model->screen_before_host_error;

        if (model->screen == R4_DISPLAY_ERROR) {
            snprintf(
                model->error,
                sizeof(model->error),
                "%s",
                model->error_before_host_error
            );
        } else {
            model->error[0] = '\0';
        }
    }
}

void r4_display_tick(
    r4_display_model_t *model,
    uint32_t now_ms
) {
    if (model == NULL) {
        return;
    }

    model->clock_colon_visible =
        ((now_ms / 500U) & 1U) == 0U;

    if (
        model->achievement_visible &&
        now_ms - model->achievement_hide_at_ms <
            UINT32_C(0x80000000)
    ) {
        model->achievement_visible = false;
    }

    if (model->game_timer_running) {
        model->game_elapsed_seconds =
            (now_ms - model->game_started_at_ms) / 1000U;
    }

    if (
        model->host_watchdog_armed &&
        !model->host_link_lost &&
        now_ms - model->host_last_seen_ms >=
            R4_DISPLAY_HOST_WATCHDOG_TIMEOUT_MS
    ) {
        model->host_link_lost = true;
        model->orange_pi_connected = false;
        model->screen_before_host_error = model->screen;
        snprintf(
            model->error_before_host_error,
            sizeof(model->error_before_host_error),
            "%s",
            model->error
        );
        model->screen = R4_DISPLAY_ERROR;
        snprintf(
            model->error,
            sizeof(model->error),
            "HOST LINK LOST"
        );
    }
}

static void render_status_bar(
    const r4_display_model_t *model,
    r4_framebuffer_t *framebuffer
) {
    char clock_text[R4_DISPLAY_TEXT_CAPACITY];
    snprintf(
        clock_text,
        sizeof(clock_text),
        "%s",
        model->time[0] != '\0' ? model->time : "--:--"
    );

    if (!model->clock_colon_visible) {
        char *colon = strchr(clock_text, ':');

        if (colon != NULL) {
            *colon = ' ';
        }
    }

    draw_text(
        framebuffer,
        1,
        1,
        clock_text,
        1,
        5
    );

    draw_text(
        framebuffer,
        38,
        1,
        model->external_power ? "EXT" : "BAT",
        1,
        3
    );

    char battery[8] = "--%";

    if (model->battery_available) {
        snprintf(
            battery,
            sizeof(battery),
            "%u%%",
            (unsigned int)model->battery_percent
        );
    }

    draw_text(framebuffer, 63, 1, battery, 1, 4);

    char runtime[8] = "--";

    if (model->external_power) {
        strcpy(runtime, "POWER");
    } else if (model->remaining_runtime_available) {
        const uint32_t hours =
            model->remaining_runtime_minutes / 60U;
        const uint32_t minutes =
            model->remaining_runtime_minutes % 60U;

        if (hours > 0) {
            snprintf(
                runtime,
                sizeof(runtime),
                "%luH%02lu",
                (unsigned long)(hours > 99U ? 99U : hours),
                (unsigned long)minutes
            );
        } else {
            snprintf(
                runtime,
                sizeof(runtime),
                "%luM",
                (unsigned long)minutes
            );
        }
    }

    draw_text(framebuffer, 99, 1, runtime, 1, 5);

    for (int x = 0; x < framebuffer->width; ++x) {
        set_pixel(framebuffer, x, 9, true);
    }
}

static void render_footer(
    const r4_display_model_t *model,
    r4_framebuffer_t *framebuffer,
    bool game_screen
) {
    const int separator_y = framebuffer->height - 12;

    for (int x = 0; x < framebuffer->width; ++x) {
        set_pixel(framebuffer, x, separator_y, true);
    }

    draw_text(
        framebuffer,
        2,
        separator_y + 3,
        model->retroachievements_active ? "RA ON" : "RA OFF",
        1,
        5
    );

    char volume[8] = "VOL--";

    if (model->volume_available) {
        snprintf(
            volume,
            sizeof(volume),
            "VOL%u",
            (unsigned int)model->volume_percent
        );
    }

    draw_text(
        framebuffer,
        45,
        separator_y + 3,
        volume,
        1,
        6
    );
    if (game_screen) {
        char elapsed[8];
        const uint32_t hours =
            model->game_elapsed_seconds / 3600U;
        const uint32_t minutes =
            (model->game_elapsed_seconds / 60U) % 60U;
        const uint32_t seconds =
            model->game_elapsed_seconds % 60U;

        if (hours > 0) {
            snprintf(
                elapsed,
                sizeof(elapsed),
                "%lu:%02lu",
                (unsigned long)(hours > 99U ? 99U : hours),
                (unsigned long)minutes
            );
        } else {
            snprintf(
                elapsed,
                sizeof(elapsed),
                "%02lu:%02lu",
                (unsigned long)minutes,
                (unsigned long)seconds
            );
        }

        draw_text(
            framebuffer,
            96,
            separator_y + 3,
            elapsed,
            1,
            5
        );
    } else {
        char firmware[8];
        snprintf(
            firmware,
            sizeof(firmware),
            "V%.5s",
            model->firmware_version
        );
        draw_text(
            framebuffer,
            91,
            separator_y + 3,
            firmware,
            1,
            6
        );
    }
}

bool r4_display_render(
    const r4_display_model_t *model,
    r4_framebuffer_t *framebuffer
) {
    if (
        model == NULL ||
        framebuffer == NULL ||
        framebuffer->pixels == NULL ||
        framebuffer->width < 32 ||
        framebuffer->height < 24
    ) {
        return false;
    }

    const size_t required =
        (size_t)framebuffer->width * framebuffer->height;

    if (framebuffer->pixel_capacity < required) {
        return false;
    }

    memset(framebuffer->pixels, 0, required);

    switch (model->screen) {
        case R4_DISPLAY_BOOT:
            draw_text(
                framebuffer,
                framebuffer->width / 2 - 17,
                framebuffer->height / 2 - 14,
                "R4",
                3,
                2
            );
            draw_text(
                framebuffer,
                framebuffer->width / 2 - 12,
                framebuffer->height / 2 + 11,
                "BOOT",
                1,
                4
            );
            break;

        case R4_DISPLAY_WAITING:
            draw_text(framebuffer, 23, 15, "WAITING", 2, 7);
            draw_text(framebuffer, 37, 39, "HOST LINK", 1, 9);
            break;

        case R4_DISPLAY_HOME: {
            render_status_bar(model, framebuffer);
            draw_text(framebuffer, 53, 13, "R4", 2, 2);
            draw_text(framebuffer, 17, 32, "BATOCERA", 2, 8);
            render_footer(model, framebuffer, false);
            break;
        }

        case R4_DISPLAY_GAME:
            render_status_bar(model, framebuffer);
            draw_text(framebuffer, 3, 13, model->system, 1, 20);
            draw_wrapped_text(
                framebuffer,
                3,
                25,
                model->game,
                20
            );
            render_footer(model, framebuffer, true);
            break;

        case R4_DISPLAY_DIAGNOSTIC: {
            draw_text(framebuffer, 3, 3, "DIAGNOSTIC", 1, 10);
            draw_rect(
                framebuffer,
                0,
                12,
                framebuffer->width,
                framebuffer->height - 12,
                false
            );
            draw_text(
                framebuffer,
                4,
                18,
                model->diagnostic,
                1,
                20
            );
            char temperature[16] = "TEMP --";

            if (model->temperature_available) {
                snprintf(
                    temperature,
                    sizeof(temperature),
                    "TEMP %ldC",
                    (long)(model->temperature_millicelsius / 1000)
                );
            }

            draw_text(
                framebuffer,
                4,
                framebuffer->height - 11,
                temperature,
                1,
                10
            );
            break;
        }

        case R4_DISPLAY_ERROR:
            draw_rect(
                framebuffer,
                0,
                0,
                framebuffer->width,
                framebuffer->height,
                false
            );
            draw_text(framebuffer, 8, 8, "ERROR", 2, 5);
            draw_text(
                framebuffer,
                4,
                36,
                model->error,
                1,
                20
            );
            break;
    }

    if (model->achievement_visible) {
        const int popup_height = 22;
        const int popup_y = framebuffer->height - popup_height;

        for (int y = popup_y; y < framebuffer->height; ++y) {
            for (int x = 0; x < framebuffer->width; ++x) {
                set_pixel(framebuffer, x, y, false);
            }
        }

        draw_rect(framebuffer, 0, popup_y, framebuffer->width, popup_height, false);

        draw_text(
            framebuffer,
            3,
            popup_y + 3,
            "ACHIEVEMENT",
            1,
            11
        );
        draw_text(
            framebuffer,
            3,
            popup_y + 12,
            model->achievement,
            1,
            20
        );
    }

    return true;
}

bool r4_display_write_pbm(
    const char *path,
    const r4_framebuffer_t *framebuffer
) {
    if (
        path == NULL ||
        framebuffer == NULL ||
        framebuffer->pixels == NULL
    ) {
        return false;
    }

    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        return false;
    }

    fprintf(
        file,
        "P1\n%u %u\n",
        (unsigned int)framebuffer->width,
        (unsigned int)framebuffer->height
    );

    for (uint16_t y = 0; y < framebuffer->height; ++y) {
        for (uint16_t x = 0; x < framebuffer->width; ++x) {
            const uint8_t pixel =
                framebuffer->pixels[
                    (size_t)y * framebuffer->width + x
                ];

            fputc(pixel ? '1' : '0', file);
            fputc(
                x + 1U == framebuffer->width ? '\n' : ' ',
                file
            );
        }
    }

    return fclose(file) == 0;
}

uint32_t r4_display_hash(const r4_framebuffer_t *framebuffer) {
    if (
        framebuffer == NULL ||
        framebuffer->pixels == NULL
    ) {
        return 0;
    }

    uint32_t hash = UINT32_C(2166136261);
    const size_t count =
        (size_t)framebuffer->width * framebuffer->height;

    for (size_t index = 0; index < count; ++index) {
        hash ^= framebuffer->pixels[index];
        hash *= UINT32_C(16777619);
    }

    return hash;
}
