#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

#include "render/BgaVideoDecoder.h"
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
    if (argc != 2) {
        std::wcerr << L"usage: bga_video_smoke <video.mpg>\n";
        return 2;
    }
    const std::filesystem::path path(argv[1]);
    if (!std::filesystem::is_regular_file(path)) {
        std::wcerr << L"video not found: " << path.c_str() << L'\n';
        return 2;
    }

    tenriff::render::BgaVideoDecoder decoder;
    const std::string path_utf8 = tenriff::util::path_to_utf8_lossy(path);
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

    std::cout << "BGA video smoke passed: " << later->width << 'x' << later->height
              << " first_100ns=" << first->timestamp_100ns
              << " later_100ns=" << later->timestamp_100ns << '\n';
    return 0;
}
