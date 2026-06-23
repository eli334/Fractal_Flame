#!/bin/bash

#nvcc --compiler-options -fopenmp ./fractal_flame.cu -o ./fractal_flame (for reference)

clear
set -e

BUILD_TYPE=${1:-Debug}  # default to Debug, override with first argument

if [[ ! -d "build" || "CMakeLists.txt" -nt "build/CMakeCache.txt" ]]; then
    echo "Configuring project ($BUILD_TYPE)..."
    cmake -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE
else
    echo "Skipping configuration..."
fi

echo "Compiling modified code..."

# This command automatically figures out what changed and builds quickly
cmake --build build --config $BUILD_TYPE

echo "Launching application..."
LIBGL_ALWAYS_SOFTWARE=1 ./build/fractal_flame
