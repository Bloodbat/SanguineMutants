#pragma once

#include "plugin.hpp"
#include "sanguinecomponents.hpp"

namespace nodiCommon {
	static const std::vector<uint16_t> bitReductionMasks = {
		0xc000,
		0xe000,
		0xf000,
		0xf800,
		0xff00,
		0xfff0,
		0xffff
	};

	static const std::vector<uint16_t> decimationFactors = {
		24,
		12,
		6,
		4,
		3,
		2,
		1
	};

	static const std::vector<std::string> numberStrings15 = {
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

	static const std::vector<std::string> intensityDisplayStrings = {
		"OFF ",
		"   1",
		"   2",
		"   3",
		"FULL"
	};

	static const std::vector<std::string> intensityTooltipStrings = {
		"OFF",
		"1",
		"2",
		"3",
		"FULL"
	};

	static const std::vector<std::string> bitsStrings = {
		"2BIT",
		"3BIT",
		"4BIT",
		"6BIT",
		"8BIT",
		"12B",
		"16B "
	};

	static const std::vector<std::string> ratesStrings = {
		"4KHZ",
		"8KHZ",
		"16K ",
		"24K ",
		"32K ",
		"48K ",
		"96K "
	};

	static const std::vector<std::string> pitchRangeStrings = {
		"EXT.",
		"FREE",
		"XTND",
		"440 ",
		"LFO "
	};

	static const std::vector<std::string> octaveStrings = {
		"-2",
		"-1",
		"0",
		"1",
		"2"
	};

	static const std::string quantizationStrings[49] = {
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

	static const std::vector<std::string> triggerDelayStrings = {
		"NONE",
		"125u",
		"250u",
		"500u",
		"1ms ",
		"2ms ",
		"4ms "
	};

	static const std::vector<std::string> noteStrings = {
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

	static const std::vector<std::string> scaleStrings = {
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
		"1/4 Eb",
		"1/4 E",
		"1/4 Ea",
		"Bhairav",
		"Gunakri",
		"Marwa",
		"Shree [Camel]",
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
		"Pa Khafi",
		"Natbhairav",
		"Malkauns",
		"Bairagi",
		"B Todi",
		"Chandradeep",
		"Kaushik Todi",
		"Jogeshwari"
	};

	static const std::string autoLabel = "AUTO";
	static const std::string vcaLabel = "\\VCA";
	static const std::string flatLabel = "FLAT";

	static const int kBlockSize = 24;

	struct NodiDisplay : SanguineAlphaDisplay {
		uint32_t* displayTimeout = nullptr;

		NodiDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered = true);

		void drawLayer(const DrawArgs& args, int layer) override;
	};
}