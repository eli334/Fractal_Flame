#!/bin/bash

#nvcc --compiler-options -fopenmp ./fractal_flame.cu -o ./fractal_flame

set -e

# 1. ONLY run the slow configuration step if the build folder doesn't exist
if [ ! -d "build" ]; then
    echo "First-time setup: Configuring project and pulling dependencies..."
    cmake -B build -DCMAKE_BUILD_TYPE=Release
else
    echo "Build folder found. Skipping dependency configuration..."
fi

echo "Compiling modified code..."
# 2. This command automatically figures out what changed and builds instantly!
cmake --build build --config Release

echo "Launching application..."
./build/fractal_flame --gui
