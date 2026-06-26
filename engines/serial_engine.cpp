#include "./engine.h"

// Implementing the "Variations" from the original flam3 paper (https://flam3.com/flame_draves.pdf - see Appendix)

/* Calculation code:
    Coordinate variation(Coordinate c) {} - same header for each -> [(x, y) -> (x, y)]
    other variables:
    r = sqrt(x^2 + y^2) -- r^2 = x^2 + y^2 -- std::hypot() because this is a hypotenuse
    theta = arctan(x / y)
    phi = arctan(y / x)
*/


// engine code

class Serial_Engine : public Engine {
    private:
        std::vector<uint64_t> iterationCounters;
    public:
        Serial_Engine() : Engine() {
            setup(1);
        };

        Coordinate getStartingCoordinate() {
            return {dist(rng), dist(rng), unitDist(rng)};
        }

        uint64_t getTotalIterations() {
            uint64_t totalIterations = 0;
            for(size_t i = 0; i < iterationCounters.size(); i++) {
                totalIterations += iterationCounters[i];
            }
            return totalIterations;
        }


        void setup(int numThreads, int seed = 0) {
            threadCoords.resize(numThreads);
            for(int i = 0; i < numThreads; i++) {
                threadCoords[i] = getStartingCoordinate();
            }
            setSeed(seed);
        }

        void start() {
            recordStartTime(); // for logging for data -- I plan on lots of data collection
            running = true;
            printf("Spawning worker...\r\n");
            workingThread = std::thread(&Serial_Engine::workerLoop, this);
            printf("Thread spawned -- running = %d\r\n", running);
        }

        void stop() {
            static int debugNumCalls = 0;
            debugNumCalls += 1;
            running = false;
            if(workingThread.joinable()) {
                workingThread.join();
            }
            std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
            std::chrono::duration<double> runLength = endTime - startTime;
            printf("Stopping... (stop #%d)\r\n", debugNumCalls);
            printf("Run length was %.2f seconds.\r\nThis is an average of %f iterations/sec.\r\n\r\n", runLength.count(), totalHits[0] / runLength.count());
        }

        void reset() { // called when setTransforms is reset
            printf("Resetting!\r\n");
            if(running) {
                stop();
            }
            
        }
        
        Coordinate stepNoPlot(Coordinate c) {
            int func = pickFunction();

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
        std::vector<uint64_t> iterationCount; 
        
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
                    affine = variation_julia(affine);
                    break;
                case 14: // Bent
                    affine = variation_bent(affine);
                    break;
                case 15: // Waves
                    affine = variation_waves(affine);
                    break;
                case 16: // Fisheye
                    affine = variation_fisheye(affine);
                    break;
                case 17: // Popcorn
                    affine = variation_popcorn(affine);
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
                    affine = variation_rings(affine);
                    break;
                case 22: // Fan
                    affine = variation_fan(affine);
                    break;
                case 23: // Blob
                    affine = variation_blob(affine);
                    break;
                case 24: // PDJ
                    affine = variation_pdj(affine);
                    break;
                case 25: // Fan2
                    affine = variation_fan2(affine);
                    break;
                case 26: // Rings2
                    affine = variation_rings2(affine);
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
                    affine = variation_perspective(affine);
                    break;
                case 31: // Noise
                    affine = variation_noise(affine);
                    break;
                case 32: // JuliaN
                    affine = variation_julian(affine);
                    break;
                case 33: // JuliaScope
                    affine = variation_juliascope(affine);
                    break;
                case 34: // Blur
                    affine = variation_blur(affine);
                    break;
                case 35: // Gaussian
                    affine = variation_gaussian(affine);
                    break;
                case 36: // RadialBlur
                    affine = variation_radialblur(affine);
                    break;
                case 37: // Pie
                    affine = variation_pie(affine);
                    break;
                case 38: // Ngon
                    affine = variation_ngon(affine);
                    break;
                case 39: // Curl
                    affine = variation_curl(affine);
                    break;
                case 40: // Rectangles
                    affine = variation_rectangles(affine);
                    break;
                case 41: // Arch
                    affine = variation_arch(affine);
                    break;
                case 42: // Tangent
                    affine = variation_tangent(affine);
                    break;
                case 43: // Square
                    affine = variation_square(affine);
                    break;
                case 44: // Rays
                    affine = variation_rays(affine);
                    break;
                case 45: // Blade
                    affine = variation_blade(affine);
                    break;
                case 46: // Secant
                    affine = variation_secant(affine);
                    break;
                case 47: // Twintrian
                    affine = variation_twintrian(affine);
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
        
        void workerLoop() {
            // serial engine
            int index = 0; // will be obtained programatically from OpenMP or CUDA
            Coordinate* currentThreadCoordinate = &threadCoords[0];
            iterationCount.resize(index + 1);
            printf("Worker loop started.\r\n");
            for(int i = 0; i < 20; i++) {
                *currentThreadCoordinate = stepNoPlot(*currentThreadCoordinate);
            }

            iterationCount[index] = 0;

            while(running) {
                this->step(*currentThreadCoordinate, 0);
                iterationCount[index]++;
            }
            printf("Worker loop ended.\r\n");
        }
};


std::unique_ptr<Engine> createSerialEngine() {
    return std::make_unique<Serial_Engine>();
}


