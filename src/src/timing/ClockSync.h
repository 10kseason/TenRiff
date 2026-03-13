#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace tenriff::timing {

struct ClockSyncEstimate {
    double slope = 0.0;
    double intercept = 0.0;
};

struct ClockSyncConfig {
    std::size_t max_samples = 64;
    double ema_alpha = 0.25;           // Smoothing for slope/intercept updates.
    double mad_tolerance = 3.5;        // Outlier rejection threshold multiplier.
    double mad_floor_samples = 16.0;   // Floor to avoid zero-width MAD windows.
    std::size_t min_outlier_samples = 3;  // Require enough residuals before rejecting.
};

// Maintains a robust, monotonic mapping between input timestamps (ns) and the audio clock (samples).
class ClockSync {
public:
    explicit ClockSync(const ClockSyncConfig& config = {});

    void add_sample(int64_t input_time_ns, int64_t audio_time_samples);
    void reset();

    [[nodiscard]] std::optional<int64_t> input_to_audio_samples(int64_t input_time_ns);
    [[nodiscard]] bool has_estimate() const { return estimate_.has_value(); }

private:
    void pop_oldest();
    [[nodiscard]] std::optional<ClockSyncEstimate> regress() const;
    [[nodiscard]] std::optional<double> residual_mad() const;
    [[nodiscard]] bool is_outlier(int64_t input_time_ns, int64_t audio_time_samples) const;
    [[nodiscard]] static double median(std::vector<double>& values);

    ClockSyncConfig config_{};

    std::deque<std::pair<int64_t, int64_t>> samples_;
    std::deque<double> residuals_;

    std::optional<ClockSyncEstimate> estimate_;
    std::optional<int64_t> last_output_samples_;

    long double sum_input_ = 0.0L;
    long double sum_audio_ = 0.0L;
    long double sum_input_audio_ = 0.0L;
    long double sum_input_sq_ = 0.0L;
};

}  // namespace tenriff::timing

