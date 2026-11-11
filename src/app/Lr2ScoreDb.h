#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::app {

enum class Lr2ScoreDbSource { Lr2, Lr2oraja };
inline constexpr int kLr2orajaLnModeCount = 3;

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

// Score DB fields describe independent personal bests. The lamp, combo and
// minimum BP may have been set on different plays than the EX-score counts.
struct Lr2orajaScore {
    int clear = 0;
    int notes = 0;
    int maxcombo = 0;
    int minbp = 0;
    // PG, GR, GD, BAD, POOR, empty POOR in database order.
    std::array<int, 6> early{};
    std::array<int, 6> late{};

    [[nodiscard]] std::int64_t ex_score() const noexcept;
    [[nodiscard]] bool valid() const noexcept;
};

struct Lr2orajaScoreRow {
    std::string sha256;
    int ln_mode = 0;
    Lr2orajaScore score;
};

[[nodiscard]] bool is_lr2oraja_chart_hash(std::string_view hash) noexcept;

struct Lr2ScoreDbResult {
    Lr2ScoreDbSource source = Lr2ScoreDbSource::Lr2;
    std::vector<Lr2ScoreRow> rows;
    std::vector<Lr2orajaScoreRow> lr2oraja_rows;
    std::size_t skipped_rows = 0;
    std::string error;

    [[nodiscard]] bool success() const { return error.empty(); }
};

// Reads an LR2 or LR2oraja score table without modifying it. Supports UTF-8 and
// UTF-16 SQLite files and requires WAL frames to have been checkpointed.
[[nodiscard]] Lr2ScoreDbResult read_lr2_score_db(const std::filesystem::path& path);
[[nodiscard]] Lr2ScoreDbResult read_lr2_score_db_bytes(std::string_view bytes);

}  // namespace tenriff::app
