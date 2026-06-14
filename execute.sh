#!/bin/bash

#nvcc --compiler-options -fopenmp ./fractal_flame.cu -o ./fractal_flame

set -e

# 1. ONLY run the slow configuration step if the build folder doesn't exist
if [[ ! -d "build" || "CMakeLists.txt" -nt "build/CMakeCache.txt" ]]; then
    echo "First-time setup: Configuring project and pulling dependencies..."
    cmake -B build -DCMAKE_BUILD_TYPE=Release
else
    echo "All good -- skipping dependency configuration..."
fi

echo "Compiling modified code..."
# 2. This command automatically figures out what changed and builds instantly!
cmake --build build --config Release

echo "Launching application..."
LIBGL_ALWAYS_SOFTWARE=1 ./build/fractal_flame
