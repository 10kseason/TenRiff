#include "app/GameSession.h"
#include "app/ChartAudioPlayback.h"
#include "app/ChartAudioStreaming.h"
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

#include "app/ModeResolver.h"
#include "app/GameplayHudWindow.h"
#include "app/RuntimeConfigMigration.h"
#include "config/Keymap.h"
#include "gameplay/ModeApplier.h"
#include "timing/HighResClock.h"

namespace tenriff::app {

namespace {

constexpr int64_t kLookaheadMs = 4;
constexpr int64_t kHudRefreshMs = 8;
constexpr int64_t kGameplayStartCountdownSeconds = 3;
constexpr int64_t kHudLookaheadMs = 2200;
constexpr int64_t kHudPastMs = 180;
constexpr double kHudRenderSlackMs = 24.0;
constexpr double kGuideToneMs = 28.0;
constexpr double kHitToneMs = 44.0;
constexpr double kInputBacklogCatchupFloorMs = 96.0;
constexpr double kGuideToneGain = 0.055;
constexpr double kHitToneGain = 0.120;
constexpr float kOutputSoftLimitThreshold = 0.92f;
constexpr int64_t kChartAudioStartupWindowMs = 3000;
constexpr int64_t kChartAudioPrefetchWindowMs = 15000;
constexpr int64_t kChartAudioServiceIntervalMs = 25;
constexpr double kHispeedMin = 0.50;
constexpr double kHispeedMax = 50.00;
constexpr double kHispeedStep = 0.25;
constexpr double kHispeedStepCoarse = 10.0;
constexpr std::size_t kMaxToneVoices = 256;
constexpr double kTwoPi = 6.28318530717958647692;

int target_lane_count(gameplay::KeyMode mode) {
    switch (mode) {
        case gameplay::KeyMode::Keys4: return 4;
        case gameplay::KeyMode::Keys5: return 5;
        case gameplay::KeyMode::Keys6: return 6;
        case gameplay::KeyMode::Keys7: return 7;
        case gameplay::KeyMode::Keys8: return 8;
        case gameplay::KeyMode::Keys9: return 9;
        case gameplay::KeyMode::Keys10: return 10;
        case gameplay::KeyMode::Keys16: return 16;
        case gameplay::KeyMode::Auto: default: return 0;
    }
}

gameplay::ChartFormatMode chart_format_mode_for_chart(ChartFormat format) {
    switch (format) {
        case ChartFormat::Bms: return gameplay::ChartFormatMode::Bms;
        case ChartFormat::OsuMania: return gameplay::ChartFormatMode::Osu;
        case ChartFormat::Unknown: default: return gameplay::ChartFormatMode::Auto;
    }
}

gameplay::KeyMode key_mode_for_lane_count(int lane_count) {
    switch (lane_count) {
        case 4: return gameplay::KeyMode::Keys4;
        case 5: return gameplay::KeyMode::Keys5;
        case 6: return gameplay::KeyMode::Keys6;
        case 7: return gameplay::KeyMode::Keys7;
        case 8: return gameplay::KeyMode::Keys8;
        case 9: return gameplay::KeyMode::Keys9;
        case 10: return gameplay::KeyMode::Keys10;
        case 16: return gameplay::KeyMode::Keys16;
        default: return gameplay::KeyMode::Auto;
    }
}

int64_t ms_to_samples(double ms, int sample_rate) {
    if (sample_rate <= 0) {
        return 0;
    }
    return static_cast<int64_t>(std::llround(ms * static_cast<double>(sample_rate) / 1000.0));
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

float soft_limit_sample(float sample) {
    const float abs_sample = std::abs(sample);
    if (abs_sample <= kOutputSoftLimitThreshold) {
        return sample;
    }

    const float excess = abs_sample - kOutputSoftLimitThreshold;
    const float compressed = kOutputSoftLimitThreshold +
                             (1.0f - kOutputSoftLimitThreshold) * (excess / (1.0f + excess));
    return std::copysign(std::min(1.0f, compressed), sample);
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

bool resample_stereo_linear(const std::vector<float>& source,
                            int source_sample_rate,
                            int target_sample_rate,
                            std::vector<float>& out,
                            std::string* error) {
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

    const std::size_t out_frames = std::max<std::size_t>(
        1,
        static_cast<std::size_t>(std::llround(static_cast<double>(frame_count) * ratio)));
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
                                 std::string* error);
bool decode_ogg_stereo_resampled(const std::string& path, int target_sample_rate, std::vector<float>& out,
                                 std::string* error);

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
                                    std::string* error) {
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
                           input_path.wstring() + L"\" -f wav -ac 2 -ar " +
                           std::to_wstring(target_sample_rate) + L" \"" + temp_path.wstring() + L"\"";

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
        success = decode_wav_stereo_resampled(temp_path.u8string(), target_sample_rate, out, &wav_error);
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
                                std::string* error) {
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
        const std::size_t frame_count = static_cast<std::size_t>(cur_len) / frame_stride;
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
    }

    return resample_stereo_linear(source, static_cast<int>(source_sample_rate), target_sample_rate, out, error);
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

std::optional<std::string> select_primary_audio_path(ChartFormat format, const gameplay::GameplayChart& chart) {
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

    if (format == ChartFormat::OsuMania && !chart.audio_cues.empty()) {
        if (const std::string* path = chart.audio_asset_path(chart.audio_cues.front().asset_id)) {
            return *path;
        }
    }

    for (const auto& cue : chart.audio_cues) {
        if (const std::string* path = chart.audio_asset_path(cue.asset_id)) {
            consider_path(*path);
        }
    }
    for (const auto& note : chart.notes) {
        if (const std::string* path = chart.audio_asset_path(note.audio_asset_id)) {
            consider_path(*path);
        }
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
        if (const std::string* path = chart.audio_asset_path(note.audio_asset_id)) {
            return *path;
        }
    }
    return std::nullopt;
}

std::optional<int> detect_chart_preferred_sample_rate(ChartFormat format,
                                                      const gameplay::GameplayChart& chart,
                                                      std::string* diagnostic) {
    auto primary_path = select_primary_audio_path(format, chart);
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
                                 std::string* error) {
    std::ifstream file;
    WavFileInfo info;
    if (!open_wav_file(path, file, info, error)) {
        return false;
    }

    const uint16_t bytes_per_sample = static_cast<uint16_t>(info.bits_per_sample / 8u);
    const std::size_t frame_count = info.data_size / info.block_align;
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
    return resample_stereo_linear(source, static_cast<int>(info.sample_rate), target_sample_rate, out, error);
}

bool decode_ogg_stereo_resampled(const std::string& path, int target_sample_rate, std::vector<float>& out,
                                 std::string* error) {
    int source_sample_rate = 0;
    std::vector<float> source;
    if (!audio::decode_ogg_vorbis_stereo(path, &source_sample_rate, source, error)) {
        return false;
    }
    return resample_stereo_linear(source, source_sample_rate, target_sample_rate, out, error);
}

bool decode_audio_stereo_resampled(const std::string& path, int target_sample_rate, std::vector<float>& out,
                                   std::string* error) {
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
        if (decode_wav_stereo_resampled(path, target_sample_rate, out, &wav_error)) {
            return true;
        }
#ifdef _WIN32
        // Some WAV variants are better handled by Media Foundation.
        std::string mf_error;
        if (decode_mf_stereo_resampled(path, target_sample_rate, out, &mf_error)) {
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
        if (decode_ogg_stereo_resampled(path, target_sample_rate, out, &ogg_error)) {
            return true;
        }
#ifdef _WIN32
        std::string mf_error;
        if (decode_mf_stereo_resampled(path, target_sample_rate, out, &mf_error)) {
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
    if (decode_mf_stereo_resampled(path, target_sample_rate, out, &mf_error)) {
        return true;
    }
    std::string ffmpeg_error;
    if (decode_ffmpeg_stereo_resampled(path, target_sample_rate, out, &ffmpeg_error)) {
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

std::optional<game::GaugeType> parse_gauge_type(std::string_view value) {
    std::string token(value);
    std::transform(token.begin(), token.end(), token.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
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

std::string chart_format_token(ChartFormat format) {
    switch (format) {
        case ChartFormat::Bms: return "bms";
        case ChartFormat::OsuMania: return "osu";
        case ChartFormat::Unknown:
        default: return "unknown";
    }
}

std::string gauge_type_token(game::GaugeType type) {
    switch (type) {
        case game::GaugeType::Hard: return "hard";
        case game::GaugeType::Easy: return "easy";
        case game::GaugeType::Normal:
        default: return "normal";
    }
}

std::string clear_status_label(bool game_over, game::GaugeType final_gauge) {
    if (game_over) {
        return "FAILED";
    }
    switch (final_gauge) {
        case game::GaugeType::Hard: return "HARD CLEAR";
        case game::GaugeType::Easy: return "EASY CLEAR";
        case game::GaugeType::Normal:
        default: return "CLEAR";
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

bool GameSession::FutureQueue::push(const FutureEvent& evt) {
    std::size_t next = (head + 1) % kCapacity;
    if (next == tail) {
        return false;
    }
    data[head] = evt;
    head = next;
    return true;
}

std::optional<GameSession::FutureEvent> GameSession::FutureQueue::pop() {
    if (head == tail) {
        return std::nullopt;
    }
    auto value = data[tail];
    tail = (tail + 1) % kCapacity;
    return value;
}

GameSession::GameSession() = default;

GameSession::~GameSession() {
    shutdown();
}

bool GameSession::initialize(const CommandLineOptions& options) {
    options_ = options;
    result_ = {};
    stop_requested_.store(false, std::memory_order_release);
    finished_.store(false, std::memory_order_release);
    user_aborted_.store(false, std::memory_order_release);
    last_audio_sample_.store(0, std::memory_order_release);
    audio_timing_sequence_.store(0, std::memory_order_release);
    last_audio_timing_ = {};
    countdown_active_ = false;
    countdown_value_ = 0;
    countdown_started_ns_ = 0;
    gameplay_started_ = false;

    future_events_ = {};
    tone_voices_.clear();
    stop_chart_audio_workers();
    chart_audio_assets_.clear();
    chart_audio_events_.clear();
    chart_audio_voices_.clear();
    chart_audio_load_queue_.clear();
    chart_audio_active_until_samples_.reset();
    pending_input_events_.clear();
    hidden_hit_note_ids_.clear();
    active_holds_buffer_.clear();
    next_chart_audio_event_ = 0;
    startup_preload_budget_bytes_ = 0;
    runtime_chart_audio_budget_bytes_ = 0;
    chart_audio_decoded_bytes_ = 0;
    chart_audio_deferred_count_ = 0;
    chart_audio_eviction_count_ = 0;
    last_chart_audio_service_sample_ = (std::numeric_limits<int64_t>::min)();
    chart_audio_startup_logged_ = false;
    chart_audio_steady_state_logged_ = false;
    synthetic_tones_enabled_.store(true, std::memory_order_release);
    next_guide_note_index_ = 0;
    hud_scan_start_ = 0;
    chart_ = {};
    lane_activity_.clear();
    lane_pressed_.clear();
    pending_input_events_.reserve(64);
    last_loading_percent_ = -1;
    last_loading_stage_.clear();
    report_loading_progress(0, "Loading profile");
    if (loading_cancel_requested()) {
        return false;
    }

    const std::filesystem::path profile_dir =
#ifdef _WIN32
        std::filesystem::u8path("profiles") / std::filesystem::u8path(options.profile);
#else
        std::filesystem::path("profiles") / std::filesystem::path(options.profile);
#endif
    profile_dir_ = profile_dir.u8string();

    config::ConfigLoader config_loader;
    auto config_result = config_loader.load_profile(profile_dir_);
    if (!config_result.success()) {
        return false;
    }
    config_ = config_result.config;
    const bool migrated_config = migrate_bms_first_runtime_config(config_);

    if (config_result.used_defaults || migrated_config) {
        config_loader.save_profile(profile_dir_, config_);
    }
    report_loading_progress(12, "Loading keymap");
    if (loading_cancel_requested()) {
        return false;
    }

    config::KeymapManager keymap_manager;
    auto keymap_result = keymap_manager.load_profile(profile_dir_);
    if (!keymap_result.success()) {
        return false;
    }
    keymap_ = keymap_result.keymap;

    if (keymap_result.used_defaults) {
        keymap_manager.save_profile(profile_dir_, keymap_);
    }
    report_loading_progress(22, "Resolving chart");
    if (loading_cancel_requested()) {
        return false;
    }

    if (options.has_rate) {
        config_.speed.rate = options.rate;
    }
    if (options.has_hispeed) {
        config_.speed.hi_speed = options.hispeed;
    }
    if (options.has_autoshift) {
        config_.gauge.auto_shift = options.autoshift;
    }
    escape_keycode_ = config::KeycodeMap::to_keycode("Esc").value_or(0);
    f3_keycode_ = config::KeycodeMap::to_keycode("F3").value_or(0);
    f4_keycode_ = config::KeycodeMap::to_keycode("F4").value_or(0);
    f5_keycode_ = config::KeycodeMap::to_keycode("F5").value_or(0);
    f6_keycode_ = config::KeycodeMap::to_keycode("F6").value_or(0);

    std::string chart_path = options.chart_path;
    if (chart_path.empty()) {
        chart_path = find_first_chart(options.songs_path);
    }
    if (chart_path.empty()) {
        return false;
    }
    chart_path_ = chart_path;
    report_loading_progress(30, "Initializing input");
    if (loading_cancel_requested()) {
        return false;
    }

    input::InputThreadConfig input_config;
    input_config.backend = config_.input.rawinput ? input::InputBackend::RawInput
                                                  : input::InputBackend::Polling;
    input_config.raw_input.register_keyboard = config_.input.rawinput;
    input_config.raw_input.input_sink = true;
    input_config.raw_input.no_legacy = true;
    input_config.polling_hz = config_.input.polling_hz;

    if (!input_thread_.initialize(input_config)) {
        return false;
    }
    report_loading_progress(42, "Opening audio device");
    if (loading_cancel_requested()) {
        return false;
    }

    auto initialize_audio_session = [this](uint32_t requested_rate) -> bool {
        audio_thread_.shutdown();
        config_.audio.sample_rate = requested_rate;
        auto audio_result = audio_thread_.initialize(config_.audio, [this](float* output, uint32_t frames,
                                                                           int64_t buffer_start) {
            audio_callback(output, frames, buffer_start);
        });
        if (audio_result != audio::AudioResult::Success) {
            return false;
        }
        sample_rate_ = static_cast<int>(audio_thread_.sample_rate());
        if (sample_rate_ <= 0) {
            sample_rate_ = static_cast<int>(requested_rate);
        }
        input_offset_samples_ = ms_to_samples(config_.input_offset_ms, sample_rate_);
        return true;
    };

    auto load_chart_for_session_rate = [this](ChartLoadResult& out_result) -> bool {
        ChartLoader loader;
        out_result = loader.load(chart_path_, sample_rate_, config_.speed.rate,
                                 config_.audio_ui.bms_keysound_policy,
                                 config_.mode.enable_osu_charts);
        for (const auto& message : out_result.messages) {
            std::cerr << "[warn] " << message << std::endl;
        }
        return out_result.success();
    };

    const uint32_t initial_requested_rate = config_.audio.sample_rate;
    if (!initialize_audio_session(initial_requested_rate)) {
        return false;
    }
    if (loading_cancel_requested()) {
        return false;
    }

    report_loading_progress(56, "Parsing chart");
    if (loading_cancel_requested()) {
        return false;
    }
    ChartLoadResult chart_result;
    if (!load_chart_for_session_rate(chart_result)) {
        return false;
    }
    log_memory_phase("GameSession",
                     "chart-parse",
                     query_process_memory_snapshot(),
                     "asset_count=" + std::to_string(chart_result.chart.audio_assets.size()) +
                         " note_count=" + std::to_string(chart_result.chart.notes.size()));

    report_loading_progress(68, "Matching chart sample rate");
    if (loading_cancel_requested()) {
        return false;
    }
    std::string preferred_rate_diagnostic;
    const auto preferred_rate = detect_chart_preferred_sample_rate(chart_result.format,
                                                                   chart_result.chart,
                                                                   &preferred_rate_diagnostic);
    if (preferred_rate.has_value() && *preferred_rate != sample_rate_) {
        const int previous_actual_rate = sample_rate_;
        std::cerr << "[info] Detected chart audio sample rate " << *preferred_rate
                  << " Hz. Reinitializing gameplay audio for this chart." << std::endl;
        if (!initialize_audio_session(static_cast<uint32_t>(*preferred_rate))) {
            std::cerr << "[warn] Failed to switch gameplay audio to " << *preferred_rate
                      << " Hz. Falling back to " << previous_actual_rate << " Hz." << std::endl;
            if (!initialize_audio_session(initial_requested_rate)) {
                return false;
            }
        } else {
            const uint32_t device_mix_rate = audio_thread_.device_mix_sample_rate();
            if (!audio_thread_.is_exclusive() && device_mix_rate > 0 &&
                device_mix_rate != static_cast<uint32_t>(sample_rate_)) {
                std::cerr << "[info] Gameplay audio stream running at " << sample_rate_
                          << " Hz (shared-mode device mix " << device_mix_rate << " Hz)." << std::endl;
            } else {
                std::cerr << "[info] Gameplay audio stream running at " << sample_rate_ << " Hz." << std::endl;
            }
            if (sample_rate_ != previous_actual_rate && !load_chart_for_session_rate(chart_result)) {
                return false;
            }
        }
    } else if (!preferred_rate.has_value() && !preferred_rate_diagnostic.empty()) {
        std::cerr << "[warn] " << preferred_rate_diagnostic << std::endl;
    }
    if (loading_cancel_requested()) {
        return false;
    }

    chart_format_ = chart_result.format;
    report_loading_progress(78, "Applying gameplay mode");
    if (loading_cancel_requested()) {
        return false;
    }

    auto mode_result = resolve_mode_settings(config_.mode);
    for (const auto& warning : mode_result.warnings) {
        std::cerr << "[warn] " << warning << std::endl;
    }

    if (mode_result.settings.format == gameplay::ChartFormatMode::Bms &&
        chart_result.format != ChartFormat::Bms) {
        std::cerr << "[warn] mode.format=BMS does not match the selected chart. Using detected chart format instead."
                  << std::endl;
        mode_result.settings.format = chart_format_mode_for_chart(chart_result.format);
    }
    if (mode_result.settings.format == gameplay::ChartFormatMode::Osu &&
        chart_result.format != ChartFormat::OsuMania) {
        std::cerr << "[warn] mode.format=OSU does not match the selected chart. Using detected chart format instead."
                  << std::endl;
        mode_result.settings.format = chart_format_mode_for_chart(chart_result.format);
    }

    if (chart_result.format == ChartFormat::OsuMania) {
        const int chart_lane_count = std::max(1, chart_result.chart.lane_count);
        const int configured_lane_count = target_lane_count(mode_result.settings.key_mode);
        if (configured_lane_count > 0 && configured_lane_count != chart_lane_count) {
            std::cerr << "[warn] mode.key_mode does not match the selected osu!mania chart. "
                      << "Using the chart lane count instead." << std::endl;
            mode_result.settings.key_mode = key_mode_for_lane_count(chart_lane_count);
        }
    }

    auto applied = gameplay::apply_mode_settings(chart_result.chart, mode_result.settings);
    for (const auto& warning : applied.warnings) {
        std::cerr << "[warn] " << warning << std::endl;
    }
    chart_result.chart = std::move(applied.chart);

    gameplay::GameplayConfig gameplay_config;
    gameplay_config.sample_rate = sample_rate_;
    gameplay_config.rate = config_.speed.rate;
    gameplay_config.judge = config_.judge;
    gameplay_config.gauge = config_.gauge;
    gameplay_config.input_offset_ms = config_.input_offset_ms;
    switch (mode_result.settings.gauge) {
        case gameplay::GaugeMode::Hard:
            gameplay_config.initial_gauge = game::GaugeType::Hard;
            break;
        case gameplay::GaugeMode::Easy:
            gameplay_config.initial_gauge = game::GaugeType::Easy;
            break;
        case gameplay::GaugeMode::Normal:
        default:
            gameplay_config.initial_gauge = game::GaugeType::Normal;
            break;
    }
    if (options.has_gauge) {
        auto gauge = parse_gauge_type(options.gauge);
        if (gauge.has_value()) {
            gameplay_config.initial_gauge = gauge.value();
        }
    }

    chart_ = std::move(chart_result.chart);
    for (std::size_t i = 0; i < chart_.notes.size(); ++i) {
        chart_.notes[i].note_id = i;
    }
    hidden_hit_note_ids_.assign(chart_.notes.size(), 0);
    active_holds_buffer_.clear();
    key_to_lane_.clear();
    config::KeymapManager keymap_manager_runtime;
    const std::string active_key_mode =
        keymap_manager_runtime.normalize_mode_token(std::to_string(std::max(1, chart_.lane_count)) + "k");
    for (const auto& [lane, key] : keymap_manager_runtime.bindings_for_mode(keymap_, active_key_mode)) {
        auto lane_index = parse_lane_index(lane);
        if (!lane_index.has_value()) {
            continue;
        }
        auto keycode = config::KeycodeMap::to_keycode(key);
        if (!keycode.has_value()) {
            continue;
        }
        key_to_lane_[keycode.value()] = lane_index.value();
    }
    report_loading_progress(88, "Preparing chart audio");
    if (loading_cancel_requested()) {
        return false;
    }
    if (!prepare_chart_audio()) {
        return false;
    }
    next_guide_note_index_ = 0;
    hud_scan_start_ = 0;
    tone_voices_.reserve(std::max<std::size_t>(64, chart_.notes.size() / 8));
    lane_activity_.assign(static_cast<std::size_t>(std::max(1, chart_.lane_count)), 0.0f);
    lane_pressed_.assign(static_cast<std::size_t>(std::max(1, chart_.lane_count)), 0);

    engine_ = std::make_unique<gameplay::GameplayEngine>(chart_, gameplay_config);
    report_loading_progress(96, "Starting gameplay");
    if (loading_cancel_requested()) {
        return false;
    }

    if (!input_thread_.start()) {
        return false;
    }

    countdown_active_ = true;
    countdown_value_ = static_cast<int>(kGameplayStartCountdownSeconds);
    report_loading_progress(100, "Ready");
    return true;
}

void GameSession::run() {
    auto next_hud_tick = std::chrono::steady_clock::now();

    if (countdown_active_ && !stop_requested_.load(std::memory_order_acquire)) {
        countdown_started_ns_ = timing::HighResClock::now_ns();
        const int64_t countdown_duration_ns = kGameplayStartCountdownSeconds * 1'000'000'000LL;
        while (!stop_requested_.load(std::memory_order_acquire)) {
            if (finished_.load(std::memory_order_acquire)) {
                break;
            }

            process_countdown_input_queue();
            if (finished_.load(std::memory_order_acquire) || stop_requested_.load(std::memory_order_acquire)) {
                break;
            }

            const int64_t now_ns = timing::HighResClock::now_ns();
            const int64_t elapsed_ns = std::max<int64_t>(0, now_ns - countdown_started_ns_);
            const int64_t remaining_ns = std::max<int64_t>(0, countdown_duration_ns - elapsed_ns);
            if (remaining_ns <= 0) {
                countdown_active_ = false;
                countdown_value_ = 0;
                break;
            }

            countdown_value_ = std::clamp(
                static_cast<int>((remaining_ns + 999'999'999LL) / 1'000'000'000LL),
                1,
                static_cast<int>(kGameplayStartCountdownSeconds));

            const auto now = std::chrono::steady_clock::now();
            if (now >= next_hud_tick) {
                if (hud_callback_) {
                    hud_callback_(hud_snapshot());
                }
                next_hud_tick += std::chrono::milliseconds(kHudRefreshMs);
                if (next_hud_tick < now) {
                    next_hud_tick = now + std::chrono::milliseconds(kHudRefreshMs);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    if (!stop_requested_.load(std::memory_order_acquire) && !finished_.load(std::memory_order_acquire)) {
        if (audio_thread_.start() != audio::AudioResult::Success) {
            std::cerr << "[error] Failed to start gameplay audio." << std::endl;
            stop_requested_.store(true, std::memory_order_release);
            finished_.store(true, std::memory_order_release);
        } else {
            gameplay_started_ = true;
        }
    }

    while (!stop_requested_.load(std::memory_order_acquire)) {
        if (finished_.load(std::memory_order_acquire)) {
            break;
        }

        service_chart_audio_streaming(last_audio_sample_.load(std::memory_order_acquire));

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_hud_tick) {
            if (hud_callback_) {
                hud_callback_(hud_snapshot());
            }
            next_hud_tick += std::chrono::milliseconds(kHudRefreshMs);
            if (next_hud_tick < now) {
                next_hud_tick = now + std::chrono::milliseconds(kHudRefreshMs);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (hud_callback_) {
        hud_callback_(hud_snapshot());
    }
}

void GameSession::set_hud_callback(HudCallback callback) {
    hud_callback_ = std::move(callback);
}

void GameSession::set_loading_progress_callback(LoadingProgressCallback callback) {
    loading_progress_callback_ = std::move(callback);
}

void GameSession::set_loading_cancel_callback(LoadingCancelCallback callback) {
    loading_cancel_callback_ = std::move(callback);
}

void GameSession::report_loading_progress(int percent, std::string_view stage) {
    const int clamped_percent = std::clamp(percent, 0, 100);
    if (last_loading_percent_ == clamped_percent && last_loading_stage_ == stage) {
        return;
    }

    last_loading_percent_ = clamped_percent;
    last_loading_stage_ = std::string(stage);
    if (!loading_progress_callback_) {
        return;
    }

    LoadingProgress progress;
    progress.percent = clamped_percent;
    progress.stage = last_loading_stage_;
    loading_progress_callback_(progress);
}

bool GameSession::loading_cancel_requested() {
    if (user_aborted_.load(std::memory_order_acquire)) {
        return true;
    }
    if (!loading_cancel_callback_ || !loading_cancel_callback_()) {
        return false;
    }

    (void)user_aborted_.exchange(true, std::memory_order_acq_rel);
    return true;
}

GameSession::HudSnapshot GameSession::hud_snapshot() {
    HudSnapshot snapshot;
    snapshot.finished = finished_.load(std::memory_order_acquire);
    snapshot.user_aborted = user_aborted_.load(std::memory_order_acquire);
    snapshot.sample_rate = sample_rate_;
    snapshot.hud_publish_time_ns = timing::HighResClock::now_ns();
    snapshot.countdown_active = countdown_active_;
    snapshot.countdown_value = countdown_value_;

    for (;;) {
        const uint64_t begin = audio_timing_sequence_.load(std::memory_order_acquire);
        if ((begin & 1u) != 0u) {
            continue;
        }

        const AudioTimingState timing = last_audio_timing_;
        const uint64_t end = audio_timing_sequence_.load(std::memory_order_acquire);
        if (begin != end) {
            continue;
        }

        snapshot.current_sample = timing.sample;
        snapshot.audio_sample_time_ns = timing.time_ns;
        snapshot.audio_buffer_frames = timing.buffer_frames;
        break;
    }

    {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        if (!engine_) {
            return snapshot;
        }

        snapshot.active = true;
        snapshot.finished = snapshot.finished || engine_->is_finished();
        snapshot.game_over = engine_->is_game_over();
        snapshot.rate = config_.speed.rate;
        snapshot.hispeed = config_.speed.hi_speed;
        snapshot.lane_count = std::max(1, engine_->lane_count());
        snapshot.duration_samples = engine_->duration_samples();

        const auto& stats = engine_->stats();
        snapshot.combo = stats.combo;
        snapshot.max_combo = stats.max_combo;
        snapshot.counts = stats.counts;

        const auto& gauge_state = engine_->gauge_state();
        snapshot.gauge = gauge_state.value;
        snapshot.gauge_type = gauge_state.type;

        const auto& feedback = engine_->live_feedback();
        snapshot.has_feedback = feedback.has_value;
        snapshot.feedback_judgement = feedback.judgement;
        snapshot.feedback_delta_ms = feedback.delta_ms;
        engine_->collect_active_holds(active_holds_buffer_);
        snapshot.lane_activity.fill(0.0f);
        snapshot.lane_activity_count = std::min<std::size_t>(lane_activity_.size(), kGameplayHudMaxLanes);
        std::copy_n(lane_activity_.begin(), snapshot.lane_activity_count, snapshot.lane_activity.begin());
    }

    const double safe_rate =
        (std::isfinite(snapshot.rate) && snapshot.rate > 0.01) ? snapshot.rate : 1.0;
    const double safe_hispeed =
        (std::isfinite(snapshot.hispeed) && snapshot.hispeed > 0.01) ? snapshot.hispeed : 3.0;
    const double scroll_scale = safe_hispeed / safe_rate;
    const double normalized_scale = std::max(0.1, scroll_scale / 3.0);
    const double dynamic_lookahead_ms = std::clamp(
        static_cast<double>(kHudLookaheadMs) / normalized_scale,
        350.0,
        6000.0);

    const int64_t past_samples = ms_to_samples(static_cast<double>(kHudPastMs), sample_rate_);
    const int64_t lookahead_samples = ms_to_samples(dynamic_lookahead_ms, sample_rate_);
    const GameplayHudWindow expanded_window = expand_gameplay_hud_window(
        sample_rate_,
        past_samples,
        lookahead_samples,
        config_.visual_offset_ms,
        kHudRenderSlackMs);
    snapshot.past_samples = past_samples;
    snapshot.lookahead_samples = lookahead_samples;
    snapshot.lane_activity_count = std::max<std::size_t>(
        snapshot.lane_activity_count,
        std::min<std::size_t>(static_cast<std::size_t>(snapshot.lane_count), kGameplayHudMaxLanes));

    if (hud_scan_start_ >= chart_.notes.size()) {
        hud_scan_start_ = chart_.notes.size();
    }
    while (hud_scan_start_ < chart_.notes.size() &&
           note_is_expired_for_hud(chart_.notes[hud_scan_start_], snapshot.current_sample,
                                   expanded_window.past_samples)) {
        ++hud_scan_start_;
    }

    snapshot.note_count = 0;
    for (std::size_t i = hud_scan_start_; i < chart_.notes.size(); ++i) {
        const auto& note = chart_.notes[i];
        if (note.note_id < hidden_hit_note_ids_.size() && hidden_hit_note_ids_[note.note_id] != 0) {
            continue;
        }
        if (note.start_sample > snapshot.current_sample + expanded_window.lookahead_samples) {
            break;
        }
        if (note.lane <= 0 || note.lane > snapshot.lane_count) {
            continue;
        }

        HudNote hud_note;
        hud_note.lane = note.lane;
        hud_note.start_sample = note.start_sample;
        hud_note.tail_sample = note_visible_end_sample(note);
        hud_note.hold = note.end_sample.has_value();
        hud_note.head_visible = true;
        snapshot.notes[snapshot.note_count++] = hud_note;
        if (snapshot.note_count >= kGameplayHudMaxNotes) {
            break;
        }
    }

    for (const auto& hold : active_holds_buffer_) {
        if (snapshot.note_count >= kGameplayHudMaxNotes) {
            break;
        }
        if (hold.lane <= 0 || hold.lane > snapshot.lane_count) {
            continue;
        }
        if (hold.end_sample < snapshot.current_sample - expanded_window.past_samples) {
            continue;
        }

        HudNote hud_note;
        hud_note.lane = hold.lane;
        hud_note.start_sample = snapshot.current_sample;
        hud_note.tail_sample = std::max(hold.end_sample, snapshot.current_sample);
        hud_note.hold = true;
        hud_note.head_visible = false;
        snapshot.notes[snapshot.note_count++] = hud_note;
    }

    std::sort(snapshot.notes.begin(),
              snapshot.notes.begin() + static_cast<std::ptrdiff_t>(snapshot.note_count),
              [](const HudNote& lhs, const HudNote& rhs) {
                  if (lhs.start_sample != rhs.start_sample) {
                      return lhs.start_sample < rhs.start_sample;
                  }
                  if (lhs.tail_sample != rhs.tail_sample) {
                      return lhs.tail_sample < rhs.tail_sample;
                  }
                  return lhs.lane < rhs.lane;
              });

    return snapshot;
}

void GameSession::adjust_hispeed(double delta) {
    if (!std::isfinite(delta) || std::abs(delta) < 1e-9) {
        return;
    }
    if (!engine_) {
        return;
    }

    double next = std::clamp(config_.speed.hi_speed + delta, kHispeedMin, kHispeedMax);
    next = std::round(next * 100.0) / 100.0;
    config_.speed.hi_speed = next;
}

bool GameSession::prepare_chart_audio() {
    stop_chart_audio_workers();
    chart_audio_assets_.clear();
    chart_audio_events_.clear();
    chart_audio_voices_.clear();
    chart_audio_load_queue_.clear();
    chart_audio_active_until_samples_.reset();
    next_chart_audio_event_ = 0;
    startup_preload_budget_bytes_ = 0;
    runtime_chart_audio_budget_bytes_ = 0;
    chart_audio_decoded_bytes_ = 0;
    chart_audio_deferred_count_ = 0;
    chart_audio_eviction_count_ = 0;
    last_chart_audio_service_sample_ = (std::numeric_limits<int64_t>::min)();
    chart_audio_startup_logged_ = false;
    chart_audio_steady_state_logged_ = false;
    synthetic_tones_enabled_.store(true, std::memory_order_release);

    if (loading_cancel_requested()) {
        return false;
    }

    const std::size_t asset_count = chart_.audio_assets.size();
    if (asset_count == 0) {
        return true;
    }

    chart_audio_assets_.resize(asset_count);
    chart_audio_active_until_samples_ = std::make_unique<std::atomic<int64_t>[]>(asset_count);
    for (std::size_t i = 0; i < asset_count; ++i) {
        chart_audio_assets_[i].path = chart_.audio_assets[i].path;
        chart_audio_assets_[i].estimated_decoded_bytes = estimate_audio_decoded_bytes(chart_.audio_assets[i].path,
                                                                                      sample_rate_);
        chart_audio_active_until_samples_[i].store(0, std::memory_order_release);
    }

    int64_t max_sample = chart_.duration_samples;
    chart_audio_events_.reserve(chart_.audio_cues.size());
    for (const auto& cue : chart_.audio_cues) {
        if (cue.asset_id >= chart_audio_assets_.size()) {
            continue;
        }
        auto& asset = chart_audio_assets_[cue.asset_id];
        const int64_t sample = std::max<int64_t>(0, cue.start_sample);
        asset.has_bgm = true;
        asset.use_samples.push_back(sample);
        asset.first_use_sample = (std::min)(asset.first_use_sample, sample);
        asset.last_use_sample = (std::max)(asset.last_use_sample, sample);
        ++asset.use_count;
        chart_audio_events_.push_back(ChartAudioEvent{sample, cue.asset_id, ChartAudioEvent::Kind::Bgm});
    }
    for (const auto& note : chart_.notes) {
        if (note.audio_asset_id >= chart_audio_assets_.size()) {
            continue;
        }
        auto& asset = chart_audio_assets_[note.audio_asset_id];
        const int64_t sample = std::max<int64_t>(0, note.start_sample);
        asset.has_keysound = true;
        asset.use_samples.push_back(sample);
        asset.first_use_sample = (std::min)(asset.first_use_sample, sample);
        asset.last_use_sample = (std::max)(asset.last_use_sample, sample);
        ++asset.use_count;
    }

    for (auto& asset : chart_audio_assets_) {
        std::stable_sort(asset.use_samples.begin(), asset.use_samples.end());
    }
    std::stable_sort(chart_audio_events_.begin(), chart_audio_events_.end(),
                     [](const ChartAudioEvent& lhs, const ChartAudioEvent& rhs) {
                         if (lhs.start_sample != rhs.start_sample) {
                             return lhs.start_sample < rhs.start_sample;
                         }
                         return lhs.asset_id < rhs.asset_id;
                     });

    const auto budgets = choose_chart_audio_budgets(query_system_memory_snapshot());
    startup_preload_budget_bytes_ = budgets.startup_preload_bytes;
    runtime_chart_audio_budget_bytes_ = budgets.runtime_cache_bytes;

    const std::size_t worker_count = (std::min)(static_cast<std::size_t>(2u), chart_audio_assets_.size());
    start_chart_audio_workers(worker_count);

    const int64_t startup_window_samples = ms_to_samples(static_cast<double>(kChartAudioStartupWindowMs), sample_rate_);
    std::vector<ChartAudioStartupCandidate> startup_candidates(asset_count);
    for (std::size_t asset_id = 0; asset_id < asset_count; ++asset_id) {
        const auto& asset = chart_audio_assets_[asset_id];
        startup_candidates[asset_id].first_use_sample = asset.first_use_sample;
        startup_candidates[asset_id].has_bgm = asset.has_bgm;
        startup_candidates[asset_id].estimated_decoded_bytes = asset.estimated_decoded_bytes;
        startup_candidates[asset_id].use_count = asset.use_count;
    }
    const auto startup_plan =
        build_chart_audio_startup_plan(startup_candidates, startup_window_samples, startup_preload_budget_bytes_);
    chart_audio_deferred_count_ = startup_plan.deferred_count;
    {
        std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
        for (std::size_t asset_id = 0; asset_id < startup_plan.queued_assets.size(); ++asset_id) {
            if (startup_plan.queued_assets[asset_id] == 0) {
                continue;
            }
            auto& asset = chart_audio_assets_[asset_id];
            if (asset.state == ChartAudioAssetState::Unloaded) {
                asset.state = ChartAudioAssetState::Queued;
                chart_audio_load_queue_.push_back(asset_id);
            }
        }
    }
    chart_audio_stream_cv_.notify_all();

    if (!wait_for_chart_audio_startup(startup_plan.required_assets)) {
        stop_chart_audio_workers();
        chart_audio_assets_.clear();
        chart_audio_events_.clear();
        chart_audio_voices_.clear();
        chart_audio_active_until_samples_.reset();
        return false;
    }

    for (const auto& event : chart_audio_events_) {
        if (event.asset_id >= chart_audio_assets_.size()) {
            continue;
        }
        const auto& asset = chart_audio_assets_[event.asset_id];
        auto samples = std::atomic_load_explicit(&asset.clip.samples, std::memory_order_acquire);
        const int64_t source_frames = (samples && !samples->empty())
                                          ? static_cast<int64_t>(samples->size() / 2u)
                                          : static_cast<int64_t>(asset.estimated_decoded_bytes /
                                                                 (2u * sizeof(float)));
        const int64_t playback_frames =
            chart_audio_playback_duration_frames(source_frames, config_.speed.rate);
        if (playback_frames > 0) {
            max_sample = std::max(max_sample, event.start_sample + playback_frames);
        }
    }

    chart_.duration_samples = std::max(chart_.duration_samples, max_sample);
    synthetic_tones_enabled_.store(true, std::memory_order_release);
    for (const auto& asset : chart_audio_assets_) {
        auto samples = std::atomic_load_explicit(&asset.clip.samples, std::memory_order_acquire);
        if (samples && !samples->empty()) {
            synthetic_tones_enabled_.store(false, std::memory_order_release);
            break;
        }
    }

    log_chart_audio_memory("startup-preload");
    chart_audio_startup_logged_ = true;
    return true;
}

void GameSession::start_chart_audio_workers(std::size_t worker_count) {
    stop_chart_audio_workers();
    if (worker_count == 0) {
        return;
    }

    chart_audio_loader_stop_ = false;
    chart_audio_loader_threads_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        chart_audio_loader_threads_.emplace_back(&GameSession::chart_audio_loader_thread_main, this);
    }
}

void GameSession::stop_chart_audio_workers() {
    {
        std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
        chart_audio_loader_stop_ = true;
        chart_audio_load_queue_.clear();
    }
    chart_audio_stream_cv_.notify_all();
    for (auto& thread : chart_audio_loader_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    chart_audio_loader_threads_.clear();
    chart_audio_loader_stop_ = false;
}

void GameSession::chart_audio_loader_thread_main() {
    for (;;) {
        std::size_t asset_id = 0;
        std::string path;
        {
            std::unique_lock<std::mutex> lock(chart_audio_stream_mutex_);
            chart_audio_stream_cv_.wait(lock, [this]() {
                return chart_audio_loader_stop_ || !chart_audio_load_queue_.empty();
            });
            if (chart_audio_loader_stop_) {
                return;
            }

            asset_id = chart_audio_load_queue_.front();
            chart_audio_load_queue_.pop_front();
            if (asset_id >= chart_audio_assets_.size()) {
                continue;
            }
            auto& asset = chart_audio_assets_[asset_id];
            if (asset.state != ChartAudioAssetState::Queued) {
                continue;
            }
            asset.state = ChartAudioAssetState::Loading;
            path = asset.path;
        }

        std::vector<float> decoded;
        std::string error;
        const bool success = decode_audio_stereo_resampled(path, sample_rate_, decoded, &error);
        const std::uint64_t decoded_bytes = static_cast<std::uint64_t>(decoded.size()) * sizeof(float);
        auto clip_samples = std::make_shared<const std::vector<float>>(std::move(decoded));

        {
            std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
            if (asset_id >= chart_audio_assets_.size()) {
                continue;
            }
            auto& asset = chart_audio_assets_[asset_id];
            if (asset.state != ChartAudioAssetState::Loading) {
                continue;
            }

            if (!success || !clip_samples || clip_samples->empty()) {
                asset.state = ChartAudioAssetState::Failed;
                std::atomic_store_explicit(&asset.clip.samples,
                                           std::shared_ptr<const std::vector<float>>{},
                                           std::memory_order_release);
                asset.decoded_bytes = 0;
                if (!success) {
                    std::cerr << "[warn] Failed to load audio cue '" << path << "': " << error << std::endl;
                }
            } else {
                chart_audio_decoded_bytes_ += decoded_bytes;
                asset.decoded_bytes = decoded_bytes;
                std::atomic_store_explicit(&asset.clip.samples, clip_samples, std::memory_order_release);
                asset.state = ChartAudioAssetState::Ready;
                synthetic_tones_enabled_.store(false, std::memory_order_release);
            }
        }
        chart_audio_stream_cv_.notify_all();
    }
}

bool GameSession::wait_for_chart_audio_startup(const std::vector<uint8_t>& required_assets) {
    if (required_assets.empty()) {
        return true;
    }

    std::unique_lock<std::mutex> lock(chart_audio_stream_mutex_);
    for (;;) {
        if (loading_cancel_requested()) {
            return false;
        }

        bool pending_required = false;
        for (std::size_t asset_id = 0; asset_id < required_assets.size(); ++asset_id) {
            if (required_assets[asset_id] == 0 || asset_id >= chart_audio_assets_.size()) {
                continue;
            }
            const auto state = chart_audio_assets_[asset_id].state;
            if (state == ChartAudioAssetState::Queued || state == ChartAudioAssetState::Loading) {
                pending_required = true;
                break;
            }
        }
        if (!pending_required) {
            return true;
        }

        chart_audio_stream_cv_.wait_for(lock, std::chrono::milliseconds(10));
    }
}

void GameSession::log_chart_audio_memory(std::string_view phase) {
    std::size_t asset_count = 0;
    std::uint64_t decoded_bytes = 0;
    std::size_t deferred_count = 0;
    std::size_t eviction_count = 0;
    {
        std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
        asset_count = chart_audio_assets_.size();
        decoded_bytes = chart_audio_decoded_bytes_;
        deferred_count = chart_audio_deferred_count_;
        eviction_count = chart_audio_eviction_count_;
    }
    log_memory_phase("GameSession",
                     phase,
                     query_process_memory_snapshot(),
                     "asset_count=" + std::to_string(asset_count) +
                         " decoded=" + format_memory_bytes(decoded_bytes) +
                         " deferred=" + std::to_string(deferred_count) +
                         " evictions=" + std::to_string(eviction_count));
}

void GameSession::service_chart_audio_streaming(int64_t current_sample) {
    if (chart_audio_assets_.empty() || sample_rate_ <= 0) {
        return;
    }
    const int64_t min_step = ms_to_samples(static_cast<double>(kChartAudioServiceIntervalMs), sample_rate_);
    if (last_chart_audio_service_sample_ != (std::numeric_limits<int64_t>::min)() &&
        current_sample < last_chart_audio_service_sample_ + min_step) {
        return;
    }

    last_chart_audio_service_sample_ = current_sample;
    queue_chart_audio_prefetch(current_sample);
    trim_chart_audio_cache(current_sample);

    if (!chart_audio_steady_state_logged_ &&
        current_sample >= ms_to_samples(static_cast<double>(kChartAudioPrefetchWindowMs), sample_rate_)) {
        log_chart_audio_memory("steady-state-cache");
        chart_audio_steady_state_logged_ = true;
    }
}

void GameSession::queue_chart_audio_prefetch(int64_t current_sample) {
    const int64_t horizon_sample =
        current_sample + ms_to_samples(static_cast<double>(kChartAudioPrefetchWindowMs), sample_rate_);
    bool queued_any = false;

    std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
    for (std::size_t asset_id = 0; asset_id < chart_audio_assets_.size(); ++asset_id) {
        auto& asset = chart_audio_assets_[asset_id];
        while (asset.next_use_index < asset.use_samples.size() &&
               asset.use_samples[asset.next_use_index] < current_sample) {
            ++asset.next_use_index;
        }
        if (asset.next_use_index >= asset.use_samples.size()) {
            continue;
        }
        if (asset.use_samples[asset.next_use_index] > horizon_sample) {
            continue;
        }
        if (asset.state == ChartAudioAssetState::Unloaded) {
            asset.state = ChartAudioAssetState::Queued;
            chart_audio_load_queue_.push_back(asset_id);
            queued_any = true;
        }
    }

    if (queued_any) {
        chart_audio_stream_cv_.notify_all();
    }
}

void GameSession::trim_chart_audio_cache(int64_t current_sample) {
    if (chart_audio_decoded_bytes_ <= runtime_chart_audio_budget_bytes_) {
        return;
    }

    struct EvictCandidate {
        std::size_t asset_id = 0;
        int64_t next_use_sample = (std::numeric_limits<int64_t>::max)();
    };

    const int64_t horizon_sample =
        current_sample + ms_to_samples(static_cast<double>(kChartAudioPrefetchWindowMs), sample_rate_);

    std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
    auto evict_until = [&](bool include_prefetch_window) {
        std::vector<EvictCandidate> candidates;
        for (std::size_t asset_id = 0; asset_id < chart_audio_assets_.size(); ++asset_id) {
            auto& asset = chart_audio_assets_[asset_id];
            if (asset.state != ChartAudioAssetState::Ready || asset.decoded_bytes == 0) {
                continue;
            }

            while (asset.next_use_index < asset.use_samples.size() &&
                   asset.use_samples[asset.next_use_index] < current_sample) {
                ++asset.next_use_index;
            }

            const int64_t active_until = chart_audio_active_until_samples_
                                             ? chart_audio_active_until_samples_[asset_id].load(std::memory_order_acquire)
                                             : 0;
            if (active_until > current_sample) {
                continue;
            }

            const int64_t next_use =
                (asset.next_use_index < asset.use_samples.size()) ? asset.use_samples[asset.next_use_index]
                                                                  : (std::numeric_limits<int64_t>::max)();
            if (!include_prefetch_window && next_use <= horizon_sample) {
                continue;
            }
            candidates.push_back(EvictCandidate{asset_id, next_use});
        }

        std::stable_sort(candidates.begin(), candidates.end(), [](const EvictCandidate& lhs,
                                                                  const EvictCandidate& rhs) {
            if (lhs.next_use_sample != rhs.next_use_sample) {
                return lhs.next_use_sample > rhs.next_use_sample;
            }
            return lhs.asset_id < rhs.asset_id;
        });

        for (const auto& candidate : candidates) {
            if (chart_audio_decoded_bytes_ <= runtime_chart_audio_budget_bytes_) {
                break;
            }
            auto& asset = chart_audio_assets_[candidate.asset_id];
            auto samples = std::atomic_load_explicit(&asset.clip.samples, std::memory_order_acquire);
            if (!samples) {
                continue;
            }
            std::atomic_store_explicit(&asset.clip.samples,
                                       std::shared_ptr<const std::vector<float>>{},
                                       std::memory_order_release);
            chart_audio_decoded_bytes_ -= asset.decoded_bytes;
            asset.decoded_bytes = 0;
            asset.state = ChartAudioAssetState::Unloaded;
            ++chart_audio_eviction_count_;
        }
    };

    evict_until(false);
    if (chart_audio_decoded_bytes_ > runtime_chart_audio_budget_bytes_) {
        evict_until(true);
    }
}

void GameSession::schedule_note_keysound(const gameplay::NoteEvent& note, int64_t sample) {
    if (note.audio_asset_id >= chart_audio_assets_.size()) {
        return;
    }
    chart_audio_voices_.push_back(
        ChartAudioVoice{std::max<int64_t>(0, sample), note.audio_asset_id, ChartAudioEvent::Kind::Keysound});
}

void GameSession::schedule_chart_audio(int64_t buffer_end_samples) {
    while (next_chart_audio_event_ < chart_audio_events_.size()) {
        const auto& evt = chart_audio_events_[next_chart_audio_event_];
        if (evt.start_sample >= buffer_end_samples) {
            break;
        }
        chart_audio_voices_.push_back(ChartAudioVoice{evt.start_sample, evt.asset_id, evt.kind});
        ++next_chart_audio_event_;
    }
}

void GameSession::mix_chart_audio(float* output, uint32_t frames, int64_t buffer_start_samples) {
    if (!output || frames == 0 || chart_audio_voices_.empty()) {
        return;
    }

    const float bgm_gain = static_cast<float>(std::clamp(config_.audio_ui.bgm_volume, 0.0, 2.0));
    const float keysound_gain = static_cast<float>(std::clamp(config_.audio_ui.keysound_volume, 0.0, 2.0));
    const int64_t buffer_end_samples = buffer_start_samples + static_cast<int64_t>(frames);
    for (std::size_t i = 0; i < chart_audio_voices_.size();) {
        const auto& voice = chart_audio_voices_[i];
        if (voice.asset_id >= chart_audio_assets_.size()) {
            chart_audio_voices_.erase(chart_audio_voices_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        const auto& asset = chart_audio_assets_[voice.asset_id];
        auto samples = std::atomic_load_explicit(&asset.clip.samples, std::memory_order_acquire);
        if (!samples || samples->empty()) {
            const int64_t estimated_source_frames =
                static_cast<int64_t>(asset.estimated_decoded_bytes / (2u * sizeof(float)));
            const int64_t estimated_playback_frames =
                chart_audio_playback_duration_frames(estimated_source_frames, config_.speed.rate);
            if (estimated_playback_frames > 0 &&
                voice.start_sample + estimated_playback_frames <= buffer_start_samples) {
                chart_audio_voices_.erase(chart_audio_voices_.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
            ++i;
            continue;
        }

        const int64_t source_frames = static_cast<int64_t>(samples->size() / 2u);
        const int64_t playback_frames =
            chart_audio_playback_duration_frames(source_frames, config_.speed.rate);
        const int64_t active_until = voice.start_sample + playback_frames;
        if (chart_audio_active_until_samples_) {
            auto& slot = chart_audio_active_until_samples_[voice.asset_id];
            int64_t observed = slot.load(std::memory_order_acquire);
            while (observed < active_until &&
                   !slot.compare_exchange_weak(observed, active_until, std::memory_order_acq_rel)) {
            }
        }

        const int64_t clip_end = voice.start_sample + playback_frames;
        if (clip_end <= buffer_start_samples) {
            chart_audio_voices_.erase(chart_audio_voices_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        const float gain = (voice.kind == ChartAudioEvent::Kind::Keysound) ? keysound_gain : bgm_gain;
        (void)mix_chart_audio_clip_linear(*samples,
                                          voice.start_sample,
                                          config_.speed.rate,
                                          gain,
                                          output,
                                          frames,
                                          buffer_start_samples);

        if (clip_end <= buffer_end_samples) {
            chart_audio_voices_.erase(chart_audio_voices_.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
}

void GameSession::clamp_output(float* output, uint32_t frames, float master_gain) {
    if (!output || frames == 0) {
        return;
    }
    const std::size_t sample_count = static_cast<std::size_t>(frames) * 2;
    for (std::size_t i = 0; i < sample_count; ++i) {
        output[i] = soft_limit_sample(output[i] * master_gain);
    }
}

void GameSession::shutdown() {
    stop_requested_.store(true, std::memory_order_release);
    audio_thread_.stop();
    input_thread_.stop();
    stop_chart_audio_workers();
    if (engine_ && gameplay_started_) {
        {
            std::lock_guard<std::mutex> lock(engine_mutex_);
            result_.stats = engine_->stats();
            result_.game_over = engine_->is_game_over();
            result_.finished = engine_->is_finished();
        }
        result_.has_value = true;

        const std::string created_utc = utc_timestamp_compact();
        const std::string format_token = chart_format_token(chart_format_);

        std::filesystem::path profile_dir(profile_dir_);
        std::filesystem::path replay_path = profile_dir / "replays" / ("replay_" + created_utc + ".json");
        std::filesystem::path result_path = profile_dir / "results" / ("result_" + created_utc + ".json");

        gameplay::ReplayFile replay;
        replay.chart_path = chart_path_;
        replay.chart_format = format_token;
        replay.created_utc = created_utc;
        replay.trace = engine_->replay();
        replay.sample_rate = sample_rate_;
        replay.rate = replay.trace.rate;
        replay.input_offset_ms = config_.input_offset_ms;
        replay.stats = result_.stats;

        auto replay_export = gameplay::save_replay_json(replay_path.u8string(), replay);
        if (!replay_export.success()) {
            if (!replay_export.error.empty()) {
                result_.export_warnings.push_back("Replay export failed: " + replay_export.error);
            }
            result_.export_warnings.insert(result_.export_warnings.end(),
                                           replay_export.warnings.begin(),
                                           replay_export.warnings.end());
        } else {
            result_.replay_path = replay_path.u8string();
            result_.export_warnings.insert(result_.export_warnings.end(),
                                           replay_export.warnings.begin(),
                                           replay_export.warnings.end());
        }

        gameplay::ResultFile exported_result;
        const game::GaugeType final_gauge = engine_->gauge_state().type;
        exported_result.chart_path = chart_path_;
        exported_result.chart_format = format_token;
        exported_result.created_utc = created_utc;
        exported_result.replay_path = result_.replay_path;
        exported_result.clear_status = clear_status_label(result_.game_over, final_gauge);
        exported_result.final_gauge = gauge_type_token(final_gauge);
        exported_result.sample_rate = sample_rate_;
        exported_result.rate = replay.rate;
        exported_result.game_over = result_.game_over;
        exported_result.stats = result_.stats;

        auto result_export = gameplay::save_result_json(result_path.u8string(), exported_result);
        if (!result_export.success()) {
            if (!result_export.error.empty()) {
                result_.export_warnings.push_back("Result export failed: " + result_export.error);
            }
            result_.export_warnings.insert(result_.export_warnings.end(),
                                           result_export.warnings.begin(),
                                           result_export.warnings.end());
        } else {
            result_.result_path = result_path.u8string();
            result_.export_warnings.insert(result_.export_warnings.end(),
                                           result_export.warnings.begin(),
                                           result_export.warnings.end());
        }
    }
    engine_.reset();
    countdown_active_ = false;
    countdown_value_ = 0;
    countdown_started_ns_ = 0;
    gameplay_started_ = false;
    tone_voices_.clear();
    chart_audio_assets_.clear();
    chart_audio_events_.clear();
    chart_audio_voices_.clear();
    chart_audio_load_queue_.clear();
    chart_audio_active_until_samples_.reset();
    next_chart_audio_event_ = 0;
    startup_preload_budget_bytes_ = 0;
    runtime_chart_audio_budget_bytes_ = 0;
    chart_audio_decoded_bytes_ = 0;
    chart_audio_deferred_count_ = 0;
    chart_audio_eviction_count_ = 0;
    last_chart_audio_service_sample_ = (std::numeric_limits<int64_t>::min)();
    chart_audio_startup_logged_ = false;
    chart_audio_steady_state_logged_ = false;
    synthetic_tones_enabled_.store(true, std::memory_order_release);
    chart_ = {};
    lane_activity_.clear();
    lane_pressed_.clear();
    hidden_hit_note_ids_.clear();
    active_holds_buffer_.clear();
    hud_scan_start_ = 0;
    hud_callback_ = nullptr;
}

void GameSession::audio_callback(float* output, uint32_t frames, int64_t buffer_start_samples) {
    if (output && frames > 0) {
        std::fill(output, output + frames * 2, 0.0f);
    }

    bool engine_active = false;
    {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        if (!engine_) {
            return;
        }
        engine_active = true;

        const int64_t buffer_end_samples = buffer_start_samples + static_cast<int64_t>(frames);
        const int64_t lookahead_samples = ms_to_samples(static_cast<double>(kLookaheadMs), sample_rate_);

        const int64_t now_ns = timing::HighResClock::now_ns();
        clock_sync_.add_sample(now_ns, buffer_start_samples);
        schedule_note_guides(buffer_start_samples, buffer_end_samples);
        process_future_events(buffer_end_samples, lookahead_samples);
        process_input_queue(buffer_start_samples, buffer_end_samples, lookahead_samples);
        engine_->advance(buffer_end_samples);

        if (!lane_activity_.empty() && sample_rate_ > 0) {
            const float decay = static_cast<float>(static_cast<double>(frames) * 5.0 /
                                                   static_cast<double>(sample_rate_));
            for (std::size_t lane = 0; lane < lane_activity_.size(); ++lane) {
                if (lane < lane_pressed_.size() && lane_pressed_[lane] != 0) {
                    lane_activity_[lane] = 1.0f;
                } else {
                    lane_activity_[lane] = std::max(0.0f, lane_activity_[lane] - decay);
                }
            }
        }

        if (engine_->is_finished() || engine_->is_game_over() ||
            stop_requested_.load(std::memory_order_acquire)) {
            finished_.store(true, std::memory_order_release);
        }
    }

    if (engine_active) {
        const int64_t buffer_end_samples = buffer_start_samples + static_cast<int64_t>(frames);
        schedule_chart_audio(buffer_end_samples);
        mix_chart_audio(output, frames, buffer_start_samples);
        mix_tones(output, frames, buffer_start_samples);
        const float master_gain = static_cast<float>(std::clamp(config_.audio_ui.master_volume, 0.0, 1.0));
        clamp_output(output, frames, master_gain);
    }

    const int64_t committed_sample = buffer_start_samples + static_cast<int64_t>(frames);
    const int64_t committed_time_ns = timing::HighResClock::now_ns();
    audio_timing_sequence_.fetch_add(1, std::memory_order_acq_rel);
    last_audio_timing_.sample = committed_sample;
    last_audio_timing_.time_ns = committed_time_ns;
    last_audio_timing_.buffer_frames = frames;
    audio_timing_sequence_.fetch_add(1, std::memory_order_release);
    last_audio_sample_.store(committed_sample, std::memory_order_release);
}

void GameSession::process_countdown_input_queue() {
    while (true) {
        auto maybe_event = input_thread_.queue().pop();
        if (!maybe_event.has_value()) {
            break;
        }
        if (handle_control_input(*maybe_event) &&
            finished_.load(std::memory_order_acquire)) {
            return;
        }
    }

    std::fill(lane_pressed_.begin(), lane_pressed_.end(), 0);
    std::fill(lane_activity_.begin(), lane_activity_.end(), 0.0f);
}

bool GameSession::handle_control_input(const input::InputEvent& event) {
    if (event.state == input::InputState::Pressed) {
        if (f5_keycode_ != 0 && event.keycode == f5_keycode_) {
            adjust_hispeed(-kHispeedStepCoarse);
            return true;
        }
        if (f6_keycode_ != 0 && event.keycode == f6_keycode_) {
            adjust_hispeed(kHispeedStepCoarse);
            return true;
        }
        if (f3_keycode_ != 0 && event.keycode == f3_keycode_) {
            adjust_hispeed(-kHispeedStep);
            return true;
        }
        if (f4_keycode_ != 0 && event.keycode == f4_keycode_) {
            adjust_hispeed(kHispeedStep);
            return true;
        }
    }
    if (escape_keycode_ != 0 && event.state == input::InputState::Pressed &&
        event.keycode == escape_keycode_) {
        user_aborted_.store(true, std::memory_order_release);
        stop_requested_.store(true, std::memory_order_release);
        finished_.store(true, std::memory_order_release);
        return true;
    }
    return false;
}

void GameSession::update_lane_feedback(int lane, input::InputState state) {
    const int lane_index = lane - 1;
    if (lane_index < 0 || lane_index >= static_cast<int>(lane_pressed_.size())) {
        return;
    }

    lane_pressed_[static_cast<std::size_t>(lane_index)] = (state == input::InputState::Pressed) ? 1 : 0;
    if (state == input::InputState::Pressed && lane_index < static_cast<int>(lane_activity_.size())) {
        lane_activity_[static_cast<std::size_t>(lane_index)] = 1.0f;
    }
}

void GameSession::dispatch_lane_input(int lane, input::InputState state, int64_t sample) {
    update_lane_feedback(lane, state);
    if (state == input::InputState::Pressed) {
        schedule_tone(lane, sample, false);
    }

    auto hit_note = engine_->handle_input(lane, state, sample);
    if (state == input::InputState::Pressed && hit_note.has_value()) {
        if (hit_note->note_id < hidden_hit_note_ids_.size()) {
            hidden_hit_note_ids_[hit_note->note_id] = 1;
        }
        schedule_note_keysound(hit_note.value(), sample);
    }
}

void GameSession::catch_up_lane_input(int lane, input::InputState state, int64_t sample) {
    update_lane_feedback(lane, state);
    engine_->sync_input_state(lane, state, sample);
}

void GameSession::process_future_events(int64_t buffer_end_samples, int64_t lookahead_samples) {
    while (true) {
        auto next = future_events_.pop();
        if (!next.has_value()) {
            return;
        }
        if (handle_control_input(next->event)) {
            if (finished_.load(std::memory_order_acquire)) {
                return;
            }
            continue;
        }
        if (next->sample > buffer_end_samples + lookahead_samples) {
            next->sample = buffer_end_samples;
        }
        if (auto lane = lane_from_keycode(next->event.keycode)) {
            dispatch_lane_input(lane.value(), next->event.state, next->sample);
        }
    }
}

void GameSession::process_input_queue(int64_t buffer_start_samples, int64_t buffer_end_samples,
                                      int64_t lookahead_samples) {
    // If input falls well behind the current audio cursor, replaying every stale edge can spike the mix callback.
    const double stale_window_ms = std::max(kInputBacklogCatchupFloorMs, config_.judge.indirect_miss_ms);
    const int64_t stale_before_sample = buffer_start_samples - ms_to_samples(stale_window_ms, sample_rate_);
    std::array<BufferedLaneInput, kGameplayHudMaxLanes> stale_lane_inputs{};
    std::array<uint8_t, kGameplayHudMaxLanes> stale_lane_present{};
    pending_input_events_.clear();

    while (true) {
        auto maybe_event = input_thread_.queue().pop();
        if (!maybe_event.has_value()) {
            break;
        }

        auto event = maybe_event.value();
        if (handle_control_input(event)) {
            if (finished_.load(std::memory_order_acquire)) {
                return;
            }
            continue;
        }

        auto mapped = clock_sync_.input_to_audio_samples(event.input_time_ns);
        int64_t sample = mapped.value_or(buffer_start_samples) + input_offset_samples_;
        if (sample > buffer_end_samples + lookahead_samples) {
            sample = buffer_end_samples;
        }

        auto lane = lane_from_keycode(event.keycode);
        if (!lane.has_value()) {
            continue;
        }

        const int lane_index = lane.value() - 1;
        if (lane_index < 0 || lane_index >= static_cast<int>(kGameplayHudMaxLanes)) {
            continue;
        }
        if (sample < stale_before_sample) {
            stale_lane_present[static_cast<std::size_t>(lane_index)] = 1;
            stale_lane_inputs[static_cast<std::size_t>(lane_index)] = BufferedLaneInput{
                lane.value(), event.state, sample};
            continue;
        }
        pending_input_events_.push_back(BufferedLaneInput{lane.value(), event.state, sample});
    }

    for (std::size_t lane_index = 0; lane_index < stale_lane_present.size(); ++lane_index) {
        if (stale_lane_present[lane_index] == 0) {
            continue;
        }
        const auto& buffered = stale_lane_inputs[lane_index];
        catch_up_lane_input(buffered.lane, buffered.state, buffered.sample);
    }

    for (const auto& buffered : pending_input_events_) {
        dispatch_lane_input(buffered.lane, buffered.state, buffered.sample);
    }
}

void GameSession::schedule_note_guides(int64_t buffer_start_samples, int64_t buffer_end_samples) {
    if (!synthetic_tones_enabled_.load(std::memory_order_acquire)) {
        return;
    }
    while (next_guide_note_index_ < chart_.notes.size()) {
        const auto& note = chart_.notes[next_guide_note_index_];
        if (note.start_sample > buffer_end_samples) {
            break;
        }
        if (note.start_sample >= buffer_start_samples) {
            schedule_tone(note.lane, note.start_sample, true);
        }
        ++next_guide_note_index_;
    }
}

void GameSession::schedule_tone(int lane, int64_t sample, bool guide) {
    if (!synthetic_tones_enabled_.load(std::memory_order_acquire)) {
        return;
    }
    if (sample_rate_ <= 0 || lane <= 0) {
        return;
    }
    if (tone_voices_.size() >= kMaxToneVoices) {
        tone_voices_.erase(tone_voices_.begin());
    }

    const int64_t duration_samples = std::max<int64_t>(
        8, ms_to_samples(guide ? kGuideToneMs : kHitToneMs, sample_rate_));
    if (duration_samples <= 0) {
        return;
    }

    const int lane_count = std::max(1, chart_.lane_count);
    const double normalized = (lane_count <= 1)
                                  ? 0.0
                                  : (-1.0 + 2.0 * static_cast<double>(lane - 1) /
                                                static_cast<double>(lane_count - 1));
    const double pan = std::clamp(normalized, -0.90, 0.90);
    const double gain = guide ? kGuideToneGain : kHitToneGain;
    const double frequency = lane_frequency_hz(lane) * (guide ? 0.5 : 1.0);

    ToneVoice voice;
    voice.start_sample = sample;
    voice.end_sample = sample + duration_samples;
    voice.phase = 0.0;
    voice.phase_step = kTwoPi * frequency / static_cast<double>(sample_rate_);
    voice.gain_l = static_cast<float>(gain * (1.0 - pan) * 0.5);
    voice.gain_r = static_cast<float>(gain * (1.0 + pan) * 0.5);
    tone_voices_.push_back(voice);
}

void GameSession::mix_tones(float* output, uint32_t frames, int64_t buffer_start_samples) {
    if (!output || frames == 0 || tone_voices_.empty()) {
        return;
    }

    const int64_t buffer_end_samples = buffer_start_samples + static_cast<int64_t>(frames);
    for (std::size_t i = 0; i < tone_voices_.size();) {
        auto& voice = tone_voices_[i];
        if (voice.end_sample <= buffer_start_samples) {
            tone_voices_.erase(tone_voices_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        const int64_t start_offset = std::max<int64_t>(0, voice.start_sample - buffer_start_samples);
        const int64_t end_offset =
            std::min<int64_t>(static_cast<int64_t>(frames), voice.end_sample - buffer_start_samples);
        const int64_t duration = std::max<int64_t>(1, voice.end_sample - voice.start_sample);

        for (int64_t frame = start_offset; frame < end_offset; ++frame) {
            const int64_t abs_sample = buffer_start_samples + frame;
            const double progress =
                static_cast<double>(abs_sample - voice.start_sample) / static_cast<double>(duration);
            const double envelope = std::clamp(1.0 - progress, 0.0, 1.0);
            const float value = static_cast<float>(std::sin(voice.phase) * envelope);

            const std::size_t index = static_cast<std::size_t>(frame) * 2;
            output[index] += value * voice.gain_l;
            output[index + 1] += value * voice.gain_r;

            voice.phase += voice.phase_step;
            if (voice.phase >= kTwoPi) {
                voice.phase = std::fmod(voice.phase, kTwoPi);
            }
        }

        if (voice.end_sample <= buffer_end_samples) {
            tone_voices_.erase(tone_voices_.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }

    const std::size_t sample_count = static_cast<std::size_t>(frames) * 2;
    for (std::size_t i = 0; i < sample_count; ++i) {
        output[i] = std::clamp(output[i], -1.0f, 1.0f);
    }
}

std::optional<int> GameSession::lane_from_keycode(uint32_t keycode) const {
    auto it = key_to_lane_.find(keycode);
    if (it == key_to_lane_.end()) {
        return std::nullopt;
    }
    return it->second;
}

double GameSession::lane_frequency_hz(int lane) const {
    const int clamped_lane = std::max(1, lane);
    const int semitone = clamped_lane - 1;
    return 220.0 * std::pow(2.0, static_cast<double>(semitone) / 12.0);
}

std::string GameSession::find_first_chart(const std::string& root_path) const {
    namespace fs = std::filesystem;
    std::error_code ec;
#ifdef _WIN32
    fs::path root_dir = fs::u8path(root_path);
#else
    fs::path root_dir(root_path);
#endif
    if (!fs::exists(root_dir, ec) || !fs::is_directory(root_dir, ec)) {
        return {};
    }

    const std::vector<std::string> extensions = {".bms", ".bme", ".bml"};

    fs::directory_options options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(root_dir, options, ec);
    fs::recursive_directory_iterator end;
    while (it != end) {
        if (ec) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        const fs::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        ec.clear();

        auto ext = entry.path().extension().u8string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
            return entry.path().u8string();
        }
        it.increment(ec);
    }

    return {};
}

}  // namespace tenriff::app
