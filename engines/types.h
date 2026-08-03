#pragma once
#include <algorithm>  // std::fill in Histogram::clear(), std::max_element
#include <cassert>    // assert in Histogram::get()
#include <cfloat>     // DBL_MAX
#include <cstdint>    // uint64_t
#include <iomanip>    // std::setprecision in toString() methods
#include <sstream>    // std::stringstream in toString() methods
#include <string>     // std::string in VariationDef, Transform, etc.
#include <vector>     // std::vector in Histogram and ColorState
#include <math.h>     // for variations that use math
#include <memory>     // std::unique_ptr
#include <cstdarg>    // va_list, va_start, va_end in logLevel()
#include <unistd.h>	  // i just wanted colors man
#include <set>		  // for flame variations; cleanest way I could find

#include "./variations/portable_rand.h" // included here because types is included correctly, therefore this should be too

#ifdef __CUDACC__
#define FLAME_FUNC __host__ __device__
#define FLAME_FUNC_HOST __host__
#else
#define FLAME_FUNC
#define FLAME_FUNC_HOST
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL 1
#endif


enum LogTier { 
	LOG_SILENT = 0, // errors
	LOG_SUMMARY = 1, // run length, stats -- the usual level
	LOG_LIFECYCLE = 2, // engine spawns / despawns
	LOG_CHATTER = 3 // inside of engines
};

inline bool useColor() {
    static bool cached = isatty(fileno(stdout));
    return cached;
}

FLAME_FUNC_HOST inline void logLevel(int tier, const char* fmt, ...) { // unknown how many variables, so ... catches them all and a va_list grabs the variables, giving me a modified printf
    if (tier > LOG_LEVEL) return;
	
	const char* name  = "";
	switch (tier) {
		case LOG_SILENT:	name = "SILENT"; 		break;
		case LOG_SUMMARY:   name = "SUMMARY"; 		break;
		case LOG_LIFECYCLE: name = "LIFECYCLE"; 	break; 
		case LOG_CHATTER:   name = "CHATTER"; 		break; 
		default:            name = "";         		break;
	}

	if(useColor()) {
		const char* color = "";
		switch (tier) {
			case LOG_SILENT:	color = "\033[31m"; break; // red 	- errors / things I'm specifically looking for in debugging
			case LOG_SUMMARY:   color = "\033[32m"; break; // green - results
			case LOG_LIFECYCLE: color = "\033[36m"; break; // cyan 	- events
			case LOG_CHATTER:   color = "\033[90m"; break; // grey 	- noise
			default:            color = "\033[34m"; break; // blue 	- typed logLevel wrong
    	}
		printf("%s", color);
	}
	printf("[%s]: ", name);
	
	va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}


// File contents:
// Affine, VariationDef, Parametric, Transform, Coordinate, PixelData
// Histogram<T>, Color, Viewport, Canera, Xorshift64

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

namespace vtag { // really cool trick -- vtag::SIMPLE for 0x1, etc. Pretty intuitive, and I can ctrl+click vtag in VSCode to see what the full table is again
    // math nature
	constexpr uint32_t SIMPLE = 0x1; // +/-/*/÷, sqrt, floor, abs
    constexpr uint32_t TRIG   = 0x2; // sin/cos/tan/atan^2/sincos()
    constexpr uint32_t EXP    = 0x4; // e^x
    constexpr uint32_t LOG    = 0x8; // log/log10
	constexpr uint32_t HYPER  = 0x10; // sinh/cosh/tanh
	constexpr uint32_t POW    = 0x20;  // pow(b,e) — internally exp(e·ln b), heaviest transcendental
	// 0x40
	// 0x80
	
	// 
	constexpr uint32_t STOCHASTIC  = 0x100; // takes Xorshift64*
	constexpr uint32_t DEPENDENT   = 0x200; // takes Affine* (the paper called it this so I made it an axis -- honestly just for organization)
	constexpr uint32_t PARAMETRIC  = 0x400; // takes Parametric*
}


struct VariationDef {
	int index = 0;

	std::string name = "Identity";
	// std::string formula = "(x, y)";

	uint32_t tags = 0;

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
	float color = 0;

	bool operator<(const PixelData& other) const {
		return hits < other.hits;
	}
};

template<typename T> // PixelData
class Histogram {
	public:
		int width = 1000, height = 1000;
		std::vector<T> data;

		T operator[](int index) const {
			return data[index];
		}

		Histogram(int desiredWidth = 1000, int desiredHeight = 1000) : width(desiredWidth), height(desiredHeight) {
			data.resize(desiredWidth*desiredHeight);
		}

		int getWidth() const {
			return width;
		}

		int getHeight() const {
			return height;
		}

		size_t getSize() const {
			return width * height;
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

		void merge(Histogram<PixelData>& other) {
			for (int i = 0; i < width * height; i++) {
				data[i].hits += other.data[i].hits;
				data[i].color += other.data[i].color;
    		}
			other.clear();
		}


		Histogram(const Histogram&) = delete; // don't want or need copy constructor
		Histogram& operator=(const Histogram&) = delete; // don't need assignment

		Histogram(Histogram&&) = default; // move constructor
		Histogram& operator=(Histogram&&) = default; // move assignment -- only for moving from an expiring Histogram to one that exists (for std::vector move)
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
	Viewport(float minX, float maxX, float minY, float maxY) {
		double realMinX = std::min(minX, maxX);
		double realMaxX = std::max(minX, maxX);

		double realMinY = std::min(minY, maxY);
		double realMaxY = std::max(minY, maxY);
		
		this->minX = realMinX;
		this->maxX = realMaxX;
		this->minY = realMinY;
		this->maxY = realMaxY;
		printf("Viewport float-constructed:\r\n%s", toString());
	}

	std::string formattedOutput;
	const char* toString() {
		std::stringstream stream;
		stream << std::fixed << std::setprecision(3);
		stream << "minX: " << minX << ", maxX: " << maxX << "\r\n";
		stream << "minY: " << minY << ", maxY: " << maxY << "\r\n";

		formattedOutput = stream.str();
		return formattedOutput.c_str();
	}
};

struct Camera {
	double centerX = 0.5; // centerX = 0 would be the camera centered on the left edge, centerX = 1 would be on the right edge
	double centerY = 0.5; // centerY = 0 would be the camera centered on the bottom edge, centerY = 1 would be on the top edge
	double zoom = 1.0;	  // 1.0 by default - fills entire screen
};

// xorshift64 -- fast, pseudorandom number generator
// needs FLAME_FUNC to work in CUDA
struct Xorshift64 {
	Xorshift64() = delete;
	FLAME_FUNC Xorshift64(uint64_t seed) {
		state = seed;
		if(state == 0) { // 0 is the only degenerate state of Xorshift
			state = 0xBADBEEFBADBEEFBULL; // bad beef bull -- random data for my Xorshift64 seed if seed was made 0
		}
	}

	uint64_t state;
	FLAME_FUNC uint64_t next() {
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;
		return state;
	}

	FLAME_FUNC double getDouble() {
        return (next() >> 11) * (1.0 / (1ULL << 53)); // discards bottom 11 bits, making it a uint64_t from [0, 2^53-1]
    }												  // then, multiplies by (1/2^53), mapping it from [0, 1) -- this is effectively [0, 1]
};




inline double variationCost(uint32_t tags) {
    double cost = 1.0;                          // SIMPLE arithmetic baseline (+-x/)
    if(tags & vtag::TRIG)       cost += 4.0;    // sin/cos/tan
    if(tags & vtag::EXP)        cost += 5.0;    // e^x
    if(tags & vtag::LOG)        cost += 5.0;    // log/log10
    if(tags & vtag::HYPER)      cost += 8.0;    // sinh/cosh (via exp)
    if(tags & vtag::POW)        cost += 10.0;    // pow = exp(e·ln b), heaviest
    if(tags & vtag::STOCHASTIC) cost += 2.0;    // extra RNG draws
    return cost;
}
 
struct FlameStats {
    size_t   numTransforms = 0;
    int      numDistinct   = 0;   // distinct variation indices ()
    uint32_t tagMask       = 0;   // OR of every variation's tags -- slice on this in Pandas
    double   workExpected  = 0.0; // weighted average cost of the one branch taken
    double   workDistinct  = 0.0; // sum of cost over the distinct branches present 

	static std::string header() {
		return {"numTransforms,numDistinct,tagMask,workExpected,workDistinct"};
	}

	std::string formattedOutput = "default overwritten";

	void formatOutput() {
		std::stringstream stream;
		stream << numTransforms << ","
			   << numDistinct 	<< "," 
			   << tagMask 		<< ","
			   << workExpected 	<< "," 
			   << workDistinct;
		stream >> formattedOutput;
	}

	std::string toString() const {
		return formattedOutput.c_str();
	}
};
 
inline FlameStats analyzeFlame(const std::vector<Transform>& transforms) {
    FlameStats stats;
    stats.numTransforms = transforms.size();
 
    double totalWeight = 0.0;
    for(size_t i = 0; i < transforms.size(); i++) {
        totalWeight += transforms[i].weight;
    }
    if(totalWeight <= 0.0) totalWeight = 1.0; // guard against transforms somehow adding to <1
 
    std::set<int> distinctVariations;
    for(size_t i = 0; i < transforms.size(); i++) {
        const Transform& t = transforms[i];
        double cost = variationCost(t.variation.tags);
 
        stats.tagMask |= t.variation.tags;
        stats.workExpected += (t.weight / totalWeight) * cost;
 
        if(distinctVariations.insert(t.variation.index).second) { // returns a pair: (iterator to inserted element, bool) (true if it's a unique addition)
            stats.workDistinct += cost;
        }
    }
    stats.numDistinct = (int)distinctVariations.size();
    
	stats.formatOutput();
	logLevel(LOG_CHATTER, "Flame analyzed; %s\r\n", stats.toString().c_str());
	return stats;
}
 
