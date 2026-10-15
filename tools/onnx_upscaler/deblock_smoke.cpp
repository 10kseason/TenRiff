#include "render/FfmpegImageFilter.h"
#include <future>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    // The timeout fixture copies this probe as its isolated ffmpeg.exe.
    // It never reads/writes the pipes, exercising cancellation of both threads.
    if (argc > 1 && std::string(argv[1]) == "-hide_banner") {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        return 1;
    }
    using namespace tenriff::render;
    std::vector<std::uint8_t> source(65u * 49u * 4u);
    for (std::size_t p = 0; p < source.size() / 4; ++p) {
        for (int c = 0; c < 3; ++c) source[p * 4 + c] = 120 + (p % 65 / 8 % 2) * 15;
        source[p * 4 + 3] = static_cast<std::uint8_t>(p % 256);
    }
    auto invalid = source;
    if (deblock_bgra_ffmpeg(0, 49, invalid) != DeblockResult::Failed || invalid != source) return 1;
    if (deblock_bgra_ffmpeg(65, 49, invalid, [] { return true; }) != DeblockResult::Cancelled ||
        invalid != source) return 2;
    if (argc == 2 && std::string(argv[1]) == "--missing") {
        if (deblock_bgra_ffmpeg(65, 49, invalid) != DeblockResult::Unavailable || invalid != source) return 3;
        std::cout << "Missing FFmpeg, invalid dimensions and pre-cancellation preserve source: passed\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--timeout") {
        const auto start = std::chrono::steady_clock::now();
        const auto result = deblock_bgra_ffmpeg(65, 49, invalid);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (result != DeblockResult::Failed || invalid != source ||
            elapsed < std::chrono::seconds(9) || elapsed > std::chrono::seconds(15)) return 7;
        std::cout << "Blocked helper timed out; input/output threads joined and source preserved: passed\n";
        return 0;
    }
    auto run = [&]() {
        auto pixels = source;
        if (deblock_bgra_ffmpeg(65, 49, pixels) != DeblockResult::Completed || pixels == source) return false;
        for (std::size_t i = 3; i < pixels.size(); i += 4) if (pixels[i] != source[i]) return false;
        return true;
    };
    auto first = std::async(std::launch::async, run);
    auto second = std::async(std::launch::async, run);
    auto third = std::async(std::launch::async, run);
    if (!first.get() || !second.get() || !third.get()) return 4;
    // Cancel after process launch; leave both dimensions and source bytes intact.
    std::vector<std::uint8_t> large(4096u * 4096u * 4u, 120);
    unsigned polls = 0;
    if (deblock_bgra_ffmpeg(4096, 4096, large, [&] { return ++polls > 1; }) != DeblockResult::Cancelled)
        return 5;
    for (auto value : large) if (value != 120) return 6;
    std::cout << "FFmpeg deblock concurrent workers (3), RGB change, exact alpha, invalid input and active cancellation: passed\n";
    return 0;
}
