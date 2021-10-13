cmake_minimum_required(VERSION 3.14)
project(pmbench)
add_subdirectory(rng)

set(CMAKE_CXX_STANDARD 14)

add_executable(latency latency.cpp)
target_link_libraries(latency PRIVATE pmbench-rng)