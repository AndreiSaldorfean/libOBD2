set(TEST_USE_LEGACY_SOURCES OFF)
set(TEST_USE_ECU ON)
set(TEST_ADD_BIN_POST_BUILD OFF)

# Unit tests: every layer keeps its test cases in <type>/<category>/, next to the fixture they share:
#   type     positive/ (expected use) or negative/ (invalid input, faults, timeouts)
#   category ex/ (example-based) or pbt/ (property-based)
# New test files are picked up automatically.
set(UNIT_TEST_LAYER_DIRS
    "${CMAKE_SOURCE_DIR}/src/unit/common/host_virtual"   # helpers shared by all layers (pbt_support)
    "${CMAKE_SOURCE_DIR}/src/unit/drivers/host_virtual"
    "${CMAKE_SOURCE_DIR}/src/unit/datalink/host_virtual"
    "${CMAKE_SOURCE_DIR}/src/unit/app/host_virtual"
)

set(TEST_PLATFORM_SOURCES
    "${CMAKE_SOURCE_DIR}/Unity/src/unity.c"
    "${CMAKE_SOURCE_DIR}/src/unity_output_silent.c"
)

foreach(layerDir IN LISTS UNIT_TEST_LAYER_DIRS)
    file(GLOB layerSources CONFIGURE_DEPENDS "${layerDir}/*.c")
    list(APPEND TEST_PLATFORM_SOURCES ${layerSources})
    list(APPEND TEST_PLATFORM_INCLUDES "${layerDir}")
    foreach(testDir positive/ex positive/pbt negative/ex negative/pbt)
        file(GLOB testSources CONFIGURE_DEPENDS "${layerDir}/${testDir}/*.c")
        list(APPEND TEST_PLATFORM_SOURCES ${testSources})
        list(APPEND TEST_PLATFORM_INCLUDES "${layerDir}/${testDir}")
    endforeach()
endforeach()

# Lets shared test sources (e.g. tests/src/test_vectors) pick their host variant
list(APPEND TEST_PLATFORM_DEFINES PLATFORM_HOST_VIRTUAL)

# VCDio: waveform traces of the virtual K-line (tests/src/ports/host_virtual/uart_trace.c)
add_subdirectory(${CMAKE_SOURCE_DIR}/../libs/VCDio ${CMAKE_BINARY_DIR}/VCDio)
list(APPEND TEST_PLATFORM_LINK_LIBS VCDio)
