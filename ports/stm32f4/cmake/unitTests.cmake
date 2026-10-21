set(TEST_EXECUTABLE_SUFFIX ".elf")
set(TEST_USE_LEGACY_SOURCES ON)
set(TEST_USE_ECU ON)
set(TEST_ADD_BIN_POST_BUILD ON)

# STM32F4 test target executes RTOS-based test flows and needs FreeRTOS symbols.
set(SPT_FREERTOS "ON" CACHE STRING "Support for FreeRTOS" FORCE)

set(TINYUSB_ROOT ${CMAKE_SOURCE_DIR}/../examples/libs/tinyusb)

file(GLOB TEST_PLATFORM_STARTUP_SOURCES
    "${CMAKE_SOURCE_DIR}/../ports/stm32f4/startup/*.c"
)

list(APPEND TEST_PLATFORM_SOURCES
    "${CMAKE_SOURCE_DIR}/../examples/libs/cmsis_device_f4/Source/Templates/system_stm32f4xx.c"
    "${CMAKE_SOURCE_DIR}/src/ports/stm32f4/os/tasks.c"
    ${TEST_PLATFORM_STARTUP_SOURCES}
    ${TINYUSB_ROOT}/src/tusb.c
    ${TINYUSB_ROOT}/src/common/tusb_fifo.c
    ${TINYUSB_ROOT}/src/device/usbd.c
    ${TINYUSB_ROOT}/src/device/usbd_control.c
    ${TINYUSB_ROOT}/src/class/cdc/cdc_device.c
    ${TINYUSB_ROOT}/src/portable/synopsys/dwc2/dcd_dwc2.c
)

list(APPEND TEST_PLATFORM_INCLUDES
    ./src/ports/os/
    ../examples/libs/libopencm3/include/
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
    ../ports/stm32f4/startup/
    ../ports/stm32f4/config/
)

list(APPEND TEST_PLATFORM_DEFINES
    CFG_TUSB_MCU=OPT_MCU_STM32F4
    CFG_TUSB_OS=OPT_OS_NONE
    STM32F401xC
    UNITY_EXCLUDE_SETJMP_H
)

get_filename_component(OPENCM3_LIB
    "${CMAKE_CURRENT_LIST_DIR}/../../../examples/libs/libopencm3/lib/libopencm3_stm32f4.a"
    ABSOLUTE
)

list(APPEND TEST_PLATFORM_LINK_LIBS ${OPENCM3_LIB})
