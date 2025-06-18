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

# Find liburing using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED liburing)
message(STATUS "liburing include dirs: ${LIBURING_INCLUDE_DIRS}")
message(STATUS "liburing libraries:    ${LIBURING_LIBRARIES}")


# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${LIBURING_INCLUDE_DIRS}
)

# Link directories (usually not needed unless using a non-standard location)
link_directories(${LIBURING_LIBRARY_DIRS})

# Source files
add_executable(demo
    src/uring_ctx.c
    src/main_demo.c
)

# Link libraries
target_link_libraries(demo
    pthread
    ${OpenMP_C_FLAGS}    # If OpenMP was found
    ${LIBURING_LIBRARIES}
)
