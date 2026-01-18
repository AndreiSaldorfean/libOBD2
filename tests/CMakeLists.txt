cmake_minimum_required(VERSION 3.14)
project(libOBD2 LANGUAGES C)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# TinyUSB sources
set(TINYUSB_ROOT ${CMAKE_SOURCE_DIR}/../examples/libs/tinyusb)

file(GLOB SOURCES
    "${CMAKE_SOURCE_DIR}/src/startup/isr_vector.c"
    "${CMAKE_SOURCE_DIR}/src/startup/reset_handler.c"
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/src/os/*.c"
    "${CMAKE_SOURCE_DIR}/src/kline/*.c"
    "${CMAKE_SOURCE_DIR}/Unity/src/*.c"
    "${CMAKE_SOURCE_DIR}/../libs/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c"
    "${CMAKE_SOURCE_DIR}/../examples/libs/cmsis_device_f4/Source/Templates/system_stm32f4xx.c"
    ${TINYUSB_ROOT}/src/*.c
    ${TINYUSB_ROOT}/src/common/*.c
    ${TINYUSB_ROOT}/src/device/*.c
    ${TINYUSB_ROOT}/src/class/cdc/*.c
    ${TINYUSB_ROOT}/src/portable/synopsys/dwc2/dcd_dwc2.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ./inc/
    ../examples/libs/libopencm3/include/
    ../include
    ./src/kline
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
# include_directories(SYSTEM
#     /home/rudy/tools/arm-gnu-toolchain/lib/gcc/arm-none-eabi/14.2.1/
#     /home/rudy/tools/arm-gnu-toolchain/lib/gcc/
#     /home/rudy/tools/arm-gnu-toolchain/arm-none-eabi/lib/)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    CFG_TUSB_MCU=OPT_MCU_STM32F4
    CFG_TUSB_OS=OPT_OS_FREERTOS
    STM32F401xC
    UNITY_EXCLUDE_SETJMP_H
)

set( FREERTOS_PORT "GCC_ARM_CM4F" CACHE STRING "" FORCE)
set( FREERTOS_HEAP "4" CACHE STRING "" FORCE)

add_library(freertos_config INTERFACE)

target_include_directories(freertos_config SYSTEM
INTERFACE
    ${CMAKE_SOURCE_DIR}/inc
)

target_compile_definitions(freertos_config
  INTERFACE
    projCOVERAGE_TEST=0
)

add_subdirectory(
    ${CMAKE_CURRENT_LIST_DIR}/../examples/libs/FreeRTOS-Kernel
    ${CMAKE_BINARY_DIR}/FreeRTOS-Kernel
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/../examples/libs/libopencm3/lib/libopencm3_stm32f4.a
    freertos_config
    freertos_kernel
    ${CMAKE_SOURCE_DIR}/../build/libOBD2.a
    )

# Generate .bin file from .elf
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMENT "Generating ${PROJECT_NAME}.bin"
)
