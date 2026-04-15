cmake_minimum_required(VERSION 3.14)
project(test LANGUAGES C)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# TinyUSB sources
set(TINYUSB_ROOT ${CMAKE_SOURCE_DIR}/../examples/libs/tinyusb)

file(GLOB SOURCES
    "${CMAKE_SOURCE_DIR}/Unity/src/unity.c"
    "${CMAKE_SOURCE_DIR}/src/main.c"
    "${CMAKE_SOURCE_DIR}/src/unit/*.c"
    "${CMAKE_SOURCE_DIR}/src/unit/ecu_ports/stm32f4/*.c"
    "${CMAKE_SOURCE_DIR}/src/os/*.c"
    "${CMAKE_SOURCE_DIR}/src/usb_descriptors.c"
    "${CMAKE_SOURCE_DIR}/../examples/libs/cmsis_device_f4/Source/Templates/system_stm32f4xx.c"
    "${CMAKE_SOURCE_DIR}/../ports/stm32f4/*.c"
    "${CMAKE_SOURCE_DIR}/../ports/stm32f4/startup/*.c"
    ${TINYUSB_ROOT}/src/tusb.c
    ${TINYUSB_ROOT}/src/common/tusb_fifo.c
    ${TINYUSB_ROOT}/src/device/usbd.c
    ${TINYUSB_ROOT}/src/device/usbd_control.c
    ${TINYUSB_ROOT}/src/class/cdc/cdc_device.c
    ${TINYUSB_ROOT}/src/portable/synopsys/dwc2/dcd_dwc2.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wno-error=overflow
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ./inc/
    ../examples/libs/libopencm3/include/
    ../include
    ../include/freertos/
    ../ports/stm32f4/
    ../ports/stm32f4/startup/
    ../ports/stm32f4/config/
    ../utils
    ../interface/
    ../include/app/
    ../include/data_link/
    ../include/data_link/l2_kwp/
    ../include/data_link/l2_iso9141/
    ./src/int/
    ./src/unit/
    ./src/unit/ecu_ports/stm32f4/
    ./src/os/
    ./Unity/src/
    ../examples/libs/FreeRTOS-Kernel/include/
    ../examples/libs/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/
    ../examples/libs/cmsis_device_f4/Include
    ../examples/libs/CMSIS_5/CMSIS/Core/Include
    ${TINYUSB_ROOT}/src
    ${TINYUSB_ROOT}/src/common
    ${TINYUSB_ROOT}/src/device
    ${TINYUSB_ROOT}/src/class/cdc
    ${TINYUSB_ROOT}/src/portable/synopsys/dwc2
    ${TINYUSB_ROOT}/hw
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CFG_TUSB_MCU=OPT_MCU_STM32F4
    CFG_TUSB_OS=OPT_OS_NONE
    STM32F401xC
    UNITY_EXCLUDE_SETJMP_H
    SPT_FREERTOS
)

if(DEBUG STREQUAL "ON")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/.. ${CMAKE_BINARY_DIR}/libOBD2)

target_compile_definitions(libOBD2
    PRIVATE
    UNIT_TESTS
    SPT_FREERTOS
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/../examples/libs/libopencm3/lib/libopencm3_stm32f4.a
    libOBD2
)

# Generate .bin file from .elf
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMENT "Generating ${PROJECT_NAME}.bin"
)

