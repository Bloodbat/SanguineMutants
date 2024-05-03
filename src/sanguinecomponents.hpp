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

struct SanguineAlphaDisplay : TransparentWidget {
	Module* module;
	std::shared_ptr<Font> font = nullptr;
	std::vector<std::string>* itemList = nullptr;
	int* selectedItem = nullptr;
	NVGcolor textColor = nvgRGB(200, 0, 0);
	SanguineAlphaDisplay();
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineLedNumberDisplay : TransparentWidget {
	Module* module;
	std::shared_ptr<Font> font = nullptr;
	int* value = nullptr;
	NVGcolor textColor = nvgRGB(200, 0, 0);
	SanguineLedNumberDisplay();
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

// Decorations

struct SanguineShapedLight : widget::SvgWidget {
	Module* module;
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

// Drawing utils
void drawCircularHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float radiusFactor);
void drawRectHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float positionX);