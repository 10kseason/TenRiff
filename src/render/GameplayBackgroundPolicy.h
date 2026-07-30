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
};

inline GameplayBackgroundPolicy resolve_gameplay_background_policy(
    bool bga_enabled,
    std::string_view base_path,
    std::string_view overlay_path,
    int64_t base_start_sample,
    int64_t overlay_start_sample,
    std::string_view upscale_mode) {
    if (!bga_enabled) {
        return {};
    }
    GameplayBackgroundPolicy result;
    result.base_path = std::string(base_path);
    result.overlay_path = std::string(overlay_path);
    result.base_start_sample = base_start_sample;
    result.overlay_start_sample = overlay_start_sample;
    result.upscale_mode = std::string(upscale_mode);
    return result;
}

}  // namespace tenriff::render
