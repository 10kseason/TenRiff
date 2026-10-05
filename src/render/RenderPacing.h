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

inline int64_t advance_frame_deadline_ns(int64_t frame_started_ns,
                                         int64_t frame_interval_ns,
                                         int64_t frame_completed_ns) {
    if (frame_interval_ns <= 0) {
        return frame_completed_ns;
    }

    // Limit start-to-start cadence, not slots on an old time grid. A render
    // that barely exceeds its budget must not wait almost another full frame.
    // Rebasing on the actual start also prevents catch-up bursts after a late wake.
    const int64_t max_time = (std::numeric_limits<int64_t>::max)();
    const int64_t next_deadline = frame_started_ns > max_time - frame_interval_ns
        ? max_time : frame_started_ns + frame_interval_ns;
    return (std::max)(next_deadline, frame_completed_ns);
}

}  // namespace tenriff::render
