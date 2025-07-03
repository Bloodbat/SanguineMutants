// Copyright 2014 Emilie Gillet.
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
// ScalariaModulator.

#ifndef SCALARIA_DSP_MODULATOR_H_
#define SCALARIA_DSP_MODULATOR_H_

#include "parasites_stmlib/parasites_stmlib.h"
#include "parasites_stmlib/dsp/parasites_dsp.h"
#include "parasites_stmlib/dsp/parasites_filter.h"
#include "parasites_stmlib/dsp/parasites_parameter_interpolator.h"

#include "scalaria/dsp/scalaria_oscillator.h"
#include "scalaria/dsp/scalaria_parameters.h"
#include "scalaria/scalaria_resources.h"
#include "scalaria/dsp/filters/scalaria_ladder_filter.h"
#include <algorithm>

namespace scalaria {
  using namespace std;

  const size_t kMaxBlockSize = 96;
  static const size_t kMaxChannels = 2;

  typedef struct { short l; short r; } ShortFrame;

  class SaturatingAmplifier {
  public:
    SaturatingAmplifier() {}
    ~SaturatingAmplifier() {}
    void Init() {
      drive_ = 0.0f;
    }

    void Process(float drive, float limit, short* in, float* out, float* outRaw, size_t inStride, size_t size) {
      // Process noise gate and compute raw output.
      parasites_stmlib::ParameterInterpolator drive_modulation(&drive_, drive, size);
      float level = level_;
      for (size_t i = 0; i < size; ++i) {
        float s = static_cast<float>(*in) / 32768.0f;
        float error = s * s - level;
        level += error * (error > 0.0f ? 0.1f : 0.0001f);
        s *= level <= 0.0001f ? (1.0f / 0.0001f) * level : 1.0f;
        out[i] = s;
        outRaw[i] += s * drive_modulation.Next();
        in += inStride;
      }
      level_ = level;

      // Process overdrive / gain.
      float drive2 = drive * drive;
      float preGainA = drive * 0.5f;
      float preGainB = drive2 * drive2 * drive * 24.0f;
      float preGain = preGainA + (preGainB - preGainA) * drive2;
      float driveSquished = drive * (2.0f - drive);
      float postGain = 1.0f / parasites_stmlib::SoftClip(0.33f + driveSquished * (preGain - 0.33f));
      parasites_stmlib::ParameterInterpolator pre_gain_modulation(&preGain_, preGain, size);
      parasites_stmlib::ParameterInterpolator post_gain_modulation(&postGain_, postGain, size);

      for (size_t i = 0; i < size; ++i) {
        float pre = pre_gain_modulation.Next() * out[i];
        float post = parasites_stmlib::SoftClip(pre) * post_gain_modulation.Next();
        out[i] = pre + (post - pre) * limit;
      }
    }

  private:
    float level_;
    float drive_;
    float postGain_;
    float preGain_;

    DISALLOW_COPY_AND_ASSIGN(SaturatingAmplifier);
  };

  class ScalariaModulator {
  public:
    ScalariaModulator() {}
    ~ScalariaModulator() {}

    void Init(float sampleRate);
    void Process(ShortFrame* input, ShortFrame* output, size_t size);
    void ProcessLadderFilter(ShortFrame* input, ShortFrame* output, size_t size);
    inline Parameters* mutableParameters() {
      return &parameters_;
    }

  private:
    void ApplyAmplification(ShortFrame* input, const float* level, float* auxOutput, size_t size, bool rawLevel) {
      if (!parameters_.oscillatorShape || rawLevel) {
        fill(&auxOutput[0], &auxOutput[size], 0.0f);
      }
      // Convert audio inputs to float and apply VCA/saturation (5.8% per channel).
      short* input_samples = &input->l;
      for (int32_t i = (parameters_.oscillatorShape && !rawLevel) ? 1 : 0; i < 2; ++i) {
        amplifier_[i].Process(level[i], 1.0f, input_samples + i, buffer_[i], auxOutput, 2, size);
      }
    }

    void RenderInternalOscillator(ShortFrame* input, float* carrier, float* auxOutput, size_t size, bool excludeSine = false) {
      // Scale phase-modulation input.
      for (size_t i = 0; i < size; ++i) {
        internalModulation_[i] = static_cast<float>(input[i].l) / 32768.0f;
      }

      OscillatorShape oscillatorShape = static_cast<OscillatorShape>(parameters_.oscillatorShape - (excludeSine ? 0 : 1));
      internalOscillator_.Render(oscillatorShape, parameters_.note, internalModulation_, auxOutput, size);
      for (size_t i = 0; i < size; ++i) {
        carrier[i] = auxOutput[i] * 0.75f;
      }
    }

    void Convert(ShortFrame* output, const float* mainOutput, const float* auxOutput, const float auxGain,
      size_t size) {
      while (size--) {
        output->l = parasites_stmlib::Clip16(static_cast<int32_t>(*mainOutput * 32768.0f));
        output->r = parasites_stmlib::Clip16(static_cast<int32_t>(*auxOutput * auxGain));
        ++mainOutput;
        ++auxOutput;
        ++output;
      }
    }

    Parameters parameters_;
    Parameters previousParameters_;

    MoogLadderFilter moogLadderFilter;

    SaturatingAmplifier amplifier_[kMaxChannels];

    Oscillator internalOscillator_;

    float internalModulation_[kMaxBlockSize];
    float buffer_[3][kMaxBlockSize];

    DISALLOW_COPY_AND_ASSIGN(ScalariaModulator);
  };
}  // namespace scalaria
#endif  // SCALARIA_DSP_MODULATOR_H_