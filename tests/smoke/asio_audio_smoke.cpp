#include "audio/AsioBackend.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    using namespace tenriff::audio;
    std::string driver;
    int seconds = 2;
    uint32_t requested_rate = 44100, requested_frames = 128;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--list") continue;
            if (i + 1 >= argc) return 2;
            if (argument == "--driver") driver = argv[++i];
            else if (argument == "--seconds") seconds = std::stoi(argv[++i]);
            else if (argument == "--rate") requested_rate = static_cast<uint32_t>(std::stoul(argv[++i]));
            else if (argument == "--frames") requested_frames = static_cast<uint32_t>(std::stoul(argv[++i]));
            else return 2;
        }
    } catch (...) { return 2; }
    if (seconds < 1 || seconds > 10) return 2;
    if (driver.empty()) {
        const auto drivers = AsioBackend::enumerate_drivers();
        for (const auto& item : drivers) std::cout << item.id << " " << item.name << '\n';
        std::cout << "x64_drivers=" << drivers.size() << '\n';
        return 0;
    }
    AsioBackend backend;
    AudioConfig config; config.backend = AudioBackend::ASIO; config.asio_driver = driver;
    config.sample_rate = requested_rate; config.frames_per_buffer = requested_frames;
    std::atomic<uint64_t> callbacks{0};
    std::atomic<bool> monotonic{true};
    int64_t last_write = -1, last_playback = -1;
    const auto result = backend.initialize(config, [&](float*, uint32_t, int64_t write, int64_t playback) {
        // Backend pre-zeroes the mix buffer. This smoke never emits sound.
        if (write < last_write || playback < last_playback || playback > write) monotonic = false;
        last_write = write; last_playback = playback; ++callbacks;
    });
    if (result != AudioResult::Success) { std::cerr << backend.error_message() << '\n'; return 1; }
    std::cout << "rate=" << backend.sample_rate() << " frames=" << backend.buffer_frames() << '\n';
    for (int run = 0; run < 2; ++run) {
        last_write = last_playback = -1; callbacks = 0; monotonic = true;
        if (backend.start() != AudioResult::Success) { std::cerr << backend.error_message() << '\n'; return 1; }
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        backend.stop();
        std::cout << "run=" << run + 1 << " callbacks=" << callbacks << " monotonic=" << monotonic << " playback=" << backend.playback_samples() << '\n';
        if (backend.has_runtime_error() || callbacks == 0 || !monotonic) { std::cerr << backend.error_message() << '\n'; return 1; }
    }
    backend.shutdown();
    std::cout << "ASIO_SILENT_SMOKE_PASS\n";
    return 0;
}
