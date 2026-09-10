#include "doctest/doctest.h"
#include "app/GameSession.h"
#include "../support/MockAsioDriver.h"

#include <algorithm>
#include <memory>

namespace tenriff::app {

// Exercise the production input-to-voice-to-mixer path without starting a device,
// input thread, decoder, profile or record writer.
struct GameSessionAudioTestAccess {
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

    static void paused_buffer(GameSession& session, int64_t sample, int count) {
        std::vector<float> audio(960, 1.0f);
        session.audio_callback(audio.data(), 480, sample, sample);
        CHECK(std::all_of(audio.begin(), audio.end(), [](float x) { return x == 0.0f; }));
        CHECK(session.last_audio_sample_.load() == 48000);
        CHECK(session.resume_countdown_value_.load() == count);
        CHECK(session.engine_->stats().raw_score == 0);
    }

    static void check_pause_countdown(uint32_t resume_key) {
        auto session = pause_fixture();
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
        session->audio_callback(audio.data(), 480, 192960, 192960);
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

    static void check_released_hold_rebaseline() {
        auto session = pause_fixture();
        session->chart_.notes = {gameplay::NoteEvent{1, 48000, 144000}};
        gameplay::GameplayConfig config;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->dispatch_lane_input(1, input::InputState::Pressed, 48000, 48000);
        // An unpollable key provides a deterministic physical-up state without
        // reading or synthesizing a key on the user's real keyboard.
        session->key_to_lane_[0] = 1;
        session->polled_gameplay_keys_.push_back({0});
        const auto score_before = session->engine_->stats().raw_score;
        session->rebaseline_gameplay_start_input_state(60000);
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
        session->last_keysound_chart_samples_.assign(1, -1);

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

TEST_CASE("long note release never starts another keysound through the production mixer") {
    for (bool release_required : {false, true}) {
        for (int64_t sample : {72000, 96000, 100800}) {
            tenriff::app::GameSessionAudioTestAccess::check_hold_release(release_required, sample);
        }
    }
}

TEST_CASE("pause Continue and Esc keep audio and judgement frozen for a full three second countdown") {
    for (uint32_t key : {13u, 27u}) tenriff::app::GameSessionAudioTestAccess::check_pause_countdown(key);
}

TEST_CASE("Esc cancels a resume countdown without advancing the chart") {
    tenriff::app::GameSessionAudioTestAccess::check_countdown_cancel();
}

TEST_CASE("session mix ignores Esc during gameplay and its starting countdown") {
    tenriff::app::GameSessionAudioTestAccess::check_course_escape();
}

TEST_CASE("resume synchronizes a released hold without scoring the paused input") {
    tenriff::app::GameSessionAudioTestAccess::check_released_hold_rebaseline();
}

TEST_CASE("audio driver failure on the last finished buffer does not export a score or replay") {
    tenriff::app::GameSessionAudioTestAccess::check_audio_failure_no_result();
}
