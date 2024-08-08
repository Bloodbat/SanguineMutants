#include <algorithm>

#include "fluctus/dsp/fluctus_kammerl_player.h"

namespace fluctus {

float KammerlPlayer::quantizeSize(float size) const {
	if (size >= 1.0f) {
		return 1.0f;
	}
	if (size <= 0.0f) {
		return 0.0f;
	}
	static const float kSizeQuantizationBorder[] = {
			1.0f / 32.0f,
			(1.0f / 32.0f + 1.0f / 16.0f) / 2.0f,
			(1.0f / 16.0f + 1.0f / 8.0f) / 2.0f,
			(1.0f / 8.0f + 1.0f / 4.0f) / 2.0f,
			(1.0f / 4.0f + 1.0f / 3.0f) / 2.0f,
			(1.0f / 3.0f + 1.0f / 2.0f) / 2.0f,
			(1.0f / 2.0f + 3.0f / 4.0f) / 2.0f,
			(3.0f / 4.0f + 1.0f) / 2.0f, 1.0f };
	static const float kSizeQuantization[] = {
			1.0f / 32.0f,
			1.0f / 32.0f,
			1.0f / 16.0f,
			1.0f / 8.0f,
			1.0f / 4.0f,
			1.0f / 3.0f,
			1.0f / 2.0f,
			3.0f / 4.0f,
			1.0f };

	static const size_t kNumSizeQuantizationIntevals = sizeof(kSizeQuantization)
			/ sizeof(float);
	if (size > kSizeQuantizationBorder[0]) {
		size_t index = std::upper_bound(kSizeQuantizationBorder,
				kSizeQuantizationBorder + kNumSizeQuantizationIntevals, size)
				- kSizeQuantizationBorder;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
		CONSTRAIN(index, 0, kNumSizeQuantizationIntevals - 1);
#pragma GCC diagnostic pop
		return kSizeQuantization[index];
	}
	return size;
}

}  // namespace fluctus
