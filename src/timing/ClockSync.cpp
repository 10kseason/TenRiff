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
    config_.rebase_after_consecutive_outliers =
        std::max<std::size_t>(1, config_.rebase_after_consecutive_outliers);
}

void ClockSync::add_sample(int64_t input_time_ns, int64_t audio_time_samples) {
    if (is_outlier(input_time_ns, audio_time_samples)) {
        ++consecutive_outliers_;
        if (consecutive_outliers_ < config_.rebase_after_consecutive_outliers) {
            return;
        }

        // A permanent offset is a clock rebase, not an endless stream of bad
        // samples. Drop the stale fit and seed a new one with the current pair.
        clear_fit();
    }
    consecutive_outliers_ = 0;

    if (samples_.size() >= config_.max_samples) {
        pop_oldest();
    }

    samples_.emplace_back(input_time_ns, audio_time_samples);

    auto regression = regress();
    if (!regression.has_value()) {
        return;
    }

    if (!estimate_.has_value()) {
        estimate_ = regression;
    } else {
        const double retained = 1.0 - config_.ema_alpha;
        const long double previous_at_anchor =
            predict_audio_samples(*estimate_, regression->input_anchor_ns);
        estimate_->slope =
            retained * estimate_->slope + config_.ema_alpha * regression->slope;
        estimate_->input_anchor_ns = regression->input_anchor_ns;
        estimate_->audio_anchor_samples = static_cast<double>(
            retained * previous_at_anchor +
            config_.ema_alpha * regression->audio_anchor_samples);
    }

    const long double predicted = predict_audio_samples(*estimate_, input_time_ns);
    const double residual = static_cast<double>(predicted - audio_time_samples);
    residuals_.push_back(residual);
    if (residuals_.size() > config_.max_samples) {
        residuals_.pop_front();
    }
}

void ClockSync::pop_oldest() {
    samples_.pop_front();
}

void ClockSync::clear_fit() {
    samples_.clear();
    residuals_.clear();
    estimate_.reset();
    last_output_samples_.reset();
    consecutive_outliers_ = 0;
}

std::optional<ClockSyncEstimate> ClockSync::regress() const {
    if (samples_.size() < 2) {
        return std::nullopt;
    }

    const int64_t input_origin_ns = samples_.front().first;
    const int64_t audio_origin_samples = samples_.front().second;
    const long double n = static_cast<long double>(samples_.size());
    long double mean_input_delta_ns = 0.0L;
    long double mean_audio_delta_samples = 0.0L;

    for (const auto& [input_time_ns, audio_time_samples] : samples_) {
        mean_input_delta_ns +=
            static_cast<long double>(input_time_ns - input_origin_ns);
        mean_audio_delta_samples +=
            static_cast<long double>(audio_time_samples - audio_origin_samples);
    }
    mean_input_delta_ns /= n;
    mean_audio_delta_samples /= n;

    long double numerator = 0.0L;
    long double denominator = 0.0L;
    for (const auto& [input_time_ns, audio_time_samples] : samples_) {
        const long double input_delta_ns =
            static_cast<long double>(input_time_ns - input_origin_ns) -
            mean_input_delta_ns;
        const long double audio_delta_samples =
            static_cast<long double>(audio_time_samples - audio_origin_samples) -
            mean_audio_delta_samples;
        numerator += input_delta_ns * audio_delta_samples;
        denominator += input_delta_ns * input_delta_ns;
    }

    if (std::abs(denominator) < 1e-9L) {
        return std::nullopt;
    }

    const long double slope = numerator / denominator;
    if (!std::isfinite(slope) || slope <= 0.0L) {
        return std::nullopt;
    }

    const int64_t input_anchor_ns = samples_.back().first;
    const long double anchor_delta_ns =
        static_cast<long double>(input_anchor_ns - input_origin_ns);
    const long double audio_anchor_samples =
        static_cast<long double>(audio_origin_samples) +
        mean_audio_delta_samples +
        slope * (anchor_delta_ns - mean_input_delta_ns);
    return ClockSyncEstimate{
        static_cast<double>(slope),
        input_anchor_ns,
        static_cast<double>(audio_anchor_samples),
    };
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

    const long double predicted = predict_audio_samples(*estimate_, input_time_ns);
    const double residual = static_cast<double>(predicted - audio_time_samples);

    auto mad_opt = residual_mad();
    if (!mad_opt.has_value()) {
        return false;
    }

    double width = std::max(config_.mad_floor_samples, mad_opt.value());
    double limit = width * config_.mad_tolerance;
    return std::abs(residual) > limit;
}

long double ClockSync::predict_audio_samples(const ClockSyncEstimate& estimate,
                                             int64_t input_time_ns) {
    // Subtract as integers before converting. On MSVC, long double has double
    // precision, so multiplying a multi-day absolute QPC value loses the local
    // callback-scale deltas that the fit needs.
    const int64_t input_delta_ns = input_time_ns - estimate.input_anchor_ns;
    return static_cast<long double>(estimate.audio_anchor_samples) +
           static_cast<long double>(estimate.slope) *
               static_cast<long double>(input_delta_ns);
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

    const long double predicted = predict_audio_samples(*estimate_, input_time_ns);
    int64_t mapped = static_cast<int64_t>(std::llround(predicted));
    if (last_output_samples_.has_value() && mapped < *last_output_samples_) {
        mapped = *last_output_samples_;
    }

    last_output_samples_ = mapped;
    return mapped;
}

void ClockSync::reset() {
    clear_fit();
}

}  // namespace tenriff::timing
