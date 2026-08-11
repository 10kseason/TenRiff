#pragma once

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tenriff::app {

// First ownership boundary extracted from MenuApp. SongSelectScreen owns the
// asynchronous preview decode and its cancellation token; MenuApp still owns
// the actual audio device and rendering while the rest of the screen is moved
// incrementally.
class SongSelectScreen {
public:
    struct PreviewDecodeResult {
        std::string selection_key;
        std::string path;
        std::string error;
        std::shared_ptr<const std::vector<float>> samples;
        int sample_rate = 0;
    };

    SongSelectScreen() = default;
    ~SongSelectScreen();

    SongSelectScreen(const SongSelectScreen&) = delete;
    SongSelectScreen& operator=(const SongSelectScreen&) = delete;

    void set_active(bool active);
    [[nodiscard]] bool active() const { return active_; }

    void set_preview_target(std::string selection_key, std::int64_t due_ns);
    void clear_preview_target();
    [[nodiscard]] const std::string& preview_selection_key() const { return preview_selection_key_; }
    [[nodiscard]] std::int64_t preview_due_ns() const { return preview_due_ns_; }
    [[nodiscard]] bool preview_pending() const { return preview_pending_; }
    void set_preview_pending(bool pending) { preview_pending_ = pending; }

    [[nodiscard]] bool preview_decode_in_flight() const { return preview_decode_future_.valid(); }
    void begin_preview_decode(const std::string& selection_key,
                              const std::string& chart_path,
                              const std::string& indexed_preview_path,
                              int target_sample_rate);
    [[nodiscard]] std::optional<PreviewDecodeResult> take_ready_preview_decode();
    void cancel_preview_decode();
    void shutdown();

    [[nodiscard]] const std::string& preview_active_path() const { return preview_active_path_; }
    void set_preview_active_path(std::string path) { preview_active_path_ = std::move(path); }
    void clear_preview_active_path() { preview_active_path_.clear(); }
    [[nodiscard]] std::atomic<float>& preview_gain() { return preview_gain_; }

private:
    bool active_ = false;
    std::string preview_selection_key_{};
    std::string preview_active_path_{};
    std::int64_t preview_due_ns_ = 0;
    bool preview_pending_ = false;
    std::future<PreviewDecodeResult> preview_decode_future_{};
    std::shared_ptr<std::atomic<bool>> preview_decode_cancel_{};
    std::atomic<float> preview_gain_{0.0f};
};

}  // namespace tenriff::app
