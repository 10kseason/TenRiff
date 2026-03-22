#pragma once

#include <cstdint>
#include <string>

namespace tenriff::audio {

/// Audio subsystem configuration.
/// Default values target low-latency exclusive mode on modern hardware.
struct AudioConfig {
    uint32_t sample_rate = 44100;        ///< Output sample rate in Hz.
    uint32_t frames_per_buffer = 128;    ///< Frames per audio callback period.
    uint32_t periods = 3;                ///< Number of buffer periods (latency multiplier).
    bool exclusive_mode = true;          ///< WASAPI exclusive mode preferred.
    bool use_mmcss = true;               ///< Enable MMCSS "Pro Audio" thread boosting.
    int32_t affinity_core = -1;          ///< CPU core affinity (-1 = OS default).
    std::string device_id;               ///< Device identifier (empty = default device).
    
    /// Computed total latency in milliseconds (approximate).
    [[nodiscard]] double latency_ms() const {
        return static_cast<double>(frames_per_buffer * periods) / sample_rate * 1000.0;
    }
    
    /// Computed total buffer size in samples.
    [[nodiscard]] uint32_t buffer_size_samples() const {
        return frames_per_buffer * periods;
    }
};

/// Audio backend type selection.
enum class AudioBackend {
    WASAPI,     ///< Windows Audio Session API (default on Windows).
    // ALSA,    ///< Advanced Linux Sound Architecture (future).
    // JACK,    ///< JACK Audio Connection Kit (future).
};

/// Result codes for audio operations.
enum class AudioResult {
    Success = 0,
    DeviceNotFound,
    DeviceInUse,
    FormatNotSupported,
    InitializationFailed,
    BufferError,
    Timeout,
    Unknown,
};

[[nodiscard]] inline int64_t playback_sample_from_write_cursor(int64_t write_cursor_samples,
                                                               uint32_t queued_padding_frames) {
    return write_cursor_samples > static_cast<int64_t>(queued_padding_frames)
               ? (write_cursor_samples - static_cast<int64_t>(queued_padding_frames))
               : 0;
}

/// Callback signature for audio processing.
/// @param output     Interleaved stereo float buffer to fill.
/// @param frames     Number of frames to render.
/// @param buffer_start_samples  Absolute sample position of the writable buffer start.
/// @param playback_sample       Absolute sample position currently leaving the device.
using AudioCallback = void(*)(float* output,
                              uint32_t frames,
                              int64_t buffer_start_samples,
                              int64_t playback_sample);

}  // namespace tenriff::audio
