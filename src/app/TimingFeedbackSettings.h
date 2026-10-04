#pragma once
#include <optional>
#include "config/Config.h"

namespace tenriff::app {
struct TimingFeedbackVisibility { bool text; bool bar; };

// A legacy manifest's single switch still governs both elements until the user
// edits either switch. Afterwards the saved profile wins, including after reload.
inline TimingFeedbackVisibility resolve_timing_feedback_visibility(
    const config::SkinConfig& skin, std::optional<bool> text = {}, std::optional<bool> bar = {}) {
    if (skin.timing_feedback_override) return {skin.show_timing_feedback, skin.show_timing_bar};
    return {text.value_or(skin.show_timing_feedback), bar.value_or(text.value_or(skin.show_timing_bar))};
}
} // namespace tenriff::app
