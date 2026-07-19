#include "input/RawInputHandler.h"

#include "config/KeycodeMap.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <iostream>
#include <limits>
#include <mutex>

namespace tenriff::input {

namespace {

struct RawInputRegistrationOwners {
    std::mutex mutex;
    RawInputHandler* keyboard = nullptr;
    RawInputHandler* gamepad = nullptr;
};

RawInputRegistrationOwners& registration_owners() {
    static RawInputRegistrationOwners owners;
    return owners;
}

}  // namespace

RawInputHandler::RawInputHandler() {
    // Cache QPC frequency.
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    qpc_frequency_ = freq.QuadPart;

    // Reserve input buffer.
    input_buffer_.resize(256);
}

RawInputHandler::~RawInputHandler() {
    shutdown();
}

bool RawInputHandler::initialize(HWND hwnd, const RawInputConfig& config) {
    if (!hwnd) {
        std::cerr << "[warn] RawInput registration skipped: missing target window." << std::endl;
        return false;
    }

    // RawInput registration is process-global per usage. Release only this
    // instance's previous ownership before attempting a new registration.
    shutdown();

    // Build registration array.
    std::vector<RAWINPUTDEVICE> devices;

    if (config.register_keyboard) {
        RAWINPUTDEVICE keyboard{};
        keyboard.usUsagePage = 0x01;  // HID_USAGE_PAGE_GENERIC
        keyboard.usUsage = 0x06;       // HID_USAGE_GENERIC_KEYBOARD
        keyboard.dwFlags = RIDEV_DEVNOTIFY;
        
        if (config.input_sink) {
            keyboard.dwFlags |= RIDEV_INPUTSINK;
        }
        if (config.no_legacy) {
            keyboard.dwFlags |= RIDEV_NOLEGACY;
        }
        
        keyboard.hwndTarget = hwnd;
        devices.push_back(keyboard);
    }

    if (config.register_gamepad) {
        RAWINPUTDEVICE gamepad{};
        gamepad.usUsagePage = 0x01;  // HID_USAGE_PAGE_GENERIC
        gamepad.usUsage = 0x05;       // HID_USAGE_GENERIC_GAMEPAD
        gamepad.dwFlags = RIDEV_DEVNOTIFY;
        
        if (config.input_sink) {
            gamepad.dwFlags |= RIDEV_INPUTSINK;
        }
        
        gamepad.hwndTarget = hwnd;
        devices.push_back(gamepad);
    }

    if (devices.empty()) {
        std::cerr << "[warn] RawInput registration skipped: no devices requested." << std::endl;
        return false;
    }

    auto& owners = registration_owners();
    std::lock_guard<std::mutex> lock(owners.mutex);

    SetLastError(ERROR_SUCCESS);
    const BOOL result = RegisterRawInputDevices(
        devices.data(),
        static_cast<UINT>(devices.size()),
        sizeof(RAWINPUTDEVICE));

    if (result != TRUE) {
        std::cerr << "[warn] RegisterRawInputDevices failed error=" << GetLastError()
                  << " keyboard=" << (config.register_keyboard ? "true" : "false")
                  << " gamepad=" << (config.register_gamepad ? "true" : "false")
                  << " input_sink=" << (config.input_sink ? "true" : "false")
                  << " no_legacy=" << (config.no_legacy ? "true" : "false")
                  << std::endl;
        return false;
    }

    hwnd_ = hwnd;
    registered_keyboard_ = config.register_keyboard;
    registered_gamepad_ = config.register_gamepad;
    if (registered_keyboard_) {
        owners.keyboard = this;
    }
    if (registered_gamepad_) {
        owners.gamepad = this;
    }
    return true;
}

bool RawInputHandler::process_message(intptr_t lparam) {
    // Get required buffer size.
    UINT size = 0;
    GetRawInputData(
        reinterpret_cast<HRAWINPUT>(lparam),
        RID_INPUT,
        nullptr,
        &size,
        sizeof(RAWINPUTHEADER)
    );

    if (size == 0) {
        return false;
    }

    // Resize buffer if needed.
    if (input_buffer_.size() < size) {
        input_buffer_.resize(size);
    }

    // Get the raw input data.
    UINT copied = GetRawInputData(
        reinterpret_cast<HRAWINPUT>(lparam),
        RID_INPUT,
        input_buffer_.data(),
        &size,
        sizeof(RAWINPUTHEADER)
    );

    if (copied != size) {
        return false;
    }

    auto* raw = reinterpret_cast<const RAWINPUT*>(input_buffer_.data());

    // Process based on input type.
    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        process_keyboard_input(raw);
        return true;
    }

    return false;
}

void RawInputHandler::process_keyboard_input(const void* raw_input) {
    auto* raw = static_cast<const RAWINPUT*>(raw_input);
    const auto& kb = raw->data.keyboard;

    // Filter out auto-repeat (check RI_KEY_MAKE without previous break).
    // We'll handle state tracking in KeyStateTracker, so just filter obvious repeats here.
    // Auto-repeat generates RI_KEY_MAKE without matching RI_KEY_BREAK.
    // However, RawInput doesn't directly indicate repeat, so we pass through.

    // Build the event.
    InputEvent event{};
    
    // Use locale-stable physical scan aliases for layout-sensitive OEM keys so
    // keymaps survive non-US layouts and IME-heavy Windows setups.
    event.keycode = config::KeycodeMap::normalize_windows_raw_keycode(kb.VKey, kb.MakeCode, kb.Flags);
    
    // Determine state: RI_KEY_BREAK means key released.
    event.state = (kb.Flags & RI_KEY_BREAK) ? InputState::Released : InputState::Pressed;
    
    // Get high-resolution timestamp immediately.
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    
    // Convert to nanoseconds.
    // ns = counter * 1e9 / frequency
    // Use double to avoid overflow (loses some precision but acceptable for ~292 year range).
    event.input_time_ns = static_cast<int64_t>(
        static_cast<double>(counter.QuadPart) * 1'000'000'000.0 / static_cast<double>(qpc_frequency_)
    );

    const auto source_token = reinterpret_cast<std::uintptr_t>(raw->header.hDevice);
    event.device_id = (source_token == kPollingAggregateDeviceId)
                          ? (std::numeric_limits<InputSourceToken>::max)()
                          : source_token;

    ++events_processed_;

    // Invoke callback.
    if (callback_) {
        callback_(event);
    }
}

void RawInputHandler::shutdown() {
    if (!hwnd_ && !registered_keyboard_ && !registered_gamepad_) {
        return;
    }

    auto& owners = registration_owners();
    std::lock_guard<std::mutex> lock(owners.mutex);

    std::vector<RAWINPUTDEVICE> devices;
    if (registered_keyboard_ && owners.keyboard == this) {
        RAWINPUTDEVICE keyboard{};
        keyboard.usUsagePage = 0x01;
        keyboard.usUsage = 0x06;
        keyboard.dwFlags = RIDEV_REMOVE;
        keyboard.hwndTarget = nullptr;
        devices.push_back(keyboard);
        owners.keyboard = nullptr;
    }
    if (registered_gamepad_ && owners.gamepad == this) {
        RAWINPUTDEVICE gamepad{};
        gamepad.usUsagePage = 0x01;
        gamepad.usUsage = 0x05;
        gamepad.dwFlags = RIDEV_REMOVE;
        gamepad.hwndTarget = nullptr;
        devices.push_back(gamepad);
        owners.gamepad = nullptr;
    }

    if (!devices.empty() &&
        RegisterRawInputDevices(devices.data(), static_cast<UINT>(devices.size()), sizeof(RAWINPUTDEVICE)) != TRUE) {
        std::cerr << "[warn] RawInput unregister failed error=" << GetLastError() << std::endl;
    }

    hwnd_ = nullptr;
    registered_keyboard_ = false;
    registered_gamepad_ = false;
}

}  // namespace tenriff::input
