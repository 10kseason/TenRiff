#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tenriff::app {

// A small, offline-only preview request used by the internal BMS editor. The
// chart text is the editor's current buffer; source_chart_path is used only to
// resolve relative WAV assets and is never modified.
struct BmsEditorPreviewRequest {
    std::string chart_text;
    std::string source_chart_path;
    int cursor_measure = 0;
    double cursor_fraction = 0.0;
    int sample_rate = 48'000;
    int max_duration_seconds = 10;
    bool play_to_end = false;
    // Optional BMS WAV object id (for example "01") to audition at the cursor.
    std::string keysound_id;
    bool keysound_only = false;
};

struct BmsEditorPreviewResult {
    std::shared_ptr<const std::vector<float>> stereo_samples;
    int sample_rate = 0;
    int64_t cursor_sample = 0;
    int64_t chart_duration_samples = 0;
    std::string warning;
    std::string error;

    [[nodiscard]] bool success() const noexcept {
        return error.empty() && stereo_samples != nullptr;
    }
};

using BmsEditorPreviewCancelFlag = std::shared_ptr<std::atomic<bool>>;

[[nodiscard]] BmsEditorPreviewResult build_bms_editor_preview(
    const BmsEditorPreviewRequest& request,
    BmsEditorPreviewCancelFlag cancel_flag = {});

}  // namespace tenriff::app
