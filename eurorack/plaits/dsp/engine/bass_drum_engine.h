// Copyright 2016 Emilie Gillet.
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
// 808 and synthetic bass drum generators.

#ifndef PLAITS_DSP_ENGINE_BASS_DRUM_ENGINE_H_
#define PLAITS_DSP_ENGINE_BASS_DRUM_ENGINE_H_

#include "plaits/dsp/drums/analog_bass_drum.h"
#include "plaits/dsp/drums/synthetic_bass_drum.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/fx/overdrive.h"
#include "plaits/dsp/fx/sample_rate_reducer.h"

namespace plaits {
  
class BassDrumEngine : public Engine {
 public:
  BassDrumEngine() { }
  ~BassDrumEngine() { }
  
  virtual void Init(stmlib::BufferAllocator* allocator) override;
  virtual void Reset() override;
  virtual void LoadUserData(const uint8_t* user_data) override { }
  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) override;

 private:
  AnalogBassDrum analog_bass_drum_;
  SyntheticBassDrum synthetic_bass_drum_;
  
  Overdrive overdrive_;
  
  DISALLOW_COPY_AND_ASSIGN(BassDrumEngine);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_BASS_DRUM_ENGINE_H_