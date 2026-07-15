#include "./engines/openmp_engine.cpp"
#include "./engines/cuda_engine.cuh"

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

    

    std::string toCSVRow() {
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
           << r2 << "\n";
        return ss.str();
    }

    static std::string csvHeader() {
        return "engine,seed,threads,wall_time_sec,total_iterations,iter_per_sec,num_transforms,r,r2\n";
    }
};

void runEngine(Engine& engine, uint64_t targetIterations) {
    engine.setTargetIterations(targetIterations);
    engine.start();
    while(!engine.done()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    engine.stop();
}



int main() {
    testCUDA();
    std::vector<int> seeds = {1687931058, -439194709, 1989290325, -219179936}; // your known seeds
    std::vector<int> threadCounts = {2, 4, 6, 8, 10, 12, 14, 16};
    uint64_t targetIterations = 200'000'000; // 200M

    printf("seed,threads,wall_time_sec,total_iterations,iter_per_sec\n");

    time_t now = time(0);
    char filename[64];
    strftime(filename, sizeof(filename), "results_%Y%m%d_%H%M%S.csv", localtime(&now)); 
    FILE* csvFile = fopen("./results/benchmark_results.csv", "w");
    if(!csvFile) {
        printf("Failed to open CSV file for writing!\r\n");
        return 1;
    }

    std::string header = BenchmarkResult::csvHeader(); // static
    fprintf(csvFile, "%s", header.c_str());
    printf("%s", header.c_str());
    
    for(int seed : seeds) {
        // Serial baseline
        Serial_Engine serialEngine;
        serialEngine.randomize(seed);
        
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now(); 
        runEngine(serialEngine, targetIterations);
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

        Viewport sharedViewport = serialEngine.getViewport();
        
        std::vector<uint64_t> serialHits(serialEngine.getHistogram().getSize());
        const Histogram<PixelData>& serialRunData = serialEngine.getHistogram(); // reference variable for correlation test
        for(size_t i = 0; i < serialHits.size(); i++) {
            serialHits[i] = serialRunData[i].hits;
        }

        BenchmarkResult serialResult(seed, start, end, serialEngine); // r and r2 are -1; serial is the base case, so it can't correlate (maybe run it twice in the future to see if it correlates to itself?)
        std::string serialRow = serialResult.toCSVRow();
        printf("%s", serialRow.c_str());
        fprintf(csvFile, "%s", serialRow.c_str());
        fflush(csvFile);

        // printf("%s,%d,1,%.3f,%lu,%.1f,%zu,%.4f\n", 
        //     "Serial", seed, wallTime, totalIter, totalIter / wallTime, engine.getTransforms().size());
        // fprintf(csvFile, "%s,%d,1,%.3f,%lu,%.1f,%zu\n",
        //     "Serial", seed, wallTime, totalIter, totalIter / wallTime, engine.getTransforms().size());
        // fflush(csvFile);

        for(int threads : threadCounts) {
            OpenMP_Engine OMPEngine(threads);
            OMPEngine.randomize(seed);
            OMPEngine.setViewport(sharedViewport); // data doesn't correlate if they have different viewports
            
            // printf("Serial viewport: minX=%.3f maxX=%.3f minY=%.3f maxY=%.3f\n",
            //     serialEngine.getViewport().minX, serialEngine.getViewport().maxX,
            //     serialEngine.getViewport().minY, serialEngine.getViewport().maxY);
            
            // printf("OMP viewport: minX=%.3f maxX=%.3f minY=%.3f maxY=%.3f\n",
            //     OMPEngine.getViewport().minX, OMPEngine.getViewport().maxX,
            //     OMPEngine.getViewport().minY, OMPEngine.getViewport().maxY);
            
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            runEngine(OMPEngine, targetIterations);            
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

            BenchmarkResult result("OpenMP", seed, threads, start, end, OMPEngine, serialHits);
            std::string row = result.toCSVRow();
            printf("%s", row.c_str());
            fprintf(csvFile, "%s", row.c_str());

            // printf("%s,%d,%d,%.3f,%lu,%.1f,%zu,%.4f\n", 
            //     "OpenMP", seed, threads, wallTime, totalIter, totalIter / wallTime, engine.getTransforms().size(), r);


            // fprintf(csvFile, "%s,%d,%d,%.3f,%lu,%.1f,%zu,%.4f\n",
            //     "OpenMP", seed, threads, wallTime, totalIter, totalIter / wallTime, engine.getTransforms().size(), r);
            fflush(csvFile);

            
        }
    }
    fclose(csvFile);
    return 0;
}