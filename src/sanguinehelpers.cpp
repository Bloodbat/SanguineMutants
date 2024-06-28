#include "sanguinehelpers.hpp"

rack::math::Vec millimetersToPixelsVec(const float x, const float y) {
	return rack::window::mm2px(rack::math::Vec(x, y));
}

rack::math::Vec centerWidgetInMillimeters(rack::widget::Widget* theWidget, const float x, const float y) {
	rack::math::Vec widgetVec;
	widgetVec = millimetersToPixelsVec(x, y);
	widgetVec = widgetVec.minus(theWidget->box.size.div(2));
	return widgetVec;
}