#include "./engine.h"

// Implementing the "Variations" from the original flam3 paper (https://flam3.com/flame_draves.pdf - see Appendix)

// Calculation code:

// Coordinate variation(Coordinate c) {} - same header for each -> [(x, y) -> (x, y)]

Coordinate variation_identity(Coordinate c) {
    return c;
}

Coordinate variation_sinusoidal(Coordinate c) {
    return {sin(c.x), sin(c.y)};
}

Coordinate variation_spherical(Coordinate c) {
    // r = sqrt(x^2 + y^2) -- distance formula
    double r2 = (c.x*c.x) + (c.y*c.y); // or std::hypot(c.x, c.y);
    return {c.x / r2, c.y / r2};
}


// engine code

class Serial_Engine : public Engine {
    public:
        Serial_Engine() : Engine() {
            setup(1);
        };

        Coordinate getStartingCoordinate() {
            return {dist(rng), dist(rng), unitDist(rng)};
        }

        void setup(int numThreads, int seed = 0) {
            threadCoords.resize(numThreads);
            for(int i = 0; i < numThreads; i++) {
                threadCoords[i] = getStartingCoordinate();
            }
            setSeed(seed);
        }

        void start() {
            running = true;
            printf("Spawning worker...\r\n");
            workingThread = std::thread(&Serial_Engine::workerLoop, this);
            printf("Thread spawned -- running = %d\r\n", running);
        }

        void stop() {
            running = false;
            if(workingThread.joinable()) {
                workingThread.join();
            }
        }

        void reset() {
            stop();
            std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();

            std::chrono::duration<double> runLength = endTime - startTime;
            printf("Resetting...\r\n");
            printf("Run length was %.2f seconds.\r\nThis is an average of %f iterations/sec.", runLength.count(), totalHits[0] / runLength.count());
        }
        
        Coordinate stepNoPlot(Coordinate c) {
            int func = pickFunction();

            constexpr bool ultra_debug = false;
            if constexpr (ultra_debug) { // if true, constexpr makes this if() compile.  if false, it doesn't get added to the code
                static std::vector<int> funcHits(transforms.size());
                static std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
                funcHits.resize(transforms.size()); // resize to the number of transforms -- not necessarily unchanging in multithreading, so resize

                if (func >= 0 && func < (int)funcHits.size()) {
                    funcHits[func]++;
                }

                static uint64_t totalFuncHits = 0;
                totalFuncHits++;

                if(totalFuncHits % 20000000 == 0) {
                    // every 20m hits, print
                    printf("totalFuncHits = %ld\r\n", totalFuncHits);
                    // for(size_t i = 0; i < transforms.size(); i++) {
                    //     printf("Function %lu: %d\r\n", i, funcHits[i]);
                    // }
                    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
                    std::chrono::duration<double> timeSinceStart = now - startTime; 
                    
                    double iterationsPerSec = (double) totalFuncHits / timeSinceStart.count();
                    printf("Iterations per second: %f\r\n", iterationsPerSec);
                }
            }

            if(func < 0) { // there are no transforms selected but the user pressed > -- do nothing
                return c;
            }

            c.color = (c.color + transforms[func].color) / 2.0;
            c = calculate(func, c);

            if(hasFinalTransform) {
                // calculateFinalTransform()
            }

            return c;
        }

        void step(Coordinate c, int threadIndex) {
            threadCoords[threadIndex] = stepNoPlot(c);
            plot(threadCoords[threadIndex]); // plots the current point if within histogram
        };

        void plot(Coordinate pointToPlot) {
            int plotX = (int)((pointToPlot.x - viewport.minX) / (viewport.maxX - viewport.minX) * globalHistogram.width);
            int plotY = (int)((pointToPlot.y - viewport.minY) / (viewport.maxY - viewport.minY) * globalHistogram.height);

            // Optimization note: / (viewport.maxX - viewport.minX) * globalHistogram.width); -- all of these do not change.
            // The only variable that changes is pointToPlot x, y -- I could precompute 
            
            if(plotX >= 0 && plotX < globalHistogram.width && // if within bounds of histogram:
               plotY >= 0 && plotY < globalHistogram.height) {
                int index = plotY * globalHistogram.width + plotX;
                globalHistogram.data[index].hits++;
                globalHistogram.data[index].color += pointToPlot.color;
            }
        }

        std::vector<VariationDef> getSupportedVariations() { // updated by me, per engine, for frontend purposes
            std::vector<VariationDef> supportedVariations = {
                {0, "Identity", "x, y"},
                {1, "Sinusoidal", "sin(x), sin(y)"},
                {2, "Spherical", "x / r^2, y / r^2"}
            };

            return supportedVariations;
        }

    private:
        std::thread workingThread;
        std::mt19937_64 rng; // random number generator, set to 'seed' when run
        std::uniform_real_distribution<double> dist{-1.0, 1.0};   // for x, y starting point
        std::uniform_real_distribution<double> unitDist{0.0, 1.0}; // for color coordinates
        
        int pickFunction() {
            double i = unitDist(rng) * getTotalWeight(); // a number from 0 to totalWeight
            float cumulativeWeight = 0;
            for(size_t func = 0; func < transforms.size(); func++) { // [0, slice0), (slice0, slice1)..., (sliceN-1, totalWeight]
                cumulativeWeight += transforms[func].weight;
                if(i < cumulativeWeight) {
                    return func;
                }
            }
            
            //throw("unreachable state in theory");
            return (int)transforms.size() - 1; 
        };

        Coordinate calculate(int funcIndex, Coordinate coord) {
            Transform& t = transforms[funcIndex];

            // apply affine transform first
            Coordinate affine = {
                t.coeffs.a * coord.x + t.coeffs.b * coord.y + t.coeffs.c,
                t.coeffs.d * coord.x + t.coeffs.e * coord.y + t.coeffs.f 
            }; // (x, y) = (a*x+b*y+c), (d*x+e*y+f))
            // with identity matrix this maps to
            // (x, y) = (x, y)

            switch(funcIndex) {
                case 0: // Identity
                    affine = variation_identity(affine);
                    break;
                case 1: // Sinusoidal
                    affine = variation_sinusoidal(affine);
                    break;

                case 2: // Spherical
                    affine = variation_spherical(affine);
                    break;

                default:
                    affine = affine;
            }

            affine.color = (coord.color + t.color) / 2.0;

            return affine;
        }

        void workerLoop() {
            printf("Worker loop started.\r\n");
            Coordinate* currentThreadCoordinate = &threadCoords[0];

            for(int i = 0; i < 20; i++) {
                *currentThreadCoordinate = stepNoPlot(*currentThreadCoordinate);
            }

            while(running) {
                this->step(*currentThreadCoordinate, 0);
            }
            printf("Worker loop ended.\r\n");
        }
};


std::unique_ptr<Engine> createSerialEngine() {
    return std::make_unique<Serial_Engine>();
}


