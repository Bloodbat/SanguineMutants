#include <stdio.h>
#include <stdlib.h>

#include "../renaissance_resources.h"
#include "vocalist.h"
#include "wordlist.h"

void Vocalist::Init(VocalistState *s) {
    state = s;

    sam.Init(&state->samState);
    state->phase = 0;
    state->bank = 0;
    state->offset = 0;
    state->word = -1;
    
    SetWord(0);
}

void Vocalist::set_shape(int shape) {
  if (state->bank != shape) {
    state->bank = shape;
    Load();
  }
}

void Vocalist::SetWord(unsigned char w) {
  if (state->word != w) {
    state->word = w;
    Load();
  }
}

void Vocalist::Load() {
  state->scan = false;
  state->phase = 0;
  sam.LoadTables(&data[wordpos[state->bank][state->word]]);
  sam.InitFrameProcessor();

  state->doubleAbsorbOffset_ = &doubleAbsorbOffset[doubleAbsorbPos[state->bank][state->word]];
  state->doubleAbsorbLen_ = doubleAbsorbLen[state->bank][state->word];
}

static const uint16_t kHighestNote = 140 * 128;
static const uint16_t kPitchTableStart = 128 * 128;
static const uint16_t kOctave = 12 * 128;

uint32_t ComputePhaseIncrement(int16_t midi_pitch) {
  if (midi_pitch >= kPitchTableStart) {
    midi_pitch = kPitchTableStart - 1;
  }
  
  int32_t ref_pitch = midi_pitch;
  ref_pitch -= kPitchTableStart;
  
  size_t num_shifts = 0;
  while (ref_pitch < 0) {
    ref_pitch += kOctave;
    ++num_shifts;
  }
  
  uint32_t a = renaissance::lut_oscillator_increments[ref_pitch >> 4];
  uint32_t b = renaissance::lut_oscillator_increments[(ref_pitch >> 4) + 1];
  uint32_t phase_increment = a + \
      (static_cast<int32_t>(b - a) * (ref_pitch & 0xf) >> 4);
  phase_increment >>= num_shifts;
  return phase_increment;
}

// in braids, one cycle is 65536 * 65536 in the phase increment.
// and since SAM is outputting 155.6Hz at 22050 sample rate, one SAM cycle is (22050/155.6) samples
// so every 65536 * 65536 / (22050/155.6) phase increment, we consume 1 SAM sample.
// but we precompute to get an accurate number without integer overflow or rounding
const uint32_t kPhasePerSample = 30308250;

void Vocalist::Render(const uint8_t *sync, int16_t *output, int len) {
  int written = 0;
  unsigned char sample;
  int phase_increment = ComputePhaseIncrement(state->braids_pitch);
  unsigned char samplesToLoad = 0;

  while (written < len) {
    // if (*sync++) {
    //   phase = 0;
    //   sam.InitFrameProcessor();
    //   sam.SetFramePosition(validOffset_[offset]);

    //   // reload first two samples
    //   samplesToLoad = 2;
    // }

    while (state->phase > kPhasePerSample) {
      samplesToLoad++;
      state->phase -= kPhasePerSample;
    }

    while (samplesToLoad > 0) {
      int wrote = sam.Drain(0, 1, &sample);
      if (wrote) {
        // there was data in the SAM buffer
        state->samples[0] = state->samples[1];
        state->samples[1] = (((int16_t) sample)-127) << 8;
        samplesToLoad--;
      } else {
        // no data remaining in frame buffer, update frame position
        if (state->scan) {
          if (sam.state->framesRemaining == 0) {
            state->scan = false;
          }
        }
        if (!state->scan) {
          // not scanning, force frame processor to remain on current frame
          if (sam.state->frameProcessorPosition != state->targetOffset) {
            // note this resets glottal pulse, and now that we modify frameProcessorPosition below that might
            // be unwanted. If this sounds worse, only modify frameProcessorPosition when scanning.
            sam.SetFramePosition(state->targetOffset);
          }
        }
        // load a new frame into sample buffer and update frame processor position
        unsigned char absorbed = sam.ProcessFrame(sam.state->frameProcessorPosition, sam.state->framesRemaining);
        if (state->scan) {
          sam.state->frameProcessorPosition += absorbed;
          sam.state->framesRemaining -= absorbed;
        }
      }
    }

    // TODO: try interpolation although I suspect it won't feel right
    output[written++] = state->samples[0];
    state->phase += phase_increment;
  }
}

void Vocalist::set_parameters(uint16_t parameter1, uint16_t parameter2)
{
  SetWord(parameter2 >> 11);

  // max parameter2 is 32767, divisor must be higher so max offset is sam.totalFrames-1
  if (parameter1 > 32767) {
    parameter1 = 32767;
  }
  unsigned char offsetLen = sam.state->totalFrames - state->doubleAbsorbLen_;
  uint16_t offset = offsetLen * parameter1 / 32768;
  for (unsigned char i = 0; i < state->doubleAbsorbLen_ && state->doubleAbsorbOffset_[i] < offset; i++) {
    offset++;
  }
  state->targetOffset = offset;
}

void Vocalist::set_pitch(uint16_t pitch) {
  state->braids_pitch = pitch;
}

void Vocalist::Strike() {
  state->scan = true;
  sam.SetFramePosition(state->targetOffset);
}
