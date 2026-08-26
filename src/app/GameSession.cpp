#include "app/GameSession.h"
#include "app/AudioFileDecoder.h"
#include "app/AudioMixPolicy.h"
#include "app/ChartAudioPlayback.h"
#include "app/ChartAudioStreaming.h"
#include "app/ChartFileHash.h"
#include "app/GameplayCompletionFlow.h"
#include "app/MemoryDiagnostics.h"
#include "audio/OggVorbisDecoder.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <thread>
#include <unordered_set>

#include "app/ModeManager.h"
#include "app/GameplayHudWindow.h"
#include "app/RuntimeConfigMigration.h"
#include "app/PersistedRuntimeConfig.h"
#include "app/PeerBattleRules.h"
#include "app/ReplayVerifier.h"
#include "app/SessionRandomSeed.h"
#include "app/SessionResultStatus.h"
#include "config/Keymap.h"
#include "game/SpeedManager.h"
#include "timing/HighResClock.h"

namespace tenriff::app {

namespace {

constexpr int64_t kLookaheadMs = 4;
constexpr int64_t kHudRefreshMs = 8;
constexpr int64_t kGameplayStartCountdownSeconds = 3;
constexpr int64_t kGameplayStartLeadInMs = 3000;
constexpr int64_t kHudLookaheadMs = 2200;
constexpr int64_t kHudPastMs = 180;
constexpr double kHudFeedbackDisplayMs = 240.0;
constexpr double kHudRenderSlackMs = 24.0;
constexpr double kGuideToneMs = 28.0;
constexpr double kHitToneMs = 44.0;
constexpr double kGuideToneGain = 0.055;
constexpr double kHitToneGain = 0.120;
constexpr int64_t kChartAudioStartupWindowMs = 3000;
constexpr int64_t kChartAudioPrefetchWindowMs = 15000;
constexpr int64_t kChartAudioServiceIntervalMs = 25;
constexpr double kHispeedMin = 0.50;
constexpr double kHispeedMax = 50.00;
constexpr double kHispeedStep = 0.25;
constexpr double kHispeedStepCoarse = 10.0;
constexpr int64_t kHispeedRepeatInitialDelayMs = 180;
constexpr int64_t kHispeedRepeatIntervalMs = 45;
// In-play tuning steps. The judgement line moves in 1% of the playfield height and
// the visual offset in 1 ms, both finer than the menu steps because the player is
// watching the chart scroll while they nudge it.
constexpr double kJudgementLinePositionStep = 0.01;
constexpr double kInPlayVisualOffsetStep = 1.0;
constexpr double kVisualOffsetMin = config::kVisualOffsetMin;
constexpr double kVisualOffsetMax = config::kVisualOffsetMax;
constexpr std::size_t kMaxToneVoices = 256;
constexpr double kTwoPi = 6.28318530717958647692;

bool process_owns_foreground_window() {
#ifdef _WIN32
    const HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return false;
    }
    DWORD foreground_process_id = 0;
    GetWindowThreadProcessId(foreground, &foreground_process_id);
    return foreground_process_id == GetCurrentProcessId();
#else
    return true;
#endif
}

std::string normalize_runtime_key_mode_local(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "none" || value == "auto") {
        return "none";
    }
    if (value == "4k" || value == "5k" || value == "6k" || value == "7k" || value == "8k" ||
        value == "9k" || value == "10k" || value == "12k" || value == "14k" || value == "16k") {
        return value;
    }
    return "none";
}

std::string normalize_nk2_preset_local(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "transform" || value == "transform35") {
        return "transform";
    }
    if (value == "remaster" || value == "remaster65" || value == "rm") {
        return "remaster";
    }
    return "native";
}

int64_t ms_to_samples(double ms, int sample_rate) {
    if (sample_rate <= 0) {
        return 0;
    }
    return static_cast<int64_t>(std::llround(ms * static_cast<double>(sample_rate) / 1000.0));
}

int64_t ms_to_ns(int64_t ms) {
    return ms * 1'000'000LL;
}

int64_t note_visible_end_sample(const gameplay::NoteEvent& note) {
    return note.end_sample.value_or(note.start_sample);
}

bool note_is_expired_for_hud(const gameplay::NoteEvent& note, int64_t current_sample, int64_t past_samples) {
    return note_visible_end_sample(note) < current_sample - past_samples;
}

std::uint64_t estimate_audio_bytes_from_file_size(const std::string& path) {
    std::error_code ec;
#ifdef _WIN32
    const std::filesystem::path fs_path = std::filesystem::u8path(path);
#else
    const std::filesystem::path fs_path(path);
#endif
    const std::uint64_t size = static_cast<std::uint64_t>(std::filesystem::file_size(fs_path, ec));
    if (ec) {
        return 0;
    }
    return (std::max)(size * 8ull, 256ull * 1024ull);
}

template <typename Fn>
void for_each_note_audio_asset_id(const gameplay::NoteEvent& note, Fn&& fn) {
    const std::size_t count = gameplay::note_audio_asset_count(note);
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t asset_id = gameplay::note_audio_asset_at(note, index);
        if (asset_id == gameplay::kInvalidAudioAssetId) {
            continue;
        }
        fn(asset_id);
    }
}

template <typename Fn>
void for_each_note_audio_path(const gameplay::GameplayChart& chart, const gameplay::NoteEvent& note, Fn&& fn) {
    for_each_note_audio_asset_id(note, [&](std::size_t asset_id) {
        if (const std::string* path = chart.audio_asset_path(asset_id)) {
            fn(asset_id, *path);
        }
    });
}

uint16_t read_le_u16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}

uint32_t read_le_u32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

struct WavFileInfo {
    uint16_t format = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    uint16_t block_align = 0;
    std::streamoff data_offset = 0;
    uint32_t data_size = 0;
};

bool open_wav_file(const std::string& path, std::ifstream& file, WavFileInfo& info, std::string* error) {
#ifdef _WIN32
    const std::filesystem::path fs_path = std::filesystem::u8path(path);
#else
    const std::filesystem::path fs_path(path);
#endif
    file = std::ifstream(fs_path, std::ios::binary);
    if (!file) {
        if (error) {
            *error = "Failed to open audio file.";
        }
        return false;
    }

    uint8_t riff_header[12] = {};
    if (!file.read(reinterpret_cast<char*>(riff_header), sizeof(riff_header))) {
        if (error) {
            *error = "WAV file is too small.";
        }
        return false;
    }
    if (std::memcmp(riff_header, "RIFF", 4) != 0 || std::memcmp(riff_header + 8, "WAVE", 4) != 0) {
        if (error) {
            *error = "Unsupported audio container (expected RIFF/WAVE).";
        }
        return false;
    }

    bool has_fmt = false;
    bool has_data = false;
    while (file) {
        uint8_t chunk_header[8] = {};
        if (!file.read(reinterpret_cast<char*>(chunk_header), sizeof(chunk_header))) {
            break;
        }

        const uint32_t chunk_size = read_le_u32(chunk_header + 4);
        const std::streamoff chunk_data_offset = file.tellg();
        const std::streamoff padded_advance =
            static_cast<std::streamoff>(chunk_size) + (((chunk_size & 1u) != 0u) ? 1 : 0);

        if (std::memcmp(chunk_header, "fmt ", 4) == 0) {
            if (chunk_size < 16) {
                if (error) {
                    *error = "Invalid WAV fmt chunk.";
                }
                return false;
            }
            uint8_t fmt_data[16] = {};
            if (!file.read(reinterpret_cast<char*>(fmt_data), sizeof(fmt_data))) {
                if (error) {
                    *error = "Failed to read WAV fmt chunk.";
                }
                return false;
            }
            info.format = read_le_u16(fmt_data + 0);
            info.channels = read_le_u16(fmt_data + 2);
            info.sample_rate = read_le_u32(fmt_data + 4);
            info.block_align = read_le_u16(fmt_data + 12);
            info.bits_per_sample = read_le_u16(fmt_data + 14);
            has_fmt = true;
        } else if (std::memcmp(chunk_header, "data", 4) == 0) {
            info.data_offset = chunk_data_offset;
            info.data_size = chunk_size;
            has_data = true;
        }

        file.clear();
        file.seekg(chunk_data_offset + padded_advance, std::ios::beg);
    }

    if (!has_fmt || !has_data) {
        if (error) {
            *error = "WAV is missing fmt/data chunk.";
        }
        return false;
    }
    if (info.channels == 0 || info.block_align == 0 || info.bits_per_sample == 0 || info.sample_rate == 0) {
        if (error) {
            *error = "WAV fmt has invalid values.";
        }
        return false;
    }
    if (info.format != 1 && info.format != 3) {
        if (error) {
            *error = "Only PCM/IEEE-float WAV is supported.";
        }
        return false;
    }
    if ((info.bits_per_sample % 8u) != 0u) {
        if (error) {
            *error = "WAV bits-per-sample must be byte aligned.";
        }
        return false;
    }

    const uint16_t bytes_per_sample = static_cast<uint16_t>(info.bits_per_sample / 8u);
    if (static_cast<uint32_t>(bytes_per_sample) * static_cast<uint32_t>(info.channels) > info.block_align) {
        if (error) {
            *error = "WAV block align is inconsistent.";
        }
        return false;
    }

    file.clear();
    return true;
}

std::optional<int> probe_wav_sample_rate(const std::string& path, std::string* error) {
    std::ifstream file;
    WavFileInfo info;
    if (!open_wav_file(path, file, info, error)) {
        return std::nullopt;
    }
    return static_cast<int>(info.sample_rate);
}

float decode_sample(const uint8_t* ptr, uint16_t format, uint16_t bits_per_sample) {
    if (format == 1) {
        switch (bits_per_sample) {
            case 8: {
                const float value = static_cast<float>(ptr[0]);
                return std::clamp((value - 128.0f) / 128.0f, -1.0f, 1.0f);
            }
            case 16: {
                const int16_t raw = static_cast<int16_t>(read_le_u16(ptr));
                return std::clamp(static_cast<float>(raw) / 32768.0f, -1.0f, 1.0f);
            }
            case 24: {
                int32_t raw = static_cast<int32_t>(ptr[0]) |
                              (static_cast<int32_t>(ptr[1]) << 8) |
                              (static_cast<int32_t>(ptr[2]) << 16);
                if ((raw & 0x00800000) != 0) {
                    raw |= 0xFF000000;
                }
                return std::clamp(static_cast<float>(raw) / 8388608.0f, -1.0f, 1.0f);
            }
            case 32: {
                const int32_t raw = static_cast<int32_t>(read_le_u32(ptr));
                return std::clamp(static_cast<float>(raw) / 2147483648.0f, -1.0f, 1.0f);
            }
            default:
                break;
        }
    } else if (format == 3) {
        if (bits_per_sample == 32) {
            float raw = 0.0f;
            std::memcpy(&raw, ptr, sizeof(float));
            return std::clamp(raw, -1.0f, 1.0f);
        }
        if (bits_per_sample == 64) {
            double raw = 0.0;
            std::memcpy(&raw, ptr, sizeof(double));
            return static_cast<float>(std::clamp(raw, -1.0, 1.0));
        }
    }
    return 0.0f;
}

std::size_t source_frame_limit_for_output(std::size_t max_output_frames,
                                          int source_sample_rate,
                                          int target_sample_rate) {
    if (max_output_frames == 0 || source_sample_rate <= 0 || target_sample_rate <= 0) {
        return 0;
    }
    if (source_sample_rate == target_sample_rate) {
        return max_output_frames;
    }
    const long double scaled =
        std::ceil(static_cast<long double>(max_output_frames) *
                  static_cast<long double>(source_sample_rate) /
                  static_cast<long double>(target_sample_rate)) +
        1.0L;
    return scaled >= static_cast<long double>(std::numeric_limits<std::size_t>::max())
               ? std::numeric_limits<std::size_t>::max()
               : static_cast<std::size_t>(scaled);
}

bool resample_stereo_linear(const std::vector<float>& source,
                            int source_sample_rate,
                            int target_sample_rate,
                            std::vector<float>& out,
                            std::string* error,
                            std::size_t max_output_frames = 0) {
    if (source.empty()) {
        out.clear();
        return true;
    }
    if ((source.size() % 2) != 0) {
        if (error) {
            *error = "Stereo sample buffer is malformed.";
        }
        return false;
    }

    if (target_sample_rate <= 0 || source_sample_rate <= 0 || target_sample_rate == source_sample_rate) {
        out = source;
        if (max_output_frames > 0 && out.size() / 2u > max_output_frames) {
            out.resize(max_output_frames * 2u);
        }
        return true;
    }

    const std::size_t frame_count = source.size() / 2;
    const double ratio = static_cast<double>(target_sample_rate) / static_cast<double>(source_sample_rate);
    if (!std::isfinite(ratio) || ratio <= 0.0) {
        if (error) {
            *error = "Invalid sample-rate conversion ratio.";
        }
        return false;
    }

    std::size_t out_frames = std::max<std::size_t>(
        1,
        static_cast<std::size_t>(std::llround(static_cast<double>(frame_count) * ratio)));
    if (max_output_frames > 0) {
        out_frames = std::min(out_frames, max_output_frames);
    }
    out.resize(out_frames * 2);

    for (std::size_t i = 0; i < out_frames; ++i) {
        const double src_pos = static_cast<double>(i) * static_cast<double>(source_sample_rate) /
                               static_cast<double>(target_sample_rate);
        const std::size_t idx0 = std::min<std::size_t>(frame_count - 1, static_cast<std::size_t>(src_pos));
        const std::size_t idx1 = std::min<std::size_t>(frame_count - 1, idx0 + 1);
        const double frac = std::clamp(src_pos - static_cast<double>(idx0), 0.0, 1.0);

        const float l0 = source[idx0 * 2];
        const float r0 = source[idx0 * 2 + 1];
        const float l1 = source[idx1 * 2];
        const float r1 = source[idx1 * 2 + 1];
        out[i * 2] = static_cast<float>(l0 + (l1 - l0) * frac);
        out[i * 2 + 1] = static_cast<float>(r0 + (r1 - r0) * frac);
    }

    return true;
}

bool decode_wav_stereo_resampled(const std::string& path, int target_sample_rate, std::vector<float>& out,
                                 std::string* error, std::size_t max_output_frames = 0);
bool decode_ogg_stereo_resampled(const std::string& path, int target_sample_rate, std::vector<float>& out,
                                 std::string* error, std::size_t max_output_frames = 0);

#ifdef _WIN32
bool ensure_mf_started(std::string* error) {
    static std::once_flag once;
    static HRESULT startup_hr = E_FAIL;
    std::call_once(once, []() {
        startup_hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    });
    if (FAILED(startup_hr)) {
        if (error) {
            *error = "Media Foundation startup failed (hr=0x" +
                     std::to_string(static_cast<unsigned long>(startup_hr)) + ").";
        }
        return false;
    }
    return true;
}

struct ThreadComInit {
    HRESULT hr = E_FAIL;
    ThreadComInit() : hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
    ~ThreadComInit() {
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }
    [[nodiscard]] bool ok() const {
        return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
};

std::optional<std::wstring> find_ffmpeg_executable() {
    wchar_t buffer[32768] = {};
    DWORD length = SearchPathW(nullptr, L"ffmpeg.exe", nullptr,
                               static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])), buffer, nullptr);
    if (length == 0 || length >= (sizeof(buffer) / sizeof(buffer[0]))) {
        return std::nullopt;
    }
    return std::wstring(buffer, length);
}

std::filesystem::path normalize_media_foundation_path(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path normalized = fs::u8path(path);
    if (!normalized.is_absolute()) {
        const fs::path absolute = fs::absolute(normalized, ec);
        if (!ec && !absolute.empty()) {
            normalized = absolute;
        } else {
            ec.clear();
        }
    }

    const fs::path canonical = fs::weakly_canonical(normalized, ec);
    if (!ec && !canonical.empty()) {
        return canonical;
    }
    return normalized.lexically_normal();
}

bool decode_ffmpeg_stereo_resampled(const std::string& path,
                                    int target_sample_rate,
                                    std::vector<float>& out,
                                    std::string* error,
                                    std::size_t max_output_frames = 0) {
    if (target_sample_rate <= 0) {
        if (error) {
            *error = "FFmpeg fallback requires a positive target sample rate.";
        }
        return false;
    }

    const auto ffmpeg_path = find_ffmpeg_executable();
    if (!ffmpeg_path.has_value()) {
        if (error) {
            *error = "FFmpeg fallback is unavailable (ffmpeg.exe not found in PATH).";
        }
        return false;
    }

    wchar_t temp_dir[MAX_PATH] = {};
    const DWORD temp_dir_len = GetTempPathW(MAX_PATH, temp_dir);
    if (temp_dir_len == 0 || temp_dir_len >= MAX_PATH) {
        if (error) {
            *error = "Failed to acquire a temporary directory for FFmpeg fallback.";
        }
        return false;
    }

    wchar_t temp_file[MAX_PATH] = {};
    if (GetTempFileNameW(temp_dir, L"trf", 0, temp_file) == 0) {
        if (error) {
            *error = "Failed to create a temporary file for FFmpeg fallback.";
        }
        return false;
    }

    std::filesystem::path temp_path(temp_file);
    std::filesystem::path input_path = normalize_media_foundation_path(path);
    std::wstring command = L"\"" + ffmpeg_path.value() + L"\" -v error -y -nostdin -i \"" +
                           input_path.wstring() + L"\"";
    if (max_output_frames > 0) {
        const long double duration_seconds =
            static_cast<long double>(max_output_frames) /
            static_cast<long double>(target_sample_rate);
        command += L" -t " + std::to_wstring(static_cast<double>(duration_seconds));
    }
    command += L" -f wav -ac 2 -ar " + std::to_wstring(target_sample_rate) +
               L" \"" + temp_path.wstring() + L"\"";

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};
    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');

    const BOOL launched = CreateProcessW(nullptr,
                                         command_buffer.data(),
                                         nullptr,
                                         nullptr,
                                         FALSE,
                                         CREATE_NO_WINDOW,
                                         nullptr,
                                         nullptr,
                                         &startup_info,
                                         &process_info);
    if (!launched) {
        std::error_code ec;
        std::filesystem::remove(temp_path, ec);
        if (error) {
            *error = "Failed to launch FFmpeg fallback.";
        }
        return false;
    }

    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);

    bool success = false;
    std::string wav_error;
    if (exit_code == 0) {
        success = decode_wav_stereo_resampled(
            temp_path.u8string(), target_sample_rate, out, &wav_error, max_output_frames);
    }

    std::error_code remove_ec;
    std::filesystem::remove(temp_path, remove_ec);

    if (!success) {
        if (error) {
            if (exit_code != 0) {
                *error = "FFmpeg fallback process returned exit code " + std::to_string(exit_code) + ".";
            } else {
                *error = "FFmpeg fallback produced an unreadable WAV: " + wav_error;
            }
        }
        return false;
    }

    return true;
}

std::optional<int> probe_mf_sample_rate(const std::string& path, std::string* error) {
    if (!ensure_mf_started(error)) {
        return std::nullopt;
    }

    thread_local ThreadComInit com_init;
    if (!com_init.ok()) {
        if (error) {
            *error = "COM initialization failed for Media Foundation probe.";
        }
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    const std::wstring wide_path = normalize_media_foundation_path(path).wstring();
    HRESULT hr = MFCreateSourceReaderFromURL(wide_path.c_str(), nullptr, &reader);
    if (FAILED(hr) || !reader) {
        if (error) {
            *error = "Media Foundation could not open file.";
        }
        return std::nullopt;
    }

    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    Microsoft::WRL::ComPtr<IMFMediaType> current_type;
    hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &current_type);
    if (FAILED(hr) || !current_type) {
        if (error) {
            *error = "Media Foundation failed to query audio format.";
        }
        return std::nullopt;
    }

    UINT32 sample_rate = 0;
    hr = current_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sample_rate);
    if (FAILED(hr) || sample_rate == 0) {
        if (error) {
            *error = "Media Foundation did not report a valid sample rate.";
        }
        return std::nullopt;
    }
    return static_cast<int>(sample_rate);
}

bool decode_mf_stereo_resampled(const std::string& path,
                                int target_sample_rate,
                                std::vector<float>& out,
                                std::string* error,
                                std::size_t max_output_frames = 0) {
    if (!ensure_mf_started(error)) {
        return false;
    }

    thread_local ThreadComInit com_init;
    if (!com_init.ok()) {
        if (error) {
            *error = "COM initialization failed for Media Foundation decode.";
        }
        return false;
    }

    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    const std::wstring wide_path = normalize_media_foundation_path(path).wstring();
    HRESULT hr = MFCreateSourceReaderFromURL(wide_path.c_str(), nullptr, &reader);
    if (FAILED(hr)) {
        if (error) {
            *error = "Media Foundation could not open file.";
        }
        return false;
    }

    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    auto try_set_output_type = [&](const GUID& subtype) -> bool {
        Microsoft::WRL::ComPtr<IMFMediaType> out_type;
        if (FAILED(MFCreateMediaType(&out_type))) {
            return false;
        }
        if (FAILED(out_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio))) {
            return false;
        }
        if (FAILED(out_type->SetGUID(MF_MT_SUBTYPE, subtype))) {
            return false;
        }
        return SUCCEEDED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, out_type.Get()));
    };

    if (!try_set_output_type(MFAudioFormat_Float) && !try_set_output_type(MFAudioFormat_PCM)) {
        if (error) {
            *error = "Media Foundation could not provide float/PCM output.";
        }
        return false;
    }

    Microsoft::WRL::ComPtr<IMFMediaType> current_type;
    hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &current_type);
    if (FAILED(hr) || !current_type) {
        if (error) {
            *error = "Media Foundation failed to query audio format.";
        }
        return false;
    }

    UINT32 channels = 0;
    UINT32 source_sample_rate = 0;
    UINT32 bits_per_sample = 0;
    GUID subtype = GUID_NULL;
    current_type->GetGUID(MF_MT_SUBTYPE, &subtype);
    current_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    current_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &source_sample_rate);
    current_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits_per_sample);
    if (channels == 0 || source_sample_rate == 0) {
        if (error) {
            *error = "Media Foundation returned an invalid audio format.";
        }
        return false;
    }

    const bool is_float = (subtype == MFAudioFormat_Float);
    const bool is_pcm = (subtype == MFAudioFormat_PCM);
    if (!is_float && !is_pcm) {
        if (error) {
            *error = "Media Foundation output subtype is unsupported.";
        }
        return false;
    }
    if (bits_per_sample == 0) {
        bits_per_sample = is_float ? 32u : 16u;
    }
    if ((bits_per_sample % 8u) != 0u) {
        if (error) {
            *error = "Media Foundation output bits-per-sample is invalid.";
        }
        return false;
    }

    const std::size_t bytes_per_sample = static_cast<std::size_t>(bits_per_sample / 8u);
    const std::size_t max_source_frames = source_frame_limit_for_output(
        max_output_frames, static_cast<int>(source_sample_rate), target_sample_rate);
    std::vector<float> source;

    for (;;) {
        DWORD stream_index = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        Microsoft::WRL::ComPtr<IMFSample> sample;
        hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                0,
                                &stream_index,
                                &flags,
                                &timestamp,
                                &sample);
        if (FAILED(hr)) {
            if (error) {
                *error = "Media Foundation failed while decoding audio.";
            }
            return false;
        }
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
            break;
        }
        if (!sample) {
            continue;
        }

        Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (FAILED(hr) || !buffer) {
            continue;
        }

        BYTE* data = nullptr;
        DWORD max_len = 0;
        DWORD cur_len = 0;
        hr = buffer->Lock(&data, &max_len, &cur_len);
        if (FAILED(hr) || !data || cur_len == 0) {
            if (SUCCEEDED(hr)) {
                buffer->Unlock();
            }
            continue;
        }

        const std::size_t frame_stride = bytes_per_sample * static_cast<std::size_t>(channels);
        if (frame_stride == 0) {
            buffer->Unlock();
            continue;
        }
        std::size_t frame_count = static_cast<std::size_t>(cur_len) / frame_stride;
        if (max_source_frames > 0) {
            const std::size_t decoded_frames = source.size() / 2u;
            if (decoded_frames >= max_source_frames) {
                buffer->Unlock();
                break;
            }
            frame_count = std::min(frame_count, max_source_frames - decoded_frames);
        }
        source.reserve(source.size() + frame_count * 2);

        for (std::size_t frame = 0; frame < frame_count; ++frame) {
            const BYTE* frame_ptr = data + frame * frame_stride;
            float left = 0.0f;
            float right = 0.0f;
            if (channels == 1) {
                const float mono = decode_sample(frame_ptr, is_float ? 3 : 1, static_cast<uint16_t>(bits_per_sample));
                left = mono;
                right = mono;
            } else {
                left = decode_sample(frame_ptr, is_float ? 3 : 1, static_cast<uint16_t>(bits_per_sample));
                right = decode_sample(frame_ptr + bytes_per_sample, is_float ? 3 : 1,
                                      static_cast<uint16_t>(bits_per_sample));
            }
            source.push_back(left);
            source.push_back(right);
        }

        buffer->Unlock();
        if (max_source_frames > 0 && source.size() / 2u >= max_source_frames) {
            break;
        }
    }

    return resample_stereo_linear(source,
                                  static_cast<int>(source_sample_rate),
                                  target_sample_rate,
                                  out,
                                  error,
                                  max_output_frames);
}
#endif

std::optional<int> probe_audio_sample_rate(const std::string& path, std::string* error) {
    namespace fs = std::filesystem;
#ifdef _WIN32
    const fs::path fs_path = fs::u8path(path);
#else
    const fs::path fs_path(path);
#endif
    std::string ext = fs_path.extension().u8string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (ext == ".wav" || ext == ".wave") {
        return probe_wav_sample_rate(path, error);
    }
    if (ext == ".ogg") {
        std::string ogg_error;
        auto sample_rate = audio::probe_ogg_vorbis_sample_rate(path, &ogg_error);
        if (sample_rate.has_value()) {
            return sample_rate;
        }
#ifdef _WIN32
        std::string mf_error;
        sample_rate = probe_mf_sample_rate(path, &mf_error);
        if (sample_rate.has_value()) {
            return sample_rate;
        }
        if (error) {
            *error = ogg_error + " | MF fallback: " + mf_error;
        }
        return std::nullopt;
#else
        if (error) {
            *error = ogg_error;
        }
        return std::nullopt;
#endif
    }

#ifdef _WIN32
    return probe_mf_sample_rate(path, error);
#else
    if (error) {
        *error = "Sample-rate probing is only implemented on Windows for non-WAV files.";
    }
    return std::nullopt;
#endif
}

std::uint64_t estimate_audio_decoded_bytes(const std::string& path, int target_sample_rate) {
    namespace fs = std::filesystem;
#ifdef _WIN32
    const fs::path fs_path = fs::u8path(path);
#else
    const fs::path fs_path(path);
#endif
    std::string ext = fs_path.extension().u8string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (ext == ".wav" || ext == ".wave") {
        std::ifstream file;
        WavFileInfo info;
        std::string error;
        if (open_wav_file(path, file, info, &error)) {
            const std::uint64_t frame_count = static_cast<std::uint64_t>(info.data_size / info.block_align);
            const std::uint64_t source_rate = static_cast<std::uint64_t>(info.sample_rate);
            const std::uint64_t target_rate = static_cast<std::uint64_t>(target_sample_rate > 0 ? target_sample_rate
                                                                                                : info.sample_rate);
            const std::uint64_t out_frames =
                (source_rate > 0) ? (frame_count * target_rate + (source_rate / 2u)) / source_rate : frame_count;
            return (std::max)(out_frames * 2u * static_cast<std::uint64_t>(sizeof(float)), 256ull * 1024ull);
        }
    }

    return estimate_audio_bytes_from_file_size(path);
}

std::optional<std::string> select_primary_audio_path(const gameplay::GameplayChart& chart) {
    std::unordered_set<std::string> seen;
    std::optional<std::string> best_path;
    std::uintmax_t best_size = 0;

    auto consider_path = [&](const std::string& path) {
        if (path.empty() || !seen.emplace(path).second) {
            return;
        }

        std::error_code ec;
#ifdef _WIN32
        const std::filesystem::path fs_path = std::filesystem::u8path(path);
#else
        const std::filesystem::path fs_path(path);
#endif
        const std::uintmax_t size = std::filesystem::file_size(fs_path, ec);
        if (!best_path.has_value() || (!ec && size > best_size)) {
            best_path = path;
            best_size = ec ? 0 : size;
        }
    };

    for (const auto& cue : chart.audio_cues) {
        if (const std::string* path = chart.audio_asset_path(cue.asset_id)) {
            consider_path(*path);
        }
    }
    for (const auto& note : chart.notes) {
        for_each_note_audio_path(chart, note, [&](std::size_t, const std::string& path) {
            consider_path(path);
        });
    }

    if (best_path.has_value()) {
        return best_path;
    }
    if (!chart.audio_cues.empty()) {
        if (const std::string* path = chart.audio_asset_path(chart.audio_cues.front().asset_id)) {
            return *path;
        }
    }
    for (const auto& note : chart.notes) {
        std::optional<std::string> first_path;
        for_each_note_audio_path(chart, note, [&](std::size_t, const std::string& path) {
            if (!first_path.has_value()) {
                first_path = path;
            }
        });
        if (first_path.has_value()) {
            return first_path;
        }
    }
    return std::nullopt;
}

std::optional<int> detect_chart_preferred_sample_rate(const gameplay::GameplayChart& chart,
                                                      std::string* diagnostic) {
    auto primary_path = select_primary_audio_path(chart);
    if (!primary_path.has_value()) {
        return std::nullopt;
    }

    std::string probe_error;
    auto sample_rate = probe_audio_sample_rate(primary_path.value(), &probe_error);
    if (!sample_rate.has_value()) {
        if (diagnostic) {
            *diagnostic = "Failed to probe chart audio sample rate from " + primary_path.value() +
                          ": " + probe_error;
        }
        return std::nullopt;
    }

    if (*sample_rate < 8'000 || *sample_rate > 192'000) {
        if (diagnostic) {
            *diagnostic = "Ignoring implausible chart audio sample rate " + std::to_string(*sample_rate) +
                          " Hz from " + primary_path.value() + ".";
        }
        return std::nullopt;
    }

    return sample_rate;
}

bool decode_wav_stereo_resampled(const std::string& path, int target_sample_rate, std::vector<float>& out,
                                 std::string* error, std::size_t max_output_frames) {
    std::ifstream file;
    WavFileInfo info;
    if (!open_wav_file(path, file, info, error)) {
        return false;
    }

    const uint16_t bytes_per_sample = static_cast<uint16_t>(info.bits_per_sample / 8u);
    const std::size_t available_frame_count = info.data_size / info.block_align;
    const std::size_t max_source_frames = source_frame_limit_for_output(
        max_output_frames, static_cast<int>(info.sample_rate), target_sample_rate);
    const std::size_t frame_count = max_source_frames > 0
                                        ? std::min(available_frame_count, max_source_frames)
                                        : available_frame_count;
    if (frame_count == 0) {
        out.clear();
        return true;
    }

    file.seekg(info.data_offset, std::ios::beg);
    if (!file) {
        if (error) {
            *error = "Failed to seek to WAV data chunk.";
        }
        return false;
    }

    auto decode_frames = [&](std::vector<float>& destination) -> bool {
        destination.resize(frame_count * 2);
        constexpr std::size_t kChunkFrames = 4096;
        std::vector<uint8_t> chunk_bytes(kChunkFrames * info.block_align);
        std::size_t frame_offset = 0;
        std::size_t frames_remaining = frame_count;
        while (frames_remaining > 0) {
            const std::size_t frames_to_read = (std::min)(frames_remaining, kChunkFrames);
            const std::size_t bytes_to_read = frames_to_read * info.block_align;
            if (!file.read(reinterpret_cast<char*>(chunk_bytes.data()), static_cast<std::streamsize>(bytes_to_read))) {
                if (error) {
                    *error = "Failed to read WAV audio data.";
                }
                return false;
            }

            for (std::size_t frame = 0; frame < frames_to_read; ++frame) {
                const uint8_t* frame_ptr = chunk_bytes.data() + frame * info.block_align;
                float left = 0.0f;
                float right = 0.0f;
                if (info.channels == 1) {
                    const float mono = decode_sample(frame_ptr, info.format, info.bits_per_sample);
                    left = mono;
                    right = mono;
                } else {
                    left = decode_sample(frame_ptr, info.format, info.bits_per_sample);
                    right = decode_sample(frame_ptr + bytes_per_sample, info.format, info.bits_per_sample);
                }

                const std::size_t dest_index = (frame_offset + frame) * 2;
                destination[dest_index] = left;
                destination[dest_index + 1] = right;
            }

            frame_offset += frames_to_read;
            frames_remaining -= frames_to_read;
        }
        return true;
    };

    if (target_sample_rate <= 0 || target_sample_rate == static_cast<int>(info.sample_rate)) {
        return decode_frames(out);
    }

    std::vector<float> source;
    if (!decode_frames(source)) {
        return false;
    }
    return resample_stereo_linear(source,
                                  static_cast<int>(info.sample_rate),
                                  target_sample_rate,
                                  out,
                                  error,
                                  max_output_frames);
}

bool decode_ogg_stereo_resampled(const std::string& path, int target_sample_rate, std::vector<float>& out,
                                 std::string* error, std::size_t max_output_frames) {
    int source_sample_rate = 0;
    const auto probed_sample_rate = audio::probe_ogg_vorbis_sample_rate(path, nullptr);
    const std::size_t max_source_frames = probed_sample_rate.has_value()
                                              ? source_frame_limit_for_output(
                                                    max_output_frames,
                                                    *probed_sample_rate,
                                                    target_sample_rate)
                                              : 0;
    std::vector<float> source;
    if (!audio::decode_ogg_vorbis_stereo(
            path, &source_sample_rate, source, error, max_source_frames)) {
        return false;
    }
    return resample_stereo_linear(
        source, source_sample_rate, target_sample_rate, out, error, max_output_frames);
}

bool decode_audio_stereo_resampled(const std::string& path, int target_sample_rate, std::vector<float>& out,
                                   std::string* error, std::size_t max_output_frames = 0) {
    namespace fs = std::filesystem;
#ifdef _WIN32
    const fs::path fs_path = fs::u8path(path);
#else
    const fs::path fs_path(path);
#endif
    std::string ext = fs_path.extension().u8string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (ext == ".wav" || ext == ".wave") {
        std::string wav_error;
        if (decode_wav_stereo_resampled(
                path, target_sample_rate, out, &wav_error, max_output_frames)) {
            return true;
        }
#ifdef _WIN32
        // Some WAV variants are better handled by Media Foundation.
        std::string mf_error;
        if (decode_mf_stereo_resampled(
                path, target_sample_rate, out, &mf_error, max_output_frames)) {
            return true;
        }
        if (error) {
            *error = wav_error + " | MF fallback: " + mf_error;
        }
        return false;
#else
        if (error) {
            *error = wav_error;
        }
        return false;
#endif
    }

    if (ext == ".ogg") {
        std::string ogg_error;
        if (decode_ogg_stereo_resampled(
                path, target_sample_rate, out, &ogg_error, max_output_frames)) {
            return true;
        }
#ifdef _WIN32
        std::string mf_error;
        if (decode_mf_stereo_resampled(
                path, target_sample_rate, out, &mf_error, max_output_frames)) {
            return true;
        }
        if (error) {
            *error = ogg_error + " | MF fallback: " + mf_error;
        }
        return false;
#else
        if (error) {
            *error = ogg_error;
        }
        return false;
#endif
    }

#ifdef _WIN32
    std::string mf_error;
    if (decode_mf_stereo_resampled(
            path, target_sample_rate, out, &mf_error, max_output_frames)) {
        return true;
    }
    std::string ffmpeg_error;
    if (decode_ffmpeg_stereo_resampled(
            path, target_sample_rate, out, &ffmpeg_error, max_output_frames)) {
        return true;
    }
    if (error) {
        *error = mf_error + " | FFmpeg fallback: " + ffmpeg_error;
    }
    return false;
#else
    if (error) {
        *error = "Only WAV is currently supported on this platform.";
    }
    out.clear();
    return false;
#endif
}

std::optional<int> parse_lane_index(std::string_view lane) {
    if (lane.rfind("lane", 0) != 0) {
        return std::nullopt;
    }
    int value = 0;
    for (std::size_t i = 4; i < lane.size(); ++i) {
        char ch = lane[i];
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        value = value * 10 + (ch - '0');
    }
    if (value <= 0) {
        return std::nullopt;
    }
    return value;
}

std::string replay_key_mode_token(int lane_count) {
    switch (lane_count) {
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 16:
            return std::to_string(lane_count) + "k";
        default:
            return {};
    }
}

std::optional<game::GaugeType> parse_gauge_type(std::string_view value) {
    std::string token(value);
    std::transform(token.begin(), token.end(), token.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch) {
        return ch == '-' || ch == '_' || ch == ' ' || ch == '\t';
    }), token.end());
    if (token == "exhard") {
        return game::GaugeType::ExHard;
    }
    if (token == "hard") {
        return game::GaugeType::Hard;
    }
    if (token == "normal") {
        return game::GaugeType::Normal;
    }
    if (token == "easy") {
        return game::GaugeType::Easy;
    }
    return std::nullopt;
}
bool is_gauge_shift_token(std::string_view value) {
    std::string token(value);
    std::transform(token.begin(), token.end(), token.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch) {
        return ch == '-' || ch == '_' || ch == ' ' || ch == '\t';
    }), token.end());
    return token == "shift" || token == "gaugeshift";
}

std::string chart_format_token(ChartFormat format) {
    switch (format) {
        case ChartFormat::Bms: return "bms";
        case ChartFormat::Unknown:
        default: return "unknown";
    }
}

std::string gauge_type_token(game::GaugeType type) {
    switch (type) {
        case game::GaugeType::ExHard: return "ex_hard";
        case game::GaugeType::Hard: return "hard";
        case game::GaugeType::Easy: return "easy";
        case game::GaugeType::Normal:
        default: return "normal";
    }
}

std::string utc_timestamp_compact() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d_%02d%02d%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buffer;
}

}  // namespace

bool decode_audio_file_stereo_resampled(const std::string& path,
                                        int target_sample_rate,
                                        std::vector<float>& out,
                                        std::string* error,
                                        std::size_t max_output_frames) {
    return decode_audio_stereo_resampled(
        path, target_sample_rate, out, error, max_output_frames);
}

#include "GameSessionTail.inl"
