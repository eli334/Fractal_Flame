#pragma once
#include "./engine.h"
#include <cuda_runtime.h>
#include <thread>

class CUDA_Engine : public Engine {
public:
    uint64_t* d_histHits = nullptr;    // GPU histogram hits
    float* d_histColors = nullptr;    // GPU histogram colors
    Transform* d_transforms = nullptr; // GPU transforms
    uint64_t* d_seeds = nullptr;       // GPU RNG seeds per thread
    int numThreads = 1024;
    int threadsPerBlock = 256; // tweakable launch config
    uint64_t totalIterationsCompleted = 0; // set by start() after kernel finishes

    std::thread workingThread;



    /// @brief Default constructor - 1024 threads by default
    CUDA_Engine();
    
    /// @brief Default destructor
    ~CUDA_Engine();

    /// @brief Sets up CUDA kernels, allocating memory and copying arrays over to CUDA
    /// @param desiredThreads 1024 threads by default
    void setup(int desiredThreads) override; 

    /// @brief Not used in CUDA engine.
    /// @param c 
    /// @param threadIndex 
    /// @param rng 
    void step(Coordinate c, int threadIndex, Xorshift64 &rng) ;

    /// @brief Not used in CUDA engine.
    /// @param c 
    /// @param rng 
    /// @return 
    Coordinate stepNoPlot(Coordinate c, Xorshift64 &rng) override;
    
    /// @brief Returns a vector that corellates to variations in engine.h
    /// @return 
    std::vector<VariationDef> getSupportedVariations() override;
    
    bool done() override;

    /// @brief 
    void start();

    /// @brief 
    void stop();

    /// @brief 
    void reset();

    /// @brief 
    /// @return 
    uint64_t getTotalIterations() override;

    /// @brief 
    /// @return 
    uint64_t getMaxHits() override;
};

void testCUDA();