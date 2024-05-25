#pragma once

static const std::string numberStrings[16] = {
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

static const std::string bitsStrings[7] = {
	"2BIT",
	"3BIT",
	"4BIT",
	"6BIT",
	"8BIT",
	"12B",
	"16B "
};

static const std::string ratesStrings[7] = {
	"4KHZ",
	"8KHZ",
	"16K ",
	"24K ",
	"32K ",
	"48K ",
	"96K "
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

static const std::string pitchRangeStrings[5] = {
	"EXT.",
	"FREE",
	"XTND",
	"440 ",
	"LFO "
};

static const std::string octaveStrings[5] = {
	"-2",
	"-1",
	"0",
	"1",
	"2"
};

static const std::string triggerDelayStrings[7] = {
	"NONE",
	"125u",
	"250u",
	"500u",
	"1ms ",
	"2ms ",
	"4ms "
};

static const std::string noteStrings[12] = {
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

struct ModelInfo {
	std::string code;
	std::string label;
};

static const std::vector<ModelInfo> modelInfos = {
	{"CSAW", "Quirky sawtooth"},
	{"/\\-_", "Triangle to saw"},
	{"//-_", "Sawtooth wave with dephasing"},
	{"FOLD", "Wavefolded sine/triangle"},
	{"uuuu", "Buzz"},
	{"SUB-", "Square sub"},
	{"SUB/", "Saw sub"},
	{"SYN-", "Square sync"},
	{"SYN/", "Saw sync"},
	{"//x3", "Triple saw"},
	{"-_x3", "Triple square"},
	{"/\\x3", "Triple triangle"},
	{"SIx3", "Triple sine"},
	{"RING", "Triple ring mod"},
	{"////", "Saw swarm"},
	{"//uu", "Saw comb"},
	{"TOY*", "Circuit-bent toy"},
	{"ZLPF", "Low-pass filtered waveform"},
	{"ZPKF", "Peak filtered waveform"},
	{"ZBPF", "Band-pass filtered waveform"},
	{"ZHPF", "High-pass filtered waveform"},
	{"VOSM", "VOSIM formant"},
	{"VOWL", "Speech synthesis"},
	{"VFOF", "FOF speech synthesis"},
	{"HARM", "12 sine harmonics"},
	{"FM  ", "2-operator phase-modulation"},
	{"FBFM", "2-operator phase-modulation with feedback"},
	{"WTFM", "2-operator phase-modulation with chaotic feedback"},
	{"PLUK", "Plucked string"},
	{"BOWD", "Bowed string"},
	{"BLOW", "Blown reed"},
	{"FLUT", "Flute"},
	{"BELL", "Bell"},
	{"DRUM", "Drum"},
	{"KICK", "Kick drum circuit simulation"},
	{"CYMB", "Cymbal"},
	{"SNAR", "Snare"},
	{"WTBL", "Wavetable"},
	{"WMAP", "2D wavetable"},
	{"WLIN", "1D wavetable"},
	{"WTx4", "4-voice paraphonic 1D wavetable"},
	{"NOIS", "Filtered noise"},
	{"TWNQ", "Twin peaks noise"},
	{"CLKN", "Clocked noise"},
	{"CLOU", "Granular cloud"},
	{"PRTC", "Particle noise"},
	{"QPSK", "Digital modulation"},
	{"  49", "Paques morse code"}, // "Paques"
};