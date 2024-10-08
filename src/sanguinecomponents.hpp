#pragma once

#include "rack.hpp"
#include <color.hpp>
#include "sanguinehelpers.hpp"
#include "themes.hpp"

using namespace rack;

// Ports

struct BananutBlack : app::SvgPort {
	BananutBlack();
};

struct BananutBlackPoly : app::SvgPort {
	BananutBlackPoly();
};

struct BananutBlue : app::SvgPort {
	BananutBlue();
};

struct BananutBluePoly : app::SvgPort {
	BananutBluePoly();
};

struct BananutGreen : app::SvgPort {
	BananutGreen();
};

struct BananutGreenPoly : app::SvgPort {
	BananutGreenPoly();
};

struct BananutPurple : app::SvgPort {
	BananutPurple();
};

struct BananutPurplePoly : app::SvgPort {
	BananutPurplePoly();
};

struct BananutRed : app::SvgPort {
	BananutRed();
};

struct BananutRedPoly : app::SvgPort {
	BananutRedPoly();
};

struct BananutYellow : app::SvgPort {
	BananutYellow();
};

struct BananutYellowPoly : app::SvgPort {
	BananutYellowPoly();
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

struct Sanguine1SGray : Rogan {
	Sanguine1SGray();
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
	unsigned char backgroundCharAlpha = 16;
	DisplayType displayType = DISPLAY_STRING;
	std::string backgroundCharacter = " ";
	std::string fallbackString = "";
	std::string fontName = "";
	math::Vec textMargin = { 2.f, 2.f };
	float kerning = 2.f;
	bool leadingZero = true;
	bool drawHalo = true;
	int fallbackNumber = 0;

	SanguineBaseSegmentDisplay(uint32_t newCharacterCount, Module* theModule);
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineAlphaDisplay : SanguineBaseSegmentDisplay {
	SanguineAlphaDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineLedNumberDisplay : SanguineBaseSegmentDisplay {
	SanguineLedNumberDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineMatrixDisplay : SanguineBaseSegmentDisplay {
	unsigned char haloOpacity = 55;
	SanguineMatrixDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineTinyNumericDisplay : SanguineLedNumberDisplay {
	SanguineTinyNumericDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct Sanguine96x32OLEDDisplay : TransparentWidget {
	Module* module;
	std::string* oledText = nullptr;
	std::string fallbackString = "";
	std::string fontName = "";
	NVGcolor textColor = nvgRGB(254, 254, 254);
	Sanguine96x32OLEDDisplay(Module* theModule, const float X, const float Y, bool createCentered = true);
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

struct SanguineLightUpRGBSwitch : app::SvgSwitch {
	std::vector<unsigned int>colors;
	std::vector<NVGcolor> halos;
	SvgWidget* glyph;
	TransformWidget* transformWidget;
	SanguineLightUpRGBSwitch();
	void addColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255);
	void addColor(unsigned int color);
	void addHalo(NVGcolor haloColor);
	void drawLayer(const DrawArgs& args, int layer) override;
	void setBackground(const std::string fileName);
	void setGlyph(const std::string fileName, const float offsetX, const float offsetY);
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
		this->box.size = Vec(28.f - 6.f, 28.f - 6.f);
	}
};

template <typename Base>
struct Rogan6PSLight : Base {
	Rogan6PSLight() {
		this->box.size = mm2px(Vec(23.04f, 23.04f));
		this->borderColor = nvgRGBA(0, 0, 0, 0);
	}
};

// Decorations

struct SanguineShapedLight : SvgLight {
	Module* module;
	SanguineShapedLight(Module* theModule, const std::string shapeFileName, const float X, const float Y, bool createCentered = true);
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineStaticRGBLight : SvgLight {
	unsigned int lightColor;
	SanguineStaticRGBLight(Module* theModule, const std::string shapeFileName, const float X, const float Y,
		bool createCentered, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255);
	SanguineStaticRGBLight(Module* theModule, const std::string shapeFileName, const float X, const float Y,
		bool createCentered, unsigned int newLightColor);
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineMonoInputLight : SanguineStaticRGBLight {
	SanguineMonoInputLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineMonoOutputLight : SanguineStaticRGBLight {
	SanguineMonoOutputLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguinePolyInputLight : SanguineStaticRGBLight {
	SanguinePolyInputLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguinePolyOutputLight : SanguineStaticRGBLight {
	SanguinePolyOutputLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineBloodLogoLight : SanguineShapedLight {
	SanguineBloodLogoLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineMonstersLogoLight : SanguineShapedLight {
	SanguineMonstersLogoLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineMutantsLogoLight : SanguineShapedLight {
	SanguineMutantsLogoLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

// Panels
struct SanguinePanel : SvgPanel {
	widget::SvgWidget* foreground;
	SanguinePanel(const std::string newBackgroundFileName, const std::string newForegroundFileName);
	void addLayer(const std::string layerFileName);
};

// Modules
struct SanguineModule : Module {
	bool bUniqueTheme = false;
	FaceplateThemes previousTheme = THEME_NONE;
	FaceplateThemes currentTheme = THEME_VITRIOL;
	void dataFromJson(json_t* rootJ) override;
	json_t* dataToJson() override;
	void setModuleTheme(int themeNum);
};

// Module widgets
struct SanguineModuleWidget : ModuleWidget {
	enum ScrewIds {
		SCREW_TOP_LEFT = 1,
		SCREW_BOTTOM_LEFT = 2,
		SCREW_TOP_RIGHT = 4,
		SCREW_BOTTOM_RIGHT = 8,
		SCREW_ALL = 15
	};

	bool bFaceplateSuffix = true;
	bool bHasCommon = true;
	std::string moduleName;
	PanelSizes panelSize = SIZE_34;
	BackplateColors backplateColor = PLATE_PURPLE;
	void addScrews(int screwIds);
	void appendContextMenu(Menu* menu) override;
	void makePanel();
	void step() override;
};

// Drawing utils
void drawCircularHalo(const Widget::DrawArgs& args, const Vec boxSize, const NVGcolor haloColor,
	const unsigned char haloOpacity, const float radiusFactor);
void drawRectHalo(const Widget::DrawArgs& args, const Vec boxSize, const NVGcolor haloColor,
	const unsigned char haloOpacity, const float positionX);

inline void fillSvgSolidColor(NSVGimage* svgImage, const unsigned int fillColor);

// utils
inline unsigned int rgbColorToInt(const uint8_t red, const uint8_t green, const uint8_t blue, const uint8_t alpha = 255) {
	return (alpha << 24) + (blue << 16) + (green << 8) + red;
}

// Light colors.

struct RGBLightColor {
	float red;
	float green;
	float blue;
};

// Color constants for decorative lights
static const unsigned int kSanguineBlueLight = rgbColorToInt(0, 167, 255);
static const unsigned int kSanguineYellowLight = rgbColorToInt(239, 250, 100);