#pragma once

#include "sanguinecomponents.hpp"
#include "plugin.hpp"

static const uint16_t nodiBitReductionMasks[7] = {
	0xc000,
	0xe000,
	0xf000,
	0xf800,
	0xff00,
	0xfff0,
	0xffff
};

static const uint16_t nodiDecimationFactors[7] = {
	24,
	12,
	6,
	4,
	3,
	2,
	1
};

static const std::string nodiNumberStrings[16] = {
	"   0",
	"   1",
	"   2",
	"   3",
	"   4",
	"   5",
	"   6",
	"   7",
	"   8",
	"   9",
	"  10",
	"  11",
	"  12",
	"  13",
	"  14",
	"  15"
};

static const std::vector<std::string> nodiBitsStrings = {
	"2BIT",
	"3BIT",
	"4BIT",
	"6BIT",
	"8BIT",
	"12B",
	"16B "
};

static const std::vector<std::string> nodiRatesStrings = {
	"4KHZ",
	"8KHZ",
	"16K ",
	"24K ",
	"32K ",
	"48K ",
	"96K "
};

static const std::vector<std::string> nodiPitchRangeStrings = {
	"EXT-",
	"FREE",
	"XTND",
	"440 ",
	"LFO "
};

static const std::vector<std::string> nodiOctaveStrings = {
	"-2",
	"-1",
	"0",
	"1",
	"2"
};

static const std::string nodiQuantizationStrings[49] = {
	"OFF ",
	"SEMI",
	"IONI",
	"DORI",
	"PHRY",
	"LYDI",
	"MIXO",
	"AEOL",
	"LOCR",
	"BLU+",
	"BLU-",
	"PEN+",
	"PEN-",
	"FOLK",
	"JAPA",
	"GAME",
	"GYPS",
	"ARAB",
	"FLAM",
	"WHOL",
	"PYTH",
	"EB/4",
	"E /4",
	"EA/4",
	"BHAI",
	"GUNA",
	"MARW",
	"SHRI",
	"PURV",
	"BILA",
	"YAMA",
	"KAFI",
	"BHIM",
	"DARB",
	"RAGE",
	"KHAM",
	"MIMA",
	"PARA",
	"RANG",
	"GANG",
	"KAME",
	"PAKA",
	"NATB",
	"KAUN",
	"BAIR",
	"BTOD",
	"CHAN",
	"KTOD",
	"JOGE"
};

static const std::vector<std::string> nodiTriggerDelayStrings = {
	"NONE",
	"125u",
	"250u",
	"500u",
	"1ms ",
	"2ms ",
	"4ms "
};

static const std::vector<std::string> nodiNoteStrings = {
	"C",
	"Db",
	"D",
	"Eb",
	"E",
	"F",
	"Gb",
	"G",
	"Ab",
	"A",
	"Bb",
	"B",
};

static const std::vector<std::string> nodiScaleStrings = {
	"Off",
	"Semitones",
	"Ionian",
	"Dorian",
	"Phrygian",
	"Lydian",
	"Mixolydian",
	"Aeolian",
	"Locrian",
	"Blues major",
	"Blues minor",
	"Pentatonic major",
	"Pentatonic minor",
	"Folk",
	"Japanese",
	"Gamelan",
	"Gypsy",
	"Arabian",
	"Flamenco",
	"Whole tone",
	"Pythagorean",
	"1_4_Eb",
	"1_4_E",
	"1_4_Ea",
	"Bhairav",
	"Gunakri",
	"Marwa",
	"Shree",
	"Purvi",
	"Bilawal",
	"Yaman",
	"Kafi",
	"Bhimpalasree",
	"Darbari",
	"Rageshree",
	"Khamaj",
	"Mimal",
	"Parameshwari",
	"Rangeshwari",
	"Gangeshwari",
	"Kameshwari",
	"Pa__Kafi",
	"Natbhairav",
	"M_Kauns",
	"Bairagi",
	"B_todi",
	"Chandradeep",
	"Kaushik_todi",
	"Jogeshwari"
};

static const std::string nodiMetaLabel = "META";
static const std::string nodiAutoLabel = "AUTO";
static const std::string nodiVCALabel = "\\VCA";
static const std::string nodiFlatLabel = "FLAT";
static const std::string nodiDriftLabel = "DRFT";
static const std::string nodiSignLabel = "SIGN";

struct NodiDisplay : SanguineAlphaDisplay {
	uint32_t* displayTimeout = nullptr;

	NodiDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered = true);

	void drawLayer(const DrawArgs& args, int layer) override;
};