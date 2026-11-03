#include "render/ResultPresentation.h"
#include "render/GameplayGaugePalette.h"

#include <algorithm>
#include <cmath>

namespace tenriff::render {
namespace {

float phase(double seconds, double start, double end) {
    if (end <= start) {
        return seconds >= end ? 1.0f : 0.0f;
    }
    return static_cast<float>(std::clamp((seconds - start) / (end - start), 0.0, 1.0));
}

float smooth(float value) {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

float pulse(double seconds, double center, double half_width) {
    if (half_width <= 0.0) {
        return 0.0f;
    }
    return static_cast<float>(
        std::clamp(1.0 - std::abs(seconds - center) / half_width, 0.0, 1.0));
}

}  // namespace

std::optional<ResultTextRange> result_semantic_suffix_range(std::wstring_view text, std::wstring_view suffix) {
    if (suffix.empty() || suffix.size() > text.size()) return std::nullopt;
    const std::size_t start = text.size() - suffix.size();
    if (text.substr(start) != suffix) return std::nullopt;
    return ResultTextRange{start, suffix.size()};
}

ResultRecordEffectFrame result_record_effect_frame(int64_t elapsed_ns, bool rank_upgraded, bool skipped) {
    ResultRecordEffectFrame frame;
    if (skipped) {
        frame.score_visibility = 1.0f;
        return frame;
    }
    const double seconds = std::max(0.0, static_cast<double>(elapsed_ns) * 1e-9);
    frame.score_visibility = smooth(phase(seconds, 0.65, 0.75));
    if (elapsed_ns >= kRecordScoreEffectStartNs &&
        elapsed_ns < kRecordScoreEffectStartNs + kRecordScoreEffectDurationNs)
        frame.distortion = 1.0f - phase(seconds, 0.65, 1.85) * 0.65f;
    if (rank_upgraded) {
        frame.old_rank_brightness = smooth(phase(seconds, 0.65, 1.25));
        frame.shatter = smooth(phase(seconds, 1.25, 1.65));
        frame.old_rank_opacity = smooth(phase(seconds, 0.65, 0.82)) * (1.0f - frame.shatter);
        frame.new_rank_opacity = smooth(phase(seconds, 1.50, 1.95));
    }
    return frame;
}

std::string result_record_score_text(int64_t target_score, float countup_progress,
                                     int64_t elapsed_ns, bool new_record, bool skipped) {
    if (!new_record) return std::to_string(result_counted_score(target_score, countup_progress));
    target_score = std::max<int64_t>(0, target_score);
    if (skipped || elapsed_ns >= kRecordScoreEffectStartNs + kRecordScoreEffectDurationNs)
        return std::to_string(target_score);
    if (elapsed_ns < kRecordScoreEffectStartNs) return "0";
    // Fixed 30 Hz deterministic interference, never written back into score
    // state. The final frame always resolves to the exact stored integer.
    std::string text = std::to_string(target_score);
    const uint64_t tick = static_cast<uint64_t>((elapsed_ns - kRecordScoreEffectStartNs) * 30 / 1'000'000'000LL);
    for (std::size_t i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>('0' + (tick * 7 + i * 3 + static_cast<uint64_t>(target_score) + 1) % 10);
    return text;
}

std::optional<uint32_t> result_gauge_shift_clear_color(std::string_view status) {
    if (status == "GAUGE SHIFT EASY CLEAR") return 0x72DB98;
    if (status == "GAUGE SHIFT NORMAL CLEAR") return 0xFFE477;
    if (status == "GAUGE SHIFT HARD CLEAR") return 0xFFAA60;
    if (status == "GAUGE SHIFT EX CLEAR") return kClearExRgb;
    return std::nullopt;
}

ResultPresentationFrame result_presentation_frame(int64_t elapsed_ns, bool skipped) {
    ResultPresentationFrame frame;
    const double seconds = skipped
                               ? 10.0
                               : std::max(0.0, static_cast<double>(elapsed_ns) / 1'000'000'000.0);

    frame.background = smooth(phase(seconds, 0.00, 0.20));
    frame.information = smooth(phase(seconds, 0.15, 0.50));
    frame.prism = smooth(phase(seconds, 0.30, 0.85));
    frame.score = smooth(phase(seconds, 0.65, 1.20));
    frame.rank = smooth(phase(seconds, 1.10, 1.45));
    frame.status = smooth(phase(seconds, 1.40, 1.70));
    for (std::size_t index = 0; index < frame.statistics.size(); ++index) {
        const double start = 1.65 + static_cast<double>(index) * 0.06;
        frame.statistics[index] = smooth(phase(seconds, start, start + 0.28));
    }
    frame.graph = smooth(phase(seconds, 1.90, 2.30));
    frame.controls = smooth(phase(seconds, 2.05, 2.20));
    frame.all_perfect = smooth(phase(seconds, 2.05, 2.42));
    frame.interaction_ready = skipped || elapsed_ns >= kResultPresentationDurationNs;

    if (!skipped) {
        frame.score_pulse = pulse(seconds, 1.22, 0.12);
        frame.chromatic = pulse(seconds, 1.15, 0.05);
        frame.shockwave = smooth(phase(seconds, 1.10, 1.42));
        frame.status_scan = phase(seconds, 1.40, 1.70);
    } else {
        frame.controls = 1.0f;
        frame.graph = 1.0f;
        frame.all_perfect = 1.0f;
    }
    return frame;
}

int64_t result_counted_score(int64_t target_score, float progress) {
    if (target_score <= 0) {
        return 0;
    }
    const double p = std::clamp(static_cast<double>(progress), 0.0, 1.0);
    const int64_t slow_tail = std::min<int64_t>(999, target_score);
    const int64_t fast_target = target_score - slow_tail;
    if (p < 0.76) {
        const double fast_progress = p / 0.76;
        const double eased = 1.0 - std::pow(1.0 - fast_progress, 4.0);
        return static_cast<int64_t>(std::llround(static_cast<double>(fast_target) * eased));
    }
    const double tail_progress = (p - 0.76) / 0.24;
    const double eased_tail = tail_progress * tail_progress * (3.0 - 2.0 * tail_progress);
    return fast_target +
           static_cast<int64_t>(std::llround(static_cast<double>(slow_tail) * eased_tail));
}

}  // namespace tenriff::render
