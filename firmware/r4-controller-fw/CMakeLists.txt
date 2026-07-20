cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(pico_sdk_import.cmake)

project(r4-controller-fw C CXX ASM)

pico_sdk_init()

add_executable(r4-controller-fw
    r4-controller-fw.c
    usb_descriptors.c
)

pico_set_program_name(r4-controller-fw "r4-controller-fw")
pico_set_program_version(r4-controller-fw "0.1.0")

# USB полностью принадлежит TinyUSB HID.
pico_enable_stdio_uart(r4-controller-fw 0)
pico_enable_stdio_usb(r4-controller-fw 0)

# Здесь TinyUSB найдёт tusb_config.h.
target_include_directories(r4-controller-fw PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(r4-controller-fw
    pico_stdlib
    hardware_adc
    tinyusb_device
)

pico_add_extra_outputs(r4-controller-fw)