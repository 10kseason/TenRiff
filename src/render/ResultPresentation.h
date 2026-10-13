#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace tenriff::render {

inline constexpr int64_t kResultPresentationDurationNs = 2'200'000'000LL;

struct ResultPresentationFrame {
    float background = 0.0f;
    float information = 0.0f;
    float prism = 0.0f;
    float score = 0.0f;
    float rank = 0.0f;
    float status = 0.0f;
    std::array<float, 7> statistics{};
    float graph = 0.0f;
    float controls = 0.0f;
    float all_perfect = 0.0f;
    float score_pulse = 0.0f;
    float chromatic = 0.0f;
    float shockwave = 0.0f;
    float status_scan = 0.0f;
    bool interaction_ready = false;
};

[[nodiscard]] ResultPresentationFrame result_presentation_frame(int64_t elapsed_ns,
                                                                bool skipped = false);
[[nodiscard]] int64_t result_counted_score(int64_t target_score, float progress);
// Only Gauge Shift clears receive these accents; other result states keep their skin color.
[[nodiscard]] std::optional<uint32_t> result_gauge_shift_clear_color(std::string_view status);

}  // namespace tenriff::render
