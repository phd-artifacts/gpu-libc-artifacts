#!/usr/bin/env bash
set -e

clang --version

cd "$(dirname "$0")"
#rm -fr build
#mkdir -p build

cd build
cmake ..
# cmake --build . -- VERBOSE=1
#cmake --build . -- VERBOSE=1

echo "--------------"
./demo_cpu
echo "--------------"
#./demo_target
