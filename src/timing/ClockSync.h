#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace tenriff::timing {

struct ClockSyncEstimate {
    double slope = 0.0;
    int64_t input_anchor_ns = 0;
    double audio_anchor_samples = 0.0;
};

struct ClockSyncConfig {
    std::size_t max_samples = 64;
    double ema_alpha = 0.25;           // Smoothing for slope/anchor updates.
    double mad_tolerance = 3.5;        // Outlier rejection threshold multiplier.
    double mad_floor_samples = 16.0;   // Floor to avoid zero-width MAD windows.
    std::size_t min_outlier_samples = 3;  // Require enough residuals before rejecting.
    std::size_t rebase_after_consecutive_outliers = 8;  // Recover from a lasting audio-clock discontinuity.
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
    void clear_fit();
    [[nodiscard]] std::optional<ClockSyncEstimate> regress() const;
    [[nodiscard]] std::optional<double> residual_mad() const;
    [[nodiscard]] bool is_outlier(int64_t input_time_ns, int64_t audio_time_samples) const;
    [[nodiscard]] static long double predict_audio_samples(const ClockSyncEstimate& estimate,
                                                           int64_t input_time_ns);
    [[nodiscard]] static double median(std::vector<double>& values);

    ClockSyncConfig config_{};

    std::deque<std::pair<int64_t, int64_t>> samples_;
    std::deque<double> residuals_;

    std::optional<ClockSyncEstimate> estimate_;
    std::optional<int64_t> last_output_samples_;
    std::size_t consecutive_outliers_ = 0;
};

}  // namespace tenriff::timing
