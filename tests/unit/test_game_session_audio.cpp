#include "doctest/doctest.h"
#include "app/GameSession.h"
#include "app/ChartLoader.h"
#include "app/GameplayStartPolicy.h"
#include "app/JudgeTimingPolicy.h"
#include "app/ModeManager.h"
#include "app/ReplayVerifier.h"
#include "app/SitesLeaderboardClient.h"
#include "app/PersistedRuntimeConfig.h"
#include "config/SimpleJson.h"
#include "gameplay/ModeApplier.h"
#include "timing/HighResClock.h"
#include "render/GameplayMotion.h"
#include "../support/MockAsioDriver.h"

#include <algorithm>
#include <memory>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace tenriff::app {

namespace {
struct CompletionTestDirectory {
    std::filesystem::path path = std::filesystem::current_path() /
        std::filesystem::u8path(u8"completion-한글-テスト-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CompletionTestDirectory() { std::filesystem::create_directory(path); }
    ~CompletionTestDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
}

// Exercise the production input-to-voice-to-mixer path without starting a device,
// input thread, decoder, profile or record writer.
struct GameSessionAudioTestAccess {
    static void check_full_combo_hud() {
        for (bool hold : {false, true}) {
            auto session = shift_lane_fixture(32, hold);
            CHECK_FALSE(session->hud_snapshot().full_combo);
            queue_physical_edge(*session, 32, input::InputState::Pressed, 48000);
            CHECK(session->engine_->stats().total_combo_steps == (hold ? 2 : 1));
            CHECK(session->hud_snapshot().full_combo == !hold);
            if (hold) {
                session->engine_->advance(95999);
                CHECK_FALSE(session->hud_snapshot().full_combo);
                queue_physical_edge(*session, 32, input::InputState::Released, 96000);
            }
            REQUIRE(session->engine_->is_finished());
            REQUIRE(session->hud_snapshot().full_combo);
            const auto score = session->engine_->stats().raw_score;
            const auto recorded = session->engine_->replay().events.size();
            session->autoplay_enabled_ = true;
            CHECK_FALSE(session->hud_snapshot().full_combo);
            session->autoplay_enabled_ = false;
            session->auto_scratch_enabled_ = true;
            CHECK_FALSE(session->hud_snapshot().full_combo);
            session->auto_scratch_enabled_ = false;
            session->practice_no_fail_enabled_ = true;
            CHECK_FALSE(session->hud_snapshot().full_combo);
            session->practice_no_fail_enabled_ = false;
            session->practice_start_seconds_ = 1.0;
            CHECK_FALSE(session->hud_snapshot().full_combo);
            // Immediate restart plays the entire chart despite an editor marker.
            session->instant_restart_start_ = true;
            CHECK(session->hud_snapshot().full_combo);
            session->practice_start_seconds_ = 0.0;
            session->user_aborted_.store(true);
            CHECK_FALSE(session->hud_snapshot().full_combo);
            session->user_aborted_.store(false);
            session->replay_playback_enabled_ = true;
            session->replay_source_.mode.autoplay_enabled = true;
            CHECK_FALSE(session->hud_snapshot().full_combo);
            session->replay_source_.mode.autoplay_enabled = false;
            session->replay_source_.aborted = true;
            CHECK_FALSE(session->hud_snapshot().full_combo);
            session->replay_source_.aborted = false;
            CHECK(session->hud_snapshot().full_combo);
            CHECK(session->engine_->stats().raw_score == score);
            CHECK(session->engine_->replay().events.size() == recorded);
        }
        auto broken = shift_lane_fixture(32, true);
        queue_physical_edge(*broken, 32, input::InputState::Pressed, 48000);
        queue_physical_edge(*broken, 32, input::InputState::Released, 60000);
        broken->engine_->advance(120000);
        REQUIRE(broken->engine_->is_finished());
        CHECK_FALSE(broken->hud_snapshot().full_combo);

        auto session = shift_lane_fixture(32, true);
        gameplay::GameplayConfig config;
        config.sample_rate = 48000;
        session->ghost_engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        queue_physical_edge(*session, 32, input::InputState::Pressed, 48000);
        queue_physical_edge(*session, 32, input::InputState::Released, 96000);
        REQUIRE(session->hud_snapshot().full_combo);
        CHECK_FALSE(session->hud_snapshot().ghost_full_combo);
        (void)session->ghost_engine_->handle_input(1, input::InputState::Pressed, 48000);
        session->ghost_engine_->advance(95999);
        CHECK_FALSE(session->hud_snapshot().ghost_full_combo);
        (void)session->ghost_engine_->handle_input(1, input::InputState::Released, 96000);
        session->ghost_engine_->advance(96000);
        REQUIRE(session->hud_snapshot().ghost_full_combo);
        session->ghost_replay_source_.mode.autoplay_enabled = true;
        CHECK_FALSE(session->hud_snapshot().ghost_full_combo);
        CHECK(session->hud_snapshot().full_combo);
        session->ghost_replay_source_.mode.autoplay_enabled = false;
        session->ghost_replay_source_.mode.practice_no_fail_enabled = true;
        CHECK_FALSE(session->hud_snapshot().ghost_full_combo);
        session->ghost_replay_source_.mode.practice_no_fail_enabled = false;
        session->ghost_replay_source_.aborted = true;
        CHECK_FALSE(session->hud_snapshot().ghost_full_combo);
        CHECK(session->hud_snapshot().full_combo);
    }

    static void check_key_overlay_hud() {
        auto session = shift_lane_fixture(32, false);
        session->key_overlay_.configure(true, 7);
        REQUIRE(session->key_overlay_.start());
        for (int state = 0; state < 3; ++state) {
            session->paused_.store(state == 1);
            session->autoplay_enabled_ = state == 2;
            const auto hud = session->hud_snapshot();
            CHECK(hud.key_overlay.visible);
            if (state != 0) {
                CHECK_FALSE(hud.key_overlay.live);
                CHECK(hud.key_overlay.kps == 0);
                CHECK(hud.key_overlay.note_count == 0);
            }
        }
        session->key_overlay_.stop();
    }
    static void check_key_overlay_routing() {
        for (int boundary = 0; boundary < 3; ++boundary) {
            auto session = shift_lane_fixture(32, false);
            session->key_overlay_.configure(true, 4);
            session->physical_key_state_probe_ = [](uint32_t) { return false; };
            queue_physical_edge(*session, 32, input::InputState::Pressed, 48000);
            if (boundary == 0) session->process_countdown_input_queue();
            if (boundary == 1) session->process_paused_input_queue();
            if (boundary == 2) session->rebaseline_gameplay_start_input_state(48000);
            CHECK(session->key_overlay_.update(1'050'000'000LL, true).note_count == 0);
        }
        for (bool future : {false, true}) {
            std::vector<gameplay::ReplayEvent> baseline;
            int64_t baseline_score = 0;
            for (bool enabled : {false, true}) {
                auto session = shift_lane_fixture(32, false);
                session->key_overlay_.configure(enabled, 4);
                session->key_to_lane_ = {{32, 1}, {33, 1}};
                session->lane_binding_state_.configure(session->key_to_lane_);
                queue_physical_edge(*session, 32, input::InputState::Pressed, 48000, future);
                queue_physical_edge(*session, 33, input::InputState::Pressed, 48480, future);
                queue_physical_edge(*session, 33, input::InputState::Released, 48960, future);
                queue_physical_edge(*session, 32, input::InputState::Released, 49440, future);
                queue_physical_edge(*session, 116, input::InputState::Pressed, 49920, future);
                const auto state = session->key_overlay_.update(1'050'000'000LL, true);
                CHECK(state.visible == enabled);
                CHECK(state.note_count == (enabled ? 1u : 0u));
                CHECK_FALSE(state.held[0]);
                if (enabled) {
                    CHECK(state.notes[0].press_ms == 1000);
                    CHECK(state.notes[0].release_ms == 1020);
                    CHECK(state.accepted_presses == 1);
                    CHECK(state.kps == 1);
                }
                // Enabling this observer must not alter either accepted scoring edge.
                const auto& events = session->engine_->replay().events;
                if (!enabled) {
                    baseline = events;
                    baseline_score = session->engine_->stats().raw_score;
                } else {
                    REQUIRE(!baseline.empty());
                    REQUIRE(events.size() == baseline.size());
                    for (std::size_t i = 0; i < events.size(); ++i) {
                        CHECK(events[i].lane == baseline[i].lane);
                        CHECK(events[i].state == baseline[i].state);
                        CHECK(events[i].sample == baseline[i].sample);
                    }
                    CHECK(session->engine_->stats().raw_score == baseline_score);
                }
            }
        }
        for (bool replay : {false, true}) {
            auto session = shift_lane_fixture(32, false);
            session->key_overlay_.configure(true, 4);
            session->replay_playback_enabled_ = replay;
            session->autoplay_enabled_ = !replay;
            queue_physical_edge(*session, 32, input::InputState::Pressed, 48000);
            CHECK(session->key_overlay_.update(1'050'000'000LL, true).note_count == 0);
        }
    }
    static void check_default_tuning_directions() {
        for (bool paused : {false, true}) {
            auto session = pause_fixture();
            const auto bindings = resolve_function_keys(session->config_.ui, session->key_to_lane_);
            session->f1_keycode_ = bindings[0]; session->f2_keycode_ = bindings[1];
            session->f3_keycode_ = bindings[2]; session->f4_keycode_ = bindings[3];
            session->config_.speed.hi_speed = 4.0;
            session->config_.skin.judgement_line_position = 0.82;
            if (paused) control(*session, 27);
            const auto press = [&](uint32_t key) {
                control(*session, key);
                input::InputEvent release{};
                release.keycode = key; release.state = input::InputState::Released;
                REQUIRE(session->handle_control_input(release));
            };
            // Physical Windows F1-F4 codes, passed only to the production handler.
            press(112); CHECK(session->config_.speed.hi_speed == doctest::Approx(3.75));
            CHECK(session->config_.skin.judgement_line_position == doctest::Approx(0.82));
            press(113); CHECK(session->config_.speed.hi_speed == doctest::Approx(4.0));
            press(114); CHECK(session->config_.skin.judgement_line_position == doctest::Approx(0.81));
            press(115); CHECK(session->config_.skin.judgement_line_position == doctest::Approx(0.82));
            session->config_.skin.judgement_line_position = 0.01;
            press(114); CHECK(session->config_.skin.judgement_line_position == doctest::Approx(0.01));
            session->config_.skin.judgement_line_position = 1.0;
            press(115); CHECK(session->config_.skin.judgement_line_position == doctest::Approx(1.0));
            CHECK(session->engine_->replay().events.empty());
            CHECK(session->engine_->stats().raw_score == 0);
        }
    }
    static void check_accelerated_tuning() {
        constexpr int64_t start = 1'000'000'000LL;
        for (bool paused : {false, true}) {
            for (const int action : {0, 1, 2, 3, 6, 7}) {
                auto session = pause_fixture();
                const auto keys = resolve_function_keys(session->config_.ui, session->key_to_lane_);
                session->f1_keycode_ = keys[0]; session->f2_keycode_ = keys[1];
                session->f3_keycode_ = keys[2]; session->f4_keycode_ = keys[3];
                session->f7_keycode_ = keys[6]; session->f8_keycode_ = keys[7];
                session->config_.speed.hi_speed = 10.0;
                session->config_.skin.judgement_line_position = 0.5;
                session->config_.visual_offset_ms = 0.0;
                if (paused) control(*session, 27);
                const auto value = [&] {
                    return action < 2 ? session->config_.skin.judgement_line_position
                         : action < 4 ? session->config_.speed.hi_speed : session->config_.visual_offset_ms;
                };
                const double step = (action % 2 == 0 ? -1.0 : 1.0) *
                    (action < 2 ? 0.01 : action < 4 ? 0.25 : 1.0);
                const double initial = value();
                const auto edge = [&](input::InputState state, int64_t time) {
                    REQUIRE(session->handle_control_input({keys[action], state, time}));
                };
                edge(input::InputState::Pressed, start);
                CHECK(value() == doctest::Approx(initial + step));
                edge(input::InputState::Pressed, start + 100'000'000LL);
                session->service_control_repeats(start + 179'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + step));
                session->service_control_repeats(start + 180'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + 2 * step));
                session->service_control_repeats(start + 224'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + 2 * step));
                session->service_control_repeats(start + 225'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + 3 * step));
                // Late ticks apply one step, then 25ms/15ms intervals after the
                // specified hold thresholds instead of a burst of missed steps.
                session->service_control_repeats(start + 600'000'000LL, true);
                session->service_control_repeats(start + 624'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + 4 * step));
                session->service_control_repeats(start + 625'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + 5 * step));
                session->service_control_repeats(start + 1'500'000'000LL, true);
                session->service_control_repeats(start + 1'514'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + 6 * step));
                session->service_control_repeats(start + 1'515'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + 7 * step));
                edge(input::InputState::Released, start + 1'516'000'000LL);
                session->service_control_repeats(start + 5'000'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + 7 * step));
                // A fresh press restarts the full delay, including after focus loss.
                edge(input::InputState::Pressed, start + 6'000'000'000LL);
                session->service_control_repeats(start + 6'179'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + 8 * step));
                session->service_control_repeats(start + 6'180'000'000LL, false);
                session->service_control_repeats(start + 7'000'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + 8 * step));
                edge(input::InputState::Released, start + 7'000'000'001LL);
                edge(input::InputState::Pressed, start + 8'000'000'000LL);
                session->pause_resume_requested_.store(true);
                session->service_control_repeats(start + 10'000'000'000LL, true);
                CHECK(value() == doctest::Approx(initial + 9 * step));
                CHECK(session->engine_->replay().events.empty());
                CHECK(session->engine_->stats().raw_score == 0);
            }
        }
        // Chat/help consumption also clears a previously held speed key.
        auto session = pause_fixture();
        session->f3_keycode_ = 112;
        session->config_.speed.hi_speed = 10.0;
        REQUIRE(session->handle_control_input({112, input::InputState::Pressed, start}));
        session->control_input_callback_ = [](const input::InputEvent&) { return true; };
        REQUIRE(session->handle_control_input({122, input::InputState::Pressed, start + 1}));
        session->service_control_repeats(start + 2'000'000'000LL, true);
        CHECK(session->config_.speed.hi_speed == doctest::Approx(9.75));
    }

    static void check_pcm_retention_until_stop() {
        GameSession session;
        session.sample_rate_ = 1000;
        session.chart_audio_assets_.resize(1);
        auto& asset = session.chart_audio_assets_[0];
        auto samples = std::make_shared<const std::vector<float>>(2048, 0.25f);
        std::weak_ptr<const std::vector<float>> retained = samples;
        asset.clip.samples = samples;
        asset.state = GameSession::ChartAudioAssetState::Ready;
        asset.decoded_bytes = samples->size() * sizeof(float);
        asset.use_samples = {0};
        session.chart_audio_decoded_bytes_ = 1024ull * 1024ull * 1024ull;
        samples.reset();
        for (const int64_t now : {10'000, 20'000, 120'000}) {
            session.service_chart_audio_streaming(now);
            CHECK_FALSE(retained.expired());
            CHECK(asset.state == GameSession::ChartAudioAssetState::Ready);
        }
        session.shutdown();
        CHECK(retained.expired());
    }

    static void callback_with_sample_clock(GameSession& session, float* output, uint32_t frames,
                                            int64_t write, int64_t playback) {
        session.audio_thread_.callback_ = [&session](float* out, uint32_t count, int64_t start, int64_t head) {
            session.audio_callback(out, count, start, head);
        };
        session.audio_thread_.invoke_callback(output, frames, write, playback,
            1'000'000'000LL + static_cast<int64_t>(std::llround(
                playback * 1'000'000'000.0 / session.sample_rate_)));
    }
    static void check_timing_audit_ln_release_against_playback_head(int release_ms,
                                                                  bool delay_final_callback = false) {
        // Same authored LN and exact physical release, with only callback
        // buffer size / queued device padding changed. Use real timestamp
        // mapping and the production callback, without a physical device.
        constexpr int64_t epoch_ns = 1'000'000'000LL;
        int mismatches = 0;
        for (const int rate : {44100, 48000, 96000}) {
            for (const int buffer_ms : {10, 40}) {
                for (const int padding_ms : {0, 30}) {
                    auto session = std::make_unique<GameSession>();
                    session->sample_rate_ = rate;
                    session->chart_.lane_count = 1;
                    session->chart_.duration_samples = rate * 4;
                    session->chart_.notes = {{1, rate, rate * 2}, {1, rate * 3, std::nullopt}};
                    gameplay::GameplayConfig config;
                    config.sample_rate = rate;
                    config.practice_no_fail_enabled = true;
                    session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
                    session->lane_activity_.assign(1, 0.0f);
                    session->lane_pressed_.assign(1, 0);
                    session->key_to_lane_ = {{32, 1}};
                    session->lane_binding_state_.configure(session->key_to_lane_);
                    session->audio_timing_diagnostics_logged_ = true;
                    session->config_.audio_ui.mute_when_inactive = false;
                    session->synthetic_tones_enabled_ = false;
                    session->audio_thread_.callback_ = [&](float* output, uint32_t frames,
                                                            int64_t start, int64_t playback) {
                        session->audio_callback(output, frames, start, playback);
                    };
                    const auto sample_at_ms = [&](int64_t ms) {
                        return static_cast<int64_t>(std::llround(static_cast<double>(ms * rate) / 1000.0));
                    };
                    const auto frames = static_cast<uint32_t>(sample_at_ms(buffer_ms));
                    const int64_t padding = sample_at_ms(padding_ms);
                    std::vector<float> output(frames * 2);
                    const auto callback_at_ms = [&](int64_t ms) {
                        const int64_t playback = sample_at_ms(ms);
                        session->audio_thread_.invoke_callback(output.data(), frames,
                            playback + padding, playback, epoch_ns + ms * 1'000'000LL);
                    };
                    REQUIRE(session->input_thread_.queue().push(
                        {32, input::InputState::Pressed, epoch_ns + 1'000'000'000LL}));
                    callback_at_ms(1000);
                    // Deliver callbacks on the same regular grid up to just
                    // before the physical key-up, which is fixed for each case.
                    int64_t next_callback_ms = 1000 + buffer_ms;
                    while (next_callback_ms < release_ms &&
                           (!delay_final_callback || next_callback_ms <= 1970)) {
                        callback_at_ms(next_callback_ms);
                        next_callback_ms += buffer_ms;
                    }
                    REQUIRE(session->input_thread_.queue().push(
                        {32, input::InputState::Released, epoch_ns + release_ms * 1'000'000LL}));
                    // A separate control omits intermediate callbacks, then
                    // delivers the correctly stamped +30 ms release at 2040 ms.
                    // It tests caller batch ordering, not a real device underrun.
                    callback_at_ms(delay_final_callback ? 2040 : next_callback_ms);
                    const auto& recorded = session->engine_->replay().events;
                    REQUIRE(recorded.size() == 2);
                    const auto& counts = session->engine_->stats().counts;
                    const auto& feedback = session->engine_->live_feedback();
                    // An early -27 ms release earns GREAT under the current policy. A standard LN
                    // held through its tail and released +30 ms later instead
                    // completes as PG, per the current held-through contract.
                    const bool early_release = release_ms < 2000;
                    const double expected_delta = early_release
                        ? static_cast<double>(sample_at_ms(release_ms) - rate * 2) * 1000.0 / rate : 0.0;
                    const bool matches = recorded[1].sample == sample_at_ms(release_ms) &&
                        counts.pg == (early_release ? 1 : 2) &&
                        counts.gr == (early_release ? 1 : 0) && counts.bd == 0 && feedback.has_value &&
                        std::abs(feedback.delta_ms - expected_delta) < 0.000001;
                    if (!matches) ++mismatches;
                    std::cout << "[timing-audit-ln-write-ahead] rate=" << rate
                              << " buffer_ms=" << buffer_ms << " padding_ms=" << padding_ms
                              << " physical_release_ms=" << release_ms
                              << " delayed_callback=" << delay_final_callback
                              << " release_sample=" << recorded[1].sample
                              << " pg=" << counts.pg << " gr=" << counts.gr
                              << " tail_delta_ms=" << feedback.delta_ms
                              << " matches=" << matches << '\n';
                }
            }
        }
        // This repository's CHECK throws, so report every numeric condition
        // before failing the case once. REQUIRE still guards fixture validity.
        CHECK(mismatches == 0);
    }

    static void check_timing_audit_ln_release_at_observed_buffer_scale() {
        constexpr int rate = 48000;
        constexpr int release_ms = 1991;
        constexpr int64_t epoch_ns = 1'000'000'000LL;
        int mismatches = 0;
        for (const int rank : {0, 1, 2, 3}) {
            for (const int buffer_ms : {1, 10, 22}) {
                auto session = std::make_unique<GameSession>();
                session->sample_rate_ = rate;
                session->chart_.lane_count = 1;
                session->chart_.bms_rank = rank;
                session->chart_.duration_samples = rate * 4;
                session->chart_.notes = {{1, rate, rate * 2}, {1, rate * 3, std::nullopt}};
                config::ModeConfig mode;
                mode.key_mode = "auto";
                mode.random = "off";
                const auto managed = manage_modes(session->chart_, ChartFormat::Bms,
                    mode, config::JudgeConfig{}, 1.0, 120.0, rate);
                gameplay::GameplayConfig config;
                config.sample_rate = rate;
                config.judge = managed.judge;
                config.practice_no_fail_enabled = true;
                session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
                session->lane_activity_.assign(1, 0.0f);
                session->lane_pressed_.assign(1, 0);
                session->key_to_lane_ = {{32, 1}};
                session->lane_binding_state_.configure(session->key_to_lane_);
                session->audio_timing_diagnostics_logged_ = true;
                session->config_.audio_ui.mute_when_inactive = false;
                session->synthetic_tones_enabled_ = false;
                session->audio_thread_.callback_ = [&](float* output, uint32_t frames,
                                                       int64_t start, int64_t playback) {
                    session->audio_callback(output, frames, start, playback);
                };
                const auto frames = static_cast<uint32_t>(buffer_ms * 48);
                std::vector<float> output(frames * 2);
                const auto callback_at_ms = [&](int64_t ms) {
                    session->audio_thread_.invoke_callback(output.data(), frames,
                        ms * 48, ms * 48, epoch_ns + ms * 1'000'000LL);
                };
                REQUIRE(session->input_thread_.queue().push(
                    {32, input::InputState::Pressed, epoch_ns + 1'000'000'000LL}));
                callback_at_ms(1000);
                int next_callback_ms = 1000 + buffer_ms;
                while (next_callback_ms < release_ms) {
                    callback_at_ms(next_callback_ms);
                    next_callback_ms += buffer_ms;
                }
                REQUIRE(session->input_thread_.queue().push(
                    {32, input::InputState::Released, epoch_ns + release_ms * 1'000'000LL}));
                callback_at_ms(next_callback_ms);
                const auto& recorded = session->engine_->replay().events;
                REQUIRE(recorded.size() == 2);
                const auto& counts = session->engine_->stats().counts;
                const auto& feedback = session->engine_->live_feedback();
                // At -9 ms every rank is PG under the pinned 10ms minimum.
                // Every rank must nevertheless preserve the measured -9 ms.
                const bool sample_matches = recorded[1].sample == release_ms * 48;
                const bool grade_matches = counts.pg == 2 && counts.gr == 0;
                const bool delta_matches = feedback.has_value &&
                    std::abs(feedback.delta_ms + 9.0) < 0.000001;
                if (!sample_matches || !grade_matches || !delta_matches) ++mismatches;
                // 22 ms matches the observed shared-buffer scale, but no real
                // device is opened: callback phase and key-up remain synthetic.
                std::cout << "[timing-audit-ln-observed-buffer-scale] rate=" << rate
                          << " rank=" << rank << " pg_window_ms=" << managed.judge.pg_ms
                          << " buffer_ms=" << buffer_ms << " padding_ms=0"
                          << " physical_release_ms=" << release_ms
                          << " release_sample=" << recorded[1].sample
                          << " pg=" << counts.pg << " gr=" << counts.gr
                          << " tail_delta_ms=" << feedback.delta_ms
                          << " sample_matches=" << sample_matches
                          << " grade_matches=" << grade_matches
                          << " delta_matches=" << delta_matches << '\n';
            }
        }
        CHECK(mismatches == 0);
    }

    static void check_r5_calibrated_ln_horizon() {
        constexpr int64_t epoch = 1'000'000'000LL;
        for (const int offset_ms : {-200, -20, 0, 20, 200}) {
            for (const int buffer_ms : {1, 10, 22}) {
                auto session = shift_lane_fixture(32, false);
                session->chart_.notes = {{1, 48000, 96000}, {1, 144000, std::nullopt}};
                session->chart_.bms_rank = 0;
                session->chart_.duration_samples = 192000;
                gameplay::GameplayConfig config;
                config.practice_no_fail_enabled = true;
                config::ModeConfig mode;
                mode.key_mode = "auto";
                mode.random = "off";
                config.judge = manage_modes(session->chart_, ChartFormat::Bms,
                    mode, config::JudgeConfig{}, 1.0, 120.0, 48000).judge;
                session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
                session->input_offset_samples_ = offset_ms * 48;
                session->audio_timing_diagnostics_logged_ = true;
                session->config_.audio_ui.mute_when_inactive = false;
                session->audio_thread_.callback_ = [&](float* out, uint32_t frames, int64_t start, int64_t playback) {
                    session->audio_callback(out, frames, start, playback);
                };
                std::vector<float> output(buffer_ms * 48 * 2);
                const auto callback = [&](int ms) {
                    session->audio_thread_.invoke_callback(output.data(), buffer_ms * 48,
                        ms * 48, ms * 48, epoch + ms * 1'000'000LL);
                };
                const int head_ms = 1000 - offset_ms;
                // Stay just outside the 24.7619ms RANK0 GREAT window, so a
                // callback crossing the LN tail cannot hide an early drop.
                const int release_ms = 1975 - offset_ms;
                REQUIRE(session->input_thread_.queue().push({32, input::InputState::Pressed, epoch + head_ms * 1'000'000LL}));
                callback(head_ms);
                int next = head_ms + buffer_ms;
                for (; next < release_ms; next += buffer_ms) callback(next);
                REQUIRE(session->input_thread_.queue().push({32, input::InputState::Released, epoch + release_ms * 1'000'000LL}));
                callback(next);
                REQUIRE(session->engine_->replay().events.size() == 2);
                CHECK(session->engine_->replay().events[1].sample == 94800);
                CHECK(session->engine_->stats().counts.pg == 1);
                CHECK(session->engine_->stats().counts.gr == 0);
                CHECK(session->engine_->stats().counts.bd == 1);
                CHECK(session->engine_->stats().osu_od8.counts.miss == 1);
                CHECK(session->engine_->live_feedback().delta_ms == doctest::Approx(-25.0));
                CHECK(session->hud_snapshot().current_sample == next * 48);
            }
        }
    }

    static void check_r5_shared_clock_wobble_and_rebase() {
        constexpr int64_t epoch = 1'000'000'000LL;
        auto session = shift_lane_fixture(32, false);
        session->chart_.notes = {{1, 480000, std::nullopt}};
        session->chart_.duration_samples = 960000;
        gameplay::GameplayConfig config;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->audio_timing_diagnostics_logged_ = true;
        session->config_.audio_ui.mute_when_inactive = false;
        session->audio_thread_.callback_ = [&](float* out, uint32_t frames, int64_t start, int64_t playback) {
            session->audio_callback(out, frames, start, playback);
        };
        std::vector<float> output(4800 * 2);
        int64_t previous = 0;
        bool saw_filtered_difference = false;
        bool saw_continuity_clamp = false;
        for (int i = 1; i <= 100; ++i) {
            // Small accepted wobble, then a 50 ms persistent wall-clock gap.
            // Device samples remain strictly increasing; a wide synthetic write
            // buffer keeps the unrelated future-input guard out of this test.
            const int64_t qpc = epoch + (i * 10 + (i >= 70 ? 50 : 0)) * 1'000'000LL;
            const int64_t raw = i * 480 + (i < 60 ? (i % 2 ? 2 : -2) : 0);
            REQUIRE(session->input_thread_.queue().push({32,
                i % 2 ? input::InputState::Pressed : input::InputState::Released, qpc}));
            session->audio_thread_.invoke_callback(output.data(), 4800, raw, raw, qpc);
            const auto hud = session->hud_snapshot();
            REQUIRE(session->engine_->replay().events.size() == static_cast<std::size_t>(i));
            CHECK(session->engine_->replay().events.back().sample == hud.current_sample);
            CHECK(hud.audio_sample_time_ns == qpc);
            CHECK(hud.current_sample >= previous);
            saw_filtered_difference |= hud.current_sample != raw;
            saw_continuity_clamp |= session->shared_input_clock_.rate() < 1.0;
            previous = hud.current_sample;
        }
        CHECK(saw_filtered_difference);
        CHECK(saw_continuity_clamp);
        session->paused_ = true;
        session->audio_thread_.invoke_callback(output.data(), 4800, 48480, 48480, epoch + 1'060'000'000LL);
        const int64_t frozen = session->hud_snapshot().current_sample;
        session->audio_thread_.invoke_callback(output.data(), 4800, 48960, 48960, epoch + 1'070'000'000LL);
        CHECK(session->hud_snapshot().current_sample == frozen);
        CHECK(frozen >= previous);

        timing::ClockSync clock;
        clock.add_sample(epoch, 0);
        clock.add_sample(epoch + 1'000'000'000LL, 48000);
        CHECK(clock.estimate_audio_samples(epoch + 2'000'000'000LL).value() == 96000);
        CHECK(clock.input_to_audio_samples(epoch + 500'000'000LL).value() == 24000);
    }

    static void check_r5_continuous_clock_bounds() {
        constexpr int rate = 48000;
        constexpr int64_t epoch = 1'000'000'000LL;
        for (const double drift : {-0.001, 0.0, 0.001}) {
            for (const int wobble_ms : {0, 2, 5, 10}) {
                GameplayInputClock clock;
                int64_t previous_frame = 0;
                int64_t last_anchor_sample = 0;
                int64_t last_anchor_ns = epoch;
                double last_rate = 1.0;
                double max_phase_ms = 0.0;
                double final_phase_ms = 0.0;
                int recovered_ms = -1;
                for (int tick = 0; tick <= 600; ++tick) {
                    const int64_t elapsed_ms = tick * 20 + (tick >= 50 ? 50 : 0);
                    const int64_t qpc = epoch + elapsed_ms * 1'000'000LL;
                    // Wobble stops before the clock step so recovery has a
                    // stable target, including +/-1000 ppm source-rate drift.
                    const int delay = tick < 40 && tick % 2 ? wobble_ms : 0;
                    const int64_t raw = static_cast<int64_t>(std::llround(
                        (tick * 20 - delay) * 48.0 * (1.0 + drift)));
                    if (tick > 0) {
                        const int64_t before_qpc = qpc - 1'000'000LL;
                        const int64_t displayed = last_anchor_sample + static_cast<int64_t>(std::llround(
                            (before_qpc - last_anchor_ns) * (rate * last_rate / 1'000'000'000.0)));
                        const int64_t mapped = clock.sample_at(before_qpc).value();
                        CHECK(std::abs(displayed - mapped) <= 1);
                        CHECK(displayed >= previous_frame);
                        previous_frame = displayed;
                    }
                    const int64_t anchor = clock.observe(qpc, raw, rate);
                    CHECK(anchor >= previous_frame);
                    CHECK(clock.sample_at(qpc).value() == anchor);
                    CHECK(clock.rate() >= 0.98);
                    CHECK(clock.rate() <= 1.02);
                    last_anchor_sample = anchor;
                    last_anchor_ns = qpc;
                    last_rate = clock.rate();
                    previous_frame = anchor;
                    final_phase_ms = std::abs(anchor - raw) / 48.0;
                    max_phase_ms = (std::max)(max_phase_ms, final_phase_ms);
                    if (tick >= 50 && recovered_ms < 0 && final_phase_ms <= 1.0)
                        recovered_ms = (tick - 50) * 20;
                }
                CHECK(recovered_ms >= 0);
                CHECK(recovered_ms <= 6000);
                CHECK(final_phase_ms <= 1.0);
                std::cout << "[R5-continuous-clock] drift_ppm=" << drift * 1'000'000.0
                          << " wobble_ms=" << wobble_ms << " max_phase_ms=" << max_phase_ms
                          << " recovered_ms=" << recovered_ms << " final_phase_ms=" << final_phase_ms << '\n';
                clock.reset(last_anchor_sample);
                CHECK(clock.observe(last_anchor_ns + 3'000'000'000LL, last_anchor_sample, rate) == last_anchor_sample);
                CHECK(clock.sample_at(last_anchor_ns + 3'010'000'000LL).value() == last_anchor_sample + 480);
            }
        }
        // Exercise the actual ring overwrite, including a fresh queued event
        // from 300 ms ago at a callback rate of 4 kHz.
        GameplayInputClock wrapped;
        for (int64_t i = 0; i <= 5000; ++i) {
            CHECK(wrapped.observe(epoch + i * 250'000, i * 12, rate) == i * 12);
            if (i >= 1200) CHECK(wrapped.sample_at(epoch + (i - 1200) * 250'000).value() == (i - 1200) * 12);
        }
        const int64_t last_time = epoch + 5000 * 250'000LL;
        CHECK(wrapped.observe(last_time, -1000, rate) == 60000);
        CHECK(wrapped.observe(last_time - 250'000, -1000, rate) == 60000);
        CHECK(wrapped.sample_at(last_time + 250'000).value() == 60012);
        CHECK(wrapped.observe(last_time + 250'000, 60012, rate) == 60012);
    }

    static void check_r5_scheduled_audio_and_historical_frontier() {
        for (const int mode : {0, 1, 2, 3}) {
            auto session = shift_lane_fixture(32, false);
            session->chart_.notes = {{1, 48000, 96000}, {1, 144000, std::nullopt}};
            session->chart_.duration_samples = 192000;
            session->chart_.notes[0].audio_asset_id = 0;
            session->chart_.notes[1].note_id = 1;
            session->chart_audio_assets_.resize(1);
            session->chart_audio_assets_[0].clip.samples = std::make_shared<const std::vector<float>>(1920, 0.25f);
            session->audio_timing_diagnostics_logged_ = true;
            session->config_.audio_ui.mute_when_inactive = false;
            session->config_.audio_ui.normalize_audio = false;
            session->replay_playback_enabled_ = mode >= 2;
            session->autoplay_enabled_ = mode == 0;
            session->auto_scratch_enabled_ = mode == 1;
            if (mode == 1) {
                session->chart_format_ = ChartFormat::Bms;
                session->chart_.scratch_lanes = {1};
            }
            session->chronological_judgement_ = mode != 3;
            session->replay_source_.ruleset_id = std::string(mode == 3 ? kRuleset4ReplayRulesetId : kCanonicalReplayRulesetId);
            session->replay_source_.trace.sample_rate = 48000;
            session->replay_source_.trace.events = {{1, input::InputState::Pressed, 48000}, {1, input::InputState::Released, 96000}};
            gameplay::GameplayConfig config;
            config.practice_no_fail_enabled = true;
            config.chronological_judgement = session->chronological_judgement_;
            session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
            session->build_autoplay_events();
            session->prepare_scheduled_input_audio(config);
            session->audio_thread_.callback_ = [&](float* out, uint32_t frames, int64_t start, int64_t playback) {
                session->audio_callback(out, frames, start, playback);
            };
            std::vector<float> output(1920 * 2);
            session->audio_thread_.invoke_callback(output.data(), 1920, 47520, 46080, 1'960'000'000LL);
            CHECK(session->engine_->stats().counts.pg == (mode == 3 ? 1 : 0));
            CHECK(std::all_of(output.begin(), output.begin() + 480 * 2, [](float value) { return value == 0.0f; }));
            CHECK(std::any_of(output.begin() + 480 * 2, output.end(), [](float value) { return value != 0.0f; }));
            session->audio_thread_.invoke_callback(output.data(), 1920, 49440, 48000, 2'000'000'000LL);
            CHECK(session->engine_->stats().counts.pg == 1);
            CHECK(std::all_of(output.begin(), output.end(), [](float value) { return value == 0.0f; }));
        }
    }

    static void check_callback_timestamp_pair_delays() {
        constexpr int64_t playback_sample = 48000;
        constexpr uint32_t frames = 480;
        const int64_t paired_time_ns = timing::HighResClock::now_ns();
        for (const int delay_ms : {0, 1, 3, 9}) {
            auto session = shift_lane_fixture(32, false);
            session->chart_.notes.push_back({1, 150000, std::nullopt});
            session->chart_.duration_samples = 200000;
            gameplay::GameplayConfig config;
            config.sample_rate = 48000;
            config.practice_no_fail_enabled = true;
            session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
            session->audio_timing_diagnostics_logged_ = true;
            session->config_.audio_ui.mute_when_inactive = false;
            // The fit is deliberately live: the real callback supplies its
            // second point. Using delivery-time QPC would shift this input.
            session->clock_sync_.add_sample(paired_time_ns - 1'000'000'000LL, 0);
            REQUIRE(session->input_thread_.queue().push(
                {32, input::InputState::Pressed, paired_time_ns}));
            session->audio_thread_.callback_ = [&](float* output, uint32_t count,
                                                   int64_t start, int64_t playback) {
                CHECK(session->audio_thread_.callback_playback_time_ns() == paired_time_ns);
                session->audio_callback(output, count, start, playback);
            };
            std::array<float, frames * 2> output{};
            // Inject only additional delivery latency. Every case deliberately
            // receives the same observed sample/QPC pair; elapsed wall time is
            // not an assertion, so scheduler oversleep cannot make this flaky.
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            session->audio_thread_.invoke_callback(
                output.data(), frames, playback_sample, playback_sample, paired_time_ns);
            CHECK(session->audio_thread_.callback_playback_time_ns() == 0);
            const auto timing_state = session->last_audio_timing_.load();
            CHECK(timing_state.playback_sample == playback_sample);
            CHECK(timing_state.time_ns == paired_time_ns);
            CHECK(session->startup_input_timing_anchor_.callback_time_ns == paired_time_ns);
            CHECK(session->clock_sync_.input_to_audio_samples(paired_time_ns) == playback_sample);
            const auto hud = session->hud_snapshot();
            CHECK(hud.current_sample == playback_sample);
            CHECK(hud.audio_sample_time_ns == paired_time_ns);
            REQUIRE(session->engine_->replay().events.size() == 1);
            CHECK(session->engine_->replay().events.front().sample == playback_sample);
            REQUIRE(session->engine_->live_feedback().has_value);
            CHECK(session->engine_->live_feedback().delta_ms == doctest::Approx(0.0));
            std::cout << "[callback-timestamp-pair] additional_delay_ms=" << delay_ms
                      << " replay_sample=" << session->engine_->replay().events.front().sample
                      << " feedback_delta_ms=" << session->engine_->live_feedback().delta_ms
                      << " hud_pair_time_matches=" << (hud.audio_sample_time_ns == paired_time_ns)
                      << '\n';
        }
    }

    static void check_callback_timestamp_lifetime_and_fallback() {
        auto session = shift_lane_fixture(32);
        session->audio_timing_diagnostics_logged_ = true;
        session->config_.audio_ui.mute_when_inactive = false;
        constexpr uint32_t frames = 480;
        std::array<float, frames * 2> output{};
        const int64_t old_pair_ns = timing::HighResClock::now_ns() - 1'000'000'000LL;
        session->audio_thread_.callback_ = [&](float*, uint32_t, int64_t, int64_t) {
            CHECK(session->audio_thread_.callback_playback_time_ns() == old_pair_ns);
            throw std::runtime_error("callback timestamp cleanup fixture");
        };
        bool caught_runtime_error = false;
        try {
            session->audio_thread_.invoke_callback(
                output.data(), frames, 48000, 48000, old_pair_ns);
        } catch (const std::runtime_error&) {
            caught_runtime_error = true;
        }
        CHECK(caught_runtime_error);
        CHECK(session->audio_thread_.callback_playback_time_ns() == 0);

        // A direct callback after the throwing callback must use its own QPC,
        // never inherit the previous WASAPI observation.
        const int64_t before_ns = timing::HighResClock::now_ns();
        session->audio_callback(output.data(), frames, 48000, 48000);
        const int64_t after_ns = timing::HighResClock::now_ns();
        const auto state = session->last_audio_timing_.load();
        CHECK(state.time_ns >= before_ns);
        CHECK(state.time_ns <= after_ns);
        CHECK(session->hud_snapshot().audio_sample_time_ns == state.time_ns);
        session->audio_thread_.stop();
        CHECK(session->audio_thread_.callback_playback_time_ns() == 0);
        session->audio_thread_.shutdown();
        CHECK(session->audio_thread_.callback_playback_time_ns() == 0);
    }

    static void check_asio_callback_timestamp_fallback() {
        auto session = shift_lane_fixture(32);
        session->audio_timing_diagnostics_logged_ = true;
        session->config_.audio_ui.mute_when_inactive = false;
        auto driver = std::make_shared<tenriff::tests::MockState>();
        session->audio_thread_.asio_backend_ = std::make_unique<audio::AsioBackend>(
            tenriff::tests::mock_factory(driver));
        audio::AudioConfig config;
        config.backend = audio::AudioBackend::ASIO;
        int callbacks = 0;
        REQUIRE(session->audio_thread_.asio_backend_->initialize(config,
            [&](float* output, uint32_t frames, int64_t start, int64_t playback) {
                ++callbacks;
                CHECK(session->audio_thread_.callback_playback_time_ns() == 0);
                const int64_t before_ns = timing::HighResClock::now_ns();
                session->audio_callback(output, frames, start, playback);
                const int64_t after_ns = timing::HighResClock::now_ns();
                const auto state = session->last_audio_timing_.load();
                CHECK(state.playback_sample == playback);
                CHECK(state.time_ns >= before_ns);
                CHECK(state.time_ns <= after_ns);
                CHECK(session->hud_snapshot().audio_sample_time_ns == state.time_ns);
            }) == audio::AudioResult::Success);
        REQUIRE(session->audio_thread_.start() == audio::AudioResult::Success);
        driver->tick();
        driver->tick(1, true);
        CHECK(callbacks == 2);
        session->audio_thread_.stop();
        CHECK(session->audio_thread_.callback_playback_time_ns() == 0);
        session->audio_thread_.shutdown();
        CHECK(session->audio_thread_.callback_playback_time_ns() == 0);
        CHECK(driver->correct_owner);
    }

    static void check_input_offset_callback_phase(int offset_ms) {
        constexpr int sample_rate = 48000;
        constexpr int64_t note_sample = 48000;
        constexpr uint32_t callback_frames = 480;
        constexpr std::array<int64_t, 4> phases{{0, 120, 240, 360}};
        const int64_t event_time_ns = timing::HighResClock::now_ns();
        const int64_t offset_samples = offset_ms * 48;
        std::array<int64_t, phases.size()> recorded_samples{};
        std::array<double, phases.size()> feedback_deltas{};
        std::array<int64_t, phases.size()> keysound_samples{};
        std::array<bool, phases.size()> keysound_mixed{};

        for (std::size_t i = 0; i < phases.size(); ++i) {
            auto session = std::make_unique<GameSession>();
            session->sample_rate_ = sample_rate;
            session->chart_.lane_count = 1;
            session->chart_.duration_samples = 200000;
            session->chart_.notes = {{1, note_sample, std::nullopt}, {1, 150000, std::nullopt}};
            session->chart_.notes[0].note_id = 0;
            session->chart_.notes[0].audio_asset_id = 0;
            session->chart_.notes[1].note_id = 1;
            gameplay::GameplayConfig config;
            config.sample_rate = sample_rate;
            config.practice_no_fail_enabled = true;
            session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
            session->lane_activity_.assign(1, 0.0f);
            session->lane_pressed_.assign(1, 0);
            session->hidden_hit_note_ids_.assign(2, 0);
            session->synthetic_tones_enabled_ = false;
            session->audio_timing_diagnostics_logged_ = true;
            session->config_.audio_ui.mute_when_inactive = false;
            session->config_.audio_ui.normalize_audio = false;
            session->chart_audio_assets_.resize(1);
            session->chart_audio_assets_[0].clip.samples =
                std::make_shared<const std::vector<float>>(1920, 0.25f);
            session->input_offset_samples_ = offset_samples;
            session->key_to_lane_ = {{32, 1}};
            session->lane_binding_state_.configure(session->key_to_lane_);

            // Freeze only the learned clock fit so each callback consumes the
            // exact same stamped input at note_sample. Varying callback phase
            // must not quantize the user's constant input offset. This tests
            // production queue/judgement/mixer code, not a physical device clock.
            timing::ClockSyncConfig fit;
            fit.ema_alpha = 0.0;
            session->clock_sync_ = timing::ClockSync(fit);
            session->clock_sync_.add_sample(event_time_ns - 1'000'000'000LL, 0);
            session->clock_sync_.add_sample(event_time_ns, note_sample);
            REQUIRE(session->clock_sync_.input_to_audio_samples(event_time_ns) == note_sample);
            REQUIRE(session->input_thread_.queue().push(
                {32, input::InputState::Pressed, event_time_ns}));
            std::vector<float> audio(callback_frames * 2);
            const int64_t buffer_start = note_sample + phases[i];
            session->audio_thread_.callback_ = [&](float* out, uint32_t frames, int64_t start, int64_t playback) {
                session->audio_callback(out, frames, start, playback);
            };
            session->audio_thread_.invoke_callback(audio.data(), callback_frames, buffer_start, buffer_start,
                event_time_ns + phases[i] * 1'000'000'000LL / sample_rate);
            REQUIRE(session->engine_->replay().events.size() == 1);
            recorded_samples[i] = session->engine_->replay().events.front().sample;
            REQUIRE(session->engine_->live_feedback().has_value);
            feedback_deltas[i] = session->engine_->live_feedback().delta_ms;
            REQUIRE(session->chart_audio_voices_.size() == 1);
            keysound_samples[i] = session->chart_audio_voices_[0].start_sample;
            keysound_mixed[i] = std::any_of(audio.begin(), audio.end(),
                [](float value) { return value != 0.0f; });
        }

        bool exact_offset = true;
        for (std::size_t i = 0; i < phases.size(); ++i) {
            std::cout << "[input-offset-phase] offset_ms=" << offset_ms
                      << " callback_phase_samples=" << phases[i]
                      << " replay_sample=" << recorded_samples[i]
                      << " feedback_delta_ms=" << feedback_deltas[i]
                      << " keysound_sample=" << keysound_samples[i]
                      << " keysound_in_first_callback=" << keysound_mixed[i] << '\n';
            exact_offset &= recorded_samples[i] == note_sample + offset_samples;
            exact_offset &= std::abs(feedback_deltas[i] - offset_ms) < 1e-9;
            exact_offset &= keysound_samples[i] == note_sample + phases[i];
            exact_offset &= keysound_mixed[i];
        }
        CHECK(exact_offset);
    }

    static void check_input_offset_future_clock_guard() {
        constexpr int64_t raw_sample = 48000;
        constexpr int64_t buffer_start = 47040;
        constexpr int64_t buffer_end = 47520;
        constexpr int64_t lookahead = 192;
        constexpr int64_t event_time_ns = 1'000'000'000LL;
        for (const int offset_ms : {-200, -20, 0, 20, 200}) {
            auto session = shift_lane_fixture(32, false);
            session->chart_.notes.push_back({1, 200000, std::nullopt});
            session->chart_.duration_samples = 240000;
            gameplay::GameplayConfig config;
            config.practice_no_fail_enabled = true;
            session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
            session->input_offset_samples_ = offset_ms * 48;
            // Both clock sources agree on a raw event ahead of the write
            // buffer. The safety bound must apply even when a negative user
            // offset would otherwise disguise that implausible timestamp.
            session->startup_input_timing_anchor_ = {raw_sample, event_time_ns, true};
            session->current_playback_sample_ = raw_sample;
            REQUIRE(session->input_thread_.queue().push(
                {32, input::InputState::Pressed, event_time_ns}));
            session->process_input_queue(buffer_start, buffer_end, lookahead);
            REQUIRE(session->engine_->replay().events.size() == 1);
            CHECK(session->engine_->replay().events[0].sample ==
                buffer_end + session->input_offset_samples_);
            CHECK(session->future_events_.empty());
        }
    }

    static void check_input_offset_order_and_live_audio() {
        constexpr int64_t event_time_ns = 1'000'000'000LL;
        constexpr std::array<int64_t, 4> samples{{48000, 48048, 48096, 48144}};
        for (const int offset_ms : {-200, 0, 200}) {
            auto session = shift_lane_fixture(32);
            // Keep the session alive after an early LN release so all four
            // physical edges must remain visible in the recorded input order.
            session->chart_.notes.push_back({1, 200000, std::nullopt});
            session->chart_.duration_samples = 240000;
            gameplay::GameplayConfig config;
            config.practice_no_fail_enabled = true;
            session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
            session->input_offset_samples_ = offset_ms * 48;
            session->synthetic_tones_enabled_ = true;
            session->startup_input_timing_anchor_ = {samples.back(),
                event_time_ns + 3'000'000LL, true};
            session->current_playback_sample_ = samples.back();
            for (std::size_t i = 0; i < samples.size(); ++i) {
                REQUIRE(session->input_thread_.queue().push({32,
                    i % 2 ? input::InputState::Released : input::InputState::Pressed,
                    event_time_ns + static_cast<int64_t>(i) * 1'000'000LL}));
            }
            session->process_input_queue(48000, 48480, 192);
            const auto& replay = session->engine_->replay().events;
            REQUIRE(replay.size() == samples.size());
            for (std::size_t i = 0; i < samples.size(); ++i) {
                CHECK(replay[i].sample == samples[i] + session->input_offset_samples_);
                CHECK(replay[i].state ==
                    (i % 2 ? input::InputState::Released : input::InputState::Pressed));
            }
            // Immediate physical feedback uses the uncalibrated timestamp;
            // no new deferred queue or delayed synthetic key tone is introduced.
            REQUIRE(session->tone_voices_.size() == 2);
            CHECK(session->tone_voices_[0].start_sample == samples[0]);
            CHECK(session->tone_voices_[1].start_sample == samples[2]);
            CHECK(session->input_thread_.queue().empty());
            CHECK(session->future_events_.empty());
        }
    }

    static void check_input_offset_recorded_audio_unchanged() {
        constexpr int64_t recorded_sample = 48144;
        for (const auto ruleset : {kLegacyReplayRulesetId, kPreviousReplayRulesetId,
                                   kRuleset3ReplayRulesetId, kRuleset4ReplayRulesetId, kCanonicalReplayRulesetId}) {
            auto session = shift_lane_fixture(32);
            session->input_offset_samples_ = 9600;
            session->synthetic_tones_enabled_ = true;
            session->replay_playback_enabled_ = true;
            session->replay_source_.ruleset_id = std::string(ruleset);
            session->replay_source_.trace.sample_rate = 48000;
            session->replay_source_.trace.events =
                {{1, input::InputState::Pressed, recorded_sample}};
            session->process_input_queue(48000, 48480, 192);
            REQUIRE(session->engine_->replay().events.size() == 1);
            CHECK(session->engine_->replay().events[0].sample == recorded_sample);
            REQUIRE(session->tone_voices_.size() == 1);
            CHECK(session->tone_voices_[0].start_sample == recorded_sample);
        }
        auto autoplay = shift_lane_fixture(32);
        autoplay->input_offset_samples_ = 9600;
        autoplay->synthetic_tones_enabled_ = true;
        autoplay->autoplay_enabled_ = true;
        autoplay->autoplay_events_ = {{1, input::InputState::Pressed, recorded_sample}};
        autoplay->process_autoplay_queue(48000, 48480, 192);
        REQUIRE(autoplay->engine_->replay().events.size() == 1);
        CHECK(autoplay->engine_->replay().events[0].sample == recorded_sample);
        REQUIRE(autoplay->tone_voices_.size() == 1);
        CHECK(autoplay->tone_voices_[0].start_sample == recorded_sample);
    }

    static void check_loading_does_not_open_audio(bool cancel_parse) {
        CompletionTestDirectory directory;
        config::ConfigLoader loader;
        auto config = loader.defaults();
        // A nonexistent driver makes an accidental early device open fail
        // deterministically, without claiming or reconfiguring real hardware.
        config.audio.backend = audio::AudioBackend::ASIO;
        config.audio.asio_driver = "invalid-loading-regression-driver";
        REQUIRE(loader.save_profile(directory.path.u8string(), config));
        CommandLineOptions options;
        options.profile = directory.path.u8string();
        options.chart_path = (directory.path / "missing.bms").u8string();
        auto session = std::make_unique<GameSession>();
        bool parsing = false;
        session->set_loading_progress_callback([&](const GameSession::LoadingProgress& progress) {
            if (progress.stage == "Parsing chart") {
                parsing = true;
                CHECK(session->audio_thread_.sample_rate() == 0);
            }
        });
        session->set_loading_cancel_callback([&] { return cancel_parse && parsing; });
        CHECK_FALSE(session->initialize(options));
        CHECK(parsing);
        CHECK(session->audio_error().empty());
        CHECK(session->was_user_aborted() == cancel_parse);
        CHECK(session->audio_thread_.sample_rate() == 0);
    }

    static void check_session_judge_handoff() {
        CompletionTestDirectory directory;
        config::ConfigLoader loader;
        auto disk = loader.defaults();
        disk.audio.backend = audio::AudioBackend::ASIO;
        disk.audio.asio_driver = "invalid-loading-regression-driver";
        // Stale disk evidence is never enough to enable a transient judge mod.
        disk.mode.mods = {"judge_hard", "no_ln_release"};
        for (int scenario = 0; scenario < 6; ++scenario) {
            REQUIRE(loader.save_profile(directory.path.u8string(), disk));
            auto session = std::make_unique<GameSession>();
            if (scenario == 1 || scenario == 3)
                session->set_session_mode_mods({"judge_hard"});
            if (scenario == 2) session->set_session_mode_mods({"judge_easy"});
            if (scenario == 3) session->set_peer_battle_mode(true);
            if (scenario == 4) session->set_session_mode_mods({});
            if (scenario == 5) session->set_session_mode_mods({"judge_ex"});
            CommandLineOptions options;
            options.profile = directory.path.u8string();
            options.chart_path = (directory.path / "missing.bms").u8string();
            bool parsing = false;
            session->set_loading_progress_callback([&](const GameSession::LoadingProgress& progress) {
                parsing |= progress.stage == "Parsing chart";
            });
            session->set_loading_cancel_callback([&] { return parsing; });
            CHECK_FALSE(session->initialize(options));
            CHECK(session->audio_thread_.sample_rate() == 0);
            const auto expected = scenario == 1 ? "Judge Hard" : scenario == 2 ? "Judge Easy" :
                                  scenario == 5 ? "Judge EX" : "Off";
            CHECK(mode_mod_category_value("judge_window", session->config_.mode.mods) == expected);
            if (scenario != 3)
                CHECK(std::find(session->config_.mode.mods.begin(), session->config_.mode.mods.end(),
                                "no_ln_release") != session->config_.mode.mods.end());
            const auto persisted = loader.load_profile(directory.path.u8string());
            REQUIRE(persisted.success());
            CHECK(mode_mod_category_value("judge_window", persisted.config.mode.mods) == "Off");
        }
        for (const bool legacy : {true, false}) {
            REQUIRE(loader.save_profile(directory.path.u8string(), disk));
            gameplay::ReplayFile original;
            original.replay_format_version = legacy ? 0 : gameplay::kReplayFormatVersion;
            original.ruleset_id = legacy ? "tenriff-native-score-v2-ruleset-1"
                                         : std::string(kCanonicalReplayRulesetId);
            original.chart_sha256 = std::string(64, 'a');
            original.sample_rate = original.trace.sample_rate = 48000;
            original.trace.lane_count = 4;
            original.trace.duration_samples = 48000;
            if (!legacy) original.mods = {"judge_easy"};
            const auto replay_path = (directory.path / "original-replay.json").u8string();
            REQUIRE(gameplay::save_replay_json(replay_path, original).success());
            auto session = std::make_unique<GameSession>();
            session->set_session_mode_mods({"judge_hard"});
            CommandLineOptions options;
            options.profile = directory.path.u8string();
            options.chart_path = (directory.path / "missing.bms").u8string();
            options.replay_path = replay_path;
            bool parsing = false;
            session->set_loading_progress_callback([&](const GameSession::LoadingProgress& progress) {
                parsing |= progress.stage == "Parsing chart";
            });
            session->set_loading_cancel_callback([&] { return parsing; });
            CHECK_FALSE(session->initialize(options));
            CHECK(parsing);
            CHECK(session->replay_playback_enabled_);
            CHECK(session->audio_thread_.sample_rate() == 0);
            CHECK(session->replay_source_.ruleset_id == original.ruleset_id);
            CHECK(session->replay_source_.mods == original.mods);
            CHECK(mode_mod_category_value("judge_window", session->config_.mode.mods) ==
                  (legacy ? "Off" : "Judge Easy"));
        }
    }

    static void check_riff_map_snapshot_lifecycle() {
        const auto prepare = [](GameSession& session) {
            session.chart_.lane_count = 1;
            session.chart_.duration_samples = 64 * 48000;
            // Far more heads than the visible-note snapshot can hold; the map
            // must cover the whole chart, including the final timestamp.
            for (int i = 0; i < 256; ++i) {
                session.chart_.notes.push_back({1, i * 12000, std::nullopt});
            }
            session.chart_.notes.push_back({1, session.chart_.duration_samples, std::nullopt});
            gameplay::GameplayConfig config;
            session.engine_ = std::make_unique<gameplay::GameplayEngine>(session.chart_, config);
            session.prepare_riff_map();
        };
        auto first = std::make_unique<GameSession>();
        prepare(*first);
        auto snapshot = first->hud_snapshot();
        REQUIRE(snapshot.riff_map_count == kGameplayRiffMapBins);
        CHECK(snapshot.riff_map[0] == 204);
        CHECK(snapshot.riff_map[63] == 255);
        CHECK(snapshot.note_count < first->chart_.notes.size());
        CHECK(snapshot.duration_samples == first->chart_.duration_samples);
        const auto first_revision = snapshot.riff_map_revision;
        REQUIRE(first_revision != 0);
        CHECK(first->hud_snapshot().riff_map_revision == first_revision);

        first->chart_.notes.clear();
        first->prepare_riff_map();
        snapshot = first->hud_snapshot();
        CHECK(snapshot.riff_map_count == 0);
        CHECK(snapshot.riff_map_revision > first_revision);
        CHECK(std::all_of(snapshot.riff_map.begin(), snapshot.riff_map.end(),
                          [](uint8_t value) { return value == 0; }));

        auto next_song = std::make_unique<GameSession>();
        prepare(*next_song);
        CHECK(next_song->hud_snapshot().riff_map_revision > snapshot.riff_map_revision);
    }

    static void check_fixed_baseline_player_and_ghost_boundaries() {
        const auto play = [](int rank, std::string_view judge_mod, int64_t delta, std::string_view ghost_ruleset) {
            auto session = std::make_unique<GameSession>();
            session->sample_rate_ = 48000;
            session->chart_.lane_count = 1;
            session->chart_.bms_rank = rank;
            session->chart_.duration_samples = 480000;
            session->chart_.notes = {{1, 48000, std::nullopt}};
            session->chart_.notes[0].note_id = 0;
            config::ModeConfig mode;
            mode.key_mode = "auto";
            mode.random = "off";
            if (!judge_mod.empty()) mode.mods = {std::string(judge_mod)};
            const auto managed = manage_modes(session->chart_, ChartFormat::Bms,
                mode, config::JudgeConfig{}, 1.0, 120.0, 48000);
            gameplay::GameplayConfig live;
            live.sample_rate = 48000;
            live.practice_no_fail_enabled = true;
            live.judge = managed.judge;
            live.chronological_judgement = true;
            session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, live);
            session->ghost_replay_source_.ruleset_id = std::string(ghost_ruleset);
            // Explicit scoring policy still applies to unverified old-format
            // evidence: absence of a hash must not switch R3/R4 to R1 windows.
            session->ghost_replay_source_.replay_format_version = 0;
            session->ghost_replay_source_.mods = mode.mods;
            session->ghost_replay_source_.trace.sample_rate = 48000;
            session->ghost_replay_source_.trace.events = {{1, input::InputState::Pressed, 48000 + delta}};
            auto ghost = live;
            ghost.judge = replay_judge_config_for_playback(
                session->ghost_replay_source_, config::JudgeConfig{}, rank);
            ghost.legacy_hold_release = replay_uses_legacy_hold_release(session->ghost_replay_source_);
            ghost.chronological_judgement = replay_uses_chronological_judgement(session->ghost_replay_source_);
            session->ghost_engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, ghost);
            session->ghost_replay_enabled_ = true;
            session->lane_activity_.assign(1, 0); session->lane_pressed_.assign(1, 0);
            session->ghost_lane_activity_.assign(1, 0); session->ghost_lane_pressed_.assign(1, 0);
            session->hidden_hit_note_ids_.assign(1, 0);
            session->ghost_hidden_hit_note_ids_.assign(1, 0);
            session->synthetic_tones_enabled_ = false;
            session->audio_timing_diagnostics_logged_ = true;
            session->config_.audio_ui.mute_when_inactive = false;
            session->key_to_lane_ = {{32, 1}};
            session->lane_binding_state_.configure(session->key_to_lane_);
            input::InputEvent press{};
            press.keycode = 32; press.state = input::InputState::Pressed;
            const int64_t sample = 48000 + delta;
            REQUIRE(session->future_events_.push({press, sample}));
            float output[2]{};
            session->audio_callback(output, 1, sample, sample);
            return session;
        };
        const double normal[][4] = {{10, 65.0 * 8 / 21, 115.0 * 8 / 21, 80},
            {15, 65.0 * 15 / 21, 115.0 * 15 / 21, 150},
            {19, 65.0 * 18 / 21, 115.0 * 18 / 21, 180}, {22, 65, 115, 210}};
        for (int rank = 0; rank <= 3; ++rank) {
            const double rank_scale = std::array<double, 4>{8.0, 15.0, 18.0, 21.0}[rank] / 21.0;
            for (const std::string_view judge_mod : {"", "judge_easy", "judge_hard", "judge_ex"}) {
                const bool ex = judge_mod == "judge_ex";
                const bool hard = judge_mod == "judge_hard";
                const bool easy = judge_mod == "judge_easy";
                const double* pinned = normal[ex ? 0 : hard ? 2 : rank];
                const std::array<double, 4> windows = easy
                    ? std::array<double, 4>{21.0 * 1.35 * rank_scale, 65.0 * 1.35 * rank_scale,
                                           115.0 * 1.35 * rank_scale, 210.0}
                    : std::array<double, 4>{pinned[0], pinned[1], pinned[2], pinned[3]};
                for (std::size_t boundary = 0; boundary < windows.size(); ++boundary) {
                    for (const int outside : {0, 1}) for (const int sign : {-1, 1}) {
                        const int64_t delta = sign * (static_cast<int64_t>(std::llround(windows[boundary] * 48.0)) + outside);
                        auto session = play(rank, judge_mod, delta, ex ? kExReplayRulesetId : kCanonicalReplayRulesetId);
                        const auto& player = session->engine_->stats();
                        const auto& ghost = session->ghost_engine_->stats();
                        const std::array<int, 5> actual{player.counts.pg, player.counts.gr, player.counts.gd,
                                                       player.counts.bd, player.counts.pr};
                        const std::array<int, 5> replayed{ghost.counts.pg, ghost.counts.gr, ghost.counts.gd,
                                                         ghost.counts.bd, ghost.counts.pr};
                        CHECK(actual == replayed);
                        CHECK(player.raw_score == ghost.raw_score);
                        CHECK(player.delta_samples == ghost.delta_samples);
                        const bool hit = boundary < 3 || outside == 0;
                        CHECK(session->hidden_hit_note_ids_[0] == (hit ? 1 : 0));
                        CHECK(session->ghost_hidden_hit_note_ids_[0] == (hit ? 1 : 0));
                        if (hit) {
                            const auto expected = boundary + outside;
                            for (std::size_t index = 0; index < actual.size(); ++index)
                                CHECK(actual[index] == (index == expected ? 1 : 0));
                            CHECK(player.mean_delta_ms == doctest::Approx(static_cast<double>(delta) / 48.0));
                            CHECK(ghost.mean_delta_ms == doctest::Approx(player.mean_delta_ms));
                        }
                    }
                }
            }
        }
        // A historical R3 ghost at 20ms must stay PG while today's Hard player
        // gets GR; "same ghost mod" does not mean relabeling its old ruleset.
        auto mixed = play(3, "judge_hard", 20 * 48, kRuleset3ReplayRulesetId);
        CHECK(mixed->engine_->stats().counts.gr == 1);
        CHECK(mixed->ghost_engine_->stats().counts.pg == 1);
        auto old_normal = play(0, "", 9 * 48, kRuleset7ReplayRulesetId);
        CHECK(old_normal->engine_->stats().counts.pg == 1);
        CHECK(old_normal->ghost_engine_->stats().counts.gr == 1);
        auto old_hard = play(3, "judge_hard", 18 * 48, kRuleset7ReplayRulesetId);
        CHECK(old_hard->engine_->stats().counts.pg == 1);
        CHECK(old_hard->ghost_engine_->stats().counts.gr == 1);
        auto old_ex = play(3, "judge_ex", 12 * 48, kRuleset8ReplayRulesetId);
        CHECK(old_ex->engine_->stats().counts.gr == 1);
        CHECK(old_ex->ghost_engine_->stats().counts.pg == 1);
    }

    static void check_long_hold_hud_scan() {
        const bool benchmark = std::getenv("TENRIFF_BENCH_GHOST_HUD") != nullptr;
        const char* requested_notes = std::getenv("TENRIFF_BENCH_GHOST_HUD_NOTES");
        const int past_notes = benchmark && requested_notes
            ? std::clamp(std::atoi(requested_notes), 1000, 100000) : benchmark ? 100000 : 1000;
        const bool paced = std::getenv("TENRIFF_BENCH_GHOST_HUD_PACED") != nullptr;
        const int64_t now = static_cast<int64_t>(past_notes + 10) * 480;
        for (const bool ghost : {false, true}) {
            auto session = std::make_unique<GameSession>();
            session->sample_rate_ = 48000;
            session->chart_.lane_count = 10;
            session->chart_.duration_samples = now + 480000;
            session->chart_.notes.push_back({1, 0, now + 240000});
            for (int i = 1; i <= past_notes; ++i)
                session->chart_.notes.push_back({2 + i % 9, static_cast<int64_t>(i) * 480, std::nullopt});
            session->chart_.notes.push_back({2, now + 24000, std::nullopt});
            for (std::size_t i = 0; i < session->chart_.notes.size(); ++i) session->chart_.notes[i].note_id = i;
            gameplay::GameplayConfig config;
            config.practice_no_fail_enabled = true;
            session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
            session->engine_->advance(now);
            if (ghost) {
                session->ghost_engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
                session->ghost_engine_->advance(now);
            }
            session->lane_activity_.assign(10, 0); session->lane_pressed_.assign(10, 0);
            session->ghost_lane_activity_.assign(10, 0); session->ghost_lane_pressed_.assign(10, 0);
            session->synthetic_tones_enabled_ = false;
            session->audio_timing_diagnostics_logged_ = true;
            session->config_.audio_ui.mute_when_inactive = false;
            session->hidden_hit_note_ids_.assign(session->chart_.notes.size(), 0);
            session->ghost_hidden_hit_note_ids_.assign(session->chart_.notes.size(), 0);
            std::vector<float> output(960);
            session->audio_callback(output.data(), 480, now, now);
            const auto missed = session->hud_snapshot();
            std::cout << "[ghost-hud-structure] ghost=" << ghost
                      << " past=" << past_notes << " upcoming=" << hud_contains_note(missed, now + 24000)
                      << " old_ln=" << hud_contains_note(missed, 0)
                      << " ghost_upcoming=" << hud_contains_note(missed, now + 24000, true) << '\n';
            // An old missed LN remains visible, but expired missed taps must
            // neither pin the head scan nor crowd the upcoming note out of HUD.
            CHECK(hud_contains_note(missed, 0));
            CHECK(hud_contains_note(missed, now + 24000));
            CHECK(session->hud_scan_start_ > static_cast<std::size_t>(past_notes - 32));
            CHECK(session->hud_past_hold_indices_ == std::vector<std::size_t>{0});
            if (ghost) {
                CHECK(hud_contains_note(missed, 0, true));
                CHECK(hud_contains_note(missed, now + 24000, true));
                CHECK(session->ghost_hud_scan_start_ == session->hud_scan_start_);
                CHECK(session->ghost_hud_past_hold_indices_ == session->hud_past_hold_indices_);
            }
            for (std::size_t i = 1; i + 1 < session->chart_.notes.size(); ++i) {
                session->hidden_hit_note_ids_[i] = 1;
                session->ghost_hidden_hit_note_ids_[i] = 1;
            }
            (void)session->hud_snapshot();
            if (!benchmark) continue;
            const auto percentile = [](std::vector<double> values, double fraction) {
                std::sort(values.begin(), values.end());
                return values[static_cast<std::size_t>((values.size() - 1) * fraction)];
            };
            const auto timed = [](auto&& operation) {
                const auto start = std::chrono::steady_clock::now();
                operation();
                return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
            };
            std::vector<double> hud_us, callback_us, contended_us;
            for (int i = 0; i < 300; ++i) {
                hud_us.push_back(timed([&] { (void)session->hud_snapshot(); }));
                callback_us.push_back(timed([&] { session->audio_callback(output.data(), 480, now, now); }));
            }
            std::atomic<bool> stop{false}, ready{false};
            std::thread reader([&] {
                while (!stop.load()) {
                    (void)session->hud_snapshot(); ready.store(true);
                    if (paced) std::this_thread::sleep_for(std::chrono::milliseconds(8));
                }
            });
            while (!ready.load()) std::this_thread::yield();
            for (int i = 0; i < 300; ++i) {
                contended_us.push_back(timed([&] { session->audio_callback(output.data(), 480, now, now); }));
                if (paced) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                else std::this_thread::yield();
            }
            stop.store(true); reader.join();
            // Unpaced is deliberate mutex-contention stress, not the menu's
            // real 8ms HUD cadence; do not report it as device input latency.
            std::cout << "[ghost-hud-bench] ghost=" << ghost << " past=" << past_notes
                      << " reader_cadence_ms=" << (paced ? 8 : 0)
                      << " hud_p50_us=" << percentile(hud_us, .50) << " hud_p95_us=" << percentile(hud_us, .95)
                      << " callback_p95_us=" << percentile(callback_us, .95)
                      << " contended_callback_p95_us=" << percentile(contended_us, .95)
                      << " contended_callback_max_us=" << percentile(contended_us, 1.0) << '\n';
        }
    }
    static void check_ghost_backlog_player_callback() {
        std::vector<float> reference_audio;
        for (const bool ghost : {false, true}) {
            auto session = std::make_unique<GameSession>();
            session->sample_rate_ = 48000;
            session->chart_.lane_count = 2;
            session->chart_.duration_samples = 200000;
            session->chart_.notes = {{1, 48000, std::nullopt}, {2, 100000, std::nullopt}};
            session->chart_.notes[0].note_id = 0;
            session->chart_.notes[0].audio_asset_id = 0;
            session->chart_.notes[1].note_id = 1;
            gameplay::GameplayConfig config;
            config.sample_rate = 48000;
            config.practice_no_fail_enabled = true;
            session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
            session->lane_activity_.assign(2, 0); session->lane_pressed_.assign(2, 0);
            session->hidden_hit_note_ids_.assign(2, 0);
            session->synthetic_tones_enabled_ = false;
            session->audio_timing_diagnostics_logged_ = true;
            session->config_.audio_ui.mute_when_inactive = false;
            session->config_.audio_ui.normalize_audio = false;
            session->chart_audio_assets_.resize(1);
            session->chart_audio_assets_[0].clip.samples =
                std::make_shared<const std::vector<float>>(1920, 0.25f);
            session->key_to_lane_ = {{32, 1}};
            session->lane_binding_state_.configure(session->key_to_lane_);
            if (ghost) {
                session->ghost_engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
                session->ghost_lane_activity_.assign(2, 0); session->ghost_lane_pressed_.assign(2, 0);
                session->ghost_hidden_hit_note_ids_.assign(2, 0);
                session->ghost_replay_enabled_ = true;
                // Resampled replay plus a large due-event backlog must not move
                // the player's mapped input, feedback or keysound sample.
                session->ghost_replay_source_.trace.sample_rate = 24000;
                for (int i = 0; i < 2048; ++i)
                    session->ghost_replay_source_.trace.events.push_back({2,
                        i % 2 ? input::InputState::Released : input::InputState::Pressed, 10000 + i * 4});
                session->ghost_replay_source_.trace.events.push_back({1, input::InputState::Pressed, 24000});
            }
            // Freeze the timestamp mapping, then run the production callback's
            // queued lane edge, input/control queue, ghost queue and audio mixer.
            input::InputEvent press{};
            press.keycode = 32; press.state = input::InputState::Pressed;
            REQUIRE(session->future_events_.push({press, 48000}));
            bool observed_player_first = false;
            session->set_control_input_callback([&](const input::InputEvent& event) {
                if (event.keycode != 34) return false;
                observed_player_first = session->engine_->stats().counts.pg == 1 &&
                    session->ghost_replay_event_index_ == 0;
                return true;
            });
            input::InputEvent sentinel{};
            sentinel.keycode = 34; sentinel.state = input::InputState::Pressed;
            REQUIRE(session->input_thread_.queue().push(sentinel));
            std::vector<float> audio(960);
            session->audio_callback(audio.data(), 480, 48000, 48000);
            CHECK(observed_player_first);
            REQUIRE(session->engine_->replay().events.size() == 1);
            CHECK(session->engine_->replay().events[0].sample == 48000);
            CHECK(session->engine_->stats().counts.pg == 1);
            CHECK(session->engine_->live_feedback().sample == 48000);
            CHECK(session->engine_->live_feedback().delta_ms == 0.0);
            CHECK(session->lane_activity_[0] == doctest::Approx(0.95));
            CHECK(session->hidden_hit_note_ids_[0] == 1);
            REQUIRE(session->chart_audio_voices_.size() == 1);
            CHECK(session->chart_audio_voices_[0].start_sample == 48000);
            CHECK(std::any_of(audio.begin(), audio.end(), [](float value) { return value != 0; }));
            if (ghost) {
                CHECK(session->ghost_replay_event_index_ == 2049);
                CHECK(session->ghost_engine_->stats().counts.pg == 1);
                CHECK(audio == reference_audio);
            } else reference_audio = audio;
        }
    }

    static void check_hud_hold_survivor_lifecycle() {
        auto session = pause_hud_fixture();
        // This scan test deliberately seeks the historical raw playback head;
        // R5 clock continuity is tested separately rather than bypassed here.
        session->chronological_judgement_ = false;
        session->audio_timing_diagnostics_logged_ = true;
        std::vector<float> audio(960);
        const auto snapshot_at = [&](int64_t playback) {
            session->audio_callback(audio.data(), 480, 130000, playback);
            return session->hud_snapshot();
        };
        auto hud = snapshot_at(70000);
        CHECK_FALSE(hud_contains_note(hud, 37500));
        CHECK(hud_contains_note(hud, 42000));
        CHECK(session->hud_past_hold_indices_ == std::vector<std::size_t>{1});
        session->config_.visual_offset_ms = -500;
        hud = snapshot_at(70000); // offset alone reintroduces a retired head
        CHECK(hud_contains_note(hud, 37500));
        CHECK(hud_contains_note(hud, 37500, true));
        session->config_.visual_offset_ms = 0;
        hud = snapshot_at(65000); // rewind without duplicating an old LN survivor
        CHECK(hud_contains_note(hud, 42000));
        CHECK(session->hud_past_hold_indices_ == std::vector<std::size_t>{1});
        CHECK(session->ghost_hud_past_hold_indices_ == session->hud_past_hold_indices_);
        hud = snapshot_at(120000); // tail has left the past window
        CHECK_FALSE(hud_contains_note(hud, 42000));
        CHECK_FALSE(hud_contains_note(hud, 42000, true));
        CHECK(session->hud_past_hold_indices_.empty());
        hud = snapshot_at(70000); // audible-head rebase restores the missed body
        CHECK(hud_contains_note(hud, 42000));
        CHECK(hud_contains_note(hud, 42000, true));

        session = pause_hud_fixture();
        session->audio_timing_diagnostics_logged_ = true;
        for (const auto sample : {37500, 42000}) {
            session->dispatch_lane_input(1, input::InputState::Pressed, sample, sample);
            session->dispatch_ghost_lane_input(1, input::InputState::Pressed, sample);
            if (sample == 37500) {
                session->dispatch_lane_input(1, input::InputState::Released, sample + 1, sample + 1);
                session->dispatch_ghost_lane_input(1, input::InputState::Released, sample + 1);
            }
        }
        session->audio_callback(audio.data(), 480, 70000, 70000);
        hud = session->hud_snapshot();
        for (const bool ghost : {false, true}) {
            const auto& notes = ghost ? hud.ghost_notes : hud.notes;
            const auto count = ghost ? hud.ghost_note_count : hud.note_count;
            CHECK_FALSE(hud_contains_note(hud, 42000, ghost));
            CHECK(std::count_if(notes.begin(), notes.begin() + count, [](const auto& note) {
                return note.hold && !note.head_visible && note.start_sample == 70000 && note.tail_sample == 100000;
            }) == 1);
        }
    }

    static void check_early_released_hold_hud(bool release_required) {
        auto session = pause_hud_fixture();
        session->chronological_judgement_ = false;
        session->audio_timing_diagnostics_logged_ = true;
        session->chart_.notes[1].release_required = release_required;
        gameplay::GameplayConfig config;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->ghost_engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->dispatch_lane_input(1, input::InputState::Pressed, 37500, 37500);
        session->dispatch_ghost_lane_input(1, input::InputState::Pressed, 37500);
        session->dispatch_lane_input(1, input::InputState::Released, 37501, 37501);
        session->dispatch_ghost_lane_input(1, input::InputState::Released, 37501);
        session->dispatch_lane_input(1, input::InputState::Pressed, 42000, 42000);
        session->dispatch_ghost_lane_input(1, input::InputState::Pressed, 42000);
        session->dispatch_lane_input(1, input::InputState::Released, 70000, 70000);
        session->dispatch_ghost_lane_input(1, input::InputState::Released, 70000);
        std::vector<gameplay::ActiveHoldView> active;
        session->engine_->collect_active_holds(active);
        CHECK(active.empty());
        session->ghost_engine_->collect_active_holds(active);
        CHECK(active.empty());
        const auto bad = session->engine_->stats().counts.bd;
        const auto ghost_bad = session->ghost_engine_->stats().counts.bd;
        CHECK(bad > 0);
        CHECK(ghost_bad > 0);
        std::vector<float> audio(960);
        for (const int64_t sample : {70000, 85000, 100000, 100001, 120000}) {
            session->audio_callback(audio.data(), 480, sample, sample);
            const auto hud = session->hud_snapshot();
            for (const bool ghost : {false, true}) {
                const auto& notes = ghost ? hud.ghost_notes : hud.notes;
                const auto count = ghost ? hud.ghost_note_count : hud.note_count;
                int rendered_bodies = 0;
                for (std::size_t i = 0; i < count; ++i) {
                    const auto& note = notes[i];
                    if (!note.hold || note.tail_sample != 100000) continue;
                    CHECK(note.released);
                    CHECK_FALSE(note.pending);
                    CHECK(note.head_visible); // unanchored; released suppresses the scored head
                    CHECK_FALSE(render::should_render_gameplay_note_head(
                        note.start_sample, note.head_visible, sample, note.released));
                    if (render::should_render_gameplay_note(note.start_sample, note.tail_sample,
                            note.hold, note.head_visible, note.pending, sample, sample, 32, note.released))
                        ++rendered_bodies;
                }
                CHECK(rendered_bodies == (sample <= 100000 ? 1 : 0));
            }
            // Display retention never postpones or scores the released tail again.
            CHECK(session->engine_->stats().counts.bd == bad);
            CHECK(session->ghost_engine_->stats().counts.bd == ghost_bad);
        }
    }

    static void check_final_released_hold_completion(bool play_to_end, bool manual_skip, double visual_offset_ms) {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->chronological_judgement_ = false;
        session->audio_timing_diagnostics_logged_ = true;
        session->synthetic_tones_enabled_ = false;
        session->gameplay_started_ = true;
        session->enter_keycode_ = 13;
        session->config_.ui.result_tail_ms = 100;
        session->config_.audio_ui.play_to_end = play_to_end;
        session->config_.audio_ui.mute_when_inactive = false;
        session->config_.visual_offset_ms = visual_offset_ms;
        session->chart_.lane_count = 1;
        session->chart_.duration_samples = 240000;
        session->chart_.notes = {{1, 48000, 144000}};
        session->hidden_hit_note_ids_.assign(1, 0);
        session->lane_activity_.assign(1, 0);
        session->lane_pressed_.assign(1, 0);
        gameplay::GameplayConfig config;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->dispatch_lane_input(1, input::InputState::Pressed, 48000, 48000);
        session->dispatch_lane_input(1, input::InputState::Released, 60000, 60000);
        std::vector<float> audio(960);
        session->audio_callback(audio.data(), 480, 60000, 60000);
        REQUIRE(session->engine_->is_finished());
        REQUIRE(session->result_transition_pending_);
        CHECK_FALSE(session->finished_.load());
        const int64_t visible_end = 144000 + static_cast<int64_t>(-visual_offset_ms * 48);
        CHECK(session->result_transition_sample_ == visible_end);
        const auto bad = session->engine_->stats().counts.bd;
        const auto events = session->engine_->replay().events.size();
        if (manual_skip) {
            input::InputEvent skip{};
            skip.keycode = 13;
            skip.state = input::InputState::Pressed;
            CHECK(session->handle_control_input(skip));
            CHECK(session->finished_.load());
            CHECK_FALSE(session->user_aborted_.load());
            return;
        }
        for (const int64_t sample : {int64_t{90000}, visible_end - 1, visible_end}) {
            session->audio_callback(audio.data(), 480, sample, sample);
            CHECK(session->finished_.load() == (sample >= visible_end));
            CHECK(session->engine_->stats().counts.bd == bad);
            CHECK(session->engine_->replay().events.size() == events);
        }
    }

    static void check_auto_scratch(bool manual_key) {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->chart_format_ = ChartFormat::Bms;
        session->chart_.lane_count = 3;
        session->chart_.scratch_lanes = {1, 3};
        session->chart_.duration_samples = 200000;
        session->chart_.notes = {{1, 48000, 96000}, {2, 50000, std::nullopt}, {3, 60000, std::nullopt}};
        session->chart_.notes[0].release_required = true;
        for (std::size_t i = 0; i < session->chart_.notes.size(); ++i) session->chart_.notes[i].note_id = i;
        session->chart_.notes[0].audio_asset_id = 0;
        gameplay::GameplayConfig config;
        config.sample_rate = 48000;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->lane_activity_.assign(3, 0);
        session->lane_pressed_.assign(3, 0);
        session->hidden_hit_note_ids_.assign(3, 0);
        session->synthetic_tones_enabled_ = false;
        session->auto_scratch_enabled_ = true;
        session->chart_audio_assets_.resize(1);
        session->chart_audio_assets_[0].clip.samples = std::make_shared<const std::vector<float>>(960, 0.25f);
        session->build_autoplay_events();
        REQUIRE(session->autoplay_events_.size() == 4);
        for (const auto& edge : session->autoplay_events_) CHECK(edge.lane != 2);
        if (manual_key) {
            session->key_to_lane_ = {{32, 2}, {33, 1}};
            session->lane_binding_state_.configure(session->key_to_lane_);
            session->startup_input_timing_anchor_ = {50000, 1'000'000'000, true};
            session->current_playback_sample_ = 50000;
            input::InputEvent event{};
            event.state = input::InputState::Pressed;
            event.input_time_ns = 1'000'000'000;
            event.keycode = 33; // physical scratch must not duplicate the generated edge
            REQUIRE(session->input_thread_.queue().push(event));
            event.keycode = 32;
            REQUIRE(session->input_thread_.queue().push(event));
            session->process_input_queue(48000, 60001, 0);
            REQUIRE(session->engine_->replay().events.size() == 2);
            CHECK(session->engine_->replay().events[0].sample == 48000);
            CHECK(session->engine_->replay().events[1].sample == 50000);
            CHECK(session->engine_->replay().events[1].lane == 2);
        }
        session->process_autoplay_queue(48000, 60001, 0);
        REQUIRE(session->engine_->replay().events.size() == (manual_key ? 4 : 3));
        CHECK(session->engine_->replay().events[0].sample == 48000);
        CHECK(session->hidden_hit_note_ids_[0] == 1);
        CHECK(session->hidden_hit_note_ids_[1] == (manual_key ? 1 : 0));
        REQUIRE(session->chart_audio_voices_.size() == 1);
        CHECK(session->chart_audio_voices_[0].start_sample == 48000);
        session->engine_->advance(60001);
        session->process_autoplay_queue(96000, 96000, 0);
        session->engine_->advance(120000);
        CHECK(session->engine_->stats().counts.pg == (manual_key ? 4 : 3));
        // The default Normal policy assigns BAD to an unplayed key note;
        // POOR is the separate indirect-miss (Hard) policy.
        CHECK(session->engine_->stats().counts.pr == 0);
        CHECK(session->engine_->stats().counts.bd == (manual_key ? 0 : 1));
        REQUIRE(session->engine_->replay().events.size() == (manual_key ? 5 : 4));
        CHECK(session->engine_->replay().events.back().sample == 96000);
        gameplay::GameplayEngine replay_engine(session->chart_, config);
        for (const auto& edge : session->engine_->replay().events) {
            replay_engine.advance(edge.sample - 1);
            (void)replay_engine.handle_input(edge.lane, edge.state, edge.sample);
            replay_engine.advance(edge.sample);
        }
        replay_engine.advance(120000);
        CHECK(replay_engine.stats().raw_score == session->engine_->stats().raw_score);
        CHECK(replay_engine.stats().counts.pg == session->engine_->stats().counts.pg);
        CHECK(replay_engine.stats().counts.pr == session->engine_->stats().counts.pr);
        CHECK(replay_engine.stats().counts.bd == session->engine_->stats().counts.bd);
        session->replay_playback_enabled_ = true;
        session->build_autoplay_events();
        CHECK(session->autoplay_events_.empty());
        session->replay_playback_enabled_ = false;
        session->chart_format_ = ChartFormat::Unknown;
        session->build_autoplay_events();
        CHECK(session->autoplay_events_.empty());
        session->chart_format_ = ChartFormat::Bms;
        session->chart_.scratch_lanes.clear();
        session->build_autoplay_events();
        CHECK(session->autoplay_events_.empty());
    }

    static void check_pause_mouse_and_hotkeys() {
        auto session = pause_fixture();
        session->f5_keycode_ = 116; session->f6_keycode_ = 117;
        session->f7_keycode_ = 118; session->f8_keycode_ = 119;
        session->config_.speed.hi_speed = 4.0;
        control(*session, 116);
        CHECK(session->config_.speed.hi_speed == 2.0);
        control(*session, 117);
        CHECK(session->config_.speed.hi_speed == 4.0);
        control(*session, 117); // Duplicate pressed edges cannot repeat a coarse step.
        CHECK(session->config_.speed.hi_speed == 4.0);
        session->f5_keycode_ = 123; // Assigned F12 / Q, unrelated to the Shift lane.
        session->f6_keycode_ = 'Q';
        session->hispeed_coarse_decrease_held_ = false;
        session->hispeed_coarse_increase_held_ = false;
        session->config_.speed.hi_speed = 49.0;
        control(*session, 'Q');
        CHECK(session->config_.speed.hi_speed == 50.0);
        control(*session, 123);
        CHECK(session->config_.speed.hi_speed == 25.0);
        session->config_.speed.hi_speed = 0.5;
        (void)session->handle_control_input({123, input::InputState::Released});
        control(*session, 123);
        CHECK(session->config_.speed.hi_speed == 0.5);
        session->config_.speed.hi_speed = 25.0;
        input::InputEvent unassigned_f5{116, input::InputState::Pressed};
        CHECK_FALSE(session->handle_control_input(unassigned_f5));
        CHECK(session->config_.speed.hi_speed == 25.0);
        control(*session, 118);
        CHECK(session->config_.visual_offset_ms == -1.0);
        control(*session, 119);
        CHECK(session->config_.visual_offset_ms == 0.0);
        control(*session, 27);
        const double original_line = session->config_.skin.judgement_line_position;
        session->request_pause_adjust(0, -1);
        session->request_pause_adjust(1, 1);
        session->request_pause_adjust(2, 1);
        session->process_paused_input_queue();
        CHECK(session->config_.skin.judgement_line_position == doctest::Approx(original_line - 0.01));
        CHECK(session->config_.speed.hi_speed == 25.25);
        CHECK(session->config_.visual_offset_ms == 1.0);
        session->request_pause_action(0);
        session->process_paused_input_queue();
        CHECK(session->pause_resume_requested_.load());
        CHECK(session->paused_.load());
        session->request_pause_action(1);
        session->process_paused_input_queue();
        CHECK_FALSE(session->restart_requested_.load());
        for (int action : {1, 2}) {
            auto other = pause_fixture();
            control(*other, 27);
            other->request_pause_action(action);
            other->process_paused_input_queue();
            CHECK(other->finished_.load());
            CHECK(other->user_aborted_.load());
            CHECK(other->restart_requested_.load() == (action == 1));
            CHECK(other->exit_requested_.load() == (action == 2));
        }
    }
    static void check_concurrent_hud_timing_publication() {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->enter_keycode_ = 13;
        session->chart_.lane_count = 4;
        session->chart_.duration_samples = 2'000'000'000;
        session->chart_.notes.push_back(gameplay::NoteEvent{1, 1'000'000'000, std::nullopt});
        gameplay::GameplayConfig config;
        config.sample_rate = 48000;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->lane_activity_.assign(4, 0.0f);
        session->lane_pressed_.assign(4, 0);
        session->synthetic_tones_enabled_ = false;
        session->config_.audio_ui.mute_when_inactive = false;
        std::atomic<bool> begin{false}, observed{false}, done{false};
        session->audio_thread_.callback_ = [&](float* out, uint32_t frames, int64_t write, int64_t playback) {
            session->audio_callback(out, frames, write, playback);
        };
        std::thread producer([&] {
            std::vector<float> output(1920);
            while (!begin.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int64_t iteration = 1; iteration <= 500; ++iteration) {
                const uint32_t frames = iteration % 2 == 0 ? 480 : 960;
                const int64_t playback = iteration * 4800;
                session->audio_thread_.invoke_callback(output.data(), frames, playback + 2400, playback,
                    1'000'000'000LL + iteration * 100'000'000LL);
                if (iteration == 1)
                    while (!observed.load(std::memory_order_acquire)) std::this_thread::yield();
                if (iteration % 16 == 0) std::this_thread::yield();
            }
            done.store(true, std::memory_order_release);
        });
        begin.store(true, std::memory_order_release);
        std::size_t published_snapshots = 0;
        std::size_t mismatches = 0;
        do {
            const auto hud = session->hud_snapshot();
            if (hud.current_sample > 0) {
                ++published_snapshots;
                observed.store(true, std::memory_order_release);
                mismatches += hud.current_sample % 4800 != 0;
                const auto iteration = hud.current_sample / 4800;
                mismatches += hud.audio_buffer_frames != (iteration % 2 == 0 ? 480u : 960u);
                mismatches += hud.audio_sample_time_ns != 1'000'000'000LL + iteration * 100'000'000LL;
                mismatches += std::abs(hud.audio_clock_rate - 1.0) > 0.000001;
            }
        } while (!done.load(std::memory_order_acquire));
        producer.join();
        // The local CHECK shim throws; join before reporting a failed snapshot
        // so a test failure cannot terminate the process via std::thread.
        CHECK(mismatches == 0);
        const auto final = session->hud_snapshot();
        CHECK(final.current_sample == 500 * 4800);
        CHECK(final.audio_buffer_frames == 480);
        CHECK(published_snapshots > 0);
    }

    static void check_secondary_binding_input_queue(uint32_t primary_key = 32, uint32_t secondary_key = 33,
                                                   bool future_queue = false, bool older_key_held = false,
                                                   bool early_release = false) {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->chart_.lane_count = 1;
        session->chart_.duration_samples = 120000;
        gameplay::NoteEvent note{1, 48000, 96000};
        note.release_required = true;
        session->chart_.notes.push_back(note);
        gameplay::GameplayConfig config;
        config.sample_rate = 48000;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->lane_activity_.assign(1, 0.0f);
        session->lane_pressed_.assign(1, 0);
        session->synthetic_tones_enabled_ = false;
        session->key_to_lane_ = {{primary_key, 1}, {secondary_key, 1}};
        session->lane_binding_state_.configure(session->key_to_lane_);
        const auto physical_event = [&](uint32_t key, input::InputState state, int64_t sample) {
            queue_physical_edge(*session, key, state, sample, future_queue);
        };
        physical_event(primary_key, input::InputState::Pressed, 48000);
        physical_event(secondary_key, input::InputState::Pressed, 60000);
        if (!older_key_held) physical_event(primary_key, input::InputState::Released, 72000);
        CHECK(session->lane_pressed_[0] == 1);
        CHECK(session->lane_binding_state_.pressed(1));
        CHECK(session->engine_->stats().counts.bd == 0);
        CHECK(session->engine_->stats().counts.pr == 0);
        physical_event(secondary_key, input::InputState::Released, early_release ? 72000 : 96000);
        if (older_key_held) {
            physical_event(primary_key, input::InputState::Pressed, 98400);
            CHECK_FALSE(session->lane_binding_state_.pressed(1));
            physical_event(primary_key, input::InputState::Released, 98400);
        }
        session->engine_->advance(100800);
        CHECK(session->lane_pressed_[0] == 0);
        CHECK_FALSE(session->lane_binding_state_.pressed(1));
        CHECK(session->engine_->stats().counts.pg == (early_release ? 1 : 2));
        CHECK(session->engine_->stats().counts.bd == (early_release ? 1 : 0));
        CHECK(session->engine_->stats().counts.pr == 0);
        const auto& replay = session->engine_->replay().events;
        REQUIRE(replay.size() == 3);
        CHECK(replay[0].state == input::InputState::Pressed);
        CHECK(replay[0].sample == 48000);
        CHECK(replay[1].state == input::InputState::Pressed);
        CHECK(replay[1].sample == 60000);
        CHECK(replay[2].state == input::InputState::Released);
        CHECK(replay[2].sample == (early_release ? 72000 : 96000));
        gameplay::GameplayEngine playback(session->chart_, config);
        for (const auto& event : replay) {
            static_cast<void>(playback.handle_input(event.lane, event.state, event.sample));
            playback.advance(event.sample);
        }
        playback.advance(100800);
        CHECK(playback.stats().counts.pg == session->engine_->stats().counts.pg);
        CHECK(playback.stats().counts.bd == session->engine_->stats().counts.bd);
        CHECK(playback.stats().counts.pr == session->engine_->stats().counts.pr);
        CHECK(playback.stats().raw_score == session->engine_->stats().raw_score);
        CHECK(playback.gauge_state().value == doctest::Approx(session->engine_->gauge_state().value));
    }

    static std::unique_ptr<GameSession> shift_lane_fixture(uint32_t key, bool hold = true) {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->chart_.lane_count = 1;
        session->chart_.duration_samples = 144000;
        gameplay::NoteEvent note{1, 48000, hold ? std::optional<int64_t>{96000} : std::nullopt};
        note.release_required = hold;
        session->chart_.notes.push_back(note);
        gameplay::GameplayConfig config;
        config.sample_rate = 48000;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->lane_activity_.assign(1, 0.0f);
        session->lane_pressed_.assign(1, 0);
        session->synthetic_tones_enabled_ = false;
        session->gameplay_started_ = true;
        session->escape_keycode_ = 27;
        session->f5_keycode_ = 116;
        session->f6_keycode_ = 117;
        session->f8_keycode_ = 119;
        session->key_to_lane_ = {{key, 1}};
        session->lane_binding_state_.configure(session->key_to_lane_);
        return session;
    }

    static void queue_physical_edge(GameSession& session, uint32_t key, input::InputState state,
                                    int64_t sample, bool future_queue = false) {
        const int64_t time_ns = 1'000'000'000LL + (sample - 48000) * 1'000'000'000LL / 48000;
        session.startup_input_timing_anchor_ = {sample, time_ns, true};
        session.current_playback_sample_ = sample;
        input::InputEvent event{};
        event.keycode = key;
        event.state = state;
        event.input_time_ns = time_ns;
        if (future_queue) {
            REQUIRE(session.future_events_.push({event, sample}));
            session.process_future_events(sample, sample + 480, 0);
        } else {
            REQUIRE(session.input_thread_.queue().push(event));
            session.process_input_queue(sample, sample + 480, 0);
        }
        session.engine_->advance(sample);
    }

    static void check_secondary_binding_tap_queue(bool future_queue, bool extra_empty_press) {
        auto session = shift_lane_fixture(32, false);
        if (!extra_empty_press) session->chart_.notes.push_back({1, 60000, std::nullopt});
        session->chart_.notes.push_back({1, 96000, std::nullopt});
        session->chart_.notes.push_back({1, 120000, std::nullopt});
        gameplay::GameplayConfig config;
        config.sample_rate = 48000;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->key_to_lane_ = {{32, 1}, {33, 1}};
        session->lane_binding_state_.configure(session->key_to_lane_);
        const auto physical_event = [&](uint32_t key, input::InputState state, int64_t sample) {
            queue_physical_edge(*session, key, state, sample, future_queue);
        };
        physical_event(32, input::InputState::Pressed, 48000);
        physical_event(33, input::InputState::Pressed, 60000);
        CHECK(session->engine_->stats().counts.pg == (extra_empty_press ? 1 : 2));
        // A real extra strike retains the engine's ordinary Empty Poor rule;
        // binding ownership must not suppress it or create synthetic strikes.
        CHECK(session->engine_->stats().counts.pr == (extra_empty_press ? 1 : 0));
        physical_event(33, input::InputState::Released, 72000);
        CHECK(session->lane_pressed_[0] == 0);
        CHECK_FALSE(session->lane_binding_state_.pressed(1));
        physical_event(32, input::InputState::Pressed, 80000);
        CHECK(session->lane_pressed_[0] == 0);
        CHECK_FALSE(session->lane_binding_state_.pressed(1));
        physical_event(32, input::InputState::Released, 84000);
        physical_event(32, input::InputState::Pressed, 96000);
        CHECK(session->lane_pressed_[0] == 1);
        physical_event(33, input::InputState::Pressed, 120000);
        CHECK(session->engine_->stats().counts.pg == (extra_empty_press ? 3 : 4));
        CHECK(session->engine_->stats().counts.bd == 0);
        CHECK(session->engine_->stats().counts.pr == (extra_empty_press ? 1 : 0));
        const auto& replay = session->engine_->replay().events;
        REQUIRE(replay.size() == 5);
        CHECK(replay[0].sample == 48000);
        CHECK(replay[1].sample == 60000);
        CHECK(replay[2].sample == 72000);
        CHECK(replay[3].sample == 96000);
        CHECK(replay[4].sample == 120000);
        gameplay::GameplayEngine playback(session->chart_, config);
        for (const auto& event : replay) {
            static_cast<void>(playback.handle_input(event.lane, event.state, event.sample));
            playback.advance(event.sample);
        }
        CHECK(playback.is_finished());
        CHECK(playback.stats().counts.pg == session->engine_->stats().counts.pg);
        CHECK(playback.stats().counts.bd == session->engine_->stats().counts.bd);
        CHECK(playback.stats().counts.pr == session->engine_->stats().counts.pr);
        CHECK(playback.stats().raw_score == session->engine_->stats().raw_score);
        CHECK(playback.gauge_state().value == doctest::Approx(session->engine_->gauge_state().value));
    }

    static void check_shift_lane_edges(uint32_t key, bool hold, bool future_queue) {
        auto session = shift_lane_fixture(key, hold);
        queue_physical_edge(*session, key, input::InputState::Pressed, 48000, future_queue);
        REQUIRE(session->engine_->stats().counts.pg == 1);
        CHECK(session->engine_->is_finished() == !hold);
        CHECK(session->lane_pressed_[0] == 1);
        CHECK(session->lane_binding_state_.pressed(1));
        queue_physical_edge(*session, key, input::InputState::Released, 96000, future_queue);
        session->engine_->advance(100800);
        CHECK(session->lane_pressed_[0] == 0);
        CHECK_FALSE(session->lane_binding_state_.pressed(1));
        CHECK(session->engine_->stats().counts.pg == (hold ? 2 : 1));
        CHECK(session->engine_->stats().counts.bd == 0);
        CHECK(session->engine_->stats().counts.pr == 0);
        const auto& replay = session->engine_->replay().events;
        // A final tap finishes the engine at its head, so its later physical
        // release clears the binding/HUD but is intentionally not replayed.
        // An LN stays active until release, which must reach the engine at the
        // exact sample and remain in the replay as its second scored edge.
        REQUIRE(replay.size() == (hold ? 2 : 1));
        CHECK(replay[0].sample == 48000);
        CHECK(replay[0].state == input::InputState::Pressed);
        if (hold) {
            CHECK(replay[1].sample == 96000);
            CHECK(replay[1].state == input::InputState::Released);
        }
    }

    static void check_shift_tuning_hotkeys(uint32_t key, bool held, bool paused) {
        auto session = shift_lane_fixture(key);
        session->config_.speed.hi_speed = 4.0;
        if (held) queue_physical_edge(*session, key, input::InputState::Pressed, 48000);
        if (paused) control(*session, 27);
        control(*session, 116);
        CHECK(session->config_.speed.hi_speed == 2.0);
        control(*session, 117);
        CHECK(session->config_.speed.hi_speed == 4.0);
        control(*session, 117); // Duplicate pressed edges cannot repeat a coarse step.
        CHECK(session->config_.speed.hi_speed == 4.0);
        control(*session, 119);
        CHECK(session->config_.visual_offset_ms == 1.0);
        input::InputEvent release{};
        release.keycode = 119;
        release.state = input::InputState::Released;
        REQUIRE(session->handle_control_input(release));
        CHECK_FALSE(session->visual_offset_increase_repeat_.held);
        CHECK(session->engine_->replay().events.size() == (held ? 1 : 0));
        if (held && !paused) {
            CHECK(session->lane_binding_state_.pressed(1));
            queue_physical_edge(*session, key, input::InputState::Released, 96000);
            CHECK(session->engine_->stats().counts.pg == 2);
            CHECK(session->engine_->stats().counts.bd == 0);
        }
    }

    static void check_shift_pause_release(uint32_t key) {
        auto session = shift_lane_fixture(key);
        queue_physical_edge(*session, key, input::InputState::Pressed, 48000);
        control(*session, 27);
        REQUIRE(session->paused_.load());
        const auto score_before = session->engine_->stats().raw_score;
        input::InputEvent release{};
        release.keycode = key;
        release.state = input::InputState::Released;
        REQUIRE(session->input_thread_.queue().push(release));
        session->process_paused_input_queue();
        CHECK(session->engine_->stats().raw_score == score_before);
        CHECK(session->engine_->replay().events.size() == 1);
        // No physical keys are polled in this deterministic fixture. The real
        // resume path reconciles every configured binding before syncing lanes.
        session->polled_gameplay_keys_.clear();
        session->rebaseline_gameplay_start_input_state(60000);
        CHECK_FALSE(session->lane_binding_state_.pressed(1));
        CHECK(session->lane_pressed_[0] == 0);
        CHECK(session->engine_->stats().raw_score == score_before);
        session->engine_->advance(100800);
        CHECK(session->engine_->stats().counts.bd == 1);
    }

    static void check_shift_and_chat_polling() {
        for (const bool rawinput : {false, true}) {
            auto session = std::make_unique<GameSession>();
            session->config_.input.rawinput = rawinput;
            session->f11_keycode_ = 122;
            session->key_to_lane_ = {{160, 1}, {161, 2}};
            for (const bool fallback : {false, true}) {
                session->rebuild_polled_gameplay_keys();
                if (fallback) session->polled_gameplay_keys_.clear();
                input::InputThreadConfig input_config;
                session->rebuild_input_thread_config(input_config);
                for (const uint32_t key : {160u, 161u, 122u}) {
                    CHECK(std::count(input_config.polling_keys.begin(), input_config.polling_keys.end(), key) == 1);
                }
                CHECK(input_config.rawinput_polling_shadow == rawinput);
            }
            session->key_to_lane_.clear();
            session->rebuild_polled_gameplay_keys();
            input::InputThreadConfig input_config;
            session->rebuild_input_thread_config(input_config);
            CHECK(std::count(input_config.polling_keys.begin(), input_config.polling_keys.end(), 122u) == 1);
            CHECK(std::count(input_config.polling_keys.begin(), input_config.polling_keys.end(), 160u) == 0);
            CHECK(std::count(input_config.polling_keys.begin(), input_config.polling_keys.end(), 161u) == 0);
        }
    }

    static void check_hud_playback_anchor(int sample_rate) {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = sample_rate;
        session->chart_.lane_count = 4;
        session->chart_.duration_samples = sample_rate * 20;
        session->chart_.notes.push_back(gameplay::NoteEvent{1, sample_rate * 10, std::nullopt});
        gameplay::GameplayConfig config;
        config.sample_rate = sample_rate;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->lane_activity_.assign(4, 0.0f);
        session->lane_pressed_.assign(4, 0);
        session->synthetic_tones_enabled_ = false;
        session->config_.audio_ui.mute_when_inactive = false;
        struct Buffer { int64_t write; int64_t playback; uint32_t frames; };
        session->audio_thread_.callback_ = [&](float* out, uint32_t frames, int64_t write, int64_t playback) {
            session->audio_callback(out, frames, write, playback);
        };
        // Increasing padding resembles extra queued audio under a screen-share
        // workload. Neither its size nor the writable buffer may lead visuals.
        for (const Buffer buffer : {Buffer{4800, 2400, 480}, Buffer{24000, 3000, 960},
                                    Buffer{24960, 4800, 1440}}) {
            std::vector<float> output(buffer.frames * 2);
            session->audio_thread_.invoke_callback(output.data(), buffer.frames, buffer.write, buffer.playback,
                1'000'000'000LL + static_cast<int64_t>(std::llround(buffer.playback * 1'000'000'000.0 / sample_rate)));
            const auto hud = session->hud_snapshot();
            REQUIRE(hud.active);
            CHECK(hud.current_sample == buffer.playback);
            CHECK(hud.current_sample != buffer.write + buffer.frames);
            CHECK(hud.audio_buffer_frames == buffer.frames);
            CHECK(hud.sample_rate == sample_rate);
            CHECK(hud.audio_sample_time_ns > 0);
        }
    }

    static void check_normalize_and_focus_output() {
        auto session = std::make_unique<GameSession>();
        session->config_.audio_ui.gameplay_volume = 1.0;
        session->sample_rate_ = 48000;
        session->mix_normalizer_.reset(48000);
        session->config_.audio_ui.master_volume = 0.7;
        session->config_.audio_ui.normalize_audio = false;
        std::vector<float> original(48000 * 2 * 3);
        for (std::size_t sample = 0; sample < original.size(); sample += 2) {
            original[sample] = 0.8f;
            original[sample + 1] = 0.4f;
        }
        auto off = original;
        session->finish_audio_output(off.data(), 48000 * 3, true);
        auto expected = original;
        for (float& sample : expected) sample *= 0.7f;
        CHECK(off == expected);

        session->config_.audio_ui.normalize_audio = true;
        auto on = original;
        session->finish_audio_output(on.data(), 48000 * 3, true);
        CHECK(on.back() < off.back() * 0.5f);
        CHECK(on[on.size() - 2] == doctest::Approx(on.back() * 2));
        session->config_.audio_ui.normalize_audio = false;
        auto disabled_again = original;
        session->finish_audio_output(disabled_again.data(), 48000 * 3, true);
        CHECK(disabled_again == off);

        session->config_.audio_ui.mute_when_inactive = true;
        auto muted = original;
        session->finish_audio_output(muted.data(), 48000 * 3, false);
        CHECK(std::all_of(muted.begin(), muted.end(), [](float sample) { return sample == 0.0f; }));
        CHECK(session->config_.audio_ui.master_volume == doctest::Approx(0.7));
        auto restored = original;
        session->finish_audio_output(restored.data(), 48000 * 3, true);
        CHECK(restored == off);
        session->config_.audio_ui.mute_when_inactive = false;
        auto inactive_unmuted = original;
        session->finish_audio_output(inactive_unmuted.data(), 48000 * 3, false);
        CHECK(inactive_unmuted == off);
    }

    static void check_normalize_off_preserves_full_scale_waveform() {
        auto session = std::make_unique<GameSession>();
        session->config_.audio_ui.gameplay_volume = 1.0;
        session->config_.audio_ui.normalize_audio = false;
        session->config_.audio_ui.master_volume = 0.7;
        const std::vector<float> original{
            1.0f, -1.0f, 0.99f, -0.99f, 0.95f, -0.95f,
            0.92f, -0.92f, 0.25f, -0.25f, 0.0f, 0.0f};
        auto output = original;
        session->finish_audio_output(output.data(), static_cast<uint32_t>(output.size() / 2), true);
        for (std::size_t i = 0; i < output.size(); ++i) {
            CHECK(output[i] == doctest::Approx(original[i] * 0.7f));
        }
    }

    static void check_normalize_off_mix_retains_master_headroom() {
        auto session = std::make_unique<GameSession>();
        session->config_.audio_ui.gameplay_volume = 1.0;
        session->sample_rate_ = 48000;
        session->chart_.lane_count = 1;
        session->chart_.duration_samples = 480000;
        session->chart_.notes.push_back(gameplay::NoteEvent{1, 240000, std::nullopt});
        gameplay::GameplayConfig config;
        config.sample_rate = 48000;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->synthetic_tones_enabled_ = false;
        session->lane_activity_.assign(1, 0);
        session->lane_pressed_.assign(1, 0);
        session->config_.speed.rate = 1.0;
        session->config_.audio_ui.normalize_audio = false;
        session->config_.audio_ui.mute_when_inactive = false;
        session->config_.audio_ui.master_volume = 0.35;
        session->config_.audio_ui.bgm_volume = 1.0;
        session->config_.audio_ui.keysound_volume = 1.0;

        constexpr uint32_t frames = 96;
        std::vector<float> clip(frames * 2);
        for (uint32_t frame = 0; frame < frames; ++frame) {
            const float wave = static_cast<float>(std::sin(frame * 6.283185307179586 / 48.0));
            clip[frame * 2] = wave * 0.85f;
            clip[frame * 2 + 1] = wave * 0.30f;
        }
        session->chart_audio_assets_.resize(2);
        for (auto& asset : session->chart_audio_assets_)
            asset.clip.samples = std::make_shared<const std::vector<float>>(clip);
        session->chart_audio_voices_.push_back({0, 0, GameSession::ChartAudioEvent::Kind::Bgm, 1.0f});
        session->chart_audio_voices_.push_back({0, 1, GameSession::ChartAudioEvent::Kind::Keysound, 1.0f});
        std::vector<float> output(frames * 2);
        session->audio_callback(output.data(), frames, 0, 0);
        // The pre-master sum peaks at 1.7, but 35% output has ample headroom.
        // Compressing first distorts only the louder left channel and changes
        // both the waveform and stereo balance even though Normalize is OFF.
        for (std::size_t i = 0; i < output.size(); ++i) {
            CHECK(output[i] == doctest::Approx(clip[i] * 2.0f * 0.35f).epsilon(0.00001));
        }
    }

    static void check_independent_chart_volume() {
        // Exercise the real BGM + realtime keysound mixer and output stage.
        for (const double volume : {0.0, 0.2, 0.35, 1.0}) {
            for (const double menu_volume : {0.0, 1.0}) {
                for (const auto legacy : {std::pair{0.0, 0.0}, std::pair{0.5, 1.5},
                                            std::pair{2.0, 0.0}, std::pair{0.0, 2.0}}) {
                    auto session = std::make_unique<GameSession>();
                    session->config_.audio_ui.master_volume = 0.8;
                    session->config_.audio_ui.gameplay_volume = volume;
                    session->config_.audio_ui.menu_music_volume = menu_volume;
                    session->config_.audio_ui.bgm_volume = legacy.first;
                    session->config_.audio_ui.keysound_volume = legacy.second;
                    session->config_.audio_ui.normalize_audio = false;
                    session->chart_audio_assets_.resize(2);
                    session->chart_audio_assets_[0].clip.samples =
                        std::make_shared<const std::vector<float>>(std::vector<float>{0.4f, -0.2f, 0.2f, -0.1f});
                    session->chart_audio_assets_[1].clip.samples =
                        std::make_shared<const std::vector<float>>(std::vector<float>{0.2f, 0.1f, 0.1f, 0.05f});
                    session->chart_audio_voices_.push_back({0, 0, GameSession::ChartAudioEvent::Kind::Bgm, 1.0f});
                    session->chart_audio_voices_.push_back({0, 1, GameSession::ChartAudioEvent::Kind::Keysound, 1.0f});
                    std::vector<float> output(4, 0.0f);
                    session->mix_chart_audio(output.data(), 2, 0);
                    session->finish_audio_output(output.data(), 2, true);
                    const std::vector<float> expected{0.6f, -0.1f, 0.3f, -0.05f};
                    for (std::size_t i = 0; i < output.size(); ++i)
                        CHECK(output[i] == doctest::Approx(expected[i] * volume * 0.8).epsilon(0.00001));
                }
            }
        }
    }

    static void check_normalize_off_output_range_boundary() {
        auto session = std::make_unique<GameSession>();
        session->config_.audio_ui.gameplay_volume = 1.0;
        session->config_.audio_ui.normalize_audio = false;
        session->config_.audio_ui.master_volume = 0.5;
        std::vector<float> output{3.0f, -3.0f, 1.6f, -1.6f,
            std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
        session->finish_audio_output(output.data(), 3, true);
        CHECK(output[0] == 1.0f);
        CHECK(output[1] == -1.0f);
        CHECK(output[2] == doctest::Approx(0.8f));
        CHECK(output[3] == doctest::Approx(-0.8f));
        CHECK(output[4] == 0.0f);
        CHECK(output[5] == 0.0f);
    }

    static void check_speed_click_production_mix() {
        auto session = pause_fixture();
        session->chart_audio_voices_.clear();
        session->config_.audio_ui.master_volume = 0.7;
        session->config_.audio_ui.mute_when_inactive = false;
        session->config_.audio_ui.normalize_audio = false;
        const double before = session->config_.speed.hi_speed;
        session->adjust_hispeed(0.5);
        CHECK(session->config_.speed.hi_speed == doctest::Approx(before + 0.5));
        std::vector<float> output(480 * 2);
        session->audio_callback(output.data(), 480, 48000, 48000);
        CHECK(std::any_of(output.begin(), output.end(), [](float sample) { return sample != 0.0f; }));
        for (std::size_t sample = 0; sample < output.size(); sample += 2)
            CHECK(output[sample] == output[sample + 1]);
        session->audio_callback(output.data(), 480, 48480, 48480);
        session->audio_callback(output.data(), 480, 48960, 48960);
        CHECK(std::all_of(output.begin(), output.end(), [](float sample) { return sample == 0.0f; }));
        session->config_.speed.hi_speed = 50.0;
        session->adjust_hispeed(1.0);
        session->audio_callback(output.data(), 480, 49440, 49440);
        CHECK(std::all_of(output.begin(), output.end(), [](float sample) { return sample == 0.0f; }));
        session->config_.audio_ui.master_volume = 0.0;
        session->adjust_hispeed(-1.0);
        session->audio_callback(output.data(), 480, 49920, 49920);
        CHECK(std::all_of(output.begin(), output.end(), [](float sample) { return sample == 0.0f; }));
        session->config_.audio_ui.master_volume = 0.7;
        session->paused_.store(true);
        session->adjust_hispeed(-1.0);
        session->audio_callback(output.data(), 480, 50400, 50400);
        CHECK(std::any_of(output.begin(), output.end(), [](float sample) { return sample != 0.0f; }));
        CHECK(session->last_audio_sample_.load() == 50400);
        session->audio_callback(output.data(), 480, 50880, 50880);
        session->audio_callback(output.data(), 480, 51360, 51360);
        CHECK(std::all_of(output.begin(), output.end(), [](float sample) { return sample == 0.0f; }));
        CHECK(session->last_audio_sample_.load() == 50400);
    }

    static void check_duplicate_keysound_pcm_render() {
        CompletionTestDirectory directory;
        gameplay::GameplayChart original;
        original.lane_count = 3;
        original.duration_samples = 2048;
        // Two different WAV containers decode to identical PCM; a third differs
        // by actual sample values. A fourth chart asset is a path alias.
        for (int asset = 0; asset < 3; ++asset) {
            const auto wav = directory.path / ("dedup-" + std::to_string(asset) + ".wav");
            std::ofstream file(wav, std::ios::binary);
            const auto le = [&](uint32_t value, int bytes) {
                for (int i = 0; i < bytes; ++i) file.put(static_cast<char>((value >> (8 * i)) & 0xff));
            };
            constexpr int frames = 128;
            const bool metadata = asset == 1;
            file.write("RIFF", 4); le(36 + frames * 2 + (metadata ? 12 : 0), 4); file.write("WAVEfmt ", 8);
            le(16, 4); le(1, 2); le(1, 2); le(48000, 4); le(96000, 4); le(2, 2); le(16, 2);
            if (metadata) { file.write("JUNK", 4); le(4, 4); file.write("meta", 4); }
            file.write("data", 4); le(frames * 2, 4);
            for (int frame = 0; frame < frames; ++frame) {
                const auto value = static_cast<int16_t>((asset == 2 ? 1200 : 2400) * std::sin((frame + 1) * 0.12));
                le(static_cast<uint16_t>(value), 2);
            }
            file.close();
            (void)original.intern_audio_asset(wav.u8string());
        }
        (void)original.intern_audio_asset((directory.path / "." / "dedup-0.wav").u8string());

        const auto render = [&](const gameplay::GameplayChart& chart, const std::string& mode,
                                int sound_offset_ms = 0, double bgm_volume = 1.0, uint32_t block = 32) {
            auto session = std::make_unique<GameSession>();
            session->sample_rate_ = 48000;
            session->chart_ = chart;
            session->config_.speed.rate = 1.0;
            session->config_.sound_offset_ms = sound_offset_ms;
            session->config_.audio_ui.bgm_volume = bgm_volume;
            session->config_.audio_ui.keysound_volume = 1.0;
            gameplay::GameplayConfig config;
            config.sample_rate = 48000;
            config.practice_no_fail_enabled = true;
            session->engine_ = std::make_unique<gameplay::GameplayEngine>(chart, config);
            session->lane_activity_.assign(3, 0.0f);
            session->lane_pressed_.assign(3, 0);
            REQUIRE(session->prepare_chart_audio());
            session->stop_chart_audio_workers();
            for (std::size_t asset_id = 0; asset_id < session->chart_audio_assets_.size(); ++asset_id) {
                const auto& asset = session->chart_audio_assets_[asset_id];
                if (asset.use_count == 0) continue;
                const auto pcm = std::atomic_load(&asset.clip.samples);
                const auto identity = std::atomic_load(&asset.clip.identity);
                REQUIRE(pcm);
                REQUIRE(pcm->size() == 256);
                REQUIRE(identity && identity->samples == pcm && identity->id != 0);
                if ((asset_id == 1 || asset_id == 3) && session->chart_audio_assets_[0].use_count > 0) {
                    const auto first = std::atomic_load(&session->chart_audio_assets_[0].clip.identity);
                    REQUIRE(first);
                    CHECK(identity->id == first->id);
                }
            }
            std::vector<gameplay::ReplayEvent> events;
            for (const auto& note : chart.notes) {
                const int64_t stagger = mode == "live" ? (note.lane - 1) * 5 : 0;
                events.push_back({note.lane, input::InputState::Pressed, note.start_sample + stagger});
                events.push_back({note.lane, input::InputState::Released,
                    note.end_sample.value_or(note.start_sample + stagger) + 1});
            }
            std::stable_sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.sample < b.sample; });
            session->autoplay_enabled_ = mode == "autoplay";
            session->replay_playback_enabled_ = mode == "replay";
            session->build_autoplay_events();
            session->replay_source_.trace.sample_rate = 48000;
            session->replay_source_.trace.events = events;
            std::size_t next = 0;
            std::vector<float> output(2048 * 2);
            for (uint32_t start = 0; start < 2048; start += block) {
                const auto count = (std::min)(block, 2048 - start);
                if (mode == "autoplay") session->process_autoplay_queue(start, start + count, 0);
                else if (mode == "replay") session->process_replay_input_queue(start, start + count, 0);
                else while (next < events.size() && events[next].sample < start + count) {
                    const auto event = events[next++];
                    session->dispatch_lane_input(event.lane, event.state, event.sample, start);
                }
                session->engine_->advance(start + count);
                session->schedule_chart_audio(start + count);
                session->mix_chart_audio(output.data() + start * 2, count, start);
            }
            CHECK(session->engine_->stats().total_notes == chart.notes.size());
            CHECK(session->engine_->stats().counts.bd == 0);
            CHECK(session->engine_->stats().counts.pr == 0);
            // The engine intentionally stops recording after completion. A
            // final LN release can arrive in the next callback after its tail
            // already finished the chart; autoplay also releases at the tail,
            // while this live/replay fixture releases one sample afterward.
            const auto& dispatched = mode == "autoplay" ? session->autoplay_events_ : events;
            const auto& recorded = session->engine_->replay().events;
            REQUIRE(recorded.size() <= dispatched.size());
            CHECK(std::count_if(recorded.begin(), recorded.end(), [](const auto& event) {
                return event.state == input::InputState::Pressed;
            }) == chart.notes.size());
            for (std::size_t i = 0; i < recorded.size(); ++i) {
                CHECK(recorded[i].lane == dispatched[i].lane);
                CHECK(recorded[i].state == dispatched[i].state);
                CHECK(recorded[i].sample == dispatched[i].sample);
            }
            int64_t final_note_sample = 0;
            for (const auto& note : chart.notes)
                final_note_sample = (std::max)(final_note_sample, note.end_sample.value_or(note.start_sample));
            for (std::size_t i = recorded.size(); i < dispatched.size(); ++i) {
                CHECK(session->engine_->is_finished());
                CHECK(dispatched[i].state == input::InputState::Released);
                CHECK(dispatched[i].sample >= final_note_sample);
            }
            CHECK(session->chart_.notes.size() == chart.notes.size());
            CHECK(session->chart_.duration_samples == chart.duration_samples);
            CHECK(session->chart_audio_voices_.empty());
            return output;
        };
        auto reference = original;
        reference.notes = {{1, 256, std::nullopt, false, 0}};
        auto chord = reference;
        chord.notes.push_back({2, 256, 1024, false, 1});
        chord.notes.push_back({3, 256, std::nullopt, false, 3});
        for (const std::string mode : {"live", "autoplay", "replay"}) {
            const auto expected = render(reference, mode);
            CHECK(render(chord, mode) == expected);
            auto cross = chord;
            cross.audio_cues = {{256, 0}, {256, 1}};
            CHECK(render(cross, mode) == expected);
            // Legacy per-channel balances no longer change sound or exact-PCM
            // deduplication, including real input and replay scheduling.
            CHECK(render(cross, mode, 0, 0.0) == expected);
            CHECK(render(cross, mode, 0, 2.0) == expected);
        }
        auto bgm = original;
        bgm.audio_cues = {{256, 0}};
        const auto one_bgm = render(bgm, "autoplay");
        bgm.audio_cues.push_back({256, 1});
        CHECK(render(bgm, "autoplay") == one_bgm);
        auto gains = reference;
        gains.audio_cues = {{256, 1, 0.5f}, {256, 2}};
        auto separate = original;
        separate.audio_cues = {{256, 0}, {256, 1, 0.5f}, {256, 2}};
        CHECK(render(gains, "autoplay") == render(separate, "autoplay"));
        CHECK(render(gains, "autoplay") != one_bgm);
        auto adjacent_chart = reference;
        adjacent_chart.notes.push_back({2, 257, std::nullopt, false, 1});
        separate.audio_cues = {{256, 0}, {257, 1}};
        CHECK(render(adjacent_chart, "autoplay") == render(separate, "autoplay"));
        CHECK(render(adjacent_chart, "autoplay") != one_bgm);
        auto cross = reference;
        cross.audio_cues = {{256, 1}};
        CHECK(render(cross, "autoplay", 0, 0.0) == one_bgm);
        CHECK(render(cross, "autoplay", 1) == one_bgm);
        bgm.audio_cues = {{256, 0}};
        const auto early_bgm = render(bgm, "autoplay", -1);
        // BGM onset and a late key can fall in one callback or separate callbacks.
        CHECK(render(cross, "autoplay", -1, 1.0, 256) == early_bgm);
        CHECK(render(cross, "autoplay", -1, 1.0, 32) == early_bgm);
    }

    static void check_reduced_keysound_pcm_render() {
        CompletionTestDirectory directory;
        gameplay::GameplayChart original;
        original.lane_count = 8;
        original.duration_samples = 12000;
        // Decode real PCM files through the production loader, then render with
        // the same scheduler/resampler as gameplay. No audio device is opened.
        for (int asset = 0; asset < 9; ++asset) {
            const auto wav = directory.path / ("sound-" + std::to_string(asset) + ".wav");
            std::ofstream file(wav, std::ios::binary);
            const auto le = [&](uint32_t value, int bytes) {
                for (int i = 0; i < bytes; ++i) file.put(static_cast<char>((value >> (8 * i)) & 0xff));
            };
            constexpr int frames = 512;
            file.write("RIFF", 4); le(36 + frames * 2, 4); file.write("WAVEfmt ", 8);
            le(16, 4); le(1, 2); le(1, 2); le(48000, 4); le(96000, 4);
            le(2, 2); le(16, 2); file.write("data", 4); le(frames * 2, 4);
            for (int frame = 0; frame < frames; ++frame) {
                const auto value = static_cast<int16_t>(2000 * std::sin(
                    (frame + 1) * 6.283185307179586 * (180 + 60 * asset) / 48000.0));
                le(static_cast<uint16_t>(value), 2);
            }
            file.close();
            (void)original.intern_audio_asset(wav.u8string());
        }
        const auto render = [&](const gameplay::GameplayChart& chart, double rate,
                                 int offset_ms, bool play_notes) {
            auto session = std::make_unique<GameSession>();
            session->sample_rate_ = 48000;
            session->chart_ = chart;
            session->config_.speed.rate = rate;
            session->config_.sound_offset_ms = offset_ms;
            session->config_.audio_ui.bgm_volume = 1.0;
            session->config_.audio_ui.keysound_volume = 1.0;
            REQUIRE(session->prepare_chart_audio());
            session->stop_chart_audio_workers();
            for (const auto& asset : session->chart_audio_assets_) {
                const auto clip = std::atomic_load(&asset.clip.samples);
                REQUIRE(clip);
                REQUIRE(clip->size() == 1024u);
            }
            REQUIRE(session->chart_audio_events_.size() == chart.audio_cues.size());
            for (std::size_t i = 0; i < chart.audio_cues.size(); ++i) {
                CHECK(session->chart_audio_events_[i].start_sample ==
                      chart.audio_cues[i].start_sample + offset_ms * 48);
                CHECK(session->chart_audio_events_[i].gain == chart.audio_cues[i].gain);
            }
            if (play_notes) {
                // Offset0 is perfect authored-time input. The offset7 variant
                // aligns reference input to the existing BGM sound-offset policy.
                for (const auto& note : chart.notes)
                    session->schedule_note_keysound(note, note.start_sample + offset_ms * 48);
            }
            constexpr uint32_t total_frames = 12000;
            std::vector<float> output(total_frames * 2);
            for (uint32_t start = 0; start < total_frames; start += 127) {
                const auto count = std::min<uint32_t>(127, total_frames - start);
                session->schedule_chart_audio(start + count);
                session->mix_chart_audio(output.data() + start * 2, count, start);
            }
            CHECK(session->chart_audio_voices_.empty());
            CHECK(session->chart_.duration_samples == chart.duration_samples);
            return output;
        };
        for (const double rate : {0.5, 1.0, 2.0}) {
            auto source = original;
            const auto head = static_cast<int64_t>(960 / rate);
            source.audio_cues = {{static_cast<int64_t>(240 / rate), 8}};
            for (int lane = 1; lane <= 8; ++lane) {
                gameplay::NoteEvent note;
                note.lane = lane;
                note.note_id = static_cast<std::size_t>(lane);
                note.start_sample = head;
                if (lane % 2 == 0) note.end_sample = static_cast<int64_t>(4320 / rate);
                note.add_audio_asset(static_cast<std::size_t>(lane - 1));
                note.audio_gain = static_cast<float>(lane) * 0.1f;
                source.notes.push_back(note);
            }
            for (const auto algorithm : {gameplay::KeyModeConversionAlgorithm::Krrcream,
                                          gameplay::KeyModeConversionAlgorithm::NK2,
                                          gameplay::KeyModeConversionAlgorithm::NK3}) {
                gameplay::ModeSettings settings;
                settings.key_mode = gameplay::KeyMode::Keys4;
                settings.key_conversion_algorithm = algorithm;
                settings.random_seed = 42;
                const auto converted = gameplay::apply_mode_settings(source, settings, {120.0 * rate, 48000});
                REQUIRE(converted.chart.lane_count == 4);
                REQUIRE(converted.chart.audio_cues.size() > source.audio_cues.size());
                for (const int offset : {0, 7}) {
                    const auto reference = render(source, rate, offset, true);
                    const auto actual = render(converted.chart, rate, offset, true);
                    double max_error = 0.0;
                    for (std::size_t i = 0; i < reference.size(); ++i)
                        max_error = (std::max)(max_error, std::abs(static_cast<double>(reference[i] - actual[i])));
                    CHECK(max_error <= 0.000001);
                    // No input: dropped notes remain audible, while the retained
                    // notes still require input. This also detects duplicated voices.
                    const auto automatic = render(converted.chart, rate, offset, false);
                    const auto old_bgm = render(source, rate, offset, false);
                    CHECK(automatic != old_bgm);
                    CHECK(automatic != actual);
                    // An explicitly requested test evidence directory can keep
                    // one decoded/mixed example; ordinary CTest leaves no files.
                    if (rate == 1.0 && offset == 0 && algorithm == gameplay::KeyModeConversionAlgorithm::Krrcream) {
#ifdef _WIN32
                        if (const wchar_t* requested = _wgetenv(L"TENRIFF_KEYSOUND_EVIDENCE_DIR")) {
                            const auto destination = std::filesystem::path(requested);
#else
                        if (const char* requested = std::getenv("TENRIFF_KEYSOUND_EVIDENCE_DIR")) {
                            const auto destination = std::filesystem::u8path(requested);
#endif
                            REQUIRE(std::filesystem::is_directory(destination));
                            const auto save = [&](const char* name, const std::vector<float>& pcm) {
                                const auto path = destination / name;
                                REQUIRE_FALSE(std::filesystem::exists(path));
                                std::ofstream wav(path, std::ios::binary);
                                const auto le = [&](uint32_t value, int bytes) {
                                    for (int i = 0; i < bytes; ++i)
                                        wav.put(static_cast<char>((value >> (8 * i)) & 0xff));
                                };
                                const auto bytes = static_cast<uint32_t>(pcm.size() * 2);
                                wav.write("RIFF", 4); le(36 + bytes, 4); wav.write("WAVEfmt ", 8);
                                le(16, 4); le(1, 2); le(2, 2); le(48000, 4); le(192000, 4);
                                le(4, 2); le(16, 2); wav.write("data", 4); le(bytes, 4);
                                for (const float sample : pcm)
                                    le(static_cast<uint16_t>(static_cast<int16_t>(
                                        std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767.0f))), 2);
                                REQUIRE(wav.good());
                            };
                            save("reference-complete.wav", reference);
                            save("reduced-complete.wav", actual);
                            save("reduced-no-input.wav", automatic);
                            save("original-bgm-only.wav", old_bgm);
                        }
                    }
                    std::cout << "[reduced-keysound-pcm] algorithm=" << gameplay::to_string(algorithm)
                              << " rate=" << rate << " offset_ms=" << offset
                              << " restored=" << converted.chart.audio_cues.size() - source.audio_cues.size()
                              << " frames=12000 max_abs_error=" << max_error << '\n';
                }
            }
        }
    }

    static void check_audio_preparation() {
        CompletionTestDirectory directory;
        const auto wav = directory.path / "tail.wav";
        {
            std::ofstream file(wav, std::ios::binary);
            const auto le = [&](uint32_t value, int bytes) {
                for (int i = 0; i < bytes; ++i) file.put(static_cast<char>((value >> (8 * i)) & 0xff));
            };
            file.write("RIFF", 4); le(44, 4); file.write("WAVEfmt ", 8);
            le(16, 4); le(1, 2); le(1, 2); le(48000, 4); le(96000, 4);
            le(2, 2); le(16, 2); file.write("data", 4); le(8, 4);
            for (int i = 0; i < 4; ++i) le(1000, 2);
        }
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->chart_.duration_samples = 1;
        session->chart_.audio_assets = {{wav.u8string()}, {(directory.path / "missing.wav").u8string()}};
        session->chart_.audio_cues = {{0, 0}, {0, 1}};
        REQUIRE(session->prepare_chart_audio());
        session->stop_chart_audio_workers();
        CHECK(session->chart_.duration_samples == 1);
        const auto ready = std::atomic_load(&session->chart_audio_assets_[0].clip.samples);
        const auto failed = std::atomic_load(&session->chart_audio_assets_[1].clip.samples);
        REQUIRE(ready);
        CHECK_FALSE(ready->empty());
        REQUIRE(failed);
        CHECK(failed->empty());
        session->schedule_chart_audio(480);
        std::vector<float> audio(960);
        session->mix_chart_audio(audio.data(), 480, 0);
        CHECK(session->chart_audio_voices_.empty());
        CHECK(std::any_of(audio.begin(), audio.end(), [](float x) { return x != 0.0f; }));
    }

    static void check_automatic_result(bool delayed_bgm, bool manual_skip, bool silent,
                                       bool failed_audio = false, bool skip_outro = false, bool pacemaker = false,
                                       bool auto_scratch = false, bool stale_catchup = false,
                                       uint32_t skip_key = 13, bool future_skip = false, bool bms_policy = false,
                                       bool online_origin = true, bool revoke_online = false) {
        auto session = std::make_unique<GameSession>();
        session->options_.online_recording_enabled = online_origin;
        session->options_.online_recording_permission = std::make_shared<std::atomic_bool>(online_origin);
        session->sample_rate_ = 48000;
        session->enter_keycode_ = 13;
        session->chart_.lane_count = 4;
        session->chart_.bms_rank = 3; // BMS loader resolves absent #RANK to EASY.
        if (bms_policy) {
            session->chart_.bms_total = 200.0;
            session->chart_.bms_has_random = true;
            session->bms_gauge_version_ = 2;
            session->bms_control_flow_version_ = 2;
            session->bms_random_seed_ = 0xffffffffu;
        }
        // Trailing chart measures and startup decode estimates must not keep a
        // completed score waiting after all audible voices have drained.
        session->chart_.duration_samples = 960000;
        session->chart_.notes.push_back(gameplay::NoteEvent{1, 12000, std::nullopt});
        gameplay::GameplayConfig config;
        config.sample_rate = 48000;
        config.gauge_shift_enabled = true;
        config.gauge_policy.bms_lr2_deltas = bms_policy;
        config.gauge_policy.bms_total = session->chart_.bms_total.value_or(0.0);
        config.judge = judge_timing_for_fixed_baseline({}, session->chart_.bms_rank, false, false, false);
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->gauge_shift_enabled_ = true;
        if (pacemaker) {
            session->pacemaker_mode_ = "score";
            session->config_.mode.pacemaker_target_score = 10000;
        }
        if (auto_scratch) {
            session->auto_scratch_enabled_ = true;
            session->active_mods_ = {"auto_scratch"};
            session->score_multiplier_ = 0.0;
        }
        session->gameplay_started_ = true;
        session->config_.ui.result_tail_ms = 100;
        session->config_.audio_ui.play_to_end = !skip_outro;
        session->config_.mode.key_mode = "4k";
        session->chart_format_ = ChartFormat::Bms;
        // This direct fixture bypasses initialize(), which keeps unaffected
        // BPM-only charts on the historical timing identity for web records.
        session->bms_timing_version_ = 1;
        session->escape_keycode_ = 27;
        session->key_to_lane_[32] = 1;
        session->lane_binding_state_.configure(session->key_to_lane_);
        session->lane_activity_.assign(4, 0);
        session->lane_pressed_.assign(4, 0);
        session->synthetic_tones_enabled_ = false;
        if (!silent) {
            session->chart_audio_assets_.resize(1);
            auto& asset = session->chart_audio_assets_[0];
            asset.clip.samples = std::make_shared<const std::vector<float>>(96000, 0.25f);
            if (failed_audio) asset.clip.samples = std::make_shared<const std::vector<float>>();
            asset.estimated_decoded_bytes = 960000 * 8;
            session->chart_audio_events_.push_back({delayed_bgm ? 48000 : 0, 0,
                                                    GameSession::ChartAudioEvent::Kind::Bgm});
        }
        const int64_t hit_sample = pacemaker ? 13200 : 12000; // GR deliberately misses a 10000 target
        if (stale_catchup) {
            session->startup_input_timing_anchor_ = {0, 2'000'000'000LL, true};
            input::InputEvent stale{};
            stale.keycode = 32;
            stale.state = input::InputState::Pressed;
            stale.input_time_ns = 1'000'000'000LL;
            REQUIRE(session->input_thread_.queue().push(stale));
            session->process_input_queue(0, 480, 0);
            CHECK(session->stale_input_catchup_used_);
            session->lane_binding_state_.reset();
            session->catch_up_lane_input(1, input::InputState::Released, 0);
        }
        session->dispatch_lane_input(1, input::InputState::Pressed, hit_sample, hit_sample);
        session->dispatch_lane_input(1, input::InputState::Released, hit_sample + 1, hit_sample);
        std::vector<float> audio(960);
        session->audio_thread_.callback_ = [&](float* out, uint32_t frames, int64_t write, int64_t playback) {
            session->audio_callback(out, frames, write, playback);
        };
        const int64_t expected_end = (skip_outro || silent || failed_audio) ? 17280 : delayed_bgm ? 96000 : 48000;
        // Model two queued device buffers. No input is injected after the last
        // note unless this is the explicit skip compatibility case.
        int64_t last_buffer_sample = 0;
        for (int64_t sample = 12000; sample <= expected_end + 960; sample += 480) {
            last_buffer_sample = sample;
            // Exercise both queue paths: lane taps and Esc must not finish the
            // pending tail, while the later deliberate Enter still can.
            if (sample == 12960 || sample == 13440) {
                input::InputEvent ignored{};
                ignored.keycode = sample == 12960 ? 32 : 27;
                ignored.state = input::InputState::Pressed;
                if (future_skip) REQUIRE(session->future_events_.push({ignored, sample}));
                else REQUIRE(session->input_thread_.queue().push(ignored));
            }
            if (manual_skip && sample == 14400) {
                input::InputEvent skip{};
                skip.keycode = skip_key;
                skip.state = input::InputState::Pressed;
                if (future_skip) REQUIRE(session->future_events_.push({skip, sample}));
                else REQUIRE(session->input_thread_.queue().push(skip));
            }
            session->audio_thread_.invoke_callback(audio.data(), 480, sample, sample - 960,
                3'000'000'000LL + (sample - 960) * 1'000'000'000LL / 48000);
            // A future-queue event waits for the audible judgement horizon;
            // the two queued device buffers must not make the fixture end early.
            if (manual_skip && session->finished_.load()) break;
            if (sample - 960 < expected_end) CHECK_FALSE(session->finished_.load());
        }
        REQUIRE(session->finished_.load());
        if (manual_skip) CHECK(last_buffer_sample <= 16320);
        CHECK_FALSE(session->user_aborted_.load());
        CHECK_FALSE(session->paused_.load());
        CHECK_FALSE(session->pause_used_.load());
        CHECK(session->engine_->stats().counts.pg == (pacemaker ? 0 : 1));
        CHECK(session->engine_->stats().counts.gr == (pacemaker ? 1 : 0));
        const auto recorded_events = session->engine_->replay().events;
        // R5 advances through the final tap before handling release at hit+1;
        // the finished engine does not record that later, scoreless release.
        // Actual stale state transitions remain in the custom replay trace.
        REQUIRE(recorded_events.size() == (stale_catchup ? 3u : 1u));
        if (stale_catchup) {
            CHECK(recorded_events[0].lane == 1);
            CHECK(recorded_events[0].state == input::InputState::Pressed);
            CHECK(recorded_events[0].sample == 0);
            CHECK(recorded_events[1].lane == 1);
            CHECK(recorded_events[1].state == input::InputState::Released);
            CHECK(recorded_events[1].sample == 0);
        }
        CHECK(recorded_events.back().lane == 1);
        CHECK(recorded_events.back().state == input::InputState::Pressed);
        CHECK(recorded_events.back().sample == hit_sample);

        CompletionTestDirectory directory;
        const auto chart_path = directory.path / "fixture.bms";
        { std::ofstream file(chart_path);
          file << "#TITLE Completion fixture\n#BPM 120\n";
          if (bms_policy) file << "#RANDOM 1\n#IF 1\n#00111:01\n#ENDIF\n#ENDRANDOM\n";
        }
        session->chart_path_ = chart_path.u8string();
        session->profile_dir_ = directory.path.u8string();
        if (revoke_online) session->options_.online_recording_permission->store(false);
        session->shutdown();
        const auto& result = session->result();
        REQUIRE(result.has_value);
        CHECK(result.bms_timing_version == 1);
        CHECK(result.bms_gauge_version == (bms_policy ? 2 : 1));
        CHECK(result.bms_control_flow_version == (bms_policy ? 2 : 1));
        CHECK(result.bms_random_seed == (bms_policy ? std::optional<uint32_t>(0xffffffffu) : std::nullopt));
        CHECK(result.finished);
        CHECK(result.all_combo_badge == !auto_scratch);
        REQUIRE_FALSE(result.replay_path.empty());
        REQUIRE_FALSE(result.result_path.empty());
        CHECK(std::filesystem::u8path(result.replay_path).parent_path() == directory.path / "replays");
        CHECK(std::filesystem::u8path(result.result_path).parent_path() == directory.path / "results");
        CHECK(std::filesystem::is_regular_file(std::filesystem::u8path(result.replay_path)));
        CHECK(std::filesystem::is_regular_file(std::filesystem::u8path(result.result_path)));
        REQUIRE(result.replay_sha256.size() == 64);
        const auto replay = gameplay::load_replay_json(result.replay_path);
        REQUIRE(replay.success());
        const bool can_upload_origin = online_origin && !revoke_online;
        CHECK(replay.replay->record_play_mode == (can_upload_origin ? "online" : "guest"));
        {
            std::ifstream saved_result(std::filesystem::u8path(result.result_path));
            const std::string saved((std::istreambuf_iterator<char>(saved_result)), std::istreambuf_iterator<char>());
            const auto json = config::parse_json(saved);
            REQUIRE(json.success());
            CHECK(json.root->as_object()->at("record_play_mode").as_string() == replay.replay->record_play_mode);
        }
        CHECK(replay.replay->mode.bms_timing_version == 1);
        CHECK(replay.replay->mode.bms_gauge_version == result.bms_gauge_version);
        CHECK(replay.replay->mode.bms_control_flow_version == result.bms_control_flow_version);
        CHECK(replay.replay->mode.bms_random_seed == result.bms_random_seed);
        REQUIRE(replay.replay->trace.events.size() == recorded_events.size());
        for (std::size_t i = 0; i < recorded_events.size(); ++i) {
            CHECK(replay.replay->trace.events[i].lane == recorded_events[i].lane);
            CHECK(replay.replay->trace.events[i].state == recorded_events[i].state);
            CHECK(replay.replay->trace.events[i].sample == recorded_events[i].sample);
        }
        std::string payload, error, branch_sha256;
        REQUIRE(prepare_sites_bms_branch_identity(*replay.replay, branch_sha256, error));
        CHECK(branch_sha256.empty() == !bms_policy);
        const bool accepted = build_sites_score_json(*replay.replay, result.replay_sha256,
                                    "Completion fixture", result.clear_status, payload, error, {}, branch_sha256);
        const bool expected_upload = can_upload_origin && !auto_scratch && !stale_catchup;
        CHECK(accepted == expected_upload);
        CHECK(payload.empty() == !expected_upload);
        CHECK(error.empty() == expected_upload);
        if (!(auto_scratch || stale_catchup)) CHECK(replay.replay->ruleset_id == kCanonicalReplayRulesetId);
        if (stale_catchup) {
            CHECK(replay.replay->ruleset_id == "custom-ruleset-13");
            CHECK_FALSE(is_supported_canonical_replay_ruleset(replay.replay->ruleset_id));
            CHECK_FALSE(result.pause_used);
            CHECK(std::any_of(result.export_warnings.begin(), result.export_warnings.end(),
                [](const std::string& warning) { return warning.find("stale input state catch-up") != std::string::npos; }));
            std::ifstream saved_result(std::filesystem::u8path(result.result_path));
            REQUIRE(saved_result.is_open());
            const std::string saved((std::istreambuf_iterator<char>(saved_result)), std::istreambuf_iterator<char>());
            CHECK(saved.find("custom-ruleset-13") != std::string::npos);
        }
        if (auto_scratch) {
            CHECK(replay.replay->ruleset_id == "custom-ruleset-13");
            CHECK(result.final_score == 0);
            CHECK(replay.replay->final_score == 0);
            CHECK(result.clear_status.find("ASSIST AUTO SCRATCH") == 0);
        }
        if (pacemaker) {
            CHECK(result.final_score < 10000);
            CHECK_FALSE(result.game_over);
            CHECK(result.clear_status.find("PACEMAKER") == std::string::npos);
            CHECK_FALSE(replay.replay->mode.practice_no_fail_enabled);
        }
        CHECK(result.export_warnings.empty() == !stale_catchup);
    }
    static void check_legacy_branch_result_seed() {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->chart_format_ = ChartFormat::Bms;
        session->chart_.lane_count = 1;
        session->chart_.bms_has_random = true;
        session->chart_.notes.push_back(gameplay::NoteEvent{1, 0, std::nullopt});
        session->chart_.duration_samples = 48000;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, gameplay::GameplayConfig{});
        session->engine_->advance(480000);
        session->gameplay_started_ = true;
        session->replay_playback_enabled_ = true;
        session->replay_source_.mode.random_seed = 123;
        session->bms_random_seed_ = 123;
        session->shutdown();
        REQUIRE(session->result().has_value);
        CHECK_FALSE(session->result().bms_random_seed.has_value());
        CHECK(session->result().bms_gauge_version == 1);
        CHECK(session->result().bms_control_flow_version == 1);
    }

    static std::unique_ptr<GameSession> pause_fixture() {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->chart_.lane_count = 1;
        session->chart_.duration_samples = 480000;
        session->chart_.notes.push_back(gameplay::NoteEvent{1, 240000, std::nullopt});
        gameplay::GameplayConfig config;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->engine_->advance(48000);
        session->gameplay_started_ = true;
        session->escape_keycode_ = 27;
        session->enter_keycode_ = 13;
        session->down_keycode_ = 40;
        session->key_to_lane_[32] = 1;
        session->lane_binding_state_.configure(session->key_to_lane_);
        session->lane_activity_.assign(1, 0);
        session->lane_pressed_.assign(1, 0);
        session->synthetic_tones_enabled_ = false;
        session->chart_audio_assets_.resize(1);
        session->chart_audio_assets_[0].clip.samples = std::make_shared<const std::vector<float>>(960, 0.25f);
        session->chart_audio_voices_.push_back({48000, 0, GameSession::ChartAudioEvent::Kind::Bgm, 1.0f});
        return session;
    }

    static void control(GameSession& session, uint32_t key) {
        input::InputEvent event{};
        event.keycode = key;
        event.state = input::InputState::Pressed;
        REQUIRE(session.handle_control_input(event));
    }

    static void check_outro_escape_guards(bool future_queue) {
        // A live LN (even after its head is judged) and a future tap must still
        // pause. Only a completed solo outro may bypass the pause UI.
        for (int mode = 0; mode < 9; ++mode) {
            auto session = shift_lane_fixture(32, mode == 1);
            session->config_.audio_ui.play_to_end = mode != 3;
            session->result_transition_pending_ = mode != 4;
            if (mode != 2) {
                queue_physical_edge(*session, 32, input::InputState::Pressed, 48000, future_queue);
            }
            session->course_gauge_enabled_ = mode == 5;
            session->peer_battle_mode_ = mode == 6;
            session->autoplay_enabled_ = mode == 7;
            session->replay_playback_enabled_ = mode == 8;
            const auto recorded_events = session->engine_->replay().events.size();
            queue_physical_edge(*session, 27, input::InputState::Pressed, 48480, future_queue);
            const bool skip = false; // Esc never confirms a completed outro.
            const bool abort_peer = mode == 6;
            CHECK(session->finished_.load() == (skip || abort_peer));
            CHECK(session->user_aborted_.load() == abort_peer);
            CHECK(session->paused_.load() == (mode == 1 || mode == 2 || mode == 4));
            CHECK(session->pause_used_.load() == (mode == 1 || mode == 2 || mode == 4));
            CHECK(session->engine_->replay().events.size() == recorded_events);
        }
    }

    static void paused_buffer(GameSession& session, int64_t sample, int count) {
        std::vector<float> audio(960, 1.0f);
        callback_with_sample_clock(session, audio.data(), 480, sample, sample);
        CHECK(std::all_of(audio.begin(), audio.end(), [](float x) { return x == 0.0f; }));
        CHECK(session.last_audio_sample_.load() == 48000);
        CHECK(session.resume_countdown_value_.load() == count);
        CHECK(session.engine_->stats().raw_score == 0);
    }

    static void check_instant_restart_guards() {
        constexpr uint32_t restart_key = 123; // F12, never injected into the OS
        for (int blocked = 0; blocked < 7; ++blocked) {
            auto session = pause_fixture();
            session->instant_restart_keycode_ = restart_key;
            if (blocked == 1) session->peer_battle_mode_ = true;
            if (blocked == 2) session->set_course_gauge(100.0);
            if (blocked == 3) session->countdown_active_ = true;
            if (blocked == 4) session->paused_ = true;
            if (blocked == 5) session->gameplay_started_ = false;
            if (blocked == 6) session->instant_restart_key_armed_ = false;
            control(*session, restart_key);
            CHECK(session->was_restart_requested() == (blocked == 0));
            CHECK(session->was_instant_restart_requested() == (blocked == 0));
            CHECK(session->was_user_aborted() == (blocked == 0));
            CHECK(session->engine_->replay().events.empty());
            CHECK_FALSE(session->was_exit_requested());
            if (blocked == 0) {
                session->shutdown();
                CHECK_FALSE(session->result().has_value);
                CHECK(session->result().replay_path.empty());
                CHECK(session->result().result_path.empty());
            }
        }
        auto session = pause_fixture();
        session->instant_restart_keycode_ = restart_key;
        session->instant_restart_key_armed_ = false;
        control(*session, restart_key);
        control(*session, restart_key);
        CHECK_FALSE(session->was_restart_requested());
        input::InputEvent release{};
        release.keycode = restart_key;
        release.state = input::InputState::Released;
        REQUIRE(session->handle_control_input(release));
        control(*session, restart_key);
        CHECK(session->was_instant_restart_requested());

        auto overlay = pause_fixture();
        overlay->instant_restart_keycode_ = restart_key;
        overlay->control_input_callback_ = [](const input::InputEvent&) { return true; };
        control(*overlay, restart_key);
        CHECK_FALSE(overlay->was_restart_requested());
        overlay->control_input_callback_ = {};
        control(*overlay, restart_key);
        CHECK_FALSE(overlay->was_restart_requested());
        REQUIRE(overlay->handle_control_input(release));
        control(*overlay, restart_key);
        CHECK(overlay->was_instant_restart_requested());
    }

    static void check_instant_restart_input_queues() {
        for (bool replay : {false, true}) {
            auto session = pause_fixture();
            session->instant_restart_keycode_ = 123;
            session->replay_playback_enabled_ = replay;
            session->rebuild_polled_gameplay_keys();
            CHECK(std::any_of(session->polled_gameplay_keys_.begin(), session->polled_gameplay_keys_.end(),
                              [](const auto& key) { return key.keycode == 123; }));
            input::InputEvent press{};
            press.keycode = 123;
            press.state = input::InputState::Pressed;
            press.input_time_ns = timing::HighResClock::now_ns();
            REQUIRE(session->input_thread_.queue().push(press));
            session->process_input_queue(48000, 48480, 0);
            CHECK(session->was_instant_restart_requested());
            CHECK(session->engine_->replay().events.empty());
            session->shutdown();
            CHECK_FALSE(session->result().has_value);
        }
        // Ordinary pause Retry remains distinct from the immediate shortcut.
        auto pause = pause_fixture();
        control(*pause, 27);
        control(*pause, 40);
        control(*pause, 13);
        CHECK(pause->was_restart_requested());
        CHECK_FALSE(pause->was_instant_restart_requested());
    }

    static void check_pause_countdown(uint32_t resume_key, bool skip_start = false) {
        auto session = pause_fixture();
        session->config_.ui.skip_start_countdown = skip_start;
        control(*session, 27);
        CHECK(session->paused_.load());
        paused_buffer(*session, 48000, 0);
        control(*session, resume_key);
        control(*session, 40);
        control(*session, 13);
        CHECK_FALSE(session->restart_requested_.load());
        paused_buffer(*session, 48480, 3);
        const auto hud = session->hud_snapshot();
        CHECK(hud.paused);
        CHECK(hud.countdown_active);
        CHECK(hud.countdown_value == 3);
        control(*session, 13); // repeated Continue cannot bypass/restart the delay
        input::InputEvent lane{};
        lane.keycode = 32;
        lane.state = input::InputState::Pressed;
        REQUIRE(session->input_thread_.queue().push(lane));
        paused_buffer(*session, 96480, 2);
        paused_buffer(*session, 144480, 1);
        paused_buffer(*session, 192000, 1);
        CHECK(session->paused_.load());
        paused_buffer(*session, 192480, 0);
        CHECK_FALSE(session->paused_.load());
        CHECK_FALSE(session->hud_snapshot().countdown_active);
        std::vector<float> audio(960, 0.0f);
        callback_with_sample_clock(*session, audio.data(), 480, 192960, 192960);
        CHECK(session->last_audio_sample_.load() == 48480);
        CHECK(std::any_of(audio.begin(), audio.end(), [](float x) { return x != 0.0f; }));
        CHECK(session->engine_->stats().raw_score == 0);
        CHECK(session->pause_used_.load());
    }

    static void check_countdown_cancel() {
        auto session = pause_fixture();
        control(*session, 27);
        paused_buffer(*session, 48000, 0);
        control(*session, 27);
        paused_buffer(*session, 48480, 3);
        paused_buffer(*session, 96480, 2);
        control(*session, 27);
        paused_buffer(*session, 200000, 0);
        CHECK(session->paused_.load());
        CHECK_FALSE(session->hud_snapshot().countdown_active);
        control(*session, 13);
        paused_buffer(*session, 200480, 3);
    }

    static std::unique_ptr<GameSession> pause_hud_fixture() {
        auto session = pause_fixture();
        // A short note shortly ahead of the audible head, then an unhit LN.
        // 250ms of queued audio is enough for the write head to expire the short
        // note even while the player still needs to see it at the playback head.
        session->chart_.notes = {gameplay::NoteEvent{1, 37500, std::nullopt},
                                 gameplay::NoteEvent{1, 42000, 100000},
                                 gameplay::NoteEvent{1, 240000, std::nullopt}};
        for (std::size_t i = 0; i < session->chart_.notes.size(); ++i)
            session->chart_.notes[i].note_id = i;
        gameplay::GameplayConfig config;
        config.sample_rate = 48000;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->ghost_engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->hidden_hit_note_ids_.assign(session->chart_.notes.size(), 0);
        session->ghost_hidden_hit_note_ids_.assign(session->chart_.notes.size(), 0);
        session->ghost_lane_activity_.assign(1, 0);
        session->ghost_lane_pressed_.assign(1, 0);
        session->config_.audio_ui.mute_when_inactive = false;
        return session;
    }

    static bool hud_contains_note(const GameSession::HudSnapshot& hud, int64_t start, bool ghost = false) {
        const auto& notes = ghost ? hud.ghost_notes : hud.notes;
        const auto count = ghost ? hud.ghost_note_count : hud.note_count;
        return std::any_of(notes.begin(), notes.begin() + count,
            [start](const auto& note) { return note.start_sample == start; });
    }

    static void check_pause_resume_hud_playback_anchor(bool pause_in_audio_queue) {
        auto session = pause_hud_fixture();
        std::vector<float> audio(960);
        callback_with_sample_clock(*session, audio.data(), 480, 47520, 35520);
        const auto before = session->hud_snapshot();
        REQUIRE(before.current_sample == 35520);
        REQUIRE(hud_contains_note(before, 37500));
        REQUIRE(hud_contains_note(before, 37500, true));
        if (pause_in_audio_queue) {
            input::InputEvent escape{};
            escape.keycode = 27;
            escape.state = input::InputState::Pressed;
            REQUIRE(session->input_thread_.queue().push(escape));
        } else {
            control(*session, 27);
        }
        callback_with_sample_clock(*session, audio.data(), 480, 48000, 36000);
        const auto paused = session->hud_snapshot();
        CHECK(paused.paused);
        CHECK(paused.current_sample == 36000);
        CHECK(hud_contains_note(paused, 37500));
        CHECK(hud_contains_note(paused, 37500, true));
        CHECK(hud_contains_note(paused, 42000));

        control(*session, 13);
        for (const int64_t write : {48480LL, 96480LL, 144480LL, 192480LL}) {
            callback_with_sample_clock(*session, audio.data(), 480, write, write - 12000);
            const auto countdown = session->hud_snapshot();
            CHECK(countdown.current_sample == 36000);
            CHECK(hud_contains_note(countdown, 37500));
            CHECK(hud_contains_note(countdown, 37500, true));
        }
        REQUIRE_FALSE(session->paused_.load());
        callback_with_sample_clock(*session, audio.data(), 480, 192960, 180960);
        const auto resumed = session->hud_snapshot();
        CHECK_FALSE(resumed.paused);
        CHECK(resumed.current_sample == 36000);
        CHECK(resumed.current_sample >= paused.current_sample);
        CHECK(hud_contains_note(resumed, 37500));
        CHECK(hud_contains_note(resumed, 37500, true));
        CHECK(hud_contains_note(resumed, 42000));
        CHECK(session->last_audio_sample_.load() == 48480);
    }

    static void check_hud_note_scan_after_playback_rewind() {
        auto session = pause_hud_fixture();
        // Historical replay playback retains raw device rewinds. R5 instead
        // follows device phase changes continuously without instant rewinds.
        session->chronological_judgement_ = false;
        std::vector<float> audio(960);
        session->audio_callback(audio.data(), 480, 96000, 96000);
        const auto ahead = session->hud_snapshot();
        REQUIRE_FALSE(hud_contains_note(ahead, 37500));
        REQUIRE_FALSE(hud_contains_note(ahead, 37500, true));
        // Devices may revise queued playback position independently of the
        // monotonic write cursor; rebuilding visual cues must also rewind notes.
        session->audio_callback(audio.data(), 480, 96480, 36000);
        const auto rewound = session->hud_snapshot();
        CHECK(rewound.current_sample == 36000);
        CHECK(hud_contains_note(rewound, 37500));
        CHECK(hud_contains_note(rewound, 37500, true));
        CHECK(hud_contains_note(rewound, 42000));
    }

    static void check_course_escape() {
        for (bool started : {false, true}) {
            auto session = pause_fixture();
            session->set_course_gauge(100.0);
            session->gameplay_started_ = started;
            control(*session, 27);
            CHECK_FALSE(session->paused_.load());
            CHECK_FALSE(session->pause_used_.load());
            CHECK_FALSE(session->stop_requested_.load());
            CHECK_FALSE(session->finished_.load());
            CHECK_FALSE(session->user_aborted_.load());
        }
    }

    static void check_gameplay_start_boundary() {
        {
            auto session = pause_fixture();
            session->gameplay_started_ = false;
            session->countdown_active_ = true;
            input::InputEvent cancel{};
            cancel.keycode = 27;
            cancel.state = input::InputState::Pressed;
            REQUIRE(session->input_thread_.queue().push(cancel));
            session->run();
            CHECK(session->was_user_aborted());
            CHECK_FALSE(session->did_start_gameplay());
        }
        {
            auto session = pause_fixture();
            session->gameplay_started_ = false;
            // No backend is initialized: exercise the real failed-start branch
            // without opening a sound device or changing the user's audio state.
            session->run();
            CHECK_FALSE(session->audio_error().empty());
            CHECK_FALSE(session->did_start_gameplay());
        }
    }

    static void check_secondary_owner_rebaseline() {
        for (const int scenario : {0, 1, 2, 3}) {
            auto session = shift_lane_fixture(32, true);
            session->gameplay_started_ = true;
            session->key_to_lane_[33] = 1;
            session->lane_binding_state_.configure(session->key_to_lane_);
            session->rebuild_polled_gameplay_keys();
            // Reverse enumeration intentionally: current ownership must not be
            // selected by the order of a physical-key snapshot.
            std::reverse(session->polled_gameplay_keys_.begin(), session->polled_gameplay_keys_.end());
            queue_physical_edge(*session, 32, input::InputState::Pressed, 48000);
            if (scenario != 1) queue_physical_edge(*session, 33, input::InputState::Pressed, 49000);
            if (scenario == 0) queue_physical_edge(*session, 33, input::InputState::Released, 50000);
            const auto score_before = session->engine_->stats().raw_score;
            const auto events_before = session->engine_->replay().events.size();
            session->physical_key_state_probe_ = [scenario](uint32_t key) {
                return key == 32 || (scenario == 3 && key == 33);
            };
            session->rebaseline_gameplay_start_input_state(50000);
            CHECK(session->engine_->stats().raw_score == score_before);
            const bool held = scenario == 1 || scenario == 3;
            CHECK(session->lane_binding_state_.pressed(1) == held);
            CHECK(session->lane_pressed_[0] == (held ? 1 : 0));
            CHECK(session->engine_->replay().events.size() == events_before + (scenario == 2 ? 1 : 0));
            if (scenario == 2) {
                CHECK(session->engine_->replay().events.back().state == input::InputState::Released);
                CHECK(session->engine_->replay().events.back().sample == 50000);
            }
            if (scenario == 3) {
                // The older held key is not the owner, regardless of polling order.
                queue_physical_edge(*session, 32, input::InputState::Released, 70000);
                CHECK(session->lane_binding_state_.pressed(1));
            }
            if (held) {
                queue_physical_edge(*session, scenario == 1 ? 32 : 33, input::InputState::Released, 96000);
                CHECK(session->engine_->stats().counts.pg == 2);
                CHECK(session->engine_->stats().counts.bd == 0);
            } else {
                // A repeated raw/polled down from the surrendered key cannot revive it.
                queue_physical_edge(*session, 32, input::InputState::Pressed, 50000);
                CHECK_FALSE(session->lane_binding_state_.pressed(1));
                CHECK(session->engine_->stats().raw_score == score_before);
            }
        }
    }

    static void check_released_hold_rebaseline() {
        auto session = pause_fixture();
        session->chart_.notes = {gameplay::NoteEvent{1, 48000, 144000}};
        gameplay::GameplayConfig config;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->dispatch_lane_input(1, input::InputState::Pressed, 48000, 48000);
        REQUIRE(session->engine_->stats().counts.pg == 1);
        // An empty polling set deterministically reports every binding released.
        // Keycode 0 is not unpollable: it reaches GetAsyncKeyState(0), so using it
        // here would depend on an undefined virtual key in the desktop session.
        session->polled_gameplay_keys_.clear();
        const auto score_before = session->engine_->stats().raw_score;
        session->rebaseline_gameplay_start_input_state(60000);
        CHECK_FALSE(session->lane_binding_state_.pressed(1));
        CHECK(session->lane_pressed_[0] == 0);
        const auto& events = session->engine_->replay().events;
        REQUIRE(events.size() == 2);
        CHECK(events.back().state == input::InputState::Released);
        CHECK(events.back().sample == 60000);
        CHECK(session->engine_->stats().raw_score == score_before);
        session->engine_->advance(144000);
        CHECK(session->engine_->stats().counts.bd == 1);
    }

    static void check_audio_failure_no_result() {
        auto session = pause_fixture();
        auto driver = std::make_shared<tenriff::tests::MockState>();
        session->audio_thread_.asio_backend_ = std::make_unique<audio::AsioBackend>(
            tenriff::tests::mock_factory(driver));
        audio::AudioConfig config;
        config.backend = audio::AudioBackend::ASIO;
        REQUIRE(session->audio_thread_.asio_backend_->initialize(config,
            [&](float*, uint32_t, int64_t, int64_t) {
                session->engine_->advance(session->chart_.duration_samples);
                session->finished_.store(true);
                // The last chart buffer finishes and receives a driver reset
                // before the menu thread gets another chance to poll the fault.
                driver->callbacks.message(3, 0, nullptr, nullptr);
            }) == audio::AudioResult::Success);
        REQUIRE(session->audio_thread_.start() == audio::AudioResult::Success);
        driver->tick();
        REQUIRE(session->finished_.load());
        REQUIRE(session->audio_thread_.has_runtime_error());
        CHECK(session->audio_error().empty());
        session->shutdown();
        CHECK_FALSE(session->result().has_value);
        CHECK(session->result().replay_path.empty());
        CHECK(session->result().result_path.empty());
        CHECK_FALSE(session->was_user_aborted());
        CHECK_FALSE(session->audio_error().empty());
        CHECK(driver->stops == 1);
        CHECK(driver->releases == 1);
        CHECK(driver->correct_owner);
    }

    static void check_hold_release(bool release_required, int64_t release_sample) {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->chart_.lane_count = 1;
        session->chart_.duration_samples = 144000;
        gameplay::NoteEvent hold{1, 48000, 96000};
        hold.release_required = release_required;
        hold.audio_asset_id = 0;
        session->chart_.notes.push_back(hold);
        gameplay::GameplayConfig config;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->lane_activity_.assign(1, 0);
        session->lane_pressed_.assign(1, 0);
        session->synthetic_tones_enabled_ = false;
        session->chart_audio_assets_.resize(1);
        session->chart_audio_assets_[0].clip.samples = std::make_shared<const std::vector<float>>(960, 0.25f);

        session->dispatch_lane_input(1, input::InputState::Pressed, 48000, 48000);
        REQUIRE(session->chart_audio_voices_.size() == 1);
        std::vector<float> head(960, 0);
        session->mix_chart_audio(head.data(), 480, 48000);
        CHECK(std::any_of(head.begin(), head.end(), [](float x) { return x != 0; }));
        CHECK(session->chart_audio_voices_.empty());

        session->engine_->advance(release_sample - 1);
        session->dispatch_lane_input(1, input::InputState::Released, release_sample, release_sample);
        session->engine_->advance(100800);
        session->schedule_chart_audio(101280);
        std::vector<float> tail(960, 0);
        session->mix_chart_audio(tail.data(), 480, release_sample);
        CHECK(session->chart_audio_voices_.empty());
        CHECK(std::all_of(tail.begin(), tail.end(), [](float x) { return x == 0; }));
    }
};

} // namespace tenriff::app

TEST_CASE("WASAPI sample timestamp pair survives delayed delivery to input judgement and HUD") {
    tenriff::app::GameSessionAudioTestAccess::check_callback_timestamp_pair_delays();
}
TEST_CASE("WASAPI callback timestamp expires on exceptions and direct callbacks use their own QPC") {
    tenriff::app::GameSessionAudioTestAccess::check_callback_timestamp_lifetime_and_fallback();
}
TEST_CASE("ASIO callbacks retain the existing QPC timestamp fallback") {
    tenriff::app::GameSessionAudioTestAccess::check_asio_callback_timestamp_fallback();
}

TEST_CASE("zero input offset keeps the same judgement across audio callback phases") {
    tenriff::app::GameSessionAudioTestAccess::check_input_offset_callback_phase(0);
}
TEST_CASE("positive input offset keeps the same judgement across audio callback phases") {
    tenriff::app::GameSessionAudioTestAccess::check_input_offset_callback_phase(20);
}
TEST_CASE("negative input offset keeps the same judgement across audio callback phases") {
    tenriff::app::GameSessionAudioTestAccess::check_input_offset_callback_phase(-20);
}
TEST_CASE("large input offsets preserve judgement and immediate keysounds across callback phases") {
    tenriff::app::GameSessionAudioTestAccess::check_input_offset_callback_phase(-200);
    tenriff::app::GameSessionAudioTestAccess::check_input_offset_callback_phase(200);
}
TEST_CASE("future clock safety clamps raw input before either sign of calibration") {
    tenriff::app::GameSessionAudioTestAccess::check_input_offset_future_clock_guard();
}
TEST_CASE("input calibration preserves physical edge order and live tone timestamps") {
    tenriff::app::GameSessionAudioTestAccess::check_input_offset_order_and_live_audio();
}
TEST_CASE("live input calibration does not shift R1 R2 R3 R4 replay or autoplay audio") {
    tenriff::app::GameSessionAudioTestAccess::check_input_offset_recorded_audio_unchanged();
}

TEST_CASE("chart loading cancellation does not open an audio device") {
    tenriff::app::GameSessionAudioTestAccess::check_loading_does_not_open_audio(true);
}

TEST_CASE("session initialization restores the menu judge choice but keeps direct replay and peer policies") {
    tenriff::app::GameSessionAudioTestAccess::check_session_judge_handoff();
}

TEST_CASE("chart loading failure does not open an audio device") {
    tenriff::app::GameSessionAudioTestAccess::check_loading_does_not_open_audio(false);
}

TEST_CASE("normalization OFF bypasses prior automatic gain and focus mute restores saved volume") {
    tenriff::app::GameSessionAudioTestAccess::check_normalize_and_focus_output();
}

TEST_CASE("normalization OFF preserves full scale PCM peaks instead of applying hidden compression") {
    tenriff::app::GameSessionAudioTestAccess::check_normalize_off_preserves_full_scale_waveform();
}

TEST_CASE("normalization OFF preserves overlapping BGM and keysound waveform with master headroom") {
    tenriff::app::GameSessionAudioTestAccess::check_normalize_off_mix_retains_master_headroom();
}

TEST_CASE("normalization OFF only clips true post master overflow and rejects nonfinite PCM") {
    tenriff::app::GameSessionAudioTestAccess::check_normalize_off_output_range_boundary();
}

TEST_CASE("production input queue preserves a charge hold through primary to secondary key handoff") {
    tenriff::app::GameSessionAudioTestAccess::check_secondary_binding_input_queue();
}

TEST_CASE("secondary owner release ends the LN even while the original key remains held in both queues and replay") {
    for (const bool future_queue : {false, true})
        for (const bool older_key_held : {false, true})
            for (const bool early_release : {false, true})
                tenriff::app::GameSessionAudioTestAccess::check_secondary_binding_input_queue(
                    32, 33, future_queue, older_key_held, early_release);
}

TEST_CASE("fresh secondary strikes score taps and preserve Empty Poor with no held-key revival in both queues and replay") {
    for (const bool future_queue : {false, true})
        for (const bool extra_empty_press : {false, true})
            tenriff::app::GameSessionAudioTestAccess::check_secondary_binding_tap_queue(
                future_queue, extra_empty_press);
}

TEST_CASE("mapped left and right Shift score note heads and LN releases through both gameplay queues") {
    for (const uint32_t key : {160u, 161u})
        for (const bool hold : {false, true})
            for (const bool future_queue : {false, true})
                tenriff::app::GameSessionAudioTestAccess::check_shift_lane_edges(key, hold, future_queue);
}

TEST_CASE("Shift bindings share secondary LN state without premature release") {
    for (const uint32_t key : {160u, 161u}) {
        tenriff::app::GameSessionAudioTestAccess::check_secondary_binding_input_queue(key, 32);
        tenriff::app::GameSessionAudioTestAccess::check_secondary_binding_input_queue(32, key);
    }
    tenriff::app::GameSessionAudioTestAccess::check_secondary_binding_input_queue(160, 161);
    tenriff::app::GameSessionAudioTestAccess::check_secondary_binding_input_queue(161, 160);
}

#ifdef _WIN32
TEST_CASE("fresh default F1 lowers speed F2 raises speed F3 raises line F4 lowers line") {
    tenriff::app::GameSessionAudioTestAccess::check_default_tuning_directions();
}
#endif

TEST_CASE("F5 F6 and F8 tuning work with or without a held Shift lane during play and pause") {
    for (const uint32_t key : {160u, 161u})
        for (const bool held : {false, true})
            for (const bool paused : {false, true})
                tenriff::app::GameSessionAudioTestAccess::check_shift_tuning_hotkeys(key, held, paused);
}

TEST_CASE("releasing Shift while paused rebaselines the held lane without scoring paused input") {
    for (const uint32_t key : {160u, 161u})
        tenriff::app::GameSessionAudioTestAccess::check_shift_pause_release(key);
}

TEST_CASE("gameplay polls assigned Shift bindings and F11 chat without reserving unbound Shift") {
    tenriff::app::GameSessionAudioTestAccess::check_shift_and_chat_polling();
}

TEST_CASE("gameplay HUD follows the audible device position as queued buffers grow") {
    for (int sample_rate : {44100, 48000})
        tenriff::app::GameSessionAudioTestAccess::check_hud_playback_anchor(sample_rate);
}

TEST_CASE("concurrent gameplay callbacks and HUD readers keep audio position and buffer size coherent") {
    tenriff::app::GameSessionAudioTestAccess::check_concurrent_hud_timing_publication();
}

TEST_CASE("speed tuning mixes a bounded click into real gameplay output and respects master mute") {
    tenriff::app::GameSessionAudioTestAccess::check_speed_click_production_mix();
}

TEST_CASE("long note release never starts another keysound through the production mixer") {
    for (bool release_required : {false, true}) {
        for (int64_t sample : {72000, 96000, 100800}) {
            tenriff::app::GameSessionAudioTestAccess::check_hold_release(release_required, sample);
        }
    }
}

TEST_CASE("completed play exports an eligible record without another key after real audio drains") {
    for (bool delayed_bgm : {false, true})
        tenriff::app::GameSessionAudioTestAccess::check_automatic_result(delayed_bgm, false, false);
}

TEST_CASE("completed silent play exports automatically after its minimum result delay") {
    tenriff::app::GameSessionAudioTestAccess::check_automatic_result(false, false, true);
}

TEST_CASE("guest and revoked online plays export permanently local replay and result origins") {
    for (const bool online_at_start : {false, true}) {
        tenriff::app::GameSessionAudioTestAccess::check_automatic_result(
            false, false, true, false, false, false, false, false,
            32, false, false, online_at_start, true);
    }
}

TEST_CASE("post-note skip still exports without adding an input or aborting the score") {
    tenriff::app::GameSessionAudioTestAccess::check_automatic_result(false, true, false);
}

TEST_CASE("Enter skips a completed listen-to-end outro without pause and preserves eligible replay export") {
    for (const bool future_queue : {false, true}) {
        for (const bool future_bgm : {false, true})
            tenriff::app::GameSessionAudioTestAccess::check_automatic_result(
                future_bgm, true, false, false, false, false, false, false, 13, future_queue);
        tenriff::app::GameSessionAudioTestAccess::check_outro_escape_guards(future_queue);
    }
}

TEST_CASE("failed decoded audio does not keep a finished record waiting for an estimated tail") {
    tenriff::app::GameSessionAudioTestAccess::check_automatic_result(false, false, false, true);
}

TEST_CASE("audio preparation preserves replay chart duration and publishes terminal decode failures") {
    tenriff::app::GameSessionAudioTestAccess::check_audio_preparation();
}

TEST_CASE("reduced keysounds match complete source PCM through real decode scheduling gain and rate mixing") {
    tenriff::app::GameSessionAudioTestAccess::check_reduced_keysound_pcm_render();
}

TEST_CASE("timing audit LN release grade is independent of callback padding and buffer size") {
    tenriff::app::GameSessionAudioTestAccess::check_timing_audit_ln_release_against_playback_head(1973);
}

TEST_CASE("timing audit held-through LN completion stays PG across callback padding and buffer size") {
    tenriff::app::GameSessionAudioTestAccess::check_timing_audit_ln_release_against_playback_head(2030);
}

TEST_CASE("timing audit held-through LN completion stays PG when the release callback is delayed") {
    tenriff::app::GameSessionAudioTestAccess::check_timing_audit_ln_release_against_playback_head(2030, true);
}

TEST_CASE("timing audit LN early release preserves rank and delta at observed shared buffer scale") {
    tenriff::app::GameSessionAudioTestAccess::check_timing_audit_ln_release_at_observed_buffer_scale();
}

TEST_CASE("fixed baseline calibrated LN horizon preserves early drop across positive and negative offsets") {
    tenriff::app::GameSessionAudioTestAccess::check_r5_calibrated_ln_horizon();
}

TEST_CASE("current input and HUD share a monotonic filtered clock through wobble rebase and pause") {
    tenriff::app::GameSessionAudioTestAccess::check_r5_shared_clock_wobble_and_rebase();
}

TEST_CASE("current continuous clock preserves interframe monotonicity and bounded recovery from phase and rate drift") {
    tenriff::app::GameSessionAudioTestAccess::check_r5_continuous_clock_bounds();
}

TEST_CASE("current stale input catch-up exports an unranked custom replay without mislabeling pause") {
    tenriff::app::GameSessionAudioTestAccess::check_automatic_result(false, false, true, false, false, false, false, true);
}

TEST_CASE("BMS new gauge and branch policies survive actual session result export") {
    tenriff::app::GameSessionAudioTestAccess::check_automatic_result(
        false, false, true, false, false, false, false, false, 32, false, true);
}

TEST_CASE("BMS legacy playback result does not invent an optional branch seed") {
    tenriff::app::GameSessionAudioTestAccess::check_legacy_branch_result_seed();
}

TEST_CASE("current scheduled replay autoplay scratch audio keeps write timing while legacy judgement stays historical") {
    tenriff::app::GameSessionAudioTestAccess::check_r5_scheduled_audio_and_historical_frontier();
}

TEST_CASE("duplicate keysounds render once through live autoplay replay BGM and long note production paths") {
    tenriff::app::GameSessionAudioTestAccess::check_duplicate_keysound_pcm_render();
}

TEST_CASE("pause Continue and Esc keep audio and judgement frozen for a full three second countdown") {
    for (bool skip_start : {false, true})
        for (uint32_t key : {13u, 27u})
            tenriff::app::GameSessionAudioTestAccess::check_pause_countdown(key, skip_start);
}

TEST_CASE("instant restart requires active solo released control and never exports the abandoned attempt") {
    tenriff::app::GameSessionAudioTestAccess::check_instant_restart_guards();
}

TEST_CASE("instant restart reaches live and replay input queues without creating lane events") {
    tenriff::app::GameSessionAudioTestAccess::check_instant_restart_input_queues();
}

TEST_CASE("Esc cancels a resume countdown without advancing the chart") {
    tenriff::app::GameSessionAudioTestAccess::check_countdown_cancel();
}

TEST_CASE("pause resume keeps playback anchored notes visible with queued audio for player and ghost") {
    for (bool queued_pause : {false, true})
        tenriff::app::GameSessionAudioTestAccess::check_pause_resume_hud_playback_anchor(queued_pause);
}

TEST_CASE("HUD note scans recover when the playback cursor moves backwards") {
    tenriff::app::GameSessionAudioTestAccess::check_hud_note_scan_after_playback_rewind();
}

TEST_CASE("cancelled starting countdown and failed audio start do not count as a played song") {
    tenriff::app::GameSessionAudioTestAccess::check_gameplay_start_boundary();
}

TEST_CASE("session mix ignores Esc during gameplay and its starting countdown") {
    tenriff::app::GameSessionAudioTestAccess::check_course_escape();
}

TEST_CASE("resume synchronizes a released hold without scoring the paused input") {
    tenriff::app::GameSessionAudioTestAccess::check_released_hold_rebaseline();
}

TEST_CASE("resume keeps secondary ownership and surrendered held keys without fresh score events") {
    tenriff::app::GameSessionAudioTestAccess::check_secondary_owner_rebaseline();
}

TEST_CASE("audio driver failure on the last finished buffer does not export a score or replay") {
    tenriff::app::GameSessionAudioTestAccess::check_audio_failure_no_result();
}

TEST_CASE("automatic outro skip keeps normal score export with active or future audio") {
    for (bool delayed : {false, true})
        for (bool silent : {false, true})
            tenriff::app::GameSessionAudioTestAccess::check_automatic_result(delayed, false, silent, false, true);
}

TEST_CASE("Auto Scratch automates BMS scratch edges only with LN keysound and replay integrity") {
    for (bool manual_key : {false, true})
        tenriff::app::GameSessionAudioTestAccess::check_auto_scratch(manual_key);
}
TEST_CASE("long missed hold cannot pin player and ghost HUD to expired note history") {
    tenriff::app::GameSessionAudioTestAccess::check_long_hold_hud_scan();
}
TEST_CASE("HUD old LN survivors preserve missed and active bodies across rewind and offset changes") {
    tenriff::app::GameSessionAudioTestAccess::check_hud_hold_survivor_lifecycle();
}
TEST_CASE("early released player and ghost holds keep their body while judgement completes immediately") {
    for (const bool release_required : {false, true})
        tenriff::app::GameSessionAudioTestAccess::check_early_released_hold_hud(release_required);
}
TEST_CASE("final released hold delays automatic results until its visible tail but preserves explicit skip") {
    for (const bool play_to_end : {false, true})
        for (const bool manual_skip : {false, true})
            for (const double offset : {0.0, -100.0})
                tenriff::app::GameSessionAudioTestAccess::check_final_released_hold_completion(
                    play_to_end, manual_skip, offset);
}
TEST_CASE("ghost replay backlog preserves player callback order samples keysound and feedback") {
    tenriff::app::GameSessionAudioTestAccess::check_ghost_backlog_player_callback();
}
TEST_CASE("pause mouse commands and explicit latency and assignable coarse speed keys reach session") {
    tenriff::app::GameSessionAudioTestAccess::check_pause_mouse_and_hotkeys();
}
TEST_CASE("live pacemaker keeps ordinary completed record and community submission eligibility") {
    tenriff::app::GameSessionAudioTestAccess::check_automatic_result(false, false, true, false, false, true);
}

TEST_CASE("Auto Scratch exports explicit assisted custom zero scores rejected by community ranking") {
    tenriff::app::GameSessionAudioTestAccess::check_automatic_result(false, false, true, false, false, false, true);
}

TEST_CASE("fixed baseline live player and ghost callbacks share edges while R3 R7 R8 ghosts retain old timing") {
    tenriff::app::GameSessionAudioTestAccess::check_fixed_baseline_player_and_ghost_boundaries();
}

TEST_CASE("whole chart riff map crosses the production HUD snapshot with unique song revisions") {
    tenriff::app::GameSessionAudioTestAccess::check_riff_map_snapshot_lifecycle();
}

TEST_CASE("BMS chart volume controls BGM and keysounds independently of menu music") {
    tenriff::app::GameSessionAudioTestAccess::check_independent_chart_volume();
}

TEST_CASE("decoded chart PCM survives runtime maintenance and releases at session stop") {
    tenriff::app::GameSessionAudioTestAccess::check_pcm_retention_until_stop();
}

TEST_CASE("accelerated gameplay tuning keeps precise steps and cancels on release focus resume and overlay") {
    tenriff::app::GameSessionAudioTestAccess::check_accelerated_tuning();
}

TEST_CASE("key overlay observes accepted gameplay edges without changing replay or counting controls autoplay replay") {
    tenriff::app::GameSessionAudioTestAccess::check_key_overlay_routing();
}

TEST_CASE("key overlay session HUD keeps the panel and clears live KPS during pause and autoplay") {
    tenriff::app::GameSessionAudioTestAccess::check_key_overlay_hud();
}

TEST_CASE("full combo HUD waits for the last LN tail and excludes partial assisted aborted play") {
    tenriff::app::GameSessionAudioTestAccess::check_full_combo_hud();
}
