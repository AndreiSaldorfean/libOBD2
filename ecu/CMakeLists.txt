cmake_minimum_required(VERSION 3.14)
project(OBD2_ECU LANGUAGES C)

# TinyUSB sources
get_filename_component(TINYUSB_ROOT "${CMAKE_CURRENT_LIST_DIR}/../examples/libs/tinyusb" ABSOLUTE)

# --- libOBD2 (base library) ---
# libOBD2's CMakeLists owns FreeRTOS setup; add it first so its targets
# (freertos_config, freertos_kernel, OBD2) are available to us.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. ${CMAKE_BINARY_DIR}/libOBD2)

# --- ECU simulator sources ---
file(GLOB ECU_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../ports/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/../ports/config/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/../ports/startup/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/modules/app/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/modules/datalink/common/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/modules/datalink/specific/l2_iso9141/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/modules/datalink/specific/l2_kwp2000/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/../examples/libs/cmsis_device_f4/Source/Templates/system_stm32f4xx.c"
    "${CMAKE_CURRENT_LIST_DIR}/ecu_ports/stm32f4/*.c"
    ${TINYUSB_ROOT}/src/tusb.c
    ${TINYUSB_ROOT}/src/common/tusb_fifo.c
    ${TINYUSB_ROOT}/src/device/usbd.c
    ${TINYUSB_ROOT}/src/device/usbd_control.c
    ${TINYUSB_ROOT}/src/class/cdc/cdc_device.c
    ${TINYUSB_ROOT}/src/portable/synopsys/dwc2/dcd_dwc2.c
)

add_library(${PROJECT_NAME} ${ECU_SOURCES})

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    SPT_FREERTOS
)

if(DEBUG STREQUAL "ON")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ../examples/libs/libopencm3/include/
    ../examples/libs/CMSIS_5/CMSIS/Core/Include
    ../examples/libs/cmsis_device_f4/Include
    ../ports/stm32f4/
    ../ports/stm32f4/config/
    ../ports/stm32f4/startup/
    ${CMAKE_CURRENT_LIST_DIR}/modules/app/
    ${CMAKE_CURRENT_LIST_DIR}/modules/datalink/common/
    ${CMAKE_CURRENT_LIST_DIR}/modules/datalink/specific/l2_iso9141/
    ${CMAKE_CURRENT_LIST_DIR}/modules/datalink/specific/l2_kwp2000/
    ${CMAKE_CURRENT_LIST_DIR}/ecu_ports/stm32f4/
    ${TINYUSB_ROOT}/src
    ${TINYUSB_ROOT}/src/common
    ${TINYUSB_ROOT}/src/device
    ${TINYUSB_ROOT}/src/class/cdc
    ${TINYUSB_ROOT}/src/portable/synopsys/dwc2
    ${TINYUSB_ROOT}/hw
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -save-temps=obj
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CFG_TUSB_MCU=OPT_MCU_STM32F4
    CFG_TUSB_OS=OPT_OS_NONE
    STM32F401xC
)

get_filename_component(OPENCM3_LIB
    "${CMAKE_CURRENT_LIST_DIR}/../examples/libs/libopencm3/lib/libopencm3_stm32f4.a"
    ABSOLUTE
)

# Link against libOBD2 — this transitively brings in freertos_kernel and
# freertos_config, so no need to list them separately.
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    OBD2
    PRIVATE
    ${OPENCM3_LIB}
)
