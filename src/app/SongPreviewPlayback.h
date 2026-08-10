#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tenriff::app {

[[nodiscard]] inline std::size_t mix_song_preview_clip_into_window(
    const std::vector<float>& stereo_clip,
    std::int64_t event_start_frame,
    std::int64_t window_start_frame,
    std::vector<float>& stereo_window) {
    if (stereo_clip.empty() || (stereo_clip.size() % 2u) != 0u ||
        stereo_window.empty() || (stereo_window.size() % 2u) != 0u) {
        return 0u;
    }
    const std::int64_t clip_frames =
        static_cast<std::int64_t>(stereo_clip.size() / 2u);
    const std::int64_t window_frames =
        static_cast<std::int64_t>(stereo_window.size() / 2u);
    std::int64_t destination_start = event_start_frame - window_start_frame;
    std::int64_t source_start = 0;
    if (destination_start < 0) {
        source_start = -destination_start;
        destination_start = 0;
    }
    if (source_start >= clip_frames || destination_start >= window_frames) {
        return 0u;
    }
    const std::int64_t frames = std::min(
        clip_frames - source_start, window_frames - destination_start);
    for (std::int64_t frame = 0; frame < frames; ++frame) {
        const std::size_t source =
            static_cast<std::size_t>(source_start + frame) * 2u;
        const std::size_t destination =
            static_cast<std::size_t>(destination_start + frame) * 2u;
        stereo_window[destination] += stereo_clip[source];
        stereo_window[destination + 1u] += stereo_clip[source + 1u];
    }
    return static_cast<std::size_t>(frames);
}

inline void mix_looping_song_preview(const std::vector<float>& stereo_samples,
                                     std::size_t& frame_cursor,
                                     float gain,
                                     float* output,
                                     std::uint32_t frames) {
    if (!output || frames == 0 || stereo_samples.empty() ||
        (stereo_samples.size() % 2u) != 0u) {
        return;
    }

    const std::size_t clip_frames = stereo_samples.size() / 2u;
    const float clamped_gain = std::clamp(gain, 0.0f, 1.0f);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const std::size_t source_frame = frame_cursor % clip_frames;
        const std::size_t source_index = source_frame * 2u;
        const std::size_t output_index = static_cast<std::size_t>(frame) * 2u;
        output[output_index] =
            std::clamp(stereo_samples[source_index] * clamped_gain, -1.0f, 1.0f);
        output[output_index + 1u] =
            std::clamp(stereo_samples[source_index + 1u] * clamped_gain, -1.0f, 1.0f);
        frame_cursor = (source_frame + 1u) % clip_frames;
    }
}

}  // namespace tenriff::app
