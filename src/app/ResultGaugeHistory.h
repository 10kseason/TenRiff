#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "gameplay/ResultStats.h"

namespace tenriff::app {

struct DisplayGaugeSample {
    int64_t sample = 0;
    float value = 0.0f;
    bool shifted = false;
};

[[nodiscard]] inline float result_total_gauge_value(game::GaugeType type, double percent) {
    const double bounded = std::isfinite(percent) ? std::clamp(percent, 0.0, 100.0) : 0.0;
    const int band = type == game::GaugeType::ExHard ? 3 : type == game::GaugeType::Hard ? 2 :
                     type == game::GaugeType::Normal ? 1 : 0;
    // EX 0 and HARD 100 share a boundary (likewise HARD/NORMAL/EASY).
    // Gauge replenishment after a downshift can no longer reset the graph.
    return static_cast<float>((band + bounded / 100.0) / 4.0);
}

[[nodiscard]] inline std::vector<DisplayGaugeSample> result_display_gauge_history(
    const gameplay::ResultStats& stats, game::GaugeType final_type, bool gauge_shift) {
    std::vector<DisplayGaugeSample> result;
    result.reserve(stats.gauge_history.size());
    std::vector<gameplay::ShiftEvent> shifts = stats.shifts;
    std::stable_sort(shifts.begin(), shifts.end(), [](const auto& a, const auto& b) { return a.sample < b.sample; });
    game::GaugeType type = shifts.empty() ? final_type : shifts.front().from;
    std::size_t shift_index = 0;
    for (std::size_t i = 0; i < stats.gauge_history.size();) {
        // Old replay/result formats store value and time, but not a gauge type
        // per observation. Simultaneous judgements have one displayed end state;
        // collapsing them avoids treating a pre-shift value as the new gauge.
        std::size_t last = i;
        while (last + 1 < stats.gauge_history.size() &&
               stats.gauge_history[last + 1].sample == stats.gauge_history[i].sample) ++last;
        const auto& sample = stats.gauge_history[last];
        bool shifted = false;
        while (shift_index < shifts.size() && shifts[shift_index].sample <= sample.sample) {
            type = shifts[shift_index++].to;
            shifted = true;
        }
        const float value = gauge_shift ? result_total_gauge_value(type, sample.value) :
            static_cast<float>(std::isfinite(sample.value) ? std::clamp(sample.value / 100.0, 0.0, 1.0) : 0.0);
        result.push_back({sample.sample, value, shifted});
        i = last + 1;
    }
    return result;
}

} // namespace tenriff::app
