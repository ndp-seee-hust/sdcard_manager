cmake_minimum_required(VERSION 3.10)
project(test VERSION 1.0)

find_package(Threads REQUIRED)
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)


include_directories(include)
set(SOURCE_FILES
    src/sdcard_util.c
    src/unity.c
    unitest/test.c
)

add_executable(test ${SOURCE_FILES})
target_link_libraries(test PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(test_sys unitest/test_sys.c)
target_link_libraries(test_sys PRIVATE Threads::Threads ${CURSES_LIBRARIES})