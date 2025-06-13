/*
   Copyright 2011 Emilie Gillet.

   Author: Emilie Gillet (emilie.o.gillet@gmail.com)

   This source code is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
   -----------------------------------------------------------------------------
   Pattern generator.
   OUTPUT MODE  OUTPUT CLOCK  BIT7  BIT6  BIT5  BIT4  BIT3  BIT2  BIT1  BIT0
   DRUMS        FALSE          RND   CLK  HHAC  SDAC  BDAC    HH    SD    BD
   DRUMS        TRUE           RND   CLK   CLK   BAR   ACC    HH    SD    BD
   EUCLIDEAN    FALSE          RND   CLK  RST3  RST2  RST1  EUC3  EUC2  EUC1
   EUCLIDEAN    TRUE           RND   CLK   CLK  STEP   RST  EUC3  EUC2  EUC1
*/

#pragma once

#pragma GCC diagnostic ignored "-Wunused-variable"

#include <cstdlib>
#include <cmath>
#include "reticula_resources.hpp"

namespace reticula {
    const uint8_t kNumParts = 3;
    const uint8_t kPulsesPerStep = 3;  // 24 ppqn ; 8 steps per quarter note.
    const uint8_t kStepsPerPattern = 32;
    const uint8_t kPulseDuration = 8;  // 8 ticks of the main clock.

    uint8_t U8U8MulShift8(uint8_t a, uint8_t b);
    uint8_t U8Mix(uint8_t a, uint8_t b, uint8_t balance);

    enum OutputBits {
        OUTPUT_BIT_COMMON = 0x08,
        OUTPUT_BIT_CLOCK = 0x10,
        OUTPUT_BIT_RESET = 0x20
    };

    static const uint8_t* drumMaps[5][5] = {
        { node10, node8, node0, node9, node11 },
        { node15, node7, node13, node12, node6 },
        { node18, node_14, node4, node5, node3 },
        { node23, node16, node21, node1, node2 },
        { node24, node19, node17, node20, node22 },
    };

    enum PatternGeneratorModes {
        PATTERN_ORIGINAL,
        PATTERN_HENRI,
        PATTERN_EUCLIDEAN
    };

    enum ClockResolutions {
        CLOCK_RESOLUTION_4_PPQN,
        CLOCK_RESOLUTION_8_PPQN,
        CLOCK_RESOLUTION_24_PPQN
    };

    const uint8_t ticksGranularities[] = {
        6,
        3,
        1
    };

    struct PatternGeneratorOptions {
        PatternGeneratorOptions() {
            x = 0;
            y = 0;
            randomness = 0;
            for (int i = 0; i < kNumParts; ++i) {
                euclideanLengths[i] = 255;
                densities[i] = 0;
            }
            patternMode = PATTERN_ORIGINAL;
            swing = false;
        }

        uint8_t x;
        uint8_t y;
        uint8_t randomness;
        uint8_t euclideanLengths[kNumParts];
        uint8_t densities[kNumParts];
        PatternGeneratorModes patternMode;
        bool swing;
    };

    class PatternGenerator {
    public:
        PatternGenerator();
        void tick(uint8_t numPulses);
        void reset();

        void setMapX(float x);
        void setMapY(float y);
        void setBDDensity(float density);
        void setSDDensity(float density);
        void setHHDensity(float density);
        void setEuclideanLength(unsigned int channel, float length);
        void setRandomness(float randomness);
        void setPatternMode(PatternGeneratorModes mode);

        uint8_t getDrumState(uint8_t channel) const;
        uint8_t getBeat() const;
        bool getFirstBeat();
        bool getClockState() const;
        bool getResetState() const;

    private:
        PatternGeneratorOptions settings;
        uint8_t pulse;
        uint8_t step;
        uint8_t euclideanSteps[3];
        uint8_t accentBits;

        uint8_t partPerturbations[kNumParts];
        uint8_t readDrumMap(uint8_t step, uint8_t instrument, uint8_t x, uint8_t y);
        uint8_t state;
        uint8_t controlState;
        bool firstBeat;
        bool beat;

        void evaluate();
        void evaluateEuclidean();
        void evaluateDrums();
    };
};