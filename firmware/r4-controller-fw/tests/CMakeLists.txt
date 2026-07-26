cmake_minimum_required(VERSION 3.16)

project(r4-controller-fw-host-tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(
    r4-controller-core
    ../core/r4_controller.c
    ../core/r4_display.c
    ../core/r4_framebuffer_transport.c
    ../core/r4_input_source.c
    ../core/r4_protocol.c
    ../core/r4_service_buttons.c
)

target_include_directories(
    r4-controller-core
    PUBLIC
    ../core
)

if(MSVC)
    target_compile_options(r4-controller-core PRIVATE /W4 /WX)
else()
    target_compile_options(
        r4-controller-core
        PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wpedantic
    )
endif()

add_executable(
    r4-controller-core-tests
    test_main.c
)

target_link_libraries(
    r4-controller-core-tests
    PRIVATE
    r4-controller-core
)

add_executable(
    r4-oled-emulator
    ../oled-emulator/main.c
)

target_link_libraries(
    r4-oled-emulator
    PRIVATE
    r4-controller-core
)

enable_testing()

add_test(
    NAME r4-controller-core-tests
    COMMAND r4-controller-core-tests
)

add_test(
    NAME r4-oled-snapshots
    COMMAND
        r4-oled-emulator
        --output-dir
        ${CMAKE_CURRENT_BINARY_DIR}/oled-snapshots
        --scenario
        all
)

add_custom_target(
    r4-oled-gui
    SOURCES
        ../oled-emulator/r4_oled_gui.py
        ../oled-emulator/r4_oled_protocol.py
        ../oled-emulator/r4_tcp_protocol.py
        ../oled-emulator/requirements.txt
        ../oled-emulator/requirements-build.txt
)

find_package(Python3 COMPONENTS Interpreter QUIET)

if(Python3_Interpreter_FOUND)
    add_custom_target(
        r4-oled-gui-package
        COMMAND
            ${Python3_EXECUTABLE}
            -m
            PyInstaller
            --noconfirm
            --clean
            --onefile
            --windowed
            --name
            r4-oled-emulator-gui
            --distpath
            ${CMAKE_CURRENT_BINARY_DIR}/oled-gui-dist
            --workpath
            ${CMAKE_CURRENT_BINARY_DIR}/oled-gui-build
            --specpath
            ${CMAKE_CURRENT_BINARY_DIR}/oled-gui-spec
            ${CMAKE_CURRENT_LIST_DIR}/../oled-emulator/r4_oled_gui.py
        WORKING_DIRECTORY
            ${CMAKE_CURRENT_LIST_DIR}/../oled-emulator
        COMMENT
            "Packaging the Windows OLED GUI"
        VERBATIM
    )

    add_test(
        NAME r4-oled-gui-protocol
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_LIST_DIR}/test_oled_protocol.py
    )
    add_test(
        NAME r4-oled-tcp-protocol
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_LIST_DIR}/test_tcp_protocol.py
    )

    execute_process(
        COMMAND
            ${Python3_EXECUTABLE}
            -c
            "import tkinter"
        RESULT_VARIABLE
            R4_TKINTER_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(R4_TKINTER_RESULT EQUAL 0)
        add_test(
            NAME r4-oled-gui-tk
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_LIST_DIR}/test_oled_gui.py
        )
        add_test(
            NAME r4-oled-tcp-worker
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_LIST_DIR}/test_oled_tcp_worker.py
        )
    else()
        message(
            WARNING
            "Tkinter not found; GUI rendering and TCP worker tests are disabled"
        )
    endif()
else()
    message(
        WARNING
        "Python 3 not found; r4-oled-gui-protocol test is disabled"
    )
endif()
