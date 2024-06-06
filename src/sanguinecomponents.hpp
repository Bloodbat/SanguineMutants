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

struct Sanguine1PBlue : Rogan {
	Sanguine1PBlue();
};

struct Sanguine1PGrayCap : Rogan {
	Sanguine1PGrayCap();
};

struct Sanguine1PGreen : Rogan {
	Sanguine1PGreen();
};

struct Sanguine1PPurple : Rogan {
	Sanguine1PPurple();
};

struct Sanguine1PRed : Rogan {
	Sanguine1PRed();
};

struct Sanguine1PYellow : Rogan {
	Sanguine1PYellow();
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

enum DisplayType {
	DISPLAY_NUMERIC,
	DISPLAY_STRING
};

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
	DisplayType displayType = DISPLAY_STRING;
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
	std::vector<NVGcolor> halos;
	SanguineLightUpSwitch();
	void addHalo(NVGcolor haloColor);
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct Befaco2StepSwitch : app::SvgSwitch {
	Befaco2StepSwitch();
};

// Lights
struct SanguineMultiColoredShapedLight : SvgWidget {
	Module* module;
	std::shared_ptr<window::Svg> svgGradient = nullptr;
	NVGcolor* innerColor = nullptr;
	NVGcolor* outerColor = nullptr;

	SanguineMultiColoredShapedLight();
	static float getLineCrossing(math::Vec p0, math::Vec p1, math::Vec p2, math::Vec p3);
	static NVGcolor getNVGColor(uint32_t color);
	static NVGpaint getPaint(NVGcontext* vg, NSVGpaint* p, NVGcolor innerColor, NVGcolor outerColor);
	void drawLayer(const DrawArgs& args, int layer) override;
};

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

// A light for displaying on top of a CKD6. Must add a color by subclassing or templating.
template <typename TBase>
struct CKD6Light : TBase {
	CKD6Light() {
		this->bgColor = color::BLACK_TRANSPARENT;
		this->box.size = Vec(28 - 6, 28 - 6);
	}
};

template <typename Base>
struct Rogan6PSLight : Base {
	Rogan6PSLight() {
		this->box.size = mm2px(Vec(23.04, 23.04));
		this->borderColor = nvgRGBA(0, 0, 0, 0);
	}
};

// Decorations

struct SanguineShapedLight : SvgLight {
	Module* module;
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

// Drawing utils
void drawCircularHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float radiusFactor);
void drawRectHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float positionX);