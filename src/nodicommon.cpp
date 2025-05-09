#include "nodicommon.hpp"

nodiCommon::NodiDisplay::NodiDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered) :
	SanguineAlphaDisplay(newCharacterCount, theModule, X, Y, createCentered) {

}

void nodiCommon::NodiDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, fontName));
		if (font) {
			// Text
			nvgFontSize(args.vg, fontSize);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, 2.5f);

			Vec textPos = Vec(9, 52);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
			// Background of all segments
			std::string backgroundText = "";
			for (uint32_t character = 0; character < characterCount; ++character) {
				backgroundText += "~";
			}
			nvgText(args.vg, textPos.x, textPos.y, backgroundText.c_str(), NULL);

			if (module && !module->isBypassed()) {
				// Blink effect
				if (!(displayTimeout && (*displayTimeout & 0x1000))) {
					nvgFillColor(args.vg, textColor);
					if (values.displayText && !(values.displayText->empty()))
					{
						// TODO: Make sure we only display max. display chars.
						nvgText(args.vg, textPos.x, textPos.y, values.displayText->c_str(), NULL);
					}
				}
				drawRectHalo(args, box.size, textColor, haloOpacity, 0.f);
			} else if (!module) {
				nvgFillColor(args.vg, textColor);
				nvgText(args.vg, textPos.x, textPos.y, fallbackString.c_str(), NULL);
			}
		}
	}
	Widget::drawLayer(args, layer);
}