#pragma once
#include <cstdint>    // uint8_t, uint64_t
#include <cstring>    // might not be needed actually
#include <cfloat>     // DBL_MAX
#include <vector>     // std::vector in Histogram and ColorState
#include <string>     // std::string in VariationDef, Transform, etc.
#include <sstream>    // std::stringstream in toString() methods
#include <iomanip>    // std::setprecision in toString() methods
#include <algorithm>  // std::fill in Histogram::clear(), std::max_element
#include <cassert>    // assert in Histogram::get()
#include <math.h>     // for variations that use math -- actually this might belong in variations/standard.h
#include <memory>     // std::unique_ptr

// File contents:
// Affine, VariationDef, Parametric, Transform, Coordinate, PixelData
// Histogram<T>, Color, Viewport, Canera, Xorshift64, 

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
	double centerX = 0.5;
	double centerY = 0.5;
	double zoom = 1.0;
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