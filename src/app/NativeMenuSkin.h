#pragma once

#include <array>
#include <string>
#include <unordered_map>

namespace tenriff::app {

// Optional presentation overrides shared by the manifest loader and menu renderer.
// Slot names are defined in tools/skin_editor/native-catalog.json. Omitted slots use
// renderer defaults; none of these values are read by the gameplay simulation.
struct NativeMenuSkinStyle {
    std::unordered_map<std::string, float> metrics;
    std::unordered_map<std::string, std::array<float, 4>> colors;
    // Additive [dx, dy, dwidth, dheight] adjustments to each computed base rect.
    // Repeated rows therefore preserve their own positions when edited together.
    std::unordered_map<std::string, std::array<float, 4>> rects;
    std::unordered_map<std::string, std::string> assets;
    std::unordered_map<std::string, float> motion;
    std::unordered_map<std::string, std::string> fonts;
};

}  // namespace tenriff::app
