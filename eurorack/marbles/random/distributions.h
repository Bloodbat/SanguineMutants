// Copyright 2015 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Generates samples from various kinds of random distributions.

#ifndef MARBLES_RANDOM_DISTRIBUTIONS_H_
#define MARBLES_RANDOM_DISTRIBUTIONS_H_

#include "stmlib/stmlib.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"

#include "marbles/resources.h"

namespace marbles {
  const size_t kNumBiasValues = 5;
  const size_t kNumRangeValues = 9;
  const float kIcdfTableSize = 128.0f;

  /*
     Generates samples from beta distribution, from uniformly distributed samples.
     For higher throughput, uses pre-computed tables of inverse cdfs.
  */
  inline float BetaDistributionSample(float uniform, float spread, float bias) {
    /*
       Tables are pre-computed only for bias <= 0.5. For values above 0.5,
       symmetry is used.
    */
    bool flip_result = bias > 0.5f;
    if (flip_result) {
      uniform = 1.0f - uniform;
      bias = 1.0f - bias;
    }

    bias *= (static_cast<float>(kNumBiasValues) - 1.0f) * 2.0f;
    spread *= (static_cast<float>(kNumRangeValues) - 1.0f);

    MAKE_INTEGRAL_FRACTIONAL(bias);
    MAKE_INTEGRAL_FRACTIONAL(spread);

    size_t cell = bias_integral * (kNumRangeValues + 1) + spread_integral;

    // Lower 5% and 95% percentiles use a different table with higher resolution.
    size_t offset = 0;
    if (uniform <= 0.05f) {
      offset = kIcdfTableSize + 1;
      uniform *= 20.0f;
    } else if (uniform >= 0.95f) {
      offset = 2 * (kIcdfTableSize + 1);
      uniform = (uniform - 0.95f) * 20.0f;
    }

    float x1y1 = stmlib::Interpolate(distributions_table[cell] + offset, uniform,
      kIcdfTableSize);
    float x2y1 = stmlib::Interpolate(distributions_table[cell + 1] + offset, uniform,
      kIcdfTableSize);
    float x1y2 = stmlib::Interpolate(distributions_table[cell + kNumRangeValues + 1] + offset, uniform,
      kIcdfTableSize);
    float x2y2 = stmlib::Interpolate(distributions_table[cell + kNumRangeValues + 2] + offset, uniform,
      kIcdfTableSize);

    float y1 = x1y1 + (x2y1 - x1y1) * spread_fractional;
    float y2 = x1y2 + (x2y2 - x1y2) * spread_fractional;
    float y = y1 + (y2 - y1) * bias_fractional;

    if (flip_result) {
      y = 1.0f - y;
    }
    return y;
  }

  // Pre-computed beta(3, 3) with a fatter tail.
  inline float FastBetaDistributionSample(float uniform) {
    return stmlib::Interpolate(dist_icdf_4_3, uniform, kIcdfTableSize);
  }
}  // namespace marbles
#endif  // MARBLES_RANDOM_DISTRIBUTIONS_H_