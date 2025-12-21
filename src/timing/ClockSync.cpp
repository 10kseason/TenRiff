#include "timing/ClockSync.h"

#include <algorithm>
#include <cmath>

namespace tenriff::timing {

ClockSync::ClockSync(std::size_t max_samples) : max_samples_(std::max<std::size_t>(2, max_samples)) {}

void ClockSync::add_sample(int64_t input_time_ns, int64_t audio_time_samples) {
    if (samples_.size() >= max_samples_) {
        pop_oldest();
    }

    samples_.emplace_back(input_time_ns, audio_time_samples);
    sum_input_ += input_time_ns;
    sum_audio_ += audio_time_samples;
    sum_input_audio_ += static_cast<long double>(input_time_ns) * static_cast<long double>(audio_time_samples);
    sum_input_sq_ += static_cast<long double>(input_time_ns) * static_cast<long double>(input_time_ns);
}

void ClockSync::pop_oldest() {
    auto [input_time, audio_time] = samples_.front();
    samples_.pop_front();

    sum_input_ -= input_time;
    sum_audio_ -= audio_time;
    sum_input_audio_ -= static_cast<long double>(input_time) * static_cast<long double>(audio_time);
    sum_input_sq_ -= static_cast<long double>(input_time) * static_cast<long double>(input_time);
}

std::optional<ClockSyncEstimate> ClockSync::estimate() const {
    const auto n = static_cast<long double>(samples_.size());
    if (n < 2) {
        return std::nullopt;
    }

    long double denominator = n * sum_input_sq_ - sum_input_ * sum_input_;
    if (std::abs(denominator) < 1e-9L) {
        return std::nullopt;
    }

    long double slope = (n * sum_input_audio_ - sum_input_ * sum_audio_) / denominator;
    long double intercept = (sum_audio_ - slope * sum_input_) / n;
    return ClockSyncEstimate{static_cast<double>(slope), static_cast<double>(intercept)};
}

std::optional<int64_t> ClockSync::input_to_audio_samples(int64_t input_time_ns) const {
    auto estimate_opt = estimate();
    if (!estimate_opt.has_value()) {
        return std::nullopt;
    }

    const auto& est = estimate_opt.value();
    long double predicted = est.slope * static_cast<long double>(input_time_ns) + est.intercept;
    return static_cast<int64_t>(std::llround(predicted));
}

bool ClockSync::has_estimate() const { return estimate().has_value(); }

}  // namespace tenriff::timing

