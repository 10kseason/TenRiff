#pragma once

#include "render/GameplaySongTitle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace tenriff::render {

struct GameplayStudioDeckLayout {
    float info_x = 0.0f;
    float info_y = 52.0f;
    float info_width = 0.0f;
    float info_scale = 1.0f;
    float score_x = 0.0f;
    float score_y = 0.0f;
    float score_width = 0.0f;
    float score_scale = 1.0f;
    float map_x = 0.0f;
    float map_top = 924.0f;
    float map_height = 96.0f;
    float map_width = 0.0f;
    float map_scale = 1.0f;
    float gauge_marker_x = 0.0f;
    bool info_visible = false;
    bool score_visible = false;
    bool map_visible = false;
    bool map_times_visible = false;
};

inline float gameplay_studio_header_safe_right(bool performance_visible) {
    // The full gameplay/skin-preview performance panel begins at 1450.
    // Share its 28px gap with static and dynamic deck layout invalidation.
    return performance_visible ? 1422.0f : 1856.0f;
}

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
    if (!std::isfinite(field_left) || !std::isfinite(field_right) || field_right <= field_left)
        return layout;
    const bool gauge_on_left = field_right + 114.0f > 1896.0f && field_left >= 138.0f;
    layout.gauge_marker_x = gauge_on_left ? field_left - 114.0f : field_right + 18.0f;
    const float left_end = std::clamp(field_left - (gauge_on_left ? 132.0f : 32.0f), 24.0f, 1856.0f);
    const float right_start = std::clamp(field_right + (gauge_on_left ? 32.0f : 132.0f), 24.0f, 1856.0f);
    const float safe_right = std::isfinite(header_safe_right)
        ? std::clamp(header_safe_right, 24.0f, 1856.0f) : 1856.0f;
    struct Span { float x; float width; };
    const auto left_span = [&](float end) {
        return Span{24.0f, std::min(600.0f, std::max(0.0f, std::min(left_end, end) - 24.0f))};
    };
    const auto right_span = [&](float end) {
        const float width = std::min(600.0f, std::max(0.0f, end - right_start));
        return Span{end - width, width};
    };
    const auto prefer = [](Span preferred, Span other) {
        return preferred.width >= 220.0f || preferred.width >= other.width ? preferred : other;
    };
    const auto info = prefer(right_span(safe_right), left_span(safe_right));
    const auto score = prefer(left_span(1856.0f), right_span(1856.0f));
    const auto map = prefer(right_span(1856.0f), left_span(1856.0f));
    layout.info_x = info.x; layout.info_width = info.width;
    layout.score_x = score.x; layout.score_width = score.width;
    layout.map_x = map.x; layout.map_width = map.width;
    layout.info_visible = info.width > 0.0f;
    layout.score_visible = score.width > 0.0f;
    layout.map_visible = riff_map_visible && map.width > 0.0f;
    layout.map_times_visible = layout.map_visible && map.width >= 160.0f;
    // Fit the whole card/number group into narrow side gaps instead of letting
    // individual glyphs spill into the field. The title keeps measured fitting.
    layout.info_scale = std::min(1.0f, info.width / 300.0f);
    layout.score_scale = std::min(1.0f, score.width / 320.0f);
    // The map needs less width than the score card: its two 76px end clocks
    // fit comfortably at 280px. Keep ordinary side gaps at native glyph size;
    // only genuinely narrow maps need a fractional clock/bar transform.
    layout.map_scale = std::min(1.0f, map.width / 280.0f);
    layout.map_height = 96.0f * layout.map_scale;
    layout.map_top = 1020.0f - layout.map_height;
    const bool share_bottom_column = map.width > 0.0f &&
        score.x < map.x + map.width && map.x < score.x + score.width;
    const float score_bottom = share_bottom_column ? layout.map_top - 56.0f : 1020.0f;
    layout.score_y = score_bottom - 444.0f * layout.score_scale;
    return layout;
}

inline GameplaySongTitleLayout gameplay_studio_song_title_layout(const GameplayStudioDeckLayout& deck) {
    return {deck.info_x, deck.info_y, deck.info_x + deck.info_width,
            deck.info_y + 34.0f * deck.info_scale};
}

inline float gameplay_studio_gauge_marker_y(float hit_y, double gauge_ratio) {
    const double ratio = std::isfinite(gauge_ratio) ? std::clamp(gauge_ratio, 0.0, 1.0) : 0.0;
    const float safe_hit_y = std::isfinite(hit_y) ? std::max(0.0f, hit_y) : 0.0f;
    return std::clamp(safe_hit_y * (1.0f - static_cast<float>(ratio)) - 20.0f,
                      8.0f, std::max(8.0f, safe_hit_y - 66.0f));
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
