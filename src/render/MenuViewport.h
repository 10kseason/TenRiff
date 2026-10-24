#pragma once

#include <algorithm>

namespace tenriff::render {

// Skin rectangles and pointer targets stay in the authored 1920x1080 space.
// Wide displays expose additional background on either side instead of
// stretching notes/text or changing saved skin coordinates. Narrow displays
// retain their existing top/bottom letterbox behavior.
struct MenuViewport {
    float scale = 1.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float background_left = 0.0f;
    float background_right = 1920.0f;
};

inline MenuViewport menu_viewport(int width, int height) {
    if (width <= 0 || height <= 0) return {};
    MenuViewport result;
    result.scale = std::min(width / 1920.0f, height / 1080.0f);
    result.offset_x = (width - 1920.0f * result.scale) * 0.5f;
    result.offset_y = (height - 1080.0f * result.scale) * 0.5f;
    const float side_extension = std::max(0.0f, result.offset_x / result.scale);
    result.background_left = -side_extension;
    result.background_right = 1920.0f + side_extension;
    return result;
}

}  // namespace tenriff::render
