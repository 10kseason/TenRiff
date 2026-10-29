#include "doctest/doctest.h"
#include "gameplay/KeyModeConverter.h"
#include "gameplay/ModeApplier.h"
#include "app/ModeManager.h"
#include "app/ChartLoader.h"
#include "app/ChartFileHash.h"
#include <cstdlib>
#include <fstream>
#include "app/ReplayVerifier.h"
#include "app/SitesLeaderboardClient.h"
#include "app/MenuAppSettingsUtils.h"
#include "config/Config.h"
#include "gameplay/GameplayEngine.h"
#include "gameplay/Replay.h"
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <map>
#include <iostream>
#include <set>
#include <tuple>

namespace {
using namespace tenriff::gameplay;

GameplayChart e_fixture(int keys, bool holds = true) {
    GameplayChart chart;
    chart.lane_count = keys;
    chart.audio_assets.push_back({"fixture.wav"});
    for (int i = 0; i < 64; ++i) {
        NoteEvent note;
        note.note_id = 10000 + i;
        note.lane = i % keys + 1;
        note.start_sample = 13 + static_cast<int64_t>(i) * 22050;
        if (holds && i % 8 == 0) {
            note.end_sample = note.start_sample + 39690;
            note.release_required = true;
        }
        note.add_audio_asset(0);
        chart.notes.push_back(note);
    }
    chart.duration_samples = 1500000;
    return chart;
}

auto signature(const GameplayChart& chart) {
    std::vector<std::tuple<int,int64_t,std::optional<int64_t>,std::size_t,bool,std::size_t>> out;
    for (const auto& n : chart.notes)
        out.emplace_back(n.lane, n.start_sample, n.end_sample, n.note_id, n.release_required, note_audio_asset_count(n));
    return out;
}

KeyModeConverterOptions e_options(int target) {
    KeyModeConverterOptions opts;
    opts.algorithm = KeyModeConversionAlgorithm::NK2;
    opts.nk2_preset = Nk2Preset::SourceLnE;
    opts.target_lane_count = target;
    opts.sample_rate = 44100;
    opts.base_bpm = 120;
    return opts;
}

void check_e_contract(const GameplayChart& source, const GameplayChart& output) {
    std::map<std::size_t, const NoteEvent*> original;
    for (const auto& n : source.notes) original[n.note_id] = &n;
    std::map<int,int64_t> previous_end;
    std::set<std::size_t> ids;
    for (const auto& n : output.notes) {
        CHECK(n.lane >= 1);
        CHECK(n.lane <= output.lane_count);
        CHECK(ids.insert(n.note_id).second);
        const auto end = previous_end.find(n.lane);
        if (end != previous_end.end()) CHECK(n.start_sample > end->second);
        previous_end[n.lane] = n.end_sample.value_or(n.start_sample);
        const auto found = original.find(n.note_id);
        if (found != original.end()) {
            CHECK(n.start_sample == found->second->start_sample);
            CHECK(n.end_sample == found->second->end_sample);
            CHECK(n.release_required == found->second->release_required);
            CHECK(note_audio_asset_count(n) == note_audio_asset_count(*found->second));
        } else {
            CHECK(output.lane_count > source.lane_count);
            CHECK(note_audio_asset_count(n) == 0);
            if (n.end_sample) {
                CHECK(*n.end_sample > n.start_sample);
                CHECK(std::any_of(source.notes.begin(), source.notes.end(), [&](const auto& old) {
                    return old.end_sample && old.start_sample <= n.start_sample && *old.end_sample >= *n.end_sample;
                }));
            }
        }
    }
    // The runtime deduplicates equal timestamp/asset voices across source lanes.
    for (const auto& old : source.notes) for (std::size_t i = 0; i < note_audio_asset_count(old); ++i) {
        const auto asset = note_audio_asset_at(old, i);
        CHECK((std::any_of(output.notes.begin(), output.notes.end(), [&](const auto& n) {
            if (n.start_sample != old.start_sample) return false;
            for (std::size_t j = 0; j < note_audio_asset_count(n); ++j)
                if (note_audio_asset_at(n, j) == asset) return true;
            return false;
        }) || std::any_of(output.audio_cues.begin(), output.audio_cues.end(), [&](const auto& cue) {
            return cue.start_sample == old.start_sample && cue.asset_id == asset;
        })));
    }
}
}

TEST_CASE("NK2 E all 100 menu source-target pairs preserve deterministic sample and LN contracts") {
    for (int source_keys : {4,5,6,7,8,9,10,12,14,16}) {
        const auto chart = e_fixture(source_keys);
        for (int target : {4,5,6,7,8,9,10,12,14,16}) {

            const auto options = e_options(target);
            const auto a = convert_key_mode_chart(chart, options);
            if (a.chart.lane_count != target) {
                std::cerr << "NK2 E pair " << source_keys << "->" << target << ": ";
                for (const auto& warning : a.warnings) std::cerr << warning << "\n";
            }
            REQUIRE(a.chart.lane_count == target);
            CHECK(a.converted == (source_keys != target));
            check_e_contract(chart, a.chart);
            auto other_seed = options;
            other_seed.seed = 0x1234abcd;
            const auto b = convert_key_mode_chart(chart, other_seed);
            CHECK(signature(a.chart) == signature(b.chart));
            if (target == source_keys) CHECK(signature(a.chart) == signature(chart));
        }
    }
}

TEST_CASE("NK2 E adds silent covered LNs and never invents holds in a tap-only source") {
    const auto source = e_fixture(4);
    const auto result = convert_key_mode_chart(source, e_options(16));
    REQUIRE(result.converted);
    CHECK(std::any_of(result.chart.notes.begin(), result.chart.notes.end(), [](const auto& n) {
        return n.note_id >= 10064 && n.end_sample.has_value();
    }));
    const auto taps = e_fixture(4, false);
    const auto tap_result = convert_key_mode_chart(taps, e_options(16));
    REQUIRE(tap_result.converted);
    CHECK(std::none_of(tap_result.chart.notes.begin(), tap_result.chart.notes.end(), [](const auto& n) {
        return n.end_sample.has_value();
    }));
    check_e_contract(taps, tap_result.chart);
}

TEST_CASE("NK2 E reduction drops overflow without retiming and retains omitted keysounds") {
    auto source = e_fixture(16, false);
    for (auto& n : source.notes) {
        n.start_sample = (n.note_id - 10000) / 16 * 88200 + 13;
        n.clear_audio_assets();
        n.add_audio_asset(source.intern_audio_asset(std::to_string(n.note_id) + ".wav"));
    }
    const auto result = convert_key_mode_chart(source, e_options(4));
    REQUIRE(result.converted);
    CHECK(result.chart.notes.size() < source.notes.size());
    check_e_contract(source, result.chart);
}

TEST_CASE("NK2 E existing menu selectors and config identify E separately from historical presets") {
    CHECK(tenriff::config::RuntimeConfig{}.mode.key_conversion_nk2_preset == "source_ln_e_v1");
    REQUIRE(parse_nk2_preset("E") == Nk2Preset::SourceLnE);
    CHECK(to_string(Nk2Preset::SourceLnE) == "source_ln_e_v1");
    CHECK(parse_nk2_preset("") == Nk2Preset::Native);
    CHECK(parse_nk2_preset("native") == Nk2Preset::Native);
    CHECK_FALSE(parse_key_mode("11K").has_value());
    CHECK_FALSE(parse_key_mode("13K").has_value());
    CHECK_FALSE(parse_key_mode("15K").has_value());
    for (int k : {4,5,6,7,8,9,10,12,14,16}) {
        const auto parsed = parse_key_mode(std::to_string(k) + "K");
        REQUIRE(parsed.has_value());
        CHECK(to_string(*parsed) == std::to_string(k) + "K");
        ModeSettings settings;
        settings.key_mode = *parsed;
        settings.key_conversion_algorithm = KeyModeConversionAlgorithm::NK2;
        settings.key_conversion_nk2_preset = Nk2Preset::SourceLnE;
        const auto applied = apply_mode_settings(e_fixture(7), settings, {120,44100,3});
        CHECK(applied.chart.lane_count == k);
    }
}

TEST_CASE("NK2 E converts DP as a complete field for every existing menu target") {
    auto chart = e_fixture(14);
    chart.lane_group_size = 7;
    for (int k : {4,5,6,7,8,9,10,12,14,16}) {
        ModeSettings settings;
        settings.key_mode = *parse_key_mode(std::to_string(k) + "k");
        settings.key_conversion_algorithm = KeyModeConversionAlgorithm::NK2;
        settings.key_conversion_nk2_preset = Nk2Preset::SourceLnE;
        const auto applied = apply_mode_settings(chart, settings, {120,44100,3});
        REQUIRE(applied.chart.lane_count == k);
        check_e_contract(chart, applied.chart);
    }
}

TEST_CASE("NK2 E R9 and R10 saved replays verify and serialize Sites identity for ten menu modes") {
    using namespace tenriff;
    using input::InputState;
    auto source = e_fixture(7);
    source.bms_rank = 2;
    const auto directory = std::filesystem::temp_directory_path() /
        ("tenriff-nk2-e-replay-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(std::filesystem::create_directory(directory));
    for (int target : {4,5,6,7,8,9,10,12,14,16}) for (bool ex : {false,true}) {
        config::ModeConfig mode;
        mode.key_mode = std::to_string(target) + "k";
        mode.key_conversion_algorithm = "nk2";
        mode.key_conversion_nk2_preset = "source_ln_e_v1";
        mode.random = "off";
        mode.gauge = "normal";
        if (ex) mode.mods = {"judge_ex"};
        const auto managed = app::manage_modes(source, app::ChartFormat::Bms, mode, {}, 1.0, 120, 44100);
        REQUIRE(managed.chart.lane_count == target);
        auto played = managed.chart;
        offset_gameplay_chart_samples(played, 132300);
        ReplayFile replay;
        replay.chart_format = "bms";
        replay.chart_sha256 = std::string(64, 'a');
        replay.bms_rank = source.bms_rank;
        replay.ruleset_id = std::string(ex ? app::kRuleset10ReplayRulesetId : app::kRuleset9ReplayRulesetId);
        replay.sample_rate = 44100;
        replay.created_utc = "20261022_000000Z";
        replay.record_play_mode = "online"; // Synthetic opt-in, never uploaded.
        replay.mods = managed.active_mods;
        replay.mode.key_mode = mode.key_mode;
        replay.mode.key_conversion_algorithm = mode.key_conversion_algorithm;
        replay.mode.key_conversion_nk2_preset = mode.key_conversion_nk2_preset;
        replay.mode.key_conversion_version = 3;
        replay.mode.bms_gauge_version = 2;
        replay.mode.gauge = "normal";
        replay.mode.random = "off";
        replay.score_multiplier = managed.final_multiplier;
        replay.rate_multiplier = managed.rate_multiplier;
        GameplayConfig config;
        config.sample_rate = 44100;
        config.judge = app::replay_judge_config_for_playback(replay, {}, source.bms_rank);
        config.strict_standard_hold_release = app::replay_uses_strict_standard_hold_release(replay);
        config.great_standard_hold_release = app::replay_uses_great_standard_hold_release(replay);
        config.gauge_shift_enabled = true;
        config.gauge_policy.bms_lr2_deltas = true;
        GameplayEngine engine(played, config);
        std::vector<ReplayEvent> events;
        for (const auto& n : played.notes) {
            events.push_back({n.lane, InputState::Pressed, n.start_sample});
            events.push_back({n.lane, InputState::Released, n.end_sample.value_or(n.start_sample + 1)});
        }
        std::stable_sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.sample < b.sample; });
        for (const auto& event : events) {
            engine.advance(event.sample - 1);
            (void)engine.handle_input(event.lane, event.state, event.sample);
        }
        engine.advance(played.duration_samples + 220500);
        replay.trace = engine.replay();
        replay.stats = engine.stats();
        replay.final_score = scale_native_score(replay.stats.raw_score, replay.score_multiplier);
        const auto path = directory / (std::to_string(target) + (ex ? "-r10.json" : "-r9.json"));
        REQUIRE(save_replay_json(path.u8string(), replay).success());
        const auto loaded = load_replay_json(path.u8string());
        REQUIRE(loaded.success());
        CHECK(loaded.replay->mode.key_conversion_nk2_preset == "source_ln_e_v1");
        const auto verified = app::verify_replay_against_chart(*loaded.replay, source, app::ChartFormat::Bms, 120);
        if (!verified.verified() || !verified.claims_match) std::cerr << "NK2 E replay " << target << ": " << verified.detail << '\n';
        REQUIRE(verified.verified());
        CHECK(verified.claims_match);
        CHECK(verified.official_eligible);
        std::string payload, error;
        const auto replay_hash = app::hash_chart_file(path);
        REQUIRE(replay_hash.valid());
        const bool serialized = app::build_sites_score_json(*loaded.replay, replay_hash.sha256, "NK2 E QA", verified.clear_status, payload, error);
        if (!serialized) std::cerr << "NK2 E Sites " << target << ": " << error << '\n';
        REQUIRE(serialized);
        CHECK(payload.find("nk2:source_ln_e_v1") != std::string::npos);
        // Optional local evidence export: no production request or user account.
        if (const char* out = std::getenv("TENRIFF_NK2_PAYLOAD_DIR"); out && *out) {
            const auto output = std::filesystem::path(out);
            std::filesystem::create_directories(output);
            std::ofstream file(output / (std::to_string(target) + (ex ? "-r10.json" : "-r9.json")));
            file << payload << '\n';
            REQUIRE(file.good());
        }
    }
    std::filesystem::remove_all(directory);
}

TEST_CASE("NK2 E rejects unrepresentable LNs and non-menu targets without fallback") {
    auto source = e_fixture(7);
    source.notes[0].end_sample = source.notes[0].start_sample + 1;
    const auto result = convert_key_mode_chart(source, e_options(10));
    CHECK_FALSE(result.converted);
    CHECK(signature(result.chart) == signature(source));
    CHECK_FALSE(result.warnings.empty());
    ModeSettings settings;
    settings.key_mode = *parse_key_mode("10k");
    settings.key_conversion_algorithm = KeyModeConversionAlgorithm::NK2;
    settings.key_conversion_nk2_preset = Nk2Preset::SourceLnE;
    const auto managed = apply_mode_settings(source, settings, {120,44100,3});
    CHECK(managed.chart.lane_count == source.lane_count);
    CHECK(signature(managed.chart) == signature(source));
    for (int k : {3,11,13,15,17}) {
        const auto unsupported = convert_key_mode_chart(e_fixture(7), e_options(k));
        CHECK_FALSE(unsupported.converted);
        CHECK(unsupported.chart.lane_count == 7);
    }
}

TEST_CASE("NK2 E local BMS fixtures convert across the ten existing menu modes") {
    using namespace tenriff;
    for (const char* variable : {"TENRIFF_BMS_TIMING_CHART", "TENRIFF_BMS_RANDOM_CHART"}) {
        const char* path = std::getenv(variable);
        if (!path || !*path) continue; // Private reporter charts are never shipped.
        const auto loaded = app::ChartLoader{}.load(path, 44100, 1.0, "ignore", 42, 2, 2);
        REQUIRE(loaded.success());
        REQUIRE(loaded.chart.notes.size() > 100);
        for (int k : {4,5,6,7,8,9,10,12,14,16}) {
            config::ModeConfig mode;
            mode.key_mode = std::to_string(k) + "k";
            mode.key_conversion_algorithm = "nk2";
            mode.key_conversion_nk2_preset = "source_ln_e_v1";
            const auto managed = app::manage_modes(loaded.chart, loaded.format, mode, {}, 1.0, loaded.base_bpm, 44100);
            std::cout << "NK2 E reporter " << variable << " target=" << k << " lanes=" << managed.chart.lane_count
                      << " notes=" << managed.chart.notes.size() << '\n';
            if (managed.chart.lane_count != k)
                for (const auto& warning : managed.warnings) std::cerr << warning << '\n';
            REQUIRE(managed.chart.lane_count == k);
            std::map<int,int64_t> ends;
            for (const auto& note : managed.chart.notes) {
                CHECK(note.lane >= 1);
                CHECK(note.lane <= k);
                const auto previous = ends.find(note.lane);
                if (k != loaded.chart.lane_count && previous != ends.end()) CHECK(note.start_sample > previous->second);
                ends[note.lane] = note.end_sample.value_or(note.start_sample);
            }
        }
    }
}
