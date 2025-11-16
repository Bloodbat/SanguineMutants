#pragma once

using namespace sanguineCommonCode;

namespace aestusCommon {
	static const std::vector<std::string> modeMenuLabels = {
		"AD Envelope",
		"Cyclic",
		"AR Envelope"
	};

	static const std::vector<std::string> rangeMenuLabels = {
		"High",
		"Medium",
		"Low"
	};

	static const std::vector<std::string> sheepMenuLabels = {
		"Additive harmonics",
		"PWMish",
		"Nodi WMAP"
	};

	static const std::vector<std::string> modelModeHeaders = {
		"Mode",
		"Wave table",
		"Harmonics"
	};

	static const std::vector<float> calibrationOffsets = {
		0.f,
		-1.f
	};
}