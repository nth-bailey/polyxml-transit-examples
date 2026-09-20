cmake_minimum_required(VERSION 3.20)
project(gtfs_siri_bridge_cpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(gtfs_siri_bridge main.cpp)

target_include_directories(gtfs_siri_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../generated/cpp
)
