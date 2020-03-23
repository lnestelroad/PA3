# Disable in-source builds to prevent source tree corruption.
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "
FATAL: In-source builds are not allowed.
       You should create a separate directory for build files.
")
endif()

cmake_minimum_required(VERSION 2.8.9)
project(PA3)

add_executable(multi-lookup ./multi-lookup.c)
target_link_libraries(multi-lookup pthread)
target_compile_options(multi-lookup PRIVATE -Werror -Wall -Wextra)
