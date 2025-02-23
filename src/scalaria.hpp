#pragma once

static const std::vector<std::string> scalariaOscillatorNames = { "Off", "Triangle", "Saw", "Square" };

static const uint8_t paletteScalaria[10][3] = {
  { 255, 0, 0 },
  { 255, 64, 0 },
  { 255, 192, 0 },
  { 255, 255, 0 },
  { 64, 255, 0 },
  { 0, 192, 64 },
  { 0, 255, 192 },
  { 0, 0, 255 },
  { 192, 0, 192 }
};

static const float kScalariaSampleRate = 96000.f;