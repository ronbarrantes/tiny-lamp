cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Pico board")
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(tiny_lamp C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(tiny_lamp
    src/main.c
)

pico_generate_pio_header(tiny_lamp ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

target_link_libraries(tiny_lamp
    pico_stdlib
    hardware_pio
)

target_include_directories(tiny_lamp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

pico_enable_stdio_usb(tiny_lamp 1)
pico_enable_stdio_uart(tiny_lamp 0)
pico_add_extra_outputs(tiny_lamp)
