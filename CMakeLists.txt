cmake_minimum_required(VERSION 3.10)

set(LANGUAGES C)
include (GNUInstallDirs)

project(HOT-RELOADER)
set(CMAKE_C_STANDARD 99)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin)
set(CMAKE_BUILD_TYPE Release)
include_directories(${CMAKE_SOURCE_DIR})

add_executable(hot-reload hot-reload.c utils.c)

install(TARGETS hot-reload DESTINATION bin)
