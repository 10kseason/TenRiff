#include "audio/OggVorbisDecoder.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "stb_vorbis.c"

namespace tenriff::audio {

namespace {

const char* vorbis_error_label(int error_code) {
    switch (error_code) {
        case VORBIS__no_error: return "No error.";
        case VORBIS_need_more_data: return "Need more data.";
        case VORBIS_invalid_api_mixing: return "Invalid stb_vorbis API mixing.";
        case VORBIS_outofmem: return "Out of memory.";
        case VORBIS_feature_not_supported: return "Vorbis feature is not supported.";
        case VORBIS_too_many_channels: return "Vorbis stream has too many channels.";
        case VORBIS_file_open_failure: return "Failed to open OGG file.";
        case VORBIS_seek_without_length: return "Cannot seek in stream without a known length.";
        case VORBIS_unexpected_eof: return "Unexpected end of OGG stream.";
        case VORBIS_seek_invalid: return "Invalid OGG seek.";
        case VORBIS_invalid_setup: return "Invalid Vorbis setup.";
        case VORBIS_invalid_stream: return "Invalid Vorbis stream.";
        case VORBIS_missing_capture_pattern: return "Missing OGG capture pattern.";
        case VORBIS_invalid_stream_structure_version: return "Invalid OGG stream structure version.";
        case VORBIS_continued_packet_flag_invalid: return "Invalid OGG continued-packet flag.";
        case VORBIS_incorrect_stream_serial_number: return "Incorrect OGG stream serial number.";
        case VORBIS_invalid_first_page: return "Invalid first OGG page.";
        case VORBIS_bad_packet_type: return "Invalid OGG packet type.";
        case VORBIS_cant_find_last_page: return "Could not find the last OGG page.";
        case VORBIS_seek_failed: return "OGG seek failed.";
        case VORBIS_ogg_skeleton_not_supported: return "OGG skeleton streams are not supported.";
        default: return "Unknown OGG decoder error.";
    }
}

FILE* open_ogg_file_utf8(const std::string& path) {
#ifdef _WIN32
    const std::filesystem::path fs_path = std::filesystem::u8path(path);
    return _wfopen(fs_path.c_str(), L"rb");
#else
    return std::fopen(path.c_str(), "rb");
#endif
}

std::string format_ogg_error(int error_code) {
    return std::string(vorbis_error_label(error_code)) + " (code=" + std::to_string(error_code) + ").";
}

bool open_vorbis_stream(const std::string& path,
                        stb_vorbis** out_vorbis,
                        std::string* error) {
    if (!out_vorbis) {
        if (error) {
            *error = "OGG decoder output pointer is null.";
        }
        return false;
    }

    FILE* file = open_ogg_file_utf8(path);
    if (!file) {
        if (error) {
            *error = "Failed to open OGG file.";
        }
        return false;
    }

    int decode_error = VORBIS__no_error;
    stb_vorbis* vorbis = stb_vorbis_open_file(file, TRUE, &decode_error, nullptr);
    if (!vorbis) {
        if (error) {
            *error = format_ogg_error(decode_error);
        }
        return false;
    }

    *out_vorbis = vorbis;
    return true;
}

}  // namespace

std::optional<int> probe_ogg_vorbis_sample_rate(const std::string& path, std::string* error) {
    stb_vorbis* vorbis = nullptr;
    if (!open_vorbis_stream(path, &vorbis, error)) {
        return std::nullopt;
    }

    const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    stb_vorbis_close(vorbis);
    if (info.sample_rate <= 0) {
        if (error) {
            *error = "OGG decoder returned an invalid sample rate.";
        }
        return std::nullopt;
    }
    return info.sample_rate;
}

bool decode_ogg_vorbis_stereo(const std::string& path,
                              int* out_sample_rate,
                              std::vector<float>& out,
                              std::string* error) {
    out.clear();
    if (out_sample_rate) {
        *out_sample_rate = 0;
    }

    stb_vorbis* vorbis = nullptr;
    if (!open_vorbis_stream(path, &vorbis, error)) {
        return false;
    }

    const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    if (info.sample_rate <= 0 || info.channels <= 0) {
        if (error) {
            *error = "OGG decoder returned an invalid audio format.";
        }
        stb_vorbis_close(vorbis);
        return false;
    }

    if (out_sample_rate) {
        *out_sample_rate = info.sample_rate;
    }

    const int channel_count = info.channels;
    const int total_samples_per_channel = stb_vorbis_stream_length_in_samples(vorbis);
    if (total_samples_per_channel > 0) {
        out.reserve(static_cast<std::size_t>(total_samples_per_channel) * 2u);
    }

    constexpr int kChunkFrames = 4096;
    std::vector<float> chunk(static_cast<std::size_t>(kChunkFrames) * static_cast<std::size_t>(channel_count));
    for (;;) {
        const int frames = stb_vorbis_get_samples_float_interleaved(
            vorbis,
            channel_count,
            chunk.data(),
            static_cast<int>(chunk.size()));
        if (frames <= 0) {
            break;
        }

        out.reserve(out.size() + static_cast<std::size_t>(frames) * 2u);
        for (int frame = 0; frame < frames; ++frame) {
            const float* source = chunk.data() + static_cast<std::size_t>(frame) * static_cast<std::size_t>(channel_count);
            if (channel_count == 1) {
                out.push_back(source[0]);
                out.push_back(source[0]);
            } else {
                out.push_back(source[0]);
                out.push_back(source[1]);
            }
        }
    }

    stb_vorbis_close(vorbis);
    return true;
}

}  // namespace tenriff::audio
