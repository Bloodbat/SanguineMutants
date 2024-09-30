#include "sanguinecomponents.hpp"
#include <color.hpp>

using namespace rack;

extern Plugin* pluginInstance;

// Ports

BananutBlack::BananutBlack() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutBlack.svg")));
}

BananutBlue::BananutBlue() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutBlue.svg")));
}

BananutGreen::BananutGreen() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutGreen.svg")));
}

BananutGreenPoly::BananutGreenPoly() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutGreenPoly.svg")));
}

BananutPurple::BananutPurple() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutPurple.svg")));
}

BananutPurplePoly::BananutPurplePoly() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutPurplePoly.svg")));
}

BananutRed::BananutRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutRed.svg")));
}

BananutRedPoly::BananutRedPoly() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutRedPoly.svg")));
}

BananutYellow::BananutYellow() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutYellow.svg")));
}

// Knobs

BefacoTinyKnobRed::BefacoTinyKnobRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyPointWhite.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyKnobRed_bg.svg")));
}

BefacoTinyKnobBlack::BefacoTinyKnobBlack() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyPointWhite.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyKnobBlack_bg.svg")));
}

Sanguine1PBlue::Sanguine1PBlue() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PBlue.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PBlue_fg.svg")));
}

Sanguine1PGrayCap::Sanguine1PGrayCap() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PWhite.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PGrayCap_fg.svg")));
}

Sanguine1PGreen::Sanguine1PGreen() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PGreen.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PGreen_fg.svg")));
}

Sanguine1PPurple::Sanguine1PPurple() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PPurple.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PPurple_fg.svg")));
}

Sanguine1PRed::Sanguine1PRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PRed.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PRed_fg.svg")));
}

Sanguine1PYellow::Sanguine1PYellow() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PYellow.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PYellow_fg.svg")));
}

Sanguine1PSBlue::Sanguine1PSBlue() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSBlue.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSBlue_fg.svg")));
}

Sanguine1PSGreen::Sanguine1PSGreen() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSGreen.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSGreen_fg.svg")));
}

Sanguine1PSOrange::Sanguine1PSOrange() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSOrange.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSOrange_fg.svg")));
}

Sanguine1PSPurple::Sanguine1PSPurple() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSPurple.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSPurple_fg.svg")));
}

Sanguine1PSRed::Sanguine1PSRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSRed.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSRed_fg.svg")));
}

Sanguine1PSYellow::Sanguine1PSYellow() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSYellow.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSYellow_fg.svg")));
}

Sanguine1SGray::Sanguine1SGray() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1SGray.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1S_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1SGray_fg.svg")));
}

Sanguine2PSBlue::Sanguine2PSBlue() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PSBlue.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PSBlue_fg.svg")));
}

Sanguine2PSRed::Sanguine2PSRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PSRed.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PSRed_fg.svg")));
}

Sanguine3PSGreen::Sanguine3PSGreen() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PSGreen.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PSGreen_fg.svg")));
}

Sanguine3PSRed::Sanguine3PSRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PSRed.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PSRed_fg.svg")));
}

// Displays

SanguineBaseSegmentDisplay::SanguineBaseSegmentDisplay(uint32_t newCharacterCount, Module* theModule) {
	characterCount = newCharacterCount;
	module = theModule;
	fontSize = 10;
}

void SanguineBaseSegmentDisplay::draw(const DrawArgs& args) {
	// Background		
	nvgBeginPath(args.vg);
	nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 5.0);
	nvgFillColor(args.vg, backgroundColor);
	nvgFill(args.vg);
	nvgStrokeWidth(args.vg, 1.0);
	nvgStrokeColor(args.vg, borderColor);
	nvgStroke(args.vg);

	Widget::draw(args);
}

void SanguineBaseSegmentDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (font) {
			// Text				
			nvgFontSize(args.vg, fontSize);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, kerning);

			Vec textPos = textMargin;
			nvgFillColor(args.vg, nvgTransRGBA(textColor, backgroundCharAlpha));
			// Background of all characters
			std::string backgroundText = "";
			for (uint32_t i = 0; i < characterCount; i++)
				backgroundText += backgroundCharacter;
			nvgText(args.vg, textPos.x, textPos.y, backgroundText.c_str(), NULL);
			nvgFillColor(args.vg, textColor);

			std::string displayValue = "";

			// TODO!!! Ensure we draw only characterCount chars.

			if (module && !module->isBypassed()) {
				switch (displayType)
				{
				case DISPLAY_NUMERIC:
					if (values.numberValue) {
						displayValue = std::to_string(*values.numberValue);
						if (*values.numberValue < 10 && leadingZero)
							displayValue.insert(0, 1, '0');
					}
					break;
				case DISPLAY_STRING:
					if (values.displayText) {
						displayValue = *values.displayText;
					}
					break;
				}

				nvgText(args.vg, textPos.x, textPos.y, displayValue.c_str(), NULL);
				if (drawHalo) {
					drawRectHalo(args, box.size, textColor, haloOpacity, 0.f);
				}
			}
			else if (!module) {
				switch (displayType)
				{
				case DISPLAY_NUMERIC:
					displayValue = std::to_string(fallbackNumber);
					if (fallbackNumber < 10 && leadingZero)
						displayValue.insert(0, 1, '0');
					break;
				case DISPLAY_STRING:
					displayValue = fallbackString;
					break;
				}

				nvgText(args.vg, textPos.x, textPos.y, displayValue.c_str(), NULL);
			}
		}
	}
	Widget::drawLayer(args, layer);
}

SanguineAlphaDisplay::SanguineAlphaDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineBaseSegmentDisplay(newCharacterCount, theModule) {
	font = APP->window->loadFont(asset::plugin(pluginInstance, "res/components/Segment14.ttf"));
	box.size = mm2px(Vec(newCharacterCount * 12.6, 21.2));
	fontSize = 40;

	if (createCentered) {
		box.pos = centerWidgetInMillimeters(this, X, Y);
	}
	else
	{
		box.pos = mm2px(Vec(X, Y));
	}

	backgroundCharacter = '~';
	textMargin = Vec(9.f, 52.f);
	kerning = 2.5f;
}

SanguineLedNumberDisplay::SanguineLedNumberDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineBaseSegmentDisplay(newCharacterCount, theModule) {
	font = APP->window->loadFont(asset::plugin(pluginInstance, "res/components/Segment7Standard.otf"));
	box.size = mm2px(Vec(newCharacterCount * 7.75, 15));
	fontSize = 33.95;

	if (createCentered) {
		box.pos = centerWidgetInMillimeters(this, X, Y);
	}
	else
	{
		box.pos = mm2px(Vec(X, Y));
	}

	backgroundCharacter = '8';
	textMargin = Vec(2.f, 36.f);
	kerning = 2.5f;

	displayType = DISPLAY_NUMERIC;
}

SanguineMatrixDisplay::SanguineMatrixDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineBaseSegmentDisplay(newCharacterCount, theModule)
{
	font = APP->window->loadFont(asset::plugin(pluginInstance, "res/components/sanguinematrix.ttf"));
	box.size = mm2px(Vec(newCharacterCount * 5.70275, 10.16));
	fontSize = 16.45;

	if (createCentered) {
		box.pos = centerWidgetInMillimeters(this, X, Y);
	}
	else
	{
		box.pos = mm2px(Vec(X, Y));
	}

	backgroundCharacter = "\u2588";
	textMargin = Vec(5, 24);
	kerning = 2.f;
}

SanguineTinyNumericDisplay::SanguineTinyNumericDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineLedNumberDisplay(newCharacterCount, theModule, X, Y, createCentered) {
	displayType = DISPLAY_NUMERIC;
	box.size = mm2px(Vec(newCharacterCount * 6.45, 8.f));
	fontSize = 21.4;

	if (createCentered) {
		box.pos = centerWidgetInMillimeters(this, X, Y);
	}
	else
	{
		box.pos = mm2px(Vec(X, Y));
	}

	backgroundCharacter = '8';
	textMargin = Vec(5, 20);
	kerning = 2.5f;
};

Sanguine96x32OLEDDisplay::Sanguine96x32OLEDDisplay(Module* theModule, const float X, const float Y, bool createCentered) {
	font = APP->window->loadFont(asset::plugin(pluginInstance, "res/components/sanguinematrix.ttf"));
	box.size = mm2px(Vec(16.298, 5.418));

	module = theModule;

	if (createCentered) {
		box.pos = centerWidgetInMillimeters(this, X, Y);
	}
	else
	{
		box.pos = mm2px(Vec(X, Y));
	}
}

void Sanguine96x32OLEDDisplay::draw(const DrawArgs& args) {
	// Background
	NVGcolor backgroundColor = nvgRGB(10, 10, 10);
	NVGcolor borderColor = nvgRGB(100, 100, 100);
	nvgBeginPath(args.vg);
	nvgRect(args.vg, 0.0, 0.0, box.size.x, box.size.y);
	nvgFillColor(args.vg, backgroundColor);
	nvgFill(args.vg);
	nvgStrokeWidth(args.vg, 1.0);
	nvgStrokeColor(args.vg, borderColor);
	nvgStroke(args.vg);

	Widget::draw(args);
}

void Sanguine96x32OLEDDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (font) {
			// Text					
			nvgFontSize(args.vg, 6);
			nvgFontFaceId(args.vg, font->handle);

			nvgFillColor(args.vg, textColor);

			Vec textPos = Vec(3, 7.5);

			if (module && !module->isBypassed()) {
				if (oledText && !(oledText->empty())) {
					std::string textCopy;
					textCopy.assign(oledText->data());
					bool multiLine = oledText->size() > 8;
					if (multiLine) {
						std::string displayText = "";
						for (uint32_t i = 0; i < 8; i++)
							displayText += textCopy[i];
						textCopy.erase(0, 8);
						nvgText(args.vg, textPos.x, textPos.y, displayText.c_str(), NULL);
						textPos = Vec(3, 14.5);
						displayText = "";
						for (uint32_t i = 0; (i < 8 || i < textCopy.length()); i++)
							displayText += textCopy[i];
						nvgText(args.vg, textPos.x, textPos.y, displayText.c_str(), NULL);
					}
					else {
						nvgText(args.vg, textPos.x, textPos.y, oledText->c_str(), NULL);
					}
					//drawRectHalo(args, box.size, textColor, 55, 0.f);					
				}
			}
			else {
				if (!module) {
					nvgText(args.vg, textPos.x, textPos.y, fallbackString.c_str(), NULL);
				}
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
}

void SanguineLightUpSwitch::addHalo(NVGcolor haloColor) {
	halos.push_back(haloColor);
}

void SanguineLightUpSwitch::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (module && !module->isBypassed()) {
			uint32_t frameNum = getParamQuantity()->getValue();
			std::shared_ptr<window::Svg> svg = frames[static_cast<int>(frameNum)];
			if (!svg)
				return;
			nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
			rack::window::svgDraw(args.vg, svg->handle);
			if (frameNum < halos.size()) {
				drawCircularHalo(args, box.size, halos[frameNum], 175, 8.f);
			}
		}
	}
	Widget::drawLayer(args, layer);
}

SanguineLightUpRGBSwitch::SanguineLightUpRGBSwitch() {
	momentary = true;
	shadow->opacity = 0.0;
	sw->wrap();
	box.size = sw->box.size;
	transformWidget = new TransformWidget;
	fb->addChildAbove(transformWidget, sw);
	glyph = new SvgWidget();
	transformWidget->addChild(glyph);
}

void SanguineLightUpRGBSwitch::addColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
	colors.push_back(rgbColorToInt(red, green, blue, alpha));
}

void SanguineLightUpRGBSwitch::addColor(unsigned int color) {
	colors.push_back(color);
}

void SanguineLightUpRGBSwitch::addHalo(NVGcolor haloColor) {
	halos.push_back(haloColor);
}

void SanguineLightUpRGBSwitch::drawLayer(const DrawArgs& args, int layer) {
	// Programmers responsibility: set both a background and glyph or Rack will crash here. You've been warned.
	if (layer == 1) {
		if (module && !module->isBypassed() && sw->svg) {
			svgDraw(args.vg, sw->svg->handle);
			uint32_t frameNum = getParamQuantity()->getValue();
			nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
			/*
			Programmer responsibility : make sure there are enough colors here for every switch state
			or Rack will go bye-bye.
			*/
			fillSvgSolidColor(glyph->svg->handle, colors[frameNum]);
			nvgSave(args.vg);
			nvgTransform(args.vg, transformWidget->transform[0], transformWidget->transform[1], transformWidget->transform[2],
				transformWidget->transform[3], transformWidget->transform[4], transformWidget->transform[5]);
			svgDraw(args.vg, glyph->svg->handle);
			nvgRestore(args.vg);
			/*
			Programmer responsibility: make sure there are enough halos here for  every switch state
			or Rack will go to synth heaven.
			*/
			drawCircularHalo(args, box.size, halos[frameNum], 175, 8.f);
		}
		// For module browser
		else if (!module && sw->svg) {
			svgDraw(args.vg, sw->svg->handle);
			fillSvgSolidColor(glyph->svg->handle, colors[0]);
			nvgSave(args.vg);
			nvgTransform(args.vg, transformWidget->transform[0], transformWidget->transform[1], transformWidget->transform[2],
				transformWidget->transform[3], transformWidget->transform[4], transformWidget->transform[5]);
			svgDraw(args.vg, glyph->svg->handle);
			nvgRestore(args.vg);
		}
	}
	Widget::drawLayer(args, layer);
}

void SanguineLightUpRGBSwitch::setBackground(const std::string fileName) {
	sw->setSvg(Svg::load(asset::plugin(pluginInstance, fileName)));
	sw->wrap();
	box.size = sw->box.size;
	fb->box.size = sw->box.size;
	// Move shadow downward by 10%
	shadow->box.size = sw->box.size;
	shadow->box.pos = math::Vec(0, sw->box.size.y * 0.10);
}

void SanguineLightUpRGBSwitch::setGlyph(const std::string fileName, const float offsetX, const float offsetY) {
	glyph->setSvg(Svg::load(asset::plugin(pluginInstance, fileName)));
	glyph->wrap();
	transformWidget->box.size = glyph->box.size;
	transformWidget->identity();
	transformWidget->translate(millimetersToPixelsVec(offsetX, offsetY));
}

Befaco2StepSwitch::Befaco2StepSwitch() {
	addFrame(Svg::load(asset::system("res/ComponentLibrary/BefacoSwitch_0.svg")));
	addFrame(Svg::load(asset::system("res/ComponentLibrary/BefacoSwitch_2.svg")));
}

// Lights

SanguineMultiColoredShapedLight::SanguineMultiColoredShapedLight() {
	module = nullptr;
}

/* Returns the parameterized value of the line p2--p3 where it intersects with p0--p1 */
float SanguineMultiColoredShapedLight::getLineCrossing(math::Vec p0, math::Vec p1, math::Vec p2, math::Vec p3) {
	math::Vec b = p2.minus(p0);
	math::Vec d = p1.minus(p0);
	math::Vec e = p3.minus(p2);
	float m = d.x * e.y - d.y * e.x;
	// Check if lines are parallel, or if either pair of points are equal
	if (std::abs(m) < 1e-6)
		return NAN;
	return -(d.x * b.y - d.y * b.x) / m;
}

NVGcolor SanguineMultiColoredShapedLight::getNVGColor(uint32_t color) {
	return nvgRGBA((color >> 0) & 0xff, (color >> 8) & 0xff, (color >> 16) & 0xff, (color >> 24) & 0xff);
}

NVGpaint SanguineMultiColoredShapedLight::getPaint(NVGcontext* vg, NSVGpaint* p, NVGcolor innerColor, NVGcolor outerColor) {
	assert(p->type == NSVG_PAINT_LINEAR_GRADIENT || p->type == NSVG_PAINT_RADIAL_GRADIENT);
	NSVGgradient* g = p->gradient;
	assert(g->nstops >= 1);

	float inverse[6];
	nvgTransformInverse(inverse, g->xform);
	math::Vec s, e;
	// Is it always the case that the gradient should be transformed from (0, 0) to (0, 1)?
	nvgTransformPoint(&s.x, &s.y, inverse, 0, 0);
	nvgTransformPoint(&e.x, &e.y, inverse, 0, 1);

	NVGpaint paint;
	if (p->type == NSVG_PAINT_LINEAR_GRADIENT)
		paint = nvgLinearGradient(vg, s.x, s.y, e.x, e.y, innerColor, outerColor);
	else
		paint = nvgRadialGradient(vg, s.x, s.y, 0.0, 160, innerColor, outerColor);
	return paint;
};

void SanguineMultiColoredShapedLight::drawLayer(const DrawArgs& args, int layer) {
	if (innerColor && svg) {
		if (layer == 1) {
			if (module && !module->isBypassed()) {
				int shapeIndex = 0;
				NSVGimage* mySvg = svg->handle;
				NSVGimage* myGradient = svgGradient->handle;

				// Iterate shape linked list
				for (NSVGshape* shape = mySvg->shapes; shape; shape = shape->next, shapeIndex++) {

					// Visibility
					if (!(shape->flags & NSVG_FLAGS_VISIBLE))
						continue;

					nvgSave(args.vg);

					// Opacity
					if (shape->opacity < 1.0)
						nvgAlpha(args.vg, shape->opacity);

					// Build path
					nvgBeginPath(args.vg);

					// Iterate path linked list
					for (NSVGpath* path = shape->paths; path; path = path->next) {

						nvgMoveTo(args.vg, path->pts[0], path->pts[1]);
						for (int i = 1; i < path->npts; i += 3) {
							float* p = &path->pts[2 * i];
							nvgBezierTo(args.vg, p[0], p[1], p[2], p[3], p[4], p[5]);
						}

						// Close path
						if (path->closed)
							nvgClosePath(args.vg);

						// Compute whether this is a hole or a solid.
						// Assume that no paths are crossing (usually true for normal SVG graphics).
						// Also assume that the topology is the same if we use straight lines rather than Beziers (not always the case but usually true).
						// Using the even-odd fill rule, if we draw a line from a point on the path to a point outside the boundary (e.g. top left) and count the number of times it crosses another path, the parity of this count determines whether the path is a hole (odd) or solid (even).
						int crossings = 0;
						math::Vec p0 = math::Vec(path->pts[0], path->pts[1]);
						math::Vec p1 = math::Vec(path->bounds[0] - 1.0, path->bounds[1] - 1.0);
						// Iterate all other paths
						for (NSVGpath* path2 = shape->paths; path2; path2 = path2->next) {
							if (path2 == path)
								continue;

							// Iterate all lines on the path
							if (path2->npts < 4)
								continue;
							for (int i = 1; i < path2->npts + 3; i += 3) {
								float* p = &path2->pts[2 * i];
								// The previous point
								math::Vec p2 = math::Vec(p[-2], p[-1]);
								// The current point
								math::Vec p3 = (i < path2->npts) ? math::Vec(p[4], p[5]) : math::Vec(path2->pts[0], path2->pts[1]);
								float crossing = getLineCrossing(p0, p1, p2, p3);
								float crossing2 = getLineCrossing(p2, p3, p0, p1);
								if (0.0 <= crossing && crossing < 1.0 && 0.0 <= crossing2) {
									crossings++;
								}
							}
						}

						if (crossings % 2 == 0)
							nvgPathWinding(args.vg, NVG_SOLID);
						else
							nvgPathWinding(args.vg, NVG_HOLE);
					}

					// Fill shape with external gradient
					if (svgGradient) {
						if (myGradient->shapes->fill.type) {
							switch (myGradient->shapes->fill.type) {
							case NSVG_PAINT_COLOR: {
								nvgFillColor(args.vg, *innerColor);
								break;
							}
							case NSVG_PAINT_LINEAR_GRADIENT:
							case NSVG_PAINT_RADIAL_GRADIENT: {
								if (innerColor && outerColor) {
									nvgFillPaint(args.vg, getPaint(args.vg, &myGradient->shapes->fill, *innerColor, *outerColor));
								}
								else {
									nvgFillColor(args.vg, *innerColor);
								}
								break;
							}
							}
							nvgFill(args.vg);
						}
					}
					else {
						NVGcolor color = nvgRGB(0, 250, 0);
						nvgFillColor(args.vg, color);
						nvgFill(args.vg);
					}

					// Stroke shape
					if (shape->stroke.type) {
						nvgStrokeWidth(args.vg, shape->strokeWidth);
						nvgLineCap(args.vg, (NVGlineCap)shape->strokeLineCap);
						nvgLineJoin(args.vg, (int)shape->strokeLineJoin);

						switch (shape->stroke.type) {
						case NSVG_PAINT_COLOR: {
							NVGcolor color = getNVGColor(shape->stroke.color);
							nvgStrokeColor(args.vg, color);
						} break;
						case NSVG_PAINT_LINEAR_GRADIENT: {
							break;
						}
						}
						nvgStroke(args.vg);
					}

					nvgRestore(args.vg);
				}
			}
		}
	}
}

// Decorations
SanguineShapedLight::SanguineShapedLight(Module* theModule, const std::string shapeFileName, const float X, const float Y, bool createCentered) {
	module = theModule;

	setSvg(Svg::load(asset::plugin(pluginInstance, shapeFileName)));

	if (createCentered) {
		box.pos = centerWidgetInMillimeters(this, X, Y);
	}
	else
	{
		box.pos = mm2px(Vec(X, Y));
	}
}

void SanguineShapedLight::draw(const DrawArgs& args) {
	// Draw lights in module browser.
	if (!module) {
		Widget::draw(args);
	}
	// else do not call Widget::draw: it draws on the wrong layer.
}

void SanguineShapedLight::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		//From SvgWidget::draw()
		if (!sw->svg)
			return;
		if (module && !module->isBypassed()) {
			nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
			rack::window::svgDraw(args.vg, sw->svg->handle);
		}
	}
	Widget::drawLayer(args, layer);
}

SanguineStaticRGBLight::SanguineStaticRGBLight(Module* theModule, const std::string shapeFileName, const float X, const float Y,
	bool createCentered, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
	module = theModule;
	setSvg(Svg::load(asset::plugin(pluginInstance, shapeFileName)));
	lightColor = (alpha << 24) + (blue << 16) + (green << 8) + red;

	if (createCentered) {
		box.pos = centerWidgetInMillimeters(this, X, Y);
	}
	else
	{
		box.pos = mm2px(Vec(X, Y));
	}
}

SanguineStaticRGBLight::SanguineStaticRGBLight(Module* theModule, const std::string shapeFileName, const float X, const float Y,
	bool createCentered, unsigned int newLightColor) {
	module = theModule;
	setSvg(Svg::load(asset::plugin(pluginInstance, shapeFileName)));
	lightColor = newLightColor;

	if (createCentered) {
		box.pos = centerWidgetInMillimeters(this, X, Y);
	}
	else
	{
		box.pos = mm2px(Vec(X, Y));
	}
}

// draw and drawLayer logic partially based on code by BaconPaul and Hemmer.
void SanguineStaticRGBLight::draw(const DrawArgs& args) {
	// Draw lights in module browser.	
	if (!module) {
		if (!sw->svg)
			return;

		NSVGimage* mySvg = sw->svg->handle;

		fillSvgSolidColor(mySvg, lightColor);
		svgDraw(args.vg, sw->svg->handle);
	}
	// else do not call Widget::draw: it draws on the wrong layer.
}

void SanguineStaticRGBLight::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		//From SvgWidget::draw()
		if (!sw->svg)
			return;
		if (module && !module->isBypassed()) {
			NSVGimage* mySvg = sw->svg->handle;

			fillSvgSolidColor(mySvg, lightColor);
			nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

			svgDraw(args.vg, sw->svg->handle);
		}
	}
	Widget::drawLayer(args, layer);
}

SanguineMonoInputLight::SanguineMonoInputLight(Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineStaticRGBLight(theModule, "res/in_light.svg", X, Y, createCentered, kSanguineYellowLight) {
}

SanguineMonoOutputLight::SanguineMonoOutputLight(Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineStaticRGBLight(theModule, "res/out_light.svg", X, Y, createCentered, kSanguineYellowLight) {
}

SanguinePolyInputLight::SanguinePolyInputLight(Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineStaticRGBLight(theModule, "res/in_light.svg", X, Y, createCentered, kSanguineBlueLight) {
}

SanguinePolyOutputLight::SanguinePolyOutputLight(Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineStaticRGBLight(theModule, "res/out_light.svg", X, Y, createCentered, kSanguineBlueLight) {
}

SanguineBloodLogoLight::SanguineBloodLogoLight(Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineShapedLight(theModule, "res/blood_glowy.svg", X, Y, createCentered) {
}

SanguineMonstersLogoLight::SanguineMonstersLogoLight(Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineShapedLight(theModule, "res/monsters_lit.svg", X, Y, createCentered) {
}

SanguineMutantsLogoLight::SanguineMutantsLogoLight(Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineShapedLight(theModule, "res/mutants_glowy.svg", X, Y, createCentered) {
}

// Panels

SanguinePanel::SanguinePanel(const std::string newBackgroundFileName, const std::string newForegroundFileName) {
	setBackground(Svg::load(asset::plugin(pluginInstance, newBackgroundFileName)));
	foreground = new SvgWidget();
	foreground->setSvg(Svg::load(asset::plugin(pluginInstance, newForegroundFileName)));
	fb->addChildBelow(foreground, panelBorder);
}

void SanguinePanel::addLayer(const std::string layerFileName) {
	SvgWidget* layer = new SvgWidget();
	layer->setSvg(Svg::load(asset::plugin(pluginInstance, layerFileName)));
	fb->addChildBelow(layer, panelBorder);
}

// Modules

void SanguineModule::dataFromJson(json_t* rootJ) {
	json_t* uniqueThemeJ = json_object_get(rootJ, "uniqueSanguineTheme");
	if (uniqueThemeJ) {
		bUniqueTheme = json_boolean_value(uniqueThemeJ);
		if (bUniqueTheme) {
			json_t* moduleThemeJ = json_object_get(rootJ, "sanguineModuleTheme");
			if (moduleThemeJ) {
				currentTheme = FaceplateThemes(json_integer_value(moduleThemeJ));
			}
		}
	}
}

json_t* SanguineModule::dataToJson() {
	json_t* rootJ = json_object();
	json_object_set_new(rootJ, "uniqueSanguineTheme", json_boolean(bUniqueTheme));
	json_object_set_new(rootJ, "sanguineModuleTheme", json_integer(int(currentTheme)));

	return rootJ;
}

void SanguineModule::setModuleTheme(int themeNum) {
	currentTheme = FaceplateThemes(themeNum);
	bUniqueTheme = true;
}

// Module widgets

void SanguineModuleWidget::makePanel() {
	BackplateColors themeBackplateColor = PLATE_PURPLE;
	FaceplateThemes faceplateTheme = defaultTheme;

	// Programmer responsibility: if the module is not a SanguineModule, Rack will jump off a cliff.
	// Now you know.
	SanguineModule* sanguineModule = dynamic_cast<SanguineModule*>(this->module);

	if (sanguineModule) {
		if (!sanguineModule->bUniqueTheme) {
			sanguineModule->setModuleTheme(faceplateTheme);
		}
		else {
			faceplateTheme = sanguineModule->currentTheme;
		}
	}

	switch (faceplateTheme)
	{
	case THEME_NONE:
	case THEME_VITRIOL:
		themeBackplateColor = backplateColor;
		break;
	case THEME_PLUMBAGO:
		themeBackplateColor = PLATE_BLACK;
		break;
	}

	std::string backplateFileName = "res/backplate_" + panelSizeStrings[panelSize] + backplateColorStrings[themeBackplateColor] + ".svg";

	std::string faceplateFileName = "res/" + moduleName;

	if (bFaceplateSuffix) {
		faceplateFileName += "_faceplate";
	}

	faceplateFileName += faceplateThemeStrings[faceplateTheme] + ".svg";

	SanguinePanel* panel = new SanguinePanel(backplateFileName, faceplateFileName);
	if (bHasCommon) {
		panel->addLayer("res/" + moduleName + "_common.svg");
	}
	setPanel(panel);
}

void SanguineModuleWidget::appendContextMenu(Menu* menu) {
	SanguineModule* sanguineModule = dynamic_cast<SanguineModule*>(this->module);
	assert(sanguineModule);

	menu->addChild(new MenuSeparator);

	menu->addChild(createIndexSubmenuItem("Default theme", faceplateMenuLabels,
		[=]() { return int(defaultTheme); },
		[=](int i) { setDefaultTheme(i); sanguineModule->setModuleTheme(i); }
	));

	menu->addChild(createIndexSubmenuItem("Module theme", faceplateMenuLabels,
		[=]() { return int(sanguineModule->currentTheme); },
		[=](int i) { sanguineModule->setModuleTheme(i); }
	));
}

void SanguineModuleWidget::step() {
	SanguineModule* sanguineModule = dynamic_cast<SanguineModule*>(this->module);
	if (module) {
		if (sanguineModule->currentTheme != sanguineModule->previousTheme) {
			sanguineModule->previousTheme = sanguineModule->currentTheme;
			makePanel();
		}
	}
	Widget::step();
}

// Drawing utils

void drawCircularHalo(const Widget::DrawArgs& args, const Vec boxSize, const NVGcolor haloColor,
	const unsigned char haloOpacity, const float radiusFactor) {
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
}

void drawRectHalo(const Widget::DrawArgs& args, const Vec boxSize, const NVGcolor haloColor,
	const unsigned char haloOpacity, const float positionX) {
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
}

void fillSvgSolidColor(NSVGimage* svgImage, const unsigned int fillColor) {
	for (NSVGshape* shape = svgImage->shapes; shape; shape = shape->next) {
		shape->fill.color = fillColor;
		shape->fill.type = NSVG_PAINT_COLOR;
	}
}

FaceplateThemes defaultTheme = THEME_VITRIOL;