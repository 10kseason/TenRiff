#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "input/InputEvent.h"

// Forward declaration to avoid Windows.h in header.
struct HWND__;
using HWND = HWND__*;

namespace tenriff::input {

/// Configuration for RawInput handling.
struct RawInputConfig {
    bool register_keyboard = true;    ///< Register for keyboard input.
    bool register_gamepad = false;    ///< Register for HID gamepad input.
    bool input_sink = true;           ///< Receive input even when not focused.
    bool no_legacy = false;           ///< Disable legacy WM_KEY* messages.
};

/// Windows RawInput handler for low-latency keyboard input capture.
/// Thread-safety: Must be used from the thread that owns the message window.
class RawInputHandler {
public:
    /// Callback invoked when a valid input event is captured.
    using EventCallback = std::function<void(const InputEvent&)>;

    RawInputHandler();
    ~RawInputHandler();

    // Non-copyable.
    RawInputHandler(const RawInputHandler&) = delete;
    RawInputHandler& operator=(const RawInputHandler&) = delete;

    /// Initialize RawInput for the given window.
    /// @param hwnd    Window handle to receive WM_INPUT messages.
    /// @param config  RawInput configuration.
    /// @return true on success.
    [[nodiscard]] bool initialize(HWND hwnd, const RawInputConfig& config = {});

    /// Set the callback for input events.
    void set_callback(EventCallback callback) { callback_ = std::move(callback); }

    /// Process a WM_INPUT message.
    /// @param lparam  The LPARAM from the WM_INPUT message.
    /// @return true if the message was handled.
    bool process_message(intptr_t lparam);

    /// Verify that this handler still owns live process-global RawInput
    /// registrations targeting its message window.
    [[nodiscard]] bool registration_target_is_healthy() const;

    /// Unregister and clean up.
    void shutdown();

    /// Get the number of events processed since initialization.
    [[nodiscard]] uint64_t events_processed() const { return events_processed_; }

private:
    void process_keyboard_input(const void* raw_input);

    HWND hwnd_ = nullptr;
    bool registered_keyboard_ = false;
    bool registered_gamepad_ = false;
    EventCallback callback_;
    std::vector<uint8_t> input_buffer_;  // Reusable buffer for GetRawInputData.
    
    // Performance counter frequency (cached).
    int64_t qpc_frequency_ = 0;

    uint64_t events_processed_ = 0;
};

}  // namespace tenriff::input
