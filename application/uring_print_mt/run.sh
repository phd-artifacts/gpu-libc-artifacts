#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"
rm -fr build
mkdir -p build

cd build
cmake ..
# cmake --build . -- VERBOSE=1
cmake --build . -- VERBOSE=1

echo "--------------"
./demo
echo "--------------"
./demo
echo "--------------"
./demo
echo "--------------"
./demo
echo "--------------"
./demo
