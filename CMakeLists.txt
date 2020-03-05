cmake_minimum_required(VERSION 2.8.9)
project(PA3)

add_executable(multi-lookup ./multi-lookup.c)
target_link_libraries(multi-lookup pthread)

