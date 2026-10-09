#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "app/GameSession.h"
#include "app/ChartLoader.h"
#include "config/SimpleJson.h"
#include "timing/HighResClock.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace tenriff::app {
struct GameSessionAudioTestAccess {
    static uint32_t device_rate(const GameSession& session) {
        return session.audio_thread_.sample_rate();
    }
    static bool timeline_matches(const GameSession& session, const std::string& chart_path) {
        const auto reference = ChartLoader{}.load(chart_path, session.sample_rate_, 1.0);
        if (!reference.success() || reference.chart.notes.empty() ||
            reference.chart.notes.size() != session.chart_.notes.size()) return false;
        // Runtime deliberately adds a fixed lead-in. Compare every relative
        // timestamp and the duration after removing that single translation.
        const auto lead_in = session.chart_.notes.front().start_sample - reference.chart.notes.front().start_sample;
        if (reference.chart.duration_samples + lead_in != session.chart_.duration_samples) return false;
        for (std::size_t i = 0; i < reference.chart.notes.size(); ++i) {
            if (reference.chart.notes[i].start_sample + lead_in != session.chart_.notes[i].start_sample ||
                reference.chart.notes[i].end_sample != session.chart_.notes[i].end_sample) return false;
        }
        return true;
    }
};
}

namespace {
using namespace tenriff;
using namespace std::chrono_literals;
using config::JsonArray;
using config::JsonObject;
using config::JsonValue;

void fixture(const std::filesystem::path& dir, unsigned rate) {
    std::filesystem::create_directories(dir);
    std::ofstream wav(dir / "silence.wav", std::ios::binary);
    const auto word = [&](uint32_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) wav.put(static_cast<char>(value >> (8 * i)));
    };
    wav.write("RIFF", 4); word(996, 4); wav.write("WAVEfmt ", 8);
    word(16, 4); word(1, 2); word(1, 2); word(rate, 4); word(rate * 2, 4);
    word(2, 2); word(16, 2); wav.write("data", 4); word(960, 4);
    for (unsigned i = 0; i < 960; ++i) wav.put(0);
    if (!wav) throw std::runtime_error("Could not write silent fixture");
    std::ofstream bms(dir / "load.bms");
    bms << "#PLAYER 1\n#TITLE Loading audio fixture\n#BPM 137\n#4K\n#WAV01 silence.wav\n";
    // Enough scheduled events to exercise parsing and sample-domain rebuilding.
    for (unsigned bar = 0; bar < 300; ++bar) {
        for (unsigned lane = 1; lane <= 4; ++lane) {
            bms << '#' << char('0' + bar / 100) << char('0' + bar / 10 % 10)
                << char('0' + bar % 10) << '1' << lane << ':';
            for (unsigned note = 0; note < 32; ++note) bms << "01";
            bms << '\n';
        }
    }
    if (!bms) throw std::runtime_error("Could not write chart fixture");
}
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) { std::cerr << "Usage: loading_audio_smoke NEW_SANDBOX\n"; return 2; }
    const auto original = std::filesystem::current_path();
    const auto root = std::filesystem::absolute(argv[1]).lexically_normal();
    if (!std::filesystem::create_directory(root)) return 2;
    JsonArray checks, runs;
    unsigned failed = 0;
    const auto check = [&](const std::string& name, bool pass) {
        checks.emplace_back(JsonObject{{"name", JsonValue(name)}, {"passed", JsonValue(pass)}});
        failed += !pass;
        std::cout << (pass ? "PASS " : "FAIL ") << name << '\n';
    };
    try {
        std::filesystem::current_path(root);
        fixture("songs/44100", 44100);
        fixture("songs/48000", 48000);
        auto settings = config::ConfigLoader{}.defaults();
        settings.audio.backend = audio::AudioBackend::WASAPI;
        settings.audio.exclusive_mode = false;
        settings.audio.sample_rate = 44100;
        settings.audio_ui.master_volume = 0.0;
        settings.mode.key_mode = "auto";
        settings.mode.random = "off";
        if (!config::ConfigLoader{}.save_profile("profiles/probe", settings))
            throw std::runtime_error("Could not save synthetic profile");

        std::atomic<uint64_t> callbacks{0};
        std::atomic<int64_t> last{0}, max_gap{0};
        std::atomic<uint64_t> valid_timestamp_pairs{0}, invalid_timestamp_pairs{0};
        std::atomic<int64_t> max_timestamp_pair_age_ns{0};
        audio::AudioThread companion;
        auto companion_config = settings.audio;
        companion_config.sample_rate = 48000;
        const auto opened = companion.initialize(companion_config,
            [&](float* out, uint32_t frames, int64_t, int64_t) {
                std::fill_n(out, frames * 2, 0.0f);
                const auto now = timing::HighResClock::now_ns();
                const auto paired = companion.callback_playback_time_ns();
                if (paired > 0 && paired <= now) {
                    ++valid_timestamp_pairs;
                    const auto age = now - paired;
                    auto peak = max_timestamp_pair_age_ns.load();
                    while (age > peak &&
                           !max_timestamp_pair_age_ns.compare_exchange_weak(peak, age)) {}
                } else {
                    ++invalid_timestamp_pairs;
                }
                const auto previous = last.exchange(now);
                if (previous != 0) {
                    const auto gap = now - previous;
                    auto peak = max_gap.load();
                    while (gap > peak && !max_gap.compare_exchange_weak(peak, gap)) {}
                }
                ++callbacks;
            });
        if (opened != audio::AudioResult::Success || companion.start() != audio::AudioResult::Success)
            throw std::runtime_error("Shared companion audio unavailable");
        std::this_thread::sleep_for(300ms);
        check("companion.started", callbacks.load() > 0);
        for (int pass = 0; pass < 2; ++pass) for (const unsigned rate : {44100u, 48000u}) {
            max_gap.store(0);
            app::CommandLineOptions options;
            options.profile = "probe";
            options.chart_path = "songs/" + std::to_string(rate) + "/load.bms";
            auto session = std::make_unique<app::GameSession>();
            bool parse_seen = false, device_closed_while_parsing = true, companion_advanced = true;
            session->set_loading_progress_callback([&](const app::GameSession::LoadingProgress& progress) {
                if (progress.stage == "Parsing chart" || progress.stage == "Matching chart sample rate") {
                    parse_seen = true;
                    device_closed_while_parsing &= app::GameSessionAudioTestAccess::device_rate(*session) == 0;
                    const auto before = callbacks.load();
                    // Extend both pre-open phases to observe the other stream.
                    // This is a continuity probe, not a load-time benchmark.
                    std::this_thread::sleep_for(200ms);
                    companion_advanced &= callbacks.load() > before;
                }
            });
            const bool initialized = session->initialize(options);
            const auto label = std::to_string(pass) + "." + std::to_string(rate);
            check(label + ".initialized", initialized);
            check(label + ".no_device_during_parse", parse_seen && device_closed_while_parsing);
            check(label + ".companion_advanced", companion_advanced);
            check(label + ".rate_and_timeline", initialized &&
                app::GameSessionAudioTestAccess::device_rate(*session) == rate &&
                app::GameSessionAudioTestAccess::timeline_matches(*session, options.chart_path));
            session->shutdown();
            const auto before = callbacks.load();
            std::this_thread::sleep_for(200ms);
            check(label + ".companion_after_shutdown", callbacks.load() > before);
            runs.emplace_back(JsonObject{{"case", JsonValue(label)},
                {"max_callback_gap_ms", JsonValue(double(max_gap.load()) / 1e6)}});
        }
        companion.shutdown();
        check("companion.callback_timestamp_pairs",
            valid_timestamp_pairs.load() == callbacks.load() && callbacks.load() > 0 &&
            invalid_timestamp_pairs.load() == 0);
        check("companion.callback_timestamp_cleared", companion.callback_playback_time_ns() == 0);
        runs.emplace_back(JsonObject{{"case", JsonValue("callback_timestamp_pairs")},
            {"valid_pairs", JsonValue(double(valid_timestamp_pairs.load()))},
            {"invalid_pairs", JsonValue(double(invalid_timestamp_pairs.load()))},
            {"max_pair_age_ms", JsonValue(double(max_timestamp_pair_age_ns.load()) / 1e6)}});
    } catch (const std::exception& error) {
        check("exception", false); std::cerr << error.what() << '\n';
    }
    std::filesystem::current_path(original);
    std::ofstream(root / "report.json") << config::json_stringify(JsonValue(JsonObject{
        {"failed", JsonValue(double(failed))}, {"checks", JsonValue(std::move(checks))},
        {"runs", JsonValue(std::move(runs))},
        {"scope", JsonValue("Synthetic 38400-note charts, real WASAPI shared clients with silent output. No microphone capture, Discord, physical audibility or remote-PC proof. Observation delays are excluded from load-time claims.")}}), 2);
    return failed ? 1 : 0;
}
