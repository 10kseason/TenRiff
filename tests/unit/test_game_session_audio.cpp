#include "doctest/doctest.h"
#include "app/GameSession.h"
#include "app/SitesLeaderboardClient.h"
#include "../support/MockAsioDriver.h"

#include <algorithm>
#include <memory>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <cmath>
#include <limits>

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
    static void check_concurrent_hud_timing_publication() {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
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
        std::thread producer([&] {
            std::vector<float> output(1920);
            while (!begin.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int64_t iteration = 1; iteration <= 500; ++iteration) {
                const uint32_t frames = iteration % 2 == 0 ? 480 : 960;
                const int64_t playback = iteration * 4800;
                session->audio_callback(output.data(), frames, playback + 2400, playback);
                if (iteration == 1)
                    while (!observed.load(std::memory_order_acquire)) std::this_thread::yield();
                if (iteration % 16 == 0) std::this_thread::yield();
            }
            done.store(true, std::memory_order_release);
        });
        begin.store(true, std::memory_order_release);
        std::size_t published_snapshots = 0;
        do {
            const auto hud = session->hud_snapshot();
            if (hud.current_sample > 0) {
                ++published_snapshots;
                observed.store(true, std::memory_order_release);
                CHECK(hud.current_sample % 4800 == 0);
                const auto iteration = hud.current_sample / 4800;
                CHECK(hud.audio_buffer_frames == (iteration % 2 == 0 ? 480u : 960u));
                CHECK(hud.audio_sample_time_ns > 0);
            }
        } while (!done.load(std::memory_order_acquire));
        producer.join();
        const auto final = session->hud_snapshot();
        CHECK(final.current_sample == 500 * 4800);
        CHECK(final.audio_buffer_frames == 480);
        CHECK(published_snapshots > 0);
    }

    static void check_secondary_binding_input_queue() {
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
        session->key_to_lane_ = {{32, 1}, {33, 1}};
        session->lane_binding_state_.configure(session->key_to_lane_);
        const auto physical_event = [&](uint32_t key, input::InputState state, int64_t sample) {
            const int64_t time_ns = 1'000'000'000LL + (sample - 48000) * 1'000'000'000LL / 48000;
            session->startup_input_timing_anchor_ = {sample, time_ns, true};
            session->current_playback_sample_ = sample;
            input::InputEvent event{};
            event.keycode = key; event.state = state; event.input_time_ns = time_ns;
            REQUIRE(session->input_thread_.queue().push(event));
            session->process_input_queue(sample, sample + 480, 0);
            session->engine_->advance(sample);
        };
        physical_event(32, input::InputState::Pressed, 48000);
        physical_event(33, input::InputState::Pressed, 60000);
        physical_event(32, input::InputState::Released, 72000);
        CHECK(session->lane_pressed_[0] == 1);
        CHECK(session->lane_binding_state_.pressed(1));
        CHECK(session->engine_->stats().counts.bd == 0);
        CHECK(session->engine_->stats().counts.pr == 0);
        physical_event(33, input::InputState::Released, 96000);
        session->engine_->advance(100800);
        CHECK(session->lane_pressed_[0] == 0);
        CHECK_FALSE(session->lane_binding_state_.pressed(1));
        CHECK(session->engine_->stats().counts.pg == 2);
        CHECK(session->engine_->stats().counts.bd == 0);
        CHECK(session->engine_->stats().counts.pr == 0);
        CHECK(session->engine_->replay().events.size() == 2);
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
        // Increasing padding resembles extra queued audio under a screen-share
        // workload. Neither its size nor the writable buffer may lead visuals.
        for (const Buffer buffer : {Buffer{4800, 2400, 480}, Buffer{24000, 3000, 960},
                                    Buffer{24960, 4800, 1440}}) {
            std::vector<float> output(buffer.frames * 2);
            session->audio_callback(output.data(), buffer.frames, buffer.write, buffer.playback);
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

    static void check_normalize_off_output_range_boundary() {
        auto session = std::make_unique<GameSession>();
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
                                       bool failed_audio = false, bool skip_outro = false) {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->chart_.lane_count = 4;
        session->chart_.bms_rank = 3; // BMS loader resolves absent #RANK to EASY.
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
        session->config_.audio_ui.play_to_end = !skip_outro;
        session->config_.mode.key_mode = "4k";
        session->chart_format_ = ChartFormat::Bms;
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
        session->dispatch_lane_input(1, input::InputState::Pressed, 12000, 12000);
        session->dispatch_lane_input(1, input::InputState::Released, 12001, 12000);
        std::vector<float> audio(960);
        const int64_t expected_end = (skip_outro || silent || failed_audio) ? 17280 : delayed_bgm ? 96000 : 48000;
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
        session->audio_callback(audio.data(), 480, 47520, 35520);
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
        session->audio_callback(audio.data(), 480, 48000, 36000);
        const auto paused = session->hud_snapshot();
        CHECK(paused.paused);
        CHECK(paused.current_sample == 36000);
        CHECK(hud_contains_note(paused, 37500));
        CHECK(hud_contains_note(paused, 37500, true));
        CHECK(hud_contains_note(paused, 42000));

        control(*session, 13);
        for (const int64_t write : {48480LL, 96480LL, 144480LL, 192480LL}) {
            session->audio_callback(audio.data(), 480, write, write - 12000);
            const auto countdown = session->hud_snapshot();
            CHECK(countdown.current_sample == 36000);
            CHECK(hud_contains_note(countdown, 37500));
            CHECK(hud_contains_note(countdown, 37500, true));
        }
        REQUIRE_FALSE(session->paused_.load());
        session->audio_callback(audio.data(), 480, 192960, 180960);
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
        session->lane_binding_state_.configure(session->key_to_lane_);
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

TEST_CASE("audio driver failure on the last finished buffer does not export a score or replay") {
    tenriff::app::GameSessionAudioTestAccess::check_audio_failure_no_result();
}

TEST_CASE("automatic outro skip keeps normal score export with active or future audio") {
    for (bool delayed : {false, true})
        for (bool silent : {false, true})
            tenriff::app::GameSessionAudioTestAccess::check_automatic_result(delayed, false, silent, false, true);
}
