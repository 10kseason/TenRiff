#include "doctest/doctest.h"
#include "app/BmsGameplayBuilder.h"
#include "app/ModeManager.h"
#include "app/ReplayVerifier.h"
#include "chart/BmsChartNorm.h"
#include "chart/BmsTimeline.h"
#include "gameplay/KeyModeConverter.h"
#include "gameplay/GameplayEngine.h"
#include "gameplay/ModeApplier.h"
#include "gameplay/Nk3OnnxKeyModeAdapter.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

namespace {
using namespace tenriff::gameplay;

GameplayChart feedback_chart(int lanes) {
    GameplayChart chart;
    chart.lane_count = lanes;
    chart.duration_samples = 240000;
    for (int row = 0; row < 24; ++row) {
        for (int lane = 1; lane <= lanes; ++lane) {
            NoteEvent note;
            note.lane = lane;
            note.start_sample = row * 8000;
            note.note_id = chart.notes.size();
            note.audio_asset_id = chart.intern_audio_asset(std::to_string(note.note_id) + ".wav");
            if (lane == 1 && row % 4 == 0) note.end_sample = note.start_sample + 20000;
            if (lane == 1 && row % 4 != 0 && row % 4 != 3) continue;
            chart.notes.push_back(note);
        }
    }
    return chart;
}

void check_no_overlap(const GameplayChart& chart) {
    std::map<int, int64_t> end;
    for (const auto& note : chart.notes) {
        REQUIRE(note.lane >= 1);
        REQUIRE(note.lane <= chart.lane_count);
        if (end.count(note.lane)) CHECK(note.start_sample > end[note.lane]);
        CHECK(note.end_sample.value_or(note.start_sample) >= note.start_sample);
        end[note.lane] = note.end_sample.value_or(note.start_sample);
    }
}

std::size_t missing_sounds(const GameplayChart& source, const GameplayChart& output) {
    std::set<std::pair<int64_t, std::size_t>> sounds;
    for (const auto& note : output.notes)
        for (std::size_t i = 0; i < note_audio_asset_count(note); ++i)
            sounds.emplace(note.start_sample, note_audio_asset_at(note, i));
    for (const auto& cue : output.audio_cues) sounds.emplace(cue.start_sample, cue.asset_id);
    std::size_t missing = 0;
    for (const auto& note : source.notes)
        for (std::size_t i = 0; i < note_audio_asset_count(note); ++i)
            missing += sounds.count({note.start_sample, note_audio_asset_at(note, i)}) == 0;
    return missing;
}
}

static void check_feedback_matrix(const std::vector<KeyModeConversionAlgorithm>& algorithms) {
    std::size_t failures = 0;
    std::vector<std::pair<int, int>> matrix{{10, 4}};
    for (int source : {5, 16})
        for (int target : {4, 5, 6, 7, 8, 9, 10, 12, 14, 16})
            if (source != target) matrix.emplace_back(source, target);
    for (auto algorithm : algorithms) {
        for (auto preset : {Nk2Preset::Native, Nk2Preset::Transform, Nk2Preset::Remaster}) {
            if (algorithm != KeyModeConversionAlgorithm::NK2 && preset != Nk2Preset::Native) continue;
            for (const auto counts : matrix) {
                std::cout << "algorithm=" << static_cast<int>(algorithm) << " preset=" << static_cast<int>(preset)
                          << " source=" << counts.first << " target=" << counts.second << '\n';
                const auto source = feedback_chart(counts.first);
                KeyModeConverterOptions options;
                options.target_lane_count = counts.second;
                options.algorithm = algorithm;
                options.nk2_preset = preset;
                options.seed = 42;
                auto result = convert_key_mode_chart(source, options);
                REQUIRE(result.converted);
                check_no_overlap(result.chart);
                const auto missing = missing_sounds(source, result.chart);
                failures += missing;
                int lost_holds = 0;
                for (const auto& note : source.notes) {
                    if (!note.end_sample) continue;
                    lost_holds += !std::any_of(result.chart.notes.begin(), result.chart.notes.end(), [&](const auto& other) {
                        return other.note_id == note.note_id && other.start_sample == note.start_sample &&
                               other.end_sample == note.end_sample;
                    });
                }
                failures += lost_holds;
                std::cout << "missing_sounds=" << missing << " missing_holds=" << lost_holds << '\n';
            }
        }
    }
    CHECK(failures == 0);
}

static void check_feedback_bms(const std::vector<KeyModeConversionAlgorithm>& algorithms) {
    // A file supplied by a reporter can be exercised without bundling their chart/audio.
    // The default fixture covers the same adjacent-measure LNOBJ construction.
    std::string content = "#TITLE LNOBJ regression\n#BPM 173\n#LNTYPE 1\n#LNOBJ ZZ\n"
        "#00015:00\n#00114:000000000000000003000000000000ZZ\n"
        "#04211:HF0000000000000000000000000000ZZ\n"
        "#04212:FC0000000000000000000000000000ZZ\n"
        "#04312:HG0000000000000000000000000000ZZ\n"
        "#04313:GN0000000000000000000000000000ZZ\n";
    const char* external = std::getenv("TENRIFF_BMS_FEEDBACK_CHART");
    if (external) {
        std::ifstream in(std::filesystem::u8path(external), std::ios::binary);
        REQUIRE(in.good());
        content.assign(std::istreambuf_iterator<char>(in), {});
    }
    tenriff::chart::BmsParser parser;
    const auto parsed = parser.parse(content);
    REQUIRE(parsed.success());
    const auto normalized = tenriff::chart::BmsChartNormalizer{}.normalize(parsed.chart);
    REQUIRE(normalized.success());
    const auto timed = tenriff::chart::BmsTimelineBuilder{}.build(normalized.chart, 44100);
    REQUIRE(timed.success());
    auto built = tenriff::app::build_bms_gameplay_chart(timed.timeline, parsed.chart, 1.0);
    for (std::size_t i = 0; i < built.chart.notes.size(); ++i) built.chart.notes[i].note_id = i;
    REQUIRE(built.messages.empty());
    const auto holds = std::count_if(built.chart.notes.begin(), built.chart.notes.end(), [](const auto& n) { return n.end_sample.has_value(); });
    CHECK(holds == (external ? 21 : 5));
    std::cout << "BMS native notes=" << built.chart.notes.size() << " holds=" << holds << '\n';
    std::size_t failures = 0;
    for (auto algorithm : algorithms) {
        for (int target : {4, 6, 7, 8, 9, 10, 12, 14, 16}) {
            std::cout << "algorithm=" << static_cast<int>(algorithm) << " target=" << target << '\n';
            KeyModeConverterOptions options;
            options.target_lane_count = target;
            options.algorithm = algorithm;
            options.seed = 42;
            options.base_bpm = 173;
            const auto result = convert_key_mode_chart(built.chart, options);
            REQUIRE(result.converted);
            check_no_overlap(result.chart);
            int retained = 0;
            for (const auto& note : built.chart.notes) {
                if (!note.end_sample) continue;
                const bool found = std::any_of(result.chart.notes.begin(), result.chart.notes.end(), [&](const auto& other) {
                    return other.note_id == note.note_id && other.start_sample == note.start_sample && other.end_sample == note.end_sample;
                });
                failures += !found;
                retained += found;
            }
            std::cout << "BMS algorithm=" << static_cast<int>(algorithm) << " target=" << target
                      << " notes=" << result.chart.notes.size() << " retained_holds=" << retained << '/' << holds << '\n';
        }
    }
    CHECK(failures == 0);
}

static void check_feedback_chords(const std::vector<KeyModeConversionAlgorithm>& algorithms) {
    GameplayChart source;
    source.lane_count = 16;
    source.duration_samples = 44100;
    for (int i = 0; i < 16; ++i) {
        NoteEvent note;
        note.lane = i + 1;
        note.note_id = i;
        note.end_sample = 22050;
        note.audio_asset_id = source.intern_audio_asset(std::to_string(i));
        source.notes.push_back(note);
    }
    for (auto algorithm : algorithms) {
        KeyModeConverterOptions options;
        options.target_lane_count = 4;
        options.algorithm = algorithm;
        const auto result = convert_key_mode_chart(source, options);
        REQUIRE(result.converted);
        CHECK(result.chart.notes.size() == 4);
        check_no_overlap(result.chart);
        CHECK(missing_sounds(source, result.chart) == 0);
        for (const auto& note : result.chart.notes) CHECK(note.end_sample == 22050);
    }
}

static void check_feedback_replay(const std::vector<const char*>& algorithms) {
    const auto source = feedback_chart(10);
    for (const auto algorithm : algorithms) {
        for (int version : {1, 2, 3}) {
            tenriff::config::ModeConfig mode;
            mode.key_mode = "4k"; mode.key_conversion_algorithm = algorithm; mode.random_seed = 42;
            mode.key_conversion_nk2_preset = "native"; // Historical replay omits the preset field.
            mode.gauge = "normal";
            const auto managed = tenriff::app::manage_modes(source, tenriff::app::ChartFormat::Bms,
                mode, {}, 1, 120, 44100, tenriff::app::LongNoteMixVersion::Musical, version);
            // A failed model load must not turn this into a self-consistent native replay test.
            REQUIRE(managed.chart.lane_count == 4);
            auto chart = managed.chart;
            offset_gameplay_chart_samples(chart, 132300);
            GameplayConfig config;
            config.sample_rate = 44100;
            config.gauge_shift_enabled = true;
            config.judge = managed.judge;
            GameplayEngine engine(chart, config);
            std::vector<ReplayEvent> events;
            for (const auto& note : chart.notes) {
                events.push_back({note.lane, tenriff::input::InputState::Pressed, note.start_sample});
                events.push_back({note.lane, tenriff::input::InputState::Released, note.end_sample.value_or(note.start_sample) + 1});
            }
            std::stable_sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.sample < b.sample; });
            for (const auto& e : events) {
                engine.advance(e.sample - 1);
                (void)engine.handle_input(e.lane, e.state, e.sample);
            }
            engine.advance(chart.duration_samples + 240000);
            ReplayFile replay;
            replay.chart_sha256 = std::string(64, 'a'); replay.chart_format = "bms";
            replay.ruleset_id = tenriff::app::kCanonicalReplayRulesetId;
            replay.sample_rate = 44100;
            replay.mode.key_mode = mode.key_mode; replay.mode.random_seed = 42;
            replay.mode.key_conversion_algorithm = algorithm; replay.mode.key_conversion_version = version;
            replay.mode.gauge = mode.gauge;
            replay.mods = managed.active_mods; replay.rate_multiplier = managed.rate_multiplier;
            replay.score_multiplier = managed.final_multiplier;
            replay.trace = engine.replay(); replay.stats = engine.stats();
            replay.final_score = scale_native_score(replay.stats.raw_score, managed.final_multiplier);
            const auto verified = tenriff::app::verify_replay_against_chart(replay, source, tenriff::app::ChartFormat::Bms, 120);
            std::cout << "replay algorithm=" << algorithm << " version=" << version << " verified=" << verified.verified()
                      << " claims=" << verified.claims_match << '\n';
            CHECK(verified.verified());
            CHECK(verified.claims_match);
        }
    }
}

TEST_CASE("feedback regression converters preserve full source holds and every keysound") {
    check_feedback_matrix({KeyModeConversionAlgorithm::Krrcream, KeyModeConversionAlgorithm::NK2});
}

TEST_CASE("NK3 feedback regression converters preserve full source holds and every keysound") {
    check_feedback_matrix({KeyModeConversionAlgorithm::NK3});
}

TEST_CASE("feedback regression real BMS LNOBJ heads survive native load and key conversion") {
    check_feedback_bms({KeyModeConversionAlgorithm::Krrcream, KeyModeConversionAlgorithm::NK2});
}

TEST_CASE("NK3 feedback regression real BMS LNOBJ heads survive native load and key conversion") {
    check_feedback_bms({KeyModeConversionAlgorithm::NK3});
}

TEST_CASE("feedback regression impossible chords preserve sound without cutting occupied holds") {
    check_feedback_chords({KeyModeConversionAlgorithm::Krrcream, KeyModeConversionAlgorithm::NK2});
}

TEST_CASE("NK3 feedback regression impossible chords preserve sound without cutting occupied holds") {
    check_feedback_chords({KeyModeConversionAlgorithm::NK3});
}

TEST_CASE("feedback regression replay verifier reproduces legacy and repaired key conversion") {
    check_feedback_replay({"krrcream", "nk2"});
}

TEST_CASE("NK3 feedback regression replay verifier reproduces legacy and repaired key conversion") {
    check_feedback_replay({"nk3"});
}

TEST_CASE("DELTA native playback coalesces duplicated visible and long-note heads") {
    std::string content = "#TITLE Duplicate LN heads\n#BPM 173\n#LNTYPE 1\n"
        "#04211:GM\n#04213:LY\n#04215:FC\n#04251:GM0000GM\n"
        "#04312:GN\n#04352:GN0000GN\n";
    const char* external = std::getenv("TENRIFF_BMS_DELTA_CHART");
    if (external) {
        std::ifstream in(std::filesystem::u8path(external), std::ios::binary);
        REQUIRE(in.good());
        content.assign(std::istreambuf_iterator<char>(in), {});
    }
    const auto parsed = tenriff::chart::BmsParser{}.parse(content);
    REQUIRE(parsed.success());
    const auto normalized = tenriff::chart::BmsChartNormalizer{}.normalize(parsed.chart);
    REQUIRE(normalized.success());
    const auto timeline = tenriff::chart::BmsTimelineBuilder{}.build(normalized.chart, 44100);
    REQUIRE(timeline.success());
    const auto built = tenriff::app::build_bms_gameplay_chart(timeline.timeline, parsed.chart, 1.0);
    REQUIRE(built.messages.empty());
    const auto source_holds = std::count_if(built.chart.notes.begin(), built.chart.notes.end(),
        [](const auto& n) { return n.end_sample.has_value(); });
    tenriff::config::ModeConfig mode;
    mode.key_mode = "native";
    for (const int version : {1, 2, 3}) {
        const auto managed = tenriff::app::manage_modes(built.chart, tenriff::app::ChartFormat::Bms,
            mode, {}, 1.0, 173.0, 44100, tenriff::app::LongNoteMixVersion::Musical, version);
        const auto& notes = managed.chart.notes;
        const auto holds = std::count_if(notes.begin(), notes.end(), [](const auto& n) { return n.end_sample.has_value(); });
        std::size_t duplicate_heads = 0;
        for (const auto& n : notes) {
            if (n.end_sample) continue;
            duplicate_heads += std::any_of(notes.begin(), notes.end(), [&](const auto& hold) {
                return hold.end_sample && hold.lane == n.lane && hold.start_sample == n.start_sample;
            });
        }
        std::cout << "DELTA version=" << version << " source_notes=" << built.chart.notes.size()
                  << " notes=" << notes.size() << " holds=" << holds << " duplicate_heads=" << duplicate_heads << '\n';
        CHECK(holds == source_holds);
        if (version < 3) {
            CHECK(notes.size() == built.chart.notes.size());
            CHECK(duplicate_heads == (external ? 35u : 2u));
        } else {
            CHECK(duplicate_heads == 0);
            CHECK(notes.size() + (external ? 35u : 2u) == built.chart.notes.size());
            for (const auto& hold : built.chart.notes) {
                if (!hold.end_sample) continue;
                CHECK(std::any_of(notes.begin(), notes.end(), [&](const auto& n) {
                    return n.lane == hold.lane && n.start_sample == hold.start_sample && n.end_sample == hold.end_sample;
                }));
            }
        }
    }
}

TEST_CASE("DELTA correction keeps nearby notes other lanes tails and authored hold identity") {
    GameplayChart source;
    source.bms_rank = 3;
    source.lane_count = 5;
    source.notes = {{1, 1000, std::nullopt}, {1, 1000, 2000, true, kInvalidAudioAssetId, 71},
                    {2, 1000, std::nullopt}, {1, 999, std::nullopt}, {1, 1001, std::nullopt},
                    {1, 2000, std::nullopt}, {3, 3000, 3000}, {3, 3000, std::nullopt}};
    auto fixed = source;
    CHECK(coalesce_bms_long_note_heads(fixed) == 1);
    REQUIRE(fixed.notes.size() == 7);
    CHECK(fixed.notes.front().note_id == 71);
    CHECK(fixed.notes.front().end_sample == 2000);
    CHECK(fixed.notes.front().release_required);
    CHECK(coalesce_bms_long_note_heads(fixed) == 0);
    source.bms_rank.reset();
    CHECK(coalesce_bms_long_note_heads(source) == 0);
    CHECK(source.notes.size() == 8);
}

TEST_CASE("DELTA correction keeps distinct keysounds overflow and separate sound gains") {
    GameplayChart source;
    source.bms_rank = 3;
    source.lane_count = 5;
    for (int i = 0; i < 8; ++i) (void)source.intern_audio_asset(std::to_string(i));
    source.notes.push_back({1, 1000, 2000, false, 0, 77});
    for (int i = 0; i < 7; ++i) source.notes.push_back({1, 1000, std::nullopt, false, static_cast<std::size_t>(i), static_cast<std::size_t>(i)});
    source.notes.push_back({1, 1000, std::nullopt, false, 7, 8, 0.25f});
    auto fixed = source;
    CHECK(coalesce_bms_long_note_heads(fixed) == 8);
    REQUIRE(fixed.notes.size() == 1);
    CHECK(fixed.notes.front().note_id == 77);
    CHECK(note_audio_asset_count(fixed.notes.front()) == 4);
    CHECK(missing_sounds(source, fixed) == 0);
    CHECK(fixed.audio_cues.size() == 4);
    CHECK(std::any_of(fixed.audio_cues.begin(), fixed.audio_cues.end(), [](const auto& cue) {
        return cue.asset_id == 7 && cue.start_sample == 1000 && cue.gain == 0.25f;
    }));
}

TEST_CASE("DELTA one input press starts the hold instead of consuming a duplicate tap") {
    GameplayChart source;
    source.bms_rank = 3;
    source.lane_count = 5;
    source.duration_samples = 3000;
    source.notes = {{1, 1000, std::nullopt}, {1, 1000, 2000}};
    tenriff::config::ModeConfig mode;
    mode.key_mode = "native";
    for (int version : {1, 2, 3}) {
        auto fixed = tenriff::app::manage_modes(source, tenriff::app::ChartFormat::Bms, mode, {},
            1, 173, 1000, tenriff::app::LongNoteMixVersion::Musical, version);
        GameplayConfig config;
        config.sample_rate = 1000;
        config.judge = fixed.judge;
        GameplayEngine engine(fixed.chart, config);
        const auto head = engine.handle_input(1, tenriff::input::InputState::Pressed, 1000);
        REQUIRE(head.has_value());
        CHECK(head->end_sample.has_value() == (version == 3));
        engine.advance(2000);
        (void)engine.handle_input(1, tenriff::input::InputState::Released, 2001);
        engine.advance(4000);
        if (version == 3) {
            CHECK(engine.stats().total_notes == 1);
            CHECK(engine.stats().counts.bd == 0);
            CHECK(engine.stats().counts.pr == 0);
            CHECK(engine.stats().accuracy_percent() == doctest::Approx(100.0));
        } else {
            CHECK(engine.stats().total_notes == 2);
            CHECK(engine.stats().counts.bd + engine.stats().counts.pr > 0);
        }
    }
}

static void check_delta_conversions(const std::vector<KeyModeConversionAlgorithm>& algorithms) {
    // Sparse source: a duplicate head must not become an extra converted chord.
    // IDs deliberately have gaps to catch provenance errors after coalescing.
    GameplayChart source;
    source.bms_rank = 3;
    source.lane_count = 5;
    source.duration_samples = 44100 * 4;
    const auto sound = source.intern_audio_asset("hold.wav");
    source.notes = {{1, 44100, std::nullopt, false, sound, 7},
                    {1, 44100, 88200, false, sound, 99}};
    for (auto algorithm : algorithms) {
        for (int target : {4, 5, 6, 8, 10, 16}) {
            KeyModeConverterOptions options;
            options.algorithm = algorithm;
            options.target_lane_count = target;
            options.seed = 42;
            options.conversion_version = 3;
            auto normalized = source;
            REQUIRE(coalesce_bms_long_note_heads(normalized) == 1);
            const auto expected = convert_key_mode_chart(normalized, options);
            const auto actual = convert_key_mode_chart(source, options);
            CHECK(actual.converted == expected.converted);
            if (target != source.lane_count) REQUIRE(actual.converted);
            CHECK(actual.chart.notes.size() == expected.chart.notes.size());
            CHECK(missing_sounds(source, actual.chart) == 0);
            CHECK(std::any_of(actual.chart.notes.begin(), actual.chart.notes.end(), [](const auto& n) {
                return n.note_id == 99 && n.start_sample == 44100 && n.end_sample == 88200;
            }));
            CHECK(std::none_of(actual.chart.notes.begin(), actual.chart.notes.end(), [](const auto& n) { return n.note_id == 7; }));
            std::cout << "DELTA conversion algorithm=" << static_cast<int>(algorithm) << " target=" << target
                      << " notes=" << actual.chart.notes.size() << " hold_id=99 sounds=preserved\n";
        }
    }
}

TEST_CASE("DELTA host conversions normalize duplicate heads before remapping") {
    check_delta_conversions({KeyModeConversionAlgorithm::Krrcream, KeyModeConversionAlgorithm::NK2});
}

TEST_CASE("NK3 feedback regression DELTA converts corrected long-note heads") {
    check_delta_conversions({KeyModeConversionAlgorithm::NK3});
}
