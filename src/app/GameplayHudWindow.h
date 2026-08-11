#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>

namespace tenriff::app {

struct GameplayHudWindow {
    int64_t past_samples = 0;
    int64_t lookahead_samples = 0;
};

inline double gameplay_reference_visual_span(double base_bpm,
                                             double playback_rate,
                                             int sample_rate,
                                             int64_t window_samples) {
    constexpr double kBeatsPerMeasure = 4.0;
    if (!std::isfinite(base_bpm) || base_bpm <= 0.0 ||
        !std::isfinite(playback_rate) || playback_rate <= 0.0 ||
        sample_rate <= 0 || window_samples <= 0) {
        return 1.0;
    }

    const double span = static_cast<double>(window_samples) * base_bpm * playback_rate /
                        (kBeatsPerMeasure * 60.0 * static_cast<double>(sample_rate));
    return std::max(1e-9, span);
}

inline GameplayHudWindow expand_gameplay_hud_window(int sample_rate,
                                                    int64_t base_past_samples,
                                                    int64_t base_lookahead_samples,
                                                    double visual_offset_ms,
                                                    double render_slack_ms) {
    if (sample_rate <= 0) {
        return GameplayHudWindow{
            std::max<int64_t>(0, base_past_samples),
            std::max<int64_t>(0, base_lookahead_samples),
        };
    }

    const auto ms_to_samples = [sample_rate](double ms) -> int64_t {
        return static_cast<int64_t>(std::llround(ms * static_cast<double>(sample_rate) / 1000.0));
    };

    const int64_t visual_offset_samples = ms_to_samples(visual_offset_ms);
    const int64_t render_slack_samples = std::max<int64_t>(0, ms_to_samples(render_slack_ms));

    GameplayHudWindow window;
    window.past_samples = std::max<int64_t>(
        0, base_past_samples + std::max<int64_t>(0, -visual_offset_samples) + render_slack_samples);
    window.lookahead_samples = std::max<int64_t>(
        0, base_lookahead_samples + std::max<int64_t>(0, visual_offset_samples) + render_slack_samples);
    return window;
}

}  // namespace tenriff::app
