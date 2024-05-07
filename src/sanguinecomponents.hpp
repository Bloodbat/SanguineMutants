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

// Displays

struct SanguineBaseSegmentDisplay : TransparentWidget {
	void draw(const DrawArgs& args) override;
};

struct SanguineAlphaDisplay : SanguineBaseSegmentDisplay {
	Module* module;
	std::shared_ptr<Font> font = nullptr;
	std::string* displayText = nullptr;
	NVGcolor textColor = nvgRGB(200, 0, 0);
	SanguineAlphaDisplay();	
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineLedNumberDisplay : SanguineBaseSegmentDisplay {
	Module* module;
	std::shared_ptr<Font> font = nullptr;
	int* value = nullptr;
	NVGcolor textColor = nvgRGB(200, 0, 0);
	SanguineLedNumberDisplay();	
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineMatrixDisplay :TransparentWidget {
	Module* module;
	std::shared_ptr<Font> font = nullptr;
	std::string* displayText = nullptr;
	NVGcolor textColor = nvgRGB(200, 0, 0);
	SanguineMatrixDisplay();
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct Sanguine96x32OLEDDisplay :TransparentWidget {
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

// Lights
template <typename TBase = GrayModuleLightWidget>
struct TOrangeLight : TBase {
	TOrangeLight() {
		this->addBaseColor(SCHEME_ORANGE);
	}
};
using OrangeLight = TOrangeLight<>;

// Decorations

struct SanguineShapedLight : widget::SvgWidget {
	Module* module;
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

// Drawing utils
void drawCircularHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float radiusFactor);
void drawRectHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float positionX);