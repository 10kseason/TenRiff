// Opt-in measurement of the audio-callback / HUD-thread lock on a real
// GameSession: real audio device (silent), real HUD loop, synthetic autoplay
// chart inside a new sandbox folder. Never part of CTest or packages.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "app/GameSession.h"

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace tenriff;

void fixture(const std::filesystem::path& dir, unsigned bars, unsigned division, unsigned bpm) {
    std::filesystem::create_directories(dir);
    std::ofstream wav(dir / "silence.wav", std::ios::binary);
    const auto word = [&](uint32_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) wav.put(static_cast<char>(value >> (8 * i)));
    };
    wav.write("RIFF", 4); word(996, 4); wav.write("WAVEfmt ", 8);
    word(16, 4); word(1, 2); word(1, 2); word(48000, 4); word(96000, 4);
    word(2, 2); word(16, 2); wav.write("data", 4); word(960, 4);
    for (unsigned i = 0; i < 960; ++i) wav.put(0);
    if (!wav) throw std::runtime_error("Could not write silent fixture");
    std::ofstream bms(dir / "probe.bms");
    bms << "#PLAYER 1\n#TITLE RT lock probe\n#BPM " << bpm << "\n#RANK 3\n#WAV01 silence.wav\n";
    static const char* const kLanes[] = {"11", "12", "13", "14", "15", "18", "19"};
    for (unsigned bar = 1; bar <= bars; ++bar) {
        for (const char* lane : kLanes) {
            bms << '#' << char('0' + bar / 100) << char('0' + bar / 10 % 10) << char('0' + bar % 10)
                << lane << ':';
            for (unsigned note = 0; note < division; ++note) bms << "01";
            bms << '\n';
        }
    }
    if (!bms) throw std::runtime_error("Could not write chart fixture");
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) {
        std::cerr << "Usage: rt_lock_probe NEW_SANDBOX shared|exclusive [bars=180] [division=16] "
                     "[bpm=240] [load_threads=0]\n";
        return 2;
    }
    const std::wstring mode = argv[2];
    const unsigned bars = argc > 3 ? static_cast<unsigned>(_wtoi(argv[3])) : 180;
    const unsigned division = argc > 4 ? static_cast<unsigned>(_wtoi(argv[4])) : 16;
    const unsigned bpm = argc > 5 ? static_cast<unsigned>(_wtoi(argv[5])) : 240;
    const unsigned load_threads = argc > 6 ? static_cast<unsigned>(_wtoi(argv[6])) : 0;
    if (bars == 0 || bars > 999 || division == 0 || bpm == 0) return 2;

    const auto original = std::filesystem::current_path();
    const auto root = std::filesystem::absolute(argv[1]).lexically_normal();
    if (!std::filesystem::create_directory(root)) {
        std::cerr << "Sandbox must be a new folder\n";
        return 2;
    }
    int exit_code = 0;
    std::atomic<bool> stop_load{false};
    std::vector<std::thread> load;
    try {
        std::filesystem::current_path(root);
        fixture("songs/probe", bars, division, bpm);
        auto settings = config::ConfigLoader{}.defaults();
        settings.audio.backend = audio::AudioBackend::WASAPI;
        settings.audio.exclusive_mode = mode == L"exclusive";
        settings.audio_ui.master_volume = 0.0;
        settings.mode.key_mode = "native";
        settings.mode.random = "off";
        settings.mode.mods.clear();
        settings.mode.autoplay_enabled = true;
        settings.ui.skip_start_countdown = true;
        if (!config::ConfigLoader{}.save_profile("profiles/probe", settings))
            throw std::runtime_error("Could not save synthetic profile");

        app::CommandLineOptions options;
        options.profile = "probe";
        options.chart_path = "songs/probe/probe.bms";
        auto session = std::make_unique<app::GameSession>();
        uint64_t hud_frames = 0;
        std::size_t max_notes = 0;
        // The menu copies each snapshot for the renderer; keep that cost here.
        auto latest = std::make_unique<app::GameSession::HudSnapshot>();
        session->set_hud_callback([&](const app::GameSession::HudSnapshot& hud) {
            *latest = hud;
            ++hud_frames;
            if (hud.note_count > max_notes) max_notes = hud.note_count;
        });
        if (!session->initialize(options)) throw std::runtime_error("GameSession initialize failed");
        for (unsigned i = 0; i < load_threads; ++i) {
            load.emplace_back([&] {
                volatile uint64_t sink = 0;
                while (!stop_load.load(std::memory_order_relaxed)) sink = sink + 1;
            });
        }
        session->run();
        stop_load.store(true);
        for (auto& thread : load) thread.join();
        load.clear();
        session->shutdown();
        const auto& result = session->result();
        std::ostringstream report;
        report << "{\"mode\":\"" << (settings.audio.exclusive_mode ? "exclusive" : "shared")
               << "\",\"bars\":" << bars << ",\"division\":" << division << ",\"bpm\":" << bpm
               << ",\"load_threads\":" << load_threads
               << ",\"hardware_threads\":" << std::thread::hardware_concurrency()
               << ",\"audio_error\":\"" << (session->audio_error().empty() ? "" : "yes")
               << "\",\"finished\":" << (result.finished ? "true" : "false")
               << ",\"total_notes\":" << result.stats.total_notes
               << ",\"hud_frames\":" << hud_frames << ",\"max_hud_notes\":" << max_notes
               << ",\"probe\":";
        session->rt_lock_probe().write(report);
        report << "}";
        std::cout << report.str() << std::endl;
        std::filesystem::current_path(original);
        std::ofstream(root / "report.json") << report.str() << '\n';
        if (!session->audio_error().empty()) {
            std::cerr << session->audio_error() << '\n';
            exit_code = 1;
        }
    } catch (const std::exception& error) {
        stop_load.store(true);
        for (auto& thread : load) thread.join();
        std::filesystem::current_path(original);
        std::cerr << error.what() << '\n';
        exit_code = 1;
    }
    return exit_code;
}
