cmake_minimum_required(VERSION 3.14)
project(pmbench)
add_subdirectory(rng)

set(CMAKE_CXX_STANDARD 14)

SET(GCC_COMPILE_FLAGS "-pthread -mavx")

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COMPILE_FLAGS}")

add_executable(latency latency.cpp)
add_executable(bandwidth bandwidth.cpp)
target_link_libraries(latency PRIVATE pmbench-rng)