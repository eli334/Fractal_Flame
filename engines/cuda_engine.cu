#include "./cuda_engine.cuh"
#include <cuda_runtime.h>
#include <stdio.h>
#include "cuda_engine.cuh"

__global__ void helloKernel() {
    printf("Hello from GPU thread %d/%d!\n", threadIdx.x, blockDim.x);
}

CUDA_Engine::CUDA_Engine() {
    
}

CUDA_Engine::~CUDA_Engine() {
    if(d_histHits) cudaFree(d_histHits);
    if(d_histColors) cudaFree(d_histColors);
    if(d_transforms) cudaFree(d_transforms);
    if(d_seeds) cudaFree(d_seeds);
}

void CUDA_Engine::setup(int desiredThreads) {
    numThreads = desiredThreads;
    size_t histSize = globalHistogram.getSize();
    
    // free any existing GPU memory
    if(d_histHits)    cudaFree(d_histHits);
    if(d_histColors)  cudaFree(d_histColors);
    if(d_transforms)  cudaFree(d_transforms);
    if(d_seeds)       cudaFree(d_seeds);
    
    // allocate GPU histogram
    cudaMalloc(&d_histHits,   histSize * sizeof(uint64_t));
    cudaMalloc(&d_histColors, histSize * sizeof(double));
    
    // allocate GPU transforms
    cudaMalloc(&d_transforms, transforms.size() * sizeof(Transform));
    
    // allocate GPU RNG seeds
    cudaMalloc(&d_seeds, numThreads * sizeof(uint64_t));
    
    // zero out histogram
    cudaMemset(d_histHits,   0, histSize * sizeof(uint64_t));
    cudaMemset(d_histColors, 0, histSize * sizeof(double));
    
    // copy transforms to GPU
    cudaMemcpy(d_transforms, transforms.data(), 
               transforms.size() * sizeof(Transform), 
               cudaMemcpyHostToDevice);
    
    // seed RNGs -- one per GPU thread
    std::vector<uint64_t> seeds(numThreads);
    std::random_device rd;
    uint64_t baseSeed = ((uint64_t)rd() << 32) | rd();
    for(int i = 0; i < numThreads; i++) {
        seeds[i] = baseSeed ^ (i * 2654435761ULL);
    }

    cudaMemcpy(d_seeds, seeds.data(), 
               numThreads * sizeof(uint64_t), 
               cudaMemcpyHostToDevice);
}    


void CUDA_Engine::step(Coordinate c, int threadIndex, Xorshift64 &rng) {
    return; // is not called -- CPU-specific (engine.h needs refactored I think)
}
Coordinate CUDA_Engine::stepNoPlot(Coordinate c, Xorshift64 &rng) { 
    return c; // same -- is not called
}
std::vector<VariationDef> CUDA_Engine::getSupportedVariations()  { 
    return {}; 
}
Coordinate CUDA_Engine::getStartingCoordinate(int threadIndex) { 
    return {}; 
}
void CUDA_Engine::start() {

}
void CUDA_Engine::stop() {

}
void CUDA_Engine::reset() {

}
uint64_t CUDA_Engine::getTotalIterations() { 
    return 0;
}
uint64_t CUDA_Engine::getMaxHits() { 
    return 0;
}







void testCUDA() {
    cudaError_t initErr = cudaFree(0); // forces CUDA context initialization
    printf("Init error: %s\n", cudaGetErrorString(initErr));
    
    int device;
    cudaGetDevice(&device);
    printf("Current device: %d\n", device);
    
    size_t free, total;
    cudaMemGetInfo(&free, &total);
    printf("Free memory: %zu MB, Total: %zu MB\n", free/(1024*1024), total/(1024*1024));

    helloKernel<<<1, 8>>>();
    cudaError_t err = cudaDeviceSynchronize();
    if(err != cudaSuccess) {
        printf("CUDA error: %s\n", cudaGetErrorString(err));
    } else {
        printf("CUDA works!\n");
    }
}