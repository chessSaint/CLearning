cmake_minimum_required(VERSION 3.21)
project(numa_bind)

set(CMAKE_CXX_STANDARD 14)

add_executable(numa_bind main.cpp)
add_executable(testify testify.cpp)
add_executable(test test.cpp)
add_executable(not_overlap not_overlap.cpp)


target_link_libraries(numa_bind -lnuma)
target_link_libraries(numa_bind -lpthread)