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
// Number station.

#ifndef DEADMAN_BYTEBEATS_BYTEBEATS_H_
#define DEADMAN_BYTEBEATS_BYTEBEATS_H_

#include "stmlib/stmlib.h"

#include "deadman/drums/deadman_svf.h"
#include "deadman/deadman_gate_processor.h"

namespace deadman {

	class ByteBeats {
	public:
		ByteBeats() {}
		~ByteBeats() {}

		void Init();

		void Process(const GateFlags* gate_flags, int16_t* out, size_t size);

		void Configure(const uint16_t* parameter, ControlMode control_mode) {
			if (control_mode == CONTROL_MODE_HALF) {
				set_frequency(parameter[0]);
				set_p0(parameter[1]);
			} else {
				set_frequency(parameter[0]);
				set_p0(parameter[1]);
				set_p1(parameter[2]);
				set_p2(parameter[3]);
			}
		}

		inline void set_frequency(uint16_t freq) {
			frequency_ = freq;
		}

		inline void set_p0(uint16_t parameter) {
			p0_ = parameter > 0 ? parameter : 1;
		}

		inline void set_p1(uint16_t parameter) {
			p1_ = parameter > 0 ? parameter : 1;
		}

		inline void set_p2(uint16_t parameter) {
			p2_ = parameter;
		}

	private:
		uint16_t frequency_ = 0;
		uint16_t p0_ = 0;
		uint16_t p1_ = 0;
		uint16_t p2_ = 0;
		uint32_t t_ = 0;
		uint32_t phase_ = 0;
		uint8_t equation_index_ = 0;



		DISALLOW_COPY_AND_ASSIGN(ByteBeats);
	};

}  // namespace deadman

#endif  // DEADMAN_BYTEBEATS_BYTEBEATS_H_
