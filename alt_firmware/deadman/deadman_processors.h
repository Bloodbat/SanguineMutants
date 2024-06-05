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
// This is the common entry points for all types of modulation sources!

#ifndef DEADMAN_PROCESSORS_H_
#define DEADMAN_PROCESSORS_H_

#include "stmlib/stmlib.h"

#include <algorithm>

#include "deadman/drums/deadman_bass_drum.h"
#include "deadman/drums/deadman_fm_drum.h"
#include "deadman/drums/deadman_snare_drum.h"
#include "deadman/drums/deadman_high_hat.h"
#include "deadman/drums/deadman_cymbal.h"
#include "deadman/modulations/deadman_bouncing_ball.h"
#include "deadman/modulations/deadman_lfo.h"
#include "deadman/modulations/deadman_mini_sequencer.h"
#include "deadman/modulations/deadman_turing_machine.h"
#include "deadman/modulations/deadman_multistage_envelope.h"
#include "deadman/number_station/deadman_number_station.h"
#include "deadman/pulse_processor/deadman_pulse_shaper.h"
#include "deadman/pulse_processor/deadman_pulse_randomizer.h"
#include "deadman/number_station/deadman_bytebeats.h"

#include "deadman/deadman_gate_processor.h"

namespace deadman {

	enum ProcessorFunction {
		PROCESSOR_FUNCTION_ENVELOPE, // Done
		PROCESSOR_FUNCTION_LFO, // Done
		PROCESSOR_FUNCTION_TAP_LFO, //Done
		PROCESSOR_FUNCTION_BASS_DRUM, // Done
		PROCESSOR_FUNCTION_SNARE_DRUM, // Done
		PROCESSOR_FUNCTION_HIGH_HAT, // Done
		PROCESSOR_FUNCTION_CYMBAL, //Done
		PROCESSOR_FUNCTION_FM_DRUM, // Done
		PROCESSOR_FUNCTION_PULSE_SHAPER, // Done
		PROCESSOR_FUNCTION_PULSE_RANDOMIZER, // Done
		PROCESSOR_FUNCTION_MINI_SEQUENCER, // Done
		PROCESSOR_FUNCTION_NUMBER_STATION, // Done
		PROCESSOR_FUNCTION_BYTEBEATS, // Done
		PROCESSOR_FUNCTION_DUAL_ATTACK_ENVELOPE, // Done
		PROCESSOR_FUNCTION_REPEATING_ATTACK_ENVELOPE, // Done
		PROCESSOR_FUNCTION_LOOPING_ENVELOPE, // Done
		PROCESSOR_FUNCTION_RANDOMISED_ENVELOPE, // Done
		PROCESSOR_FUNCTION_BOUNCING_BALL, // Done
		PROCESSOR_FUNCTION_RANDOMISED_BASS_DRUM, // Done
		PROCESSOR_FUNCTION_RANDOMISED_SNARE_DRUM, // Done
		PROCESSOR_FUNCTION_TURING_MACHINE, // Done
		PROCESSOR_FUNCTION_MOD_SEQUENCER, // Done
		PROCESSOR_FUNCTION_FMLFO, // Done
		PROCESSOR_FUNCTION_RFMLFO, // Done
		PROCESSOR_FUNCTION_WSMLFO, // Done
		PROCESSOR_FUNCTION_RWSMLFO, // Done
		PROCESSOR_FUNCTION_PLO, // Done (but broken!)
		PROCESSOR_FUNCTION_LAST
	};

#define DECLARE_PROCESSOR(ClassName, variable) \
  void ClassName ## Init() { \
    variable.Init(); \
  } \
  void ClassName ## Process(const GateFlags* gate_flags, int16_t* out, size_t size) { \
    variable.Process(gate_flags, out, size); \
  } \
  void ClassName ## Configure(uint16_t* p, ControlMode control_mode) { \
    variable.Configure(p, control_mode); \
  } \
  ClassName variable;

	class Processors {
	public:
		Processors() { }
		~Processors() { }

		void Init(uint8_t index);

		typedef void (Processors::* InitFn)();
		typedef void (Processors::* ProcessFn)(const GateFlags*, int16_t*, size_t);
		typedef void (Processors::* ConfigureFn)(uint16_t*, ControlMode);

		struct ProcessorCallbacks {
			InitFn init_fn;
			ProcessFn process_fn;
			ConfigureFn configure_fn;
		};

		inline void set_control_mode(ControlMode control_mode) {
			control_mode_ = control_mode;
			Configure();
		}

		inline void set_parameter(uint8_t index, uint16_t parameter) {
			parameter_[index] = parameter;
			Configure();
		}

		inline void CopyParameters(uint16_t* parameters, uint16_t size) {
			std::copy(&parameters[0], &parameters[size], &parameter_[0]);
		}

		inline void set_function(ProcessorFunction function) {
			function_ = function;
			lfo_.set_sync(function == PROCESSOR_FUNCTION_TAP_LFO);
			fmlfo_.set_mod_type(function == PROCESSOR_FUNCTION_RFMLFO);
			wsmlfo_.set_mod_type(function == PROCESSOR_FUNCTION_RWSMLFO);
			plo_.set_sync(function == PROCESSOR_FUNCTION_PLO);
			callbacks_ = callbacks_table_[function];
			if (function != PROCESSOR_FUNCTION_TAP_LFO and function != PROCESSOR_FUNCTION_PLO) {
				(this->*callbacks_.init_fn)();
			}
			Configure();
		}

		inline ProcessorFunction function() const { return function_; }

		inline void Process(const GateFlags* gate_flags, int16_t* output, size_t size) {
			(this->*callbacks_.process_fn)(gate_flags, output, size);
		}

		inline const NumberStation& number_station() const { return number_station_; }

	private:
		void Configure() {
			(this->*callbacks_.configure_fn)(&parameter_[0], control_mode_);
		}

		ControlMode control_mode_;
		ProcessorFunction function_;
		uint16_t parameter_[4];

		ProcessorCallbacks callbacks_;
		static const ProcessorCallbacks callbacks_table_[PROCESSOR_FUNCTION_LAST];

		DECLARE_PROCESSOR(MultistageEnvelope, envelope_);
		DECLARE_PROCESSOR(Lfo, lfo_);
		DECLARE_PROCESSOR(BassDrum, bass_drum_);
		DECLARE_PROCESSOR(SnareDrum, snare_drum_);
		DECLARE_PROCESSOR(HighHat, high_hat_);
		DECLARE_PROCESSOR(Cymbal, cymbal_);
		DECLARE_PROCESSOR(FmDrum, fm_drum_);
		DECLARE_PROCESSOR(PulseShaper, pulse_shaper_);
		DECLARE_PROCESSOR(PulseRandomizer, pulse_randomizer_);
		DECLARE_PROCESSOR(BouncingBall, bouncing_ball_);
		DECLARE_PROCESSOR(MiniSequencer, mini_sequencer_);
		DECLARE_PROCESSOR(NumberStation, number_station_);
		DECLARE_PROCESSOR(ByteBeats, bytebeats_);
		DECLARE_PROCESSOR(DualAttackEnvelope, dual_attack_envelope_);
		DECLARE_PROCESSOR(LoopingEnvelope, looping_envelope_);
		DECLARE_PROCESSOR(RepeatingAttackEnvelope, repeating_attack_envelope_);
		DECLARE_PROCESSOR(RandomisedEnvelope, randomised_envelope_);
		DECLARE_PROCESSOR(RandomisedBassDrum, randomised_bass_drum_);
		DECLARE_PROCESSOR(RandomisedSnareDrum, randomised_snare_drum_);
		DECLARE_PROCESSOR(TuringMachine, turing_machine_);
		DECLARE_PROCESSOR(ModSequencer, mod_sequencer_);
		DECLARE_PROCESSOR(FmLfo, fmlfo_);
		DECLARE_PROCESSOR(WsmLfo, wsmlfo_);
		DECLARE_PROCESSOR(Plo, plo_);

		DISALLOW_COPY_AND_ASSIGN(Processors);
	};

	extern Processors processors[2];

}  // namespace deadman

#endif  // DEADMAN_PROCESSORS_H_
