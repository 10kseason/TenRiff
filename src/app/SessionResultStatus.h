#pragma once

#include <string>

#include "game/GaugeManager.h"

namespace tenriff::app {

inline bool gameplay_session_cleared(bool finished, bool game_over, bool user_aborted) {
    return finished && !game_over && !user_aborted;
}

inline std::string gameplay_session_clear_status(bool finished,
                                                 bool game_over,
                                                 bool user_aborted,
                                                 game::GaugeType final_gauge) {
    if (user_aborted) {
        return "ABORTED";
    }
    if (!finished || game_over) {
        return "FAILED";
    }
    switch (final_gauge) {
        case game::GaugeType::Hard: return "HARD CLEAR";
        case game::GaugeType::Easy: return "EASY CLEAR";
        case game::GaugeType::Normal:
        default: return "CLEAR";
    }
}

}  // namespace tenriff::app
