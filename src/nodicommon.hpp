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

static const std::string nodiBitsStrings[7] = {
	"2BIT",
	"3BIT",
	"4BIT",
	"6BIT",
	"8BIT",
	"12B",
	"16B "
};

static const std::string nodiRatesStrings[7] = {
	"4KHZ",
	"8KHZ",
	"16K ",
	"24K ",
	"32K ",
	"48K ",
	"96K "
};

static const std::string nodiPitchRangeStrings[5] = {
	"EXT.",
	"FREE",
	"XTND",
	"440 ",
	"LFO "
};

static const std::string nodiOctaveStrings[5] = {
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

static const std::string nodiTriggerDelayStrings[7] = {
	"NONE",
	"125u",
	"250u",
	"500u",
	"1ms ",
	"2ms ",
	"4ms "
};

static const std::string nodiNoteStrings[12] = {
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