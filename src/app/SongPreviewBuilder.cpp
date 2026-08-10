#include "app/SongPreviewBuilder.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include "app/AudioFileDecoder.h"
#include "app/AudioMixPolicy.h"
#include "app/ChartLoader.h"
#include "app/MenuSongUtils.h"
#include "app/SongPreviewPlayback.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

bool preview_cancelled(const SongPreviewCancelFlag& cancel_flag) {
    return cancel_flag && cancel_flag->load(std::memory_order_acquire);
}

bool stop_if_preview_cancelled(const SongPreviewCancelFlag& cancel_flag,
                               std::vector<float>& out,
                               std::string* error) {
    if (!preview_cancelled(cancel_flag)) {
        return false;
    }
    out.clear();
    if (error) {
        *error = "cancelled";
    }
    return true;
}

bool decode_preview_file(const std::string& path,
                         int sample_rate,
                         int max_duration_seconds,
                         std::vector<float>& out,
                         std::string* error,
                         const SongPreviewCancelFlag& cancel_flag) {
    if (stop_if_preview_cancelled(cancel_flag, out, error)) {
        return false;
    }
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(util::path_from_utf8_lossy(path), ec) || ec) {
        if (error) {
            *error = "Preview file does not exist: " + path;
        }
        return false;
    }
    const int safe_seconds = std::clamp(max_duration_seconds, 5, 60);
    const std::size_t max_frames =
        static_cast<std::size_t>(sample_rate) * static_cast<std::size_t>(safe_seconds);
    const bool decoded = decode_audio_file_stereo_resampled(
        path, sample_rate, out, error, max_frames);
    if (stop_if_preview_cancelled(cancel_flag, out, error)) {
        return false;
    }
    return decoded && !out.empty();
}

bool build_chart_event_mix(const std::string& chart_path,
                           int sample_rate,
                           int max_duration_seconds,
                           std::vector<float>& out,
                           std::string* error,
                           const SongPreviewCancelFlag& cancel_flag) {
    if (stop_if_preview_cancelled(cancel_flag, out, error)) {
        return false;
    }
    ChartLoader loader;
    ChartLoadResult loaded = loader.load(chart_path, sample_rate, 1.0, "autoplay");
    if (stop_if_preview_cancelled(cancel_flag, out, error)) {
        return false;
    }
    if (!loaded.success() || loaded.chart.audio_cues.empty()) {
        if (error) {
            *error = loaded.success() ? "Chart has no previewable audio events."
                                      : loaded.error;
        }
        return false;
    }

    int64_t window_start = std::numeric_limits<int64_t>::max();
    for (const auto& cue : loaded.chart.audio_cues) {
        window_start = std::min(window_start, std::max<int64_t>(0, cue.start_sample));
    }
    if (window_start == std::numeric_limits<int64_t>::max()) {
        return false;
    }

    const int safe_seconds = std::clamp(max_duration_seconds, 5, 60);
    const int64_t max_frames = static_cast<int64_t>(sample_rate) * safe_seconds;
    const int64_t chart_remaining = loaded.chart.duration_samples > window_start
                                        ? loaded.chart.duration_samples - window_start
                                        : max_frames;
    const int64_t window_frames = std::max<int64_t>(
        1, std::min(max_frames, chart_remaining + static_cast<int64_t>(sample_rate)));
    out.assign(static_cast<std::size_t>(window_frames) * 2u, 0.0f);

    std::vector<std::vector<int64_t>> starts_by_asset(loaded.chart.audio_assets.size());
    const int64_t window_end = window_start + window_frames;
    for (const auto& cue : loaded.chart.audio_cues) {
        if (cue.asset_id >= starts_by_asset.size() || cue.start_sample >= window_end) {
            continue;
        }
        starts_by_asset[cue.asset_id].push_back(cue.start_sample);
    }

    std::size_t mixed_event_count = 0;
    std::size_t failed_asset_count = 0;
    for (std::size_t asset_id = 0; asset_id < starts_by_asset.size(); ++asset_id) {
        if (stop_if_preview_cancelled(cancel_flag, out, error)) {
            return false;
        }
        if (starts_by_asset[asset_id].empty()) {
            continue;
        }
        const std::string* path = loaded.chart.audio_asset_path(asset_id);
        if (!path || path->empty()) {
            ++failed_asset_count;
            continue;
        }
        std::vector<float> clip;
        std::string decode_error;
        const bool decoded = decode_audio_file_stereo_resampled(
            *path,
            sample_rate,
            clip,
            &decode_error,
            static_cast<std::size_t>(window_frames));
        if (stop_if_preview_cancelled(cancel_flag, out, error)) {
            return false;
        }
        if (!decoded || clip.empty()) {
            ++failed_asset_count;
            continue;
        }
        for (const int64_t start : starts_by_asset[asset_id]) {
            if (mix_song_preview_clip_into_window(
                    clip, start, window_start, out) > 0u) {
                ++mixed_event_count;
            }
        }
    }

    if (mixed_event_count == 0u) {
        out.clear();
        if (error) {
            *error = "No chart audio assets could be decoded.";
        }
        return false;
    }
    if (stop_if_preview_cancelled(cancel_flag, out, error)) {
        return false;
    }
    for (float& sample : out) {
        sample = soft_limit_audio_sample(sample);
    }
    if (stop_if_preview_cancelled(cancel_flag, out, error)) {
        return false;
    }
    if (error && failed_asset_count > 0u) {
        *error = "Preview mixed with " + std::to_string(failed_asset_count) +
                 " unavailable audio asset(s).";
    }
    return true;
}

}  // namespace

bool build_song_preview_audio(const std::string& chart_path,
                              const std::string& indexed_preview_path,
                              int target_sample_rate,
                              int max_duration_seconds,
                               std::vector<float>& out_stereo_samples,
                               std::string& out_source,
                               std::string* error,
                               SongPreviewCancelFlag cancel_flag) {
    out_stereo_samples.clear();
    out_source.clear();
    if (stop_if_preview_cancelled(cancel_flag, out_stereo_samples, error)) {
        return false;
    }
    if (chart_path.empty() || target_sample_rate < 8'000) {
        if (error) {
            *error = "Invalid chart path or preview sample rate.";
        }
        return false;
    }

    const std::string declared =
        menu_songs::resolve_song_declared_audio_preview_path(chart_path);
    if (stop_if_preview_cancelled(cancel_flag, out_stereo_samples, error)) {
        return false;
    }
    std::string declared_error;
    if (decode_preview_file(
            declared,
            target_sample_rate,
             max_duration_seconds,
             out_stereo_samples,
             &declared_error,
             cancel_flag)) {
        if (stop_if_preview_cancelled(cancel_flag, out_stereo_samples, error)) {
            return false;
        }
        out_source = declared;
        return true;
    }
    if (stop_if_preview_cancelled(cancel_flag, out_stereo_samples, error)) {
        return false;
    }

    std::string mix_warning;
    if (build_chart_event_mix(chart_path,
                              target_sample_rate,
                               max_duration_seconds,
                               out_stereo_samples,
                               &mix_warning,
                               cancel_flag)) {
        if (stop_if_preview_cancelled(cancel_flag, out_stereo_samples, error)) {
            return false;
        }
        out_source = chart_path + "#autoplay-preview";
        return true;
    }
    if (stop_if_preview_cancelled(cancel_flag, out_stereo_samples, error)) {
        return false;
    }

    std::string fallback = indexed_preview_path;
    if (fallback.empty()) {
        fallback = menu_songs::resolve_song_audio_preview_path(chart_path);
    }
    if (stop_if_preview_cancelled(cancel_flag, out_stereo_samples, error)) {
        return false;
    }
    std::string fallback_error;
    if (fallback != declared &&
        decode_preview_file(
            fallback,
            target_sample_rate,
             max_duration_seconds,
             out_stereo_samples,
             &fallback_error,
             cancel_flag)) {
        if (stop_if_preview_cancelled(cancel_flag, out_stereo_samples, error)) {
            return false;
        }
        out_source = fallback;
        return true;
    }
    if (stop_if_preview_cancelled(cancel_flag, out_stereo_samples, error)) {
        return false;
    }

    if (error) {
        *error = "Declared preview: " +
                 (declared_error.empty() ? std::string("not provided") : declared_error) +
                 " | chart mix: " +
                 (mix_warning.empty() ? std::string("unavailable") : mix_warning) +
                 " | fallback: " +
                 (fallback_error.empty() ? std::string("unavailable") : fallback_error);
    }
    return false;
}

}  // namespace tenriff::app
