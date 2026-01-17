cmake_minimum_required(VERSION 3.14)
project(libOBD2 LANGUAGES C)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")
file(GLOB SOURCES "src/*.c" "src/os/*.c" "startup/*.c" "${CMAKE_SOURCE_DIR}./libs/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c")
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME}
    PRIVATE
    ./inc/
    ./libs/libopencm3/include/
    ./libs/FreeRTOS-Kernel/include/
    ./libs/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/
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

add_subdirectory(libs/FreeRTOS-Kernel)
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/libopencm3/lib/libopencm3_stm32f4.a
    freertos_config
    freertos_kernel
    )

# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND utils/add_path_to_comp_com
# )

add_custom_target(
    renode
    COMMAND renode ${CMAKE_SOURCE_DIR}/utils/stm_dev.resc --console
    DEPENDS ${PROJECT_NAME}
)

add_custom_target(
    renode_make
    COMMAND renode ${CMAKE_SOURCE_DIR}/utils/stm_dev_make.resc --console
    DEPENDS ${PROJECT_NAME}
)

# add_custom_target(
#     cc
#     COMMAND ${CMAKE_COMMAND} -E echo "Running my script..."
#     COMMAND /bin/zsh ${CMAKE_SOURCE_DIR}/utils/add_path_to_comp_com
#     DEPENDS ${PROJECT_NAME}
# )
