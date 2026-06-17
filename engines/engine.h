#pragma once
#include <cstdint>
#include <cstring>
#include <iostream>
#include <variant> // for the parametric coefficients later maybe  
#include <vector> // OH YEAH!!!
#include <random>
#include <math.h>
#include <memory>

// Abstract class for all Fractal Flame rendering engines 
// CPU, OpenMP, and CUDA implementations all inherit from this
// See ./engines/ directory for each implementation (Serial, OpenMP, CUDA)

struct Transform {
    float weight = 1.0f; // sumOfWeights / transform_i.weight = probability that transform_i gets picked
    int variationIndex = 0; // just pick one for now
    float color = 0;

    // affine coefficients - identity matrix by default
    double a = 1.0, b = 0.0, c = 0.0;
    double d = 0.0, e = 1.0, f = 0.0;
};

struct variationDef {
    int index;
    const char* name;
    const char* formula;
};

struct Coordinate {
    double x, y;
    double color = 0.0;
};




template<typename T>
class Histogram {
    public:
        uint32_t width = 1000, height = 1000;
        T* data = nullptr;
        float* colorData = nullptr;
        
        Histogram(uint32_t desiredWidth = 1000, uint32_t desiredHeight = 1000) : width(desiredWidth), height(desiredHeight) {
            data = new T[width * height]();
            colorData = new float[width * height]();
        }

        uint32_t getWidth() const {
            return width;
        }

        uint32_t getHeight() const {
            return height;
        }

        void clear() {
            memset(data, 0, width * height * sizeof(T));
            memset(colorData, 0, width * height * sizeof(float));
        }

        void resize(int newWidth, int newHeight) {
            delete [] data;
            delete [] colorData;

            width = (uint32_t) newWidth;
            height = (uint32_t) newHeight;

            data = new T[width * height]();
            colorData = new float[width*height]();
        }

        ~Histogram() {
            delete [] data;
            delete [] colorData;
        }

        Histogram(const Histogram&) = delete; // don't want or need copy constructor
        Histogram& operator=(const Histogram&) = delete; // don't need assignment
};

struct Viewport { // it's a math thing more than a rendering thing, so it's baked into the engine
    double minX = -1.0, maxX = 1.0;
    double minY = -1.0, maxY = 1.0;
};

struct fractalSettings {
    int seed = 0;
    float totalWeight = 1.0; 
    
    Histogram<uint64_t> global_histogram;
    
    std::vector<Transform> transforms;
    
    bool hasFinalTransform;
    Transform finalTransform;
    Viewport viewport;
};

class Engine {
    public:
        Engine(fractalSettings& config) : settings(config) {};
        //virtual void setup() = 0; // set up renderer
        virtual void setup(int seed = 0) = 0;
        virtual void step() = 0; // iterate renderer once
        virtual void setSeed(int seed) = 0;
        virtual bool setTransforms(std::vector<Transform> transforms) = 0;   
        virtual ~Engine() = default; // reset
        virtual std::vector<variationDef> getSupportedVariations() = 0;
        // user controls
        virtual void start() = 0;
        virtual void stop() = 0;

        virtual bool getStatus() = 0; // True if running, false if not
    protected:
        fractalSettings& settings;
};

Engine* createSerialEngine(fractalSettings& config);
Engine* createOpenMPEngine(fractalSettings& config, int threadCount);


#ifdef HAS_CUDA
Engine* createCUDAEngine(fractalSettings& config);
#endif