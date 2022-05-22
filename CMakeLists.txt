cmake_minimum_required(VERSION 3.21)
project(numa_bind)

set(CMAKE_CXX_STANDARD 14)

add_executable(numa_bind main.cpp)

target_link_libraries(numa_bind -lnuma)
target_link_libraries(numa_bind -lpthread)