#include "rack.hpp"

using namespace rack;

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

struct LightUpSvgWidget : widget::SvgWidget {
	Module* module;
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};