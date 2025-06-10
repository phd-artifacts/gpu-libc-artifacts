cmake_minimum_required(VERSION 3.10)

project(demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(OpenMP)

if(OpenMP_C_found)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(demo
    src/uring_ctx.c
    src/uring_fprintf.c
    src/main_demo.c
)