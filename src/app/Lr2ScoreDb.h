#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::app {

// One row of the LR2 `score` table. Only the columns the importer needs.
struct Lr2ScoreRow {
    std::string hash;  // chart MD5, 32 hex characters
    int clear = 0;     // 1 failed, 2 easy, 3 groove, 4 hard, 5 full combo
    int perfect = 0;
    int great = 0;
    int good = 0;
    int bad = 0;
    int poor = 0;      // missed notes plus empty poors
    int totalnotes = 0;
    int maxcombo = 0;
    int minbp = 0;
    std::string ghost;  // per-note judgement sequence of the stored best-score play
};

struct Lr2ScoreDbResult {
    std::vector<Lr2ScoreRow> rows;
    std::string error;

    [[nodiscard]] bool success() const { return error.empty(); }
};

// Reads an LR2 score database without modifying it. This is a small read-only
// SQLite table reader: rollback-journal databases only, UTF-8 or UTF-16 text,
// no WAL replay. Every offset is bounds-checked because the file is user input.
[[nodiscard]] Lr2ScoreDbResult read_lr2_score_db(const std::filesystem::path& path);
[[nodiscard]] Lr2ScoreDbResult read_lr2_score_db_bytes(std::string_view bytes);

}  // namespace tenriff::app
