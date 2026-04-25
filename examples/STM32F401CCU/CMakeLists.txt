cmake_minimum_required(VERSION 3.14)
project(demo LANGUAGES C)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# TinyUSB sources
set(TINYUSB_ROOT ${CMAKE_SOURCE_DIR}/../libs/tinyusb)

file(GLOB SOURCES
    "${CMAKE_SOURCE_DIR}/src/startup/*.c"
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/src/os/*.c"
    "${CMAKE_SOURCE_DIR}/../../ports/stm32f4/*.c"
    "${CMAKE_SOURCE_DIR}/../../ports/stm32f4/startup/*.c"
    "${CMAKE_SOURCE_DIR}/../../utils/*.c"
    "${CMAKE_SOURCE_DIR}/../libs/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c"
    "${CMAKE_SOURCE_DIR}/../libs/cmsis_device_f4/Source/Templates/system_stm32f4xx.c"
    ${TINYUSB_ROOT}/src/*.c
    ${TINYUSB_ROOT}/src/common/*.c
    ${TINYUSB_ROOT}/src/device/*.c
    ${TINYUSB_ROOT}/src/class/cdc/*.c
    ${TINYUSB_ROOT}/src/portable/synopsys/dwc2/dcd_dwc2.c
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
    ../../ports/stm32f4/
    ../../ports/stm32f4/startup/
    ../../ports/stm32f4/config/
    ./inc/
    ./src/os/
    ../libs/libopencm3/include/
    ../libs/FreeRTOS-Kernel/include/
    ../libs/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/
    ../libs/cmsis_device_f4/Include
    ../libs/CMSIS_5/CMSIS/Core/Include
    ${TINYUSB_ROOT}/src
    ${TINYUSB_ROOT}/src/common
    ${TINYUSB_ROOT}/src/device
    ${TINYUSB_ROOT}/src/class/cdc
    ${TINYUSB_ROOT}/src/portable/synopsys/dwc2
    ${TINYUSB_ROOT}/hw
    ../../include
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

add_subdirectory(${CMAKE_SOURCE_DIR}/../.. ${CMAKE_BINARY_DIR}/libOBD2)

target_compile_definitions(libOBD2
    PRIVATE
    UNIT_TESTS
    SPT_FREERTOS
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/../libs/libopencm3/lib/libopencm3_stm32f4.a
    libOBD2
)

add_custom_target(
    renode
    COMMAND renode ${CMAKE_SOURCE_DIR}/utils/stm_dev.resc --console
    DEPENDS ${PROJECT_NAME}
)

target_link_options(${PROJECT_NAME} PRIVATE
  -Wl,--print-memory-usage
)

add_custom_target(
    renode_make
    COMMAND renode ${CMAKE_SOURCE_DIR}/utils/stm_dev_make.resc --console
    DEPENDS ${PROJECT_NAME}
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
