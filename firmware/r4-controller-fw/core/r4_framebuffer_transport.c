#include "r4_framebuffer_transport.h"

#include <stdio.h>

bool r4_framebuffer_pack_mono1_msb(
    const uint8_t *pixels,
    size_t pixel_count,
    uint8_t *packed,
    size_t packed_capacity
) {
    if (
        pixels == NULL ||
        packed == NULL ||
        pixel_count % 8U != 0 ||
        packed_capacity < pixel_count / 8U
    ) {
        return false;
    }

    const size_t packed_size = pixel_count / 8U;

    for (size_t byte_index = 0; byte_index < packed_size; ++byte_index) {
        uint8_t value = 0;

        for (size_t bit = 0; bit < 8U; ++bit) {
            if (pixels[byte_index * 8U + bit] != 0) {
                value |= (uint8_t)(UINT8_C(0x80) >> bit);
            }
        }

        packed[byte_index] = value;
    }

    return true;
}

bool r4_framebuffer_format_info(
    char *buffer,
    size_t capacity,
    uint32_t snapshot_id,
    uint32_t hash
) {
    if (buffer == NULL || capacity == 0 || snapshot_id == 0) {
        return false;
    }

    const int written = snprintf(
        buffer,
        capacity,
        "FRAMEBUFFER INFO ID=%lu WIDTH=%u HEIGHT=%u "
        "FORMAT=MONO1_MSB BYTES=%u HASH=%08lX CHUNK_MAX=%u",
        (unsigned long)snapshot_id,
        (unsigned int)R4_OLED_PROFILE_WIDTH,
        (unsigned int)R4_OLED_PROFILE_HEIGHT,
        (unsigned int)R4_FRAMEBUFFER_PACKED_SIZE,
        (unsigned long)hash,
        (unsigned int)R4_FRAMEBUFFER_CHUNK_MAX
    );

    return
        written >= 0 &&
        (size_t)written < capacity &&
        (size_t)written <= R4_FRAMEBUFFER_RESPONSE_MAX;
}

bool r4_framebuffer_format_chunk(
    char *buffer,
    size_t capacity,
    uint32_t snapshot_id,
    size_t offset,
    const uint8_t *packed,
    size_t packed_size,
    size_t length
) {
    if (
        buffer == NULL ||
        capacity == 0 ||
        snapshot_id == 0 ||
        packed == NULL ||
        length == 0 ||
        length > R4_FRAMEBUFFER_CHUNK_MAX ||
        offset > packed_size ||
        length > packed_size - offset
    ) {
        return false;
    }

    int written = snprintf(
        buffer,
        capacity,
        "FRAMEBUFFER DATA ID=%lu OFFSET=%lu LENGTH=%lu HEX=",
        (unsigned long)snapshot_id,
        (unsigned long)offset,
        (unsigned long)length
    );

    if (written < 0 || (size_t)written >= capacity) {
        return false;
    }

    static const char hex[] = "0123456789ABCDEF";
    size_t cursor = (size_t)written;

    for (size_t index = 0; index < length; ++index) {
        if (cursor + 2U >= capacity) {
            return false;
        }

        const uint8_t value = packed[offset + index];
        buffer[cursor++] = hex[value >> 4U];
        buffer[cursor++] = hex[value & UINT8_C(0x0F)];
    }

    buffer[cursor] = '\0';
    return cursor <= R4_FRAMEBUFFER_RESPONSE_MAX;
}
