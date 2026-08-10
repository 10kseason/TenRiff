#pragma once

#include <algorithm>
#include <cmath>

namespace tenriff::app {

inline constexpr float kOutputSoftLimitThreshold = 0.92f;

[[nodiscard]] inline float soft_limit_audio_sample(float sample) {
    const float abs_sample = std::abs(sample);
    if (abs_sample <= kOutputSoftLimitThreshold) {
        return sample;
    }

    const float excess = abs_sample - kOutputSoftLimitThreshold;
    const float compressed = kOutputSoftLimitThreshold +
                             (1.0f - kOutputSoftLimitThreshold) *
                                 (excess / (1.0f + excess));
    return std::copysign(std::min(1.0f, compressed), sample);
}

// Limit the combined mix first, then apply master volume. Applying master
// before the limiter makes loud mixes stay nearly full-volume across much of
// the master slider.
[[nodiscard]] inline float apply_master_volume_to_sample(float mixed_sample,
                                                         double master_volume) {
    const float master = static_cast<float>(std::clamp(master_volume, 0.0, 1.0));
    return soft_limit_audio_sample(mixed_sample) * master;
}

[[nodiscard]] inline float gameplay_bgm_gain(double bgm_volume) {
    return static_cast<float>(std::clamp(bgm_volume, 0.0, 2.0));
}

// MCI exposes a 0..1000 volume range and cannot represent the gameplay mix's
// 200% boost. Clamp the BGM submix first so master remains responsive over its
// entire 0..100% range instead of saturating master*bgm at 100%.
[[nodiscard]] inline double menu_background_gain(bool enabled,
                                                 double master_volume,
                                                 double bgm_volume) {
    if (!enabled) {
        return 0.0;
    }
    return std::clamp(master_volume, 0.0, 1.0) *
           std::clamp(bgm_volume, 0.0, 1.0);
}

}  // namespace tenriff::app
