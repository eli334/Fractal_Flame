// portable replacements for std::uniform_{real,int}_distribution.
// std::mt19937 is standardized (same stream everywhere); only the
// distributions differ across libstdc++/libc++, so we pin them here.
// verified bit-exact vs libstdc++ (gcc 13.3) at -O0..-O3 -flto.

// generate_canonical<double,53>: consumes 2 mt19937 draws
#include <random>
static inline double genCanonicalDouble(std::mt19937& g) {
    double sum = 0.0, tmp = 1.0;
    sum += (double)(uint32_t)g() * tmp; tmp *= 4294967296.0; // 2^32
    sum += (double)(uint32_t)g() * tmp; tmp *= 4294967296.0; // tmp = 2^64
    double ret = sum / tmp;
    if (ret >= 1.0) ret = std::nextafter(1.0, 0.0);
    return ret;
}

// generate_canonical<float,24>: consumes 1 draw
static inline float genCanonicalFloat(std::mt19937& g) {
    float sum = 0.0f, tmp = 1.0f;
    sum += (float)(uint32_t)g() * tmp; tmp *= 4294967296.0f;
    float ret = sum / tmp;
    if (ret >= 1.0f) ret = std::nextafter(1.0f, 0.0f);
    return ret;
}

static inline double uniformRealD(std::mt19937& g, double a, double b) {
    return (genCanonicalDouble(g) * (b - a)) + a; // matches operator() order
}
static inline float uniformRealF(std::mt19937& g, float a, float b) {
    return (genCanonicalFloat(g) * (b - a)) + a;
}

// lemire nearly-divisionless, matches libstdc++ _S_nd<uint64_t,_,uint32_t>
static inline uint32_t lemire(std::mt19937& g, uint32_t range) {
    uint64_t product = (uint64_t)(uint32_t)g() * (uint64_t)range;
    uint32_t low = (uint32_t)product;
    if (low < range) {
        uint32_t threshold = (uint32_t)(0u - range) % range;
        while (low < threshold) {
            product = (uint64_t)(uint32_t)g() * (uint64_t)range;
            low = (uint32_t)product;
        }
    }
    return (uint32_t)(product >> 32);
}
static inline int uniformIntI(std::mt19937& g, int a, int b) {
    uint32_t range = (uint32_t)(b - a) + 1u; // outcomes, inclusive
    return a + (int)lemire(g, range);
}