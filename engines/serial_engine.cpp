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
            setup();
        };

        Coordinate getStartingCoordinate() {
            return {dist(rng), dist(rng), unitDist(rng)};
        }

        void setup(int seed = 0) {
            current = getStartingCoordinate();
            
        }

        void start() {
            running = true;
            
        }

        void stop() {
            running = false;
            if(workingThread.joinable()) {
                workingThread.join();
            }
        }
        
        Coordinate stepNoPlot(Coordinate c) {
            int func = pickFunction();
            if(func <= 0) { // there are no transforms selected but the user pressed > -- do nothing
                return c;
            }
            c.color = (c.color + transforms[func].color) / 2.0;
            current = calculate(func);
            return current;
        }

        void step(Coordinate c) {
            current = stepNoPlot(c);
            if(hasFinalTransform) {

            }
            plot(c); // plots the current point if within histogram
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
            std::vector<VariationDef> supportedVariations = {
                {0, "Identity", "x, y"},
                {1, "Sinusoidal", "sin(x), sin(y)"},
                {2, "Spherical", "x / r^2, y / r^2"}
            };

            return supportedVariations;
        }

    private:
        bool running = false;
        std::thread workingThread;
        std::mt19937_64 rng; // random number generator, set to 'seed' when run
        std::uniform_real_distribution<double> dist{-1.0, 1.0};   // for x, y starting point
        std::uniform_real_distribution<double> unitDist{0.0, 1.0}; // for color coordinates
        
        int pickFunction() {
            double i = unitDist(rng) * getTotalWeight(); // a number from 0 to totalWeight
            float cumulativeWeight = 0;
            for(int func = 0; func < transforms.size(); func++) { // [0, slice0), (slice0, slice1)..., (sliceN-1, totalWeight]
                cumulativeWeight += transforms[func].weight;
                if(i < cumulativeWeight) {
                    return func;
                }
            }
            
            //throw("unreachable state in theory");
            return (int)transforms.size() - 1; 
        };

        Coordinate calculate(int funcIndex) {
            Transform& t = transforms[funcIndex];

            // apply affine transform first
            Coordinate affine = {
                t.coeffs.a * current.x + t.coeffs.b * current.y + t.coeffs.c,
                t.coeffs.d * current.x + t.coeffs.e * current.y + t.coeffs.f 
            }; // (x, y) = (a*x+b*y+c), (d*x+e*y+f))
            // with identity matrix this maps to
            // (x, y) = (x, y)
            switch(funcIndex) {
                case 0: // Identity
                    return variation_identity(affine);
                case 1: // Sinusoidal
                    return variation_sinusoidal(affine);
                case 2: // Spherical
                    return variation_spherical(affine);
                default:
                    return affine;
            }
        }

        void workerLoop() {
            Coordinate workerCoord;
            while(running) {
                this->step(workerCoord);
            }
        }
};


std::unique_ptr<Engine> createSerialEngine() {
    return std::make_unique<Serial_Engine>();
}


