#include "doctest/doctest.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

#include "app/MultiplayerMenuState.h"
#include "app/MultiplayerChartSearch.h"
#include "app/MenuSongUtils.h"
#include "app/PeerBattleRules.h"
#include "app/PeerBattleRuntimeRules.h"

using tenriff::app::MultiplayerMenuRow;
using tenriff::app::MultiplayerMenuState;
using tenriff::app::MultiplayerRole;
using tenriff::app::MultiplayerScoreOutcome;
using tenriff::app::PeerBattleSpectatorDecision;
using tenriff::app::PeerBattleSpectatorState;
using tenriff::app::GameplayLaunchKind;
using tenriff::app::clamp_multiplayer_menu_cursor;
using tenriff::app::compare_multiplayer_scores;
using tenriff::app::gameplay_launch_uses_peer_battle;
using tenriff::app::is_valid_multiplayer_address_text;
using tenriff::app::kMultiplayerAddressMaxLength;
using tenriff::app::kMultiplayerMenuRowCount;
using tenriff::app::move_multiplayer_menu_cursor;
using tenriff::app::multiplayer_chart_fingerprints_match;
using tenriff::app::multiplayer_chart_candidate_name_matches;
using tenriff::app::build_multiplayer_chart_candidates;
using tenriff::app::multiplayer_chart_path_for_source;
using tenriff::app::multiplayer_loaded_song_sources;
using tenriff::app::load_multiplayer_chart_candidates_from_profile_cache;
using tenriff::app::multiplayer_compensated_peer_begin_delay_ms;
using tenriff::app::multiplayer_host_can_choose_chart;
using tenriff::app::multiplayer_host_can_start;
using tenriff::app::multiplayer_ready_gate_open;
using tenriff::app::multiplayer_start_gate_open;
using tenriff::app::parse_multiplayer_port;
using tenriff::app::peer_battle_score_lead;
using tenriff::app::peer_battle_spectator_decision;
using tenriff::app::reset_multiplayer_menu_session;
using tenriff::app::try_append_multiplayer_address_character;
using tenriff::app::try_append_multiplayer_port_character;

static_assert(static_cast<int>(MultiplayerMenuRow::Address) == 0);
static_assert(static_cast<int>(MultiplayerMenuRow::Back) == kMultiplayerMenuRowCount - 1);

TEST_CASE("multiplayer menu exposes nine stable rows") {
    CHECK(kMultiplayerMenuRowCount == 9);
    CHECK(static_cast<int>(MultiplayerMenuRow::Address) == 0);
    CHECK(static_cast<int>(MultiplayerMenuRow::Port) == 1);
    CHECK(static_cast<int>(MultiplayerMenuRow::Host) == 2);
    CHECK(static_cast<int>(MultiplayerMenuRow::Join) == 3);
    CHECK(static_cast<int>(MultiplayerMenuRow::Chart) == 4);
    CHECK(static_cast<int>(MultiplayerMenuRow::Ready) == 5);
    CHECK(static_cast<int>(MultiplayerMenuRow::Start) == 6);
    CHECK(static_cast<int>(MultiplayerMenuRow::Options) == 7);
    CHECK(static_cast<int>(MultiplayerMenuRow::Back) == 8);
}

TEST_CASE("gameplay launch intent cannot promote a single-player run to peer battle") {
    CHECK_FALSE(gameplay_launch_uses_peer_battle(GameplayLaunchKind::SinglePlayer, false));
    CHECK_FALSE(gameplay_launch_uses_peer_battle(GameplayLaunchKind::SinglePlayer, true));
    CHECK(gameplay_launch_uses_peer_battle(GameplayLaunchKind::PeerBattle, false));
    CHECK_FALSE(gameplay_launch_uses_peer_battle(GameplayLaunchKind::PeerBattle, true));
}

TEST_CASE("leaving multiplayer clears session state but keeps connection fields") {
    MultiplayerMenuState state;
    state.address = "192.168.0.25";
    state.port_text = "31415";
    state.connected = true;
    state.local_ready = true;
    state.peer_ready = true;
    state.local_chart_fingerprint = 11;
    state.local_chart_size = 22;
    state.peer_chart_fingerprint = 11;
    state.peer_chart_size = 22;

    reset_multiplayer_menu_session(state);

    CHECK(state.address == "192.168.0.25");
    CHECK(state.port_text == "31415");
    CHECK_FALSE(state.connected);
    CHECK_FALSE(state.local_ready);
    CHECK_FALSE(state.peer_ready);
    CHECK(state.local_chart_fingerprint == 0);
    CHECK(state.local_chart_size == 0);
    CHECK(state.peer_chart_fingerprint == 0);
    CHECK(state.peer_chart_size == 0);
}

TEST_CASE("multiplayer menu cursor clamps and moves within the row range") {
    CHECK(clamp_multiplayer_menu_cursor(-4) == 0);
    CHECK(clamp_multiplayer_menu_cursor(3) == 3);
    CHECK(clamp_multiplayer_menu_cursor(99) == 8);

    CHECK(move_multiplayer_menu_cursor(0, -1) == 0);
    CHECK(move_multiplayer_menu_cursor(0, 3) == 3);
    CHECK(move_multiplayer_menu_cursor(6, 1) == 7);
    CHECK(move_multiplayer_menu_cursor(8, 1) == 8);
    CHECK(move_multiplayer_menu_cursor(-20, 1) == 1);
}

TEST_CASE("multiplayer address editing accepts direct connect address forms") {
    CHECK(is_valid_multiplayer_address_text("127.0.0.1"));
    CHECK(is_valid_multiplayer_address_text("rhythm-host.local"));
    CHECK_FALSE(is_valid_multiplayer_address_text("[2001:db8::1]"));

    CHECK_FALSE(is_valid_multiplayer_address_text(""));
    CHECK_FALSE(is_valid_multiplayer_address_text("host name"));
    CHECK_FALSE(is_valid_multiplayer_address_text("host/name"));

    std::string address = "10.0.0";
    CHECK(try_append_multiplayer_address_character(address, '.'));
    CHECK(try_append_multiplayer_address_character(address, '1'));
    CHECK(address == "10.0.0.1");
    CHECK_FALSE(try_append_multiplayer_address_character(address, '/'));
    CHECK(address == "10.0.0.1");
}

TEST_CASE("multiplayer address editing enforces its maximum length") {
    std::string address(kMultiplayerAddressMaxLength, 'a');
    CHECK(is_valid_multiplayer_address_text(address));
    CHECK_FALSE(try_append_multiplayer_address_character(address, 'b'));
    CHECK(address.size() == kMultiplayerAddressMaxLength);

    address.push_back('c');
    CHECK_FALSE(is_valid_multiplayer_address_text(address));
}

TEST_CASE("multiplayer port parser accepts only the TCP port range") {
    CHECK(parse_multiplayer_port("1") == 1);
    CHECK(parse_multiplayer_port("27300") == 27300);
    CHECK(parse_multiplayer_port("65535") == 65535);

    CHECK_FALSE(parse_multiplayer_port("").has_value());
    CHECK_FALSE(parse_multiplayer_port("0").has_value());
    CHECK_FALSE(parse_multiplayer_port("65536").has_value());
    CHECK_FALSE(parse_multiplayer_port("12a4").has_value());
    CHECK_FALSE(parse_multiplayer_port(" 80").has_value());
    CHECK_FALSE(parse_multiplayer_port("000001").has_value());
}

TEST_CASE("peer battle rules fix scoring while preserving local presentation") {
    tenriff::config::RuntimeConfig config;
    config.judge.pg_ms = 99.0;
    config.gauge.normal.pg = 9.0;
    config.speed.rate = 1.75;
    config.speed.hi_speed = 6.25;
    config.mode.key_mode = "7k";
    config.mode.gauge = "hard";
    config.mode.random = "sr";
    config.mode.random_seed = 42;
    config.mode.mods = {"judge_easy", "full_long_notes"};
    config.mode.ghost_battle_enabled = true;
    config.mode.autoplay_enabled = true;
    config.mode.practice_no_fail_enabled = true;
    config.mode.one_miss_fail_enabled = true;
    config.skin.note_height_scale = 2.4;
    config.input_offset_ms = 17.0;
    config.visual_offset_ms = -12.0;

    tenriff::app::apply_peer_battle_rules(config);

    const tenriff::config::RuntimeConfig defaults;
    CHECK(config.judge.pg_ms == defaults.judge.pg_ms);
    CHECK(config.gauge.normal.pg == defaults.gauge.normal.pg);
    CHECK(config.speed.rate == doctest::Approx(1.0));
    CHECK(config.mode.key_mode == "none");
    CHECK(config.mode.gauge == "shift");
    CHECK(config.mode.random == "off");
    CHECK(config.mode.random_seed == 0);
    CHECK(config.mode.mods.empty());
    CHECK_FALSE(config.mode.ghost_battle_enabled);
    CHECK_FALSE(config.mode.autoplay_enabled);
    CHECK_FALSE(config.mode.practice_no_fail_enabled);
    CHECK_FALSE(config.mode.one_miss_fail_enabled);

    CHECK(config.speed.hi_speed == doctest::Approx(6.25));
    CHECK(config.skin.note_height_scale == doctest::Approx(2.4));
    CHECK(config.input_offset_ms == doctest::Approx(17.0));
    CHECK(config.visual_offset_ms == doctest::Approx(-12.0));
}

TEST_CASE("multiplayer port editing accepts digits and limits the field to five characters") {
    std::string port_text = "2730";
    CHECK(try_append_multiplayer_port_character(port_text, '0'));
    CHECK(port_text == "27300");
    CHECK_FALSE(try_append_multiplayer_port_character(port_text, '1'));
    CHECK_FALSE(try_append_multiplayer_port_character(port_text, '-'));
    CHECK(port_text == "27300");
}

TEST_CASE("multiplayer readiness requires a connection and matching nonzero chart fingerprints") {
    MultiplayerMenuState state;
    state.local_chart_fingerprint = 0xABCDEFu;
    state.local_chart_size = 1234;
    state.peer_chart_fingerprint = 0xABCDEFu;
    state.peer_chart_size = 1234;

    CHECK(multiplayer_chart_fingerprints_match(state));
    CHECK_FALSE(multiplayer_ready_gate_open(state));

    state.connected = true;
    CHECK(multiplayer_ready_gate_open(state));

    state.peer_chart_fingerprint = 0;
    CHECK_FALSE(multiplayer_chart_fingerprints_match(state));
    CHECK_FALSE(multiplayer_ready_gate_open(state));

    state.peer_chart_fingerprint = 0x123456u;
    CHECK_FALSE(multiplayer_chart_fingerprints_match(state));
    CHECK_FALSE(multiplayer_ready_gate_open(state));

    state.peer_chart_fingerprint = state.local_chart_fingerprint;
    state.peer_chart_size = state.local_chart_size + 1;
    CHECK_FALSE(multiplayer_chart_fingerprints_match(state));
}

TEST_CASE("multiplayer start gate requires both ready and host authority") {
    MultiplayerMenuState state;
    state.connected = true;
    state.local_chart_fingerprint = 42;
    state.local_chart_size = 99;
    state.peer_chart_fingerprint = 42;
    state.peer_chart_size = 99;
    state.local_ready = true;
    state.peer_ready = true;

    state.role = MultiplayerRole::Join;
    CHECK(multiplayer_start_gate_open(state));
    CHECK_FALSE(multiplayer_host_can_start(state));

    state.role = MultiplayerRole::Host;
    CHECK(multiplayer_host_can_start(state));

    state.peer_ready = false;
    CHECK_FALSE(multiplayer_start_gate_open(state));
    CHECK_FALSE(multiplayer_host_can_start(state));

    state.peer_ready = true;
    state.peer_chart_fingerprint = 43;
    CHECK_FALSE(multiplayer_start_gate_open(state));
    CHECK_FALSE(multiplayer_host_can_start(state));
}

TEST_CASE("only the host has manual chart selection authority") {
    MultiplayerMenuState state;
    state.role = MultiplayerRole::Join;
    CHECK_FALSE(multiplayer_host_can_choose_chart(state));
    state.role = MultiplayerRole::Host;
    CHECK(multiplayer_host_can_choose_chart(state));
}

TEST_CASE("chart name matching only prioritizes candidates and remains case insensitive") {
    CHECK(multiplayer_chart_candidate_name_matches("Blue Sky", "pack/chart.bms", "blue sky"));
    CHECK(multiplayer_chart_candidate_name_matches("Other", "PACK/CHART.BMS", "pack/chart.bms"));
    CHECK_FALSE(multiplayer_chart_candidate_name_matches("Other", "pack/chart.bms", "missing"));
    CHECK_FALSE(multiplayer_chart_candidate_name_matches("Other", "pack/chart.bms", ""));
}

TEST_CASE("multiplayer chart lookup is limited to active and recent loaded sources") {
    namespace fs = std::filesystem;
    const fs::path base = fs::temp_directory_path() / "tenriff_multiplayer_loaded_sources";
    const fs::path active = base / "Active";
    const fs::path recent = base / "Recent";
    const fs::path unlisted = base / "Unlisted";

    const auto sources = multiplayer_loaded_song_sources(
        active.u8string(),
        {active.u8string(), recent.u8string(), recent.u8string()});

    REQUIRE(sources.size() == 2u);
    CHECK(tenriff::app::menu_songs::normalize_path_key(fs::path(sources[0])) ==
          tenriff::app::menu_songs::normalize_path_key(active));
    CHECK(tenriff::app::menu_songs::normalize_path_key(fs::path(sources[1])) ==
          tenriff::app::menu_songs::normalize_path_key(recent));
    CHECK(std::none_of(sources.begin(), sources.end(), [&](const std::string& source) {
        return tenriff::app::menu_songs::normalize_path_key(fs::path(source)) ==
               tenriff::app::menu_songs::normalize_path_key(unlisted);
    }));
}

TEST_CASE("multiplayer chart candidates resolve relative paths against their own source") {
    namespace fs = std::filesystem;
    const fs::path source = fs::temp_directory_path() / "tenriff_multiplayer_recent_pack";
    tenriff::app::SongEntry other;
    other.path = "charts/other.bms";
    other.title = "Other";
    tenriff::app::SongEntry target;
    target.path = "charts/target.bms";
    target.title = "Host Target";

    const auto candidates = build_multiplayer_chart_candidates(
        {other, target}, source.u8string(), "host target");

    REQUIRE(candidates.size() == 2u);
    CHECK(candidates[0].title == "Host Target");
    const fs::path expected = (source / "charts" / "target.bms").lexically_normal();
    const std::string resolved = multiplayer_chart_path_for_source(
        candidates[0].indexed_path, candidates[0].source_root);
    CHECK(tenriff::app::menu_songs::normalize_path_key(fs::path(resolved)) ==
          tenriff::app::menu_songs::normalize_path_key(expected));
    CHECK(multiplayer_chart_path_for_source(target, source.u8string()) ==
          resolved);
}

TEST_CASE("multiplayer cached chart paths cannot escape their loaded source") {
    namespace fs = std::filesystem;
    const fs::path source = fs::temp_directory_path() / "tenriff_multiplayer_safe_pack";
    const fs::path outside = source.parent_path() / "outside.bms";

    CHECK(multiplayer_chart_path_for_source("../outside.bms", source.u8string()).empty());
    CHECK(multiplayer_chart_path_for_source(outside.u8string(), source.u8string()).empty());
}

TEST_CASE("multiplayer recent-source lookup reads only an existing profile cache") {
    namespace fs = std::filesystem;
    const fs::path base = fs::temp_directory_path() / "tenriff_multiplayer_cache_scope_test";
    std::error_code ec;
    fs::remove_all(base, ec);
    const fs::path profile = base / "profile";
    const fs::path cached_source = base / "cached";
    const fs::path uncached_source = base / "uncached";
    fs::create_directories(cached_source / "charts", ec);
    REQUIRE_FALSE(ec);
    fs::create_directories(uncached_source / "charts", ec);
    REQUIRE_FALSE(ec);

    {
        std::ofstream cached_chart(cached_source / "charts" / "target.bms", std::ios::binary);
        cached_chart << "#TITLE cached target\n#00111:0100\n";
        std::ofstream uncached_chart(uncached_source / "charts" / "target.bms", std::ios::binary);
        uncached_chart << "#TITLE present but not indexed\n#00111:0100\n";
    }

    tenriff::app::SongIndex index;
    tenriff::app::SongEntry entry;
    entry.path = "charts/target.bms";
    entry.title = "Cached Target";
    entry.format = "bms";
    entry.key_count = 10;
    index.entries.push_back(entry);
    tenriff::app::SongIndexOptions options;
    std::string save_error;
    REQUIRE(tenriff::app::save_song_index(
        tenriff::app::song_index_cache_path_for_source(profile.u8string(), cached_source.u8string()),
        index,
        options,
        &save_error));
    REQUIRE(save_error.empty());

    const auto cached = load_multiplayer_chart_candidates_from_profile_cache(
        profile.u8string(), cached_source.u8string(), options, "cached target");
    REQUIRE(cached.loaded_from_cache);
    REQUIRE(cached.candidates.size() == 1u);
    CHECK_FALSE(multiplayer_chart_path_for_source(
                    cached.candidates[0].indexed_path,
                    cached.candidates[0].source_root)
                    .empty());

    const auto uncached = load_multiplayer_chart_candidates_from_profile_cache(
        profile.u8string(), uncached_source.u8string(), options, "present but not indexed");
    CHECK_FALSE(uncached.loaded_from_cache);
    CHECK(uncached.candidates.empty());

    fs::remove_all(base, ec);
}

TEST_CASE("multiplayer score comparison reports signed difference and outcome") {
    const auto win = compare_multiplayer_scores(1'250'000, 1'000'000);
    CHECK(win.outcome == MultiplayerScoreOutcome::Win);
    CHECK(win.difference == 250'000);

    const auto loss = compare_multiplayer_scores(750'000, 1'000'000);
    CHECK(loss.outcome == MultiplayerScoreOutcome::Loss);
    CHECK(loss.difference == -250'000);

    const auto draw = compare_multiplayer_scores(900'000, 900'000);
    CHECK(draw.outcome == MultiplayerScoreOutcome::Draw);
    CHECK(draw.difference == 0);
}

TEST_CASE("peer battle score lead maps ten thousand points to the local loss and win endpoints") {
    const auto tied = peer_battle_score_lead(500'000, 500'000);
    CHECK(tied.difference == 0);
    CHECK(tied.position == doctest::Approx(0.5));

    const auto half_win = peer_battle_score_lead(505'000, 500'000);
    CHECK(half_win.difference == 5'000);
    CHECK(half_win.position == doctest::Approx(0.75));

    const auto half_loss = peer_battle_score_lead(495'000, 500'000);
    CHECK(half_loss.difference == -5'000);
    CHECK(half_loss.position == doctest::Approx(0.25));

    CHECK(peer_battle_score_lead(510'000, 500'000).position == doctest::Approx(1.0));
    CHECK(peer_battle_score_lead(490'000, 500'000).position == doctest::Approx(0.0));
    CHECK(peer_battle_score_lead(900'000, 500'000).position == doctest::Approx(1.0));
    CHECK(peer_battle_score_lead(100'000, 500'000).position == doctest::Approx(0.0));
}

TEST_CASE("peer battle score lead stays overflow safe at signed integer limits") {
    const auto maximum_lead = peer_battle_score_lead(
        (std::numeric_limits<std::int64_t>::max)(),
        (std::numeric_limits<std::int64_t>::min)());
    CHECK(maximum_lead.difference == (std::numeric_limits<std::int64_t>::max)());
    CHECK(maximum_lead.position == doctest::Approx(1.0));

    const auto maximum_loss = peer_battle_score_lead(
        (std::numeric_limits<std::int64_t>::min)(),
        (std::numeric_limits<std::int64_t>::max)());
    CHECK(maximum_loss.difference == (std::numeric_limits<std::int64_t>::min)());
    CHECK(maximum_loss.position == doctest::Approx(0.0));
}

TEST_CASE("peer battle local game over spectates while the connected peer is still playing") {
    PeerBattleSpectatorState state;
    state.local_game_over = true;
    state.peer_connected = true;
    state.round_active = true;

    CHECK(peer_battle_spectator_decision(state) ==
          PeerBattleSpectatorDecision::ContinueSpectating);

    state.remote_finished = true;
    CHECK(peer_battle_spectator_decision(state) ==
          PeerBattleSpectatorDecision::FinishSession);
}

TEST_CASE("peer battle simultaneous game over releases both spectator sessions") {
    PeerBattleSpectatorState state;
    state.local_game_over = true;
    state.peer_connected = true;
    state.round_active = true;
    state.remote_game_over = true;

    CHECK(peer_battle_spectator_decision(state) ==
          PeerBattleSpectatorDecision::FinishSession);
}

TEST_CASE("peer battle spectator ends on disconnect or inactive round") {
    PeerBattleSpectatorState state;
    state.local_game_over = true;
    state.peer_connected = false;
    state.round_active = true;
    CHECK(peer_battle_spectator_decision(state) ==
          PeerBattleSpectatorDecision::FinishSession);

    state.peer_connected = true;
    state.round_active = false;
    CHECK(peer_battle_spectator_decision(state) ==
          PeerBattleSpectatorDecision::FinishSession);

    state.round_active = true;
    state.local_game_over = false;
    CHECK(peer_battle_spectator_decision(state) ==
          PeerBattleSpectatorDecision::FinishSession);
}

TEST_CASE("multiplayer begin delay compensates half the measured RTT") {
    CHECK(multiplayer_compensated_peer_begin_delay_ms(1500, 0) == 1500);
    CHECK(multiplayer_compensated_peer_begin_delay_ms(1500, 100) == 1450);
    CHECK(multiplayer_compensated_peer_begin_delay_ms(1500, 3000) == 1000);
    CHECK(multiplayer_compensated_peer_begin_delay_ms(300, 1000) == 300);
}
