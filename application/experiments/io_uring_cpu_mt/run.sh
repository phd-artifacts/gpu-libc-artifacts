#!/usr/bin/env bash
set -e

. ~/spack/share/spack/setup-env.sh # load spack
spack load hsa-rocr-dev@6.4.1 /rlpc6ww5r7u

clang --version

cd "$(dirname "$0")"
#rm -fr build
mkdir -p build
export CC=clang
export CXX=clang++
export LDFLAGS="-fuse-ld=lld"

export CPATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/include/${CPATH:+:$CPATH}
export LIBRARY_PATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/lib${LIBRARY_PATH:+:$LIBRARY_PATH}
export LD_LIBRARY_PATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}

cmake -S . -B build \
 -DCMAKE_PREFIX_PATH=/home/cc/gpu-libc-artifacts/llvm-infra/llvm-installs/apptainer-Debug \
 -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" \
 -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld"



cd build
cmake --build . -- VERBOSE=1

# echo "--------------"
./demo_cpu
# echo "--------------"
# LIBOMPTARGET_DEBUG=1 ./demo_target
# ./demo_target
