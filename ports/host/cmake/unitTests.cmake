set(TEST_USE_LEGACY_SOURCES OFF)
set(TEST_USE_ECU OFF)
set(TEST_ADD_BIN_POST_BUILD OFF)

set(TEST_PLATFORM_SOURCES
    "${CMAKE_SOURCE_DIR}/Unity/src/unity.c"
    "${CMAKE_SOURCE_DIR}/src/unity_output_silent.c"
    "${CMAKE_SOURCE_DIR}/src/unit/drivers/host/uart_pbt_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/datalink/host/specific/iso9141/iso9141_pbt_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/datalink/host/common/dl_pbt_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/app/host/libobd2_pbt_test.c"
)

list(APPEND TEST_PLATFORM_INCLUDES
    src/unit/drivers/host
    src/unit/datalink/host/specific/iso9141/
    src/unit/datalink/host/common/
    src/unit/app/host/
)

# VCDio: waveform traces of the virtual K-line (tests/src/ports/host/uart_trace.c)
add_subdirectory(${CMAKE_SOURCE_DIR}/../libs/VCDio ${CMAKE_BINARY_DIR}/VCDio)
list(APPEND TEST_PLATFORM_LINK_LIBS VCDio)
