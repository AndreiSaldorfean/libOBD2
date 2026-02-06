cmake_minimum_required(VERSION 3.14)
project(libOBD2 LANGUAGES C)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# TinyUSB sources
set(TINYUSB_ROOT ${CMAKE_SOURCE_DIR}/../libs/tinyusb)

file(GLOB SOURCES
    "${CMAKE_SOURCE_DIR}/src/startup/isr_vector.c"
    "${CMAKE_SOURCE_DIR}/src/startup/reset_handler.c"
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/src/os/*.c"
    "${CMAKE_SOURCE_DIR}/../libs/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c"
    "${CMAKE_SOURCE_DIR}/../libs/cmsis_device_f4/Source/Templates/system_stm32f4xx.c"
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
    CFG_TUSB_OS=OPT_OS_FREERTOS
    STM32F401xC
)
# include_directories(SYSTEM
#     /home/rudy/tools/arm-gnu-toolchain/lib/gcc/arm-none-eabi/14.2.1/
#     /home/rudy/tools/arm-gnu-toolchain/lib/gcc/
#     /home/rudy/tools/arm-gnu-toolchain/arm-none-eabi/lib/)

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
    ${CMAKE_CURRENT_LIST_DIR}/../libs/FreeRTOS-Kernel
    ${CMAKE_BINARY_DIR}/FreeRTOS-Kernel
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/libopencm3/lib/libopencm3_stm32f4.a
    freertos_config
    freertos_kernel
    ${CMAKE_SOURCE_DIR}/../../build/libOBD2.a
    )

# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND utils/add_path_to_comp_com
# )

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

# add_custom_target(
#     cc
#     COMMAND ${CMAKE_COMMAND} -E echo "Running my script..."
#     COMMAND /bin/zsh ${CMAKE_SOURCE_DIR}/utils/add_path_to_comp_com
#     DEPENDS ${PROJECT_NAME}
# )
