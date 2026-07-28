#pragma once
#include "./types.h"
#include <atomic>
#include <random>


#ifndef FLAME_VARIATIONS
#include "./variations/standard.h"
#else
#include FLAME_VARIATIONS
#endif

// Abstract class for all Fractal Flame rendering engines 
// CPU, OpenMP, and CUDA implementations all inherit from this
// See /engines directory for each implementation (Serial, OpenMP, CUDA)

class Engine;

struct ColorState; 

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
		uint64_t targetIterations = 100'000'000; // 100M iterations
		std::atomic<bool> running = false;
		
		Viewport viewport;
		std::vector<Transform> transforms;
		
		bool hasFinalTransform = false;
		Transform finalTransform;
		
		Histogram<PixelData> globalHistogram;
		

		int seed = 0;
		
		void configChanged() {
			globalHistogram.clear();

			calculateWeight();
			recalculateColors();
		}

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
	
	
	public:
		virtual ~Engine() = default; // reset
		virtual void setup(int numThreads) = 0; // Allocate memory for threads to access -- histograms and coordinates
		virtual void start() = 0; // infinitely call step()
		virtual void stop() = 0; // stop infinitely calling step()
		virtual void reset() = 0; // go back to initial state -- empty histogram 
		virtual uint64_t getTotalIterations() = 0;// used for data -- frontend reads it, and it's "accurate enough" even if it's slightly wrong
		virtual uint64_t getMaxHits() = 0;
		virtual Coordinate stepNoPlot(Coordinate c, Xorshift64& rng) { return c; };
		
		virtual bool done() {
			return getTotalIterations() >= targetIterations;
		}

		void setTargetIterations(uint64_t iterations) {
			targetIterations = iterations;
		}

		

		// not very organized: abstract class is below, this desperately needs a documentation pass
		float getTotalWeight() {
			return totalWeight;
		}

		const Histogram<PixelData>& getHistogram() {
			return globalHistogram;
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
			if(running) stop();
			viewport = vp;
			configChanged();
		}
		
		void calculateViewport() {
			constexpr int viewportIterations = 1'000'000; 
			std::random_device rd;
			Xorshift64 rng((uint64_t)rd() << 32 | rd());
			double x = 1 - 2 * rng.getDouble();
			double y = 1 - 2 * rng.getDouble();
			double color = rng.getDouble();
			Coordinate viewportThreadCoord = {x, y, color};
			std::vector<double> xVector, yVector;
			xVector.reserve(viewportIterations); // 1 million x points - reserve so it doesn't slow down to heap allocate
			yVector.reserve(viewportIterations); // 1 million y points
			// discard color -- viewport is colorblind :(

			for(int i = 0; i < viewportIterations; i++) { // 100k
				viewportThreadCoord = this->stepNoPlot(viewportThreadCoord, rng);
				if(std::isfinite(viewportThreadCoord.x) && std::isfinite(viewportThreadCoord.y)) { // not NaN
					xVector.push_back(viewportThreadCoord.x);
					yVector.push_back(viewportThreadCoord.y);
				}
			}

			std::sort(xVector.begin(), xVector.end());
			std::sort(yVector.begin(), yVector.end());

			double cutoff = 0.005; // user changable "zoom" in the future?
			size_t low_x = xVector.size() * cutoff;
			size_t high_x = xVector.size() * (1.0 - cutoff);

			size_t low_y = yVector.size() * cutoff;
			size_t high_y = yVector.size() * (1.0 - cutoff);

			setViewport(Viewport(xVector[low_x], xVector[high_x], yVector[low_y], yVector[high_y]));
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
			if(running) stop();
			finalTransform = final;
			hasFinalTransform = true;
			configChanged();
		}

		void clearFinalTransform(void) {
			if(running) stop();
			if(!hasFinalTransform) return; // don't clear if hasFinalTransform is false already
			finalTransform = Transform();
			
			
			hasFinalTransform = false;
			configChanged();
		}

		bool getStatus() {
			return running;
		}
		

		void resize(int width, int height) {
			if(running) stop();
			globalHistogram.resize(width, height);
		}

		/** might refactor with Doxygen next -- I like good documentation, and this entire project lacks that
		// @brief fills pixels[] with RGBA data
		// @param pixels must point to a buffer of at least Engine*->getWidth() * Engine*->getHeight() * 4 bytes
		// @param UIState.color.palette must have (paletteSize * 3 bytes per color) bytes allocated
		// @returns Returns true if buffer changed, false if no change
		*/
		bool fillPixelBuffer(uint8_t* pixels, const Color* palette, int paletteSize, double gamma, const Camera &view) { // function to calculate colors for texture, returns false if no change
			if(!getStatus()) return false; // if it isn't running, then the buffer is not changing
			
			static uint64_t lastMaxVal = 0;
			uint64_t maxVal = 0;
			int width = globalHistogram.getWidth();
			int height = globalHistogram.getHeight();

			for(int i = 0; i < width*height; i++) {
				if(globalHistogram[i].hits > maxVal) {
					maxVal = globalHistogram[i].hits;
				}
			}
			if(maxVal == 0) return false; // don't render when nothing has generated yet
			if(lastMaxVal == maxVal) return false; // only render when maxVal increases; since it follows a power law, one histogram pixel should be hit WAY more than the rest
						
			double logMax = std::log(1.0f + (double)maxVal); // convert uint64_t to double, with 1.0 because log(0) is undefined
			
			static std::vector<double> logTable;

			if(maxVal != lastMaxVal) {
				logTable.resize(maxVal + 1);
				for(uint64_t i = lastMaxVal + 1; i <= maxVal; i++) {
					logTable[i] = std::log((double) i); // extend log table, dividing by brightness at render time
				}
			}

			// for(int i = 0; i < width*height; i++) {
			// 	// 8 bits per pixel
			// 	// logarithmically map brightness
			// 	if(globalHistogram.get(i).hits == 0) {
			// 		pixels[i*4+0] = 0;  
			// 		pixels[i*4+1] = 0;  
			// 		pixels[i*4+2] = 0;  
			// 		pixels[i*4+3] = 255; // alpha of 255 is fully opaque
			// 		continue;
			// 	} // untouched pixels are black
			// 	PixelData currentPixel = globalHistogram.get(i);
			// 	// double brightness = std::log(currentPixel.hits) / logMax; // log of hist[i] for every index of the entire histogram
			// 	// one million log() operations per frame (oof!) [lookup table?]
			// 	uint64_t clamped = std::min(currentPixel.hits, maxVal); // algorithm is my favorite library
			// 	double brightness = logTable[clamped] / logMax;

			// 	// dividing by the max value makes the max 1 and the rest a number between 0 and 1
			// 	brightness = std::pow(brightness, 1.0/gamma); // gamma correction
			// 	double colorCoord = currentPixel.color / static_cast<double>(currentPixel.hits); 
			// 	int paletteIndex = (int)(colorCoord * (paletteSize - 1));
			// 	pixels[i*4+0] = (uint8_t)(palette[paletteIndex].r * brightness); // red  
			// 	pixels[i*4+1] = (uint8_t)(palette[paletteIndex].g * brightness); // green
			// 	pixels[i*4+2] = (uint8_t)(palette[paletteIndex].b * brightness); // blue
			// 	pixels[i*4+3] = 255; // alpha of 255 is fully opaque
			// }

			for(int pixelX = 0; pixelX < width; pixelX++) {
				for(int pixelY = 0; pixelY < height; pixelY++) {
					// normalized [0,1]
					double normalizedX = (double)pixelX / width;
					double normalizedY = (double)pixelY / height;
					
					// apply camera: center offset and zoom
					double histogramX = (normalizedX - 0.5) / view.zoom + view.centerX;
					double histogramY = (normalizedY - 0.5) / view.zoom + view.centerY;
					
					// back to histogram bin
					int binX = (int)(histogramX * width);
					int binY = (int)(histogramY * height);
					
					if(binX < 0 || binX >= width || binY < 0 || binY >= height) {
						// out of bounds -- black
						pixels[(pixelY * width + pixelX)*4 + 0] = 0;
						pixels[(pixelY * width + pixelX)*4 + 1] = 0;
						pixels[(pixelY * width + pixelX)*4 + 2] = 0;
						pixels[(pixelY * width + pixelX)*4 + 3] = 255;
					} else {
						int binIndex = binY * width + binX;
						PixelData currentPixel = globalHistogram[binIndex];

						// sample histogram at binIndex

						uint64_t logTableSize = (uint64_t)(logTable.size() - 1); // stops render thread from overtaking logTable thread, clamping it

						double brightness = logTable[std::min(currentPixel.hits, logTableSize)] / logMax;

						// dividing by the max value makes the max 1 and the rest a number between 0 and 1
						brightness = std::pow(brightness, 1.0/gamma); // gamma correction
						double colorCoord = std::min(1.0, currentPixel.color / static_cast<double>(currentPixel.hits)); 
						
						int paletteIndex = (int)(colorCoord * (paletteSize - 1));
						pixels[(pixelY * width + pixelX)*4 + 0] = (uint8_t)(palette[paletteIndex].r * brightness); // red  
						pixels[(pixelY * width + pixelX)*4 + 1] = (uint8_t)(palette[paletteIndex].g * brightness); // green
						pixels[(pixelY * width + pixelX)*4 + 2] = (uint8_t)(palette[paletteIndex].b * brightness); // blue
						pixels[(pixelY * width + pixelX)*4 + 3] = 255; // alpha of 255 is fully opaque
					}
				}
			}


			// buffer done filling
			lastMaxVal = maxVal;
			return true;
		}

		// void applySettings() {

		// }
		
		void removeTransform(int index) {
			logLevel(LOG_CHATTER, "removeTransform called\r\n");
			if(running) stop();
			this->transforms.erase(transforms.begin() + index);
			configChanged();
		}

		void addTransform(Transform transform) {
			logLevel(LOG_CHATTER, "addTransform called\r\n");
			if(running) stop();
			this->transforms.push_back(transform); // add the transform
			configChanged();
		}

		void setTransforms(std::vector<Transform> transforms) {
			logLevel(LOG_CHATTER, "setTransforms called with %d transforms\r\n", (int)transforms.size());
			if(running) stop();
			this->transforms = transforms; // set the transforms
			configChanged();
		}

		void setTransform(int index, Transform transform) {
			logLevel(LOG_CHATTER, "Transform %d changed!\r\n%s", index, transform.toString(index));
			if(running) stop();
			this->transforms[index] = transform;
			configChanged();
		}

		int randomize() {
            logLevel(LOG_CHATTER, "Randomize called!\r\n");
			std::random_device entropyGenerator;
            int randomSeed = (int) entropyGenerator();
			return randomize(randomSeed);
		}

		int randomize(int randomSeed) {
			std::uniform_int_distribution<int> transformCountDist(2, 8);

			std::mt19937 rng(randomSeed);
            int numRandTransforms = transformCountDist(rng);
            logLevel(LOG_CHATTER, "seed is %d, numTransforms = %d\r\n", randomSeed, numRandTransforms);

			std::vector<VariationDef> allowedVariations = getSupportedVariations(); // all for now -- might make checkboxes
			
			

			std::uniform_real_distribution<float> weightDist(0.5f, 2.0f);
			std::uniform_real_distribution<float> colorDist(0, 1);
			std::uniform_real_distribution<double> affineDist(-1.0, 1.0);
			std::uniform_real_distribution<double> parametricDist(-3.0, 3.0);
			
			std::uniform_int_distribution<int> varRandIndex(0, allowedVariations.size() - 1); //
			std::uniform_int_distribution<uint8_t> RGBColor(180, 255);
			

			std::vector<Transform> randomTransforms;
			for(int i = 0; i < numRandTransforms; i++) {
				int transformIndex = varRandIndex(rng);

				Transform t;
				
				t.weight = weightDist(rng);
				t.color = colorDist(rng);
				
				t.variation.index = allowedVariations[transformIndex].index; 	// coded like this so I can easily change allowedVariations
				t.variation.name = allowedVariations[transformIndex].name;		// right now it's all of them, but eventually i will implement tags

				t.coeffs = Affine(affineDist(rng), affineDist(rng), affineDist(rng), affineDist(rng), affineDist(rng), affineDist(rng));
				t.parametric = Parametric(parametricDist(rng), parametricDist(rng), parametricDist(rng), parametricDist(rng)); // all transforms get 4 random parametric coeffs, even if they don't use them
				randomTransforms.push_back(t); 
			}

			setTransforms(randomTransforms);
			calculateViewport();
			return rng();
		}
	
	
};

inline void EngineState::applyPreset(std::unique_ptr<Engine>& fractal_engine) { // inline or else the compiler complains
	fractal_engine->applyPreset(EngineState{seed, viewport, transforms, hasFinalTransform, finalTransform});
}


std::unique_ptr<Engine> createSerialEngine();
std::unique_ptr<Engine> createOpenMPEngine();

#ifdef HAS_CUDA // My laptop does not have CUDA
std::unique_ptr<Engine> createCUDAEngine();
#endif


// Transform Variations //
// Fractal flame variations based on a paper by Scott Draves & Erik Reckase: 
// The Fractal Flame Algorithm
// https://flam3.com/flame_draves.pdf

// Tbh I could have read the paper more in-depth -- I really didn't read it past the pseudocode, and that was kind of the point
// I explored the concept myself, learning about IFS first, and then expanded my code
// I struggled to add the log(), and then when it finally worked, I was genuinely shocked by the 3d effect
// It was different to see the effect so simple in the formerly obvious 2D geometry versus with post-processing bloom + blur
// I gasped when I ran it and it was so clear -- I will definitely make my post-processing optional.


// Variations

// TODO: move some state stuff to different cpp file, to reduce clutter in engine

struct Preset;

struct ColorState {
    int numColors = 128; // const
    std::unique_ptr<Color[]> palette;
    std::vector<Color> funcColors;
    std::vector<float> floats; // used for ColorEdit3
    double gamma = 2.2;

    ColorState() {
        palette = std::make_unique<Color[]>(numColors);
    }

    void add(Color color) { // appends for now
        funcColors.push_back(color);
        buildPalette();
    }

    void removeFunc(int index) {
        funcColors.erase(funcColors.begin() + index);
        buildPalette();

        floats.erase(floats.begin() + 3*index, floats.begin() + 3*index+3);
    }

    void resizeVectors(int numTransforms) {
        funcColors.resize(numTransforms);
        floats.resize(numTransforms*3);
    }

    void clearPalette() {
        for(int i = 0; i < numColors; i++) {
            palette[i] = Color(); // set entire array to black
        }
    }

    void randomizeColors(int numTransforms, int seed) {
        std::uniform_real_distribution<double> satDist(0.7f, 1.0f);
		std::uniform_real_distribution<double> valDist(0.7f, 1.0f);
		std::mt19937 rng(seed); // pseudorandom -- good enough
		funcColors.resize(numTransforms);
		for(int i = 0; i < numTransforms; i++) {
			double hue = (double)i / numTransforms; // evenly spaced hues [0, 1]
			double saturation = satDist(rng);
			double value = valDist(rng);
			funcColors[i] = Color::hsvToRGB(hue, saturation, value);
            logLevel(LOG_CHATTER, "funcColors[i] = {%u, %u, %u}\r\n", funcColors[i].r, funcColors[i].g, funcColors[i].b);
		}
		buildPalette();
    }

    void buildPalette() {
        clearPalette();
        buildPalette(numColors);
    }

    void buildPalette(int numColors) {
        palette = std::make_unique<Color[]>(numColors);
        logLevel(LOG_CHATTER, "There are %ld functions.\r\n", funcColors.size());
        std::vector<Color> lerpColors;
        
        if(funcColors.size() == 0) {
            lerpColors.push_back(Color());
            lerpColors.push_back(Color(255, 255, 255));
        } else if(funcColors.size() == 1) {
            lerpColors.push_back(Color());
            lerpColors.push_back(funcColors[0]);
        } else { // size 2 or greater
            lerpColors = funcColors;
        }

        for(int i = 0; i < numColors; i++) {
            double t = (double)i / (numColors - 1); // t is the lerp amount
            double segmentSize = 1.0 / (lerpColors.size() - 1); // size - 1 -> # of splits, all same size
            size_t segment = static_cast<size_t>(t / segmentSize);
            if(segment > lerpColors.size() - 2) {
                segment = lerpColors.size() - 2;
            }

            double localT = (t - segment * segmentSize) / segmentSize; // now lerp within the segment
            Color& a = lerpColors[segment]; // color to lerp from
            Color& b = lerpColors[segment + 1]; // color to lerp to

            palette[i].r = static_cast<uint8_t>(a.r + localT * (b.r - a.r));
            palette[i].g = static_cast<uint8_t>(a.g + localT * (b.g - a.g));
            palette[i].b = static_cast<uint8_t>(a.b + localT * (b.b - a.b));
        }

        logLevel(LOG_CHATTER, "Palette made!\r\n");
        
        floats.resize(funcColors.size()*3);
        for(size_t i = 0; i < funcColors.size(); i++) {
            floats[3*i+0] = funcColors[i].r / 255.0; 
            floats[3*i+1] = funcColors[i].g / 255.0;
            floats[3*i+2] = funcColors[i].b / 255.0;
        }
        logLevel(LOG_CHATTER, "Floats changed!\r\n");
    }

    const Color* getPalettePtr() {
        return palette.get();
    }

    ~ColorState() {
        clearPalette();
    }
    
};