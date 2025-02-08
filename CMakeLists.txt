cmake_minimum_required(VERSION 3.10)
project(SHIT-Compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(-Wall)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lm")

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DSHIT_DEBUG)
endif ()

add_subdirectory(src)