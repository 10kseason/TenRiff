#include "doctest/doctest.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "app/Lr2ClearImport.h"
#include "app/Lr2ScoreDb.h"

using namespace tenriff;
using app::Lr2ExVerdict;
using app::Lr2Judgement;

namespace {

std::string fixture_bytes() {
    const auto path = std::filesystem::u8path(TENRIFF_SOURCE_DIR) / "tests" / "fixtures" / "lr2" / "score_fixture.db";
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::string fixture_hash(int i) {
    const std::uint64_t value = static_cast<std::uint64_t>(i) * 2654435761ull;
    char text[33];
    std::snprintf(text, sizeof(text), "%032llx", static_cast<unsigned long long>(value));
    return text;
}

gameplay::GameplayChart tap_chart(int notes, int rank) {
    gameplay::GameplayChart chart;
    chart.lane_count = 8;
    chart.bms_rank = rank;
    chart.bms_total = 300.0;
    for (int i = 0; i < notes; ++i) {
        gameplay::NoteEvent note;
        note.lane = 1 + i % 8;
        note.start_sample = 44100 + static_cast<int64_t>(i) * 4410;
        note.note_id = static_cast<std::size_t>(i);
        chart.notes.push_back(note);
    }
    return chart;
}

app::Lr2ScoreRow row_for(std::string ghost, int notes, int pg, int gr, int gd, int bd, int pr) {
    app::Lr2ScoreRow row;
    row.hash = std::string(32, 'a');
    row.clear = 4;
    row.perfect = pg;
    row.great = gr;
    row.good = gd;
    row.bad = bd;
    row.poor = pr;
    row.totalnotes = notes;
    row.ghost = std::move(ghost);
    return row;
}

}  // namespace

TEST_CASE("LR2 score database reader returns every row including overflow and NULL text") {
    const std::string bytes = fixture_bytes();
    REQUIRE(bytes.size() > 100u);
    const auto result = app::read_lr2_score_db_bytes(bytes);
    REQUIRE(result.success());
    REQUIRE(result.rows.size() == 400u);
    if (result.rows.size() != 400u) return;  // REQUIRE does not abort in this harness

    std::vector<bool> seen(400, false);
    for (const auto& row : result.rows) {
        int index = -1;
        for (int i = 0; i < 400; ++i) {
            if (fixture_hash(i) == row.hash) {
                index = i;
                break;
            }
        }
        REQUIRE(index >= 0);
        if (index < 0) continue;
        CHECK_FALSE(seen[static_cast<std::size_t>(index)]);
        seen[static_cast<std::size_t>(index)] = true;
        CHECK(row.clear == index % 6);
        CHECK(row.perfect == index);
        CHECK(row.great == 2 * index);
        CHECK(row.good == index % 7);
        CHECK(row.bad == index % 5);
        CHECK(row.poor == index % 3);
        CHECK(row.totalnotes == 3 * index + 11);
        CHECK(row.maxcombo == index + 1);
        CHECK(row.minbp == index % 9);
        if (index % 97 == 3) {
            CHECK(row.ghost.empty());
        } else if (index % 50 == 7) {
            CHECK(row.ghost.size() == static_cast<std::size_t>(2 * (1500 + index) + 1));
            CHECK(row.ghost.substr(0, 4) == "EDED");
            CHECK(row.ghost.back() == 'Z');
        } else {
            CHECK(row.ghost == "E" + std::to_string(index + 1) + "Z");
        }
    }
    CHECK(std::all_of(seen.begin(), seen.end(), [](bool value) { return value; }));
}

TEST_CASE("LR2 score database reader rejects damaged input without crashing") {
    const std::string bytes = fixture_bytes();
    REQUIRE(bytes.size() > 4096u);
    if (bytes.size() <= 50'000u) return;
    CHECK_FALSE(app::read_lr2_score_db_bytes("").success());
    CHECK_FALSE(app::read_lr2_score_db_bytes("not a database").success());
    CHECK_FALSE(app::read_lr2_score_db_bytes(std::string_view(bytes).substr(0, 99)).success());
    // Truncations and deterministic byte damage must end in a result, never a
    // read outside the buffer (the ASan job runs this test too).
    for (std::size_t size : {100u, 512u, 1024u, 1500u, 4096u, 50'000u}) {
        const auto result = app::read_lr2_score_db_bytes(std::string_view(bytes).substr(0, size));
        CHECK(result.rows.size() <= 400u);
    }
    std::uint32_t state = 0x1234567u;
    for (int round = 0; round < 400; ++round) {
        std::string damaged = bytes;
        for (int flip = 0; flip < 6; ++flip) {
            state = state * 1664525u + 1013904223u;
            const std::size_t offset = state % damaged.size();
            state = state * 1664525u + 1013904223u;
            damaged[offset] = static_cast<char>(state >> 24);
        }
        const auto result = app::read_lr2_score_db_bytes(damaged);
        CHECK(result.rows.size() <= 1'000'000u);
    }
}

TEST_CASE("LR2 ghost decoder expands run lengths and substitutions exactly") {
    // Real-format example: 224 PGREAT, 7 GREAT, 1 BAD over 232 notes.
    const auto sequence = app::decode_lr2_ghost("F00t7BG1xwttq46Z");
    REQUIRE(sequence.has_value());
    if (!sequence || sequence->size() != 232u) {
        CHECK(false);
        return;
    }
    CHECK(std::count(sequence->begin(), sequence->end(), Lr2Judgement::PGreat) == 224);
    CHECK(std::count(sequence->begin(), sequence->end(), Lr2Judgement::Great) == 7);
    CHECK(std::count(sequence->begin(), sequence->end(), Lr2Judgement::Bad) == 1);
    CHECK((*sequence)[99] == Lr2Judgement::PGreat);
    CHECK((*sequence)[100] == Lr2Judgement::Great);
    CHECK((*sequence)[138] == Lr2Judgement::Bad);

    const auto mixed = app::decode_lr2_ghost("E2@AoCZ");
    REQUIRE(mixed.has_value());
    if (!mixed) return;
    const std::vector<Lr2Judgement> expected{
        Lr2Judgement::PGreat, Lr2Judgement::PGreat, Lr2Judgement::EmptyPoor, Lr2Judgement::Poor,
        Lr2Judgement::Poor, Lr2Judgement::Poor, Lr2Judgement::Good};
    CHECK(*mixed == expected);

    CHECK_FALSE(app::decode_lr2_ghost("").has_value());
    CHECK_FALSE(app::decode_lr2_ghost("E10").has_value());        // no terminator
    CHECK_FALSE(app::decode_lr2_ghost("E10ZE").has_value());      // text after terminator
    CHECK_FALSE(app::decode_lr2_ghost("E10?Z").has_value());      // unknown symbol
    CHECK_FALSE(app::decode_lr2_ghost("12Z").has_value());        // count without a symbol
    CHECK_FALSE(app::decode_lr2_ghost("E99999999Z").has_value()); // absurd length
}

TEST_CASE("LR2 worst-case EX evaluation proves only plays that survive every allowed timing") {
    const int notes = 1000;
    {  // all PGREAT survives
        const auto row = row_for("E1000Z", notes, 1000, 0, 0, 0, 0);
        const auto result = app::evaluate_lr2_ex_worst_case(row, tap_chart(notes, 2), app::ChartFormat::Bms);
        CHECK(result.verdict == Lr2ExVerdict::Proven);
        CHECK(result.minimum_gauge > 99.999);
    }
    {  // a run of missed notes fails
        const auto row = row_for("E500A20E480Z", notes, 980, 0, 0, 0, 20);
        const auto result = app::evaluate_lr2_ex_worst_case(row, tap_chart(notes, 2), app::ChartFormat::Bms);
        CHECK(result.verdict == Lr2ExVerdict::Failed);
        CHECK(result.minimum_gauge <= 0.0);
    }
    {  // LR2 GOOD can be a TenRiff BAD at NORMAL rank but not at HARD rank
        // NORMAL: LR2 GOOD reaches 100 ms, past TenRiff's 98.57 ms GOOD edge.
        // HARD: LR2 GOOD reaches 60 ms, inside TenRiff's 82.14 ms GOOD edge.
        const auto row = row_for("E400C30E570Z", notes, 970, 0, 30, 0, 0);
        CHECK(app::evaluate_lr2_ex_worst_case(row, tap_chart(notes, 2), app::ChartFormat::Bms).verdict ==
              Lr2ExVerdict::Failed);
        CHECK(app::evaluate_lr2_ex_worst_case(row, tap_chart(notes, 1), app::ChartFormat::Bms).verdict ==
              Lr2ExVerdict::Proven);
    }
    {  // LR2 BAD outside TenRiff's BAD window costs more than a BAD
        const auto row = row_for("E500BE499Z", notes, 999, 0, 0, 1, 0);
        const auto easy = app::evaluate_lr2_ex_worst_case(row, tap_chart(notes, 3), app::ChartFormat::Bms);
        const auto normal = app::evaluate_lr2_ex_worst_case(row, tap_chart(notes, 2), app::ChartFormat::Bms);
        CHECK(easy.verdict == Lr2ExVerdict::Proven);
        CHECK(normal.verdict == Lr2ExVerdict::Proven);
        CHECK(normal.minimum_gauge < easy.minimum_gauge);
    }
    {  // empty poors are charged in place
        const auto clean = row_for("E1000Z", notes, 1000, 0, 0, 0, 0);
        const auto dirty = row_for("E500@3E500Z", notes, 1000, 0, 0, 0, 3);
        const auto a = app::evaluate_lr2_ex_worst_case(clean, tap_chart(notes, 2), app::ChartFormat::Bms);
        const auto b = app::evaluate_lr2_ex_worst_case(dirty, tap_chart(notes, 2), app::ChartFormat::Bms);
        CHECK(b.verdict == Lr2ExVerdict::Proven);
        CHECK(b.minimum_gauge < a.minimum_gauge);
    }
}

TEST_CASE("LR2 worst-case EX evaluation refuses inputs it cannot compare") {
    const int notes = 200;
    const auto good_row = row_for("E200Z", notes, 200, 0, 0, 0, 0);
    const auto reason = [&](const app::Lr2ScoreRow& row, const gameplay::GameplayChart& chart,
                            app::ChartFormat format = app::ChartFormat::Bms) {
        const auto result = app::evaluate_lr2_ex_worst_case(row, chart, format);
        CHECK(result.verdict == Lr2ExVerdict::NotEvaluated);
        return result.reason;
    };
    CHECK(app::evaluate_lr2_ex_worst_case(good_row, tap_chart(notes, 2), app::ChartFormat::Bms).verdict ==
          Lr2ExVerdict::Proven);

    auto failed = good_row;
    failed.clear = 1;
    CHECK(reason(failed, tap_chart(notes, 2)) == "lr2_not_cleared");
    CHECK(reason(good_row, tap_chart(notes, 2), app::ChartFormat::Unknown) == "not_bms");

    auto random_chart = tap_chart(notes, 2);
    random_chart.bms_has_random = true;
    CHECK(reason(good_row, random_chart) == "random_chart");

    auto long_chart = tap_chart(notes, 2);
    long_chart.notes[10].end_sample = long_chart.notes[10].start_sample + 1000;
    CHECK(reason(good_row, long_chart) == "has_long_notes");

    auto mine_chart = tap_chart(notes, 2);
    mine_chart.mines.emplace_back();
    CHECK(reason(good_row, mine_chart) == "has_mines");

    auto no_rank = tap_chart(notes, 2);
    no_rank.bms_rank.reset();
    CHECK(reason(good_row, no_rank) == "rank_unsupported");
    CHECK(reason(good_row, tap_chart(notes, 4)) == "rank_unsupported");
    CHECK(reason(good_row, tap_chart(notes + 1, 2)) == "note_count_mismatch");

    CHECK(reason(row_for("", notes, 200, 0, 0, 0, 0), tap_chart(notes, 2)) == "ghost_missing");
    CHECK(reason(row_for("E200", notes, 200, 0, 0, 0, 0), tap_chart(notes, 2)) == "ghost_undecodable");
    CHECK(reason(row_for("E150Z", notes, 150, 0, 0, 0, 0), tap_chart(notes, 2)) == "ghost_incomplete");
    CHECK(reason(row_for("E200Z", notes, 199, 1, 0, 0, 0), tap_chart(notes, 2)) == "ghost_counts_mismatch");
}

TEST_CASE("LR2 imported lamps map LR2 clears and survive a strict round trip") {
    app::Lr2ScoreRow row;
    row.clear = 0;
    CHECK_FALSE(app::lr2_base_lamp(row).has_value());
    row.clear = 1;
    CHECK_FALSE(app::lr2_base_lamp(row).has_value());
    row.clear = 2;
    CHECK(app::lr2_base_lamp(row)->clear_status == "EASY CLEAR");
    row.clear = 3;
    CHECK(app::lr2_base_lamp(row)->clear_status == "CLEAR");
    row.clear = 4;
    CHECK(app::lr2_base_lamp(row)->clear_status == "HARD CLEAR");
    CHECK_FALSE(app::lr2_base_lamp(row)->full_combo);
    row.clear = 5;
    auto full = *app::lr2_base_lamp(row);
    CHECK(full.clear_status == "HARD CLEAR");
    CHECK(full.full_combo);

    app::Lr2ExEvaluation proven;
    proven.verdict = Lr2ExVerdict::Proven;
    app::apply_lr2_ex_evaluation(full, proven);
    CHECK(full.clear_status == "EX-HARD CLEAR");
    CHECK(full.ex_proven);

    auto hard = *app::lr2_base_lamp(row);
    app::Lr2ExEvaluation failed;
    failed.verdict = Lr2ExVerdict::Failed;
    app::apply_lr2_ex_evaluation(hard, failed);
    CHECK(hard.clear_status == "HARD CLEAR");
    CHECK(hard.ex_note == "failed");

    app::Lr2ImportedLamps lamps;
    lamps.emplace(std::string(32, '1'), full);
    lamps.emplace(std::string(32, '2'), hard);
    const auto parsed = app::parse_lr2_imported_lamps(app::serialize_lr2_imported_lamps(lamps));
    REQUIRE(parsed.size() == 2u);
    if (parsed.size() != 2u) return;
    CHECK(parsed.at(std::string(32, '1')).clear_status == "EX-HARD CLEAR");
    CHECK(parsed.at(std::string(32, '1')).full_combo);
    CHECK(parsed.at(std::string(32, '2')).clear_status == "HARD CLEAR");
    CHECK(parsed.at(std::string(32, '2')).ex_note == "failed");

    // Hand-edited files cannot add other lamp text or an unproven EX.
    const std::string edited =
        R"({"format":1,"entries":[)"
        R"({"md5":"33333333333333333333333333333333","clear_status":"AUTOPLAY","lr2_clear":4},)"
        R"({"md5":"44444444444444444444444444444444","clear_status":"EX-HARD CLEAR","ex_proven":false},)"
        R"({"md5":"short","clear_status":"CLEAR"},)"
        R"({"md5":"55555555555555555555555555555555","clear_status":"CLEAR","lr2_clear":3}]})";
    const auto filtered = app::parse_lr2_imported_lamps(edited);
    REQUIRE(filtered.size() == 1u);
    CHECK(filtered.count(std::string(32, '5')) == 1u);
    std::string error;
    CHECK(app::parse_lr2_imported_lamps(R"({"format":2,"entries":[]})", &error).empty());
    CHECK_FALSE(error.empty());
    CHECK(app::parse_lr2_imported_lamps("not json", &error).empty());
}
