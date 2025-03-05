#pragma once

// Drawing and color utils for Sanguine Components.

#include "rack.hpp"

using namespace rack;

inline unsigned int rgbColorToInt(const uint8_t red, const uint8_t green, const uint8_t blue, const uint8_t alpha = 255) {
    return (alpha << 24) + (blue << 16) + (green << 8) + red;
}

inline void drawCircularHalo(const Widget::DrawArgs& args, const Vec boxSize, const NVGcolor haloColor,
    const unsigned char haloOpacity, const float radiusFactor) {
        {
            // Adapted from LightWidget
            // Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser
            if (args.fb) {
                return;
            }

            const float halo = settings::haloBrightness;
            if (halo == 0.f) {
                return;
            }

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
}

inline void drawRectHalo(const Widget::DrawArgs& args, const Vec boxSize, const NVGcolor haloColor,
    const unsigned char haloOpacity, const float positionX) {
    // Adapted from MindMeld & LightWidget.
    if (args.fb) {
        return;
    }

    const float halo = settings::haloBrightness;
    if (halo == 0.f) {
        return;
    }

    nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

    nvgBeginPath(args.vg);
    nvgRect(args.vg, -9 + positionX, -9, boxSize.x + 9, boxSize.y + 9);

    NVGcolor icol = color::mult(nvgTransRGBA(haloColor, haloOpacity), halo);

    NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
    NVGpaint paint = nvgBoxGradient(args.vg, 4.5f + positionX, 4.5f, boxSize.x - 4.5f, boxSize.y - 4.5f, 5.f, 8.f, icol, ocol);
    nvgFillPaint(args.vg, paint);
    nvgFill(args.vg);

    nvgFillPaint(args.vg, paint);
    nvgFill(args.vg);
    nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
}

inline void fillSvgSolidColor(const NSVGimage* svgImage, const unsigned int fillColor) {
    for (NSVGshape* shape = svgImage->shapes; shape; shape = shape->next) {
        shape->fill.color = fillColor;
        shape->fill.type = NSVG_PAINT_COLOR;
    }
}