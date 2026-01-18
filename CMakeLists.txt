cmake_minimum_required(VERSION 3.14)
project(OBD2 LANGUAGES C)

set(CMAKE_EXECUTABLE_SUFFIX ".a")

file(GLOB SOURCES "./src/*.c")

add_library(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ./include/
)

