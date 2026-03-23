#include "audio/AudioThread.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <avrt.h>

#include <cstring>

#pragma comment(lib, "avrt.lib")

namespace tenriff::audio {

AudioThread::AudioThread() = default;

AudioThread::~AudioThread() {
    shutdown();
}

AudioResult AudioThread::initialize(const AudioConfig& config, Callback callback) {
    if (!callback) {
        return AudioResult::InitializationFailed;
    }

    config_ = config;
    callback_ = std::move(callback);
    backend_ = std::make_unique<WasapiBackend>();

    return backend_->initialize(config);
}

AudioResult AudioThread::start() {
    if (!backend_ || !backend_->is_initialized()) {
        return AudioResult::InitializationFailed;
    }

    if (is_running_.load(std::memory_order_acquire)) {
        return AudioResult::Success;  // Already running.
    }

    should_stop_.store(false, std::memory_order_release);

    // Start the backend.
    auto result = backend_->start();
    if (result != AudioResult::Success) {
        return result;
    }

    // Launch the audio thread.
    is_running_.store(true, std::memory_order_release);
    thread_ = std::thread(&AudioThread::thread_main, this);

    return AudioResult::Success;
}

void AudioThread::stop() {
    if (!is_running_.load(std::memory_order_acquire)) {
        return;
    }

    should_stop_.store(true, std::memory_order_release);

    // Signal the event to wake up the thread if waiting.
    if (backend_ && backend_->get_event_handle()) {
        SetEvent(static_cast<HANDLE>(backend_->get_event_handle()));
    }

    if (thread_.joinable()) {
        thread_.join();
    }

    if (backend_) {
        backend_->stop();
    }

    is_running_.store(false, std::memory_order_release);
}

void AudioThread::shutdown() {
    stop();
    
    if (backend_) {
        backend_->shutdown();
        backend_.reset();
    }

    callback_ = nullptr;
}

int64_t AudioThread::playback_samples() const {
    return backend_ ? backend_->total_samples_played() : 0;
}

uint32_t AudioThread::sample_rate() const {
    return backend_ ? backend_->sample_rate() : 0;
}

uint32_t AudioThread::device_mix_sample_rate() const {
    return backend_ ? backend_->device_mix_sample_rate() : 0;
}

uint32_t AudioThread::buffer_frames() const {
    return backend_ ? backend_->buffer_frames() : 0;
}

bool AudioThread::is_exclusive() const {
    return backend_ && backend_->is_exclusive();
}

void AudioThread::thread_main() {
    // Boost thread priority with MMCSS.
    DWORD task_index = 0;
    HANDLE mmcss_handle = nullptr;
    
    if (config_.use_mmcss) {
        mmcss_handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
        if (mmcss_handle) {
            AvSetMmThreadPriority(mmcss_handle, AVRT_PRIORITY_CRITICAL);
            mmcss_handle_ = mmcss_handle;
        }
    }

    // Set thread priority as fallback.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    // Set CPU affinity if requested.
    if (config_.affinity_core >= 0) {
        DWORD_PTR affinity_mask = 1ULL << config_.affinity_core;
        SetThreadAffinityMask(GetCurrentThread(), affinity_mask);
    }

    // Main audio loop.
    HANDLE event_handle = static_cast<HANDLE>(backend_->get_event_handle());
    
    while (!should_stop_.load(std::memory_order_acquire)) {
        // Wait for buffer event (or timeout for safety).
        DWORD wait_result = WaitForSingleObject(event_handle, 100);

        if (should_stop_.load(std::memory_order_acquire)) {
            break;
        }

        if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_TIMEOUT) {
            process_buffer();
        }
    }

    // Clean up MMCSS.
    if (mmcss_handle) {
        AvRevertMmThreadCharacteristics(mmcss_handle);
        mmcss_handle_ = nullptr;
    }
}

void AudioThread::process_buffer() {
    if (!backend_ || !backend_->is_playing()) {
        return;
    }

    // Calculate available frames.
    uint32_t padding = backend_->get_padding();
    uint32_t buffer_size = backend_->buffer_frames();
    
    if (padding >= buffer_size) {
        return;  // Buffer is full.
    }

    uint32_t frames_available = buffer_size - padding;
    
    uint32_t frames_to_write = frames_available;
    if (backend_->is_exclusive()) {
        // Exclusive mode uses our requested period; shared event-driven mode must
        // fill the whole available buffer each wake-up to track the engine period.
        frames_to_write = (frames_available > config_.frames_per_buffer)
                              ? config_.frames_per_buffer
                              : frames_available;
    }

    if (frames_to_write == 0) {
        return;
    }

    // Get buffer from WASAPI.
    float* buffer = nullptr;
    uint32_t frames_obtained = 0;
    
    auto result = backend_->get_buffer(frames_to_write, &buffer, &frames_obtained);
    if (result != AudioResult::Success || !buffer || frames_obtained == 0) {
        return;
    }

    // `total_samples_played()` is our running count of frames already released to the
    // backend. The next writable region begins at that write cursor; adding padding
    // again pushes the callback timeline one device-latency too far into the future.
    const int64_t write_cursor_samples = backend_->total_samples_played();
    const int64_t buffer_start_samples = write_cursor_samples;
    const int64_t playback_sample = playback_sample_from_write_cursor(write_cursor_samples, padding);

    // Invoke the callback.
    if (callback_) {
        callback_(buffer, frames_obtained, buffer_start_samples, playback_sample);
    } else {
        // Silent fill if no callback.
        std::memset(buffer, 0, frames_obtained * 2 * sizeof(float));
    }

    // Do not advance the engine cursor if WASAPI rejected the rendered buffer.
    auto release_result = backend_->release_buffer(frames_obtained);
    if (release_result != AudioResult::Success) {
        should_stop_.store(true, std::memory_order_release);
        return;
    }

    // Release buffer and update sample count.
    backend_->add_samples_played(frames_obtained);
}

}  // namespace tenriff::audio
