#include "renaissance/renaissance_digital_oscillator.h"

#include <algorithm>
#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "renaissance/renaissance_parameter_interpolation.h"
#include "renaissance/renaissance_resources.h"

namespace renaissance {

using namespace stmlib;

void DigitalOscillator::RenderHarmonics(
    const uint8_t* sync,
    int16_t* buffer,
    size_t size) {
  uint32_t phase = phase_;
  int16_t previous_sample = state_.add.previous_sample;
  uint32_t phase_increment = phase_increment_ << 1;
  int32_t target_amplitude[kNumAdditiveHarmonics];
  int32_t amplitude[kNumAdditiveHarmonics];

  int32_t peak = (kNumAdditiveHarmonics * parameter_[0]) >> 7;
  int32_t second_peak = (peak >> 1) + kNumAdditiveHarmonics * 128;
  int32_t second_peak_amount = parameter_[1] * parameter_[1] >> 15;

  int32_t sqrtsqrt_width = parameter_[1] < 16384
      ? parameter_[1] >> 6 : 511 - (parameter_[1] >> 6);
  int32_t sqrt_width = sqrtsqrt_width * sqrtsqrt_width >> 10;
  int32_t width = sqrt_width * sqrt_width + 4;
  int32_t total = 0;
  for (size_t i = 0; i < kNumAdditiveHarmonics; ++i) {
    int32_t x = i << 8;
    int32_t d, g;

    d = (x - peak);
    g = 32768 * 128 / (128 + d * d / width);

    d = (x - second_peak);
    g += second_peak_amount * 128 / (128 + d * d / width);
    total += g;
    target_amplitude[i] = g;
  }

  int32_t attenuation = 2147483647 / total;
  for (size_t i = 0; i < kNumAdditiveHarmonics; ++i) {
    if ((phase_increment >> 16) * (i + 1) > 0x4000) {
      target_amplitude[i] = 0;
    } else {
      target_amplitude[i] = target_amplitude[i] * attenuation >> 16;
    }
    amplitude[i] = state_.hrm.amplitude[i];
  }

  while (size) {
    int32_t out;

    phase += phase_increment;
    if (*sync++ || *sync++) {
      phase = 0;
    }
    out = 0;
    for (size_t i = 0; i < kNumAdditiveHarmonics; ++i) {
      out += Interpolate824(wav_sine, phase * (i + 1)) * amplitude[i] >> 15;
      amplitude[i] += (target_amplitude[i] - amplitude[i]) >> 8;
    }
    CLIP(out)
    *buffer++ = (out + previous_sample) >> 1;
    *buffer++ = out;
    previous_sample = out;
    size -= 2;
  }
  state_.add.previous_sample = previous_sample;
  phase_ = phase;
  for (size_t i = 0; i < kNumAdditiveHarmonics; ++i) {
    state_.hrm.amplitude[i] = amplitude[i];
  }
}

}
