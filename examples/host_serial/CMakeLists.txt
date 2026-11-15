cmake_minimum_required(VERSION 3.14)
project(obd2_reader LANGUAGES C)

# Plain host process: no RTOS, no tracing
set(SPT_FREERTOS "OFF" CACHE STRING "Support for FreeRTOS" FORCE)
set(SPT_TRACING  "OFF" CACHE STRING "Support for tracing" FORCE)

get_filename_component(LIBOBD2_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(PORT_DIR "${LIBOBD2_ROOT}/ports/host_serial")

add_subdirectory(${LIBOBD2_ROOT} ${CMAKE_BINARY_DIR}/libOBD2)
include(${PORT_DIR}/cmake/libserialport.cmake)

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/src/main.c
    ${PORT_DIR}/libobd2_uart_port.c
    ${PORT_DIR}/libobd2_timer_port.c
)

target_include_directories(${PROJECT_NAME} PRIVATE ${PORT_DIR})

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)

target_link_libraries(${PROJECT_NAME} PRIVATE
    OBD2
    serialport
)
