#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace tenriff::render {

struct GameplayStudioDeckLayout {
    float map_x = 0.0f;
    float map_top = 60.0f;
    float map_height = 960.0f;
    float map_width = 56.0f;
    float left_x = 84.0f;
    float left_y = 52.0f;
    float left_width = 260.0f;
    float right_x = 0.0f;
    float right_y = 48.0f;
    float right_width = 0.0f;
    bool map_visible = false;
    bool map_times_visible = false;
    bool right_hud_visible = false;
};

inline bool gameplay_studio_deck_enabled(std::string_view hud_layout,
                                         std::string_view skin_source,
                                         bool ghost_visible,
                                         bool peer_visible) {
    // LR2 imports keep their note/key/gear assets and geometry; only the HUD
    // adopts the deck. Other image skins and shared battle HUDs remain classic.
    return hud_layout == "studio" && (skin_source == "native" || skin_source == "lr2") &&
           !ghost_visible && !peer_visible;
}

inline GameplayStudioDeckLayout compute_gameplay_studio_deck_layout(
    float field_left, float field_right, float header_safe_right = 1856.0f,
    bool riff_map_visible = true) {
    GameplayStudioDeckLayout layout;
    layout.map_x = field_left - 80.0f;
    layout.left_width = std::max(260.0f, layout.map_x - layout.left_x - 70.0f);
    layout.right_x = field_right + 100.0f;
    // Measure remaining space before choosing the fallback. A 240 px minimum
    // applied first would make the under-200 fallback unreachable, and could
    // draw beneath the performance overlay after its safe edge moves left.
    layout.right_width = std::max(0.0f, std::min(1856.0f, header_safe_right) - layout.right_x);
    layout.right_hud_visible = layout.right_width >= 200.0f;
    layout.map_visible = riff_map_visible && layout.map_x >= layout.left_x + 220.0f;
    layout.map_times_visible = layout.map_visible && layout.map_x - 58.0f >= layout.left_x + 220.0f;
    return layout;
}

inline float gameplay_studio_gauge_marker_y(float hit_y, double gauge_ratio) {
    const double ratio = std::isfinite(gauge_ratio) ? std::clamp(gauge_ratio, 0.0, 1.0) : 0.0;
    const float safe_hit_y = std::isfinite(hit_y) ? std::max(0.0f, hit_y) : 0.0f;
    return std::clamp(safe_hit_y * (1.0f - static_cast<float>(ratio)) - 12.0f,
                      8.0f, std::max(8.0f, safe_hit_y - 45.0f));
}

inline std::array<float, 5> gameplay_studio_judgement_ratios(const std::array<int, 5>& counts) {
    int64_t total = 0;
    for (const int count : counts) total += std::max(0, count);
    std::array<float, 5> ratios{};
    if (total == 0) return ratios;
    for (std::size_t i = 0; i < ratios.size(); ++i)
        ratios[i] = static_cast<float>(static_cast<double>(std::max(0, counts[i])) / total);
    return ratios;
}

struct GameplayStudioScoreCounter {
    std::wstring padded;
    std::wstring significant;
};

// Native scores are clamped to kNativeScoreMaximum = 10,000 in ResultStats.
// Keep overflow digits for future score formats and construct these strings
// only when the scene text revision changes, never in the frame drawing path.
inline GameplayStudioScoreCounter make_gameplay_studio_score_counter(
    int64_t score, std::size_t min_digits = 5) {
    GameplayStudioScoreCounter result;
    result.significant = std::to_wstring(std::max<int64_t>(0, score));
    result.padded = result.significant;
    // int64_t has at most 19 digits; bound optional padding too.
    min_digits = std::clamp<std::size_t>(min_digits, 1, 19);
    if (result.padded.size() < min_digits)
        result.padded.insert(0, min_digits - result.padded.size(), L'0');
    const auto group = [](std::wstring& digits) {
        for (std::size_t position = digits.size(); position > 3;) {
            position -= 3;
            digits.insert(position, 1, L',');
        }
    };
    group(result.padded);
    group(result.significant);
    return result;
}

}  // namespace tenriff::render
