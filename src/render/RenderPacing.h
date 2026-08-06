#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace tenriff::render {

inline constexpr int64_t kRenderDefaultCoarseSleepMinNs = 500'000LL;
inline constexpr int64_t kRenderDefaultSpinGuardNs = 150'000LL;
inline constexpr int64_t kRenderDefaultYieldThresholdNs = 80'000LL;
inline constexpr int kRenderAdaptiveWaitThresholdFps = 300;

struct RenderWaitPolicy {
    int64_t coarse_sleep_min_ns = kRenderDefaultCoarseSleepMinNs;
    int64_t spin_guard_ns = kRenderDefaultSpinGuardNs;
    int64_t yield_threshold_ns = kRenderDefaultYieldThresholdNs;
};

inline bool should_use_unlimited_render_pacing(bool vsync_enabled, int target_fps) {
    return !vsync_enabled && target_fps <= 0;
}

inline RenderWaitPolicy render_wait_policy(bool vsync_enabled, int target_fps) {
    RenderWaitPolicy policy;
    if (vsync_enabled || target_fps <= kRenderAdaptiveWaitThresholdFps) {
        return policy;
    }

    const int64_t frame_interval_ns = 1'000'000'000LL / static_cast<int64_t>(target_fps);
    policy.coarse_sleep_min_ns = std::clamp(frame_interval_ns / 4, int64_t{100'000}, int64_t{250'000});
    policy.spin_guard_ns = std::clamp(frame_interval_ns / 32, int64_t{20'000}, int64_t{50'000});
    policy.yield_threshold_ns = std::clamp(frame_interval_ns / 64, int64_t{10'000}, int64_t{25'000});
    if (policy.yield_threshold_ns > policy.spin_guard_ns) {
        policy.yield_threshold_ns = policy.spin_guard_ns;
    }
    return policy;
}

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
