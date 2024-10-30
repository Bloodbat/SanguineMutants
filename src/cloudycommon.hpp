#pragma once

struct CloudyParasiteModeDisplay {
	std::string labelFreeze;
	std::string labelPosition;
	std::string labelDensity;
	std::string labelSize;
	std::string labelTexture;
	std::string labelPitch;
	std::string labelTrigger;
	// Labels for parasite.
	std::string labelBlend;
	std::string labelSpread;
	std::string labelFeedback;
	std::string labelReverb;
};

struct NebulaeModeInfo {
	std::string display;
	std::string menuLabel;
};

static const std::string nebulaeCVSuffix = " CV";

static const std::string nebulaeLedButtonPrefix = "LED display value: ";

static const std::vector<std::string> nebulaeButtonTexts{
	"Input",
	"Output",
	"Blends",
	"Momentary"
};

enum NebulaeLedModes {
	LEDS_INPUT,
	LEDS_OUTPUT,
	LEDS_PARAMETERS,
	LEDS_MOMENTARY,
	LEDS_QUALITY_MOMENTARY,
	LEDS_MODE_MOMENTARY
};