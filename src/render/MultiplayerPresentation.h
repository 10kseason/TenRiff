#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tenriff::render {

// Value-only room standings shared by the live HUD and result screen.
struct MultiplayerPlayerData {
    uint8_t player_id = 0;
    std::string name;
    bool local = false;
    bool has_score = false;
    bool finished = false;
    bool game_over = false;
    bool aborted = false;
    int rank = 0; // Competition rank; ties share a rank, missing scores are unranked.
    int64_t score = 0;
    int combo = 0;
    int max_combo = 0;
    int perfect = 0;
    int great = 0;
    int good = 0;
    int bad = 0;
    int poor = 0;
    double gauge = 0.0;
};

} // namespace tenriff::render
