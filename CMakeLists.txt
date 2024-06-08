cmake_minimum_required(VERSION 3.10)
project(test VERSION 1.0)

find_package(Threads REQUIRED)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

add_executable(main main.c sdcard_util.c)
target_link_libraries(main PRIVATE Threads::Threads)

add_executable(test test.c sdcard_util.c)
target_link_libraries(test PRIVATE Threads::Threads)