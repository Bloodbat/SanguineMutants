#pragma once

struct EtesiaModeDisplay {
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
	std::string labelFeeback;
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