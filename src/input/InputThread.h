#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "input/InputEvent.h"
#include "input/KeyStateTracker.h"
#include "input/RawInputHandler.h"
#include "input/SPSCQueue.h"

namespace tenriff::input {

enum class InputBackend : uint8_t {
    RawInput = 0,
    Polling = 1,
};

/// Configuration for the input thread.
struct InputThreadConfig {
    InputBackend backend = InputBackend::Polling;  ///< Input backend selection.
    RawInputConfig raw_input;      ///< RawInput configuration.
    KeyStateConfig key_state;      ///< Key state tracker configuration.
    int polling_hz = 1000;         ///< Polling frequency for polling backend.
    std::vector<uint32_t> polling_keys;  ///< Optional list of keys to poll (empty = all 0..255).
};

/// Input thread wrapper for asynchronous RawInput processing.
/// Creates a hidden message window and runs a message pump on a dedicated thread.
/// Events are pushed to an SPSC queue for consumption by the audio thread.
class InputThread {
public:
    /// Queue capacity (power of two).
    static constexpr std::size_t kQueueCapacity = 2048;

    /// Queue type for input events.
    using EventQueue = SPSCQueue<InputEvent, kQueueCapacity>;

    InputThread();
    ~InputThread();

    // Non-copyable.
    InputThread(const InputThread&) = delete;
    InputThread& operator=(const InputThread&) = delete;

    /// Initialize the input thread.
    /// @param config  Configuration for RawInput and key state tracking.
    /// @return true on success.
    [[nodiscard]] bool initialize(const InputThreadConfig& config = {});

    /// Start the input thread.
    /// @return true on success.
    [[nodiscard]] bool start();

    /// Stop the input thread.
    void stop();

    /// Shutdown and release all resources.
    void shutdown();

    /// Check if the input thread is running.
    [[nodiscard]] bool is_running() const { return is_running_.load(std::memory_order_acquire); }

    /// Get the event queue for consumption.
    /// Thread-safe: Consumer should only be the audio thread.
    [[nodiscard]] EventQueue& queue() { return event_queue_; }
    [[nodiscard]] const EventQueue& queue() const { return event_queue_; }

    /// Get the number of events processed.
    [[nodiscard]] uint64_t events_processed() const { 
        return events_processed_.load(std::memory_order_acquire); 
    }

    /// Get the number of events dropped due to queue overflow.
    [[nodiscard]] uint64_t events_dropped() const { 
        return events_dropped_.load(std::memory_order_acquire); 
    }

    /// Reset all key states (call on focus loss).
    void reset_key_states();

private:
    void thread_main();
    void thread_main_rawinput();
    void thread_main_polling();
    void on_input_event(const InputEvent& event);
    void signal_start_result(bool success);
    [[nodiscard]] bool is_input_allowed() const;
    void sync_input_gate(bool allowed);

    InputThreadConfig config_{};
    
    std::unique_ptr<RawInputHandler> raw_input_handler_;
    std::unique_ptr<KeyStateTracker> key_state_tracker_;
    EventQueue event_queue_;

    std::thread thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};

    // Message window handle (stored as void* to avoid Windows.h).
    void* hwnd_ = nullptr;
    bool last_input_allowed_ = false;
    std::mutex start_mutex_;
    std::condition_variable start_cv_;
    bool start_result_ready_ = false;
    bool start_result_success_ = false;

    std::atomic<uint64_t> events_processed_{0};
    std::atomic<uint64_t> events_dropped_{0};
};

}  // namespace tenriff::input
