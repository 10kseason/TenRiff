#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include "gameplay/GameplayChart.h"
#include "input/SPSCQueue.h"
#include "key_note_overlay/pattern_difficulty.h"

namespace tenriff::app {

inline constexpr bool key_overlay_supports(int lanes) {
    return lanes == 4 || lanes == 5 || lanes == 7 || lanes == 8 || lanes == 10;
}

struct KeyOverlayNote {
    int lane = 0;  // Zero based, remapped to visual order by MenuApp.
    double press_ms = 0.0;
    double release_ms = -1.0;
};

struct KeyOverlaySnapshot {
    static constexpr std::size_t MaxNotes = 256;
    static constexpr double TravelMs = 850.0;
    bool visible = false;
    bool live = false;
    bool preview = false;
    double now_ms = 0.0;
    std::array<bool, 10> held{};
    std::array<KeyOverlayNote, MaxNotes> notes{};
    std::size_t note_count = 0;
    double kps = 0.0;
    bool limited = false;
    std::uint64_t accepted_presses = 0;
    bool bms_chart = false;
    // Immutable, loading-time chart estimate; never derived from live presses.
    pattern::Snapshot difficulty;
};

struct BmsOverlayEstimate {
    bool bms_chart = false;
    bool cancelled = false;
    pattern::Snapshot difficulty;
    double peak_end_ms = 0.0;
};

// The already parsed, mode/rate-adjusted BMS is scanned before practice trimming
// and countdown lead-in. No media, keyboard activity, or judgement results enter it.
BmsOverlayEstimate estimate_bms_overlay_difficulty(const gameplay::GameplayChart& chart,
    int sample_rate, const std::function<bool()>& cancelled = {});

// One accepted-input producer, one optional low-priority overlay consumer.
// record() only copies a bounded event: no allocation, mutex, OS wake or analysis.
// Configure/stop only while the producer is stopped; update() is worker/test-only.
class KeyNoteOverlay {
public:
    ~KeyNoteOverlay();
    void configure(bool enabled, int lanes);
    bool start();
    void stop();
    KeyOverlaySnapshot snapshot(int64_t now_ns, bool live);
    void record(int lane, bool down, int64_t time_ns) noexcept;
    void invalidate() noexcept;
    KeyOverlaySnapshot update(int64_t now_ns, bool live);

private:
    struct Event { int lane; bool down; int64_t time_ns; };
    void clear(double now_ms);
    bool enabled_ = false;
    int lanes_ = 0;
    input::SPSCQueue<Event, 2048> queue_;
    std::atomic<bool> invalid_{false};
    std::array<double, 4096> presses_{};
    std::size_t press_head_ = 0, press_count_ = 0;
    KeyOverlaySnapshot state_;
    double reset_ms_ = -1.0;
    double last_event_ms_ = -1.0;
    double chord_start_ms_ = -1.0;
    double chord_visual_ms_ = -1.0;
    std::thread worker_;
    std::atomic<bool> stopping_{true};
    std::atomic<bool> live_{false};
    std::mutex published_mutex_;
    KeyOverlaySnapshot published_, hud_copy_;
};

}  // namespace tenriff::app
