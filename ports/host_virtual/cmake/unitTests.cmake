set(TEST_USE_LEGACY_SOURCES OFF)
set(TEST_USE_ECU ON)
set(TEST_ADD_BIN_POST_BUILD OFF)

set(TEST_PLATFORM_SOURCES
    "${CMAKE_SOURCE_DIR}/Unity/src/unity.c"
    "${CMAKE_SOURCE_DIR}/src/unity_output_silent.c"
    "${CMAKE_SOURCE_DIR}/src/unit/drivers/host_virtual/uart_pbt_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/drivers/host_virtual/uart_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/drivers/host_virtual/timer_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/datalink/host_virtual/specific/iso9141/iso9141_pbt_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/datalink/host_virtual/specific/iso9141/iso9141_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/datalink/host_virtual/common/dl_pbt_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/datalink/host_virtual/common/dl_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/app/host_virtual/libobd2_pbt_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/app/host_virtual/libobd2_test.c"
)

list(APPEND TEST_PLATFORM_INCLUDES
    src/unit/drivers/host_virtual
    src/unit/datalink/host_virtual/specific/iso9141/
    src/unit/datalink/host_virtual/common/
    src/unit/app/host_virtual/
)

# Lets shared test sources (e.g. tests/src/test_vectors) pick their host variant
list(APPEND TEST_PLATFORM_DEFINES PLATFORM_HOST_VIRTUAL)

# VCDio: waveform traces of the virtual K-line (tests/src/ports/host_virtual/uart_trace.c)
add_subdirectory(${CMAKE_SOURCE_DIR}/../libs/VCDio ${CMAKE_BINARY_DIR}/VCDio)
list(APPEND TEST_PLATFORM_LINK_LIBS VCDio)
