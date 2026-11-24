cmake_minimum_required(VERSION 3.15)

set(PICO_BOARD pico)
set(PICO_PLATFORM rp2040)
include(pico_sdk_import.cmake)

project(411_i2c_spi)
pico_sdk_init()

add_compile_options(-Wall)
add_subdirectory(../libs ${CMAKE_BINARY_DIR}/libs)

add_library(platform STATIC platform/platform.c)
target_include_directories(platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(platform PRIVATE
    pico_stdlib
    hardware_i2c
    hardware_spi
    pico_multicore
)

add_library(logic STATIC
    logic/api/api.c
    logic/api/api-callbacks.c
    logic/slave/slave.c
)
target_include_directories(logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/logic)
target_link_libraries(logic
    PUBLIC stdio_text_protocol
    PRIVATE platform
)

add_executable(${PROJECT_NAME} main.c)

target_link_libraries(${PROJECT_NAME} logic platform pico_stdlib)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_add_extra_outputs(${PROJECT_NAME})
