#ifndef SCALARIA_LADDER_FILTER_H
#define SCALARIA_LADDER_FILTER_H

static const float kMoogLadderPi = 3.1415927410125732421875f;
static const float kLadderThermalVoltage = 0.312f;
static const float kLadderDoubleThermalVoltage = 2.f * kLadderThermalVoltage;

namespace scalaria {
	// Ported from https://github.com/ddiakopoulos/MoogLadders/blob/master/src/ImprovedModel.h
	class MoogLadderFilter {
	public:
		MoogLadderFilter() {}
		~MoogLadderFilter() {}

		void Init(float theSampleRate) {
			sampleRate = theSampleRate / 2.f;
			SetFrequency(1000.f); // Normalized cutoff frequency.
			SetResonance(0.1f); // [0, 4]
		}

		float Process(float in, float drive = 1.f) {
			float dV0, dV1, dV2, dV3;

			dV0 = -g * (getTanh((drive * in + resonance * V[3]) / kLadderDoubleThermalVoltage) + tV[0]);
			/*
			   Not oversampling like in the original model in order
			   to allow the filter to open more when knob is fully CW.
			*/
			V[0] += (dV0 + dV[0]) / sampleRate;
			dV[0] = dV0;
			tV[0] = getTanh(V[0] / kLadderDoubleThermalVoltage);

			dV1 = g * (tV[0] - tV[1]);
			V[1] += (dV1 + dV[1]) / sampleRate;
			dV[1] = dV1;
			tV[1] = getTanh(V[1] / kLadderDoubleThermalVoltage);

			dV2 = g * (tV[1] - tV[2]);
			V[2] += (dV2 + dV[2]) / sampleRate;
			dV[2] = dV2;
			tV[2] = getTanh(V[2] / kLadderDoubleThermalVoltage);

			dV3 = g * (tV[2] - tV[3]);
			V[3] += (dV3 + dV[3]) / sampleRate;
			dV[3] = dV3;
			tV[3] = getTanh(V[3] / kLadderDoubleThermalVoltage);

			return V[3];
		}

		virtual void SetResonance(float theResonance) {
			resonance = theResonance;
		}

		virtual void SetFrequency(float cutoffFrequency) {
			cutoff = cutoffFrequency;
			x = (kMoogLadderPi * cutoff) / sampleRate;
			g = 4.f * kMoogLadderPi * kLadderThermalVoltage * cutoff * (1.f - x) / (1.f + x);
		}

	private:
		float V[4] = {};
		float dV[4] = {};
		float tV[4] = {};

		float x;
		float g;
		float cutoff;
		float resonance;
		float sampleRate;

		float getTanh(float x) {
			float x2 = x * x;
			return x * (27.f + x2) / (27.f + 9.f * x2);
		}
	};
}
#endif