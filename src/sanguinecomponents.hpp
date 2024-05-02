#pragma once

#include "rack.hpp"
#include <color.hpp>

using namespace rack;

// Ports

struct BananutRed : app::SvgPort {
	BananutRed();
};

struct BananutGreen : app::SvgPort {
	BananutGreen();
};

struct BananutPurple : app::SvgPort {
	BananutPurple();
};

struct BananutBlack : app::SvgPort {
	BananutBlack();
};

// Knobs

struct BefacoTinyKnobRed : BefacoTinyKnob {
	BefacoTinyKnobRed();
};

// Displays

struct SanguineLedNumberDisplay : TransparentWidget {
	std::shared_ptr<Font> font;
	int* value = nullptr;
	Module* module;
	SanguineLedNumberDisplay();	
	void draw(const DrawArgs& args) override;
	void drawHalo(const DrawArgs& args);
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