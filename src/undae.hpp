// Mutable Instruments Ripples emulation for VCV Rack
// Copyright (C) 2020 Tyler Coy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <cmath>
#include <algorithm>
#include <random>
#include "rack.hpp"
#include "undaeaafilter.hpp"

using namespace rack;

namespace undae {
    // Frequency knob
    static const float kFreqKnobMin = 20.f;
    static const float kFreqKnobMax = 20000.f;
    static const float kFreqKnobVoltage = std::log2f(kFreqKnobMax / kFreqKnobMin);

    /*
    Calculate base and multiplier values to pass to configParam so that the
    knob value is labeled in Hz.
    Model the knob as a generic V/oct input with 100k input impedance.
    Assume the internal knob voltage `v` is on the interval [0, vmax] and
    let `p` be the position of the knob varying linearly along [0, 1]. Then,
      freq = fmin * 2^v
      v = vmax * p
      vmax = log2(fmax / fmin)
      freq = fmin * 2^(log2(fmax / fmin) * p)
           = fmin * (fmax / fmin)^p
    */
    static const float kFreqKnobDisplayBase = kFreqKnobMax / kFreqKnobMin;
    static const float kFreqKnobDisplayMultiplier = kFreqKnobMin;

    /*
    Frequency CV amplifier
    The 2164's gain constant is -33mV/dB. Multiply by 6dB/1V to find the
    nominal gain of the amplifier.
    */
    static const float kVCAGainConstant = -33e-3f;
    static const float kPlus6dB = 20.f * std::log10(2.f);
    static const float kFreqAmpGain = kVCAGainConstant * kPlus6dB;
    static const float kFreqInputR = 100e3f;
    static const float kFreqAmpR = -kFreqAmpGain * kFreqInputR;
    static const float kFreqAmpC = 560e-12f;

    // Generic amplifier feedback resistor for Ripples.
    static const float kGenericAmpR = 47e3f;

    // Resonance CV amplifier
    static const float kResInputR = 22e3f;
    static const float kResKnobV = 12.f;
    static const float kResKnobR = 62e3f;
    static const float kResAmpC = 560e-12f;

    // Gain CV amplifier
    static const float kGainInputR = 27e3f;
    static const float kGainNormalV = 12.f;
    static const float kGainNormalR = 15e3f;
    static const float kGainAmpC = 560e-12f;

    // Filter core
    static const float kFilterMaxCutoff = kFreqKnobMax;
    static const float kFilterCellR = 33e3f;
    static const float kFilterCellRC = 1.f / (2.f * M_PI * kFilterMaxCutoff);
    static const float kFilterCellC = kFilterCellRC / kFilterCellR;
    static const float kFilterInputR = 100e3f;
    static const float kFilterInputGain = kFilterCellR / kFilterInputR;
    static const float kFilterCellSelfModulation = 0.01f;

    // Filter core feedback path
    static const float kFeedbackRt = 22e3f;
    static const float kFeedbackRb = 1e3f;
    static const float kFeedbackR = kFeedbackRt + kFeedbackRb;
    static const float kFeedbackGain = kFeedbackRb / kFeedbackR;

    // Filter core feedforward path
    static const float kFeedforwardRt = 300e3f;
    static const float kFeedforwardRb = 1e3f;
    static const float kFeedforwardR = kFeedforwardRt + kFeedforwardRb;
    static const float kFeedforwardGain = kFeedforwardRb / kFeedforwardR;
    static const float kFeedforwardC = 220e-9f;

    // Filter output amplifiers
    static const float kLP2Gain = -100e3f / 39e3f;
    static const float kLP4Gain = -100e3f / 33e3f;
    static const float kBP2Gain = -100e3f / 39e3f;

    // VCA
    static const float kVCAInputC = 4.7e-6f;
    static const float kVCAInputRt = 100e3f;
    static const float kVCAInputRb = 1e3f;
    static const float kVCAInputR = kVCAInputRt + kVCAInputRb;
    static const float kVCAInputGain = kVCAInputRb / kVCAInputR;
    static const float kVCAOutputR = 100e3f;

    // Voltage-to-current converters
    // Saturation voltage at BJT collector
    static const float kVtoICollectorVSat = -10.f;

    // Opamp saturation voltage
    static const float kOpampSatV = 10.6f;

    static const float kTemperature = 40.f; // Silicon temperature in Celsius
    static const float kKoverQ = 8.617333262145e-5;
    static const float kKelvin = 273.15f; // 0C in K
    static const float kVt = kKoverQ * (kTemperature + kKelvin);
    static const float kZlim = 2.f * std::sqrt(3.f);

    class UndaeEngine {
    public:
        struct Frame {
            float knobResonance;    //  0 to 1 linear
            float knobFrequency;    //  0 to 1 linear
            float knobFm;           // -1 to 1 linear

            float cvResonance;
            float cvFrequency;
            float cvFm;
            float input;
            float cvGain;

            float bp2;
            float lp2;
            float lp4;
            float lp4vca;

            bool bGainConnected;
            bool bBp2Connected;
            bool bLp2Connected;
            bool bLp4Connected;
            bool bLp4VcaConnected;
        };

        UndaeEngine() {
            setSampleRate(1.f);
        }

        void setSampleRate(float newSampleRate) {
            sampleTime = 1.f / newSampleRate;
            cellVoltages = 0.f;

            aaFilter_.Init(newSampleRate);

            float oversample_rate = newSampleRate * aaFilter_.GetOversamplingFactor();

            float cutFrequency = 1.f / (2.f * M_PI * kFreqAmpR * kFreqAmpC);
            float cutResonance = 1.f / (2.f * M_PI * kGenericAmpR * kResAmpC);
            float cutGain = 1.f / (2.f * M_PI * kGenericAmpR * kGainAmpC);
            float cutFf = 1.f / (2.f * M_PI * kFeedforwardR * kFeedforwardC);

            simd::float_4 cutoffs = simd::float_4(cutFf, cutFrequency, cutResonance, cutGain);
            rcFilters_.setCutoffFreq(cutoffs / oversample_rate);

            float vcaCut = 1.f / (2.f * M_PI * kVCAInputR * kVCAInputC);
            vcaHpf_.setCutoffFreq(vcaCut / oversample_rate);
        }

        void process(Frame& frame) {
            // Calculate equivalent frequency CV
            float vOct = (frame.knobFrequency - 1.f) * kFreqKnobVoltage;
            vOct += frame.cvFrequency;
            vOct += frame.cvFm * frame.knobFm;
            vOct = std::min(vOct, 0.f);

            // Calculate resonance control current
            float iResonance = VtoIConverter(frame.cvResonance, kResInputR,
                frame.knobResonance * kResKnobV, kResKnobR);

            // Calculate gain control current
            float cvGain;
            float gainInputR = kGainInputR;
            if (!frame.bGainConnected) {
                cvGain = kGainNormalV;
                gainInputR += kGainNormalR;
            } else {
                cvGain = frame.cvGain;
            }
            float iVca = VtoIConverter(cvGain, gainInputR);

            // Pack and upsample inputs
            int oversamplingFactor = aaFilter_.GetOversamplingFactor();
            float timestep = sampleTime / oversamplingFactor;
            // Add noise to input to bootstrap self-oscillation
            float input = frame.input + 1e-6 * (random::uniform() - 0.5f);
            simd::float_4 inputs = simd::float_4(input, vOct, iResonance, iVca);
            inputs *= oversamplingFactor;
            simd::float_4 outputs;

            for (int sample = 0; sample < oversamplingFactor; ++sample) {
                inputs = aaFilter_.ProcessUp((sample == 0) ? inputs : 0.f);
                outputs = CoreProcess(inputs, timestep, frame);
                outputs = aaFilter_.ProcessDown(outputs);
            }

            frame.bp2 = outputs[0];
            frame.lp2 = outputs[1];
            frame.lp4 = outputs[2];
            frame.lp4vca = outputs[3];
        }

    protected:
        float sampleTime;
        simd::float_4 cellVoltages;
        undae::AAFilter<simd::float_4> aaFilter_;
        dsp::TRCFilter<simd::float_4> rcFilters_;
        dsp::TRCFilter<float> vcaHpf_;

        /*
        High-rate processing core
        inputs: vector containing (input, vOct, iReso, iVca)
        returns: vector containing (bp2, lp2, lp4, lp4vca)
        */
        simd::float_4 CoreProcess(simd::float_4 inputs, float timestep, Frame& frame) {
            rcFilters_.process(inputs);

            // Lowpass the control signals
            simd::float_4 control = rcFilters_.lowpass();
            float vOct = control[1];
            float iResonance = control[2];
            float iVca = control[3];

            // Highpass the input signal to generate the resonance feedforward
            float feedForward = rcFilters_.highpass()[0];

            /*
            The 2164's input terminal is a virtual ground, so we can model the
            vca-integrator cell like so:
                                       ___
                          ┌───────────┤___├───────────┝
                          │             R             │
                          │                  ┌───┤├───┤
                          │           A*ix   │    C   │
                    ___   │          ┌───┝   │ │╲     │
             vIn ──┤___├──┤        ┌─┤ → ├───┴─┤-╲____├── vOut
                     R    │ ↓ix    │ └───┘   ┌─┤+╱
                          ╧        ╧         ╧ │╱
                         We can see that:
               ix = (vIn + vOut) / R
               A*ix = -C * dvout/dt
             Thus,
               dvout/dt = -A/(RC) * (vIn + vOut)
            */

            // Calculate -A / RC
            simd::float_4 radPerS = -std::exp2f(vOct) / kFilterCellRC;

            // Emulate the filter core
            cellVoltages = StepRK2(timestep, cellVoltages, [&](simd::float_4 vOut) {
                // vOut contains the initial cell voltages (v0, v1 v2, v3)

                // Rotate cell voltages. vIn will contain (v3, v0, v1, v2)
                simd::float_4 vIn = _mm_shuffle_ps(vOut.v, vOut.v, _MM_SHUFFLE(2, 1, 0, 3));

                // The core input is the filter input plus the resonance signal
                float vp = feedForward * kFeedforwardGain;
                float vn = vOut[3] * kFeedbackGain;
                float res = kFilterCellR * OTAVCA(vp, vn, iResonance);
                simd::float_4 in = inputs[0] * kFilterInputGain + res;

                // Replace lowest element of vIn with lowest element from in
                vIn = _mm_move_ss(vIn.v, in.v);

                /*
                Now, vIn contains (in, v0, v1, v2)
                and vOut contains (v0, v1, v2, v3)
                Their sum gives us vIn + vOut for each cell.
                */
                simd::float_4 vSum = vIn + vOut;
                simd::float_4 dvout = radPerS * vSum;

                // Generate some even-order harmonics via self-modulation
                dvout *= (1.f + vSum * kFilterCellSelfModulation);

                return dvout;
                });

            cellVoltages = simd::clamp(cellVoltages, -kOpampSatV, kOpampSatV);

            float lp2 = cellVoltages[1];
            float lp4 = cellVoltages[3];
            float bp2 = lp2;
            if (frame.bBp2Connected) {
                bp2 = (cellVoltages[0] + bp2) * kBP2Gain;
            }
            float lp4vca = lp4;

            if (frame.bLp4VcaConnected) {
                vcaHpf_.process(lp4);
                lp4vca = vcaHpf_.highpass();
                lp4vca = -kVCAOutputR * OTAVCA(0.f, lp4vca * kVCAInputGain, iVca);
            }
            if (frame.bLp2Connected) {
                lp2 *= kLP2Gain;
            }
            if (frame.bLp4Connected) {
                lp4 *= kLP4Gain;
            }
            return simd::float_4(bp2, lp2, lp4, lp4vca);

        }

        // Solves an ODE system using the 2nd order Runge-Kutta method
        template <typename T, typename F>
        T StepRK2(float dt, T y, F f) {
            T k1 = f(y);
            T k2 = f(y + k1 * dt / 2.f);
            return y + dt * k2;
        }

        // Model of Ripples' nonlinear CV voltage-to-current converters.
        float VtoIConverter(float voltageCV, float inputResistor,
            float knobVoltage = 0.f, float knobResistor = 1e12f) {
            // Find nominal voltage at the BJT collector, ignoring nonlinearity.
            float voltageNominal = -(voltageCV * kGenericAmpR / inputResistor +
                knobVoltage * kGenericAmpR / knobResistor);

            // Apply clipping - naive for now.
            float vOut = std::max(voltageNominal, kVtoICollectorVSat);

            // Find voltage at the opamp's negative terminal
            float nrc = knobResistor * kGenericAmpR;
            float nrp = inputResistor * kGenericAmpR;
            float nrfb = inputResistor * knobResistor;
            float voltageNegative = (voltageCV * nrc + knobVoltage * nrp + vOut * nrfb) /
                (nrc + nrp + nrfb);

            // Find output current.
            float iout = (voltageNegative - vOut) / kGenericAmpR;

            return std::max(iout, 0.f);
        }

        /*
        Model of LM13700 OTA VCA, neglecting linearizing diodes
        returns: OTA output current
        */
        template <typename T>
        T OTAVCA(T voltagePositive, T voltageNegative, T amplifierBiasCurrent) {
            /*
            For the derivation of this equation, see this fantastic paper:
              http://www.openmusiclabs.com/files/otadist.pdf
            Thanks guest!

              i_out = amplifierBiasCurrent * (e^(vi/vt) - 1) / (e^(vi/vt) + 1)
            or equivalently,
              i_out = amplifierBiasCurrent * tanh(vi / (2vt))
            */

            T vi = voltagePositive - voltageNegative;
            T zlim = kZlim;
            T z = math::clamp(vi / (2 * kVt), -zlim, zlim);

            // Pade approximant of tanh(z)
            T z2 = z * z;
            T q = 12.f + z2;
            T p = 12.f * z * q / (36.f * z2 + q * q);

            return amplifierBiasCurrent * p;
        }
    };
}