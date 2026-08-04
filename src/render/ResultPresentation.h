#pragma once

#include <array>
#include <cstdint>

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

}  // namespace tenriff::render
