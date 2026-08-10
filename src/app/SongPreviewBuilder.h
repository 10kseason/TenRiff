#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace tenriff::app {

using SongPreviewCancelFlag = std::shared_ptr<std::atomic<bool>>;

// Prefer a chart-declared preview file. When one is not present, render a
// bounded autoplay mix from the chart's BGM and keysound events so fragmented
// BMS songs still have a real musical preview. The indexed/local file is kept
// as a final compatibility fallback.
[[nodiscard]] bool build_song_preview_audio(
    const std::string& chart_path,
    const std::string& indexed_preview_path,
    int target_sample_rate,
    int max_duration_seconds,
    std::vector<float>& out_stereo_samples,
    std::string& out_source,
    std::string* error = nullptr,
    SongPreviewCancelFlag cancel_flag = {});

}  // namespace tenriff::app
