cmake_minimum_required(VERSION 3.27)
project(2dqr)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEBUG "Enable debug mode" OFF)

if (DEBUG)
    add_definitions(-DDEBUG)
endif()

find_package(cxxopts CONFIG REQUIRED)
find_package(Boost REQUIRED)

include_directories(inc)
include_directories(${Boost_INCLUDE_DIRS})

file(GLOB SRC_2DQR "src/2dqr/*.cpp")
file(GLOB SRC_2DQR_EXT "src/2dqr_ext/*.cpp")
file(GLOB SRC_ABC_WRAPPER "src/abc_wrapper/*.cpp")
file(GLOB SRC_ALGO_BASE "src/algorithm_base/*.cpp")
file(GLOB SRC_COMMON "src/common/*.cpp")

include(ExternalProject)
ExternalProject_Add(abc
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/abc
    BUILD_COMMAND make libabc-pic
    INSTALL_COMMAND ""
)

add_executable(2dqr "src/main.cpp" ${SRC_COMMON} ${SRC_ALGO_BASE} ${SRC_2DQR} ${SRC_2DQR_EXT} ${SRC_ABC_WRAPPER})
add_library(abc-pic STATIC IMPORTED)
set_target_properties(abc-pic PROPERTIES
    IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/abc-prefix/src/abc-build/libabc-pic.a
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/abc/src/
)
target_link_libraries(2dqr PRIVATE abc-pic)
target_link_libraries(2dqr PRIVATE cxxopts::cxxopts)
target_link_libraries(2dqr PRIVATE edit)

add_executable(verifier "src/verifier.cpp" ${SRC_COMMON} ${SRC_ABC_WRAPPER})
target_link_libraries(verifier PRIVATE abc-pic)
target_link_libraries(verifier PRIVATE cxxopts::cxxopts)
target_link_libraries(verifier PRIVATE edit)
