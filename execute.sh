#!/bin/bash

#nvcc --compiler-options -fopenmp ./fractal_flame.cu -o ./fractal_flame (for reference)

set -e

#  ONLY run the slow configuration step if the build folder doesn't exist or has been updated
if [[ ! -d "build" || "CMakeLists.txt" -nt "build/CMakeCache.txt" ]]; then
    echo "First-time setup: Configuring project and pulling dependencies..."
    cmake -B build -DCMAKE_BUILD_TYPE=Release
else
    echo "All good -- skipping dependency configuration..."
fi

echo "Compiling modified code..."

# This command automatically figures out what changed and builds quickly
cmake --build build --config Release

echo "Launching application..."
LIBGL_ALWAYS_SOFTWARE=1 ./build/fractal_flame
