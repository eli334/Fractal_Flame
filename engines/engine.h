#pragma once
#include <cstdint>
#include <variant> // for the parametric coefficients later maybe  
#include <vector> // OH YEAH!!!
#include <random>



// Abstract class for all Fractal Flame rendering engines 
// CPU, OpenMP, and CUDA implementations all inherit from this
// See ./engines/ directory for each implementation (Serial, OpenMP, CUDA)
typedef void (*VariationFn)(double x, double y, double& outX, double& outY);

struct Transform {
    float weight = 1.0f; // sumOfWeights / transform_i.weight = probability that transform_i gets picked
    int variationIndex = 0; // just pick one for now
};

struct fractalSettings {
    uint16_t histogram_width = 1000, histogram_height = 1000; // defaults to 1000x1000 -- TODO: add canvas settings to ImGui settings window
    int seed;
    uint64_t* global_histogram;
    std::vector<Transform>* transforms;
    
};

class Engine {
    public:
        Engine(fractalSettings config) : settings(config) {};
        virtual void setup() = 0; // set up renderer
        virtual void step() = 0; // iterate renderer once
        virtual void setSeed() = 0;
        virtual bool setTransforms(std::vector<Transform> transforms) = 0;   
        virtual ~Engine() = default; // reset
    protected:
        fractalSettings settings;
        static std::vector<Transform> transforms;
};

Engine* createSerialEngine();
Engine* createOpenMPEngine();


#ifdef HAS_CUDA
Engine* createCUDAEngine();
#endif