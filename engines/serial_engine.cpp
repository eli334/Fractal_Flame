#include "./engine.h"
#include <thread> // for serial implementation -- it runs on a single thread for the engine, with a seperate render thread for the frontend

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
        Serial_Engine(fractalSettings& config) : Engine(config), rng(config.seed) {
            setup();
            };

        void setup(int seed = 0) {
            current.x = dist(rng); // starting point
            current.y = dist(rng); 
        }
        void start() {
            running = true;
            workingThread = std::thread(&Serial_Engine::workerLoop, this);
            std::cout << "Spawned thread with ID " << workingThread.get_id() << "!" << std::endl; 
        };

        void stop() {
            std::cout << "Killing thread ID: " << workingThread.get_id() << std::endl; 
            running = false;
            workingThread.join(); // let working thread clean up
            printf("Thread killed!\n");
        }
        
        void step() {
            int func = pickFunction();
            if(func < 0) { // there are no transforms selected but the user pressed > -- do nothing
                return;
            }
            current = calculate(func);
            
            plot(); // plots the current point if within histogram

            if(settings.hasFinalTransform) {

            }
        };

        void plot() {
            int plotX = (int)((current.x + 1) / 2 * settings.global_histogram.width);
            int plotY = (int)((current.y + 1) / 2 * settings.global_histogram.height);
            
            // static int count = 0;
            // if(count++ < 20) {
            //        printf("plotX: %d, plotY: %d, x: %f, y: %f\n", plotX, plotY, current.x, current.y);
            // }
            if(plotX >= 0 && plotX <= settings.global_histogram.width &&
               plotY >= 0 && plotY <= settings.global_histogram.height) {
                settings.global_histogram.data[plotY * settings.global_histogram.width + plotX]++;
               }
        }

        bool setTransforms(std::vector<Transform> TFList) {
            printf("setTransforms called with %d transforms\n", (int)TFList.size());
            settings.transforms = TFList;
            
            

            settings.totalWeight = 0;
            for(Transform t : settings.transforms) {
                settings.totalWeight += t.weight;
            }
            printf("totalWeight: %f\n", settings.totalWeight);

            settings.global_histogram.clear(); // clear the histogram for the new transforms
            return true;
        }

        void setSeed(int seed) {
            // frontend frees old histogram
            settings.seed = seed;
        }

        bool getStatus() {
            return running;
        }

        std::vector<variationDef> getSupportedVariations() { // updated by me, per engine, for frontend purposes
            std::vector<variationDef> supportedVariations = {
                {0, "Identity", "x, y"},
                {1, "Sinusoidal", "sin(x), sin(y)"},
                {2, "Spherical", "x / r^2, y / r^2"}
            };

            return supportedVariations;
        }

        // ~Serial_Engine() {}
    private:
        bool running = false;
        std::thread workingThread;
        Coordinate current;
        std::mt19937_64 rng; // random number generator, set to 'seed' when run
        std::uniform_real_distribution<double> dist{-1.0, 1.0};   // for x, y starting point
        std::uniform_real_distribution<double> unitDist{0.0, 1.0}; // for color coordinates and step(), picking from weighted functions

        int pickFunction() {
            double i = unitDist(rng) * settings.totalWeight; // a number from 0 to totalWeight
            float cumulativeWeight = 0;
            for(int func = 0; func < settings.transforms.size(); func++) { // [0, slice0), (slice0, slice1)..., (sliceN-1, totalWeight]
                cumulativeWeight += settings.transforms[func].weight;
                if(i < cumulativeWeight) {
                    return func;
                }
            }
            
            //throw("unreachable state in theory");
            return (int)settings.transforms.size() - 1; 
        };

        Coordinate calculate(int funcIndex) { // current is a private variable
            switch(funcIndex) {
                case 0: // Identity
                    return variation_identity(current);
                case 1: // Sinusoidal
                    return variation_sinusoidal(current);
                case 2: // Spherical
                    return variation_spherical(current);
                default:
                    return {0, 0};
            }
        }

        void workerLoop() {
            while(running) {
                step();
            }
        }
};


Engine* createSerialEngine(fractalSettings& config) {
    return new Serial_Engine(config);
}


