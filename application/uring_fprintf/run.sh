#!/usr/bin/env bash

rm -fr build
mkdir -p build

cd build

cmake ..

cmake --build .

./demo
