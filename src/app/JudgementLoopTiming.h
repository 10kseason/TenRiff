#pragma once

#include <algorithm>
#include <cstdint>

namespace tenriff::app {

inline constexpr int kJudgementLoopMinHz = 1000;
inline constexpr int kJudgementLoopMaxHz = 8000;

struct JudgementLoopTimingPlan {
    int target_hz = kJudgementLoopMinHz;
    int64_t base_step_samples = 1;
    int64_t remainder_samples = 0;
};

[[nodiscard]] inline JudgementLoopTimingPlan build_judgement_loop_timing_plan(int sample_rate, int polling_hz) {
    const int safe_sample_rate = std::max(1, sample_rate);
    int target_hz = std::clamp(polling_hz, kJudgementLoopMinHz, kJudgementLoopMaxHz);
    target_hz = std::min(target_hz, safe_sample_rate);

    const int64_t base_step_samples =
        std::max<int64_t>(1, static_cast<int64_t>(safe_sample_rate) / static_cast<int64_t>(target_hz));
    const int64_t remainder_samples =
        std::max<int64_t>(0,
                          static_cast<int64_t>(safe_sample_rate) -
                              base_step_samples * static_cast<int64_t>(target_hz));

    return JudgementLoopTimingPlan{target_hz, base_step_samples, remainder_samples};
}

[[nodiscard]] inline int64_t next_judgement_loop_step_samples(const JudgementLoopTimingPlan& plan,
                                                              int64_t& remainder_carry) {
    int64_t step_samples = std::max<int64_t>(1, plan.base_step_samples);
    remainder_carry += std::max<int64_t>(0, plan.remainder_samples);
    if (plan.target_hz > 0 && remainder_carry >= static_cast<int64_t>(plan.target_hz)) {
        const int64_t extra_samples = remainder_carry / static_cast<int64_t>(plan.target_hz);
        step_samples += extra_samples;
        remainder_carry -= extra_samples * static_cast<int64_t>(plan.target_hz);
    }
    return step_samples;
}

}  // namespace tenriff::app
