#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tenriff::audio {

// Stereo-linked RMS auto level for the combined gameplay mix. No allocations,
// I/O or locks in process(). This is not offline LUFS mastering. A silence gate
// holds gain rather than amplifying a quiet gap; the existing limiter follows it.
class MixNormalizer {
public:
    void reset(int sample_rate) {
        const double rate = std::max(1, sample_rate);
        energy_alpha_ = 1.0 - std::exp(-1.0 / (rate * 0.4));
        attack_alpha_ = 1.0 - std::exp(-1.0 / (rate * 0.08));
        release_alpha_ = 1.0 - std::exp(-1.0 / (rate * 2.0));
        energy_ = 0.0;
        gain_ = 1.0;
    }

    void process(float* stereo, uint32_t frames) {
        if (!stereo) return;
        for (uint32_t frame = 0; frame < frames; ++frame) {
            float& left = stereo[frame * 2];
            float& right = stereo[frame * 2 + 1];
            if (!std::isfinite(left)) left = 0;
            if (!std::isfinite(right)) right = 0;
            const double power = (static_cast<double>(left) * left + static_cast<double>(right) * right) * 0.5;
            energy_ += energy_alpha_ * (power - energy_);
            if (energy_ > 0.000025 && power > 0.000001) {
                const double desired = std::clamp(0.16 / std::sqrt(energy_), 0.10, 2.0);
                gain_ += (desired < gain_ ? attack_alpha_ : release_alpha_) * (desired - gain_);
            }
            left *= static_cast<float>(gain_);
            right *= static_cast<float>(gain_);
        }
    }

private:
    double energy_ = 0.0;
    double gain_ = 1.0;
    double energy_alpha_ = 0.0;
    double attack_alpha_ = 0.0;
    double release_alpha_ = 0.0;
};

} // namespace tenriff::audio
