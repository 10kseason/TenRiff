#pragma once

#include <algorithm>
#include <cstdint>

namespace tenriff::render {

inline constexpr int kSceneTransitionFrames = 30;
inline constexpr int64_t kSceneTransitionDurationNs = 1'000'000'000LL;

struct SceneTransitionFrame {
    bool active = false;
    int index = 0;
    float opacity = 0.0f;
    float travel = 0.0f;
};

// Quantize decoration, not presentation pacing or the gameplay clock. A slow
// frame skips artwork samples rather than extending the one-second transition.
[[nodiscard]] inline SceneTransitionFrame scene_transition_frame(int64_t elapsed_ns,
                                                                bool reduced_motion = false) {
    if (elapsed_ns < 0 || elapsed_ns >= kSceneTransitionDurationNs) return {};
    const int index = static_cast<int>((elapsed_ns * kSceneTransitionFrames) /
                                       kSceneTransitionDurationNs);
    const float progress = static_cast<float>(index) / (kSceneTransitionFrames - 1);
    const float opacity = std::min({1.0f, (index + 1) / 5.0f,
                                   (kSceneTransitionFrames - index) / 7.0f});
    return {true, index, reduced_motion ? 0.70f : opacity,
            reduced_motion ? 0.5f : progress};
}

} // namespace tenriff::render
