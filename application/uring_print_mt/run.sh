#!/usr/bin/env bash
set -e

clang --version

cd "$(dirname "$0")"
#rm -fr build
mkdir -p build
export CC=/home/cc/gpu-libc-artifacts/llvm-infra/llvm-installs/apptainer-Debug/bin/clang
export CXX=/home/cc/gpu-libc-artifacts/llvm-infra/llvm

cmake -S . -B build \
 -DCMAKE_PREFIX_PATH=/home/cc/gpu-libc-artifacts/llvm-infra/llvm-installs/apptainer-Debug \


cd build
cmake --build . -- VERBOSE=1

echo "--------------"
./demo_cpu
echo "--------------"
./demo_target
