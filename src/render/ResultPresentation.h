#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace tenriff::render {

inline constexpr int64_t kResultPresentationDurationNs = 2'200'000'000LL;
inline constexpr int64_t kRecordScoreEffectStartNs = 650'000'000LL;
inline constexpr int64_t kRecordScoreEffectDurationNs = 1'200'000'000LL;

struct ResultTextRange {
    std::size_t start = 0;
    std::size_t length = 0;
};

// Wide string indices match the DirectWrite drawing-effect range. Matching the
// terminal localized token keeps the ordinary heading prefix out of its glow.
[[nodiscard]] std::optional<ResultTextRange> result_semantic_suffix_range(
    std::wstring_view text, std::wstring_view suffix);

struct ResultRecordEffectFrame {
    float score_visibility = 0.0f;
    float distortion = 0.0f;
    float old_rank_opacity = 0.0f;
    float old_rank_brightness = 0.0f;
    float shatter = 0.0f;
    float new_rank_opacity = 1.0f;
};

[[nodiscard]] ResultRecordEffectFrame result_record_effect_frame(int64_t elapsed_ns, bool rank_upgraded,
                                                                bool skipped = false);
[[nodiscard]] std::string result_record_score_text(int64_t target_score, float countup_progress,
                                                 int64_t elapsed_ns, bool new_record, bool skipped = false);

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
