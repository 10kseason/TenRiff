#include "doctest/doctest.h"

#include <array>
#include <cmath>
#include <iostream>
#include <set>

#include "app/JudgeTimingPolicy.h"
#include "app/ModeManager.h"
#include "app/ReplayVerifier.h"
#include "gameplay/GameplayEngine.h"
#include "render/GameplayFeedbackText.h"

TEST_CASE("fixed PG sample boundaries distinguish exact 22 ms from the first GREAT sample") {
    for (const int sample_rate : {44100, 48000, 88200, 96000, 192000}) {
        for (const int rank : {0, 1, 2, 3, 4}) {
            for (const int difficulty : {0, 1, 2}) {
                constexpr double normal[]{10.0, 15.0, 19.0, 22.0, 22.0};
                const double expected = difficulty == 2 ? 10.0 : difficulty == 1 ? 19.0 : normal[rank];
                tenriff::gameplay::GameplayConfig config;
                config.sample_rate = sample_rate;
                config.judge = tenriff::app::judge_timing_for_fixed_baseline(
                    {}, rank, false, difficulty == 1, difficulty == 2);
                CHECK(config.judge.pg_ms == expected);
                const auto edge = std::llround(expected * sample_rate / 1000.0);
                const int64_t note_sample = sample_rate * 2;
                for (const int sign : {-1, 1}) for (const int extra : {-1, 0, 1}) {
                    tenriff::gameplay::GameplayChart chart;
                    chart.lane_count = 1;
                    chart.duration_samples = sample_rate * 4;
                    chart.notes.push_back({1, note_sample, std::nullopt});
                    tenriff::gameplay::GameplayEngine engine(chart, config);
                    const auto offset = sign * (edge + extra);
                    (void)engine.handle_input(1, tenriff::input::InputState::Pressed, note_sample + offset);
                    CHECK(engine.stats().counts.pg == (extra <= 0 ? 1 : 0));
                    CHECK(engine.stats().counts.gr == (extra > 0 ? 1 : 0));
                    if (extra > 0) {
                        const auto text = tenriff::render::gameplay_timing_feedback_text(
                            offset * 1000.0 / sample_rate);
                        const auto magnitude = std::to_wstring(static_cast<int>(expected)) + L".1 ms";
                        CHECK(text == (sign < 0 ? L"FAST -" : L"SLOW +") + magnitude);
                    }
                }
            }
        }
    }
}

// Exercise the public engine, rather than just checking the policy helper's
// output. A rate-adjusted chart already uses playback samples; its hit windows
// must retain real-time milliseconds for every rate and gauge choice.
TEST_CASE("historical R4 timing audit covers ranks mods rates gauges and sample rates") {
    using tenriff::game::GaugeType;
    using tenriff::gameplay::GameplayChart;
    using tenriff::gameplay::GameplayConfig;
    using tenriff::gameplay::GameplayEngine;
    using tenriff::gameplay::NoteEvent;
    const std::array<int, 5> sample_rates{44100, 48000, 88200, 96000, 192000};
    const std::array<int, 6> ranks{-1, 0, 1, 2, 3, 4};
    const std::array<double, 4> rates{0.5, 1.0, 1.5, 2.0};
    const std::array<GaugeType, 4> gauges{
        GaugeType::Easy, GaugeType::Normal, GaugeType::Hard, GaugeType::ExHard};
    std::size_t cases = 0;
    std::size_t mismatches = 0;
    for (int sample_rate : sample_rates) for (int rank : ranks) for (int mod = 0; mod < 3; ++mod) {
        const bool easy = mod == 1;
        const bool hard = mod == 2;
        const double pg = rank == 0 ? 8.0 : rank == 1 ? 15.0 : rank == 2 ? 18.0 : 21.0;
        const double scale = pg / 21.0;
        std::array<double, 4> milliseconds{pg, 65.0 * scale, 115.0 * scale, 210.0 * scale};
        if (easy) {
            for (int i = 0; i < 3; ++i) milliseconds[i] *= 1.35;
            milliseconds[3] = 210.0;
        } else if (hard) {
            milliseconds[0] *= 17.5 / 21.0;
            milliseconds[1] *= 18.0 / 21.0;
            milliseconds[2] *= 18.0 / 21.0;
            milliseconds[3] = 225.0;
        }
        std::array<int64_t, 4> windows{};
        std::set<int64_t> offsets{0};
        for (int i = 0; i < 4; ++i) {
            windows[i] = static_cast<int64_t>(std::llround(milliseconds[i] * sample_rate / 1000.0));
            for (int edge = -1; edge <= 1; ++edge) {
                offsets.insert(windows[i] + edge);
                offsets.insert(-windows[i] + edge);
            }
        }
        for (double rate : rates) for (auto gauge : gauges) for (int64_t offset : offsets) {
            GameplayConfig config;
            config.sample_rate = sample_rate;
            config.rate = rate;
            config.initial_gauge = gauge;
            config.judge = tenriff::app::judge_timing_for_policy(
                tenriff::app::judge_timing_for_bms_rank(config.judge, rank), easy, hard);
            GameplayChart chart;
            chart.lane_count = 1;
            chart.duration_samples = static_cast<int64_t>(sample_rate) * 10;
            const int64_t note_sample = static_cast<int64_t>(sample_rate) * 5;
            chart.notes.push_back(NoteEvent{1, note_sample, std::nullopt});
            GameplayEngine engine(chart, config);
            (void)engine.handle_input(1, tenriff::input::InputState::Pressed, note_sample + offset);
            const auto& counts = engine.stats().counts;
            const int actual = counts.pg ? 0 : counts.gr ? 1 : counts.gd ? 2 : counts.bd ? 3 : counts.pr ? 4 : -1;
            const int64_t distance = std::abs(offset);
            const int expected = distance <= windows[0] ? 0 : distance <= windows[1] ? 1 :
                distance <= windows[2] ? 2 : distance <= windows[3] ? 3 :
                (offset < 0 || hard) ? 4 : 3;
            ++cases;
            if (actual != expected || counts.pg + counts.gr + counts.gd + counts.bd + counts.pr != 1) {
                if (mismatches < 12) std::cout << "[boundary-mismatch] hz=" << sample_rate
                    << " rank=" << rank << " mod=" << mod << " rate=" << rate
                    << " gauge=" << static_cast<int>(gauge) << " offset_samples=" << offset
                    << " actual=" << actual << " expected=" << expected << '\n';
                ++mismatches;
            }
        }
    }
    std::cout << "[timing-audit-boundaries] cases=" << cases << " mismatches=" << mismatches << '\n';
    CHECK(cases == 36000);
    CHECK(mismatches == 0);
}

TEST_CASE("timing audit current verifier reproduces a live batched ordinary LN release") {
    using tenriff::gameplay::GameplayChart;
    using tenriff::gameplay::GameplayConfig;
    using tenriff::gameplay::GameplayEngine;
    using tenriff::gameplay::ReplayFile;
    using tenriff::input::InputState;
    constexpr int sample_rate = 48000;
    const auto samples = [sample_rate](int64_t milliseconds) { return milliseconds * sample_rate / 1000; };
    const int64_t lead_in = samples(tenriff::app::kReplayVerificationLeadInMs);

    GameplayChart source;
    source.lane_count = 5;
    source.bms_rank = 3;
    source.duration_samples = samples(3000);
    source.notes = {{1, samples(1000), samples(2000)}, {2, samples(2700), std::nullopt}};

    tenriff::config::ModeConfig mode;
    mode.key_mode = "auto";
    mode.key_conversion_algorithm = "krrcream";
    mode.key_conversion_nk2_preset = "native";
    mode.random = "off";
    mode.random_seed = 0;
    mode.gauge = "normal";
    mode.mods.clear();
    const auto managed = tenriff::app::manage_modes(
        source, tenriff::app::ChartFormat::Bms, mode, tenriff::config::JudgeConfig{},
        1.0, 120.0, sample_rate, tenriff::app::LongNoteMixVersion::Musical, 3);
    auto live_chart = managed.chart;
    tenriff::gameplay::offset_gameplay_chart_samples(live_chart, lead_in);

    GameplayConfig live_config;
    live_config.sample_rate = sample_rate;
    live_config.rate = 1.0;
    live_config.judge = managed.judge;
    live_config.initial_gauge = tenriff::game::GaugeType::Normal;
    live_config.gauge_shift_enabled = true;
    GameplayEngine live(live_chart, live_config);
    (void)live.handle_input(1, InputState::Pressed, lead_in + samples(1000));
    live.advance(lead_in + samples(1970));
    // Model the callback order: deliver the +30ms release before advancing
    // across the ordinary LN tail. The verifier receives this exact trace, but
    // currently advances to each input sample - 1 before delivering its group.
    (void)live.handle_input(1, InputState::Released, lead_in + samples(2030));
    live.advance(lead_in + samples(3100));
    REQUIRE(live.is_finished());

    ReplayFile replay;
    replay.chart_format = "bms";
    replay.bms_rank = source.bms_rank;
    // In-memory verifier fixtures use a valid digest envelope; this API takes
    // the supplied source chart and does not read or hash a user's chart file.
    replay.chart_sha256 = std::string(64, 'a');
    replay.ruleset_id = std::string(tenriff::app::kCanonicalReplayRulesetId);
    replay.created_utc = "20261013_000000Z";
    replay.sample_rate = sample_rate;
    replay.rate = 1.0;
    replay.mode.key_mode = mode.key_mode;
    replay.mode.key_conversion_algorithm = mode.key_conversion_algorithm;
    replay.mode.key_conversion_nk2_preset = mode.key_conversion_nk2_preset;
    replay.mode.key_conversion_version = 3;
    replay.mode.long_note_mix_version = 2;
    replay.mode.random = mode.random;
    replay.mode.random_seed = static_cast<int>(mode.random_seed);
    replay.mode.gauge = mode.gauge;
    replay.mods = managed.active_mods;
    replay.rate_multiplier = managed.rate_multiplier;
    replay.score_multiplier = managed.final_multiplier;
    replay.trace = live.replay();
    replay.stats = live.stats();
    replay.final_score = tenriff::gameplay::scale_native_score(
        replay.stats.raw_score, replay.score_multiplier);
    REQUIRE(replay.trace.duration_samples == source.duration_samples + lead_in);
    REQUIRE(replay.trace.events.size() == 2);
    REQUIRE(replay.trace.events.back().sample == lead_in + samples(2030));
    const auto validation = tenriff::gameplay::validate_replay_evidence(replay);
    std::cout << "[timing-audit-actual-verifier-fixture] valid=" << validation.success()
              << " sample_rate=" << sample_rate << " lead_in=" << lead_in
              << " source_duration=" << source.duration_samples
              << " trace_duration=" << replay.trace.duration_samples
              << " validation_error=" << validation.error << '\n';
    REQUIRE(validation.success());

    const auto verified = tenriff::app::verify_replay_against_chart(
        replay, source, tenriff::app::ChartFormat::Bms, 120.0);
    const auto& a = replay.stats.counts;
    const auto& b = verified.stats.counts;
    std::cout << "[timing-audit-actual-verifier] status="
              << tenriff::app::replay_verification_status_token(verified.status)
              << " verified=" << verified.verified()
              << " claims_match=" << verified.claims_match
              << " live_counts=" << a.pg << ',' << a.gr << ',' << a.gd << ',' << a.bd << ',' << a.pr
              << " verifier_counts=" << b.pg << ',' << b.gr << ',' << b.gd << ',' << b.bd << ',' << b.pr
              << " live_score=" << replay.final_score << " verifier_score=" << verified.final_score
              << " detail=" << verified.detail << '\n';
    REQUIRE(verified.verified());
    // Deliberately require parity rather than accepting the observed mismatch.
    const bool same_counts = std::array<int, 5>{a.pg, a.gr, a.gd, a.bd, a.pr} ==
                             std::array<int, 5>{b.pg, b.gr, b.gd, b.bd, b.pr};
    CHECK(verified.claims_match);
    CHECK(same_counts);
    CHECK(verified.final_score == replay.final_score);
}
