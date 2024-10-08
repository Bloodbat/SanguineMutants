#pragma once

#include "rack.hpp"

// Saturators: Zavalishin 2018, "The Art of VA Filter Design",
// http://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.0.0a.pdf via Bogaudio

struct SaturatorFloat_4 {
	static const simd::float_4 limit;

	static inline simd::float_4 saturation(simd::float_4 x) {
		const simd::float_4 y1 = 0.98765f; // (2*x - 1)/x**2 where x is 0.9.
		const simd::float_4 offset = 0.075f / limit; // Magic.
		simd::float_4 x1 = (x + 1.f) * 0.5f;
		return limit * (offset + x1 - simd::sqrt(x1 * x1 - y1 * x) * (1.f / y1));
	}

	simd::float_4 next(simd::float_4 sample) {
		simd::float_4 x = sample * (1.f / limit);
		simd::float_4 negativeVoltage = sample < 0.f;
		simd::float_4 saturated = simd::ifelse(negativeVoltage, -saturation(-x), saturation(x));
		return saturated;
	}
};

struct SaturatorFloat {
	static const float limit;

	static inline float saturation(float x) {
		const float y1 = 0.98765f; // (2*x - 1)/x**2 where x is 0.9.
		const float offset = 0.075f / limit; // Magic.
		float x1 = (x + 1.f) * 0.5f;
		return limit * (offset + x1 - sqrtf(x1 * x1 - y1 * x) * (1.f / y1));
	}

	float next(float sample) {
		float x = sample * (1.f / limit);
		if (sample < 0.f) {
			return -saturation(-x);
		}
		return saturation(x);
	}
};

const simd::float_4 SaturatorFloat_4::limit = 12.f;