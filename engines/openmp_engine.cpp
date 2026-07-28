#include "./serial_engine.cpp"

#include <omp.h>

class OpenMP_Engine : public Serial_Engine {
    public:
        OpenMP_Engine(int desiredThreads) {
            if(desiredThreads < 0 || desiredThreads > omp_get_max_threads()) desiredThreads = 1; // guard -- set threads to 1
            logLevel(2, "Desired threads = %d\r\n", desiredThreads);
            setup(desiredThreads);
        }

        int getMaxThreads() override {
            return omp_get_num_procs(); 
        }

        void start() override { 
            recordStartTime(); // for logging for data -- I plan on lots of data collection
            running = true;
            workingThread = std::thread(&OpenMP_Engine::workerLoop, this, numThreads);
        }

        void stepThread(int threadIndex) {
            threadCoords[threadIndex] = stepNoPlot(threadCoords[threadIndex], *rngs[threadIndex]);
            localPlot(threadIndex, threadCoords[threadIndex]); // plots the current point if within histogram
            iterationCounters[threadIndex]++;
        };

        void localPlot(int threadIndex, Coordinate pointToPlot) {
            int plotX = (int)((pointToPlot.x - viewport.minX) / (viewport.maxX - viewport.minX) * globalHistogram.width);
            int plotY = (int)((1.0 - (pointToPlot.y - viewport.minY) / (viewport.maxY - viewport.minY)) * globalHistogram.height);
            
            if(plotX >= 0 && plotX < globalHistogram.width && // if within bounds of histogram:
                plotY >= 0 && plotY < globalHistogram.height) {
                int index = plotY * globalHistogram.width + plotX;
                localHistograms[threadIndex].data[index].hits++;
                localHistograms[threadIndex].data[index].color += pointToPlot.color;
            }
        }

        int setThreads(int desiredThreadCount) {
            logLevel(2, "setThreads called with %d\r\n", desiredThreadCount);
            if(desiredThreadCount <= getMaxThreads() && desiredThreadCount > 0) {
                if(running) {
                    stop();
                    setup(desiredThreadCount);
                    start();
                } else {
                    setup(desiredThreadCount);
                }
                return desiredThreadCount;            
            } else {
                return setThreads(1);
            }
		}

        void workerLoop(int numThreads) {
            logLevel(3, "%d threads spawning.\r\n", numThreads);
            globalHistogram.clear();
            for(Histogram<PixelData>& hist : localHistograms) {
                hist.clear();
            }

            resetTotalIterations();
            static constexpr int batch_size = 10'000'000;
            
            #pragma omp parallel num_threads(numThreads) 
            {
                int id = omp_get_thread_num();
                logLevel(3, "Thread %d/%d reporting for duty!\r\n", id, numThreads);

                const int localBatch = batch_size / numThreads;

                while(running) {    
                    for(int i = 0; i < batch_size; i++) {
                        // if(!running) break; // early exit if stopped // this is not allowed in OpenMP for loops...
                        stepThread(id);
                    }
                    // moved merge to after running -- it will 
                    #pragma omp critical
                    globalHistogram.merge(localHistograms[id]);
                }
                

                
                if(!running) {
                    logLevel(3, "Thread %d exiting!\r\n", id);
                }
            }
            logLevel(3, "workerLoop:  parallel block exited.\r\n");
        }
};       