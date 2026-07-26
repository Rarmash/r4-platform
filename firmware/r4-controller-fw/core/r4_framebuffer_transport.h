#ifndef R4_FRAMEBUFFER_TRANSPORT_H
#define R4_FRAMEBUFFER_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define R4_OLED_PROFILE_WIDTH 128U
#define R4_OLED_PROFILE_HEIGHT 64U
#define R4_FRAMEBUFFER_PACKED_SIZE \
    ((R4_OLED_PROFILE_WIDTH * R4_OLED_PROFILE_HEIGHT) / 8U)
#define R4_FRAMEBUFFER_CHUNK_MAX 96U
#define R4_FRAMEBUFFER_RESPONSE_MAX 255U

bool r4_framebuffer_pack_mono1_msb(
    const uint8_t *pixels,
    size_t pixel_count,
    uint8_t *packed,
    size_t packed_capacity
);

bool r4_framebuffer_format_info(
    char *buffer,
    size_t capacity,
    uint32_t snapshot_id,
    uint32_t hash
);

bool r4_framebuffer_format_chunk(
    char *buffer,
    size_t capacity,
    uint32_t snapshot_id,
    size_t offset,
    const uint8_t *packed,
    size_t packed_size,
    size_t length
);
#endif

