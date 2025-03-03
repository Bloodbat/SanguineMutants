#pragma once

#include <rack.hpp>

enum PanelSizes {
	SIZE_4,
	SIZE_6,
	SIZE_7,
	SIZE_8,
	SIZE_9,
	SIZE_10,
	SIZE_11,
	SIZE_12,
	SIZE_13,
	SIZE_14,
	SIZE_20,
	SIZE_21,
	SIZE_22,
	SIZE_23,
	SIZE_24,
	SIZE_25,
	SIZE_27,
	SIZE_28,
	SIZE_34
};

static const std::vector<std::string> panelSizeStrings = {
	"4hp",
	"6hp",
	"7hp",
	"8hp",
	"9hp",
	"10hp",
	"11hp",
	"12hp",
	"13hp",
	"14hp",
	"20hp",
	"21hp",
	"22hp",
	"23hp",
	"24hp",
	"25hp",
	"27hp",
	"28hp",
	"34hp"
};

enum BackplateColors {
	PLATE_PURPLE,
	PLATE_RED,
	PLATE_GREEN,
	PLATE_BLACK
};

static const std::vector<std::string> backplateColorStrings = {
	"_purple",
	"_red",
	"_green",
	"_black"
};

enum FaceplateThemes {
	THEME_NONE = -1,
	THEME_VITRIOL,
	THEME_PLUMBAGO
};

static const std::vector<std::string> faceplateThemeStrings = {
	"",
	"_plumbago"
};

static const std::vector<std::string> faceplateMenuLabels = {
	"Vitriol",
	"Plumbago"
};

void getDefaultTheme();
void setDefaultTheme(int themeNum);

extern FaceplateThemes defaultTheme;