cmake_minimum_required(VERSION 3.16)
project(high_perf_timer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -O2)

include_directories(${CMAKE_SOURCE_DIR}/include)

find_package(Threads REQUIRED)

add_library(timer STATIC
    src/min_heap_timer.cpp
    src/time_wheel_timer.cpp
    src/timer_manager.cpp
)
target_link_libraries(timer PUBLIC Threads::Threads)

add_executable(test_timer tests/test_timer.cpp)
target_link_libraries(test_timer PRIVATE timer)

add_executable(epoll_example examples/epoll_server.cpp)
target_link_libraries(epoll_example PRIVATE timer)