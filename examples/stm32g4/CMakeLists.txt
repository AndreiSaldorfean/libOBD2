cmake_minimum_required(VERSION 3.14)
project(demo LANGUAGES C ASM)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")
option(SPT_FREERTOS "Support for FreeRTOS" ON)

get_filename_component(TINYUSB_ROOT "${CMAKE_CURRENT_LIST_DIR}/../libs/tinyusb" ABSOLUTE)

file(GLOB SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/startup/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/os/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../ports/stm32g4/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../ports/stm32g4/startup/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../ports/stm32g4/startup/startup.S"
    "${CMAKE_CURRENT_LIST_DIR}/../../ports/stm32g4/RTT/Syscalls/SEGGER_RTT_Syscalls_GCC.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../ports/stm32g4/RTT/RTT/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../ports/stm32g4/RTT/RTT/*.S"
    ${TINYUSB_ROOT}/src/tusb.c
    ${TINYUSB_ROOT}/src/common/tusb_fifo.c
    ${TINYUSB_ROOT}/src/device/usbd.c
    ${TINYUSB_ROOT}/src/device/usbd_control.c
    ${TINYUSB_ROOT}/src/class/cdc/cdc_device.c
    ${TINYUSB_ROOT}/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
    "${CMAKE_CURRENT_LIST_DIR}/../libs/cmsis_device_g4/Source/Templates/system_stm32g4xx.c"
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wno-error=overflow
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ../../include/data_link/
    ../../include/data_link/l2_kwp/
    ../../include/freertos/
    ../../include/app
    ../../include/data_link/l2_iso9141/
    ../../interface/
    ../../utils/
    ../../include/app/
    ../../ports/stm32g4/
    ../../ports/stm32g4/startup/
    ../../ports/stm32g4/config/
    ../../ports/stm32g4/RTT/RTT/
    ../../ports/stm32g4/RTT/Config/
    ../../ports/stm32g4/RTT/Syscalls/
    ./inc/
    ./src/os/
    ../libs/libopencm3/include/
    ../libs/FreeRTOS-Kernel/include/
    ../libs/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/
    ../../include
    ../../ports/stm32g4/config/
    ${TINYUSB_ROOT}/src
    ${TINYUSB_ROOT}/src/common
    ${TINYUSB_ROOT}/src/device
    ${TINYUSB_ROOT}/src/class/cdc
    ${TINYUSB_ROOT}/src/portable/st/stm32_fsdev
    ${TINYUSB_ROOT}/hw
    ../libs/cmsis_device_g4/Include
    ../libs/CMSIS_5/CMSIS/Core/Include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    STM32G4
    STM32G474xx
    UNITY_EXCLUDE_SETJMP_H
)

if(SPT_FREERTOS STREQUAL "ON")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SPT_FREERTOS)
endif()

if(LOGGING STREQUAL "ON")
    target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGING)
endif()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../ecu ${CMAKE_BINARY_DIR}/ecu)

get_filename_component(OPENCM3_LIB
    "${CMAKE_CURRENT_LIST_DIR}/../libs/libopencm3/lib/libopencm3_stm32g4.a"
    ABSOLUTE
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${OPENCM3_LIB}
    OBD2
    OBD2_ECU
)

target_link_options(${PROJECT_NAME} PRIVATE
  -Wl,--print-memory-usage
)

set(CMAKE_SIZE arm-none-eabi-size)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${PROJECT_NAME}>
  COMMENT "Firmware size (text/data/bss/dec/hex):"
  VERBATIM
)

# Generate .bin file from .elf
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMENT "Generating ${PROJECT_NAME}.bin"
)
