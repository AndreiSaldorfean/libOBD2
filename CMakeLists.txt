cmake_minimum_required(VERSION 3.14)
project(OBD2 LANGUAGES C)

# --- FreeRTOS Configuration Setup ---
# This MUST be defined before adding the FreeRTOS subdirectory
option(SPT_FREERTOS "Support for FreeRTOS" ON)
option(SPT_TRACING  "Support for tracing" OFF)

if(SPT_FREERTOS STREQUAL ON)
    message("FreeRTOS= ON")
else()
    message("FreeRTOS= OFF")
endif()
if(SPT_TRACING STREQUAL ON)
    message("Tracing= ON")

    if("${PLATFORM}" STREQUAL "")
        message(SEND_ERROR "Platform cannot be empty")
    else()
        message("Platform= ${PLATFORM}")
    endif()
else()
    message("Tracing= OFF")
endif()

if(SPT_FREERTOS STREQUAL ON)
    add_library(freertos_config INTERFACE)

    target_include_directories(freertos_config SYSTEM
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/ports/stm32f4/config #TODO: change stm32f4 with a variable
    )

    target_compile_definitions(freertos_config
        INTERFACE
        projCOVERAGE_TEST=0
    )

    # Set FreeRTOS build variables
    set(FREERTOS_PORT "GCC_ARM_CM4F" CACHE STRING "" FORCE)
    set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)

    # Add the FreeRTOS Kernel
    add_subdirectory(
        ${CMAKE_CURRENT_LIST_DIR}/examples/libs/FreeRTOS-Kernel
        ${CMAKE_BINARY_DIR}/FreeRTOS-Kernel
    )

    # Explicitly link the config to the kernel targets
    # The error happens in the 'port' target specifically
    target_link_libraries(freertos_kernel PUBLIC freertos_config)
    target_link_libraries(freertos_kernel_port PUBLIC freertos_config)
endif()

if(SPT_TRACING STREQUAL ON)
    file(GLOB SOURCES
        "./ports/${PLATFORM}/trace.c"
    )
endif()

file(GLOB SOURCES
    "./src/modules/app/*.c"
    "./src/modules/datalink/common/*.c"
    "./src/modules/datalink/specific/l2_iso9141/*.c"
    "./src/modules/datalink/specific/l2_kwp2000/*.c"
    "./utils/*.c"
    "./src/data_link/l2_kwp/*.c"
)

add_library(${PROJECT_NAME} ${SOURCES})

if(SPT_FREERTOS STREQUAL ON)
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        SPT_FREERTOS
    )
endif()
if(SPT_TRACING STREQUAL ON)
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        SPT_TRACING
    )
endif()
if(LOGGING STREQUAL "ON")
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        LOGGING
    )
endif()

if(SPT_FREERTOS STREQUAL ON)
    target_include_directories(${PROJECT_NAME}
        PUBLIC
        ./examples/libs/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/
        ./examples/libs/FreeRTOS-Kernel/include/
    )

    target_link_libraries(${PROJECT_NAME}
        PUBLIC
        freertos_config
        freertos_kernel
    )
endif()

if(SPT_TRACING STREQUAL ON)
    target_include_directories(${PROJECT_NAME}
        PUBLIC
        ./ports/${PLATFORM}/
    )
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ./include/
    ./utils/
    ./src/modules/datalink/common
    ./src/modules/datalink/specific/l2_iso9141/
    ./src/modules/datalink/specific/l2_kwp2000/
    ./src/modules/app/
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -save-temps=obj
)

