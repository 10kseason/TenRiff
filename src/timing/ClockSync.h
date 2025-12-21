#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace tenriff::timing {

struct ClockSyncEstimate {
    double slope = 0.0;
    double intercept = 0.0;
};

// Maintains a linear mapping between input timestamps (ns) and the audio clock (samples).
class ClockSync {
public:
    explicit ClockSync(std::size_t max_samples = 64);

    void add_sample(int64_t input_time_ns, int64_t audio_time_samples);

    [[nodiscard]] std::optional<int64_t> input_to_audio_samples(int64_t input_time_ns) const;
    [[nodiscard]] bool has_estimate() const;

private:
    void pop_oldest();
    [[nodiscard]] std::optional<ClockSyncEstimate> estimate() const;

    std::size_t max_samples_;
    std::deque<std::pair<int64_t, int64_t>> samples_;

    long double sum_input_ = 0.0L;
    long double sum_audio_ = 0.0L;
    long double sum_input_audio_ = 0.0L;
    long double sum_input_sq_ = 0.0L;
};

}  // namespace tenriff::timing

