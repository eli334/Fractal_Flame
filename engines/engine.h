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
#include <math.h> // log() in frontend
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
		stream << "Affine coefficients: \r\n( " << a << "x + " << b << "y + " << c;
		stream <<                          ", " << d << "x + " << e << "y + " << f << ")\r\n";
		formattedOutput = stream.str();
		return formattedOutput.c_str();
	}

	std::vector<double*> toPtrVector() {
		return {&a, &b, &c, &d, &e, &f};
	}

};

struct VariationDef {
	int index = 0;

	std::string name = "Identity";
	// std::string formula = "(x, y)";

	std::string formattedOutput = ""; // heap-allocated -- stored with struct so I don't have to think about
	const char* toString() {
		std::stringstream stream;
		stream << "Variation: " << name << " (" << index << ")\r\n";
		// stream << "Formula: " << formula << "\r\n";
		formattedOutput = stream.str();
		return formattedOutput.c_str();
	}
};

// Parametric struct for Transform

struct Parametric {
	double p1 = 1, p2 = 1, p3 = 1, p4 = 1;

	Parametric() = default;

	Parametric(double a, double b, double c, double d) {
		p1 = a;
		p2 = b;
		p3 = c;
		p4 = d;
	}	

	std::string formattedOutput = "";
	const char* toString() {
		std::stringstream stream;
		stream << std::fixed << std::setprecision(2);
		stream << "Parametric coefficients: \r\np1 = " << p1 << ", p2 = " << p2 << ", p3 = " << p3 << ", p4 = " << p4 << "\r\n";
		formattedOutput = stream.str();
		return formattedOutput.c_str();
	}
};


struct Transform {
	double weight = 1.0; // sumOfWeights / transform_i.weight = probability that transform_i gets picked
	double color = 0; // c_i in paper -- [0, 1]
	VariationDef variation;
	Affine coeffs; // affine coefficients - identity matrix by default
	Parametric parametric; // always include, set when setting functions

	std::string formattedOutput = "";
	const char* toString(int index) {
		std::stringstream stream;
		stream << std::fixed << std::setprecision(3);
		stream << "Transform " << index + 1 << ":\r\n";
		stream << "Weight: " << weight << "; Color: " << color << "\r\n";
		stream << variation.toString();
		stream << coeffs.toString();
		stream << parametric.toString();
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

// xorshift64 -- fast, pseudorandom number generator

struct Xorshift64 {
	Xorshift64() = delete;
	Xorshift64(uint64_t seed) {
		state = seed;
		if(state == 0) {		   // Breaks if seeded with 0, so.. don't allow it.
			state = 0xBADBEEFBADBEEFBULL; // bad beef bull -- random data for my xorshift64 seed
		}
	}

	uint64_t state;
	uint64_t next() {
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;
		return state;
	}

	double getDouble() {
        return (next() >> 11) * (1.0 / (1ULL << 53)); // discards bottom 11 bits, making it a uint64_t from [0, 2^53-1]
    }												  // then, multiplies by (1/2^53), mapping it from [0, 1) -- this is effectively [0, 1]
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
		std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
		std::chrono::system_clock::time_point startWallTime = std::chrono::system_clock::now();
		
		void recordStartTime() {
			startTime = std::chrono::steady_clock::now();
			startWallTime = std::chrono::system_clock::now();
    	}
		
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
		virtual void step(Coordinate c, int threadIndex, Xorshift64 &rng) = 0; // iterate coordinate once   
		virtual Coordinate stepNoPlot(Coordinate c, Xorshift64 &rng) = 0;
		virtual ~Engine() = default; // reset
		
		virtual std::vector<VariationDef> getSupportedVariations() = 0;
		
		virtual void setup(int numThreads, uint64_t seed) = 0; // Spawn the thread, pause
		virtual Coordinate getStartingCoordinate(int threadIndex) = 0;
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
			if(running) stop();
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
		
		Coordinate getCurrent(int index) {
			size_t correctIndexType = (size_t) index; // oh, C++
			if(correctIndexType > threadCoords.size()) return Coordinate(); // if OOB, just return (0, 0) - i don't even use getCurrent
			return threadCoords[correctIndexType];
		}

		const Histogram<PixelData>* getHistogram() { // const makes this read-only
			return &globalHistogram;
		}

		void resize(int width, int height) {
			if(running) stop();
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
			if(running) stop();
			this->transforms.erase(transforms.begin() + index);
			configChanged();
		}

		void addTransform(Transform transform) {
			printf("addTransform called\r\n");
			if(running) stop();
			this->transforms.push_back(transform); // add the transform
			configChanged();
		}

		void setTransforms(std::vector<Transform> transforms) {
			// printf("setTransforms called with %d transforms\r\n", (int)transforms.size());
			if(running) stop();
			this->transforms = transforms; // set the transforms
			configChanged();
		}

		void setTransform(int index, Transform transform) {
			printf("Transform %d changed!\r\n%s", index, transform.toString(index));
			if(running) stop();
			this->transforms[index] = transform;
			configChanged();
		}

		void calculateViewport() {
			std::random_device rd;
			Xorshift64 rng((uint64_t)rd() << 32 | rd());
			double x = 1 - 2 * rng.getDouble();
			double y = 1 - 2 * rng.getDouble();
			double color = rng.getDouble();
			Coordinate viewportThreadCoord = {x, y, color};
			std::vector<double> xVector, yVector;
			xVector.reserve(1000000); // 1 million x points - reserve so it doesn't slow down to heap allocate
			yVector.reserve(1000000); // 1 million y points
			// discard color -- viewport is colorblind :(


			for(int i = 0; i < 100'000; i++) { // 100k
				viewportThreadCoord = stepNoPlot(viewportThreadCoord, rng);
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
		
		int randomize() {
            printf("Randomize called!\r\n");
			std::random_device entropyGenerator;
            int randomSeed = (int) entropyGenerator();
			// printf("seed is %d\r\n", randomSeed);
            // printf("random_device entropy: %.3f\r\n", entropyGenerator.entropy());
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
std::unique_ptr<Engine> createOpenMPEngine(int threadCount);

#ifdef HAS_CUDA // My laptop does not have CUDA
std::unique_ptr<Engine> createCUDAEngine();
#endif


// Engine Variations //
// Fractal flame variations based on a paper by Scott Draves & Erik Reckase: 
// The Fractal Flame Algorithm
// https://flam3.com/flame_draves.pdf

// Tbh I could have read the paper more in-depth -- I really didn't read it, and that was kind of the point
// I explored the concept myself, learning about IFS first, and then expanded my code
// I struggled to add the log(), and then when it finally worked, I was genuinely shocked by the 3d effect
// It was different to see the effect so simple in the formerly obvious 2D geometry versus with post-processing bloom + blur
// I gasped when I ran it and it was so clear -- I will definitely make my post-processing optional.


// Variations

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
    double r = std::hypot(c.x, c.y); // hypot finds the distance between two points
	double inverseR = (1.0 / r);
	return {inverseR * (c.x - c.y) * (c.x + c.y), inverseR*2*c.x*c.y};
}

inline Coordinate variation_polar(Coordinate c) { // Variation 05
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	return {theta * M_1_PI,  r - 1}; // M_1_PI = math 1/pi
}

inline Coordinate variation_handkerchief(Coordinate c) { // Variation 06
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	return {r * (sin(theta + r)), r * (cos(theta - r))};
}

inline Coordinate variation_heart(Coordinate c) { // Variation 07
	double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
    return {r * sin(theta * r), r * -1 * cos(theta * r)};
}

inline Coordinate variation_disc(Coordinate c) { // Variation 08
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	double coeff = theta * M_1_PI;
	double x = 0, y = 0; 
	sincos(M_PI * r, &x, &y);
	return {coeff * x, coeff * y};
}

inline Coordinate variation_spiral(Coordinate c) { // Variation 09
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	double sinTheta, cosTheta, sinR, cosR;
	sincos(theta, &sinTheta, &cosTheta);
	sincos(r, &sinR, &cosR);
	double inverseR = (1.0/r);
	return {inverseR * (cosTheta + sinR), inverseR * (sinTheta - cosR)}; 
}

inline Coordinate variation_hyperbolic(Coordinate c) { // Variation 10
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	double sinTheta, cosTheta;
	sincos(theta, &sinTheta, &cosTheta);
	return {sinTheta / r, r * cosTheta};
}

inline Coordinate variation_diamond(Coordinate c) { // Variation 11
    double theta = atan2(c.y, c.x);              ////  <-- This block is repeated often.  I think it makes the math  	
	double r = std::hypot(c.x, c.y);			 ////      more legible to not put this into a function, and I save  	
	double sinTheta, cosTheta, sinR, cosR;		 ////      precious CPU cycles by not going to a function :P       		
	sincos(theta, &sinTheta, &cosTheta);		 ////      																
	sincos(r, &sinR, &cosR);                     ////																	
	return {sinTheta * cosR, cosTheta * sinR};   
}

inline Coordinate variation_ex(Coordinate c) { // Variation 12
	double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	
	double p0, p1;
	p0 = sin(theta + r);
	p1 = cos(theta - r);
    return {r * (p0*p0*p0 + p1*p1*p1), r * (p0*p0*p0 - p1*p1*p1)};
}

inline Coordinate variation_julia(Coordinate c, Xorshift64* rng) { // Variation 13
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y); // r = sqrt(x^2 + y^2) -- distance formula --> std::hypot because --march=native flag means it will use my CPU's specific math capabilities in the silicon -- otherwise it will be probably optimized down to the same exact thing anyway because the compiler is smarter than me
    double sqrtr = sqrt(r);
    
	double omega = 0;
	if(rng->next() >> 63) {
		omega = M_PI; // roughly 50/50
	}

    return {sqrtr * (cos(theta / 2 + omega)),sqrtr * (sin(theta / 2 + omega))};
}

inline Coordinate variation_bent(Coordinate c) { // Variation 14
	// (x, y) 		if x >= 0, y >= 0
	// (2x, y) 		if x < 0, y >= 0
	// (x, y/2) 	if x >= 0, y < 0
	// (2x, y/2) 	if x < 0, y < 0

	bool xLT0, yLT0;

	xLT0 = c.x < 0;
	yLT0 = c.y < 0;
	
	if(!xLT0 && !yLT0) { 	 // (x, y)
		return c;
	}
	else if(xLT0 && !yLT0) { // (2x, y)
		return {2 * c.x, c.y};
	}
	else if(!xLT0 && yLT0) { // (x, y/2)
		return {c.x, 0.5 * c.y};
	}
	else if(xLT0 && yLT0) { // (2x, y/2)
		return {2 * c.x, 0.5 * c.y};
	}
	else return c; // actually unreachable -- all options above are accounted for
}

inline Coordinate variation_waves(Coordinate c, const Affine* a) { // Variation 15
	return {c.x + a->b * sin(c.y / (a->c*a->f)), c.y + a->e * sin(c.x / (a->f*a->f))};
}

inline Coordinate variation_fisheye(Coordinate c) { // Variation 16
    double r = hypot(c.x, c.y);
	double coeff = 2.0 / (r + 1);
	return {coeff * c.y, coeff * c.x};
}

inline Coordinate variation_popcorn(Coordinate c, Affine* a) { // Variation 17
    double tan3x, tan3y;
	tan3x = tan(3 * c.x);
	tan3y = tan(3 * c.y);
	
	return {c.x + a->c * sin(tan3y), c.y + a->f * sin(tan3x)};
}

inline Coordinate variation_exponential(Coordinate c) { // Variation 18
    double coeff = exp(c.x - 1);
	return {coeff * cos(M_PI * c.y), coeff * sin(M_PI * c.y)};
}

inline Coordinate variation_power(Coordinate c) { // Variation 19
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	double sinTheta, cosTheta;
	sincos(theta, &sinTheta, &cosTheta);
	double coeff = pow(r, sinTheta);
	return {coeff * cosTheta, coeff * sinTheta};
}

inline Coordinate variation_cosine(Coordinate c) { // Variation 20
	double sinPiX, cosPiX;
    sincos(M_PI * c.x, &sinPiX, &cosPiX);
    return {cosPiX * cosh(c.y), -1 * sinPiX * sinh(c.y)};
}

inline Coordinate variation_rings(Coordinate c, Affine* a) { // Variation 21
    double theta = atan2(c.y, c.x);
    double r = std::hypot(c.x, c.y);
    double c2 = a->c * a->c;
    double t = fmod(r + c2, 2*c2) - c2 + r * (1 - c2); // fmod for float mod
    double sinTheta, cosTheta;
    sincos(theta, &sinTheta, &cosTheta);
    return {t * cosTheta, t * sinTheta};
}

inline Coordinate variation_fan(Coordinate c, Affine* a) { // Variation 22
    double theta = atan2(c.y, c.x);
    double r = std::hypot(c.x, c.y);
    double t = M_PI * a->c * a->c;
    double sinTheta, cosTheta;
    if(fmod(theta + a->f, t) > t / 2) {
        sincos(theta - t/2, &sinTheta, &cosTheta);
    } else {
        sincos(theta + t/2, &sinTheta, &cosTheta);
    }
    return {r * cosTheta, r * sinTheta};
}


// p1 = blob.high
// p2 = blob.low
// p3 = blob.waves
inline Coordinate variation_blob(Coordinate c, Parametric* p) { // Variation 23
    double theta = atan2(c.y, c.x);
    double r = std::hypot(c.x, c.y);
	double coeff = p->p2 + (p->p1 - p->p2) * .5 * (sin(p->p3 * theta) + 1);
	double sinTheta, cosTheta;
	sincos(theta, &sinTheta, &cosTheta);
	return {r * coeff * cosTheta, r * coeff * sinTheta};
}

// p1 = pdj.a
// p2 = pdj.b
// p3 = pdj.c
// p4 = pdj.d
inline Coordinate variation_pdj(Coordinate c, Parametric* p) { // Variation 24
    double x, y;
	x = sin(p->p1 * c.y) - cos(p->p2 * c.x);
	y = sin(p->p3 * c.x) - cos(p->p4 * c.y);
	return {x, y};
}

// p1 = fan2.x
// p2 = fan2.y
inline Coordinate variation_fan2(Coordinate c, Parametric* p) { // Variation 25
    double theta = atan2(c.y, c.x);
    double r = std::hypot(c.x, c.y);
    double p1 = M_PI * p->p1 * p->p1;
    double t = theta + p->p2 - p1 * trunc((2 * theta * p->p2) / p1);
    double sinTheta, cosTheta;
    if(t > p1 / 2) {
        sincos(theta - p1/2, &sinTheta, &cosTheta);
    } else {
        sincos(theta + p1/2, &sinTheta, &cosTheta);
    }
    return {r * sinTheta, r * cosTheta};
}

// p = rings2.val
inline Coordinate variation_rings2(Coordinate c, Parametric* p) { // Variation 26
    double theta = atan2(c.y, c.x);
    double r = std::hypot(c.x, c.y);
    double pVal = p->p1 * p->p1;
    double t = r - 2 * pVal * trunc((r + pVal) / (2 * pVal)) + r * (1 - pVal);
    double sinTheta, cosTheta;
    sincos(theta, &sinTheta, &cosTheta);
    return {t * sinTheta, t * cosTheta};
}

inline Coordinate variation_eyefish(Coordinate c) { // Variation 27
    double r = std::hypot(c.x, c.y);
    double coeff = 2.0 / (r + 1);
    return {coeff * c.x, coeff * c.y};
}

inline Coordinate variation_bubble(Coordinate c) { // Variation 28
    double r2 = c.x*c.x + c.y*c.y;
    double coeff = 4.0 / (r2 + 4);
    return {coeff * c.x, coeff * c.y};
}

inline Coordinate variation_cylinder(Coordinate c) { // Variation 29
    return {sin(c.x), c.y};
}

// p1 = perspective.angle
// p2 = perspective.dist
inline Coordinate variation_perspective(Coordinate c, Parametric* p) { // Variation 30
    double coeff = p->p2 / (p->p2 - c.y * sin(p->p1));
    return {coeff * c.x, coeff * c.y * cos(p->p1)};
}

// psi1, psi2 are random numbers [0, 1)
inline Coordinate variation_noise(Coordinate c, Xorshift64* rng) { // Variation 31
    double psi1 = rng->getDouble(), psi2 = rng->getDouble();
    return {psi1 * c.x * cos(2*M_PI*psi2), psi1 * c.y * sin(2*M_PI*psi2)};
}

// p1 = juliaN.power
// p2 = juliaN.dist
inline Coordinate variation_julian(Coordinate c, Parametric* p, Xorshift64* rng) { // Variation 32
    double phi = atan2(c.x, c.y);
    double r = std::hypot(c.x, c.y);
    double psi = rng->getDouble();
    double p3 = trunc(fabs(p->p1) * psi);
    double t = (phi + 2 * M_PI * p3) / p->p1;
    double coeff = pow(r, p->p2 / p->p1);
    double sinT, cosT;
    sincos(t, &sinT, &cosT);
    return {coeff * cosT, coeff * sinT};
}

// p1 = juliascope.power
// p2 = juliascope.dist
inline Coordinate variation_juliascope(Coordinate c, Parametric* p, Xorshift64* rng) { // Variation 33
    double phi = atan2(c.x, c.y);
    double r = std::hypot(c.x, c.y);
    double psi = rng->getDouble();
    double lambda = 0;

	if(rng->getDouble() > 0.5) {
		lambda = 1;
	} else {
		lambda = -1;
	}

    double p3 = trunc(fabs(p->p1) * psi);

    double t = (lambda * phi + 2 * M_PI * p3) / p->p1;
    double coeff = pow(r, p->p2 / p->p1);
    double sinT, cosT;
    sincos(t, &sinT, &cosT);
    return {coeff * cosT, coeff * sinT};
}

inline Coordinate variation_blur(Xorshift64* rng) { // Variation 34
    double phi1 = rng->getDouble();
    double phi2 = rng->getDouble();

	double sinPhi, cosPhi;
	sincos(2*M_PI * phi2, &sinPhi, &cosPhi);

	return {phi1 * cosPhi, phi1 * sinPhi};
}

inline Coordinate variation_gaussian(Xorshift64* rng) { // Variation 35
    double sum = rng->getDouble() + rng->getDouble() + rng->getDouble() + rng->getDouble() - 2;
	double phi5 = rng->getDouble();
	double sinPhi, cosPhi;
	sincos(2 * M_PI * phi5, &sinPhi, &cosPhi);
	return {sum * cosPhi, sum * sinPhi};
}

// p1 = radialBlur.angle
inline Coordinate variation_radialblur(Coordinate c, Parametric* p, Xorshift64* rng) { // Variation 36
	double phi = atan2(c.x, c.y);
    double r = std::hypot(c.x, c.y);
    double p1 = p->p1 * (M_PI / 2);
    double v36 = 1.0; // hardcoding as 1; see formula (v36 is not considered in my implementation -- effectively, how strongly does v36 mix with other variations?) 
	double t1 = v36 * (rng->getDouble()+rng->getDouble()+rng->getDouble()+rng->getDouble()-2); // v36 * sum of 4 psis -  2
    double cosP1, sinP1;
	sincos(p1, &sinP1, &cosP1);
	double t2 = phi + t1 * sinP1;
    double t3 = t1 * cosP1 - 1;
    double sinT2, cosT2;
    sincos(t2, &sinT2, &cosT2);

    return {(1.0/v36) * (r * cosT2 + t3 * c.x), (1.0/v36) * (r * sinT2 + t3 * c.y)};
}

// p1 = pie.slices
// p2 = pie.rotation
// p3 = pie.thickness
inline Coordinate variation_pie(Parametric* p, Xorshift64* rng) { // Variation 37
    double t1 = trunc(rng->getDouble() * p->p1 + .5);
	double t2 = p->p2 + (2 * M_PI / p->p1) * (t1 + rng->getDouble() * p->p3);
	double phi3 = rng->getDouble();
	double cosT2, sinT2;
	sincos(t2, &sinT2, &cosT2);
	return {phi3 *  cosT2, phi3 * sinT2};
}

// p1 = ngon.power
// p2 = ngon.sides
// p3 = ngon.corners
// p4 = ngon.circle
inline Coordinate variation_ngon(Coordinate c, Parametric* p) { // Variation 38
    double phi = atan2(c.x, c.y);
    double r = std::hypot(c.x, c.y);
    double p2 = 2 * M_PI / p->p2;
    double t3 = phi - p2 * floor(phi / p2);
	double t4;
	if(t3 > p2 / 2) {
		t4 = t3;
	} else {
		t4 = t3 - p2;
	}
    double k = (p->p3 * (1.0/cos(t4) - 1) + p->p4) / pow(r, p->p1);
    return {k * c.x, k * c.y};
}

// p1 = curl.c1
// p2 = curl.c2
inline Coordinate variation_curl(Coordinate c, Parametric* p) { // Variation 39
    double t1 = 1 + p->p1 * c.x + p->p2 * (c.x*c.x - c.y*c.y);
    double t2 = p->p1 * c.y + 2 * p->p2 * c.x * c.y;
    double coeff = 1.0 / (t1*t1 + t2*t2);
    return {coeff * (c.x*t1 + c.y*t2), coeff * (c.y*t1 - c.x*t2)};
}

// p1 = rectangles.x
// p2 = rectangles.y
inline Coordinate variation_rectangles(Coordinate c, Parametric* p) { // Variation 40
    return {(2*floor(c.x/p->p1) + 1)*p->p1 - c.x, (2*floor(c.y/p->p2) + 1)*p->p2 - c.y};
}

inline Coordinate variation_arch(Xorshift64* rng) { // Variation 41
    double phi = rng->getDouble();
	double sinPhi, cosPhi;
	double v41 = 1.0; // not implemented
	sincos(phi * M_PI * v41,  &sinPhi, &cosPhi);
	return {sinPhi, (sinPhi * sinPhi) / cosPhi};
}

inline Coordinate variation_tangent(Coordinate c) { // Variation 42
    double sinX, sinY, cosY;
    sinX = sin(c.x);
	sincos(c.y, &sinY, &cosY);
	double invCosY = 1.0/cosY;
    return {sinX * invCosY, sinY * invCosY};
}


inline Coordinate variation_square(Xorshift64* rng) { // Variation 43
    double phi1 = rng->getDouble(), phi2 = rng->getDouble();
	return {phi1 - .5, phi2 - .5};
}

inline Coordinate variation_rays(Coordinate c, Xorshift64* rng) { // Variation 44
    double r2 = c.x * c.x + c.y * c.y;
	double v44 = 1.0;
	double coeff = v44 * tan(rng->getDouble() * M_PI * v44) / r2;
	return {coeff * cos(c.x), coeff * sin(c.y)};
}

inline Coordinate variation_blade(Coordinate c, Xorshift64* rng) { // Variation 45
	double r = std::hypot(c.x, c.y);
	double phi = rng->getDouble();
	double v45 = 1.0;
	double sinPhi, cosPhi;
	sincos(phi * r * v45, &sinPhi, &cosPhi);
	double x = cosPhi + sinPhi;
	double y = cosPhi - sinPhi;
    return {c.x * x, c.x * y};
}

inline Coordinate variation_secant(Coordinate c) { // Variation 46
	double r = std::hypot(c.x, c.y);
	double v46 = 1.0;
    return {c.x, 1.0 / (v46 * cos(v46 * r))};
}

inline Coordinate variation_twintrian(Coordinate c, Xorshift64* rng) { // Variation 47
    double r = std::hypot(c.x, c.y);
	double phi = rng->getDouble();
	double v47 = 1.0;
	double sinPhi, cosPhi;
	sincos(phi * r * v47, &sinPhi, &cosPhi);
	double t = log10(sinPhi * sinPhi) + cosPhi;
	return {c.x * t, c.x * t - M_PI * sinPhi};
}

inline Coordinate variation_cross(Coordinate c) { // Variation 48
    double r2 = c.x*c.x - c.y*c.y;
    double coeff = sqrt(1.0 / (r2*r2));
    return {coeff * c.x, coeff * c.y};
}