#include "app/KeyNoteOverlay.h"
#include <algorithm>
#include <chrono>
#include "timing/HighResClock.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace tenriff::app {

KeyNoteOverlay::~KeyNoteOverlay() { stop(); }

void KeyNoteOverlay::configure(bool enabled, int lanes) {
    stop();
    enabled_ = enabled && key_overlay_supports(lanes);
    lanes_ = enabled_ ? lanes : 0;
    queue_.reset();
    invalid_.store(false, std::memory_order_relaxed);
    clear(-1.0);
    published_ = hud_copy_ = state_;
}

void KeyNoteOverlay::clear(double now_ms) {
    press_head_ = press_count_ = 0;
    state_ = {};
    state_.visible = enabled_;
    reset_ms_ = now_ms;
    last_event_ms_ = chord_start_ms_ = chord_visual_ms_ = -1.0;
}

bool KeyNoteOverlay::start() {
    if (!enabled_ || worker_.joinable()) return true;
    stopping_.store(false, std::memory_order_release);
    try {
        worker_ = std::thread([this] {
#ifdef _WIN32
            // This optional UI worker must not compete with MMCSS input/audio.
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
            while (!stopping_.load(std::memory_order_acquire)) {
                const auto value = update(timing::HighResClock::now_ns(), live_.load(std::memory_order_acquire));
                {
                    std::unique_lock<std::mutex> lock(published_mutex_, std::try_to_lock);
                    if (lock.owns_lock()) published_ = value;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
        });
    } catch (...) {
        stopping_.store(true, std::memory_order_release);
        enabled_ = false;
        return false;
    }
    return true;
}

void KeyNoteOverlay::stop() {
    stopping_.store(true, std::memory_order_release);
    if (worker_.joinable()) worker_.join();
    live_.store(false, std::memory_order_release);
}

KeyOverlaySnapshot KeyNoteOverlay::snapshot(int64_t now_ns, bool live) {
    if (!enabled_) return {};
    if (live_.exchange(live, std::memory_order_acq_rel) != live && !live) invalidate();
    {
        // A busy publication is optional. Reuse the last UI copy rather than
        // waiting on a lock in the session/HUD loop.
        std::unique_lock<std::mutex> lock(published_mutex_, std::try_to_lock);
        if (lock.owns_lock()) hud_copy_ = published_;
    }
    auto result = hud_copy_;
    const double now = static_cast<double>(std::max<int64_t>(0, now_ns)) / 1e6;
    if (!live || now - result.now_ms > 100.0) {
        result = {};
        result.visible = true;
        result.limited = live;
    }
    result.now_ms = now;
    result.live = live && !result.limited;
    return result;
}

void KeyNoteOverlay::record(int lane, bool down, int64_t time_ns) noexcept {
    if (!enabled_ || lane < 0 || lane >= lanes_ || time_ns < 0) return;
    if (!queue_.push(Event{lane, down, time_ns})) invalidate();
}

void KeyNoteOverlay::invalidate() noexcept {
    if (enabled_) invalid_.store(true, std::memory_order_release);
}

KeyOverlaySnapshot KeyNoteOverlay::update(int64_t now_ns, bool live) {
    if (!enabled_) return {};
    double now = static_cast<double>(std::max<int64_t>(0, now_ns)) / 1e6;
    // A lost edge or focus/pause transition invalidates the whole sample window:
    // inventing missing releases would leave stuck holds or misleading KPS.
    const bool invalid = invalid_.exchange(false, std::memory_order_acq_rel);
    if (!live || invalid || now < state_.now_ms) {
        for (std::size_t i = 0; i < queue_.capacity() && queue_.pop(); ++i) {}
        clear(now);
        state_.now_ms = now;
        state_.limited = invalid;
        return state_;
    }
    state_.live = true;
    state_.limited = false;
    state_.now_ms = now;
    for (std::size_t i = 0; i < queue_.capacity(); ++i) {
        const auto event = queue_.pop();
        if (!event) break;
        const double time = static_cast<double>(event->time_ns) / 1e6;
        if (time <= reset_ms_ || time < last_event_ms_) continue;
        // The producer may publish after this HUD frame sampled its clock.
        // Preserve that edge instead of dropping short taps at the frame boundary.
        now = std::max(now, time);
        last_event_ms_ = time;
        const auto lane = static_cast<std::size_t>(event->lane);
        if (event->down) {
            // Secondary-key takeover is another scoring Pressed edge in TenRiff,
            // but a continuously held logical lane is still one overlay hold.
            if (state_.held[lane]) continue;
            state_.held[lane] = true;
            while (press_count_ && presses_[press_head_] <= time - 1000.0) {
                press_head_ = (press_head_ + 1) % presses_.size(); --press_count_;
            }
            if (press_count_ == presses_.size()) {
                invalidate();
                state_.limited = true;
                break;
            }
            presses_[(press_head_ + press_count_++) % presses_.size()] = time;
            ++state_.accepted_presses;
            if (chord_start_ms_ < 0 || time - chord_start_ms_ > 15.0) {
                chord_start_ms_ = chord_visual_ms_ = time;
            }
            if (state_.note_count == state_.notes.size()) {
                const auto oldest_released = std::find_if(state_.notes.begin(), state_.notes.end(),
                    [](const KeyOverlayNote& note) { return note.release_ms >= 0; });
                // At most ten notes are held, so a released entry always exists.
                std::move(oldest_released + 1, state_.notes.end(), oldest_released);
                --state_.note_count;
            }
            state_.notes[state_.note_count++] = {event->lane, chord_visual_ms_, -1.0};
        } else if (state_.held[lane]) {
            state_.held[lane] = false;
            for (std::size_t n = state_.note_count; n > 0; --n) {
                auto& note = state_.notes[n - 1];
                if (note.lane == event->lane && note.release_ms < 0) {
                    note.release_ms = time;
                    break;
                }
            }
        }
    }
    state_.now_ms = now;
    const auto end = std::remove_if(state_.notes.begin(), state_.notes.begin() + state_.note_count,
        [now](const KeyOverlayNote& note) {
            return note.release_ms >= 0 && now - note.release_ms > KeyOverlaySnapshot::TravelMs;
        });
    state_.note_count = static_cast<std::size_t>(end - state_.notes.begin());
    while (press_count_ && presses_[press_head_] <= now - 1000.0) {
        press_head_ = (press_head_ + 1) % presses_.size(); --press_count_;
    }
    state_.kps = static_cast<double>(press_count_);
    return state_;
}

}  // namespace tenriff::app
