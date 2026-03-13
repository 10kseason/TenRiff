#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "audio/AudioConfig.h"
#include "audio/WasapiBackend.h"

namespace tenriff::audio {

/// Audio thread wrapper providing the master clock for the game.
/// Runs the audio callback on a high-priority thread with MMCSS boosting.
class AudioThread {
public:
    /// Callback signature for audio processing.
    /// @param output     Interleaved stereo float buffer to fill (may be nullptr for silent tick).
    /// @param frames     Number of frames to render.
    /// @param buffer_start_samples  Absolute sample position of buffer start.
    using Callback = std::function<void(float* output, uint32_t frames, int64_t buffer_start_samples)>;

    AudioThread();
    ~AudioThread();

    // Non-copyable, non-movable.
    AudioThread(const AudioThread&) = delete;
    AudioThread& operator=(const AudioThread&) = delete;

    /// Initialize the audio thread with the given configuration.
    /// @param config   Audio configuration.
    /// @param callback Processing callback invoked each audio buffer.
    /// @return AudioResult::Success or an error code.
    [[nodiscard]] AudioResult initialize(const AudioConfig& config, Callback callback);

    /// Start the audio thread and playback.
    /// @return AudioResult::Success or an error code.
    [[nodiscard]] AudioResult start();

    /// Stop the audio thread and playback.
    void stop();

    /// Shutdown and release all resources.
    void shutdown();

    /// Check if the audio thread is running.
    [[nodiscard]] bool is_running() const { return is_running_.load(std::memory_order_acquire); }

    /// Get the current playback position in samples (thread-safe).
    [[nodiscard]] int64_t playback_samples() const;

    /// Get the backend's sample rate.
    [[nodiscard]] uint32_t sample_rate() const;

    /// Get the device mix sample rate reported by the backend.
    [[nodiscard]] uint32_t device_mix_sample_rate() const;

    /// Get the backend's buffer size in frames.
    [[nodiscard]] uint32_t buffer_frames() const;

    /// Check if exclusive mode was acquired.
    [[nodiscard]] bool is_exclusive() const;

private:
    void thread_main();
    void process_buffer();

    std::unique_ptr<WasapiBackend> backend_;
    Callback callback_;
    AudioConfig config_{};

    std::thread thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};

    // MMCSS task handle (stored as void* to avoid Windows.h in header).
    void* mmcss_handle_ = nullptr;
};

}  // namespace tenriff::audio
