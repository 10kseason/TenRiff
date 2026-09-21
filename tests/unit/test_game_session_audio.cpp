#include "doctest/doctest.h"
#include "app/GameSession.h"
#include "app/SitesLeaderboardClient.h"
#include "../support/MockAsioDriver.h"

#include <algorithm>
#include <memory>
#include <filesystem>
#include <fstream>
#include <chrono>

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
                                       bool failed_audio = false) {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->chart_.lane_count = 4;
        // Trailing chart measures and startup decode estimates must not keep a
        // completed score waiting after all audible voices have drained.
        session->chart_.duration_samples = 960000;
        session->chart_.notes.push_back(gameplay::NoteEvent{1, 12000, std::nullopt});
        gameplay::GameplayConfig config;
        config.sample_rate = 48000;
        config.gauge_shift_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->gauge_shift_enabled_ = true;
        session->gameplay_started_ = true;
        session->config_.ui.result_tail_ms = 100;
        session->config_.mode.key_mode = "4k";
        session->chart_format_ = ChartFormat::Bms;
        session->key_to_lane_[32] = 1;
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
        session->dispatch_lane_input(1, input::InputState::Pressed, 12000, 12000);
        session->dispatch_lane_input(1, input::InputState::Released, 12001, 12000);
        std::vector<float> audio(960);
        const int64_t expected_end = (silent || failed_audio) ? 17280 : delayed_bgm ? 96000 : 48000;
        // Model two queued device buffers. No input is injected after the last
        // note unless this is the explicit skip compatibility case.
        for (int64_t sample = 12000; sample <= expected_end + 960; sample += 480) {
            if (manual_skip && sample == 14400) {
                input::InputEvent skip{};
                skip.keycode = 32;
                skip.state = input::InputState::Pressed;
                REQUIRE(session->input_thread_.queue().push(skip));
            }
            session->audio_callback(audio.data(), 480, sample, sample - 960);
            if (manual_skip && sample >= 14400) break;
            if (sample - 960 < expected_end) CHECK_FALSE(session->finished_.load());
        }
        REQUIRE(session->finished_.load());
        CHECK_FALSE(session->user_aborted_.load());
        CHECK(session->engine_->stats().counts.pg == 1);
        CHECK(session->engine_->replay().events.size() == 2);

        CompletionTestDirectory directory;
        const auto chart_path = directory.path / "fixture.bms";
        { std::ofstream file(chart_path); file << "#TITLE Completion fixture\n#BPM 120\n"; }
        session->chart_path_ = chart_path.u8string();
        session->profile_dir_ = directory.path.u8string();
        session->shutdown();
        const auto& result = session->result();
        REQUIRE(result.has_value);
        CHECK(result.finished);
        REQUIRE_FALSE(result.replay_path.empty());
        REQUIRE_FALSE(result.result_path.empty());
        CHECK(std::filesystem::u8path(result.replay_path).parent_path() == directory.path / "replays");
        CHECK(std::filesystem::u8path(result.result_path).parent_path() == directory.path / "results");
        CHECK(std::filesystem::is_regular_file(std::filesystem::u8path(result.replay_path)));
        CHECK(std::filesystem::is_regular_file(std::filesystem::u8path(result.result_path)));
        REQUIRE(result.replay_sha256.size() == 64);
        const auto replay = gameplay::load_replay_json(result.replay_path);
        REQUIRE(replay.success());
        std::string payload, error;
        CHECK(build_sites_score_json(*replay.replay, result.replay_sha256,
                                    "Completion fixture", result.clear_status, payload, error));
        CHECK_FALSE(payload.empty());
        CHECK(error.empty());
        CHECK(result.export_warnings.empty());
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

TEST_CASE("completed play exports an eligible record without another key after real audio drains") {
    for (bool delayed_bgm : {false, true})
        tenriff::app::GameSessionAudioTestAccess::check_automatic_result(delayed_bgm, false, false);
}

TEST_CASE("completed silent play exports automatically after its minimum result delay") {
    tenriff::app::GameSessionAudioTestAccess::check_automatic_result(false, false, true);
}

TEST_CASE("post-note skip still exports without adding an input or aborting the score") {
    tenriff::app::GameSessionAudioTestAccess::check_automatic_result(false, true, false);
}

TEST_CASE("failed decoded audio does not keep a finished record waiting for an estimated tail") {
    tenriff::app::GameSessionAudioTestAccess::check_automatic_result(false, false, false, true);
}

TEST_CASE("audio preparation preserves replay chart duration and publishes terminal decode failures") {
    tenriff::app::GameSessionAudioTestAccess::check_audio_preparation();
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
