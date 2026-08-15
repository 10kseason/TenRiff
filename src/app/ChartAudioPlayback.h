#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tenriff::app {

inline double sanitize_chart_audio_playback_rate(double rate) {
    return (std::isfinite(rate) && rate > 0.01) ? rate : 1.0;
}

inline int64_t chart_audio_playback_duration_frames(int64_t source_frames, double playback_rate) {
    if (source_frames <= 0) {
        return 0;
    }

    const double safe_rate = sanitize_chart_audio_playback_rate(playback_rate);
    const double scaled_frames = static_cast<double>(source_frames) / safe_rate;
    return (std::max)(int64_t{1}, static_cast<int64_t>(std::ceil(scaled_frames)));
}

// The sound offset is deliberately separate from chart, judgement, and visual
// timing. Positive values delay scheduled BGM/autoplay audio; negative values
// advance it and let the mixer begin partway through a clip when needed.
inline int64_t chart_audio_start_sample_with_offset(int64_t chart_start_sample,
                                                    double sound_offset_ms,
                                                    int sample_rate) {
    if (sample_rate <= 0 || !std::isfinite(sound_offset_ms)) {
        return chart_start_sample;
    }
    const int64_t offset_samples = static_cast<int64_t>(std::llround(
        sound_offset_ms * static_cast<double>(sample_rate) / 1000.0));
    return chart_start_sample + offset_samples;
}

// Judgement keeps the original input timestamp, but a sound triggered by that
// input cannot be written into an audio buffer that has already been submitted.
// Pin only the audible trigger to the current buffer so short keysounds are not
// skipped in full when the playback head trails the write cursor.
inline int64_t pin_realtime_audio_start_sample(int64_t event_sample,
                                               int64_t buffer_start_sample) {
    return (std::max)(int64_t{0}, (std::max)(event_sample, buffer_start_sample));
}

inline int64_t mix_chart_audio_clip_linear(const std::vector<float>& source,
                                           int64_t voice_start_sample,
                                           double playback_rate,
                                           float gain,
                                           float* output,
                                           uint32_t frames,
                                           int64_t buffer_start_samples) {
    if (!output || frames == 0 || source.empty() || (source.size() % 2u) != 0u) {
        return 0;
    }

    const int64_t source_frames = static_cast<int64_t>(source.size() / 2u);
    const double safe_rate = sanitize_chart_audio_playback_rate(playback_rate);
    const int64_t playback_frames = chart_audio_playback_duration_frames(source_frames, safe_rate);
    if (playback_frames <= 0) {
        return 0;
    }

    const int64_t elapsed_output_frames = (std::max)(int64_t{0}, buffer_start_samples - voice_start_sample);
    if (elapsed_output_frames >= playback_frames) {
        return 0;
    }

    const int64_t output_start = (std::max)(int64_t{0}, voice_start_sample - buffer_start_samples);
    const int64_t output_capacity = static_cast<int64_t>(frames) - output_start;
    const int64_t remaining_output_frames = playback_frames - elapsed_output_frames;
    const int64_t mix_frames = (std::max)(int64_t{0}, (std::min)(output_capacity, remaining_output_frames));

    for (int64_t frame = 0; frame < mix_frames; ++frame) {
        const double source_pos = static_cast<double>(elapsed_output_frames + frame) * safe_rate;
        const std::size_t idx0 =
            (std::min)(static_cast<std::size_t>(source_frames - 1), static_cast<std::size_t>(source_pos));
        const std::size_t idx1 = (std::min)(static_cast<std::size_t>(source_frames - 1), idx0 + 1u);
        const double frac = std::clamp(source_pos - static_cast<double>(idx0), 0.0, 1.0);

        const float l0 = source[idx0 * 2u];
        const float r0 = source[idx0 * 2u + 1u];
        const float l1 = source[idx1 * 2u];
        const float r1 = source[idx1 * 2u + 1u];

        const std::size_t out_idx = static_cast<std::size_t>(output_start + frame) * 2u;
        output[out_idx] += static_cast<float>(l0 + (l1 - l0) * frac) * gain;
        output[out_idx + 1u] += static_cast<float>(r0 + (r1 - r0) * frac) * gain;
    }

    return mix_frames;
}

}  // namespace tenriff::app
