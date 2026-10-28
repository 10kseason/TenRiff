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

struct RenderOversleepEstimate {
    static constexpr int64_t kMaximumNs = 2'000'000;
    static constexpr int64_t kRecoveryIntervalNs = 100'000'000;
    int64_t value_ns = 0;
    int64_t last_update_ns = 0;

    void observe(int64_t target_wake_ns, int64_t actual_wake_ns) {
        const int64_t overshoot = std::clamp(actual_wake_ns - target_wake_ns,
                                            int64_t{0}, kMaximumNs);
        value_ns = std::clamp((value_ns * 7 + overshoot) / 8, int64_t{0}, kMaximumNs);
        last_update_ns = actual_wake_ns;
    }

    void recover_after_wait(int64_t now_ns, bool estimate_blocked_sleep, bool slept) {
        // A stale estimate can exclude all later timer sleeps, preventing new
        // EMA samples forever. Reconsider it only after a sample-free interval.
        // Per-frame decay would retry bad timers too often under sustained load.
        // Short waits and waits with a real sleep sample retain their behavior.
        if (estimate_blocked_sleep && !slept &&
            now_ns - last_update_ns >= kRecoveryIntervalNs) {
            value_ns = value_ns * 7 / 8;
            last_update_ns = now_ns;
        }
    }
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
