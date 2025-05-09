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
// Naive phase vocoder.

#ifndef FLUCTUS_DSP_PVOC_PHASE_VOCODER_H_
#define FLUCTUS_DSP_PVOC_PHASE_VOCODER_H_

#include "stmlib/stmlib.h"

#include "stmlib/fft/shy_fft.h"

#include "fluctus/dsp/fluctus_frame.h"
#include "fluctus/dsp/pvoc/fluctus_stft.h"
#include "fluctus/dsp/pvoc/fluctus_spectral_coulds_transformation.h"

namespace fluctus {
    struct Parameters;

    class PhaseVocoder {
    public:
        PhaseVocoder() {}
        ~PhaseVocoder() {}

        void Init(void** buffer, const size_t* buffer_size, const float* large_window_lut, size_t largest_fft_size,
            int32_t num_channels, int32_t resolution, float sample_rate);

        void Process(const Parameters& parameters, const FloatFrame* input, FloatFrame* output, size_t size);
        void Buffer();

    private:
        FFT fft_;

        STFT stft_[2];
        SpectralCloudsTransformation spectral_clouds_transformation_[2];

        int32_t num_channels_;

        DISALLOW_COPY_AND_ASSIGN(PhaseVocoder);
    };
}  // namespace fluctus
#endif  // FLUCTUS_DSP_PVOC_PHASE_VOCODER_H_