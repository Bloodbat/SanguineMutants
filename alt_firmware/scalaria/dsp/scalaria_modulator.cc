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

#include "scalaria/dsp/scalaria_modulator.h"

#include <algorithm>

#include "parasites_stmlib/dsp/parasites_units.h"
#include "parasites_stmlib/utils/parasites_random.h"

#include "scalaria/scalaria_resources.h"

namespace scalaria {

  using namespace std;
  using namespace parasites_stmlib;

  void ScalariaModulator::Init(float sampleRate) {
    for (size_t i = 0; i < kMaxChannels; ++i) {
      amplifier_[i].Init();
    }

    internalOscillator_.Init(sampleRate);
    moogLadderFilter.Init(sampleRate);

    previousParameters_.oscillatorShape = 0;
    previousParameters_.channel_drive[0] = 0.f;
    previousParameters_.channel_drive[1] = 0.f;
    previousParameters_.note = 48.f;
  }

  void ScalariaModulator::ProcessLadderFilter(ShortFrame* input, ShortFrame* output, size_t size) {
    float* channel1 = buffer_[0];
    float* channel2 = buffer_[1];
    float* mainOutput = buffer_[0];
    float* auxOutput = buffer_[2];

    ApplyAmplification(input, parameters_.channel_drive, auxOutput, size, false);

    float resonanceAtt = previousParameters_.rawResonance;
    // Compute the amplification using an exponential function: extend the frequency knob's range.
    float exponentialFrequencyAtt = (expf(3.f * (previousParameters_.rawFrequency - 0.75f)) / 2) - 0.05f;
    moogLadderFilter.SetResonance(resonanceAtt * 4.f);
    moogLadderFilter.SetFrequency(exponentialFrequencyAtt * 2500.f);

    if (parameters_.oscillatorShape) {
      RenderInternalOscillator(input, channel1, auxOutput, size, true);

      for (size_t i = 0; i < size; ++i) {
        mainOutput[i] = moogLadderFilter.Process(channel1[i] + channel2[i]);
      }
    } else {
      for (size_t i = 0; i < size; ++i) {
        mainOutput[i] = moogLadderFilter.Process(channel1[i]);
        auxOutput[i] = moogLadderFilter.Process(channel2[i]);
      }
    }

    Convert(output, mainOutput, auxOutput, 32768.f, size);
    previousParameters_ = parameters_;
  }

  void ScalariaModulator::Process(ShortFrame* input, ShortFrame* output, size_t size) {
    ProcessLadderFilter(input, output, size);
  }
}  // namespace scalaria
