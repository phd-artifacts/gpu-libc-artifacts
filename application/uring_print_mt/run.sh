#!/usr/bin/env bash
set -e

. ~/spack/share/spack/setup-env.sh # load spack
spack load hsa-rocr-dev@6.4.1 

clang --version

cd "$(dirname "$0")"
#rm -fr build
mkdir -p build
export CC=clang
export CXX=clang++
export LDFLAGS="-fuse-ld=lld"



cmake -S . -B build \
 -DCMAKE_PREFIX_PATH=/home/cc/gpu-libc-artifacts/llvm-infra/llvm-installs/apptainer-Debug \
 -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" \
 -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld"



cd build
cmake --build . -- VERBOSE=1

# echo "--------------"
# ./demo_cpu
echo "--------------"
LIBOMPTARGET_DEBUG=1 ./demo_target
