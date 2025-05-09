// Copyright 2013 Emilie Gillet, 2015 Tim Churches, 2024 Bloodbat
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
// Modifications: Tim Churches (tim.churches@gmail.com)
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
// 808-style bass drum.

#ifndef DEADMAN_DRUMS_BASS_DRUM_H_
#define DEADMAN_DRUMS_BASS_DRUM_H_

#include "stmlib/stmlib.h"

#include "deadman/drums/deadman_svf.h"
#include "deadman/drums/deadman_excitation.h"

#include "deadman/deadman_gate_processor.h"

namespace deadman {

	class BassDrum {
	public:
		BassDrum() {}
		~BassDrum() {}

		void Init();
		void Process(const GateFlags* gate_flags, int16_t* out, size_t size);

		void Configure(const uint16_t* parameter, ControlMode control_mode) {
			if (control_mode == CONTROL_MODE_HALF) {
				set_frequency(0);
				set_punch(40000);
				set_tone(8192 + (parameter[0] >> 1));
				set_decay(parameter[1]);
			} else {
				set_frequency(parameter[0] - 32768);
				set_punch(parameter[1]);
				set_tone(parameter[2]);
				set_decay(parameter[3]);
			}
		}

		void set_frequency(int16_t frequency) {
			frequency_ = (31 << 7) + (static_cast<int32_t>(frequency) * 896 >> 15);
		}

		void set_decay(uint16_t decay) {
			uint32_t scaled;
			uint32_t squared;
			scaled = 65535 - decay;
			squared = scaled * scaled >> 16;
			scaled = squared * scaled >> 18;
			resonator_.set_resonance(32768 - 128 - scaled);
		}

		void set_tone(uint16_t tone) {
			uint32_t coefficient = tone;
			coefficient = coefficient * coefficient >> 16;
			lp_coefficient_ = 512 + (coefficient >> 2) * 3;
		}

		void set_punch(uint16_t punch) {
			resonator_.set_punch(punch * punch >> 16);
		}

	private:
		Excitation pulse_up_;
		Excitation pulse_down_;
		Excitation attack_fm_;
		Svf resonator_;

		int32_t frequency_;
		int32_t lp_coefficient_;
		int32_t lp_state_;

		DISALLOW_COPY_AND_ASSIGN(BassDrum);
	};

	class RandomisedBassDrum {
	public:
		RandomisedBassDrum() {}
		~RandomisedBassDrum() {}

		void Init();
		void Process(const GateFlags* gate_flags, int16_t* out, size_t size);

		void Configure(const uint16_t* parameter, ControlMode control_mode) {
			if (control_mode == CONTROL_MODE_HALF) {
				set_frequency(0);
				base_frequency_ = 0;
				last_frequency_ = base_frequency_;
				set_punch(40000);
				set_tone(8192 + (parameter[0] >> 1));
				set_decay(parameter[1]);
				base_decay_ = parameter[1];
			} else {
				set_frequency(0);
				base_frequency_ = 0;
				last_frequency_ = base_frequency_;
				set_punch(40000);
				set_tone(8192 + (parameter[0] >> 1));
				set_decay(parameter[1]);
				base_decay_ = parameter[1];
				set_frequency_randomness(parameter[2]);
				set_hit_randomness(parameter[3]);
			}
		}

		void set_frequency(int16_t frequency) {
			frequency_ = (31 << 7) + (static_cast<int32_t>(frequency) * 896 >> 15);
		}

		void set_decay(uint16_t decay) {
			uint32_t scaled;
			uint32_t squared;
			scaled = 65535 - decay;
			squared = scaled * scaled >> 16;
			scaled = squared * scaled >> 18;
			resonator_.set_resonance(32768 - 128 - scaled);
		}

		void set_tone(uint16_t tone) {
			uint32_t coefficient = tone;
			coefficient = coefficient * coefficient >> 16;
			lp_coefficient_ = 512 + (coefficient >> 2) * 3;
		}

		void set_punch(uint16_t punch) {
			resonator_.set_punch(punch * punch >> 16);
		}

		void set_hit_randomness(uint16_t hit_randomness) {
			hit_randomness_ = hit_randomness;
		}

		void set_frequency_randomness(uint16_t frequency_randomness) {
			frequency_randomness_ = frequency_randomness;
		}

	private:
		Excitation pulse_up_;
		Excitation pulse_down_;
		Excitation attack_fm_;
		Svf resonator_;

		int32_t frequency_;
		int32_t lp_coefficient_;
		int32_t lp_state_;

		uint16_t frequency_randomness_;
		uint16_t hit_randomness_;

		int16_t base_frequency_;
		int16_t last_frequency_;
		uint16_t base_decay_;
		int32_t randomised_decay_;

		DISALLOW_COPY_AND_ASSIGN(RandomisedBassDrum);
	};

}  // namespace deadman

#endif  // DEADMAN_DRUMS_BASS_DRUM_H_
