#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define make_directory(path) _mkdir(path)
#else
#include <sys/stat.h>
#define make_directory(path) mkdir(path, 0755)
#endif

#include "r4_display.h"

typedef void (*scenario_setup_t)(r4_display_model_t *model);

typedef struct {
    const char *name;
    scenario_setup_t setup;
    uint32_t expected_128x64_hash;
} scenario_t;

static void setup_boot(r4_display_model_t *model) {
    model->screen = R4_DISPLAY_BOOT;
}

static void setup_waiting(r4_display_model_t *model) {
    model->screen = R4_DISPLAY_WAITING;
}

static void setup_home(r4_display_model_t *model) {
    model->screen = R4_DISPLAY_HOME;
    strcpy(model->firmware_version, "0.8.0");
    strcpy(model->time, "12:34");
    model->battery_available = true;
    model->battery_percent = 78;
    model->remaining_runtime_available = true;
    model->remaining_runtime_minutes = 155;
    model->volume_available = true;
    model->volume_percent = 65;
    model->network_connected = true;
    model->retroachievements_active = true;
    model->temperature_available = true;
    model->temperature_millicelsius = 42125;
    model->orange_pi_connected = true;
}

static void setup_game(r4_display_model_t *model) {
    setup_home(model);
    model->screen = R4_DISPLAY_GAME;
    strcpy(model->system, "NES");
    strcpy(model->game, "SUPER MARIO");
    r4_display_start_game(model, 1000);
    r4_display_tick(model, 151000);
}

static void setup_achievement(r4_display_model_t *model) {
    setup_game(model);
    model->achievement_visible = true;
    strcpy(model->achievement, "FIRST WIN");
}

static void setup_diagnostic(r4_display_model_t *model) {
    model->screen = R4_DISPLAY_DIAGNOSTIC;
    strcpy(model->diagnostic, "CDC OK ADC NO SOURCE");
    model->temperature_available = true;
    model->temperature_millicelsius = 42125;
}

static void setup_error(r4_display_model_t *model) {
    model->screen = R4_DISPLAY_ERROR;
    strcpy(model->error, "ORANGE PI LINK LOST");
}

static const scenario_t scenarios[] = {
    {"boot", setup_boot, UINT32_C(0xC8B14A84)},
    {"waiting", setup_waiting, UINT32_C(0x717C7A4F)},
    {"home", setup_home, UINT32_C(0xA00E630B)},
    {"game", setup_game, UINT32_C(0x05667CE6)},
    {"achievement", setup_achievement, UINT32_C(0xFB917295)},
    {"diagnostic", setup_diagnostic, UINT32_C(0x55B0FB19)},
    {"error", setup_error, UINT32_C(0x755B64C2)}
};

static bool parse_size(
    const char *value,
    uint16_t *result
) {
    char *end = NULL;
    const long parsed = strtol(value, &end, 10);

    if (
        end == value ||
        *end != '\0' ||
        parsed < 32 ||
        parsed > 1024
    ) {
        return false;
    }

    *result = (uint16_t)parsed;
    return true;
}

static bool ensure_directory(const char *path) {
    if (make_directory(path) == 0) {
        return true;
    }

    return errno == EEXIST;
}

static bool render_scenario(
    const scenario_t *scenario,
    const char *output_directory,
    uint16_t width,
    uint16_t height
) {
    const size_t pixel_count = (size_t)width * height;
    uint8_t *pixels = calloc(pixel_count, sizeof(*pixels));

    if (pixels == NULL) {
        return false;
    }

    r4_display_model_t model;
    r4_display_model_init(&model);
    scenario->setup(&model);

    r4_framebuffer_t framebuffer = {
        .width = width,
        .height = height,
        .pixels = pixels,
        .pixel_capacity = pixel_count
    };

    char path[512];
    const int length = snprintf(
        path,
        sizeof(path),
        "%s/%s-%ux%u.pbm",
        output_directory,
        scenario->name,
        (unsigned int)width,
        (unsigned int)height
    );

    bool success =
        length > 0 &&
        (size_t)length < sizeof(path) &&
        r4_display_render(&model, &framebuffer) &&
        r4_display_write_pbm(path, &framebuffer);

    if (success) {
        const uint32_t hash = r4_display_hash(&framebuffer);

        if (
            width == 128 &&
            height == 64 &&
            hash != scenario->expected_128x64_hash
        ) {
            fprintf(
                stderr,
                "%s snapshot mismatch: expected %08lX, got %08lX\n",
                scenario->name,
                (unsigned long)scenario->expected_128x64_hash,
                (unsigned long)hash
            );
            success = false;
        }

        printf(
            "%s hash=%08lX\n",
            path,
            (unsigned long)hash
        );
    }

    free(pixels);
    return success;
}

static void print_usage(const char *program) {
    fprintf(
        stderr,
        "Usage: %s --output-dir DIR "
        "[--width 128] [--height 64] "
        "[--scenario all|NAME]\n",
        program
    );
}

int main(int argc, char **argv) {
    const char *output_directory = NULL;
    const char *selected_scenario = "all";
    uint16_t width = 128;
    uint16_t height = 64;

    for (int index = 1; index < argc; ++index) {
        if (
            strcmp(argv[index], "--output-dir") == 0 &&
            index + 1 < argc
        ) {
            output_directory = argv[++index];
        } else if (
            strcmp(argv[index], "--width") == 0 &&
            index + 1 < argc
        ) {
            if (!parse_size(argv[++index], &width)) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (
            strcmp(argv[index], "--height") == 0 &&
            index + 1 < argc
        ) {
            if (!parse_size(argv[++index], &height)) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (
            strcmp(argv[index], "--scenario") == 0 &&
            index + 1 < argc
        ) {
            selected_scenario = argv[++index];
        } else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (
        output_directory == NULL ||
        !ensure_directory(output_directory)
    ) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    bool rendered = false;

    for (
        size_t index = 0;
        index < sizeof(scenarios) / sizeof(scenarios[0]);
        ++index
    ) {
        if (
            strcmp(selected_scenario, "all") != 0 &&
            strcmp(selected_scenario, scenarios[index].name) != 0
        ) {
            continue;
        }

        if (
            !render_scenario(
                &scenarios[index],
                output_directory,
                width,
                height
            )
        ) {
            return EXIT_FAILURE;
        }

        rendered = true;
    }

    if (!rendered) {
        fprintf(
            stderr,
            "Unknown scenario: %s\n",
            selected_scenario
        );
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
