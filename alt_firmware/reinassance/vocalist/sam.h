#ifndef SAM_H
#define SAM_H

#include <stdint.h>

#define MAX_TINY_BUFFER 500

struct SamState {
  unsigned short tinyBufferSize; // this is in a weird "actual size * 50" unit because that's what the generated code uses elsewhere for some calculations
  unsigned short tinyBufferStart; // this is a direct index into tinyBuffer where the ring buffer begins.

  // ---- sam.cc
  unsigned char speed;

  // ---- render.cc
  const unsigned char *pitches; // tab43008

  const unsigned char *frequency1;
  const unsigned char *frequency2;
  const unsigned char *frequency3;

  const unsigned char *amplitude1;
  const unsigned char *amplitude2;
  const unsigned char *amplitude3;

  const unsigned char *sampledConsonantFlag; // tab44800

  //processframes.cc
  unsigned char framesRemaining;
  unsigned char totalFrames;
  unsigned char frameProcessorPosition;

  unsigned char speedcounter;
  unsigned char phase1;
  unsigned char phase2;
  unsigned char phase3;
  
  unsigned char mem66;
  unsigned char glottal_pulse;
  unsigned char mem38;

  char tinyBuffer[500];
};


class SAM {

public:
  SAM() {
  }

  ~SAM() { }

  void Init(SamState *s);
  void LoadTables(const unsigned char *data);
  unsigned char RLEGet(const unsigned char *rleData, unsigned char idx);

  // ---- render.cc

  void Output(int index, unsigned char A);
  void PrepareFrames();
  void SetMouthThroat(unsigned char mouth, unsigned char throat);

  void RenderSample(unsigned char *mem66, unsigned char consonantFlag, unsigned char mem49);
  unsigned char RenderVoicedSample(unsigned short hi, unsigned char off, unsigned char phase1);
  void RenderUnvoicedSample(unsigned short hi, unsigned char off, unsigned char mem53);
  void CreateFrames();
  void RescaleAmplitude();
  void AssignPitchContour();
  void AddInflection(unsigned char inflection, unsigned char phase1, unsigned char pos);

  // processframes.cc
  void InitFrameProcessor();

  //void ProcessFrames(unsigned char mem48, int *bufferpos, char *buffer);
  int Drain(int threshold, int count, uint8_t *buffer);
  
  int FillBufferFromFrame(int count, uint8_t *buffer);
  void SetFramePosition(int pos);
  unsigned char ProcessFrame(unsigned char Y, unsigned char mem48);
  void CombineGlottalAndFormants(unsigned char phase1, unsigned char phase2, unsigned char phase3, unsigned char Y);

  struct SamState *state;
};

//char input[]={"/HAALAOAO MAYN NAAMAEAE IHSTT SAEBAASTTIHAAN \x9b\x9b\0"};
//unsigned char input[]={"/HAALAOAO \x9b\0"};
//unsigned char input[]={"AA \x9b\0"};
//unsigned char input[] = {"GUH5DEHN TAEG\x9b\0"};

//unsigned char input[]={"AY5 AEM EY TAO4LXKIHNX KAX4MPYUX4TAH. GOW4 AH/HEH3D PAHNK.MEYK MAY8 DEY.\x9b\0"};
//unsigned char input[]={"/HEH3LOW2, /HAW AH YUX2 TUXDEY. AY /HOH3P YUX AH FIYLIHNX OW4 KEY.\x9b\0"};
//unsigned char input[]={"/HEY2, DHIHS IH3Z GREY2T. /HAH /HAH /HAH.AYL BIY5 BAEK.\x9b\0"};
//unsigned char input[]={"/HAH /HAH /HAH \x9b\0"};
//unsigned char input[]={"/HAH /HAH /HAH.\x9b\0"};
//unsigned char input[]={".TUW BIY5Y3,, OHR NAA3T - TUW BIY5IYIY., DHAE4T IHZ DHAH KWEH4SCHAHN.\x9b\0"};
//unsigned char input[]={"/HEY2, DHIHS \x9b\0"};

//unsigned char input[]={" IYIHEHAEAAAHAOOHUHUXERAXIX  \x9b\0"};
//unsigned char input[]={" RLWWYMNNXBDGJZZHVDH \x9b\0"};
//unsigned char input[]={" SSHFTHPTKCH/H \x9b\0"};

//unsigned char input[]={" EYAYOYAWOWUW ULUMUNQ YXWXRXLX/XDX\x9b\0"};


#endif
