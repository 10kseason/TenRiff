#include "app/SongSelectScreen.h"

#include <chrono>
#include <exception>
#include <utility>

#include "app/SongPreviewBuilder.h"

namespace tenriff::app {

SongSelectScreen::~SongSelectScreen() {
    shutdown();
}

void SongSelectScreen::set_active(bool active) {
    if (active_ == active) {
        return;
    }
    active_ = active;
    if (!active_) {
        clear_preview_target();
    }
}

void SongSelectScreen::set_preview_target(std::string selection_key, std::int64_t due_ns) {
    cancel_preview_decode();
    preview_selection_key_ = std::move(selection_key);
    preview_due_ns_ = due_ns;
    preview_pending_ = !preview_selection_key_.empty();
}

void SongSelectScreen::clear_preview_target() {
    cancel_preview_decode();
    preview_selection_key_.clear();
    preview_due_ns_ = 0;
    preview_pending_ = false;
}

void SongSelectScreen::begin_preview_decode(const std::string& selection_key,
                                            const std::string& chart_path,
                                            const std::string& indexed_preview_path,
                                            int target_sample_rate) {
    if (!active_ || selection_key.empty() || chart_path.empty() ||
        target_sample_rate <= 0 || preview_decode_future_.valid()) {
        return;
    }

    preview_pending_ = false;
    auto cancel_flag = std::make_shared<std::atomic<bool>>(false);
    preview_decode_cancel_ = cancel_flag;
    preview_decode_future_ = std::async(
        std::launch::async,
        [selection_key,
         chart_path,
         indexed_preview_path,
         target_sample_rate,
         cancel_flag]() {
            PreviewDecodeResult result;
            result.selection_key = selection_key;
            result.sample_rate = target_sample_rate;

            std::vector<float> samples;
            std::string preview_source;
            std::string preview_error;
            if (!build_song_preview_audio(chart_path,
                                          indexed_preview_path,
                                          target_sample_rate,
                                          30,
                                          samples,
                                          preview_source,
                                          &preview_error,
                                          cancel_flag) ||
                samples.empty()) {
                result.error = std::move(preview_error);
                return result;
            }

            result.path = std::move(preview_source);
            result.samples = std::make_shared<const std::vector<float>>(std::move(samples));
            return result;
        });
}

std::optional<SongSelectScreen::PreviewDecodeResult> SongSelectScreen::take_ready_preview_decode() {
    if (!preview_decode_future_.valid() ||
        preview_decode_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return std::nullopt;
    }

    PreviewDecodeResult decoded;
    try {
        decoded = preview_decode_future_.get();
    } catch (const std::exception& ex) {
        decoded.error = ex.what();
    } catch (...) {
        decoded.error = "Unknown preview decode error.";
    }
    preview_decode_cancel_.reset();
    return decoded;
}

void SongSelectScreen::cancel_preview_decode() {
    if (preview_decode_cancel_) {
        preview_decode_cancel_->store(true, std::memory_order_release);
    }
}

void SongSelectScreen::shutdown() {
    active_ = false;
    clear_preview_target();
    preview_gain_.store(0.0f, std::memory_order_release);
    preview_active_path_.clear();

    // std::async futures can otherwise keep their worker alive until the
    // enclosing MenuApp is destroyed. Cancellation makes this bounded, and
    // waiting here guarantees the worker never outlives its screen owner.
    if (preview_decode_future_.valid()) {
        preview_decode_future_.wait();
        try {
            static_cast<void>(preview_decode_future_.get());
        } catch (...) {
        }
    }
    preview_decode_cancel_.reset();
}

}  // namespace tenriff::app
