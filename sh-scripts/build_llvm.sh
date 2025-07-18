#!/usr/bin/bash
set -e

build_type=Debug
base_folder=$(realpath ./llvm-infra/)

install_name="$base_folder/llvm-installs/apptainer-$build_type"
source_name="$base_folder/llvm-project/llvm"
build_name="$base_folder/llvm-builds/apptainer-$build_type"
parent_folder=$(dirname "$base_folder")

# Activate python venv in this shell
#source $parent_folder/.venv_apptainer/bin/activate

echo "Activated python!"
python -c "import yaml; print('pyyaml is available!')"

# HIP stuff ???
export PATH=/opt/rocm-6.4.1/bin:$PATH
export PATH=/opt/rocm-6.4.1/llvm/bin:/opt/rocm-6.4.1/bin:$PATH
export LD_LIBRARY_PATH=/opt/rocm-6.4.1/lib:$LD_LIBRARY_PATH
export ROCM_PATH=/opt/rocm-6.4.1
export DEVICE_LIB_PATH=$ROCM_PATH/amdgcn/bitcode
export HIP_CLANG_PATH=$INSTALL_PREFIX/bin
export HSA_PATH=$ROCM_PATH
export HIP_PATH=$ROCM_PATH
export PATH=$HIP_CLANG_PATH:$ROCM_PATH/bin:$ROCM_PATH/llvm/bin:$PATH


# Export environment variables for CMake
#PYTHON_EXEC="$parent_folder/.venv_apptainer/bin/python"
#PYTHON_INCLUDE_DIR="$parent_folder/.venv_apptainer/include"
#PYTHON_LIBRARY_DIR="$parent_folder/.venv_apptainer/lib"

#export Python3_ROOT_DIR="$parent_folder/.venv_apptainer"
#export PATH="$parent_folder/.venv_apptainer/bin:$PATH"

mkdir -p "$install_name"
mkdir -p "$build_name"

CLEAN=0
RUNCMAKE=0
if [[ "$1" == "--clean" ]]; then
  CLEAN=1
fi

if [[ "$2" == "--run-cmake" ]]; then
  RUNCMAKE=1
fi

VALID_BUILD_TYPES=("Debug" "Release" "RelWithDebInfo" "MinSizeRel")
is_valid_build_type() {
  for type in "${VALID_BUILD_TYPES[@]}"; do
    if [[ "$1" == "$type" ]]; then
      return 0
    fi
  done
  return 1
}

if ! is_valid_build_type "$build_type"; then
  echo "Error: Invalid build type '$build_type'. Valid types are: ${VALID_BUILD_TYPES[*]}"
  exit 1
fi
# Check if CLEAN flag is passed
if [ "$CLEAN" == "1" ]; then
  echo ""
  echo "============================="
  echo "⚠️  Warning: Cleaning folders"
  echo "============================="
  echo "The following folders will be cleaned in 5 seconds:"
  echo " - $build_name"
  echo " - $install_name"
  echo ""
  echo "Press Ctrl+C to cancel..."
  sleep 5

  # Clean the folders
  echo "Cleaning folder: $build_name"
  rm -rf "$build_name"

  echo "Cleaning folder: $install_name"
  rm -rf "$install_name"

  #echo "Cleaning python folder: $parent_folder/.venv_apptainer"
  #rm -rf .venv_apptainer

  echo "Cleaning completed!"
  echo "Reinstalling python..."

  #python3 -m venv --without-pip $parent_folder/.venv_apptainer
  #source $parent_folder/.venv_apptainer/bin/activate
  #python3 get-pip.py
  # pip install pyyaml
fi

# Only run CMake if RUNCMAKE flag is set or if CLEAN was performed
if [ "$RUNCMAKE" == "1" ] || [ "$CLEAN" == "1" ]; then
  echo "Running CMake configuration..."

  cmake \
    -S"$source_name" \
    -B"$build_name" \
    -GNinja \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DPython3_EXECUTABLE="$PYTHON_EXEC" \
    -DLLVM_ENABLE_PROJECTS="clang;lld;" \
    -DLLVM_ENABLE_RUNTIMES="openmp;offload" \
    -DCLANG_VENDOR=gpulibc-build \
    -DLIBOMPTARGET_ENABLE_DEBUG=1 \
    -DLIBOMPTARGET_DLOPEN_PLUGINS='cuda' \
    -DRUNTIMES_nvptx64-nvidia-cuda_LLVM_ENABLE_RUNTIMES=libc \
    -DRUNTIMES_amdgcn-amd-amdhsa_LLVM_ENABLE_RUNTIMES=libc \
    -DLLVM_RUNTIME_TARGETS="default;amdgcn-amd-amdhsa;nvptx64-nvidia-cuda" \
    -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DLLVM_CCACHE_BUILD=ON \
    -DCMAKE_INSTALL_PREFIX=$install_name
fi

cmake --build "$build_name"

cmake --install "$build_name" --prefix "$install_name"
