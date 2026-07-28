#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <iostream>
#include <fstream>
#include <filesystem>
#include "./include/stb_image_write.h"
#include "./engines/openmp_engine.cpp"
#include "./engines/cuda_engine.cuh"


namespace fs = std::filesystem; // lets me do fs:: as a common alias

void saveFlamePNG(const std::string& filename, Engine& engine, const ColorState& color, Camera frame) {
    int width  = engine.getHistogram().getWidth();
    int height = engine.getHistogram().getHeight();
    std::vector<uint8_t> pixels(width * height * 4);
    engine.fillPixelBuffer(pixels.data(), color.palette.get(), color.numColors, color.gamma, frame);
    stbi_write_png(filename.c_str(), width, height, 4, pixels.data(), width * 4);
}

struct BenchmarkResult {
    std::string engine;
    int seed;
    int threads;
    double wallTimeSec;
    uint64_t totalIterations;
    double iterPerSec;
    size_t numTransforms;
    double r;
    double r2;

    // serial constructor — no correlation
    BenchmarkResult(
        int seed,
        std::chrono::steady_clock::time_point start,
        std::chrono::steady_clock::time_point end,
        Engine& engine
    ) {
        this->engine = "Serial";
        this->seed = seed;
        this->threads = 1;
        this->wallTimeSec = std::chrono::duration<double>(end - start).count();
        this->totalIterations = engine.getTotalIterations();
        this->iterPerSec = totalIterations / wallTimeSec;
        this->numTransforms = engine.getTransforms().size();
        r = -1.0;
        r2 = -1.0;
    }

    // Non-serial constructor; run r and r^2 test
    BenchmarkResult( 
        const std::string& engineName,
        int seed,
        int threads,
        std::chrono::steady_clock::time_point start,
        std::chrono::steady_clock::time_point end,
        Engine& engine,
        const std::vector<uint64_t>& serialHits
    ) {
        this->engine = engineName; // OpenMP / CUDA
        this->seed = seed;
        this->threads = threads;
        this->wallTimeSec = std::chrono::duration<double>(end - start).count();
        this->totalIterations = engine.getTotalIterations();
        this->iterPerSec = totalIterations / wallTimeSec;
        this->numTransforms = engine.getTransforms().size();

        const Histogram<PixelData>& hist = engine.getHistogram();
        double sumX = 0, sumY = 0, sumXY = 0, sumXSquared = 0, sumYSquared = 0;
        int n = 1000 * 1000;
        
        // Pearson correlation -- 1 is a perfect positive correlation; just using this value for comparison
        // Mostly for making sure the implementation between serial and OpenMP / CUDA have the same seed interpretations
        // If the same seeds had different results on different engines then they couldn't really be compared fairly
        
        for(int i = 0; i < n; i++) {
            double x = serialHits[i];
            double y = hist[i].hits;
            sumX += x; sumY += y;
            sumXY += x * y;
            sumXSquared += x * x;
            sumYSquared += y * y;
        }
        double denominator = sqrt((n * sumXSquared - sumX * sumX) * (n * sumYSquared - sumY * sumY));
        if(denominator < 1e-8) {
            r = 0;
        } else {
            r = (n * sumXY - sumX * sumY) / denominator;
        }
        r2 = r * r;
    }

    

    std::string toCSVRow() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3);
        ss << engine << ","
           << seed << ","
           << threads << ","
           << wallTimeSec << ","
           << totalIterations << ","
           << std::setprecision(1) << iterPerSec << ","
           << numTransforms << ","
           << std::setprecision(4) << r << ","
           << r2;
        return ss.str();
    }

    static std::string csvHeader() {
        return "engine,seed,threads,wall_time_sec,total_iterations,iter_per_sec,num_transforms,r,r2";
    }
};

struct BenchmarkRunner {
    uint64_t serialIterations = 0, CUDAIterations = 0;
    time_t runTime;
    ColorState color;
    Camera frame; // frame for rendering png
    std::string timestamp; // used for CSV
    std::ofstream csvFile;


    // needed for runOpenMP/runCUDA -- obtained from runSerial
    std::vector<uint64_t> serialHits;
    Viewport sharedViewport;

    BenchmarkRunner(uint64_t serialIterations, uint64_t CUDAIterations, const time_t runTime);

    void runSerial(int seed);
    void runOpenMP(int threads, int seed);
    void runCUDA(int totalThreads, int seed);

    // (3) helpers — replace the reused imageFilename buffer + centralize CSV/printf
    std::string imagePath(const std::string& engineName, int seed, int threads) const;
    void writeResult(const BenchmarkResult& result);
};

BenchmarkRunner::BenchmarkRunner(uint64_t serialIterations, uint64_t CUDAIterations, const time_t runTime) {
    this->serialIterations = serialIterations;
    this->CUDAIterations = CUDAIterations;

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&runTime));
    
    fs::create_directories("results/images");
    fs::path csvPath = fs::path("results") / ("results_" + std::string(timestamp) + ".csv");
    // old, legacy: csvFilename.append("./results/results_").append(timestamp).append(".csv");
    csvFile.open(csvPath);
    csvFile << BenchmarkResult::csvHeader() << std::endl;
}

// runEngine is shared between runSerial, runOpenMP, and runCUDA.

void runEngine(Engine& engine, uint64_t targetIterations) {
    engine.setTargetIterations(targetIterations);
    logLevel(LOG_CHATTER, "Engine starting with %d iterations.\r\n");
    engine.start();
    while(!engine.done()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    logLevel(LOG_CHATTER, "Engine stopping.\r\n");
    engine.stop();
    logLevel(LOG_CHATTER, "Engine stopped.\r\n");
}

void BenchmarkRunner::runSerial(int seed) {
    Serial_Engine serialEngine;
    serialEngine.randomize(seed);
    
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now(); 
    runEngine(serialEngine, serialIterations);
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    sharedViewport = serialEngine.getViewport();

    const Histogram<PixelData>& serialRunData = serialEngine.getHistogram(); // reference variable for correlation test
    
    serialHits.resize(serialRunData.getSize());

    for(size_t i = 0; i < serialHits.size(); i++) {
        serialHits[i] = serialRunData[i].hits;
    }

    BenchmarkResult result(seed, start, end, serialEngine); // r and r2 are -1; serial is the base case, so it can't correlate (maybe run it twice in the future to see if it correlates to itself?)

    writeResult(result);
    
    
    // color.randomizeColors(serialEngine.getTransforms().size(), seed);
    
    // imageFilename.append("./results/images/serial-").append(std::to_string(seed)).append("-").append(timestamp).append(".png");
    // saveFlamePNG(imageFilename.c_str(), serialEngine, color, frame);
    // imageFilename.clear();
}

void BenchmarkRunner::runOpenMP(int threads, int seed)
{
    OpenMP_Engine OMPEngine(threads);
    OMPEngine.randomize(seed);
    OMPEngine.setViewport(sharedViewport); // data doesn't correlate if they have different viewports
    
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    runEngine(OMPEngine, serialIterations);            
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    BenchmarkResult result("OpenMP", seed, threads, start, end, OMPEngine, serialHits);
    
    writeResult(result);

    // imageFilename.append("./results/images/serial-").append(std::to_string(seed)).append("-").append(timestamp).append(".png");
    // saveFlamePNG(imageFilename.c_str(), serialEngine, color, frame);
    // imageFilename.clear();
}

void BenchmarkRunner::runCUDA(int totalThreads, int seed) {
    CUDA_Engine cudaEngine;
    cudaEngine.randomize(seed);
    cudaEngine.setup(totalThreads);
    cudaEngine.setViewport(sharedViewport);

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    runEngine(cudaEngine, CUDAIterations);
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    BenchmarkResult result("CUDA", seed, totalThreads, start, end, cudaEngine, serialHits);
    
    writeResult(result);

    // imageFilename.append("./results/images/cuda-").append(std::to_string(totalThreads)).append("-").append(std::to_string(seed)).append("-").append(timestamp).append(".png");
    // saveFlamePNG(imageFilename.c_str(), cudaEngine, color, frame);
    // imageFilename.clear();
}

std::string BenchmarkRunner::imagePath(const std::string &engineName, int seed, int threads) const {
    return std::string();
}

void BenchmarkRunner::writeResult(const BenchmarkResult &result) {
    std::string row = result.toCSVRow();
    logLevel(LOG_SUMMARY, "%s\r\n", row.c_str());
    csvFile << row << std::endl;
}





// main program loop

int main() {
    testCUDA(); // this requires CUDA! crashes if you don't have it for benchmark.cpp -- it fails safely in the frontend code
    std::vector<int> seeds = {1687931058, -439194709, 1989290325, -219179936}; // Known seeds
    std::vector<int> threadCounts = {2, 4, 6, 8, 10, 12, 14, 16};           // OpenMP
    int cudaThreadCounts[] = {1024, 4096, 16384, 43008, 65536, 86016}; // CUDA

    uint64_t serialTarget = 100'000'000; // 100M for serial
    uint64_t CUDATarget = 5'000'000'000; // 5B for CUDA because it's so much better, it's nuts

    time_t now = time(0);
    BenchmarkRunner runner(serialTarget, CUDATarget, now);    
    
    for(int seed : seeds) {
        // Serial baseline is always required
        logLevel(LOG_SUMMARY, "Running serial run for seed %d with %d iterations.\r\n", seed, serialTarget);
        runner.runSerial(seed);

        // for(int threads : threadCounts) {
        //     logLevel(LOG_SUMMARY, "Running OpenMP run for seed %d, with %d iterations, with %d threads.\r\n", seed, serialTarget, threads);
        //     runner.runOpenMP(threads, seed);
        // }

        for(int threads : cudaThreadCounts) {
            logLevel(LOG_SUMMARY, "Running CUDA run for seed %d with %d iterations, with %d threads.\r\n", seed, CUDATarget, threads);
            runner.runCUDA(threads, seed);
        }
    }
    return 0;
}