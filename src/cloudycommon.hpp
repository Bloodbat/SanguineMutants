#pragma once

using namespace sanguineCommonCode;

namespace cloudyCommon {
	struct ParasiteModeDisplay {
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

	static const std::string kLedButtonTooltip = "LED display value: %s";

	static const std::vector<std::string> vuButtonLabels{
		"Input",
		"Output"
	};

	static const std::vector<std::string> bufferQualityLabels{
		"8-bit",
		"16-bit"
	};

	static const std::vector<std::string> bufferChannelsLabels{
		"Mono",
		"Stereo"
	};

	static const std::vector<std::string> freezeButtonLabels{
		"Disabled",
		"Enabled"
	};

	enum LedModes {
		LEDS_INPUT,
		LEDS_OUTPUT,
		LED_MODES_LAST
	};

	static const int kMaxFrames = 32;

	static const int kBigBufferLength = 118784;
	static const int kSmallBufferLength = 65536 - 128;

	static const int kHardwareRate = 32000;
}