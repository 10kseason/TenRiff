#pragma once
#include <optional>
#include "config/Config.h"

namespace tenriff::app {
struct TimingFeedbackVisibility { bool text; bool bar; bool always_visible; };

// A legacy manifest's single switch still governs both elements until the user
// edits either switch. Afterwards the saved profile wins, including after reload.
inline TimingFeedbackVisibility resolve_timing_feedback_visibility(
    const config::SkinConfig& skin, std::optional<bool> text = {}, std::optional<bool> bar = {}, std::optional<bool> always_visible = {}) {
    if (skin.timing_feedback_override) return {skin.show_timing_feedback, skin.show_timing_bar, skin.timing_bar_always_visible};
    return {text.value_or(skin.show_timing_feedback), bar.value_or(text.value_or(skin.show_timing_bar)),
            always_visible.value_or(skin.timing_bar_always_visible)};
}
} // namespace tenriff::app
