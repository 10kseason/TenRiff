#pragma once

#include <string>

#include "game/GaugeManager.h"

namespace tenriff::app {

inline bool gameplay_session_cleared(bool finished,
                                     bool game_over,
                                     bool user_aborted,
                                     bool autoplay_enabled = false) {
    // Autoplay may finish the chart, but it must never award an official clear.
    return finished && !game_over && !user_aborted && !autoplay_enabled;
}

inline bool gameplay_session_assist_active(bool autoplay_enabled, bool practice_no_fail_enabled) {
    return autoplay_enabled || practice_no_fail_enabled;
}

inline std::string gameplay_session_assist_prefix(bool autoplay_enabled, bool practice_no_fail_enabled) {
    if (!gameplay_session_assist_active(autoplay_enabled, practice_no_fail_enabled)) {
        return {};
    }

    std::string prefix = "ASSIST";
    if (autoplay_enabled) {
        prefix += " AUTOPLAY";
    }
    if (practice_no_fail_enabled) {
        prefix += " PRACTICE";
    }
    prefix += " ";
    return prefix;
}

inline std::string gameplay_session_clear_status(bool finished,
                                                 bool game_over,
                                                 bool user_aborted,
                                                 game::GaugeType final_gauge,
                                                 bool autoplay_enabled = false,
                                                 bool practice_no_fail_enabled = false,
                                                 bool one_miss_fail_enabled = false,
                                                 bool gauge_shift_enabled = false) {
    if (user_aborted) {
        return "ABORTED";
    }
    if (!finished || game_over) {
        return "FAILED";
    }
    if (autoplay_enabled) {
        return "AUTOPLAY";
    }
    const std::string assist_prefix =
        gameplay_session_assist_prefix(autoplay_enabled, practice_no_fail_enabled);
    if (one_miss_fail_enabled) {
        return assist_prefix + "SUDDEN DEATH CLEAR";
    }
    if (gauge_shift_enabled) {
        switch (final_gauge) {
            case game::GaugeType::ExHard: return assist_prefix + "GAUGE SHIFT EX-HARD CLEAR";
            case game::GaugeType::Hard: return assist_prefix + "GAUGE SHIFT HARD CLEAR";
            case game::GaugeType::Normal: return assist_prefix + "GAUGE SHIFT NORMAL CLEAR";
            case game::GaugeType::Easy:
            default: return assist_prefix + "GAUGE SHIFT EASY CLEAR";
        }
    }
    switch (final_gauge) {
        case game::GaugeType::ExHard: return assist_prefix + "EX-HARD CLEAR";
        case game::GaugeType::Hard: return assist_prefix + "HARD CLEAR";
        case game::GaugeType::Easy: return assist_prefix + "EASY CLEAR";
        case game::GaugeType::Normal:
        default: return assist_prefix + "CLEAR";
    }
}

}  // namespace tenriff::app
