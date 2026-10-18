#pragma once

#include <algorithm>
#include <cmath>

namespace tenriff::app {

[[nodiscard]] inline float gameplay_volume_gain(double volume) {
    return std::isfinite(volume) ? static_cast<float>(std::clamp(volume, 0.0, 1.0)) : 0.0f;
}

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

// Normalize OFF is a linear signal path while the final output fits the device
// range. Apply master before the range guard so overlapping keysounds can use
// its headroom without being compressed at the old pre-master 0.92 threshold.
[[nodiscard]] inline float apply_clean_master_volume_to_sample(float mixed_sample,
                                                               double master_volume) {
    if (!std::isfinite(mixed_sample) || !std::isfinite(master_volume)) {
        return 0.0f;
    }
    const float master = static_cast<float>(std::clamp(master_volume, 0.0, 1.0));
    return std::clamp(mixed_sample * master, -1.0f, 1.0f);
}

// Focus muting is an output gate, never a change to the saved master volume.
// Audio/chart clocks continue while silent and regain the same gain on return.
[[nodiscard]] inline double focused_audio_master_gain(double master_volume,
                                                       bool mute_when_inactive,
                                                       bool foreground) {
    return mute_when_inactive && !foreground ? 0.0 : std::clamp(master_volume, 0.0, 1.0);
}

// Both sliders have a 0..100% range. Clamp them independently so master remains
// responsive over its full range, including when loading older profile values.
[[nodiscard]] inline double menu_background_gain(bool enabled,
                                                 double master_volume,
                                                 double music_volume) {
    if (!enabled) {
        return 0.0;
    }
    return std::clamp(master_volume, 0.0, 1.0) *
           std::clamp(music_volume, 0.0, 1.0);
}

// Song previews have a separate reference gain; gameplay/editor mixes stay unchanged.
[[nodiscard]] inline double song_preview_gain(bool enabled, double master_volume,
                                               double performance_volume) {
    return menu_background_gain(enabled, master_volume, performance_volume) * 0.75;
}

// Only TenRiff's own scene music gets the quieter reference level.
[[nodiscard]] inline double tenriff_music_gain(bool enabled,
                                               double master_volume,
                                               double menu_music_volume) {
    // 1.9.7 eases the scene-music attenuation from 10% to 12.5% (~+1.94 dB).
    // Stored master/menu sliders and the separate song-preview gain stay intact.
    return menu_background_gain(enabled, master_volume, menu_music_volume) * 0.125;
}

}  // namespace tenriff::app
