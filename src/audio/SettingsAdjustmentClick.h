#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tenriff::audio {

// A short local UI click. The same waveform is mixed into gameplay buffers and
// rendered to an in-memory WAV for menu controls; neither path loads an asset.
class SettingsAdjustmentClick {
public:
    static constexpr int kDurationMs = 12;

    void trigger(int sample_rate) noexcept {
        sample_rate_ = std::max(1, sample_rate);
        duration_ = std::max(1, sample_rate_ * kDurationMs / 1000);
        cursor_ = 0;
    }

    void reset() noexcept { cursor_ = duration_; }
    [[nodiscard]] bool active() const noexcept { return cursor_ < duration_; }

    void mix(float* stereo, std::uint32_t frames) noexcept {
        if (!stereo) return;
        for (std::uint32_t frame = 0; frame < frames && active(); ++frame, ++cursor_) {
            const double phase = 6.28318530717958647692 * 1800.0 * cursor_ / sample_rate_;
            const double envelope = 1.0 - static_cast<double>(cursor_) / duration_;
            const float value = static_cast<float>(
                (std::sin(phase) + 0.35 * std::sin(phase * 2.3)) *
                envelope * envelope * 0.12);
            stereo[frame * 2] += value;
            stereo[frame * 2 + 1] += value;
        }
    }

private:
    int sample_rate_ = 48000;
    int duration_ = 0;
    int cursor_ = 0;
};

}  // namespace tenriff::audio
