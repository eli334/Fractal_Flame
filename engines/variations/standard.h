#pragma once
#include "../types.h"
#include <cmath>

// if __CUDACC__ is defined then nvcc is compiling it, so it puts __host__ __device__ before it
// otherwise there is nothing

/* Calculation code:
    FLAME_FUNC inline Coordinate variation(Coordinate c) {} - same header for each -> [(x, y) -> (x, y)]
    other variables:
    r = sqrt(x^2 + y^2) -- r^2 = x^2 + y^2 -- std::hypot() because this is a hypotenuse
    theta = arctan(x / y)
    phi = arctan(y / x)

	I use sincos() a lot -- those will change to AVX512 instructions in the avx512.h file
*/

FLAME_FUNC inline Coordinate variation_identity(Coordinate c) { // Variation 00
    return c;
}

FLAME_FUNC inline Coordinate variation_sinusoidal(Coordinate c) { // Variation 01
    return {sin(c.x), sin(c.y)};
}

FLAME_FUNC inline Coordinate variation_spherical(Coordinate c) { // Variation 02
    double r2 = c.x*c.x + c.y*c.y;
    return {c.x / r2, c.y / r2};
}

FLAME_FUNC inline Coordinate variation_swirl(Coordinate c) { // Variation 03
    double r2 = (c.x*c.x) + (c.y*c.y);
    return {c.x * sin(r2) - c.y * cos(r2), c.x * sin(r2) + c.y * cos(r2)};
}

FLAME_FUNC inline Coordinate variation_horseshoe(Coordinate c) { // Variation 04
    double r = std::hypot(c.x, c.y); // hypot finds the distance between two points
	double inverseR = (1.0 / r);
	return {inverseR * (c.x - c.y) * (c.x + c.y), inverseR*2*c.x*c.y};
}

FLAME_FUNC inline Coordinate variation_polar(Coordinate c) { // Variation 05
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	return {theta * M_1_PI,  r - 1}; // M_1_PI = math 1/pi
}

FLAME_FUNC inline Coordinate variation_handkerchief(Coordinate c) { // Variation 06
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	return {r * (sin(theta + r)), r * (cos(theta - r))};
}

FLAME_FUNC inline Coordinate variation_heart(Coordinate c) { // Variation 07
	double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
    return {r * sin(theta * r), r * -1 * cos(theta * r)};
}

FLAME_FUNC inline Coordinate variation_disc(Coordinate c) { // Variation 08
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	double coeff = theta * M_1_PI;
	double x = 0, y = 0; 
	sincos(M_PI * r, &x, &y);
	return {coeff * x, coeff * y};
}

FLAME_FUNC inline Coordinate variation_spiral(Coordinate c) { // Variation 09
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	double sinTheta, cosTheta, sinR, cosR;
	sincos(theta, &sinTheta, &cosTheta);
	sincos(r, &sinR, &cosR);
	double inverseR = (1.0/r);
	return {inverseR * (cosTheta + sinR), inverseR * (sinTheta - cosR)}; 
}

FLAME_FUNC inline Coordinate variation_hyperbolic(Coordinate c) { // Variation 10
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	double sinTheta, cosTheta;
	sincos(theta, &sinTheta, &cosTheta);
	return {sinTheta / r, r * cosTheta};
}

FLAME_FUNC inline Coordinate variation_diamond(Coordinate c) { // Variation 11
    double theta = atan2(c.y, c.x);              ////  <-- This block is repeated often.  I think it makes the math  	
	double r = std::hypot(c.x, c.y);			 ////      more legible to not put this into a function, and I save  	
	double sinTheta, cosTheta, sinR, cosR;		 ////      precious CPU cycles by not going to a function :P       		
	sincos(theta, &sinTheta, &cosTheta);		 ////      																
	sincos(r, &sinR, &cosR);                     ////																	
	return {sinTheta * cosR, cosTheta * sinR};   
}

FLAME_FUNC inline Coordinate variation_ex(Coordinate c) { // Variation 12
	double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	
	double p0, p1;
	p0 = sin(theta + r);
	p1 = cos(theta - r);
    return {r * (p0*p0*p0 + p1*p1*p1), r * (p0*p0*p0 - p1*p1*p1)};
}

FLAME_FUNC inline Coordinate variation_julia(Coordinate c, Xorshift64* rng) { // Variation 13
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y); // r = sqrt(x^2 + y^2) -- distance formula --> std::hypot because --march=native flag means it will use my CPU's specific math capabilities in the silicon -- otherwise it will be probably optimized down to the same exact thing anyway because the compiler is smarter than me
    double sqrtr = sqrt(r);
    
	double omega = 0;
	if(rng->next() >> 63) {
		omega = M_PI; // roughly 50/50
	}

    return {sqrtr * (cos(theta / 2 + omega)),sqrtr * (sin(theta / 2 + omega))};
}

FLAME_FUNC inline Coordinate variation_bent(Coordinate c) { // Variation 14
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

FLAME_FUNC inline Coordinate variation_waves(Coordinate c, const Affine* a) { // Variation 15
	return {c.x + a->b * sin(c.y / (a->c*a->f)), c.y + a->e * sin(c.x / (a->f*a->f))};
}

FLAME_FUNC inline Coordinate variation_fisheye(Coordinate c) { // Variation 16
    double r = hypot(c.x, c.y);
	double coeff = 2.0 / (r + 1);
	return {coeff * c.y, coeff * c.x};
}

FLAME_FUNC inline Coordinate variation_popcorn(Coordinate c, Affine* a) { // Variation 17
    double tan3x, tan3y;
	tan3x = tan(3 * c.x);
	tan3y = tan(3 * c.y);
	
	return {c.x + a->c * sin(tan3y), c.y + a->f * sin(tan3x)};
}

FLAME_FUNC inline Coordinate variation_exponential(Coordinate c) { // Variation 18
    double coeff = exp(c.x - 1);
	return {coeff * cos(M_PI * c.y), coeff * sin(M_PI * c.y)};
}

FLAME_FUNC inline Coordinate variation_power(Coordinate c) { // Variation 19
    double theta = atan2(c.y, c.x);
	double r = std::hypot(c.x, c.y);
	double sinTheta, cosTheta;
	sincos(theta, &sinTheta, &cosTheta);
	double coeff = pow(r, sinTheta);
	return {coeff * cosTheta, coeff * sinTheta};
}

FLAME_FUNC inline Coordinate variation_cosine(Coordinate c) { // Variation 20
	double sinPiX, cosPiX;
    sincos(M_PI * c.x, &sinPiX, &cosPiX);
    return {cosPiX * cosh(c.y), -1 * sinPiX * sinh(c.y)};
}

FLAME_FUNC inline Coordinate variation_rings(Coordinate c, Affine* a) { // Variation 21
    double theta = atan2(c.y, c.x);
    double r = std::hypot(c.x, c.y);
    double c2 = a->c * a->c;
    double t = fmod(r + c2, 2*c2) - c2 + r * (1 - c2); // fmod for float mod
    double sinTheta, cosTheta;
    sincos(theta, &sinTheta, &cosTheta);
    return {t * cosTheta, t * sinTheta};
}

FLAME_FUNC inline Coordinate variation_fan(Coordinate c, Affine* a) { // Variation 22
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
FLAME_FUNC inline Coordinate variation_blob(Coordinate c, Parametric* p) { // Variation 23
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
FLAME_FUNC inline Coordinate variation_pdj(Coordinate c, Parametric* p) { // Variation 24
    double x, y;
	x = sin(p->p1 * c.y) - cos(p->p2 * c.x);
	y = sin(p->p3 * c.x) - cos(p->p4 * c.y);
	return {x, y};
}

// p1 = fan2.x
// p2 = fan2.y
FLAME_FUNC inline Coordinate variation_fan2(Coordinate c, Parametric* p) { // Variation 25
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
FLAME_FUNC inline Coordinate variation_rings2(Coordinate c, Parametric* p) { // Variation 26
    double theta = atan2(c.y, c.x);
    double r = std::hypot(c.x, c.y);
    double pVal = p->p1 * p->p1;
    double t = r - 2 * pVal * trunc((r + pVal) / (2 * pVal)) + r * (1 - pVal);
    double sinTheta, cosTheta;
    sincos(theta, &sinTheta, &cosTheta);
    return {t * sinTheta, t * cosTheta};
}

FLAME_FUNC inline Coordinate variation_eyefish(Coordinate c) { // Variation 27
    double r = std::hypot(c.x, c.y);
    double coeff = 2.0 / (r + 1);
    return {coeff * c.x, coeff * c.y};
}

FLAME_FUNC inline Coordinate variation_bubble(Coordinate c) { // Variation 28
    double r2 = c.x*c.x + c.y*c.y;
    double coeff = 4.0 / (r2 + 4);
    return {coeff * c.x, coeff * c.y};
}

FLAME_FUNC inline Coordinate variation_cylinder(Coordinate c) { // Variation 29
    return {sin(c.x), c.y};
}

// p1 = perspective.angle
// p2 = perspective.dist
FLAME_FUNC inline Coordinate variation_perspective(Coordinate c, Parametric* p) { // Variation 30
    double coeff = p->p2 / (p->p2 - c.y * sin(p->p1));
    return {coeff * c.x, coeff * c.y * cos(p->p1)};
}

// psi1, psi2 are random numbers [0, 1)
FLAME_FUNC inline Coordinate variation_noise(Coordinate c, Xorshift64* rng) { // Variation 31
    double psi1 = rng->getDouble(), psi2 = rng->getDouble();
    return {psi1 * c.x * cos(2*M_PI*psi2), psi1 * c.y * sin(2*M_PI*psi2)};
}

// p1 = juliaN.power
// p2 = juliaN.dist
FLAME_FUNC inline Coordinate variation_julian(Coordinate c, Parametric* p, Xorshift64* rng) { // Variation 32
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
FLAME_FUNC inline Coordinate variation_juliascope(Coordinate c, Parametric* p, Xorshift64* rng) { // Variation 33
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

FLAME_FUNC inline Coordinate variation_blur(Xorshift64* rng) { // Variation 34
    double phi1 = rng->getDouble();
    double phi2 = rng->getDouble();

	double sinPhi, cosPhi;
	sincos(2*M_PI * phi2, &sinPhi, &cosPhi);

	return {phi1 * cosPhi, phi1 * sinPhi};
}

FLAME_FUNC inline Coordinate variation_gaussian(Xorshift64* rng) { // Variation 35
    double sum = rng->getDouble() + rng->getDouble() + rng->getDouble() + rng->getDouble() - 2;
	double phi5 = rng->getDouble();
	double sinPhi, cosPhi;
	sincos(2 * M_PI * phi5, &sinPhi, &cosPhi);
	return {sum * cosPhi, sum * sinPhi};
}

// p1 = radialBlur.angle
FLAME_FUNC inline Coordinate variation_radialblur(Coordinate c, Parametric* p, Xorshift64* rng) { // Variation 36
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
FLAME_FUNC inline Coordinate variation_pie(Parametric* p, Xorshift64* rng) { // Variation 37
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
FLAME_FUNC inline Coordinate variation_ngon(Coordinate c, Parametric* p) { // Variation 38
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
FLAME_FUNC inline Coordinate variation_curl(Coordinate c, Parametric* p) { // Variation 39
    double t1 = 1 + p->p1 * c.x + p->p2 * (c.x*c.x - c.y*c.y);
    double t2 = p->p1 * c.y + 2 * p->p2 * c.x * c.y;
    double coeff = 1.0 / (t1*t1 + t2*t2);
    return {coeff * (c.x*t1 + c.y*t2), coeff * (c.y*t1 - c.x*t2)};
}

// p1 = rectangles.x
// p2 = rectangles.y
FLAME_FUNC inline Coordinate variation_rectangles(Coordinate c, Parametric* p) { // Variation 40
    return {(2*floor(c.x/p->p1) + 1)*p->p1 - c.x, (2*floor(c.y/p->p2) + 1)*p->p2 - c.y};
}

FLAME_FUNC inline Coordinate variation_arch(Xorshift64* rng) { // Variation 41
    double phi = rng->getDouble();
	double sinPhi, cosPhi;
	double v41 = 1.0; // not implemented
	sincos(phi * M_PI * v41,  &sinPhi, &cosPhi);
	return {sinPhi, (sinPhi * sinPhi) / cosPhi};
}

FLAME_FUNC inline Coordinate variation_tangent(Coordinate c) { // Variation 42
    double sinX, sinY, cosY;
    sinX = sin(c.x);
	sincos(c.y, &sinY, &cosY);
	double invCosY = 1.0/cosY;
    return {sinX * invCosY, sinY * invCosY};
}


FLAME_FUNC inline Coordinate variation_square(Xorshift64* rng) { // Variation 43
    double phi1 = rng->getDouble(), phi2 = rng->getDouble();
	return {phi1 - .5, phi2 - .5};
}

FLAME_FUNC inline Coordinate variation_rays(Coordinate c, Xorshift64* rng) { // Variation 44
    double r2 = c.x * c.x + c.y * c.y;
	double v44 = 1.0;
	double coeff = v44 * tan(rng->getDouble() * M_PI * v44) / r2;
	return {coeff * cos(c.x), coeff * sin(c.y)};
}

FLAME_FUNC inline Coordinate variation_blade(Coordinate c, Xorshift64* rng) { // Variation 45
	double r = std::hypot(c.x, c.y);
	double phi = rng->getDouble();
	double v45 = 1.0;
	double sinPhi, cosPhi;
	sincos(phi * r * v45, &sinPhi, &cosPhi);
	double x = cosPhi + sinPhi;
	double y = cosPhi - sinPhi;
    return {c.x * x, c.x * y};
}

FLAME_FUNC inline Coordinate variation_secant(Coordinate c) { // Variation 46
	double r = std::hypot(c.x, c.y);
	double v46 = 1.0;
    return {c.x, 1.0 / (v46 * cos(v46 * r))};
}

FLAME_FUNC inline Coordinate variation_twintrian(Coordinate c, Xorshift64* rng) { // Variation 47
    double r = std::hypot(c.x, c.y);
	double phi = rng->getDouble();
	double v47 = 1.0;
	double sinPhi, cosPhi;
	sincos(phi * r * v47, &sinPhi, &cosPhi);
	double t = log10(sinPhi * sinPhi) + cosPhi;
	return {c.x * t, c.x * t - M_PI * sinPhi};
}

FLAME_FUNC inline Coordinate variation_cross(Coordinate c) { // Variation 48
    double r2 = c.x*c.x - c.y*c.y;
    double coeff = sqrt(1.0 / (r2*r2));
    return {coeff * c.x, coeff * c.y};
}