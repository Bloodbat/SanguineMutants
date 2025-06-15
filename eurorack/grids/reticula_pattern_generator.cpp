/*
    Copyright 2011 Emilie Gillet.

    Author: Emilie Gillet (emilie.o.gillet@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "reticula_pattern_generator.hpp"

namespace reticula {
    uint8_t U8U8MulShift8(uint8_t a, uint8_t b) {
        return (a * b) >> 8;
    }

    uint8_t U8Mix(uint8_t a, uint8_t b, uint8_t balance) {
        return (a * (255 - balance) + b * balance) / 255;
    }

    PatternGenerator::PatternGenerator() {
        pulse = 0;
        beat = false;
        firstBeat = false;
        step = 0;
        for (int i = 0; i < 3; ++i) {
            euclideanSteps[i] = 0;
        }
        state = 0;
        controlState = 0;
    }

    void PatternGenerator::tick(uint8_t numPulses) {
        evaluate();
        beat = (step & 0x7) == 0;
        firstBeat = step == 0;
        pulse += numPulses;

        // Wrap into ppqn steps.
        while (pulse >= kPulsesPerStep) {
            pulse -= kPulsesPerStep;
            if (!(step & 1)) {
                for (uint8_t i = 0; i < kNumParts; ++i) {
                    ++euclideanSteps[i];
                }
            }
            ++step;
        }

        // Wrap into step sequence steps.
        if (step >= kStepsPerPattern) {
            step -= kStepsPerPattern;
        }
    }

    void PatternGenerator::reset() {
        step = 0;
        pulse = 0;
        for (int i = 0; i < 3; ++i) {
            euclideanSteps[i] = 0;
        }
    }

    void PatternGenerator::setMapX(float x) {
        settings.x = static_cast<uint8_t>(x);
    }

    void PatternGenerator::setMapY(float y) {
        settings.y = static_cast<uint8_t>(y);
    }

    void PatternGenerator::setBDDensity(float density) {
        settings.densities[0] = static_cast<uint8_t>(density);
    }

    void PatternGenerator::setSDDensity(float density) {
        settings.densities[1] = static_cast<uint8_t>(density);
    }

    void PatternGenerator::setHHDensity(float density) {
        settings.densities[2] = static_cast<uint8_t>(density);
    }

    void PatternGenerator::setEuclideanLength(unsigned int channel, float length) {
        settings.euclideanLengths[channel] = static_cast<uint8_t>(length);
    }

    void PatternGenerator::setRandomness(float randomness) {
        settings.randomness = static_cast<uint8_t>(randomness);
    }

    void PatternGenerator::setPatternMode(PatternGeneratorModes mode) {
        settings.patternMode = mode;
    }

    uint8_t PatternGenerator::getDrumState(uint8_t channel) const {
        const uint8_t mask[6] = {
            1,
            2,
            4,
            8,
            16,
            32
        };
        return (state & mask[channel]) >> channel;
    }

    uint8_t PatternGenerator::getBeat() const {
        return beat;
    }

    bool PatternGenerator::getFirstBeat() {
        return firstBeat;
    }

    bool PatternGenerator::getClockState() const {
        return static_cast<bool>((controlState & OUTPUT_BIT_CLOCK) >> 4);
    }

    bool PatternGenerator::getResetState() const {
        return static_cast<bool>((controlState & OUTPUT_BIT_RESET) >> 5);
    }

    uint8_t PatternGenerator::readDrumMap(uint8_t step, uint8_t instrument, uint8_t x, uint8_t y) {
        uint8_t r = 0;
        if (settings.patternMode == PATTERN_ORIGINAL) {
            uint8_t i = x >> 6;
            uint8_t j = y >> 6;
            const uint8_t* a_map = drumMaps[i][j];
            const uint8_t* b_map = drumMaps[i + 1][j];
            const uint8_t* c_map = drumMaps[i][j + 1];
            const uint8_t* d_map = drumMaps[i + 1][j + 1];
            uint8_t offset = (instrument * kStepsPerPattern) + step;
            uint8_t a = *(a_map + offset);
            uint8_t b = *(b_map + offset);
            uint8_t c = *(c_map + offset);
            uint8_t d = *(d_map + offset);
            r = U8Mix(U8Mix(a, b, x << 2), U8Mix(c, d, x << 2), y << 2);
        } else {
            uint8_t i = static_cast<int>(floor(x * 3.f / 255.f));
            uint8_t j = static_cast<int>(floor(y * 3.f / 255.f));
            const uint8_t* a_map = drumMaps[i][j];
            const uint8_t* b_map = drumMaps[i + 1][j];
            const uint8_t* c_map = drumMaps[i][j + 1];
            const uint8_t* d_map = drumMaps[i + 1][j + 1];
            uint8_t offset = (instrument * kStepsPerPattern) + step;
            uint8_t a = a_map[offset];
            uint8_t b = b_map[offset];
            uint8_t c = c_map[offset];
            uint8_t d = d_map[offset];
            uint8_t maxValue = 127;
            r = ((a * x + b * (maxValue - x)) * y + (c * x + d * (maxValue - x)) *
                (maxValue - y)) / maxValue / maxValue;
        }

        return r;
    }

    void PatternGenerator::evaluate() {
        state = controlState = 0x40;

        controlState |= OUTPUT_BIT_CLOCK;

        // Refresh only on step changes.
        if (pulse != 0) {
            return;
        }

        if (settings.patternMode == PATTERN_EUCLIDEAN) {
            evaluateEuclidean();
        } else {
            evaluateDrums();
        }
    }

    void PatternGenerator::evaluateDrums() {
        // At the beginning of a pattern, decide on perturbation levels.
        if (step == 0) {
            for (uint8_t i = 0; i < kNumParts; ++i) {
                uint8_t randomNum = static_cast<uint8_t>(rand() % 256);
                uint8_t randomness = settings.swing ? 0 : settings.randomness >> 2;
                partPerturbations[i] = U8U8MulShift8(randomNum, randomness);
            }
        }

        uint8_t instrumentMask = 1;
        uint8_t x = settings.x;
        uint8_t y = settings.y;
        uint8_t accentBits = 0;
        for (uint8_t i = 0; i < kNumParts; ++i) {
            uint8_t level = readDrumMap(step, i, x, y);
            if (level < 255 - partPerturbations[i]) {
                level += partPerturbations[i];
            } else {
                /* The sequencer from Anushri uses a weird clipping rule here. Comment
                   this line to reproduce its behavior. */
                level = 255;
            }
            uint8_t threshold = ~settings.densities[i];
            if (level > threshold) {
                if (level > 192) {
                    accentBits |= instrumentMask;
                }
                state |= instrumentMask;
                controlState |= instrumentMask;
            }
            instrumentMask <<= 1;
        }

        controlState |= accentBits ? OUTPUT_BIT_COMMON : 0;
        controlState |= step == 0 ? OUTPUT_BIT_RESET : 0;

        state |= accentBits << 3;
    }

    void PatternGenerator::evaluateEuclidean() {
        // Refresh only on sixteenth notes.
        if (step & 1) {
            return;
        }

        // Euclidean pattern generation.
        uint8_t instrumentMask = 1;
        uint8_t resetBits = 0;
        for (uint8_t i = 0; i < kNumParts; ++i) {
            uint8_t length = (settings.euclideanLengths[i] >> 3) + 1;
            uint8_t density = settings.densities[i] >> 3;
            uint16_t address = (length - 1) * 32 + density;
            while (euclideanSteps[i] >= length) {
                euclideanSteps[i] -= length;
            }
            uint32_t stepMask = 1L << static_cast<uint32_t>(euclideanSteps[i]);
            uint32_t patternBits = *(lutResEuclidean + address);
            if (patternBits & stepMask) {
                state |= instrumentMask;
                controlState |= instrumentMask;
            }
            if (euclideanSteps[i] == 0) {
                resetBits |= instrumentMask;
            }
            instrumentMask <<= 1;
        }

        controlState |= resetBits ? OUTPUT_BIT_COMMON : 0;
        controlState |= (resetBits == 0x07) ? OUTPUT_BIT_RESET : 0;

        state |= resetBits << 3;
    }
};