#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <streambuf>
#include <string>
#include <thread>
#include <string_view>
#include <algorithm>
#include <cmath>

#include "render/BgaVideoDecoder.h"
#include "render/BgaVideoFfmpeg.h"
#include "util/Utf8Compat.h"

namespace {

// This mode must exercise Media Foundation, not silently pass through the
// FFmpeg fallback (which already has a forward-seek bound). Keep diagnostics
// visible and inspect them safely while the decoder worker is running.
class DecoderDiagnostics final : public std::streambuf {
public:
    DecoderDiagnostics() : destination_(std::cerr.rdbuf(this)) {}
    ~DecoderDiagnostics() override { std::cerr.rdbuf(destination_); }

    bool used_ffmpeg_fallback() {
        std::lock_guard<std::mutex> lock(mutex_);
        return used_ffmpeg_fallback_;
    }

protected:
    std::streamsize xsputn(const char* data, std::streamsize size) override {
        std::lock_guard<std::mutex> lock(mutex_);
        record(data, size);
        return destination_->sputn(data, size);
    }

    int_type overflow(int_type value) override {
        if (traits_type::eq_int_type(value, traits_type::eof())) return traits_type::not_eof(value);
        const char character = traits_type::to_char_type(value);
        std::lock_guard<std::mutex> lock(mutex_);
        record(&character, 1);
        return destination_->sputc(character);
    }

    int sync() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return destination_->pubsync();
    }

private:
    void record(const char* data, std::streamsize size) {
        recent_.append(data, static_cast<std::size_t>(size));
        used_ffmpeg_fallback_ = used_ffmpeg_fallback_ ||
            recent_.find("FFmpeg fallback") != std::string::npos;
        if (recent_.size() > 128) recent_.erase(0, recent_.size() - 128);
    }

    std::streambuf* destination_;
    std::mutex mutex_;
    std::string recent_;
    bool used_ffmpeg_fallback_ = false;
};

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
        std::wcerr << L"usage: bga_video_smoke <video> [--timeline|--ffmpeg|--stall-recovery|--stall-eof|--long-gop]\n"
                   << L"--stall-recovery requires a native Media Foundation video fixture at least 65 seconds long.\n"
                   << L"--stall-eof requires a native Media Foundation video fixture EXACTLY 65 seconds long.\n";
        return 2;
    }
    const std::filesystem::path path(argv[1]);
    if (!std::filesystem::is_regular_file(path)) {
        std::wcerr << L"video not found: " << path.c_str() << L'\n';
        return 2;
    }

    const bool stall_recovery = argc == 3 && std::wstring_view(argv[2]) == L"--stall-recovery";
    const bool stall_eof = argc == 3 && std::wstring_view(argv[2]) == L"--stall-eof";
    const bool long_gop = argc == 3 && std::wstring_view(argv[2]) == L"--long-gop";
    // Destroy the decoder worker before restoring the captured stderr buffer.
    const auto diagnostics = (stall_recovery || stall_eof || long_gop) ? std::make_unique<DecoderDiagnostics>() : nullptr;
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

    if (long_gop) {
        // A 65 s/30 fps fixture with 30 s keyframe spacing exceeds the 240-read
        // budget. Repeated requests must continue decoding instead of seeking
        // back to the same keyframe and discarding progress forever.
        std::int64_t previous = first->timestamp_100ns;
        bool monotonic = true, caught_up = false;
        int samples = 0;
        for (int i = 0; i < 12; ++i) {
            const double position = 20.0 + i / 30.0;
            decoder.request(path_utf8, position);
            const auto frame = wait_for_frame(decoder, std::chrono::seconds(10));
            if (!valid_frame(frame)) return 11;
            ++samples;
            monotonic = monotonic && frame->timestamp_100ns > previous;
            previous = frame->timestamp_100ns;
            const auto lag = static_cast<std::int64_t>(std::llround(position * 10'000'000)) - previous;
            std::cout << "{\"long_gop_target_seconds\":" << position
                      << ",\"timestamp_100ns\":" << previous << ",\"lag_100ns\":" << lag << "}\n";
            if (std::abs(lag) <= 2'000'000) { caught_up = true; break; }
        }
        // Prime normally to 39 s without triggering a catch-up seek. Seeking
        // 42 s in this fixture lands at 30 s; the first 240 reads end before
        // the already displayed 39 s and must never rewind that image.
        decoder.clear();
        previous = 0;
        bool retained_floor = true, advanced_recovery = false;
        for (int position = 0; position <= 39; position += 3) {
            decoder.request(path_utf8, static_cast<double>(position));
            const auto frame = wait_for_frame(decoder, std::chrono::seconds(10));
            if (!valid_frame(frame)) return 11;
            previous = frame->timestamp_100ns;
        }
        for (int i = 0; i < 12; ++i) {
            const double position = 43.0 + i / 30.0;
            decoder.request(path_utf8, position);
            const auto frame = wait_for_frame(decoder, std::chrono::seconds(10));
            if (!valid_frame(frame)) return 11;
            retained_floor = retained_floor && frame->timestamp_100ns >= previous;
            previous = frame->timestamp_100ns;
            const auto lag = static_cast<std::int64_t>(std::llround(position * 10'000'000)) - previous;
            std::cout << "{\"advanced_gop_target_seconds\":" << position
                      << ",\"timestamp_100ns\":" << previous << ",\"lag_100ns\":" << lag << "}\n";
            if (std::abs(lag) <= 2'000'000) { advanced_recovery = true; break; }
        }
        const bool passed = monotonic && caught_up && retained_floor && advanced_recovery &&
            !diagnostics->used_ffmpeg_fallback();
        std::cout << "{\"long_gop_recovery_passed\":" << (passed ? "true" : "false")
                  << ",\"samples\":" << samples
                  << ",\"retained_presentation_floor\":" << (retained_floor ? "true" : "false") << "}\n";
        return passed ? 0 : 11;
    }
    if (stall_eof) {
        // This fixture ends at exactly 65 seconds. Seeking past EOF must still
        // discover and retain its final frame, not preserve the old 60 s image.
        const auto report_frame = [](double position,
                                     const std::shared_ptr<const tenriff::render::BgaVideoFrame>& frame) {
            std::cout << "{\"stall_eof_target_seconds\":" << position
                      << ",\"first_completed_timestamp_100ns\":";
            if (valid_frame(frame)) std::cout << frame->timestamp_100ns;
            else std::cout << "null";
            std::cout << "}\n";
        };
        const auto request_first = [&](double position) {
            decoder.request(path_utf8, position);
            const auto frame = wait_for_frame(decoder, std::chrono::seconds(10));
            report_frame(position, frame);
            return frame;
        };
        report_frame(0.0, first);
        const auto near_end = request_first(60.0);
        const auto final = request_first(70.0);
        const auto held = request_first(71.0);
        const auto restarted = request_first(0.0);
        const bool near_end_ok = valid_frame(near_end) &&
            std::abs(near_end->timestamp_100ns - 600'000'000) <= 2'000'000;
        const bool final_ok = valid_frame(final) &&
            final->timestamp_100ns >= 640'000'000 && final->timestamp_100ns <= 650'000'000;
        // The reader retains the same immutable object once EOF is known.
        const bool retained = valid_frame(held) && held == final;
        const bool rewind_ok = valid_frame(restarted) &&
            std::abs(restarted->timestamp_100ns - first->timestamp_100ns) <= 333334;
        const bool native_mf_only = !diagnostics->used_ffmpeg_fallback();
        const bool passed = near_end_ok && final_ok && retained && rewind_ok && native_mf_only;
        std::cout << "{\"stall_eof_passed\":" << (passed ? "true" : "false")
                  << ",\"near_end_within_200ms\":" << (near_end_ok ? "true" : "false")
                  << ",\"final_frame_in_64_to_65_seconds\":" << (final_ok ? "true" : "false")
                  << ",\"eof_same_frame_retained\":" << (retained ? "true" : "false")
                  << ",\"rewind_passed\":" << (rewind_ok ? "true" : "false")
                  << ",\"native_media_foundation_only\":" << (native_mf_only ? "true" : "false")
                  << ",\"exact_fixture_seconds\":65}\n";
        return passed ? 0 : 10;
    }

    decoder.request(path_utf8, 0.25);
    const auto later = wait_for_frame(decoder, std::chrono::seconds(10));
    if (!valid_frame(later) || later->timestamp_100ns < first->timestamp_100ns) {
        std::cerr << "Media Foundation did not advance the BGA video frame\n";
        return 4;
    }
    if (stall_recovery) {
        constexpr std::int64_t kMaximumError100ns = 2'000'000; // 200 ms.
        bool timestamps_ok = true;
        int samples = 0;
        const auto check_first_completed = [&](double position,
                                               const std::shared_ptr<const tenriff::render::BgaVideoFrame>& frame) {
            const auto target = static_cast<std::int64_t>(std::llround(position * 10'000'000.0));
            const bool valid = valid_frame(frame);
            const auto lag = valid ? target - frame->timestamp_100ns : target;
            const bool ok = valid && std::abs(lag) <= kMaximumError100ns;
            std::cout << "{\"stall_target_seconds\":" << position
                      << ",\"first_completed_timestamp_100ns\":";
            if (valid) std::cout << frame->timestamp_100ns;
            else std::cout << "null";
            std::cout << ",\"lag_100ns\":" << lag
                      << ",\"within_200ms\":" << (ok ? "true" : "false") << "}\n";
            ++samples;
            timestamps_ok = timestamps_ok && ok;
        };
        check_first_completed(0.25, later);
        for (const double position : {20.0, 40.0, 60.0}) {
            decoder.request(path_utf8, position);
            // Inspect exactly the first completed frame. Polling until a newer
            // image arrives would conceal an old-frame catch-up backlog.
            check_first_completed(position, wait_for_frame(decoder, std::chrono::seconds(10)));
        }
        // A paused clock repeats the same request and intentionally produces no
        // replacement. The renderer keeps the completed image it already owns.
        decoder.request(path_utf8, 60.0);
        const bool repeat_deduplicated = !wait_for_frame(decoder, std::chrono::milliseconds(100));
        decoder.request(path_utf8, 0.0);
        const auto restarted = wait_for_frame(decoder, std::chrono::seconds(10));
        const bool rewind_ok = valid_frame(restarted) &&
            std::abs(restarted->timestamp_100ns - first->timestamp_100ns) <= 333334;
        const bool native_mf_only = !diagnostics->used_ffmpeg_fallback();
        const bool passed = timestamps_ok && repeat_deduplicated && rewind_ok && native_mf_only;
        std::cout << "{\"stall_recovery_passed\":" << (passed ? "true" : "false")
                  << ",\"samples\":" << samples
                  << ",\"same_position_deduplicated\":" << (repeat_deduplicated ? "true" : "false")
                  << ",\"rewind_passed\":" << (rewind_ok ? "true" : "false")
                  << ",\"native_media_foundation_only\":" << (native_mf_only ? "true" : "false")
                  << ",\"minimum_fixture_seconds\":65}\n";
        return passed ? 0 : 9;
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
