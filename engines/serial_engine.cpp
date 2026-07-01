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
        std::vector<std::unique_ptr<Xorshift64>> rngs; 
        std::thread workingThread;

    public:
        Serial_Engine() : Engine() {
            std::random_device slowRNG;
            uint64_t seed = ((uint64_t)slowRNG() << 32) | slowRNG(); // random 64 bit number
            setup(1, seed);
        };

        Coordinate getStartingCoordinate(int threadIndex) {
            double x = 1 - 2 *  rngs[threadIndex]->getDouble();
            double y = 1 - 2 *  rngs[threadIndex]->getDouble();
            double color = rngs[threadIndex]->getDouble();
            return {x, y, color};
        }

        uint64_t getTotalIterations() {
            uint64_t totalIterations = 0;
            for(size_t i = 0; i < iterationCounters.size(); i++) {
                totalIterations += iterationCounters[i];
            }
            return totalIterations;
        }


        void setup(int numThreads, uint64_t seed) {
            threadCoords.resize(numThreads);
            rngs.reserve(numThreads);
            iterationCounters.resize(numThreads);
            for(uint64_t i = 0; i < (uint64_t) numThreads; i++) {
                rngs.emplace_back(std::make_unique<Xorshift64>(seed ^ (i * 2654435761ULL))); // Knuth multiplicative constant (2^32 * the golden ratio)
                threadCoords[i] = getStartingCoordinate(i);
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
            printf("Run length was %.2f seconds.\r\nThis is an average of %f iterations/sec.\r\n\r\n", runLength.count(), getTotalIterations() / runLength.count());
        }

        void reset() { // called when setTransforms is reset
            printf("Resetting!\r\n");
            if(running) {
                stop();
            }
            
        }
        
        Coordinate stepNoPlot(Coordinate c, Xorshift64 &rng) {
            int func = pickFunction(rng);

            if(func < 0) { // there are no transforms selected but the user pressed > -- do nothing
                return c;
            }

            c = calculate(func, c, rng);

            if(!std::isfinite(c.x) || !std::isfinite(c.y)) {
                c = {1 - 2 * rng.getDouble(),  1 - 2 * rng.getDouble(), rng.getDouble()}; // reset to a known good point
            }

            if(hasFinalTransform) {
                c = calculateFinalTransform(c, rng);

                if(!std::isfinite(c.x) || !std::isfinite(c.y)) {
                    c = {1 - 2 * rng.getDouble(),  1 - 2 * rng.getDouble(), rng.getDouble()}; // reset to a known good point
                }   
            }

            return c;
        };

        void step(Coordinate c, int threadIndex, Xorshift64 &rng) {
            threadCoords[threadIndex] = stepNoPlot(c, rng);
            plot(threadCoords[threadIndex]); // plots the current point if within histogram
        };

        void plot(Coordinate pointToPlot) {
            int plotX = (int)((pointToPlot.x - viewport.minX) / (viewport.maxX - viewport.minX) * globalHistogram.width);
            int plotY = (int)((pointToPlot.y - viewport.minY) / (viewport.maxY - viewport.minY) * globalHistogram.height);
            
            if(plotX >= 0 && plotX < globalHistogram.width && // if within bounds of histogram:
               plotY >= 0 && plotY < globalHistogram.height) {
                int index = plotY * globalHistogram.width + plotX;
                globalHistogram.data[index].hits++;
                globalHistogram.data[index].color += pointToPlot.color;
            }
        }

        std::vector<VariationDef> getSupportedVariations() { // updated by me, per engine, for frontend purposes
            static std::vector<VariationDef> supportedVariations = { // static for c_str()
                {0, "Identity"}, // called Linear in paper, but Identity makes more sense
                {1, "Sinusoidal"},
                {2, "Spherical"},
                {3, "Swirl"},
                {4, "Horseshoe"},
                {5, "Polar"},
                {6, "Handkerchief"},
                {7, "Heart"},
                {8, "Disc"},
                {9, "Spiral"},
                {10, "Hyperbolic"},
                {11, "Diamond"},
                {12, "Ex"},
                {13, "Julia"},
                {14, "Bent"},
                {15, "Waves"},
                {16, "Fisheye"},
                {17, "Popcorn"},
                {18, "Exponential"},
                {19, "Power"},
                {20, "Cosine"},
                {21, "Rings"},
                {22, "Fan"},
                {23, "Blob"},
                {24, "PDJ"},
                {25, "Fan2"},
                {26, "Rings2"},
                {27, "Eyefish"},
                {28, "Bubble"},
                {29, "Cylinder"},
                {30, "Perspective"},
                {31, "Noise"},
                {32, "JuliaN"},
                {33, "JuliaScope"},
                {34, "Blur"},
                {35, "Gaussian"},
                {36, "RadialBlur"},
                {37, "Pie"},
                {38, "Ngon"},
                {39, "Curl"},
                {40, "Rectangles"},
                {41, "Arch"},
                {42, "Tangent"},
                {43, "Square"},
                {44, "Rays"},
                {45, "Blade"},
                {46, "Secant"},
                {47, "Twintrian"},
                {48, "Cross"}
            };

            return supportedVariations;
        }

    private:
        int pickFunction(Xorshift64 &rng) {
            double i = rng.getDouble() * getTotalWeight(); // a number from 0 to totalWeight
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

        Coordinate calculateFinalTransform(Coordinate coord, Xorshift64 &rng) {
            Coordinate finalTF = calculate(finalTransform, coord, rng);
            return finalTF;
        }

        Coordinate calculate(int funcIndex, Coordinate coord, Xorshift64 &rng) {
            return calculate(transforms[funcIndex], coord, rng);
        }

        Coordinate calculate(Transform &t, Coordinate coord, Xorshift64 &rng) {
            // apply affine transform first
            Coordinate affine = {
                t.coeffs.a * coord.x + t.coeffs.b * coord.y + t.coeffs.c,
                t.coeffs.d * coord.x + t.coeffs.e * coord.y + t.coeffs.f 
            }; // (x, y) = (a*x+b*y+c), (d*x+e*y+f))
            // with identity matrix this maps to
            // (x, y) = (x, y)

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
        
        void workerLoop() {
            // serial engine
            int threadIndex = 0; // will be obtained programatically from OpenMP or CUDA
            Coordinate* currentThreadCoordinate = &threadCoords[threadIndex];
            printf("Worker loop started.\r\n");
            for(int i = 0; i < 20; i++) {
                *currentThreadCoordinate = stepNoPlot(*currentThreadCoordinate, *rngs[threadIndex]);
                iterationCounters[threadIndex]++;
            }

            while(running) {
                this->step(*currentThreadCoordinate, threadIndex, *rngs[threadIndex]);
                iterationCounters[threadIndex]++;
            }
            printf("Worker loop ended.\r\n");
        }
};


std::unique_ptr<Engine> createSerialEngine() {
    return std::make_unique<Serial_Engine>();
}


