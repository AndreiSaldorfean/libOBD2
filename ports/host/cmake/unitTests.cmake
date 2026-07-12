set(TEST_USE_LEGACY_SOURCES OFF)
set(TEST_USE_ECU OFF)
set(TEST_ADD_BIN_POST_BUILD OFF)

set(TEST_PLATFORM_SOURCES
    "${CMAKE_SOURCE_DIR}/Unity/src/unity.c"
    "${CMAKE_SOURCE_DIR}/src/unity_output_silent.c"
    "${CMAKE_SOURCE_DIR}/src/unit/drivers/host/timer_test.c"
    "${CMAKE_SOURCE_DIR}/src/unit/drivers/host/uart_test.c"
)

list(APPEND TEST_PLATFORM_INCLUDES
    src/unit/drivers/host
)
