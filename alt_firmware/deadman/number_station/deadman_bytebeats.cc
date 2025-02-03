// Copyright 2015 Tim Churches, 2024 Bloodbat
//
// Author: Tim Churches (tim.churches@gmail.com)
// Modifications: Bloodbat
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
// Byte beats

#include "deadman/number_station/deadman_bytebeats.h"

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include <cmath>

#include "deadman/deadman_resources.h"

namespace deadman {

	const uint8_t kDownsample = 4;

	using namespace stmlib;

	void ByteBeats::Init() {
		frequency_ = 32678;
		phase_ = 0;
		p0_ = 32678;
		p1_ = 32678;
		p2_ = 0;
		equation_index_ = 0;
	}

	void ByteBeats::Process(const GateFlags* gate_flags, int16_t* out, size_t size) {
		uint32_t p0 = 0;
		uint32_t p1 = 0;
		int32_t sample = 0;
		// temporary vars
		uint32_t p = 32678;
		uint32_t q = 32678;
		uint32_t q1 = 0;
		uint32_t q2 = 0;
		uint8_t j = 0;
		uint32_t sample1 = 32678;
		uint32_t sample2 = 32678;

		uint16_t bytepitch = (65535 - frequency_) >> 11; // was 12
		if (bytepitch < 1) {
			bytepitch = 1;
		}

		equation_index_ = p2_ >> 13;

		while (size > 0) {
			int cycles = (size > kDownsample) ? kDownsample : size;

			for (uint8_t i = 0; i < cycles; i++) {
				GateFlags gate_flag = *gate_flags++;
				if (gate_flag & GATE_FLAG_RISING) {
					phase_ = 0;
					t_ = 0;
				}
			}

			++phase_;
			if (phase_ % bytepitch == 0) ++t_;
			switch (equation_index_) {
			case 0:
				// From http://royal-paw.com/2012/01/bytebeats-in-c-and-python-generative-symphonies-from-extremely-small-programs/
				// (atmospheric, hopeful)
				p0 = p0_ >> 9; // was 9
				p1 = p1_ >> 9; // was 11
				sample = ((((t_ * 3) & (t_ >> 10)) | ((t_ * p0) & (t_ >> 10)) | ((t_ * 10) & ((t_ >> 8) * p1) & 128)) & 0xFF) << 8;
				break;
			case 1:
				p0 = p0_ >> 11;
				p1 = p1_ >> 11;
				// Equation by stephth via https://www.youtube.com/watch?v=tCRPUv8V22o at 3:38.
				sample = ((((t_ * p0) & (t_ >> 4)) | ((t_ * 5) & (t_ >> 7)) | ((t_ * p1) & (t_ >> 10))) & 0xFF) << 8;
				break;
			case 2:
				p0 = p0_ >> 12;
				p1 = p1_ >> 12;
				// This one is from http://www.reddit.com/r/bytebeat/comments/20km9l/cool_equations/
				sample = ((((t_ >> p0) & t_) * (t_ >> p1)) & 0xFF) << 8;
				break;
			case 3:
				p0 = p0_ >> 11;
				p1 = p1_ >> 9;
				if (p1 == 0) {
					p1 = p1_;
				}
				// This one is the second one listed at http://xifeng.weebly.com/bytebeats.html
				sample = ((((((((t_ >> p0) | t_) | (t_ >> p0)) * 10) & ((5 * t_) | (t_ >> 10))) | (t_ ^ (t_ % p1))) & 0xFF)) << 8;
				break;
			case 4:
				p0 = p0_ >> 12; // was 9
				if (p0 == 0) {
					p0 = p0_;
				}
				p1 = p1_ >> 12; // was 11
				/* BitWiz Transplant from Equation Composer Ptah bank
				   run at twice normal sample rate. */
				for (j = 0; j < 2; ++j) {
					sample = t_ * (((t_ >> p1) ^ ((t_ >> p1) - 1) ^ 1) % p0);
					if (j == 0) {
						++t_;
					}
				}
				break;
			case 5:
				p0 = p0_ >> 11;

				if (p0 == 0) {
					p0 = p0_;
				}

				p1 = p1_ >> 9;

				if (p1 == 0) {
					p1 = p1_;
				}

				/* Arpeggiation from Equation Composer Khepri bank
				   run at twice normal sample rate */
				p = ((t_ / (1236 + p0)) % 128) & ((t_ >> (p1 >> 5)) * p1);

				if (p == 0) {
					p = 0x8000;
				}

				q1 = (500 * p1) % 5;

				if (q1 == 0) {
					q1 = 0x8000;
				}

				q2 = t_ / q1 + 1;

				q = (t_ / q2) % p;

				sample = (t_ >> q >> (p1 >> 5)) + (t_ / (t_ >> ((p1 >> 5) & 12)) >> p);

				break;
			case 6:
				p0 = p0_ >> 9;

				if (p0 == 0) {
					p0 = p0_;
				}

				p1 = p1_ >> 10; // was 9

				if (p1 == 0) {
					p1 = p1_;
				}

				// The Smoker from Equation Composer Khepri bank.
				sample1 = p1 * 4;

				sample2 = t_ << t_ / sample1;

				sample = sample ^ (t_ >> (p1 >> 4)) >> ((t_ / 6988 * t_ % (p0 + 1)) + sample2);

				break;
			default:
				p0 = p0_ >> 9;

				if (p0 == 0) {
					p0 = p0_;
				}

				p1 = t_ % p1_;

				if (p1 == 0) {
					p1 = p1_;
				}

				// Run at twice normal sample rate.
				for (j = 0; j < 2; ++j) {
					// Warping overtone echo drone, from BitWiz.
					sample = ((t_ & p0) - (t_ % p1)) ^ (t_ >> 7);
					if (j == 0) {
						++t_;
					}
				}
				break;
			}
			CLIP(sample)
				cycles = kDownsample;
			while (cycles-- > 0) {
				if (size-- > 0) {
					*out++ = sample;
				}
			}
		}
	}

}  // namespace deadman
