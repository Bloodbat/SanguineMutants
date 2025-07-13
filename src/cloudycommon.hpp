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

	struct ModeInfo {
		std::string display;
		std::string menuLabel;
	};

	static const std::string kLedButtonPrefix = "LED display value: ";

	static const std::vector<std::string> buttonTexts{
		"Input",
		"Output"
	};

	enum LedModes {
		LEDS_INPUT,
		LEDS_OUTPUT
	};

	static const int kMaxFrames = 32;

	static const int kBigBufferLength = 118784;
	static const int kSmallBufferLength = 65536 - 128;
}