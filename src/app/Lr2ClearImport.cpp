#include "app/Lr2ClearImport.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <sstream>

#include "app/JudgeTimingPolicy.h"
#include "config/Config.h"
#include "config/SimpleJson.h"
#include "game/GaugeManager.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

constexpr std::size_t kMaxGhostEntries = 400'000;
constexpr std::size_t kMaxGhostText = 1u << 20;

// Two-character expansions of the LR2 ghost alphabet. Derived from a real LR2
// score database and checked against its stored judgement counts; see
// docs/lr2-clear-import.ko.md.
const char* ghost_expansion(char ch) {
    switch (ch) {
    case 'F': return "E1"; case 'G': return "E2"; case 'H': return "E3";
    case 'I': return "E4"; case 'J': return "E5"; case 'K': return "E6";
    case 'L': return "E7"; case 'M': return "E8"; case 'N': return "E9";
    case 'P': return "EC"; case 'Q': return "EB"; case 'R': return "EA";
    case 'S': return "D2"; case 'T': return "D3"; case 'U': return "D4";
    case 'V': return "D5"; case 'W': return "D6";
    case 'X': return "DE"; case 'Y': return "DC";
    case 'a': return "DB"; case 'b': return "DA";
    case 'c': return "C2"; case 'd': return "C3"; case 'e': return "C4"; case 'f': return "C5";
    case 'g': return "CE"; case 'h': return "CD"; case 'i': return "CB"; case 'j': return "CA";
    case 'k': return "AB"; case 'l': return "AC"; case 'm': return "AD"; case 'n': return "AE";
    case 'o': return "A2"; case 'p': return "A3";
    case 'q': return "XX";
    case 'r': return "X1"; case 's': return "X2"; case 't': return "X3";
    case 'u': return "X4"; case 'v': return "X5"; case 'w': return "X6";
    case 'x': return "X7"; case 'y': return "X8"; case 'z': return "X9";
    default: return nullptr;
    }
}

std::optional<Lr2Judgement> ghost_symbol(char ch) {
    switch (ch) {
    case 'E': return Lr2Judgement::PGreat;
    case 'D': return Lr2Judgement::Great;
    case 'C': return Lr2Judgement::Good;
    case 'B': return Lr2Judgement::Bad;
    case 'A': return Lr2Judgement::Poor;
    case '@': return Lr2Judgement::EmptyPoor;
    default: return std::nullopt;
    }
}

struct Lr2Windows {
    double pg, gr, gd, bd;
};

// Upper edge of each LR2 judgement window by #RANK (0 VERY HARD .. 3 EASY).
// PGREAT and BAD agree between lr2oraja's LR2 judge table and Lunatic Vibes;
// GREAT and GOOD differ by a few ms, so each bound is the wider of the two.
constexpr std::array<Lr2Windows, 4> kLr2Windows{{
    {8.0, 27.0, 40.0, 200.0},
    {15.0, 32.0, 60.0, 200.0},
    {18.0, 40.0, 100.0, 200.0},
    {21.0, 60.0, 120.0, 200.0},
}};

enum class Worst { PG, GR, GD, BD, OutsideBad };

Worst classify(double ms, const config::JudgeConfig& judge) {
    if (ms <= judge.pg_ms) return Worst::PG;
    if (ms <= judge.gr_ms) return Worst::GR;
    if (ms <= judge.gd_ms) return Worst::GD;
    if (ms <= judge.bd_ms) return Worst::BD;
    return Worst::OutsideBad;
}

int playable_note_count(const gameplay::GameplayChart& chart) {
    // Same rule the engine uses for the BMS gauge: one per tap or complete LN.
    const int lanes = chart.lane_count > 0 ? chart.lane_count : 10;
    int count = 0;
    for (const auto& note : chart.notes) {
        if (note.lane > 0 && note.lane <= lanes && count < (std::numeric_limits<int>::max)()) ++count;
    }
    return count;
}

Lr2ExEvaluation not_evaluated(std::string reason) {
    Lr2ExEvaluation evaluation;
    evaluation.reason = std::move(reason);
    return evaluation;
}

}  // namespace

std::optional<std::vector<Lr2Judgement>> decode_lr2_ghost(std::string_view ghost) {
    if (ghost.empty() || ghost.size() > kMaxGhostText) return std::nullopt;
    // Substitution is at most two levels deep (q -> XX -> DEDE).
    std::string text(ghost);
    for (int pass = 0; pass < 3; ++pass) {
        std::string next;
        next.reserve(text.size() * 2);
        bool changed = false;
        for (const char ch : text) {
            if (const char* expansion = ghost_expansion(ch)) {
                next += expansion;
                changed = true;
            } else {
                next.push_back(ch);
            }
        }
        text = std::move(next);
        if (!changed) break;
        if (text.size() > kMaxGhostText * 4) return std::nullopt;
    }

    std::vector<Lr2Judgement> sequence;
    std::size_t i = 0;
    while (i < text.size()) {
        const char ch = text[i++];
        if (ch == 'Z') {
            return i == text.size() ? std::optional(std::move(sequence)) : std::nullopt;
        }
        const auto symbol = ghost_symbol(ch);
        if (!symbol) return std::nullopt;
        std::size_t count = 1;
        if (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            count = 0;
            while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
                count = count * 10 + static_cast<std::size_t>(text[i++] - '0');
                if (count > kMaxGhostEntries) return std::nullopt;
            }
        }
        if (sequence.size() + count > kMaxGhostEntries) return std::nullopt;
        sequence.insert(sequence.end(), count, *symbol);
    }
    return std::nullopt;  // no terminator
}

Lr2ExEvaluation evaluate_lr2_ex_worst_case(const Lr2ScoreRow& row,
                                           const gameplay::GameplayChart& chart,
                                           ChartFormat format) {
    if (row.clear < 2) return not_evaluated("lr2_not_cleared");
    if (format != ChartFormat::Bms) return not_evaluated("not_bms");
    if (chart.bms_has_random) return not_evaluated("random_chart");
    if (!chart.mines.empty()) return not_evaluated("has_mines");
    if (std::any_of(chart.notes.begin(), chart.notes.end(),
                    [](const gameplay::NoteEvent& note) { return note.end_sample.has_value(); })) {
        return not_evaluated("has_long_notes");
    }
    if (!chart.bms_rank || *chart.bms_rank < 0 || *chart.bms_rank > 3) return not_evaluated("rank_unsupported");
    const int notes = playable_note_count(chart);
    if (notes <= 0 || notes != row.totalnotes) return not_evaluated("note_count_mismatch");

    const auto ghost = decode_lr2_ghost(row.ghost);
    if (!ghost) return not_evaluated(row.ghost.empty() ? "ghost_missing" : "ghost_undecodable");
    std::array<int, 6> counts{};
    for (const auto judgement : *ghost) ++counts[static_cast<std::size_t>(judgement)];
    const int judged = counts[0] + counts[1] + counts[2] + counts[3] + counts[4];
    if (judged != row.totalnotes) return not_evaluated("ghost_incomplete");
    if (counts[0] != row.perfect || counts[1] != row.great || counts[2] != row.good ||
        counts[3] != row.bad || counts[4] + counts[5] != row.poor) {
        return not_evaluated("ghost_counts_mismatch");
    }

    const int rank = *chart.bms_rank;
    const auto judge = judge_timing_for_fixed_baseline(config::JudgeConfig{}, rank, false, false, false);
    const Lr2Windows& lr2 = kLr2Windows[static_cast<std::size_t>(rank)];
    const std::array<Worst, 4> worst{classify(lr2.pg, judge), classify(lr2.gr, judge),
                                     classify(lr2.gd, judge), classify(lr2.bd, judge)};

    game::GaugeRuntimePolicy policy;
    policy.bms_lr2_deltas = true;
    policy.bms_total = chart.bms_total.value_or(0.0);
    policy.bms_note_count = notes;
    const game::GaugeManager gauge(game::GaugeConfig{}, policy);
    auto state = gauge.initialState(game::GaugeType::ExHard);

    Lr2ExEvaluation evaluation;
    evaluation.minimum_gauge = state.value;
    const auto apply = [&](game::Judgement judgement, bool empty_poor) {
        gauge.applyJudgementWeighted(state, judgement, 0.0, 1.0, empty_poor);
        evaluation.minimum_gauge = (std::min)(evaluation.minimum_gauge, state.value);
    };
    for (const auto judgement : *ghost) {
        switch (judgement) {
        case Lr2Judgement::Poor:
            apply(game::Judgement::PR, false);
            break;
        case Lr2Judgement::EmptyPoor:
            apply(game::Judgement::PR, true);
            break;
        default:
            switch (worst[static_cast<std::size_t>(judgement)]) {
            case Worst::PG: apply(game::Judgement::PG, false); break;
            case Worst::GR: apply(game::Judgement::GR, false); break;
            case Worst::GD: apply(game::Judgement::GD, false); break;
            case Worst::BD: apply(game::Judgement::BD, false); break;
            case Worst::OutsideBad:
                // The press falls outside TenRiff's BAD window: it is an empty
                // poor, and the untouched note is then missed.
                apply(game::Judgement::PR, true);
                apply(game::Judgement::PR, false);
                break;
            }
            break;
        }
        if (state.game_over || state.value <= 0.0) {
            evaluation.verdict = Lr2ExVerdict::Failed;
            return evaluation;
        }
    }
    evaluation.verdict = Lr2ExVerdict::Proven;
    return evaluation;
}

std::optional<Lr2ImportedLamp> lr2_base_lamp(const Lr2ScoreRow& row) {
    Lr2ImportedLamp lamp;
    lamp.lr2_clear = row.clear;
    switch (row.clear) {
    case 2: lamp.clear_status = "EASY CLEAR"; break;
    case 3: lamp.clear_status = "CLEAR"; break;
    case 4: lamp.clear_status = "HARD CLEAR"; break;
    case 5:
        lamp.clear_status = "HARD CLEAR";
        lamp.full_combo = true;
        break;
    default: return std::nullopt;
    }
    lamp.ex_note = "not_evaluated";
    return lamp;
}

void apply_lr2_ex_evaluation(Lr2ImportedLamp& lamp, const Lr2ExEvaluation& evaluation) {
    switch (evaluation.verdict) {
    case Lr2ExVerdict::Proven:
        lamp.ex_proven = true;
        lamp.ex_note = "proven";
        lamp.clear_status = "EX-HARD CLEAR";
        break;
    case Lr2ExVerdict::Failed:
        lamp.ex_note = "failed";
        break;
    case Lr2ExVerdict::NotEvaluated:
        lamp.ex_note = evaluation.reason.empty() ? "not_evaluated" : evaluation.reason;
        break;
    }
}

std::string serialize_lr2_imported_lamps(const Lr2ImportedLamps& lamps) {
    using config::JsonArray;
    using config::JsonObject;
    using config::JsonValue;
    std::vector<const Lr2ImportedLamps::value_type*> ordered;
    ordered.reserve(lamps.size());
    for (const auto& entry : lamps) ordered.push_back(&entry);
    std::sort(ordered.begin(), ordered.end(),
              [](const auto* lhs, const auto* rhs) { return lhs->first < rhs->first; });
    JsonArray entries;
    entries.reserve(ordered.size());
    for (const auto* entry : ordered) {
        JsonObject object;
        object.emplace("md5", JsonValue(entry->first));
        object.emplace("lr2_clear", JsonValue(static_cast<double>(entry->second.lr2_clear)));
        object.emplace("clear_status", JsonValue(entry->second.clear_status));
        object.emplace("full_combo", JsonValue(entry->second.full_combo));
        object.emplace("ex_proven", JsonValue(entry->second.ex_proven));
        object.emplace("ex_note", JsonValue(entry->second.ex_note));
        entries.emplace_back(std::move(object));
    }
    JsonObject root;
    root.emplace("format", JsonValue(1.0));
    root.emplace("source", JsonValue("lr2_score_db"));
    root.emplace("scope", JsonValue("Display-only imported clear lamps. Never a TenRiff result or upload candidate."));
    root.emplace("entries", JsonValue(std::move(entries)));
    return config::json_stringify(JsonValue(std::move(root)), 1);
}

Lr2ImportedLamps parse_lr2_imported_lamps(std::string_view json, std::string* error) {
    Lr2ImportedLamps lamps;
    const auto parsed = config::parse_json(json);
    const auto* root = parsed.success() ? parsed.root->as_object() : nullptr;
    const auto fail = [&](const char* message) {
        if (error) *error = message;
        return Lr2ImportedLamps{};
    };
    if (!root) return fail("Invalid LR2 import file.");
    const auto format = root->find("format");
    const auto entries = root->find("entries");
    if (format == root->end() || format->second.as_number() != 1.0 || entries == root->end() ||
        !entries->second.as_array()) {
        return fail("Unsupported LR2 import file.");
    }
    for (const auto& value : *entries->second.as_array()) {
        const auto* object = value.as_object();
        if (!object) continue;
        const auto text = [&](const char* key) {
            const auto it = object->find(key);
            return it != object->end() ? it->second.as_string() : std::string{};
        };
        const auto flag = [&](const char* key) {
            const auto it = object->find(key);
            return it != object->end() && it->second.as_bool();
        };
        const std::string md5 = text("md5");
        Lr2ImportedLamp lamp;
        lamp.clear_status = text("clear_status");
        // Only the four statuses the importer writes are accepted, so an edited
        // file cannot inject arbitrary lamp text.
        if (md5.size() != 32 ||
            (lamp.clear_status != "EASY CLEAR" && lamp.clear_status != "CLEAR" &&
             lamp.clear_status != "HARD CLEAR" && lamp.clear_status != "EX-HARD CLEAR")) {
            continue;
        }
        const auto clear = object->find("lr2_clear");
        lamp.lr2_clear = clear != object->end() ? static_cast<int>(clear->second.as_number()) : 0;
        lamp.full_combo = flag("full_combo");
        lamp.ex_proven = flag("ex_proven");
        lamp.ex_note = text("ex_note");
        if (lamp.clear_status == "EX-HARD CLEAR" && !lamp.ex_proven) continue;
        lamps.emplace(md5, std::move(lamp));
    }
    return lamps;
}

Lr2ImportedLamps load_lr2_imported_lamps(const std::string& utf8_path) {
    std::ifstream stream(util::path_from_utf8_lossy(utf8_path), std::ios::binary);
    if (!stream) return {};
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return parse_lr2_imported_lamps(buffer.str());
}

std::string lr2_import_path_for_profile(std::string_view profile_root) {
    std::string path(profile_root);
    if (!path.empty() && path.back() != '/' && path.back() != '\\') path.push_back('/');
    return path + "lr2_import.json";
}

}  // namespace tenriff::app
