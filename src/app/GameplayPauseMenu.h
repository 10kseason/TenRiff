#pragma once

#include <algorithm>
#include <cstdint>

namespace tenriff::app {

enum class GameplayPauseAction {
    Continue,
    Restart,
    Exit,
};

inline constexpr int kGameplayPauseMenuItemCount = 3;

[[nodiscard]] inline std::int64_t gameplay_logical_sample(
    std::int64_t physical_sample,
    std::int64_t pause_sample_offset) {
    return std::max<std::int64_t>(0, physical_sample - pause_sample_offset);
}

[[nodiscard]] inline std::int64_t gameplay_pause_resume_offset(
    std::int64_t current_offset,
    std::int64_t physical_pause_start,
    std::int64_t physical_resume_sample) {
    return current_offset +
           std::max<std::int64_t>(0, physical_resume_sample - physical_pause_start);
}

[[nodiscard]] inline int wrap_gameplay_pause_cursor(int cursor, int delta) {
    const int normalized = std::clamp(cursor, 0, kGameplayPauseMenuItemCount - 1);
    return (normalized + delta % kGameplayPauseMenuItemCount + kGameplayPauseMenuItemCount) %
           kGameplayPauseMenuItemCount;
}

[[nodiscard]] inline GameplayPauseAction gameplay_pause_action_for_cursor(int cursor) {
    switch (std::clamp(cursor, 0, kGameplayPauseMenuItemCount - 1)) {
        case 1:
            return GameplayPauseAction::Restart;
        case 2:
            return GameplayPauseAction::Exit;
        default:
            return GameplayPauseAction::Continue;
    }
}

}  // namespace tenriff::app
