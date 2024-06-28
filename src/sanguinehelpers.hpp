#pragma once

#include "rack.hpp"

rack::math::Vec millimetersToPixelsVec(const float x, const float y);

rack::math::Vec centerWidgetInMillimeters(rack::widget::Widget* theWidget, const float x, const float y);