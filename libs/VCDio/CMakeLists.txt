cmake_minimum_required(VERSION 3.14)
project(VCDio VERSION 0.1.0 LANGUAGES C)

# Tests, tools and examples default to ON only when VCDio is the top-level project,
# so add_subdirectory(libs/VCDio) from another project just builds the library.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(VCDIO_TOP_LEVEL ON)
else()
    set(VCDIO_TOP_LEVEL OFF)
endif()

option(VCDIO_BUILD_TESTS    "Build the VCDio unit tests"          ${VCDIO_TOP_LEVEL})
option(VCDIO_BUILD_TOOLS    "Build the vcdio command line tool"   ${VCDIO_TOP_LEVEL})
option(VCDIO_BUILD_EXAMPLES "Build the VCDio examples"            ${VCDIO_TOP_LEVEL})
option(VCDIO_NO_TIME        "Do not call time()/localtime() (targets without a clock)" OFF)

set(VCDIO_WARNINGS
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes>
)

# --- Library -------------------------------------------------------------------------------------------------
add_library(VCDio STATIC
    src/vcd_io.c
    src/vcd_writer.c
    src/vcd_reader.c
    src/vcd_uart.c
)
add_library(VCDio::VCDio ALIAS VCDio)

target_include_directories(VCDio
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(VCDio PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_compile_options(VCDio PRIVATE ${VCDIO_WARNINGS})
if(VCDIO_NO_TIME)
    target_compile_definitions(VCDio PRIVATE VCDIO_NO_TIME)
endif()

# libm (floor, isfinite). Linked by name: find_library() can miss it under cross/toolchain files.
if(NOT MSVC)
    target_link_libraries(VCDio PUBLIC m)
endif()

# --- Tests ---------------------------------------------------------------------------------------------------
if(VCDIO_BUILD_TESTS)
    enable_testing()
    add_executable(vcdio_tests tests/test_vcdio.c)
    target_link_libraries(vcdio_tests PRIVATE VCDio)
    target_include_directories(vcdio_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(vcdio_tests PROPERTIES C_STANDARD 99 C_EXTENSIONS OFF)
    target_compile_options(vcdio_tests PRIVATE ${VCDIO_WARNINGS})
    add_test(NAME vcdio_tests COMMAND vcdio_tests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()

# --- Command line tool ---------------------------------------------------------------------------------------
if(VCDIO_BUILD_TOOLS)
    add_executable(vcdio tools/vcdio.c)
    target_link_libraries(vcdio PRIVATE VCDio)
    set_target_properties(vcdio PROPERTIES C_STANDARD 99 C_EXTENSIONS OFF)
    target_compile_options(vcdio PRIVATE ${VCDIO_WARNINGS})
endif()

# --- Examples ------------------------------------------------------------------------------------------------
if(VCDIO_BUILD_EXAMPLES)
    add_executable(vcdio_kline_example examples/kline_example.c)
    target_link_libraries(vcdio_kline_example PRIVATE VCDio)
    set_target_properties(vcdio_kline_example PROPERTIES C_STANDARD 99 C_EXTENSIONS OFF)
    target_compile_options(vcdio_kline_example PRIVATE ${VCDIO_WARNINGS})
endif()
