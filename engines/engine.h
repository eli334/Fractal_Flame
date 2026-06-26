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
#include <atomic> // for parallel -> serial addition
#include <algorithm>

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

	
	// void randomAffine(std::mt19937 &rng) {

	// }

	std::string formattedOutput = "";
	const char* toString() {
		std::stringstream stream;
		stream << std::fixed << std::setprecision(2);
		stream << "Affine coefficients: ( " << a << "x + " << b << "y + " << c;
		stream <<                      ", " << d << "x + " << e << "y + " << f << ")\r\n";
		formattedOutput = stream.str();
		return formattedOutput.c_str();
	}

	std::vector<double*> toPtrVector() {
		return {&a, &b, &c, &d, &e, &f};
	}

};

struct VariationDef {
	int index = 0;

	// next todo: make this std::string instead of const char*
	// ECE 0301/0302 kind of made me a worse coder :(
	std::string name = "Identity";
	std::string formula = "(x, y)";

	std::string formattedOutput = ""; // heap-allocated -- stored with struct so I don't have to think about
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


struct PixelData{
	uint64_t hits = 0;
	double color = 0;
};

template<typename T> // PixelData
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
				return data[0];
			}
		}

		Histogram(const Histogram&) = delete; // don't want or need copy constructor
		Histogram& operator=(const Histogram&) = delete; // don't need assignment
};

struct Color { // pixel colors
	uint8_t r = 0, g = 0, b = 0; // a = 255 -- removed since it kind of broke ImGui
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
			case 3: return r; // just in case -- should only ever be 0, 1, or 2
		}
	}

	static Color hsvToRGB(double hue, double saturation, double value) {
		float red, green, blue;
		int angle = (int)(hue * 6);
		float fraction = hue * 6 - angle;
		float low = value * (1 - saturation);
		float rising = value * (1 - fraction * saturation);
		float falling = value * (1 - (1 - fraction) * saturation);
		switch(angle % 6) {
			case 0: red = value; green = falling; blue = low; break;
			case 1: red = rising; green = value; blue = low; break;
			case 2: red = low; green = value; blue = falling; break;
			case 3: red = low; green = rising; blue = value; break;
			case 4: red = falling; green = low; blue = value; break;
			case 5: red = value; green = low; blue = rising; break;
			default: red = green = blue = 0;
		}
		return Color((uint8_t)(red * 255), (uint8_t)(green * 255), (uint8_t)(blue * 255));
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
		std::vector<uint64_t> totalHits{1};

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
		virtual void reset() = 0; // go back to initial state -- empty histogram 
		virtual uint64_t getTotalIterations() = 0;// used for data -- frontend reads it, and it's "accurate enough" even if it's slightly wrong

		// not very organized: abstract class is below, this desperately needs a documentation pass
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
			
			// printf("Starting upload...\r\n");
			
			double logMax = std::log(1.0f + (double)maxVal); // convert uint64_t to double, with 1.0 because log(0) is undefined
			
			static std::vector<double> logTable;

			if(maxVal != lastMaxVal) {
				logTable.resize(maxVal + 1);
				for(uint64_t logTableIndex = 1; logTableIndex <= maxVal; logTableIndex++) {
					logTable[logTableIndex] = std::log((double) logTableIndex) / logMax; // recreate the logTable when 
				}
			}

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
				// double brightness = std::log(currentPixel.hits) / logMax; // log of hist[i] for every index of the entire histogram
				// one million log() operations per frame (oof!) [lookup table?]
				uint64_t clamped = std::min(currentPixel.hits, maxVal); // algorithm is my favorite library
				double brightness = logTable[clamped];

				// dividing by the max value makes the max 1 and the rest a number between 0 and 1
				// brightness = std::pow(brightness, 1.0/2.2); // gamma correction
				double colorCoord = currentPixel.color / static_cast<double>(currentPixel.hits); 
				int paletteIndex = (int)(colorCoord * (paletteSize - 1));
				pixels[i*4+0] = (uint8_t)(palette[paletteIndex].r * brightness); // red  
				pixels[i*4+1] = (uint8_t)(palette[paletteIndex].g * brightness); // green
				pixels[i*4+2] = (uint8_t)(palette[paletteIndex].b * brightness); // blue
				pixels[i*4+3] = 255; // alpha of 255 is fully opaque
			}
			// buffer done filling
			lastMaxVal = maxVal;
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
				Coordinate viewportThreadCoord = getStartingCoordinate();
				std::vector<double> xVector, yVector;
				xVector.reserve(1000000); // 1 million x points - reserve so it doesn't slow down to heap allocate
				yVector.reserve(1000000); // 1 million y points
				// discard color -- viewport is colorblind :(


				for(int i = 0; i < 100'000; i++) { // 100k
					viewportThreadCoord = stepNoPlot(viewportThreadCoord);
					if(std::isfinite(viewportThreadCoord.x) && std::isfinite(viewportThreadCoord.y)) { // not NaN
						xVector.push_back(viewportThreadCoord.x);
						yVector.push_back(viewportThreadCoord.y);
					}
				}

				std::sort(xVector.begin(), xVector.end());
				std::sort(yVector.begin(), yVector.end());

				double cutoff = 0.01; // user changable "zoom" in the future?
				size_t low_x = xVector.size() * cutoff;
				size_t high_x = xVector.size() * (1.0 - cutoff);

				size_t low_y = yVector.size() * cutoff;
				size_t high_y = yVector.size() * (1.0 - cutoff);

				setViewport(Viewport(xVector[low_x], xVector[high_x], yVector[low_y], yVector[high_y]));
			});
			t.join(); // join thread so it runs destructor SYNCHRONOUSLY when deallocated -- avoids dangling pointers
		}
		
		int randomize() {
            printf("Randomize called!\r\n");
			std::random_device entropyGenerator;
            int randomSeed = (int) entropyGenerator();
			printf("seed is %d", randomSeed);
            printf("random_device entropy: %.3f\r\n", entropyGenerator.entropy());
			return randomize(randomSeed);
		}

		int randomize(int randomSeed) {
			std::uniform_int_distribution<int> transformCountDist(2, 8);

			std::mt19937 rng(randomSeed);
            int numRandTransforms = transformCountDist(rng);
            printf("seed is %d, numTransforms = %d\r\n", randomSeed, numRandTransforms);

			std::vector<VariationDef> allowedVariations = getSupportedVariations(); // all for now -- might make checkboxes
			
			

			std::uniform_real_distribution<float> weightDist(0.5f, 2.0f);
			std::uniform_real_distribution<float> colorDist(0, 1);
			std::uniform_real_distribution<double> affineDist(-1.0, 1.0);
			std::uniform_int_distribution<int> varRandIndex(0, allowedVariations.size() - 1); //
			std::uniform_int_distribution<uint8_t> RGBColor(180, 255);

			std::vector<Transform> randomTransforms;
			for(int i = 0; i < numRandTransforms; i++) {
				Transform t;
				t.weight = weightDist(rng);
				t.color = colorDist(rng);
				t.variation.index = allowedVariations[varRandIndex(rng)].index; // coded like this so I can easily change allowedVariations
																				//  right now it's all of them, but eventually i will limit the user to some (idk if all of them would be valid for each implementation)
				t.coeffs = Affine(affineDist(rng), affineDist(rng), affineDist(rng), affineDist(rng), affineDist(rng), affineDist(rng));
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
std::unique_ptr<Engine> createOpenMPEngine(int threadCount);


// Engine Variations //
// Fractal flame variations based on a paper by Scott Draves & Erik Reckase: 
// The Fractal Flame Algorithm

// Tbh I could have read the paper more in-depth -- I really didn't read it, and that was kind of the point
// I explored the concept myself, learning about IFS first, and then expanded my code
// I struggled to add the log(), and then when it finally worked, I was genuinely shocked by the 3d effect
// It was different to see the effect so simple in the geometry versus with post-processing
// I gasped when I ran it and it was so clear -- I will definitely make my post-processing optional.


inline Coordinate variation_identity(Coordinate c) { // Variation 00
    return c;
}

inline Coordinate variation_sinusoidal(Coordinate c) { // Variation 01
    return {sin(c.x), sin(c.y)};
}

inline Coordinate variation_spherical(Coordinate c) { // Variation 02
    double r2 = c.x*c.x + c.y*c.y;
    return {c.x / r2, c.y / r2};
}

inline Coordinate variation_swirl(Coordinate c) { // Variation 03
    double r2 = (c.x*c.x) + (c.y*c.y);
    return {c.x * sin(r2) - c.y * cos(r2), c.x * sin(r2) + c.y * cos(r2)};
}

inline Coordinate variation_horseshoe(Coordinate c) { // Variation 04
    return {};
}

inline Coordinate variation_polar(Coordinate c) { // Variation 05
    return {};
}

inline Coordinate variation_handkerchief(Coordinate c) { // Variation 06
    return {};
}

inline Coordinate variation_heart(Coordinate c) { // Variation 07
    return {};
}

inline Coordinate variation_disc(Coordinate c) { // Variation 08
    return {};
}

inline Coordinate variation_spiral(Coordinate c) { // Variation 09
    return {};
}

inline Coordinate variation_hyperbolic(Coordinate c) { // Variation 10
    return {};
}

inline Coordinate variation_diamond(Coordinate c) { // Variation 11
    return {};
}

inline Coordinate variation_ex(Coordinate c) { // Variation 12
    return {};
}

inline Coordinate variation_julia(Coordinate c) { // Variation 13
    double r = hypot(c.x*c.x, c.y*c.y); // r = sqrt(x^2 + y^2) -- distance formula --> std::hypot because --march=native flag means it will use my CPU's specific math capabilities in the silicon -- otherwise it will be probably optimized down to the same exact thing anyway because the compiler is smarter than me
    double sqrtr = sqrt(r);
    double theta = atan2(c.y, c.x);

    // I need a random, 50/50 number.  I don't want to spawn an RNG number or feed one in or anything -- I need a source of noise
    // I am going to consider the c.x coordinate's mantissa bits to be random noise. I can memcpy a double to a uint64_t:
    uint64_t rawDoubleBits;
    memcpy(&rawDoubleBits, &c.x, 8);
    // If there is a better source of fast noise, I would gladly fix that line 

    bool omegaIsPi = rawDoubleBits & 1; // get the LSB of the mantissa -- seems like it should be random to me, and the paper doesn't specify what random means.

    double omega = (omegaIsPi ? M_PI : 0); // cool and awesome ternary operator

    return {sqrtr * (cos(theta / 2 + omega)),sqrtr * (sin(theta / 2 + omega))};
}

inline Coordinate variation_bent(Coordinate c) { // Variation 14
    return {};
}

inline Coordinate variation_waves(Coordinate c) { // Variation 15
    return {};
}

inline Coordinate variation_fisheye(Coordinate c) { // Variation 16
    return {};
}

inline Coordinate variation_popcorn(Coordinate c) { // Variation 17
    return {};
}

inline Coordinate variation_exponential(Coordinate c) { // Variation 18
    return {};
}

inline Coordinate variation_power(Coordinate c) { // Variation 19
    return {};
}

inline Coordinate variation_cosine(Coordinate c) { // Variation 20
    return {};
}

inline Coordinate variation_rings(Coordinate c) { // Variation 21
    return {};
}

inline Coordinate variation_fan(Coordinate c) { // Variation 22
    return {};
}

inline Coordinate variation_blob(Coordinate c) { // Variation 23
    return {};
}

inline Coordinate variation_pdj(Coordinate c) { // Variation 24
    return {};
}

inline Coordinate variation_fan2(Coordinate c) { // Variation 25
    return {};
}

inline Coordinate variation_rings2(Coordinate c) { // Variation 26
    return {};
}

inline Coordinate variation_eyefish(Coordinate c) { // Variation 27
    return {};
}

inline Coordinate variation_bubble(Coordinate c) { // Variation 28
    return {};
}

inline Coordinate variation_cylinder(Coordinate c) { // Variation 29
    return {};
}

inline Coordinate variation_perspective(Coordinate c) { // Variation 30
    return {};
}

inline Coordinate variation_noise(Coordinate c) { // Variation 31
    return {};
}

inline Coordinate variation_julian(Coordinate c) { // Variation 32
    return {};
}

inline Coordinate variation_juliascope(Coordinate c) { // Variation 33
    return {};
}

inline Coordinate variation_blur(Coordinate c) { // Variation 34
    return {};
}

inline Coordinate variation_gaussian(Coordinate c) { // Variation 35
    return {};
}

inline Coordinate variation_radialblur(Coordinate c) { // Variation 36
    return {};
}

inline Coordinate variation_pie(Coordinate c) { // Variation 37
    return {};
}

inline Coordinate variation_ngon(Coordinate c) { // Variation 38
    return {};
}

inline Coordinate variation_curl(Coordinate c) { // Variation 39
    return {};
}

inline Coordinate variation_rectangles(Coordinate c) { // Variation 40
    return {};
}

inline Coordinate variation_arch(Coordinate c) { // Variation 41
    return {};
}

inline Coordinate variation_tangent(Coordinate c) { // Variation 42
    return {};
}

inline Coordinate variation_square(Coordinate c) { // Variation 43
    return {};
}

inline Coordinate variation_rays(Coordinate c) { // Variation 44
    return {};
}

inline Coordinate variation_blade(Coordinate c) { // Variation 45
    return {};
}

inline Coordinate variation_secant(Coordinate c) { // Variation 46
    return {};
}

inline Coordinate variation_twintrian(Coordinate c) { // Variation 47
    return {};
}

inline Coordinate variation_cross(Coordinate c) { // Variation 48
    return {};
}

#ifdef HAS_CUDA
std::unique_ptr<Engine> createCUDAEngine();
#endif