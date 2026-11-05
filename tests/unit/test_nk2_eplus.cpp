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
#include "app/menu/settings/ModeSettingsController.h"
#include "app/RuntimeConfigMigration.h"
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

GameplayChart eplus_fixture(int keys, bool holds = true) {
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

KeyModeConverterOptions eplus_options(int target) {
    KeyModeConverterOptions opts;
    opts.algorithm = KeyModeConversionAlgorithm::NK2;
    opts.nk2_preset = Nk2Preset::FullFieldTapsEPlus;
    opts.target_lane_count = target;
    opts.sample_rate = 44100;
    opts.base_bpm = 120;
    return opts;
}

void check_eplus_contract(const GameplayChart& source, const GameplayChart& output) {
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

            CHECK(note_audio_asset_count(n) == 0);
            if (!n.end_sample) {
                CHECK(std::any_of(output.notes.begin(), output.notes.end(), [&](const auto& retained) {
                    return original.count(retained.note_id) && retained.start_sample == n.start_sample;
                }));
            }
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


TEST_CASE("NK2 E+ 8K and 10K preserve exact samples LN semantics keysounds and deterministic identity") {
    for (int source_keys : {1,4,7,8,10,14,18}) for (int target : {8,10}) for (bool holds : {false,true}) {
        auto chart = eplus_fixture(source_keys, holds);
        // A one-key fixture needs room for its original LNs.
        if (source_keys == 1) for (auto& note : chart.notes)
            if (note.end_sample) note.end_sample = note.start_sample + 11025;
        const auto input = signature(chart);
        const auto options = eplus_options(target);
        const auto converted = convert_key_mode_chart(chart, options);
        if (!converted.converted) for (const auto& warning : converted.warnings) std::cerr << warning << '\n';
        REQUIRE(converted.converted);
        REQUIRE(converted.chart.lane_count == target);
        check_eplus_contract(chart, converted.chart);
        CHECK(signature(chart) == input);
        CHECK(signature(converted.chart) == signature(convert_key_mode_chart(chart, options).chart));
        if (!holds) {
            CHECK(converted.chart.notes.size() > chart.notes.size());
            CHECK(std::none_of(converted.chart.notes.begin(), converted.chart.notes.end(), [](const auto& n) {return n.end_sample.has_value();}));
        }
    }
}

TEST_CASE("NK2 E+ keeps full field for grouped charts and applies explicit equal-key remix") {
    for (int target : {8,10}) {
        auto chart = eplus_fixture(target, false);
        chart.lane_group_size = target / 2;
        ModeSettings settings;
        settings.key_mode = *parse_key_mode(std::to_string(target)+"k");
        settings.key_conversion_algorithm = KeyModeConversionAlgorithm::NK2;
        settings.key_conversion_nk2_preset = Nk2Preset::FullFieldTapsEPlus;
        const auto applied = apply_mode_settings(chart, settings, {120,44100,3});
        REQUIRE(applied.chart.lane_count == target);
        CHECK(applied.chart.notes.size() > chart.notes.size());
        CHECK(applied.chart.lane_group_size == 0);
        check_eplus_contract(chart, applied.chart);
        settings.key_mode = KeyMode::Auto;
        CHECK(signature(apply_mode_settings(chart,settings,{120,44100,3}).chart) == signature(chart));
    }
}

TEST_CASE("NK2 E+ excludes BMS scratches and keeps their exact autoplay keysounds") {
    for (int source_keys : {8,16}) for (int target : {8,10}) {
        auto chart=eplus_fixture(source_keys,false);
        chart.scratch_lanes=source_keys==8 ? std::vector<int>{1} : std::vector<int>{8,16};
        chart.lane_group_size=source_keys==16 ? 8 : 0;
        ModeSettings settings;
        settings.key_mode=*parse_key_mode(std::to_string(target)+"k");
        settings.key_conversion_algorithm=KeyModeConversionAlgorithm::NK2;
        settings.key_conversion_nk2_preset=Nk2Preset::FullFieldTapsEPlus;
        const auto result=apply_mode_settings(chart,settings,{120,44100,3});
        REQUIRE(result.chart.lane_count==target);
        CHECK(result.chart.scratch_lanes.empty());
        CHECK(result.chart.lane_group_size==0);
        // Scratch extraction has already removed those identities before the
        // converter allocates generated IDs. Validate against retained keys.
        auto keys=chart;
        keys.notes.erase(std::remove_if(keys.notes.begin(),keys.notes.end(),[&](const auto& n){
            return std::find(chart.scratch_lanes.begin(),chart.scratch_lanes.end(),n.lane)!=chart.scratch_lanes.end();
        }),keys.notes.end());
        check_eplus_contract(keys,result.chart);
        for(const auto& note : chart.notes) {
            if(std::find(chart.scratch_lanes.begin(),chart.scratch_lanes.end(),note.lane)==chart.scratch_lanes.end()) continue;
            CHECK(std::none_of(result.chart.notes.begin(),result.chart.notes.end(),[&](const auto& n){return n.start_sample==note.start_sample;}));
            CHECK(std::any_of(result.chart.audio_cues.begin(),result.chart.audio_cues.end(),[&](const auto& cue){
                return cue.start_sample==note.start_sample && cue.asset_id==0;
            }));
        }
    }
}

TEST_CASE("NK2 E+ rejects unsupported targets and unrepresentable holds without legacy fallback") {
    auto source = eplus_fixture(7);
    for (int target : {4,5,6,7,9,12,14,16}) {
        const auto converted = convert_key_mode_chart(source,eplus_options(target));
        CHECK_FALSE(converted.converted);
        CHECK(signature(converted.chart)==signature(source));
        ModeSettings settings;
        settings.key_mode=*parse_key_mode(std::to_string(target)+"k");
        settings.key_conversion_algorithm=KeyModeConversionAlgorithm::NK2;
        settings.key_conversion_nk2_preset=Nk2Preset::FullFieldTapsEPlus;
        const auto applied=apply_mode_settings(source,settings,{120,44100,3});
        CHECK(signature(applied.chart)==signature(source));
    }
    source.notes[0].end_sample=source.notes[0].start_sample+1;
    for(int target : {8,10}) CHECK_FALSE(convert_key_mode_chart(source,eplus_options(target)).converted);
}

TEST_CASE("NK2 E+ selection is available only in 8K and 10K without changing the default E") {
    using namespace tenriff;
    using namespace app::menu;
    using namespace app::menu::settings;
    CHECK(config::RuntimeConfig{}.mode.key_conversion_nk2_preset=="source_ln_e_v1");
    CHECK(parse_nk2_preset("E+")==Nk2Preset::FullFieldTapsEPlus);
    CHECK(parse_nk2_preset("fullfield_taps_eplus_v1")==Nk2Preset::FullFieldTapsEPlus);
    CHECK(to_string(Nk2Preset::FullFieldTapsEPlus)=="fullfield_taps_eplus_v1");
    for (auto mode : {"none","4k","7k","8k","9k","10k","12k","14k","16k"}) {
        config::RuntimeConfig runtime;
        runtime.mode.key_mode=mode;
        runtime.mode.key_conversion_algorithm="nk2";
        ModeSettingsController controller;
        controller.handle(MenuAction::adjust(1),runtime,ModeSettingId::Nk2Preset);
        const bool supported=std::string(mode)=="8k" || std::string(mode)=="10k";
        CHECK(runtime.mode.key_conversion_nk2_preset==(supported?"fullfield_taps_eplus_v1":"native"));
        if (supported) {
            controller.handle(MenuAction::adjust(1),runtime,ModeSettingId::KeyMode);
            CHECK(runtime.mode.key_conversion_nk2_preset=="source_ln_e_v1");
        }
    }
    CHECK(app::normalize_key_conversion_nk2_preset("E+")=="fullfield_taps_eplus_v1");
}

TEST_CASE("NK2 E+ honors nondefault sample rates without duplicated generated keysounds") {
    for (int rate : {22050,48000,96000}) for (int target : {8,10}) {
        auto chart=eplus_fixture(4,false);
        for (auto& n : chart.notes) n.start_sample=17+static_cast<int64_t>(n.note_id-10000)*rate/2;
        chart.duration_samples=40LL*rate;
        auto options=eplus_options(target); options.sample_rate=rate;
        const auto converted=convert_key_mode_chart(chart,options);
        REQUIRE(converted.converted);
        check_eplus_contract(chart,converted.chart);
    }
}
TEST_CASE("NK2 E+ R13 and R14 saved replays verify and serialize Sites identity for 8K and 10K") {
    using namespace tenriff;
    using input::InputState;
    auto source = eplus_fixture(7);
    source.bms_rank = 2;
    const auto directory = std::filesystem::temp_directory_path() /
        ("tenriff-nk2-eplus-replay-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(std::filesystem::create_directory(directory));
    for (int target : {8,10}) for (bool ex : {false,true}) {
        config::ModeConfig mode;
        mode.key_mode = std::to_string(target) + "k";
        mode.key_conversion_algorithm = "nk2";
        mode.key_conversion_nk2_preset = "fullfield_taps_eplus_v1";
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
        replay.ruleset_id = std::string(ex ? app::kExReplayRulesetId : app::kCanonicalReplayRulesetId);
        replay.sample_rate = 44100;
        replay.created_utc = "20261105_000000Z";
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
        const auto path = directory / (std::to_string(target) + (ex ? "-r14.json" : "-r13.json"));
        REQUIRE(save_replay_json(path.u8string(), replay).success());
        const auto loaded = load_replay_json(path.u8string());
        REQUIRE(loaded.success());
        CHECK(loaded.replay->mode.key_conversion_nk2_preset == "fullfield_taps_eplus_v1");
        const auto verified = app::verify_replay_against_chart(*loaded.replay, source, app::ChartFormat::Bms, 120);
        if (!verified.verified() || !verified.claims_match) std::cerr << "NK2 E+ replay " << target << ": " << verified.detail << '\n';
        REQUIRE(verified.verified());
        CHECK(verified.claims_match);
        CHECK(verified.official_eligible);
        std::string payload, error;
        const auto replay_hash = app::hash_chart_file(path);
        REQUIRE(replay_hash.valid());
        const bool serialized = app::build_sites_score_json(*loaded.replay, replay_hash.sha256, "NK2 E+ QA", verified.clear_status, payload, error);
        if (!serialized) std::cerr << "NK2 E+ Sites " << target << ": " << error << '\n';
        REQUIRE(serialized);
        CHECK(payload.find("nk2:fullfield_taps_eplus_v1") != std::string::npos);
        // Optional local evidence export: no production request or user account.
        if (const char* out = std::getenv("TENRIFF_NK2_EPLUS_PAYLOAD_DIR"); out && *out) {
            const auto output = std::filesystem::path(out);
            std::filesystem::create_directories(output);
            std::ofstream file(output / (std::to_string(target) + (ex ? "-r14.json" : "-r13.json")));
            file << payload << '\n';
            REQUIRE(file.good());
        }
    }
    std::filesystem::remove_all(directory);
}


TEST_CASE("NK2 E+ profile roundtrip and migration retain its own identity") {
    using namespace tenriff;
    const auto dir=std::filesystem::temp_directory_path() /
        ("tenriff-nk2-eplus-profile-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(std::filesystem::create_directory(dir));
    config::RuntimeConfig config;
    config.mode.key_mode="8k";
    config.mode.key_conversion_algorithm="nk2";
    config.mode.key_conversion_nk2_preset="E+";
    CHECK(app::migrate_bms_first_runtime_config(config));
    CHECK(config.mode.key_conversion_nk2_preset=="fullfield_taps_eplus_v1");
    config::ConfigLoader loader;
    std::string error;
    REQUIRE(loader.save_profile(dir.u8string(),config,&error));
    const auto loaded=loader.load_profile(dir.u8string());
    REQUIRE(loaded.success());
    CHECK(loaded.config.mode.key_mode=="8k");
    CHECK(loaded.config.mode.key_conversion_nk2_preset=="fullfield_taps_eplus_v1");
    std::filesystem::remove_all(dir);
}
