cmake_minimum_required(VERSION 3.14)
project(theft LANGUAGES C)

# theft is a git submodule at tests/theft; this wrapper builds it without modifying it.
set(THEFT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../theft")

file(GLOB THEFT_SOURCES "${THEFT_ROOT}/src/*.c")

add_library(theft STATIC ${THEFT_SOURCES})

target_include_directories(theft
    PUBLIC
        "${THEFT_ROOT}/inc"
    PRIVATE
        "${THEFT_ROOT}/src"
)

target_compile_options(theft PRIVATE
    -std=c99
    -w
)

target_compile_definitions(theft PRIVATE
    _POSIX_C_SOURCE=199309L
)
