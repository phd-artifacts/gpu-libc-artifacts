#!/bin/bash

# Check if the first argument is provided
if [ -z "$1" ]; then
  echo "Error: INSTALL_PATH is not set."
  exit 1
fi

# Set the installation path
INSTALL_PATH=$(realpath "$1")
SELECTED_CLANG="$INSTALL_PATH"

# Add the bin directory to PATH if not already included
if [[ ":$PATH:" != *":$INSTALL_PATH/bin:"* ]]; then
  export PATH="$INSTALL_PATH/bin:$PATH"
fi

export LLVM_DIR="$INSTALL_PATH/lib/cmake/llvm"

# Export library paths if needed
#export LD_LIBRARY_PATH="$INSTALL_PATH/lib:$LD_LIBRARY_PATH"
#export LIBRARY_PATH="$INSTALL_PATH/lib:$LIBRARY_PATH"
#export C_INCLUDE_PATH="$INSTALL_PATH/include:$C_INCLUDE_PATH"
#export CPLUS_INCLUDE_PATH="$INSTALL_PATH/include:$CPLUS_INCLUDE_PATH"
#export LD_LIBRARY_PATH="$INSTALL_PATH/lib:$INSTALL_PATH/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH"
#export LIBRARY_PATH="$INSTALL_PATH/lib:$INSTALL_PATH/lib/x86_64-unknown-linux-gnu:$LIBRARY_PATH"

CLANG_INCLUDE_PATH=(${SELECTED_CLANG}/lib/clang/*/include)
export PATH=${SELECTED_CLANG}/bin:$PATH
export LD_LIBRARY_PATH=${SELECTED_CLANG}/lib/x86_64-unknown-linux-gnu:${SELECTED_CLANG}/lib:$LD_LIBRARY_PATH
export LIBRARY_PATH=${SELECTED_CLANG}/lib:$LIBRARY_PATH
export CPATH=$CLANG_INCLUDE_PATH:$CPATH

#omp headers: projects/openmp/runtime/src
omp_header_path=${SELECTED_CLANG}/projects/openmp/runtime/src
export CPATH=$omp_header_path:$CPATH

export CC=clang
export CXX=clang++

# Optionally, print confirmation
echo "Environment variables set for $SELECTED_CLANG"

# Check if chosen clang is on $INSTALL_PATH/bin
if [ -f "$INSTALL_PATH/bin/clang" ]; then
  echo "Using clang:"
  /usr/bin/which clang
else
  echo "Error: clang is not found in $INSTALL_PATH/bin."
  exit 1
fi
