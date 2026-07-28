#include "./cuda_engine.cuh"
#include "./variations/standard.h"


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
    
    // get free and total memory on GPU
    size_t freeMem, totalMem;
    cudaMemGetInfo(&freeMem, &totalMem);
    logLevel(LOG_LIFECYCLE, "Pre-malloc:  CUDA free: %zu MB / %zu MB total\n", freeMem / (1024*1024), totalMem / (1024*1024));

    // allocate GPU histogram
    cudaMalloc(&d_histHits,   histSize * sizeof(uint64_t));
    cudaMalloc(&d_histColors, histSize * sizeof(float));
    
    // allocate GPU transforms
    cudaMalloc(&d_transforms, transforms.size() * sizeof(Transform));
    
    // allocate GPU RNG seeds
    cudaMalloc(&d_seeds, numThreads * sizeof(uint64_t));
    
    // zero out histogram
    cudaMemset(d_histHits,   0, histSize * sizeof(uint64_t));
    cudaMemset(d_histColors, 0, histSize * sizeof(float));
    
    // copy transforms to GPU
    cudaMemcpy(d_transforms, transforms.data(), 
               transforms.size() * sizeof(Transform), 
               cudaMemcpyHostToDevice);
    
    cudaMemGetInfo(&freeMem, &totalMem);
    logLevel(LOG_LIFECYCLE, "Post-malloc: CUDA free: %zu MB / %zu MB total\n", freeMem / (1024*1024), totalMem / (1024*1024));

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

__device__ int pickFunction(Transform* transforms, int numTransforms, float totalWeight, Xorshift64& rng) {
    double i = rng.getDouble() * totalWeight;
    float cumulative = 0;
    for(int f = 0; f < numTransforms; f++) {
        cumulative += transforms[f].weight;
        if(i < cumulative) return f;
    }
    return numTransforms - 1;
}

__device__ Coordinate calculate(Transform& t, Coordinate coord, Xorshift64& rng) {
    Coordinate affine = {
        t.coeffs.a * coord.x + t.coeffs.b * coord.y + t.coeffs.c,
        t.coeffs.d * coord.x + t.coeffs.e * coord.y + t.coeffs.f
    };
    
    switch(t.variation.index) {
        case 0: // Identity
            affine = variation_identity(affine);
            break;
        case 1: // Sinusoidal
            affine = variation_sinusoidal(affine);
            break;
        case 2: // Spherical
            affine = variation_spherical(affine);
            break;
        case 3: // Swirl
            affine = variation_swirl(affine);
            break;
        case 4: // Horseshoe
            affine = variation_horseshoe(affine);
            break;
        case 5: // Polar
            affine = variation_polar(affine);
            break;
        case 6: // Handkerchief
            affine = variation_handkerchief(affine);
            break;
        case 7: // Heart
            affine = variation_heart(affine);
            break;
        case 8: // Disc
            affine = variation_disc(affine);
            break;
        case 9: // Spiral
            affine = variation_spiral(affine);
            break;
        case 10: // Hyperbolic
            affine = variation_hyperbolic(affine);
            break;
        case 11: // Diamond
            affine = variation_diamond(affine);
            break;
        case 12: // Ex
            affine = variation_ex(affine);
            break;
        case 13: // Julia
            affine = variation_julia(affine, &rng);
            break;
        case 14: // Bent
            affine = variation_bent(affine);
            break;
        case 15: // Waves
            affine = variation_waves(affine, &t.coeffs);
            break;
        case 16: // Fisheye
            affine = variation_fisheye(affine);
            break;
        case 17: // Popcorn
            affine = variation_popcorn(affine, &t.coeffs);
            break;
        case 18: // Exponential
            affine = variation_exponential(affine);
            break;
        case 19: // Power
            affine = variation_power(affine);
            break;
        case 20: // Cosine
            affine = variation_cosine(affine);
            break;
        case 21: // Rings
            affine = variation_rings(affine, &t.coeffs);
            break;
        case 22: // Fan
            affine = variation_fan(affine, &t.coeffs);
            break;
        case 23: // Blob
            affine = variation_blob(affine, &t.parametric);
            break;
        case 24: // PDJ
            affine = variation_pdj(affine, &t.parametric);
            break;
        case 25: // Fan2
            affine = variation_fan2(affine, &t.parametric);
            break;
        case 26: // Rings2
            affine = variation_rings2(affine, &t.parametric);
            break;
        case 27: // Eyefish
            affine = variation_eyefish(affine);
            break;
        case 28: // Bubble
            affine = variation_bubble(affine);
            break;
        case 29: // Cylinder
            affine = variation_cylinder(affine);
            break;
        case 30: // Perspective
            affine = variation_perspective(affine, &t.parametric);
            break;
        case 31: // Noise
            affine = variation_noise(affine, &rng);
            break;
        case 32: // JuliaN
            affine = variation_julian(affine, &t.parametric, &rng);
            break;
        case 33: // JuliaScope
            affine = variation_juliascope(affine, &t.parametric, &rng);
            break;
        case 34: // Blur
            affine = variation_blur(&rng);
            break;
        case 35: // Gaussian
            affine = variation_gaussian(&rng);
            break;
        case 36: // RadialBlur
            affine = variation_radialblur(affine, &t.parametric, &rng);
            break;
        case 37: // Pie
            affine = variation_pie(&t.parametric, &rng);
            break;
        case 38: // Ngon
            affine = variation_ngon(affine, &t.parametric);
            break;
        case 39: // Curl
            affine = variation_curl(affine, &t.parametric);
            break;
        case 40: // Rectangles
            affine = variation_rectangles(affine, &t.parametric);
            break;
        case 41: // Arch
            affine = variation_arch(&rng);
            break;
        case 42: // Tangent
            affine = variation_tangent(affine);
            break;
        case 43: // Square
            affine = variation_square(&rng);
            break;
        case 44: // Rays
            affine = variation_rays(affine, &rng);
            break;
        case 45: // Blade
            affine = variation_blade(affine, &rng);
            break;
        case 46: // Secant
            affine = variation_secant(affine);
            break;
        case 47: // Twintrian
            affine = variation_twintrian(affine, &rng);
            break;
        case 48: // Cross
            affine = variation_cross(affine);
            break;
        default:
            break;
    }
        
    affine.color = (coord.color + t.color) / 2.0;
    return affine;
}

void CUDA_Engine::step(Coordinate c, int threadIndex, Xorshift64 &rng) {
    return; // is not called -- CPU-specific (engine.h needs refactored I think)
}
Coordinate CUDA_Engine::stepNoPlot(Coordinate c, Xorshift64 &rng) { 
    return c; // same -- is not called
}

__device__ Coordinate getStartingCoordinate(Xorshift64& rng) {
    double x = 1 - 2 * rng.getDouble();
    double y = 1 - 2 * rng.getDouble();
    double color = rng.getDouble();
    return {x, y, color};
}

__global__ void renderKernel(
    Transform* transforms, int numTransforms, float totalWeight,
    uint64_t* d_histHits, float* d_histColors,
    int histWidth, int histHeight,
    Viewport viewport,
    uint64_t* d_seeds,
    uint64_t iterationsPerThread
) {
    int threadId = blockIdx.x * blockDim.x + threadIdx.x;
    Xorshift64 rng(d_seeds[threadId]);

    Coordinate coord = getStartingCoordinate(rng);

    for (uint64_t i = 0; i < iterationsPerThread; i++) {
        int f = pickFunction(transforms, numTransforms, totalWeight, rng);
        coord = calculate(transforms[f], coord, rng);

        // match CPU stepNoPlot: escaping trajectories reset to a known-good point
        if (!isfinite(coord.x) || !isfinite(coord.y)) {
            coord.x = 1.0 - 2.0 * rng.getDouble();
            coord.y = 1.0 - 2.0 * rng.getDouble();
            coord.color = rng.getDouble();
        }

        if (i < 20) continue; // warmup, matches CPU engines' localPlot skip

        double nx = (coord.x - viewport.minX) / (viewport.maxX - viewport.minX);
        double ny = 1.0 - (coord.y - viewport.minY) / (viewport.maxY - viewport.minY); // Y-flip, matches CPU plot()

        int plotX = (int)(nx * histWidth);
        int plotY = (int)(ny * histHeight);

        if (plotX >= 0 && plotX < histWidth && plotY >= 0 && plotY < histHeight) {
            int index = plotY * histWidth + plotX;
            atomicAdd(reinterpret_cast<unsigned long long*>(&d_histHits[index]), 1ULL);
            atomicAdd(&d_histColors[index], (float)coord.color);
        }
    }
}


bool CUDA_Engine::done() {
    return !running;
}

void CUDA_Engine::start() {
    running = true;

    uint64_t iterationsPerThread = (targetIterations + numThreads - 1) / numThreads;
    int blocks = (numThreads + threadsPerBlock - 1) / threadsPerBlock; // round up

    logLevel(LOG_LIFECYCLE, "CUDA kernel spawning with %d blocks and %d threads per block...\r\n", blocks, threadsPerBlock);

    renderKernel<<<blocks, threadsPerBlock>>>(
        d_transforms, (int)transforms.size(), getTotalWeight(),
        d_histHits, d_histColors,
        globalHistogram.getWidth(), globalHistogram.getHeight(),
        viewport,
        d_seeds,
        iterationsPerThread
    );

    cudaError_t err = cudaDeviceSynchronize(); // blocks until kernel finishes

    logLevel(LOG_LIFECYCLE, "Kernel finished with error string: %s\r\n", cudaGetErrorString(err));

    if(err != cudaSuccess) {
        logLevel(LOG_SILENT, "CUDA kernel error: %s\r\n", cudaGetErrorString(err));
    }
    

    // copy device histogram back to host for benchmark/display to read
    size_t histSize = globalHistogram.getSize();
    std::vector<uint64_t> hostHits(histSize);
    std::vector<float> hostColors(histSize);
    cudaMemcpy(hostHits.data(), d_histHits, histSize * sizeof(uint64_t), cudaMemcpyDeviceToHost);
    cudaMemcpy(hostColors.data(), d_histColors, histSize * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < histSize; i++) {
        globalHistogram.data[i].hits = hostHits[i];
        globalHistogram.data[i].color = hostColors[i];
    }

    totalIterationsCompleted = iterationsPerThread * numThreads;
    running = false;
}
void CUDA_Engine::stop() {
    // start() already blocks until the kernel finishes and results are copied back,
    // so there's nothing left to stop. Kept as a noop to satisfy virtual Engine function
    // and runEngine()'s call pattern in benchmark.cpp.
}
void CUDA_Engine::reset() {

}
uint64_t CUDA_Engine::getTotalIterations() { 
    return totalIterationsCompleted;
}
uint64_t CUDA_Engine::getMaxHits() { 
    if (globalHistogram.data.empty()) return 0;
    PixelData maxHistogramBin = *std::max_element(globalHistogram.data.begin(), globalHistogram.data.end());
    return maxHistogramBin.hits;
}

__global__  void helloKernel() {
    // logLevel(LOG_CHATTER, "Hello from GPU thread %d/%d!\n", threadIdx.x, blockDim.x); <- Variadics other than printf are not allowed in kernel code!
    if constexpr (LOG_LEVEL == LOG_CHATTER) {
        printf("Hello from GPU thread %d/%d!\n", threadIdx.x, blockDim.x);
    }
}

void testCUDA() {
    cudaError_t initErr = cudaFree(0); // forces CUDA context initialization
    logLevel(LOG_LIFECYCLE, "Init error: %s\n", cudaGetErrorString(initErr));
    
    int device;
    cudaGetDevice(&device);
    logLevel(LOG_LIFECYCLE, "Current device: %d\n", device);
    
    size_t free, total;
    cudaMemGetInfo(&free, &total);
    logLevel(LOG_LIFECYCLE, "Free memory: %zu MB, Total: %zu MB\n", free/(1024*1024), total/(1024*1024));

    helloKernel<<<1, 8>>>();
    cudaError_t err = cudaDeviceSynchronize();
    if(err != cudaSuccess) {
        logLevel(LOG_SILENT, "CUDA error: %s\n", cudaGetErrorString(err));
        exit(-1);
    } else {
        logLevel(LOG_LIFECYCLE, "CUDA works!\n");
    }
}