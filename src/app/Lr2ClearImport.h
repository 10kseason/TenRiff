#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "app/ChartLoader.h"
#include "app/Lr2ScoreDb.h"
#include "gameplay/GameplayChart.h"

namespace tenriff::app {

// Imported LR2 clear lamps are display-only local data. They never become a
// TenRiff result, a BEST record or an upload candidate.

enum class Lr2Judgement : uint8_t { PGreat, Great, Good, Bad, Poor, EmptyPoor };

// Decodes the LR2 `ghost` column: the judgement of every note, in chart order,
// of the stored best-score play, with empty poors in place. Returns nullopt for
// text this decoder does not fully understand; it never guesses.
[[nodiscard]] std::optional<std::vector<Lr2Judgement>> decode_lr2_ghost(std::string_view ghost);

enum class Lr2ExVerdict { Proven, Failed, NotEvaluated };

struct Lr2ExEvaluation {
    Lr2ExVerdict verdict = Lr2ExVerdict::NotEvaluated;
    std::string reason;       // why it was not evaluated; empty otherwise
    double minimum_gauge = 100.0;
};

// Replays the LR2 judgement order through TenRiff's EX-HARD gauge, mapping each
// LR2 judgement to the worst TenRiff Normal-mode judgement its LR2 timing
// window allows. Proven therefore means the play would have survived under any
// timing consistent with LR2's own verdicts. Charts whose LR2 and TenRiff note
// models differ (long notes, mines, #RANDOM, unknown #RANK) are not evaluated.
[[nodiscard]] Lr2ExEvaluation evaluate_lr2_ex_worst_case(const Lr2ScoreRow& row,
                                                         const gameplay::GameplayChart& chart,
                                                         ChartFormat format);

struct Lr2ImportedLamp {
    int lr2_clear = 0;
    std::string clear_status;  // TenRiff clear-status text for the lamp
    bool full_combo = false;
    bool ex_proven = false;
    std::string ex_note;       // "proven", "failed" or the not-evaluated reason
};

// The lamp LR2 itself awarded; nullopt for no-play and failed rows.
[[nodiscard]] std::optional<Lr2ImportedLamp> lr2_base_lamp(const Lr2ScoreRow& row);
void apply_lr2_ex_evaluation(Lr2ImportedLamp& lamp, const Lr2ExEvaluation& evaluation);

using Lr2ImportedLamps = std::unordered_map<std::string, Lr2ImportedLamp>;  // key: lowercase MD5

[[nodiscard]] std::string serialize_lr2_imported_lamps(const Lr2ImportedLamps& lamps);
[[nodiscard]] Lr2ImportedLamps parse_lr2_imported_lamps(std::string_view json, std::string* error = nullptr);
[[nodiscard]] Lr2ImportedLamps load_lr2_imported_lamps(const std::string& utf8_path);
[[nodiscard]] std::string lr2_import_path_for_profile(std::string_view profile_root);

}  // namespace tenriff::app
