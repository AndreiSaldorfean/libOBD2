# Builds libserialport (examples/libs/libserialport, a git submodule) as the static library
# `serialport`, without its autotools build. Only Linux is supported for now.
if(TARGET serialport)
    return()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "host_serial: libserialport is only built for Linux (got ${CMAKE_SYSTEM_NAME})")
endif()

get_filename_component(LIBSERIALPORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../examples/libs/libserialport" ABSOLUTE)
if(NOT EXISTS "${LIBSERIALPORT_ROOT}/serialport.c")
    message(FATAL_ERROR "libserialport not found in ${LIBSERIALPORT_ROOT}; run: git submodule update --init examples/libs/libserialport")
endif()

set(SP_VERSION_MAJOR 0)
set(SP_VERSION_MINOR 1)
set(SP_VERSION_MICRO 2)
set(LIBSERIALPORT_CONFIG_DIR "${CMAKE_BINARY_DIR}/libserialport")
configure_file("${CMAKE_CURRENT_LIST_DIR}/libserialport_config.h.in" "${LIBSERIALPORT_CONFIG_DIR}/config.h" @ONLY)

add_library(serialport STATIC
    "${LIBSERIALPORT_ROOT}/serialport.c"
    "${LIBSERIALPORT_ROOT}/timing.c"
    "${LIBSERIALPORT_ROOT}/linux.c"
    "${LIBSERIALPORT_ROOT}/linux_termios.c"
)

# Our config.h first: the source tree may hold one left by a manual ./configure
target_include_directories(serialport BEFORE PRIVATE "${LIBSERIALPORT_CONFIG_DIR}")
target_include_directories(serialport PUBLIC "${LIBSERIALPORT_ROOT}")
target_compile_definitions(serialport PRIVATE _GNU_SOURCE LIBSERIALPORT_ATBUILD)
target_compile_options(serialport PRIVATE -w)
