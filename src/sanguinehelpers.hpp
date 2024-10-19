#pragma once

#include "rack.hpp"
#include <chrono>

rack::math::Vec millimetersToPixelsVec(const float x, const float y);

rack::math::Vec centerWidgetInMillimeters(rack::widget::Widget* theWidget, const float x, const float y);

inline long long getSystemTimeMs() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}