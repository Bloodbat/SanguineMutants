#include "sanguinecomponents.hpp"
#include <color.hpp>

using namespace rack;

extern Plugin* pluginInstance;

// Ports

BananutBlack::BananutBlack() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutBlack.svg")));
}

BananutGreen::BananutGreen() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutGreen.svg")));
}

BananutPurple::BananutPurple() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutPurple.svg")));
}

BananutRed::BananutRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutRed.svg")));
}

// Knobs

BefacoTinyKnobRed::BefacoTinyKnobRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyPointWhite.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyKnobRed_bg.svg")));
}

// Displays

SanguineAlphaDisplay::SanguineAlphaDisplay() {
	font = APP->window->loadFont(asset::plugin(pluginInstance, "res/hdad-segment14-1.002/Segment14.ttf"));
}

void SanguineAlphaDisplay::draw(const DrawArgs& args) {
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

void SanguineAlphaDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (module && !module->isBypassed()) {
			if (font) {
				// Text					
				nvgFontSize(args.vg, 38);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, 2.5);

				Vec textPos = Vec(9, 48);
				nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
				// Background of all segments
				nvgText(args.vg, textPos.x, textPos.y, "~~~~~~~~", NULL);
				nvgFillColor(args.vg, textColor);
				if (selectedItem && itemList)
				{
					std::string itemToPrint = itemList->at(*selectedItem);
					nvgText(args.vg, textPos.x, textPos.y, itemToPrint.c_str(), NULL);
				}
				drawRectHalo(args, box.size, textColor, 55, 0.f);
			}
		}
	}
	Widget::drawLayer(args, layer);
}

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

void SanguineLedNumberDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (module && !module->isBypassed()) {
			if (font) {
				// Text					
				nvgFontSize(args.vg, 33.95);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, 2.5);

				Vec textPos = Vec(2, 36);
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
				drawRectHalo(args, box.size, textColor, 55, 0.f);
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
			if (getParamQuantity()->getValue() == 0) {
				drawCircularHalo(args, box.size, haloColorOff, 175, 8.f);
			}
			else {
				drawCircularHalo(args, box.size, haloColorOn, 175, 8.f);
			}
		}
	}
	Widget::drawLayer(args, layer);
}

// Decorations

void SanguineShapedLight::draw(const DrawArgs& args) {
	// Do not call SvgWidget::draw: it draws on the wrong layer.
	Widget::draw(args);
}

void SanguineShapedLight::drawLayer(const DrawArgs& args, int layer) {
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

// Drawing utils

void drawCircularHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float radiusFactor)
{
	// Adapted from LightWidget
	// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser
	if (args.fb)
		return;

	const float halo = settings::haloBrightness;
	if (halo == 0.f)
		return;

	nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

	math::Vec c = boxSize.div(2);
	float radius = std::min(boxSize.x, boxSize.y) / radiusFactor;
	float oradius = radius + std::min(radius * 4.f, 15.f);

	nvgBeginPath(args.vg);
	nvgRect(args.vg, c.x - oradius, c.y - oradius, 2 * oradius, 2 * oradius);

	NVGcolor icol = color::mult(nvgTransRGBA(haloColor, haloOpacity), halo);

	NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
	NVGpaint paint = nvgRadialGradient(args.vg, c.x, c.y, radius, oradius, icol, ocol);
	nvgFillPaint(args.vg, paint);
	nvgFill(args.vg);
};

void drawRectHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float positionX)
{
	// Adapted from MindMeld & LightWidget.
	if (args.fb)
		return;

	const float halo = settings::haloBrightness;
	if (halo == 0.f)
		return;

	nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

	nvgBeginPath(args.vg);
	nvgRect(args.vg, -9 + positionX, -9, boxSize.x + 9, boxSize.y + 9);

	NVGcolor icol = color::mult(nvgTransRGBA(haloColor, haloOpacity), halo);

	NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
	NVGpaint paint = nvgBoxGradient(args.vg, 4.5f + positionX, 4.5f, boxSize.x - 4.5, boxSize.y - 4.5, 5, 8, icol, ocol);
	nvgFillPaint(args.vg, paint);
	nvgFill(args.vg);

	nvgFillPaint(args.vg, paint);
	nvgFill(args.vg);
	nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
};