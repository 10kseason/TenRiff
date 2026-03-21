#pragma once

#include <cstdint>
#include <limits>

namespace tenriff::render {

inline int64_t advance_frame_deadline_ns(int64_t previous_deadline_ns,
                                         int64_t frame_interval_ns,
                                         int64_t frame_completed_ns) {
    if (frame_interval_ns <= 0) {
        return frame_completed_ns;
    }

    const int64_t next_deadline = previous_deadline_ns + frame_interval_ns;
    if (next_deadline > frame_completed_ns) {
        return next_deadline;
    }

    const int64_t late_ns = frame_completed_ns - next_deadline;
    const int64_t missed_frames = late_ns / frame_interval_ns + 1;
    if (missed_frames <= 0) {
        return next_deadline;
    }

    const int64_t max_additional =
        ((std::numeric_limits<int64_t>::max)() - next_deadline) / frame_interval_ns;
    if (missed_frames > max_additional) {
        return frame_completed_ns + frame_interval_ns;
    }
    return next_deadline + missed_frames * frame_interval_ns;
}

}  // namespace tenriff::render
