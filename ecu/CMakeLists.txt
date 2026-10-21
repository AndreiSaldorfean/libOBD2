cmake_minimum_required(VERSION 3.14)
project(OBD2_ECU LANGUAGES C)

if("${PLATFORM}" STREQUAL "")
    message(FATAL_ERROR "PLATFORM is required to build the ECU (e.g. stm32f4, stm32g4, host)")
endif()

# --- libOBD2 (base library) ---
# libOBD2's CMakeLists owns FreeRTOS setup; add it first so its targets
# (freertos_config, freertos_kernel, OBD2) are available to us.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. ${CMAKE_BINARY_DIR}/libOBD2)

# --- ECU simulator sources ---
file(GLOB ECU_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/modules/app/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/modules/datalink/common/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/modules/datalink/specific/l2_iso9141/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/modules/datalink/specific/l2_kwp2000/*.c"
)

add_library(${PROJECT_NAME} ${ECU_SOURCES})

if(SPT_FREERTOS STREQUAL "ON")
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        SPT_FREERTOS
    )
endif()

if(DEBUG STREQUAL "ON")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/modules/app/
    ${CMAKE_CURRENT_LIST_DIR}/modules/datalink/common/
    ${CMAKE_CURRENT_LIST_DIR}/modules/datalink/specific/l2_iso9141/
    ${CMAKE_CURRENT_LIST_DIR}/modules/datalink/specific/l2_kwp2000/
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -save-temps=obj
)

# Link against libOBD2 — this transitively brings in freertos_kernel and
# freertos_config, so no need to list them separately.
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    OBD2
)
