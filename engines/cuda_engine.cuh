#pragma once
#include "./engine.h"
#include <cuda_runtime.h>

class CUDA_Engine : public Engine {
public:
    uint64_t* d_histHits = nullptr;    // GPU histogram hits
    double* d_histColors = nullptr;    // GPU histogram colors
    Transform* d_transforms = nullptr; // GPU transforms
    uint64_t* d_seeds = nullptr;       // GPU RNG seeds per thread
    std::thread workingThread;
    std::atomic<bool> running = false;
    int numThreads;




    /// @brief Default constructor - 1024 threads by default
    CUDA_Engine();
    
    /// @brief Default destructor
    ~CUDA_Engine();

    /// @brief Sets up CUDA kernels, allocating memory and copying arrays over to CUDA
    /// @param desiredThreads 1024 threads by default (RTX 4070 Super has )
    void setup(int desiredThreads) override; 

    /// @brief Not used in CUDA engine.
    /// @param c 
    /// @param threadIndex 
    /// @param rng 
    void step(Coordinate c, int threadIndex, Xorshift64 &rng) override;

    /// @brief Not used in CUDA engine.
    /// @param c 
    /// @param rng 
    /// @return 
    Coordinate stepNoPlot(Coordinate c, Xorshift64 &rng) override;
    
    /// @brief Returns a vector that corellates to variations in engine.h
    /// @return 
    std::vector<VariationDef> getSupportedVariations() override;
    
    /// @brief 
    /// @param threadIndex 
    /// @return 
    Coordinate getStartingCoordinate(int threadIndex) override;

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