#pragma once

#include "rack.hpp"

// Saturators: Zavalishin 2018, "The Art of VA Filter Design",
// http://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.0.0a.pdf via Bogaudio

struct SaturatorFloat_4 {
	static const rack::simd::float_4 limit;

	static inline rack::simd::float_4 saturation(rack::simd::float_4 x) {
		const rack::simd::float_4 y1 = 0.98765f; // (2*x - 1)/x**2 where x is 0.9.
		const rack::simd::float_4 offset = 0.075f / limit; // Magic.
		rack::simd::float_4 x1 = (x + 1.f) * 0.5f;
		return limit * (offset + x1 - rack::simd::sqrt(x1 * x1 - y1 * x) * (1.f / y1));
	}

	rack::simd::float_4 next(rack::simd::float_4 sample) {
		rack::simd::float_4 x = sample * (1.f / limit);
		rack::simd::float_4 negativeVoltage = sample < 0.f;
		rack::simd::float_4 saturated = rack::simd::ifelse(negativeVoltage, -saturation(-x), saturation(x));
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

const rack::simd::float_4 SaturatorFloat_4::limit = 12.f;