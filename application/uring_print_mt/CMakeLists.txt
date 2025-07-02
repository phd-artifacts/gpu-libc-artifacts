cmake_minimum_required(VERSION 3.10)

project(demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find OpenMP
find_package(OpenMP)

if(OpenMP_C_FOUND)
    message(STATUS "Found OpenMP")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Link directories (usually not needed unless using a non-standard location)


# Source files
add_executable(demo_cpu
    src/uring_ctx.c
    src/main_demo_cpu.c
)

add_executable(demo_target
    src/uring_ctx.c
    src/main_demo_target.c
)

# Link libraries
target_link_libraries(demo_cpu
    pthread
    ${OpenMP_C_FLAGS}    # If OpenMP was found
)

# Link libraries
target_link_libraries(demo_target
    pthread
    ${OpenMP_C_FLAGS}    # If OpenMP was found
)
