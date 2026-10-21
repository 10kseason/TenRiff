#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
#include <string_view>
#include <algorithm>
#include <cmath>

#include "render/BgaVideoDecoder.h"
#include "render/BgaVideoFfmpeg.h"
#include "util/Utf8Compat.h"

namespace {

std::shared_ptr<const tenriff::render::BgaVideoFrame> wait_for_frame(
    tenriff::render::BgaVideoDecoder& decoder,
    std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto frame = decoder.take_ready()) {
            return frame;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return {};
}

bool valid_frame(const std::shared_ptr<const tenriff::render::BgaVideoFrame>& frame) {
    return frame && frame->width > 0 && frame->height > 0 &&
           frame->bgra.size() == static_cast<std::size_t>(frame->width) * frame->height * 4;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2 || argc > 3) {
        std::wcerr << L"usage: bga_video_smoke <video> [--timeline|--ffmpeg]\n";
        return 2;
    }
    const std::filesystem::path path(argv[1]);
    if (!std::filesystem::is_regular_file(path)) {
        std::wcerr << L"video not found: " << path.c_str() << L'\n';
        return 2;
    }

    tenriff::render::BgaVideoDecoder decoder;
    const std::string path_utf8 = tenriff::util::path_to_utf8_lossy(path);
    if (argc == 3 && std::wstring_view(argv[2]) == L"--ffmpeg") {
        tenriff::render::BgaVideoFfmpegReader reader;
        if (!reader.open(path_utf8)) return 6;
        const auto a = reader.read_at(0);
        const auto same = reader.read_at(0);
        // Drain the short fixture before EOF instead of seeking beyond its end.
        const auto end = reader.read_at(30'000'000);
        const auto held = reader.read_at(40'000'000);
        const auto restarted = reader.read_at(0);
        const bool ok = valid_frame(a) && same == a && valid_frame(end) &&
            valid_frame(held) && valid_frame(restarted) &&
            restarted->timestamp_100ns == a->timestamp_100ns;
        std::cout << "{\"ffmpeg_repeat_seek_passed\":" << (ok ? "true" : "false") << "}\n";
        return ok ? 0 : 6;
    }
    decoder.request(path_utf8, 0.0);
    const auto first = wait_for_frame(decoder, std::chrono::seconds(10));
    if (!valid_frame(first)) {
        std::cerr << "Media Foundation did not produce the first BGA video frame\n";
        return 3;
    }

    decoder.request(path_utf8, 0.25);
    const auto later = wait_for_frame(decoder, std::chrono::seconds(10));
    if (!valid_frame(later) || later->timestamp_100ns < first->timestamp_100ns) {
        std::cerr << "Media Foundation did not advance the BGA video frame\n";
        return 4;
    }
    if (argc == 3 && std::wstring_view(argv[2]) == L"--timeline") {
        auto previous = later->timestamp_100ns;
        int regressions = 0, samples = 0, excessive_lead = 0, excessive_relative_lead = 0;
        std::int64_t max_lead = 0, max_relative_lead = 0, max_relative_lag = 0;
        for (int i = 9; i <= 90; ++i) {
            const double position = static_cast<double>(i) / 30.0;
            decoder.request(path_utf8, position);
            const auto frame = wait_for_frame(decoder, std::chrono::seconds(3));
            if (!valid_frame(frame)) {
                std::cout << "{\"missing_frame_at_seconds\":" << position
                          << ",\"samples\":" << samples
                          << ",\"timestamp_regressions\":" << regressions
                          << ",\"max_lead_100ns\":" << max_lead << "}\n";
                return 7;
            }
            ++samples;
            if (frame->timestamp_100ns < previous) ++regressions;
            previous = frame->timestamp_100ns;
            const auto lead = frame->timestamp_100ns - static_cast<std::int64_t>(position * 10'000'000);
            max_lead = std::max(max_lead, lead);
            if (lead > 2'000'000) ++excessive_lead;
            const auto relative_lead = lead - first->timestamp_100ns;
            max_relative_lead = std::max(max_relative_lead, relative_lead);
            max_relative_lag = std::max(max_relative_lag, -relative_lead);
            if (relative_lead > 2'000'000) ++excessive_relative_lead;
        }
        decoder.clear();
        decoder.request(path_utf8, 0.0);
        const auto restarted = wait_for_frame(decoder, std::chrono::seconds(3));
        const bool restart_ok = valid_frame(restarted) &&
            std::abs(restarted->timestamp_100ns - first->timestamp_100ns) <= 333334;
        std::cout << "{\"samples\":" << samples << ",\"timestamp_regressions\":" << regressions
                  << ",\"excessive_lead\":" << excessive_lead << ",\"max_lead_100ns\":" << max_lead
                  << ",\"first_100ns\":" << first->timestamp_100ns
                  << ",\"quarter_100ns\":" << later->timestamp_100ns
                  << ",\"excessive_relative_lead\":" << excessive_relative_lead
                  << ",\"max_relative_lead_100ns\":" << max_relative_lead
                  << ",\"max_relative_lag_100ns\":" << max_relative_lag
                  << ",\"restart_passed\":" << (restart_ok ? "true" : "false") << "}\n";
        return regressions == 0 && excessive_relative_lead == 0 && restart_ok ? 0 : 8;
    }

    std::cout << "BGA video smoke passed: " << later->width << 'x' << later->height
              << " first_100ns=" << first->timestamp_100ns
              << " later_100ns=" << later->timestamp_100ns << '\n';
    return 0;
}
