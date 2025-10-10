#pragma once

namespace scalaria {
  static const std::vector<std::string> oscillatorNames = {
    "Off",
    "Triangle",
    "Saw",
    "Square"
  };

  static const uint8_t paletteFrequencies[10][3] = {
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
}