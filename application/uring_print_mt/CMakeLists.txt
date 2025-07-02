cmake_minimum_required(VERSION 3.20)
project(demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

find_package(OpenMP REQUIRED)

#  Executable: pure-CPU version
add_executable(demo_cpu
    src/uring_ctx.c
    src/main_demo_cpu.c)

target_compile_options(demo_cpu PRIVATE ${OpenMP_C_FLAGS})
target_link_libraries(demo_cpu PRIVATE pthread OpenMP::OpenMP_C)

#  Executable: GPU off-load version
set(GPU_ARCH "gfx90a" CACHE STRING "AMD GCN target passed to -march")

set(OFFLOAD_FLAGS
    -fopenmp
    -fopenmp-offload-mandatory
    -fopenmp-targets=amdgcn-amd-amhsa
    -Xopenmp-target -march=${GPU_ARCH}
    -nogpulib)

add_executable(demo_target
    src/uring_ctx.c          # shared helpers
    src/uring_device.c       # GPU kernel + host stub
    src/main_demo_target.c)  # launches the kernel

target_compile_options(demo_target PRIVATE ${OFFLOAD_FLAGS})
target_link_options  (demo_target PRIVATE ${OFFLOAD_FLAGS} -lhsa-runtime64)
target_link_libraries(demo_target PRIVATE pthread)

# LTO is optional but often useful with off-load
set_property(TARGET demo_target PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
