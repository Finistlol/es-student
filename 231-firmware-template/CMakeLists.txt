cmake_minimum_required(VERSION 3.15)

set(PICO_BOARD pico)
set(PICO_PLATFORM rp2040)

include(pico_sdk_import.cmake)

project(231_firmware_template)

pico_sdk_init()

add_compile_options(-Wall)

# Общие библиотеки курса лежат в корне репозитория.
add_subdirectory(../libs ${CMAKE_BINARY_DIR}/libs)

# Тип платы передаётся библиотеке device из настроек сборки.
target_compile_definitions(device PRIVATE DEVICE_BOARD="${PICO_BOARD}")

add_library(platform platform/platform.c)
target_include_directories(platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(platform PRIVATE pico_stdio_headers hardware_gpio)

# Логика прибора: команды и задачи. SDK не подключает.
add_library(logic
    logic/api/api.c
    logic/api/api-callbacks.c
    logic/led-task/led-task.c
    logic/button-task/button-task.c
    logic/pi-task/pi-task.c
)
target_include_directories(logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/logic)
target_link_libraries(logic PUBLIC stdio_text_protocol)
target_link_libraries(logic PRIVATE platform systime profiling device logging)

add_executable(${PROJECT_NAME} main.c)
target_link_libraries(${PROJECT_NAME} pico_stdlib platform logic logging profiling stdio_text_protocol systime)

# Профилирование включено; выключается нулём в этой строке (Ф6.3).
target_compile_definitions(profiling PUBLIC PROFILING_ENABLED=1)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_set_linker_script(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/memmap_rp2040.ld
)

pico_add_extra_outputs(${PROJECT_NAME})