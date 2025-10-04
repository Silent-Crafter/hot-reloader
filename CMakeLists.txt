cmake_minimum_required(VERSION 3.10)

set(LANGUAGES C)

project(HOT-RELOADER)
set(CMAKE_C_STANDARD 99)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin)

# Add compiler flags for POSIX and GNU extensions
add_compile_definitions(_GNU_SOURCE _POSIX_C_SOURCE=200809L)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -DDO_DEBUG -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

include_directories(${CMAKE_SOURCE_DIR})

add_executable(hot-reload hot-reload.c utils.c)

install(TARGETS hot-reload DESTINATION bin)
