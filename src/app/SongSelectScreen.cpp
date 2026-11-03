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
                                            int target_sample_rate,
                                            bool title_music) {
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
         title_music,
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
                                          title_music ? 300 : 30,
                                          samples,
                                          preview_source,
                                          &preview_error,
                                          cancel_flag,
                                          title_music) ||
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

bool SongSelectScreen::service_nps_profile(std::string selection_key,
                                           std::string chart_path, int64_t now_ns) {
    bool changed = false;
    if (selection_key != nps_selection_key_) {
        if (nps_cancel_) nps_cancel_->store(true, std::memory_order_release);
        nps_selection_key_ = std::move(selection_key);
        nps_chart_path_ = std::move(chart_path);
        nps_due_ns_ = now_ns + 150'000'000LL;
        nps_pending_ = !nps_selection_key_.empty() && nps_selection_key_ != nps_cached_key_;
        changed = true;
    }
    if (nps_future_.valid() && nps_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        NpsDecodeResult decoded;
        try { decoded = nps_future_.get(); } catch (...) {}
        const bool cancelled = nps_cancel_ && nps_cancel_->load(std::memory_order_acquire);
        nps_cancel_.reset();
        if (!cancelled && !decoded.selection_key.empty() && decoded.selection_key == nps_selection_key_) {
            nps_cached_key_ = std::move(decoded.selection_key);
            nps_cached_profile_ = std::move(decoded.profile);
            nps_pending_ = false;
            changed = true;
        }
    }
    if (nps_pending_ && !nps_future_.valid() && now_ns >= nps_due_ns_) {
        const auto key = nps_selection_key_;
        const auto path = nps_chart_path_;
        auto cancelled = std::make_shared<std::atomic<bool>>(false);
        nps_cancel_ = cancelled;
        nps_pending_ = false;
        try {
            nps_future_ = std::async(std::launch::async, [key, path, cancelled]() {
                NpsDecodeResult result;
                result.selection_key = key;
                try {
                    result.profile = load_song_nps_profile(path, [cancelled]() {
                        return cancelled->load(std::memory_order_acquire);
                    });
                } catch (...) {
                    // A bad preview must not affect selection or gameplay.
                }
                return result;
            });
        } catch (...) {
            nps_cached_key_ = key;
            nps_cached_profile_ = {};
            nps_cancel_.reset();
            changed = true;
        }
    }
    return changed;
}

const SongNpsProfile* SongSelectScreen::nps_profile_for(std::string_view selection_key) const {
    return !selection_key.empty() && selection_key == nps_selection_key_ && selection_key == nps_cached_key_
        ? &nps_cached_profile_ : nullptr;
}

void SongSelectScreen::shutdown() {
    active_ = false;
    if (nps_cancel_) nps_cancel_->store(true, std::memory_order_release);
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
    if (nps_future_.valid()) {
        nps_future_.wait();
        try { static_cast<void>(nps_future_.get()); } catch (...) {}
    }
    nps_cancel_.reset();
    nps_selection_key_.clear();
    nps_chart_path_.clear();
    nps_cached_key_.clear();
    nps_cached_profile_ = {};
    nps_pending_ = false;
}

}  // namespace tenriff::app
