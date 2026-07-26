cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(pico_sdk_import.cmake)

project(r4-controller-fw C CXX ASM)

set(R4_FIRMWARE_VERSION "0.8.0")

option(
    R4_ENABLE_TEST_INPUT
    "Enable CDC commands that inject test-only controller input"
    OFF
)

configure_file(
    version.h.in
    version.h
    @ONLY
)

pico_sdk_init()

add_executable(r4-controller-fw
    r4-controller-fw.c
    rp2040_input.c
    usb_descriptors.c
    rgb_led.c
    core/r4_controller.c
    core/r4_display.c
    core/r4_framebuffer_transport.c
    core/r4_input_source.c
    core/r4_protocol.c
    core/r4_service_buttons.c
)

pico_generate_pio_header(
    r4-controller-fw
    ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
)

pico_set_program_name(
    r4-controller-fw
    "r4-controller-fw"
)

pico_set_program_version(
    r4-controller-fw
    "${R4_FIRMWARE_VERSION}"
)

pico_enable_stdio_uart(r4-controller-fw 0)
pico_enable_stdio_usb(r4-controller-fw 0)

target_include_directories(
    r4-controller-fw
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(
    r4-controller-fw
    pico_stdlib
    hardware_adc
    hardware_pio
    hardware_clocks
    tinyusb_device
)

target_compile_definitions(
    r4-controller-fw
    PRIVATE
    R4_ENABLE_TEST_INPUT=$<BOOL:${R4_ENABLE_TEST_INPUT}>
)

pico_add_extra_outputs(r4-controller-fw)
