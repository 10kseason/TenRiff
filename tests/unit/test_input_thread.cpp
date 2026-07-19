#include "doctest/doctest.h"

#include <chrono>
#include <thread>

#include "input/InputThread.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <optional>
#include <vector>

namespace {

std::optional<HWND> registered_raw_keyboard_target() {
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
        if (devices[i].usUsagePage == 0x01 && devices[i].usUsage == 0x06) {
            return devices[i].hwndTarget;
        }
    }
    return std::nullopt;
}

tenriff::input::InputThreadConfig raw_input_test_config() {
    tenriff::input::InputThreadConfig config;
    config.backend = tenriff::input::InputBackend::RawInput;
    config.raw_input.register_keyboard = true;
    config.raw_input.input_sink = true;
    config.raw_input.no_legacy = false;
    config.rawinput_polling_shadow = false;
    config.gate_policy = tenriff::input::InputGatePolicy::AlwaysAllow;
    return config;
}

bool wait_for_backend(tenriff::input::InputThread& input_thread,
                      tenriff::input::InputBackend backend,
                      std::chrono::milliseconds timeout = std::chrono::milliseconds(750)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (input_thread.current_backend() == backend) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return input_thread.current_backend() == backend;
}

}  // namespace
#endif

TEST_CASE("input gate policy keeps menu foreground-only and gameplay always-allow") {
    CHECK(tenriff::input::input_gate_policy_allows(tenriff::input::InputGatePolicy::ForegroundProcess, true));
    CHECK_FALSE(tenriff::input::input_gate_policy_allows(tenriff::input::InputGatePolicy::ForegroundProcess, false));
    CHECK(tenriff::input::input_gate_policy_allows(tenriff::input::InputGatePolicy::AlwaysAllow, true));
    CHECK(tenriff::input::input_gate_policy_allows(tenriff::input::InputGatePolicy::AlwaysAllow, false));
}

TEST_CASE("raw input thread can stop and restart without silently failing") {
#if defined(_WIN32)
    using namespace std::chrono_literals;

    tenriff::input::InputThread input_thread;
    const auto config = raw_input_test_config();

    REQUIRE(input_thread.initialize(config));
    REQUIRE(input_thread.start());
    std::this_thread::sleep_for(20ms);
    CHECK(input_thread.is_running());

    input_thread.stop();
    CHECK_FALSE(input_thread.is_running());

    REQUIRE(input_thread.start());
    std::this_thread::sleep_for(20ms);
    CHECK(input_thread.is_running());

    input_thread.shutdown();
    CHECK_FALSE(input_thread.is_running());
#else
    SUCCEED();
#endif
}

TEST_CASE("second live RawInput owner is rejected until the first owner stops") {
#if defined(_WIN32)
    using namespace std::chrono_literals;

    tenriff::input::InputThread previous_owner;
    tenriff::input::InputThread current_owner;
    const auto config = raw_input_test_config();

    REQUIRE(previous_owner.initialize(config));
    REQUIRE(previous_owner.start());
    const auto previous_target = registered_raw_keyboard_target();
    REQUIRE(previous_target.has_value());
    REQUIRE(*previous_target != nullptr);

    REQUIRE(current_owner.initialize(config));
    CHECK_FALSE(current_owner.start());
    CHECK_FALSE(current_owner.is_running());
    CHECK(previous_owner.is_running());

    const auto target_after_rejection = registered_raw_keyboard_target();
    REQUIRE(target_after_rejection.has_value());
    CHECK(*target_after_rejection == *previous_target);
    CHECK(IsWindow(*target_after_rejection) != FALSE);

    previous_owner.shutdown();

    REQUIRE(current_owner.start());
    CHECK(current_owner.is_running());
    const auto current_target = registered_raw_keyboard_target();
    REQUIRE(current_target.has_value());
    REQUIRE(*current_target != nullptr);
    CHECK(IsWindow(*current_target) != FALSE);

    DWORD target_process_id = 0;
    GetWindowThreadProcessId(*current_target, &target_process_id);
    CHECK(target_process_id == GetCurrentProcessId());

    current_owner.shutdown();
#else
    SUCCEED();
#endif
}

TEST_CASE("RawInput thread falls back in place when its message pump exits") {
#if defined(_WIN32)
    using namespace std::chrono_literals;

    tenriff::input::InputThread input_thread;
    const auto config = raw_input_test_config();
    REQUIRE(input_thread.initialize(config));
    REQUIRE(input_thread.start());

    const auto target = registered_raw_keyboard_target();
    REQUIRE(target.has_value());
    REQUIRE(*target != nullptr);
    const DWORD thread_id = GetWindowThreadProcessId(*target, nullptr);
    REQUIRE(thread_id != 0);
    REQUIRE(PostThreadMessageW(thread_id, WM_QUIT, 0, 0) != FALSE);

    CHECK(wait_for_backend(input_thread, tenriff::input::InputBackend::Polling));
    CHECK(input_thread.is_running());
    input_thread.shutdown();
#else
    SUCCEED();
#endif
}

TEST_CASE("RawInput window close switches to Polling without key activity") {
#if defined(_WIN32)
    tenriff::input::InputThread input_thread;
    const auto config = raw_input_test_config();
    REQUIRE(input_thread.initialize(config));
    REQUIRE(input_thread.start());

    const auto target = registered_raw_keyboard_target();
    REQUIRE(target.has_value());
    REQUIRE(*target != nullptr);
    REQUIRE(PostMessageW(*target, WM_CLOSE, 0, 0) != FALSE);

    CHECK(wait_for_backend(input_thread, tenriff::input::InputBackend::Polling));
    CHECK(input_thread.is_running());
    CHECK(IsWindow(*target) == FALSE);
    input_thread.shutdown();
#else
    SUCCEED();
#endif
}

TEST_CASE("RawInput target replacement switches to Polling without key activity") {
#if defined(_WIN32)
    tenriff::input::InputThread input_thread;
    const auto config = raw_input_test_config();
    REQUIRE(input_thread.initialize(config));
    REQUIRE(input_thread.start());

    HWND replacement = CreateWindowExW(0,
                                       L"STATIC",
                                       L"TenRiff RawInput replacement test",
                                       WS_POPUP,
                                       0, 0, 0, 0,
                                       nullptr,
                                       nullptr,
                                       GetModuleHandleW(nullptr),
                                       nullptr);
    REQUIRE(replacement != nullptr);

    RAWINPUTDEVICE keyboard{};
    keyboard.usUsagePage = 0x01;
    keyboard.usUsage = 0x06;
    keyboard.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    keyboard.hwndTarget = replacement;
    const BOOL override_registered = RegisterRawInputDevices(&keyboard, 1, sizeof(RAWINPUTDEVICE));
    if (override_registered == FALSE) {
        DestroyWindow(replacement);
    }
    REQUIRE(override_registered != FALSE);

    struct RegistrationCleanup {
        HWND hwnd = nullptr;
        ~RegistrationCleanup() {
            RAWINPUTDEVICE remove{};
            remove.usUsagePage = 0x01;
            remove.usUsage = 0x06;
            remove.dwFlags = RIDEV_REMOVE;
            remove.hwndTarget = nullptr;
            RegisterRawInputDevices(&remove, 1, sizeof(RAWINPUTDEVICE));
            if (hwnd && IsWindow(hwnd) != FALSE) {
                DestroyWindow(hwnd);
            }
        }
    } cleanup{replacement};

    const auto replaced_target = registered_raw_keyboard_target();
    REQUIRE(replaced_target.has_value());
    CHECK(*replaced_target == replacement);
    CHECK(wait_for_backend(input_thread, tenriff::input::InputBackend::Polling));
    CHECK(input_thread.is_running());
    input_thread.shutdown();
#else
    SUCCEED();
#endif
}

TEST_CASE("input thread health snapshot starts cleared before any events") {
#if defined(_WIN32)
    tenriff::input::InputThread input_thread;
    const auto snapshot = input_thread.health_snapshot();
    CHECK(snapshot.backend == tenriff::input::InputBackend::Polling);
    CHECK(snapshot.last_allowed_event_time_ns == 0);
    CHECK(snapshot.last_queue_push_time_ns == 0);
    CHECK(snapshot.allowed_event_count == 0);
    CHECK(snapshot.queue_push_count == 0);
    CHECK(snapshot.dropped_count == 0);
#else
    SUCCEED();
#endif
}
