#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <cuda_runtime.h>

// Fractal Flame generation
// Has a GUI at the start to pick the mode -- see other files for implementations
// Maximizes performance with OpenMP and CUDA, if it's worth splitting the work into multiple cores



int main(int argc, char **argv ) { 






    int threads = atoi(argv[2]); // ./fractal_flame -T 8 -R 1920x1080
                                //          0       1 2  3     4
    
    std::string resolution = argv[4]; // I will deal with this later lol

    std::cout << "Threads: " << threads << "\nResolution: "<< resolution << std::endl;

    

}
