#include "./engine.h"

// Web_Engine -- single-threaded backend for the Emscripten/WASM build.
//
// Extends Engine directly (not Serial_Engine) so this file has zero dependency
// on <thread>/std::thread. Serial_Engine declares a std::thread workingThread
// member and spawns it in start(); pulling that in here would tie the whole
// web build to Emscripten's pthread mode (-pthread, SharedArrayBuffer, COOP/COEP
// headers) for no benefit, since the browser's own main-loop callback already
// *is* our loop driver. Web_Engine plays that role with runBatch() instead of
// a worker thread: web_main.cpp calls runBatch(n) once per animation frame.
//
// The per-step math (pickFunction/calculate/calculateFinalTransform, and the
// getSupportedVariations list) below is copied verbatim from Serial_Engine,
// since none of it is thread-related -- it's the same 49-variation dispatch
// either way.

class Web_Engine : public Engine {
    protected:
        std::unique_ptr<Xorshift64> rng;
        uint64_t iterationCounter = 0;
        bool warmedUp = false; // tracks whether the 20-iteration chaos-game warmup (see runBatch) has run yet

    public:
        Web_Engine() : Engine() {
            setup(1);
        }

        // randomly get an x, y, and color coordinate -- mirrors Serial_Engine,
        // but reads the single member rng directly since Web_Engine never has
        // more than one "thread" (threadIndex is accepted only to satisfy the
        // Engine interface / getCurrent()).
        Coordinate getStartingCoordinate(int threadIndex) override {
            (void)threadIndex;
            double x = 1 - 2 * rng->getDouble();
            double y = 1 - 2 * rng->getDouble();
            double color = rng->getDouble();
            return {x, y, color};
        }

        uint64_t getTotalIterations() override {
            return iterationCounter;
        }

        // get value of hottest histogram bin
        uint64_t getMaxHits() override {
            if (globalHistogram.data.empty()) return 0;
            PixelData maxHistogramBin = *std::max_element(globalHistogram.data.begin(), globalHistogram.data.end());
            return maxHistogramBin.hits;
        }

        // numThreads is accepted (to satisfy Engine's pure virtual signature)
        // but ignored -- Web_Engine is always single-threaded.
        void setup(int numThreads = 1) override {
            (void)numThreads;
            std::random_device slowRNG;
            uint64_t seed = ((uint64_t)slowRNG() << 32) | slowRNG(); // random 64 bit number

            rng = std::make_unique<Xorshift64>(seed);
            threadCoords.resize(1);
            threadCoords[0] = getStartingCoordinate(0);
            iterationCounter = 0;
        }

        // No thread to spawn -- this just arms runBatch(). web_main.cpp's
        // emscripten_set_main_loop_arg callback is the actual loop driver.
        void start() override {
            recordStartTime();
            running = true;
        }

        void stop() override {
            running = false;
            recordEndTime();
        }

        void reset() override {
            if (running) stop();
        }

        Coordinate stepNoPlot(Coordinate c, Xorshift64 &rng) override {
            int func = pickFunction(rng);

            if (func < 0) { // no transforms selected -- do nothing
                return c;
            }

            c = calculate(func, c, rng);

            if (!std::isfinite(c.x) || !std::isfinite(c.y)) {
                c = {1 - 2 * rng.getDouble(), 1 - 2 * rng.getDouble(), rng.getDouble()}; // reset to a known good point
            }

            if (hasFinalTransform) {
                c = calculateFinalTransform(c, rng);

                if (!std::isfinite(c.x) || !std::isfinite(c.y)) {
                    c = {1 - 2 * rng.getDouble(), 1 - 2 * rng.getDouble(), rng.getDouble()};
                }
            }

            return c;
        }

        void step(Coordinate c, int threadIndex, Xorshift64 &rng) override {
            threadCoords[threadIndex] = stepNoPlot(c, rng);
            plot(threadCoords[threadIndex]);
        }

        void plot(Coordinate pointToPlot) {
            int plotX = (int)((pointToPlot.x - viewport.minX) / (viewport.maxX - viewport.minX) * globalHistogram.width);
            int plotY = (int)((1 - (pointToPlot.y - viewport.minY) / (viewport.maxY - viewport.minY)) * globalHistogram.height);

            if (plotX >= 0 && plotX < globalHistogram.width &&
                plotY >= 0 && plotY < globalHistogram.height) {
                int index = plotY * globalHistogram.width + plotX;
                globalHistogram.data[index].hits++;
                globalHistogram.data[index].color += pointToPlot.color;
            }
        }

        // Web-specific: run n iterations synchronously. Called once per browser
        // animation frame from web_main.cpp, sized adaptively to hit a ~16ms
        // frame budget. This fills the role Serial_Engine's workerLoop() plays
        // natively, minus the thread and the 20-iteration warmup (the warmup
        // matters for a fresh chaos-game point converging onto the attractor;
        // since Web_Engine's starting coordinate is only computed once in
        // setup(), not per-batch, we do that warmup once here instead).
        void runBatch(uint64_t n) {
            if (!running) return;

            if (!warmedUp) {
                for (int i = 0; i < 20; i++) {
                    threadCoords[0] = stepNoPlot(threadCoords[0], *rng);
                    iterationCounter++;
                }
                warmedUp = true;
            }

            for (uint64_t i = 0; i < n; i++) {
                step(threadCoords[0], 0, *rng);
                iterationCounter++;
            }
        }

        std::vector<VariationDef> getSupportedVariations() override { // updated by me, per engine, for frontend purposes
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

    protected:
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
};
