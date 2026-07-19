#include "input/RawInputHandler.h"

#include "config/KeycodeMap.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <iostream>
#include <limits>
#include <mutex>
#include <optional>

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

std::optional<HWND> registered_target_for_usage(USHORT usage_page, USHORT usage) {
    UINT count = 0;
    if (GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE)) == static_cast<UINT>(-1) ||
        count == 0) {
        return std::nullopt;
    }

    std::vector<RAWINPUTDEVICE> devices(count);
    UINT capacity = count;
    const UINT registered = GetRegisteredRawInputDevices(devices.data(), &capacity, sizeof(RAWINPUTDEVICE));
    if (registered == static_cast<UINT>(-1)) {
        return std::nullopt;
    }

    for (UINT i = 0; i < registered; ++i) {
        if (devices[i].usUsagePage == usage_page && devices[i].usUsage == usage) {
            return devices[i].hwndTarget;
        }
    }
    return std::nullopt;
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

    auto has_live_other_owner = [this, hwnd](RawInputHandler*& tracked_owner,
                                              USHORT usage_page,
                                              USHORT usage) {
        const auto registered_target = registered_target_for_usage(usage_page, usage);
        const HWND tracked_target = tracked_owner ? tracked_owner->hwnd_ : nullptr;
        const bool tracked_owner_is_live =
            tracked_owner != nullptr && tracked_owner != this && tracked_target != nullptr &&
            IsWindow(tracked_target) != FALSE && registered_target.has_value() &&
            *registered_target == tracked_target;
        const bool untracked_live_target =
            registered_target.has_value() && *registered_target != nullptr && *registered_target != hwnd &&
            IsWindow(*registered_target) != FALSE;

        if (tracked_owner_is_live || untracked_live_target) {
            return true;
        }

        if (tracked_owner != nullptr && tracked_owner != this) {
            // The bookkeeping outlived a registration/window. The stale
            // handler will no longer be allowed to remove a newer owner.
            tracked_owner = nullptr;
        }
        return false;
    };

    if ((config.register_keyboard && has_live_other_owner(owners.keyboard, 0x01, 0x06)) ||
        (config.register_gamepad && has_live_other_owner(owners.gamepad, 0x01, 0x05))) {
        std::cerr << "[warn] RawInput registration rejected: another live process-global owner already exists."
                  << std::endl;
        return false;
    }

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

bool RawInputHandler::registration_target_is_healthy() const {
    if (!hwnd_ || (!registered_keyboard_ && !registered_gamepad_) || IsWindow(hwnd_) == FALSE) {
        return false;
    }

    auto& owners = registration_owners();
    std::lock_guard<std::mutex> lock(owners.mutex);

    if (registered_keyboard_) {
        const auto target = registered_target_for_usage(0x01, 0x06);
        if (owners.keyboard != this || !target.has_value() || *target != hwnd_) {
            return false;
        }
    }
    if (registered_gamepad_) {
        const auto target = registered_target_for_usage(0x01, 0x05);
        if (owners.gamepad != this || !target.has_value() || *target != hwnd_) {
            return false;
        }
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
        const auto target = registered_target_for_usage(0x01, 0x06);
        if (target.has_value() && *target == hwnd_) {
            RAWINPUTDEVICE keyboard{};
            keyboard.usUsagePage = 0x01;
            keyboard.usUsage = 0x06;
            keyboard.dwFlags = RIDEV_REMOVE;
            keyboard.hwndTarget = nullptr;
            devices.push_back(keyboard);
        }
        owners.keyboard = nullptr;
    }
    if (registered_gamepad_ && owners.gamepad == this) {
        const auto target = registered_target_for_usage(0x01, 0x05);
        if (target.has_value() && *target == hwnd_) {
            RAWINPUTDEVICE gamepad{};
            gamepad.usUsagePage = 0x01;
            gamepad.usUsage = 0x05;
            gamepad.dwFlags = RIDEV_REMOVE;
            gamepad.hwndTarget = nullptr;
            devices.push_back(gamepad);
        }
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
