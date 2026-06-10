#!/bin/bash

nvcc --compiler-options -fopenmp ./fractal_flame.cu -o ./fractal_flame

./fractal_flame -T 8 -R 1920x1080
