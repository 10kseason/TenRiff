#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "audio/AudioConfig.h"

// Forward declarations for Windows COM types.
struct IMMDevice;
struct IAudioClient;
struct IAudioRenderClient;

namespace tenriff::audio {

/// WASAPI audio backend for low-latency exclusive mode playback.
/// Thread-safety: Methods must be called from the main thread, except where noted.
class WasapiBackend {
public:
    WasapiBackend();
    ~WasapiBackend();

    // Non-copyable, non-movable.
    WasapiBackend(const WasapiBackend&) = delete;
    WasapiBackend& operator=(const WasapiBackend&) = delete;
    WasapiBackend(WasapiBackend&&) = delete;
    WasapiBackend& operator=(WasapiBackend&&) = delete;

    /// Initialize the audio backend with the given configuration.
    /// @param config  Audio configuration specifying sample rate, buffer size, etc.
    /// @return AudioResult::Success or an error code.
    [[nodiscard]] AudioResult initialize(const AudioConfig& config);

    /// Start audio playback.
    /// @return AudioResult::Success or an error code.
    [[nodiscard]] AudioResult start();

    /// Stop audio playback.
    void stop();

    /// Release all resources.
    void shutdown();

    /// Check if the backend is currently playing.
    [[nodiscard]] bool is_playing() const { return is_playing_.load(std::memory_order_acquire); }

    /// Check if the backend is initialized.
    [[nodiscard]] bool is_initialized() const { return is_initialized_; }

    /// Get the negotiated stream sample rate.
    [[nodiscard]] uint32_t sample_rate() const { return actual_sample_rate_; }

    /// Get the device mix sample rate reported by WASAPI.
    [[nodiscard]] uint32_t device_mix_sample_rate() const { return device_mix_sample_rate_; }

    /// Get the negotiated buffer size in frames.
    [[nodiscard]] uint32_t buffer_frames() const { return buffer_frames_; }

    /// Get the current device padding in frames (call from audio thread).
    /// This represents how many frames are already queued for playback.
    [[nodiscard]] uint32_t get_padding() const;

    /// Get a buffer to write audio data into.
    /// @param frames_requested  Number of frames to request.
    /// @param buffer            Output pointer to the buffer.
    /// @param frames_available  Actual number of frames available.
    /// @return AudioResult::Success or an error code.
    [[nodiscard]] AudioResult get_buffer(uint32_t frames_requested, 
                                          float** buffer, 
                                          uint32_t* frames_available);

    /// Release the buffer after writing audio data.
    /// @param frames_written  Number of frames actually written.
    /// @return AudioResult::Success or an error code.
    [[nodiscard]] AudioResult release_buffer(uint32_t frames_written);

    /// Get whether exclusive mode was successfully acquired.
    [[nodiscard]] bool is_exclusive() const { return is_exclusive_; }

    /// Get the event handle for buffer notifications (for WaitForSingleObject).
    [[nodiscard]] void* get_event_handle() const;

    /// Get total samples played since start (monotonically increasing).
    [[nodiscard]] int64_t total_samples_played() const { 
        return total_samples_played_.load(std::memory_order_acquire); 
    }

    /// Increment total samples played (call from audio callback).
    void add_samples_played(uint32_t frames) {
        total_samples_played_.fetch_add(frames, std::memory_order_release);
    }

private:
    AudioResult initialize_device(const AudioConfig& config);
    AudioResult initialize_client(const AudioConfig& config);
    void release_resources();

    // COM interfaces (stored as void* to avoid Windows.h in header).
    void* device_ = nullptr;           // IMMDevice*
    void* audio_client_ = nullptr;     // IAudioClient*
    void* render_client_ = nullptr;    // IAudioRenderClient*
    void* event_handle_ = nullptr;     // HANDLE

    AudioConfig config_{};
    uint32_t actual_sample_rate_ = 0;
    uint32_t device_mix_sample_rate_ = 0;
    uint32_t buffer_frames_ = 0;

    bool com_initialized_ = false;
    bool is_initialized_ = false;
    bool is_exclusive_ = false;
    std::atomic<bool> is_playing_{false};
    std::atomic<int64_t> total_samples_played_{0};
};

}  // namespace tenriff::audio
