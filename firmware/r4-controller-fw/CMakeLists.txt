cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(pico_sdk_import.cmake)

project(r4-controller-fw C CXX ASM)

set(R4_FIRMWARE_VERSION "0.12.0")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

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

# The Pico Windows bundle already ships matching host tools. Import them when
# their explicit paths are supplied so CMake does not require a native C++
# toolchain merely to rebuild pioasm/picotool.
if(DEFINED ENV{PICO_PIOASM_PATH} AND NOT TARGET pioasm)
    add_executable(pioasm IMPORTED GLOBAL)
    set_property(
        TARGET pioasm
        PROPERTY IMPORTED_LOCATION "$ENV{PICO_PIOASM_PATH}"
    )
endif()

if(DEFINED ENV{PICO_PICOTOOL_PATH} AND NOT TARGET picotool)
    add_executable(picotool IMPORTED GLOBAL)
    set_property(
        TARGET picotool
        PROPERTY IMPORTED_LOCATION "$ENV{PICO_PICOTOOL_PATH}"
    )
endif()

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
    core/r4_update.c
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
    pico_bootrom
    pico_rand
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

add_custom_target(
    r4-controller-fw-release
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_LIST_DIR}/tools/package_release.py
        --uf2
        ${CMAKE_CURRENT_BINARY_DIR}/r4-controller-fw.uf2
        --version
        ${R4_FIRMWARE_VERSION}
        --output-dir
        ${CMAKE_CURRENT_BINARY_DIR}/release
    DEPENDS r4-controller-fw
    COMMENT "Packaging versioned UF2, manifest and SHA-256"
    VERBATIM
)
