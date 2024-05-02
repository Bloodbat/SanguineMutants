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
	std::shared_ptr<Font> font;
	Module* module;
	std::vector<std::string>* itemList = nullptr;
	int* selectedItem = nullptr;
	SanguineAlphaDisplay();
	void draw(const DrawArgs& args) override;	
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineLedNumberDisplay : TransparentWidget {
	std::shared_ptr<Font> font;
	int* value = nullptr;
	Module* module;
	SanguineLedNumberDisplay();	
	void draw(const DrawArgs& args) override;	
	void drawLayer(const DrawArgs& args, int layer) override;
};

// Switches

struct SanguineLightUpSwitch : app::SvgSwitch {	
	NVGcolor haloColorOn;
	NVGcolor haloColorOff;
	SanguineLightUpSwitch();
	void drawHalo(const DrawArgs& args);
	void drawLayer(const DrawArgs& args, int layer) override;
};

// Decorations

struct SanguineShapedLight : widget::SvgWidget {
	Module* module;
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

// Drawing utils
void drawRectHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float positionX);