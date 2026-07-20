#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tusb.h"

#define USB_VID 0xCAFE

// Changed from 0x4004 so Windows enumerates the new composite layout
// as a new device instead of reusing the old HID-only configuration.
#define USB_PID 0x4005

#define DEVICE_BCD_VERSION 0x0200

// -----------------------------------------------------------------------------
// Device descriptor
// -----------------------------------------------------------------------------

static tusb_desc_device_t const device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,

    // Composite USB device using Interface Association Descriptors.
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = DEVICE_BCD_VERSION,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&device_descriptor;
}

// -----------------------------------------------------------------------------
// HID report descriptor
// -----------------------------------------------------------------------------

static uint8_t const hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_GAMEPAD()
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return hid_report_descriptor;
}

// -----------------------------------------------------------------------------
// Configuration descriptor
// -----------------------------------------------------------------------------

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

enum {
    STRING_ID_LANGUAGE = 0,
    STRING_ID_MANUFACTURER,
    STRING_ID_PRODUCT,
    STRING_ID_SERIAL,
    STRING_ID_CDC,
    STRING_ID_HID
};

#define CDC_NOTIFICATION_ENDPOINT 0x81
#define CDC_OUT_ENDPOINT          0x02
#define CDC_IN_ENDPOINT           0x82
#define HID_IN_ENDPOINT           0x83

#define CONFIG_TOTAL_LENGTH \
    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t const configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(
        1,
        ITF_NUM_TOTAL,
        0,
        CONFIG_TOTAL_LENGTH,
        0x00,
        100
    ),

    // CDC ACM service channel.
    TUD_CDC_DESCRIPTOR(
        ITF_NUM_CDC,
        STRING_ID_CDC,
        CDC_NOTIFICATION_ENDPOINT,
        16,
        CDC_OUT_ENDPOINT,
        CDC_IN_ENDPOINT,
        64
    ),

    // HID gamepad.
    TUD_HID_DESCRIPTOR(
        ITF_NUM_HID,
        STRING_ID_HID,
        HID_ITF_PROTOCOL_NONE,
        sizeof(hid_report_descriptor),
        HID_IN_ENDPOINT,
        CFG_TUD_HID_EP_BUFSIZE,
        5
    )
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return configuration_descriptor;
}

// -----------------------------------------------------------------------------
// String descriptors
// -----------------------------------------------------------------------------

static char const *string_descriptors[] = {
    (const char[]){0x09, 0x04},
    "Rarmash",
    "R4 Controller",
    "R4-0001",
    "R4 Service",
    "R4 Gamepad"
};

static uint16_t string_descriptor_buffer[32];

uint16_t const *tud_descriptor_string_cb(
    uint8_t index,
    uint16_t langid
) {
    (void)langid;

    size_t character_count;

    if (index == STRING_ID_LANGUAGE) {
        memcpy(
            &string_descriptor_buffer[1],
            string_descriptors[STRING_ID_LANGUAGE],
            2
        );

        character_count = 1;
    } else {
        const size_t descriptor_count =
            sizeof(string_descriptors) /
            sizeof(string_descriptors[0]);

        if (index >= descriptor_count) {
            return NULL;
        }

        const char *string = string_descriptors[index];
        character_count = strlen(string);

        const size_t maximum_characters =
            sizeof(string_descriptor_buffer) /
            sizeof(string_descriptor_buffer[0]) - 1;

        if (character_count > maximum_characters) {
            character_count = maximum_characters;
        }

        for (size_t i = 0; i < character_count; ++i) {
            string_descriptor_buffer[1 + i] =
                (uint8_t)string[i];
        }
    }

    string_descriptor_buffer[0] =
        (uint16_t)(
            (TUSB_DESC_STRING << 8) |
            (2 * character_count + 2)
        );

    return string_descriptor_buffer;
}