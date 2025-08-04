#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

COMMON_FLAGS="-fopenmp \
  -g \
  --offload-arch=gfx908 \
  -resource-dir=/home/cc/gpu-libc-artifacts/llvm-infra/llvm-installs/apptainer-Debug/lib/clang/21 \
  -frtlib-add-rpath \
  -fopenmp-targets=amdgcn-amd-amdhsa \
  -Iinclude"

# extra flags used only when linking
LINK_FLAGS="-fuse-ld=lld -lhsa-runtime64"

export CC=clang
export CXX=clang++
export LDFLAGS="-fuse-ld=lld"

export CPATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/include/${CPATH:+:$CPATH}
export LIBRARY_PATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/lib${LIBRARY_PATH:+:$LIBRARY_PATH}
export LD_LIBRARY_PATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}

"$CXX" --version

echo "Compiling sources..."

# build sources with clang++
"$CXX" $COMMON_FLAGS -c src/uring_ctx.cpp -o uring_ctx.o
"$CXX" $COMMON_FLAGS -c src/uring_device.cpp -o uring_device.o
"$CXX" $COMMON_FLAGS -c src/main_demo_target.cpp -o main_demo_target.o

# link using clang++ so C++ runtime is included
"$CXX" $COMMON_FLAGS uring_ctx.o uring_device.o main_demo_target.o $LINK_FLAGS -o demo

set +e

printf "Done compiling\n\n\n\n"

./demo
