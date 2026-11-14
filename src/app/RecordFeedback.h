#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace tenriff::app {

// Session-only EX presentation snapshot. No legacy normalized/detail score
// participates in BEST or score celebrations.
struct RecordFeedback {
    bool has_previous = false;
    int64_t previous_score = 0;
    std::string previous_rank = "--";
    bool rank_upgraded = false;
};

[[nodiscard]] inline int record_rank_priority(std::string_view rank) {
    constexpr std::string_view ranks[]{"F", "E", "D", "C", "B", "A", "A+", "S", "S+", "S++", "S+++"};
    for (int i = 0; i < static_cast<int>(sizeof(ranks) / sizeof(ranks[0])); ++i)
        if (rank == ranks[i]) return i;
    return -1;
}

[[nodiscard]] inline bool record_score_improved(int64_t score, const RecordFeedback& previous) {
    // No previous play has a zero baseline. A zero-score first attempt is not a
    // score improvement, and a clear-lamp upgrade alone cannot trigger this.
    return score > (previous.has_previous ? previous.previous_score : 0);
}

} // namespace tenriff::app
