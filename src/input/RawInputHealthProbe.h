#pragma once

#include <cstdint>
#include <iostream>

#include "input/InputThread.h"

namespace tenriff::input {

class RawInputHealthProbe {
public:
    explicit RawInputHealthProbe(int64_t grace_ns = 120'000'000LL) : grace_ns_(grace_ns) {}

    void reset() {
        pending_ = false;
        pending_since_ns_ = 0;
        baseline_allowed_event_count_ = 0;
        baseline_queue_push_count_ = 0;
    }

    void note_polled_change(int64_t now_ns, const InputThreadHealthSnapshot& snapshot) {
        if (snapshot.backend != InputBackend::RawInput) {
            reset();
            return;
        }
        const int64_t last_raw_event_ns =
            snapshot.last_allowed_event_time_ns > snapshot.last_queue_push_time_ns
                ? snapshot.last_allowed_event_time_ns
                : snapshot.last_queue_push_time_ns;
        // The menu loop can observe the physical key change just after it has
        // already drained the matching RawInput event. Do not arm a false
        // fallback when delivery was recent enough to explain the change.
        if (last_raw_event_ns > 0 && now_ns >= last_raw_event_ns &&
            now_ns - last_raw_event_ns <= grace_ns_) {
            reset();
            return;
        }
        if (pending_) {
            return;
        }
        pending_ = true;
        pending_since_ns_ = now_ns;
        baseline_allowed_event_count_ = snapshot.allowed_event_count;
        baseline_queue_push_count_ = snapshot.queue_push_count;
    }

    [[nodiscard]] bool should_trigger_fallback(int64_t now_ns, const InputThreadHealthSnapshot& snapshot) {
        if (snapshot.backend != InputBackend::RawInput) {
            reset();
            return false;
        }
        if (!pending_) {
            return false;
        }
        if (snapshot.allowed_event_count > baseline_allowed_event_count_ ||
            snapshot.queue_push_count > baseline_queue_push_count_ ||
            snapshot.last_allowed_event_time_ns >= pending_since_ns_ ||
            snapshot.last_queue_push_time_ns >= pending_since_ns_) {
            reset();
            return false;
        }
        if (now_ns - pending_since_ns_ < grace_ns_) {
            return false;
        }
        std::cerr << "[warn] RawInput inactive for bound keys, switching to Polling" << std::endl;
        reset();
        return true;
    }

    [[nodiscard]] bool pending() const { return pending_; }

private:
    int64_t grace_ns_ = 120'000'000LL;
    bool pending_ = false;
    int64_t pending_since_ns_ = 0;
    uint64_t baseline_allowed_event_count_ = 0;
    uint64_t baseline_queue_push_count_ = 0;
};

}  // namespace tenriff::input
