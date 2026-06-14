#pragma once
 
// Abstract class for all Fractal Flame rendering engines
// CPU, OpenMP, and CUDA implementations all inherit from this
// See ./engines/ directory for each implementation (Serial, OpenMP, CUDA)
 
class AbstractFractalFlameEngine {
    public:
        AbstractFractalFlameEngine(int randomSeed);
        virtual void step() = 0; // iterate renderer once


        virtual ~AbstractFractalFlameEngine() = default;
    private:
        int seed;
        int* local_histogram; // stored locally
        int* global_histogram; // the one used for display (not sure if needed yet? almost definitely yes though)
};