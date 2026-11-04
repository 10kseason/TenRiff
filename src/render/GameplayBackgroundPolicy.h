#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace tenriff::render {

struct GameplayBackgroundPolicy {
    std::string base_path;
    std::string overlay_path;
    int64_t base_start_sample = 0;
    int64_t overlay_start_sample = 0;
    std::string upscale_mode = "off";
    bool loop_base_video = false;
};

inline GameplayBackgroundPolicy resolve_gameplay_background_policy(
    bool bga_enabled,
    std::string_view base_path,
    std::string_view overlay_path,
    int64_t base_start_sample,
    int64_t overlay_start_sample,
    std::string_view upscale_mode,
    bool common_bga_enabled = false,
    std::string_view common_bga_path = {}) {
    if (!bga_enabled) {
        return {};
    }
    GameplayBackgroundPolicy result;
    result.base_path = std::string(base_path);
    result.overlay_path = std::string(overlay_path);
    result.base_start_sample = base_start_sample;
    result.overlay_start_sample = overlay_start_sample;
    result.upscale_mode = std::string(upscale_mode);
    if (common_bga_enabled && !common_bga_path.empty()) {
        // Replace both authored layers; chart events must not reset the common loop.
        result.base_path = std::string(common_bga_path);
        result.overlay_path.clear();
        result.base_start_sample = result.overlay_start_sample = 0;
        result.loop_base_video = true;
    }
    return result;
}

}  // namespace tenriff::render
