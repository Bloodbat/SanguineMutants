#pragma once

#include "rack.hpp"
#include <color.hpp>

using namespace rack;

// Ports

struct BananutBlack : app::SvgPort {
	BananutBlack();
};

struct BananutGreen : app::SvgPort {
	BananutGreen();
};

struct BananutPurple : app::SvgPort {
	BananutPurple();
};

struct BananutRed : app::SvgPort {
	BananutRed();
};

// Knobs

struct BefacoTinyKnobRed : BefacoTinyKnob {
	BefacoTinyKnobRed();
};

struct BefacoTinyKnobBlack : BefacoTinyKnob {
	BefacoTinyKnobBlack();
};

struct Sanguine1PSBlue : Rogan {
	Sanguine1PSBlue();
};

struct Sanguine1PSGreen : Rogan {
	Sanguine1PSGreen();
};

struct Sanguine1PSOrange : Rogan {
	Sanguine1PSOrange();
};

struct Sanguine1PSPurple : Rogan {
	Sanguine1PSPurple();
};

struct Sanguine1PSRed : Rogan {
	Sanguine1PSRed();
};

struct Sanguine1PSYellow : Rogan {
	Sanguine1PSYellow();
};

struct Sanguine2PSBlue : Rogan {
	Sanguine2PSBlue();
};

struct Sanguine2PSRed : Rogan {
	Sanguine2PSRed();
};

struct Sanguine3PSGreen : Rogan {
	Sanguine3PSGreen();
};

struct Sanguine3PSRed : Rogan {
	Sanguine3PSRed();
};

// Displays

struct SanguineBaseSegmentDisplay : TransparentWidget {
	Module* module;
	std::shared_ptr<Font> font = nullptr;
	NVGcolor backgroundColor = nvgRGB(0x38, 0x38, 0x38);
	NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
	NVGcolor textColor = nvgRGB(200, 0, 0);
	uint32_t characterCount;
	union Values {
		std::string* displayText = nullptr;
		int* numberValue;
	} values;

	float fontSize;
	unsigned char haloOpacity = 55;
	SanguineBaseSegmentDisplay(uint32_t newCharacterCount);
	void draw(const DrawArgs& args) override;
};

struct SanguineAlphaDisplay : SanguineBaseSegmentDisplay {
	SanguineAlphaDisplay(uint32_t newCharacterCount);
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineLedNumberDisplay : SanguineBaseSegmentDisplay {
	SanguineLedNumberDisplay(uint32_t newCharacterCount);
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineMatrixDisplay : SanguineBaseSegmentDisplay {
	unsigned char haloOpacity = 55;
	SanguineMatrixDisplay(uint32_t newCharacterCount);
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineTinyNumericDisplay : SanguineLedNumberDisplay {
	SanguineTinyNumericDisplay(uint32_t newCharacterCount);
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct Sanguine96x32OLEDDisplay : TransparentWidget {
	Module* module;
	std::shared_ptr<Font> font = nullptr;
	std::string* oledText = nullptr;
	NVGcolor textColor = nvgRGB(254, 254, 254);
	Sanguine96x32OLEDDisplay();
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

// Switches

struct SanguineLightUpSwitch : app::SvgSwitch {
	NVGcolor haloColorOn = nvgRGB(0, 100, 0);
	NVGcolor haloColorOff = nvgRGB(100, 0, 0);
	SanguineLightUpSwitch();
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineBezel8mm : app::SvgSwitch {
	SanguineBezel8mm();
};

// Lights
template <typename TBase = GrayModuleLightWidget>
struct TOrangeLight : TBase {
	TOrangeLight() {
		this->addBaseColor(SCHEME_ORANGE);
	}
};
using OrangeLight = TOrangeLight<>;

template <typename TBase = GrayModuleLightWidget>
struct TPurpleLight : TBase {
	TPurpleLight() {
		this->addBaseColor(SCHEME_PURPLE);
	}
};
using PurpleLight = TPurpleLight<>;

template <typename TBase>
struct SanguineBezelLight8mm : TBase {
	SanguineBezelLight8mm() {
		this->borderColor = color::BLACK_TRANSPARENT;
		this->bgColor = color::BLACK_TRANSPARENT;
		this->box.size = mm2px(math::Vec(6.526, 6.526));
	}
};

// Decorations

struct SanguineShapedLight : widget::SvgWidget {
	Module* module;
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

// Drawing utils
void drawCircularHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float radiusFactor);
void drawRectHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float positionX);