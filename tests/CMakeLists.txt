cmake_minimum_required(VERSION 3.14)
project(test LANGUAGES C)

option(SPT_FREERTOS "Support for FreeRTOS" ON)

if("${PLATFORM}" STREQUAL "")
    message(FATAL_ERROR "PLATFORM is required (e.g. stm32f4, stm32g4, host)")
endif()

# Platform unit-test customization entrypoint.
set(TEST_EXECUTABLE_SUFFIX "")
set(TEST_USE_LEGACY_SOURCES ON)
set(TEST_USE_ECU ON)
set(TEST_ADD_BIN_POST_BUILD OFF)
set(TEST_PLATFORM_SOURCES "")
set(TEST_PLATFORM_INCLUDES "")
set(TEST_PLATFORM_DEFINES "")
set(TEST_PLATFORM_LINK_LIBS "")

set(UNIT_TESTS_PLATFORM_CMAKE "${CMAKE_SOURCE_DIR}/../ports/${PLATFORM}/cmake/unitTests.cmake")
if(NOT EXISTS "${UNIT_TESTS_PLATFORM_CMAKE}")
    message(FATAL_ERROR "Missing platform config: ${UNIT_TESTS_PLATFORM_CMAKE}")
endif()
include("${UNIT_TESTS_PLATFORM_CMAKE}")

if(NOT "${TEST_EXECUTABLE_SUFFIX}" STREQUAL "")
    set(CMAKE_EXECUTABLE_SUFFIX "${TEST_EXECUTABLE_SUFFIX}")
endif()

file(GLOB SOURCES
    "${CMAKE_SOURCE_DIR}/Unity/src/unity.c"
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/src/main.c"
    "${CMAKE_SOURCE_DIR}/src/unit/modules/app/*.c"
    "${CMAKE_SOURCE_DIR}/src/unit/modules/datalink/common/*.c"
    "${CMAKE_SOURCE_DIR}/src/unit/modules/datalink/specific/l2_iso9141/*.c"
    "${CMAKE_SOURCE_DIR}/src/unit/modules/datalink/specific/l2_kwp2000/*.c"
    "${CMAKE_SOURCE_DIR}/src/unit/ports/${PLATFORM}/*.c"
    "${CMAKE_SOURCE_DIR}/src/ports/${PLATFORM}/*.c"
    "${CMAKE_SOURCE_DIR}/src/os/*.c"
    "${CMAKE_SOURCE_DIR}/../ports/${PLATFORM}/startup/*.c"
    "${CMAKE_SOURCE_DIR}/../ports/${PLATFORM}/*.c"
)

list(APPEND SOURCES ${TEST_PLATFORM_SOURCES})

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wno-error=overflow
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ./inc/
    ../include
    ../include/freertos/
    "../ports/${PLATFORM}/"
    ../utils
    ../interface/
    ../include/app/
    ../include/data_link/
    ../include/data_link/l2_kwp/
    ../include/data_link/l2_iso9141/
    ./src/
    ./src/unit/modules/app/
    ./src/unit/modules/datalink/common
    ./src/unit/modules/datalink/specific/l2_iso9141/
    ./src/unit/modules/datalink/specific/l2_kwp2000/
    ./src/test_vectors/
    "./src/unit/ports/${PLATFORM}/"
    "./src/ports/${PLATFORM}/"
    ./src/os/
    ./Unity/src/
    ${TEST_PLATFORM_INCLUDES}
)

if(TEST_PLATFORM_DEFINES)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ${TEST_PLATFORM_DEFINES})
endif()

if(SPT_FREERTOS STREQUAL "ON")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SPT_FREERTOS)
endif()

if(LOGGING STREQUAL "ON")
    target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGING)
endif()

if(TEST_USE_ECU)
    # ECU CMake pulls in libOBD2 as a dependency and defines OBD2/OBD2_ECU.
    add_subdirectory(${CMAKE_SOURCE_DIR}/../ecu ${CMAKE_BINARY_DIR}/ecu)
elseif(NOT TARGET OBD2)
    # Host/non-ECU path: build base library directly so tests can link OBD2.
    add_subdirectory(${CMAKE_SOURCE_DIR}/.. ${CMAKE_BINARY_DIR}/libOBD2)
endif()

if(TARGET OBD2)
    target_compile_definitions(OBD2
        PRIVATE
        UNIT_TESTS
    )
endif()

if(TARGET OBD2_ECU)
    target_compile_definitions(OBD2_ECU
        PRIVATE
        UNIT_TESTS
    )
endif()

if(TEST_USE_ECU)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        OBD2
        OBD2_ECU
        ${TEST_PLATFORM_LINK_LIBS}
    )
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC
        OBD2
        ${TEST_PLATFORM_LINK_LIBS}
    )
endif()

if(TEST_ADD_BIN_POST_BUILD)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
        COMMENT "Generating ${PROJECT_NAME}.bin"
    )
endif()
