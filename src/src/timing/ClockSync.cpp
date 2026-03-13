#include "timing/ClockSync.h"

#include <algorithm>
#include <cmath>

namespace tenriff::timing {

ClockSync::ClockSync(const ClockSyncConfig& config) : config_(config) {
    config_.max_samples = std::max<std::size_t>(2, config_.max_samples);
    config_.ema_alpha = std::clamp(config_.ema_alpha, 0.0, 1.0);
    config_.mad_tolerance = std::max(0.0, config_.mad_tolerance);
    config_.mad_floor_samples = std::max(0.0, config_.mad_floor_samples);
    config_.min_outlier_samples = std::max<std::size_t>(1, config_.min_outlier_samples);
}

void ClockSync::add_sample(int64_t input_time_ns, int64_t audio_time_samples) {
    if (is_outlier(input_time_ns, audio_time_samples)) {
        return;
    }

    if (samples_.size() >= config_.max_samples) {
        pop_oldest();
    }

    samples_.emplace_back(input_time_ns, audio_time_samples);
    sum_input_ += input_time_ns;
    sum_audio_ += audio_time_samples;
    sum_input_audio_ += static_cast<long double>(input_time_ns) * static_cast<long double>(audio_time_samples);
    sum_input_sq_ += static_cast<long double>(input_time_ns) * static_cast<long double>(input_time_ns);

    auto regression = regress();
    if (!regression.has_value()) {
        return;
    }

    if (!estimate_.has_value()) {
        estimate_ = regression;
    } else {
        estimate_->slope = static_cast<double>((1.0 - config_.ema_alpha) * estimate_->slope +
                                               config_.ema_alpha * regression->slope);
        estimate_->intercept = static_cast<double>((1.0 - config_.ema_alpha) * estimate_->intercept +
                                                   config_.ema_alpha * regression->intercept);
    }

    const long double predicted = estimate_->slope * static_cast<long double>(input_time_ns) + estimate_->intercept;
    const double residual = static_cast<double>(predicted - audio_time_samples);
    residuals_.push_back(residual);
    if (residuals_.size() > config_.max_samples) {
        residuals_.pop_front();
    }
}

void ClockSync::pop_oldest() {
    auto [input_time, audio_time] = samples_.front();
    samples_.pop_front();

    sum_input_ -= input_time;
    sum_audio_ -= audio_time;
    sum_input_audio_ -= static_cast<long double>(input_time) * static_cast<long double>(audio_time);
    sum_input_sq_ -= static_cast<long double>(input_time) * static_cast<long double>(input_time);
}

std::optional<ClockSyncEstimate> ClockSync::regress() const {
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

std::optional<double> ClockSync::residual_mad() const {
    if (residuals_.empty()) {
        return std::nullopt;
    }

    std::vector<double> values(residuals_.begin(), residuals_.end());
    double med = median(values);
    for (auto& value : values) {
        value = std::abs(value - med);
    }

    return median(values);
}

bool ClockSync::is_outlier(int64_t input_time_ns, int64_t audio_time_samples) const {
    if (!estimate_.has_value()) {
        return false;
    }

    if (residuals_.size() < config_.min_outlier_samples) {
        return false;
    }

    const long double predicted = estimate_->slope * static_cast<long double>(input_time_ns) + estimate_->intercept;
    const double residual = static_cast<double>(predicted - audio_time_samples);

    auto mad_opt = residual_mad();
    if (!mad_opt.has_value()) {
        return false;
    }

    double width = std::max(config_.mad_floor_samples, mad_opt.value());
    double limit = width * config_.mad_tolerance;
    return std::abs(residual) > limit;
}

double ClockSync::median(std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }

    std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
    double mid = values[values.size() / 2];
    if (values.size() % 2 == 1) {
        return mid;
    }

    auto max_it = std::max_element(values.begin(), values.begin() + values.size() / 2);
    return (mid + *max_it) / 2.0;
}

std::optional<int64_t> ClockSync::input_to_audio_samples(int64_t input_time_ns) {
    if (!estimate_.has_value()) {
        return std::nullopt;
    }

    const long double predicted = estimate_->slope * static_cast<long double>(input_time_ns) + estimate_->intercept;
    int64_t mapped = static_cast<int64_t>(std::llround(predicted));
    if (last_output_samples_.has_value() && mapped < *last_output_samples_) {
        mapped = *last_output_samples_;
    }

    last_output_samples_ = mapped;
    return mapped;
}

void ClockSync::reset() {
    samples_.clear();
    residuals_.clear();
    estimate_.reset();
    last_output_samples_.reset();

    sum_input_ = 0.0L;
    sum_audio_ = 0.0L;
    sum_input_audio_ = 0.0L;
    sum_input_sq_ = 0.0L;
}

}  // namespace tenriff::timing

