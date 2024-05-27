#ifndef __VOCALIST_H__
#define __VOCALIST_H__

#include <stdint.h>
#include "wordlist.h"

#define NUM_VOCALIST_PATCHES NUM_BANKS

#include "sam.h"

struct VocalistState {
  int16_t samples[2];
  uint32_t phase;

  uint16_t braids_pitch;
  uint16_t offset;
  uint16_t targetOffset;

  unsigned char bank;
  unsigned char word;
  bool scan;
  const unsigned char *doubleAbsorbOffset_;
  unsigned char doubleAbsorbLen_;

  struct SamState samState;
};

class Vocalist {
public:
  Vocalist() {
  }

  ~Vocalist() { }

  void Init(VocalistState *s);
  void set_shape(int shape);

  void Render(const uint8_t* sync_buffer, int16_t *output, int len);
  void set_gatestate(bool gs);
  void Strike();
  // void SetBank(unsigned char b);
  void SetWord(unsigned char b);

  void set_pitch(uint16_t pitch);
  void set_parameters(uint16_t parameter1, uint16_t parameter2);
  
private:
  void Load();

  SAM sam;
  struct VocalistState *state;
};

#endif
