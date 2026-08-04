#include "render/ResultPresentation.h"

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
