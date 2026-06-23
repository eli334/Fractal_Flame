#pragma once
#include <cstdint> // uint<2^N>_t
#include <cstring> // might not be needed
#include <cfloat>  // DBL_MAX
#include <sstream> // std::stringstream -- one of the only slightly good formatting tools in C++
#include <string>  // std::string
#include <iostream> 
#include <iomanip> // why do I need 4 libraries in C++ for mediocre formatting?
#include <chrono> // time stuff in the future  
#include <vector> // OH YEAH!!!
#include <random> // rng for coordinate generation
#include <math.h> // log() in frontend -- 
#include <memory> // std::unique_ptr
#include <thread> 

#include <cassert> // debugging

// Abstract class for all Fractal Flame rendering engines 
// CPU, OpenMP, and CUDA implementations all inherit from this
// See /engines directory for each implementation (Serial, OpenMP, CUDA)

struct Affine {
    double a = 1.0, b = 0.0, c = 0.0;
    double d = 0.0, e = 1.0, f = 0.0;

    Affine() = default; // default constructor - Identity matrix 
    Affine(double a, double b, double c, double d, double e, double f) {
        this->a = a; this->b = b; this->c = c;
        this->d = d; this->e = e; this->f = f;
    }

    std::string formattedOutput = "";
    const char* toString() {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(2);
        stream << "Affine coefficients: ( " << a << "x + " << b << "y + " << c;
        stream <<                      ", " << d << "x + " << e << "y + " << f << ")\r\n";
        formattedOutput = stream.str();
        return formattedOutput.c_str();
    }

    std::vector<double> toVector() const {
        return {a, b, c, d, e, f};
    }

};

struct VariationDef {
    int index = 0;

    // next todo: make this std::string instead of const char*
    // ECE 0301/0302 kind of made me a worse coder :(
    const char* name = "Identity";
    const char* formula = "(x, y)";

    std::string formattedOutput = "";
    const char* toString() {
        std::stringstream stream;
        stream << "Varation: " << name << " (" << index << ")\r\n";
        stream << "Formula: " << formula;
        formattedOutput = stream.str();
        return formattedOutput.c_str();
    }
};

struct Transform {
    float weight = 1.0f; // sumOfWeights / transform_i.weight = probability that transform_i gets picked
    float color = 0; // c_i in paper -- [0, 1]
    VariationDef variation;
    Affine coeffs; // affine coefficients - identity matrix by default

    std::string formattedOutput = "";
    const char* toString(int index) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(3);
        stream << "Transform " << index << ":\r\n";
        stream << "Weight: " << weight << "; Color: " << color << "\r\n";
        stream << variation.toString();
        stream << coeffs.toString();
        formattedOutput = stream.str();
        return formattedOutput.c_str();
    }
};

struct Coordinate {
    double x = 0, y = 0;
    double color = 0.0;
};

template<typename T>
class Histogram {
    public:
        int width = 1000, height = 1000;
        std::vector<T> data;

        Histogram(int desiredWidth = 1000, int desiredHeight = 1000) : width(desiredWidth), height(desiredHeight) {
            data.resize(desiredWidth*desiredHeight);
        }

        int getWidth() const {
            return width;
        }

        int getHeight() const {
            return height;
        }

        void clear() {
            std::fill(data.begin(), data.end(), T{}); // std::fill -- memset but actually works how I want with constructors
        }

        void resize(int newWidth, int newHeight) {
            width = newWidth;
            height = newHeight;
            data.resize(newWidth * newHeight);
        }

        const T get(int x, int y) {
            if(x >= 0 && x < width && y >= 0 && y < height) {
                return data[y*width + x];
            }
        }
        
        const T get(int index) {
            if(index >= 0 && index < width * height) {
                return data[index];
            } else {
                assert(false); // should be impossible to get here
            }
        }

        Histogram(const Histogram&) = delete; // don't want or need copy constructor
        Histogram& operator=(const Histogram&) = delete; // don't need assignment
};

struct Color { // pixel colors
    uint8_t r = 0, g = 0, b = 0, a = 255;
    Color() = default;
    Color(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
    Color(uint8_t* triple) : r(triple[0]), g(triple[1]), b(triple[2]) {}
    Color(float* triple) {
        r = static_cast<uint8_t>(triple[0] * 255); 
        g = static_cast<uint8_t>(triple[1] * 255); 
        b = static_cast<uint8_t>(triple[2] * 255);
    }
    uint8_t operator[](int i) const { // read-only method of getting colors
        switch(i) {
            case 0: return r;
            case 1: return g;
            case 2: return b;
            default:
            case 3: return a; // just in case -- should only ever be 0, 1, or 2
        }
    }
};

struct Viewport {
    double minX = -1.0, maxX = 1.0;
    double minY = -1.0, maxY = 1.0;

    Viewport() = default;
    Viewport(float* fourFloats) {
        minX = static_cast<double>(fourFloats[0]);
        maxX = static_cast<double>(fourFloats[1]);
        minY = static_cast<double>(fourFloats[2]);
        maxY = static_cast<double>(fourFloats[3]);
    }
    Viewport(double a, double b, double c, double d) {
        minX = a;
        maxX = b;
        minY = c;
        maxY = d;
    }
};

struct PixelData{
    uint64_t hits = 0;
    double color = 0;
};

class Engine;

struct EngineState {
    int seed = 0;
    Viewport viewport;
    std::vector<Transform> transforms;
    bool hasFinalTransform = false;
    Transform finalTransform;

    void applyPreset(std::unique_ptr<Engine>& fractal_engine);
}; 


class Engine {
    protected:
        bool running = false;
        Viewport viewport;
        std::vector<Transform> transforms;
        bool hasFinalTransform = false;
        Transform finalTransform;
        Histogram<PixelData> globalHistogram;
    
        std::vector<Coordinate> threadCoords;

        int seed = 0;
        std::chrono::_V2::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    private:
        float totalWeight = 1.0;
        void calculateWeight() {
            if(transforms.size() == 0) {
                totalWeight = 1.0; // don't set to 0; step() divides by totalWeight, which would make current.<x or y> NaN
                return;
            }
            totalWeight = 0;
            for(size_t i = 0; i < transforms.size(); i++) {
                totalWeight += transforms[i].weight;
            }
        }
        void recalculateColors() {
            if(this->transforms.size() == 1) {
                this->transforms[0].color = 1.0;
                return;
            }
            
            for(size_t i = 0; i < transforms.size(); i++) {
                this->transforms[i].color = (float)i / (this->transforms.size() - 1); 
            }
        }
        void configChanged() {
            globalHistogram.clear();
            calculateWeight();
            recalculateColors();
        }
        
    public:
        virtual void step(Coordinate c, int threadIndex) = 0; // iterate coordinate once   
        virtual Coordinate stepNoPlot(Coordinate c) = 0;
        virtual ~Engine() = default; // reset
        
        virtual std::vector<VariationDef> getSupportedVariations() = 0;
        
        virtual void setup(int numThreads, int seed = 0) = 0; // Spawn the thread, pause
        virtual Coordinate getStartingCoordinate() = 0;
        virtual void start() = 0; // infinitely call step()
        virtual void stop() = 0; // stop infinitely calling step()

        float getTotalWeight() {
            return totalWeight;
        }

        std::vector<Transform> getTransforms() {
            return transforms;
        }

        bool isThereAFinalTransform() { // quite
            return hasFinalTransform;
        }
        
        Viewport getViewport() {
            return viewport;
        }

        void setViewport(Viewport vp) {
            stop();
            viewport = vp;
            configChanged();
        }







        void applyPreset(EngineState state) {
            seed = state.seed;
            viewport = state.viewport;
            hasFinalTransform = state.hasFinalTransform;
            finalTransform = state.finalTransform;
            setTransforms(state.transforms);    
            // engine stays paused after applying state
        }

        void setSeed(int seed) {
            this->seed = seed;
            configChanged();
        }

        void setFinalTransform(Transform final) {
            stop();
            finalTransform = final;
            hasFinalTransform = true;
            configChanged();
        }

        void clearFinalTransform(void) {
            stop();
            if(!hasFinalTransform) return; // don't clear if hasFinalTransform is false already
            finalTransform = Transform();
            
            
            hasFinalTransform = false;
            configChanged();
        }

        bool getStatus() {
            return running;
        }
        
        Coordinate getCurrent(int index) {
            size_t correctIndexType = (size_t) index; // oh, C++
            if(correctIndexType > threadCoords.size()) return Coordinate(); // if OOB, just return (0, 0) - i don't even use getCurrent
            return threadCoords[correctIndexType];
        }

        const Histogram<PixelData>* getHistogram() { // const makes this read-only
            return &globalHistogram;
        }

        void resize(int width, int height) {
            stop();
            globalHistogram.resize(width, height);
        }

        /** might refactor with Doxygen next -- I like good documentation, and this engine lacks that
        // @brief fills pixels[] with RGBA data
        // @param pixels must point to a buffer of at least Engine*->getWidth() * Engine*->getHeight() * 4 bytes
        // @param palette must have (paletteSize * 3 bytes per color) bytes allocated
        // @returns Returns true if buffer changed, false if no change
        */
        bool fillPixelBuffer(uint8_t* pixels, const Color* palette, int paletteSize) { // function to calculate colors for texture, returns false if no change
            if(!getStatus()) return false; // if it isn't running, then the buffer is not changing
            
            static uint64_t lastMaxVal = 0;
            uint64_t maxVal = 0;
            int width = globalHistogram.getWidth();
            int height = globalHistogram.getHeight();

            for(int i = 0; i < width*height; i++) {
                if(globalHistogram.get(i).hits > maxVal) {
                    maxVal = globalHistogram.get(i).hits;
                }
            }
            if(maxVal == 0) return false; // don't render when nothing has generated yet
            if(lastMaxVal == maxVal) return false; // only render when maxVal increases; since it follows a power law, one histogram pixel should be hit WAY more than the rest
            
            lastMaxVal = maxVal;
            
            // printf("Starting upload...\r\n");
            
            double logMax = std::log(1.0f + (double)maxVal); // convert uint64_t to double, with 1.0 because log(0) is undefined
            
            for(int i = 0; i < width*height; i++) {
                // 8 bits per pixel
                // logarithmically map brightness
                if(globalHistogram.get(i).hits == 0) {
                    pixels[i*4+0] = 0;  
                    pixels[i*4+1] = 0;  
                    pixels[i*4+2] = 0;  
                    pixels[i*4+3] = 255; // alpha of 255 is fully opaque
                    continue;
                } // untouched pixels are black
                PixelData currentPixel = globalHistogram.get(i);
                double brightness = std::log(currentPixel.hits) / logMax; // log of hist[i] for every index of the entire histogram
                // double brightness = (double)currentPixel.hits / (double)maxVal;
                // one million log() operations per frame (oof!) [lookup table?]
                //std::vector<double> logTable(maxVal + 1);
                
                // for(uint64_t i = 1; i <= maxVal; ++i) {
                //     logTable[i] = std::log((double)i) / logMax;
                // }

                // dividing by the max value makes the max 1 and the rest a number between 0 and 1
                brightness = std::pow(brightness, 1.0/2.2); // gamma correction
                double colorCoord = currentPixel.color / static_cast<double>(currentPixel.hits); 
                int paletteIndex = (int)(colorCoord * (paletteSize - 1));
                pixels[i*4+0] = (uint8_t)(palette[paletteIndex][0]); // red  
                pixels[i*4+1] = (uint8_t)(palette[paletteIndex][1]); // green
                pixels[i*4+2] = (uint8_t)(palette[paletteIndex][2]); // blue
                pixels[i*4+3] = 255; // alpha of 255 is fully opaque
            }
            // buffer done filling
            return true;
        }

        // void applySettings() {

        // }
        
        void removeTransform(int index) {
            printf("removeTransform called\r\n");
            stop();
            this->transforms.erase(transforms.begin() + index);
            configChanged();
        }

        void addTransform(Transform transform) {
            printf("addTransform called\r\n");
            stop();
            this->transforms.push_back(transform); // add the transform
            configChanged();
        }

        void setTransforms(std::vector<Transform> transforms) {
            printf("setTransforms called with %d transforms\r\n", (int)transforms.size());
            stop(); // stop the engine before setting the transforms
            this->transforms = transforms; // set the transforms
            configChanged();
        }

        void setTransform(int index, Transform transform) {
            printf("Transform %d changed!\r\n%s", index, transform.toString(index));
            stop();
            this->transforms[index] = transform;
            configChanged();
        }

        void calculateViewport() {
            std::thread t([this]() 
            {          // [this] is the lambda's capture -- it gets the object "this", which would be the pointer to the implemented engine's location in memory
                double minX = DBL_MAX, maxX = -DBL_MAX, minY = DBL_MAX, maxY = -DBL_MAX; // max representable size of double (1.8e308)
                Coordinate viewportThreadCoord = getStartingCoordinate();
                for(int i = 0; i < 1000; i++) {
                    viewportThreadCoord = stepNoPlot(viewportThreadCoord);
                    
                    if(viewportThreadCoord.x < minX) {
                        minX = viewportThreadCoord.x;
                    }
                    
                    if(viewportThreadCoord.x > maxX) {
                        maxX = viewportThreadCoord.x;
                    }
                    
                    if(viewportThreadCoord.y < minY) {
                        minY = viewportThreadCoord.y;
                    }
                    
                    if(viewportThreadCoord.y > maxY) {
                        maxY = viewportThreadCoord.y;
                    } 
                    
                }
                setViewport(Viewport(minX, maxX, minY, maxY));
            });
            t.join(); // join thread
        }
    
    
};

inline void EngineState::applyPreset(std::unique_ptr<Engine>& fractal_engine) { // inline or else the compiler complains
    fractal_engine->applyPreset(EngineState{seed, viewport, transforms, hasFinalTransform, finalTransform});
}


std::unique_ptr<Engine> createSerialEngine();
std::unique_ptr<Engine> createOpenMPEngine(int threadCount);


#ifdef HAS_CUDA
std::unique_ptr<Engine> createCUDAEngine();
#endif