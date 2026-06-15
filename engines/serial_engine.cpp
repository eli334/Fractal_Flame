#include "./engine.h"


// Implementing the "Variations" from the original flam3 paper (https://flam3.com/flame_draves.pdf - see Appendix)

fractalSettings config;


class Serial_Engine : public Engine {
    public:
        Serial_Engine(fractalSettings config) : Engine(config), rng(config.seed) {
            local_histogram = new int[settings.histogram_width * settings.histogram_height];
        };

        void setup() {
            x = dist(rng); // starting point
            y = dist(rng); 
        }

        void step() {
            uint8_t i = functionPicker(rng);
        };

        bool setTransforms(std::vector<Transform> TFList) {
            transforms = TFList;
            functionPicker = std::uniform_int_distribution<int>(0, transforms.size() - 1);
            return true;
        }

        ~Serial_Engine() {
            delete [] local_histogram;
        }
    private:
        double x, y; // thread's current x, y position (n x 2 matrix with # of threads = n in future, multithreaded implementations)
        std::mt19937_64 rng; // random number generator, set to 'seed' when run
        std::uniform_real_distribution<double> dist{-1.0, 1.0};   // for x, y starting point
        std::uniform_real_distribution<double> colorDist{0.0, 1.0}; // for color coordinates
        std::uniform_int_distribution<int> functionPicker{0, transforms.size()};
        int* local_histogram, global_histogram;
};

