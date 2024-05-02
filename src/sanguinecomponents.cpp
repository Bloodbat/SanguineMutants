#include "sanguinecomponents.hpp"
#include <color.hpp>

using namespace rack;

extern Plugin* pluginInstance;

// Ports

BananutRed::BananutRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutRed.svg")));
}

BananutGreen::BananutGreen() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutGreen.svg")));
}

BananutPurple::BananutPurple() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutPurple.svg")));
}

BananutBlack::BananutBlack() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutBlack.svg")));
}

// Knobs

BefacoTinyKnobRed::BefacoTinyKnobRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyPointWhite.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyKnobRed_bg.svg")));
}

// Displays

SanguineLedNumberDisplay::SanguineLedNumberDisplay() {
	font = APP->window->loadFont(asset::plugin(pluginInstance, "res/components/Segment7Standard.otf"));
}

void SanguineLedNumberDisplay::draw(const DrawArgs& args) {
	// Background
	NVGcolor backgroundColor = nvgRGB(0x38, 0x38, 0x38);
	NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
	nvgBeginPath(args.vg);
	nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 5.0);
	nvgFillColor(args.vg, backgroundColor);
	nvgFill(args.vg);
	nvgStrokeWidth(args.vg, 1.0);
	nvgStrokeColor(args.vg, borderColor);
	nvgStroke(args.vg);

	Widget::draw(args);
}

void SanguineLedNumberDisplay::drawHalo(const DrawArgs& args) {
	// Adapted from MindMeld & LightWidget.
	if (args.fb)
		return;

	const float halo = settings::haloBrightness;
	if (halo == 0.f)
		return;

	nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

	nvgBeginPath(args.vg);
	nvgRect(args.vg, -9, -9, box.size.x + 9, box.size.y + 9);

	NVGcolor icol = color::mult(nvgRGBA(200, 0, 0, 100), halo);

	NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
	NVGpaint paint = nvgBoxGradient(args.vg, 4.5f, 4.5f, box.size.x - 4.5, box.size.y - 4.5, 5, 8, icol, ocol);
	nvgFillPaint(args.vg, paint);
	nvgFill(args.vg);

	nvgFillPaint(args.vg, paint);
	nvgFill(args.vg);
	nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
}

void SanguineLedNumberDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (module && !module->isBypassed()) {
			if (font) {
				// Text					
				nvgFontSize(args.vg, 33.95);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, 2.5);

				Vec textPos = Vec(2, 36);
				NVGcolor textColor = nvgRGB(200, 0, 0);
				nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
				// Background of all segments
				nvgText(args.vg, textPos.x, textPos.y, "88", NULL);
				nvgFillColor(args.vg, textColor);

				std::string displayValue = "";

				if (value)
					displayValue = std::to_string(*value);

				if (*value < 10)
					displayValue.insert(0, 1, '0');

				nvgText(args.vg, textPos.x, textPos.y, displayValue.c_str(), NULL);
				drawHalo(args);
			}
		}
	}
	Widget::drawLayer(args, layer);
}

// Switches

SanguineLightUpSwitch::SanguineLightUpSwitch() {
	momentary = true;
	shadow->opacity = 0.0;
	sw->wrap();
	box.size = sw->box.size;
	haloColorOn = nvgRGB(0xAA, 0xAA, 0xAA);
	haloColorOff = nvgRGB(0x10, 0x10, 0x10);
}

void SanguineLightUpSwitch::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (module && !module->isBypassed()) {
			std::shared_ptr<window::Svg> svg = frames[static_cast<int>(getParamQuantity()->getValue())];
			if (!svg)
				return;
			nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
			rack::window::svgDraw(args.vg, svg->handle);
			drawHalo(args);
		}
	}
	Widget::drawLayer(args, layer);
}

void SanguineLightUpSwitch::drawHalo(const DrawArgs& args) {
	// Adapted from LightWidget
	// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser
	if (args.fb)
		return;

	const float halo = settings::haloBrightness;
	if (halo == 0.f)
		return;

	nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

	math::Vec c = box.size.div(2);
	float radius = std::min(box.size.x, box.size.y) / 8.0;
	float oradius = radius + std::min(radius * 4.f, 15.f);

	nvgBeginPath(args.vg);
	nvgRect(args.vg, c.x - oradius, c.y - oradius, 2 * oradius, 2 * oradius);

	NVGcolor icol;

	if (getParamQuantity()->getValue() == 0) {
		icol = color::mult(haloColorOff, halo);
	}
	else {
		icol = color::mult(haloColorOn, halo);
	}

	NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
	NVGpaint paint = nvgRadialGradient(args.vg, c.x, c.y, radius, oradius, icol, ocol);
	nvgFillPaint(args.vg, paint);
	nvgFill(args.vg);
}

// Decorations

void LightUpSvgWidget::draw(const DrawArgs& args) {
	// Do not call SvgWidget::draw: it draws on the wrong layer.
	Widget::draw(args);
}

void LightUpSvgWidget::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		//From SvgWidget::draw()
		if (!svg)
			return;
		if (module && !module->isBypassed()) {
			nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
			rack::window::svgDraw(args.vg, svg->handle);
		}
	}
	Widget::drawLayer(args, layer);
}