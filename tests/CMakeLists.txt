cmake_minimum_required(VERSION 3.14)
project(libOBD2 LANGUAGES C)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

file(GLOB SOURCES
    "${CMAKE_SOURCE_DIR}/startup/isr_vector.c"
    "${CMAKE_SOURCE_DIR}/startup/reset_handler.c"
    "${CMAKE_SOURCE_DIR}/*.c"
    "${CMAKE_SOURCE_DIR}/os/*.c"
    "${CMAKE_SOURCE_DIR}/Unity/src/*.c"
    "${CMAKE_SOURCE_DIR}/kline/*.c"
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ./inc/
    ../examples/libs/libopencm3/include/
    ../include
    ./kline
    ./Unity/src/
)
# include_directories(SYSTEM
#     /home/rudy/tools/arm-gnu-toolchain/lib/gcc/arm-none-eabi/14.2.1/
#     /home/rudy/tools/arm-gnu-toolchain/lib/gcc/
#     /home/rudy/tools/arm-gnu-toolchain/arm-none-eabi/lib/)


target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/../examples/libs/libopencm3/lib/libopencm3_stm32f4.a
    ${CMAKE_SOURCE_DIR}/../build/libOBD2.a
    )
