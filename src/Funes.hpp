#pragma once
namespace funes {
  static const char CUSTOM_DATA_FILENAME_FILTERS[] = "BIN (*.bin):bin, BIN";
  static std::string customDataDir;

  static const std::string displayLabels[24] = {
    "FLTRWAVE",
    "PHASDIST",
    "6 OP.FM1",
    "6 OP.FM2",
    "6 OP.FM3",
    "WAVETRRN",
    "STRGMACH",
    "CHIPTUNE",
    "DUALWAVE",
    "WAVESHAP",
    "2 OP.FM",
    "GRANFORM",
    "HARMONIC",
    "WAVETABL",
    "CHORDS",
    "VOWLSPCH",
    "GR.CLOUD",
    "FLT.NOIS",
    "PRT.NOIS",
    "STG.MODL",
    "MODALRES",
    "BASSDRUM",
    "SNARDRUM",
    "HI-HAT",
  };

  static const std::vector<std::string> frequencyModes = {
      "LFO mode",
      "C0 +/- 7 semitones",
      "C1 +/- 7 semitones",
      "C2 +/- 7 semitones",
      "C3 +/- 7 semitones",
      "C4 +/- 7 semitones",
      "C5 +/- 7 semitones",
      "C6 +/- 7 semitones",
      "C7 +/- 7 semitones",
      "Octaves",
      "C0 to C8"
  };

  static const std::vector<std::string> modelLabels = {
      "Classic waveshapes with filter",
      "Phase distortion",
      "6-operator FM 1",
      "6-operator FM 2",
      "6-operator FM 3",
      "Wave terrain synthesis",
      "String machine",
      "Chiptune",
      "Pair of classic waveforms",
      "Waveshaping oscillator",
      "Two operator FM",
      "Granular formant oscillator",
      "Harmonic oscillator",
      "Wavetable oscillator",
      "Chords",
      "Vowel and speech synthesis",
      "Granular cloud",
      "Filtered noise",
      "Particle noise",
      "Inharmonic string modeling",
      "Modal resonator",
      "Analog bass drum",
      "Analog snare drum",
      "Analog hi-hat"
  };

  static const std::vector<std::string> chordBankLabels = {
      "Original",
      "Alternate 1 (Jon Butler)",
      "Alternate 2 (Joe McMullen)"
  };  

  enum CustomDataStates {
    DataNotAvailable,
    DataFactory,
    DataCustom
  };

  enum SuboscillatorModes {
    SUBOSCILLATOR_OFF,
    SUBOSCILLATOR_SQUARE,
    SUBOSCILLATOR_SINE,
    SUBOSCILLATOR_SINE_MINUS_ONE,
    SUBOSCILLATOR_SINE_MINUS_TWO
  };

  static const std::vector<std::string> suboscillatorLabels = {
    "Off",
    "Square",
    "Sine",
    "Sine minus 1 octave",
    "Sine minus 2 octaves",
  };
}