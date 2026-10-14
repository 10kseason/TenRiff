#include "doctest/doctest.h"

#include <chrono>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "app/InputBackendStatus.h"
#include "input/InputThread.h"
#include "timing/HighResClock.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <optional>
#include <vector>

namespace tenriff::input {
struct InputThreadGamepadTestAccess {
    static void attach(InputThread& input, void* context,
                       bool (*read)(void*, uint32_t, gamepad::GamepadState&),
                       bool (*allowed)(void*)) {
        input.gamepad_test_hooks_ = {context, read, allowed};
    }
    static bool has_runtime_after_join(const InputThread& input) {
        return input.gamepad_runtime_ != nullptr;
    }
};
}  // namespace tenriff::input

namespace {

struct FakeGamepadDriver {
    std::mutex mutex;
    std::condition_variable cv;
    std::array<std::optional<tenriff::input::gamepad::GamepadState>, 4> states{};
    std::atomic<bool> allowed{true};
    std::atomic<bool> inside_block{false};
    std::atomic<unsigned> reads{0};
    std::atomic<int64_t> first_press_observed_ns{0};
    bool block_next = false;
    bool release_block = false;
    bool oscillate = false;
    bool fail_read = false;

    static bool read(void* context, uint32_t slot, tenriff::input::gamepad::GamepadState& state) {
        auto& self = *static_cast<FakeGamepadDriver*>(context);
        std::unique_lock<std::mutex> lock(self.mutex);
        if (slot == 0 && self.block_next) {
            self.block_next = false;
            self.inside_block.store(true, std::memory_order_release);
            // A failed assertion must not strand the test process in a driver.
            self.cv.wait_for(lock, std::chrono::seconds(2), [&] { return self.release_block; });
            self.inside_block.store(false, std::memory_order_release);
            self.release_block = false;
        }
        const auto read_count = self.reads.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (self.fail_read) throw std::runtime_error("fake driver failure");
        if (slot == 0 && self.oscillate) {
            const bool down = (read_count % 2) == 0;
            state = {static_cast<uint16_t>(down ? 0xffff : 0),
                     static_cast<uint8_t>(down ? 255 : 0), static_cast<uint8_t>(down ? 255 : 0)};
            return true;
        }
        if (!self.states[slot]) return false;
        state = *self.states[slot];
        if (slot == 0 && state.buttons != 0) {
            int64_t zero = 0;
            self.first_press_observed_ns.compare_exchange_strong(zero, tenriff::timing::HighResClock::now_ns());
        }
        return true;
    }

    static bool gate(void* context) {
        return static_cast<FakeGamepadDriver*>(context)->allowed.load(std::memory_order_acquire);
    }

    void set_buttons(uint16_t buttons) {
        std::lock_guard<std::mutex> lock(mutex);
        states[0] = tenriff::input::gamepad::GamepadState{buttons, 0, 0};
    }

    void unblock() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            release_block = true;
        }
        cv.notify_one();
    }

    void attach(tenriff::input::InputThread& input) {
        tenriff::input::InputThreadGamepadTestAccess::attach(input, this, read, gate);
    }
};

struct UnblockGamepadOnExit {
    FakeGamepadDriver& driver;
    ~UnblockGamepadOnExit() { driver.unblock(); }
};

template <typename Predicate>
bool wait_for_gamepad(Predicate predicate, std::chrono::milliseconds timeout = std::chrono::milliseconds(750)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return predicate();
}

std::optional<tenriff::input::InputEvent> wait_for_gamepad_event(tenriff::input::InputThread& input) {
    std::optional<tenriff::input::InputEvent> result;
    wait_for_gamepad([&] {
        while (const auto event = input.queue().pop()) {
            if (tenriff::input::gamepad::is_gamepad_keycode(event->keycode)) {
                result = event;
                return true;
            }
        }
        return false;
    });
    return result;
}

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

TEST_CASE("input gate policy rejects background events unless explicitly overridden") {
    CHECK(tenriff::input::input_gate_policy_allows(tenriff::input::InputGatePolicy::ForegroundProcess, true));
    CHECK_FALSE(tenriff::input::input_gate_policy_allows(tenriff::input::InputGatePolicy::ForegroundProcess, false));
    CHECK(tenriff::app::gameplay_input_gate_policy() ==
          tenriff::input::InputGatePolicy::ForegroundProcess);
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
    CHECK_FALSE(tenriff::input::InputThreadGamepadTestAccess::has_runtime_after_join(current_owner));
    CHECK(current_owner.gamepad_mask() == 0);
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

TEST_CASE("gamepad driver waits do not block the keyboard producer or its fallback") {
#if defined(_WIN32)
    using namespace std::chrono_literals;
    FakeGamepadDriver driver;
    driver.set_buttons(0);
    driver.block_next = true;
    tenriff::input::InputThread input;
    UnblockGamepadOnExit cleanup{driver};
    REQUIRE(input.initialize(raw_input_test_config()));
    driver.attach(input);
    REQUIRE(input.start());
    REQUIRE(wait_for_gamepad([&] { return driver.inside_block.load(); }));

    const auto target = registered_raw_keyboard_target();
    REQUIRE(target.has_value());
    const auto producer_id = GetWindowThreadProcessId(*target, nullptr);
    REQUIRE(PostThreadMessageW(producer_id, WM_QUIT, 0, 0) != FALSE);
    CHECK(wait_for_backend(input, tenriff::input::InputBackend::Polling));
    CHECK(driver.inside_block.load());
    CHECK(input.is_running());
    driver.unblock();
    input.shutdown();
    CHECK_FALSE(input.is_running());
#else
    SUCCEED();
#endif
}

TEST_CASE("gamepad worker delivers post-read edges and rechecks focus after a blocking call") {
#if defined(_WIN32)
    using namespace std::chrono_literals;
    FakeGamepadDriver driver;
    driver.set_buttons(0);
    tenriff::input::InputThread input;
    UnblockGamepadOnExit cleanup{driver};
    tenriff::input::InputThreadConfig config;
    config.backend = tenriff::input::InputBackend::Polling;
    config.gate_policy = tenriff::input::InputGatePolicy::AlwaysAllow;
    config.polling_keys = {0xfe}; // Avoid physical keyboard traffic in this fixture.
    REQUIRE(input.initialize(config));
    driver.attach(input);
    REQUIRE(input.start());
    REQUIRE(wait_for_gamepad([&] { return driver.reads.load() >= 5; }));
    driver.set_buttons(0x1000); // A.
    auto event = wait_for_gamepad_event(input);
    REQUIRE(event.has_value());
    CHECK(event->keycode == tenriff::input::gamepad::kKeycodeBase + 10);
    CHECK(event->state == tenriff::input::InputState::Pressed);
    CHECK(event->input_time_ns >= driver.first_press_observed_ns.load());
    CHECK(input.gamepad_key_pressed(event->keycode));
    CHECK(input.events_processed() >= 1);
    CHECK(input.health_snapshot().queue_push_count == 0);
    CHECK(input.health_snapshot().allowed_event_count == 0);

    {
        std::lock_guard<std::mutex> lock(driver.mutex);
        driver.block_next = true;
    }
    REQUIRE(wait_for_gamepad([&] { return driver.inside_block.load(); }));
    driver.allowed.store(false);
    const auto focus_lost_ns = tenriff::timing::HighResClock::now_ns();
    driver.unblock();
    event = wait_for_gamepad_event(input);
    REQUIRE(event.has_value());
    CHECK(event->state == tenriff::input::InputState::Released);
    CHECK(event->input_time_ns >= focus_lost_ns);
    CHECK(input.gamepad_mask() == 0);

    driver.allowed.store(true);
    const auto baseline_reads = driver.reads.load();
    REQUIRE(wait_for_gamepad([&] { return driver.reads.load() >= baseline_reads + 5; }));
    CHECK(input.gamepad_mask() == 0);
    CHECK_FALSE(input.queue().pop().has_value());
    driver.set_buttons(0);
    const auto released_reads = driver.reads.load();
    REQUIRE(wait_for_gamepad([&] { return driver.reads.load() >= released_reads + 3; }));
    driver.set_buttons(0x1000);
    event = wait_for_gamepad_event(input);
    REQUIRE(event.has_value());
    CHECK(event->state == tenriff::input::InputState::Pressed);
    driver.set_buttons(0);
    event = wait_for_gamepad_event(input);
    REQUIRE(event.has_value());
    CHECK(event->state == tenriff::input::InputState::Released);
    input.shutdown();
    const auto stopped_reads = driver.reads.load();
    std::this_thread::sleep_for(10ms);
    CHECK(driver.reads.load() == stopped_reads);
    CHECK(input.gamepad_mask() == 0);
#else
    SUCCEED();
#endif
}

TEST_CASE("gamepad worker shutdown wakes the disconnected-slot wait") {
#if defined(_WIN32)
    using namespace std::chrono_literals;
    FakeGamepadDriver driver;
    tenriff::input::InputThread input;
    tenriff::input::InputThreadConfig config;
    config.backend = tenriff::input::InputBackend::Polling;
    config.gate_policy = tenriff::input::InputGatePolicy::AlwaysAllow;
    config.polling_keys = {0xfe};
    REQUIRE(input.initialize(config));
    driver.attach(input);
    REQUIRE(input.start());
    REQUIRE(wait_for_gamepad([&] { return driver.reads.load() >= 4; }));
    std::this_thread::sleep_for(20ms);
    CHECK(driver.reads.load() == 4); // Empty slots retain their two-second backoff.
    const auto start = std::chrono::steady_clock::now();
    input.shutdown();
    CHECK(std::chrono::steady_clock::now() - start < 500ms);
    CHECK_FALSE(input.is_running());
#else
    SUCCEED();
#endif
}

TEST_CASE("gamepad worker failure releases queued controls and preserves keyboard input") {
#if defined(_WIN32)
    FakeGamepadDriver driver;
    driver.set_buttons(0);
    tenriff::input::InputThread input;
    tenriff::input::InputThreadConfig config;
    config.backend = tenriff::input::InputBackend::Polling;
    config.gate_policy = tenriff::input::InputGatePolicy::AlwaysAllow;
    config.polling_keys = {0xfe};
    REQUIRE(input.initialize(config));
    driver.attach(input);
    REQUIRE(input.start());
    REQUIRE(wait_for_gamepad([&] { return driver.reads.load() >= 5; }));
    driver.set_buttons(0x1000);
    auto event = wait_for_gamepad_event(input);
    REQUIRE(event.has_value());
    CHECK(event->state == tenriff::input::InputState::Pressed);
    {
        std::lock_guard<std::mutex> lock(driver.mutex);
        driver.fail_read = true;
    }
    event = wait_for_gamepad_event(input);
    REQUIRE(event.has_value());
    CHECK(event->state == tenriff::input::InputState::Released);
    CHECK(input.gamepad_mask() == 0);
    CHECK(input.is_running());
    CHECK(input.health_snapshot().queue_push_count == 0);
    input.shutdown();
#else
    SUCCEED();
#endif
}

TEST_CASE("gamepad worker low-rate polling does not extend shutdown to its next deadline") {
#if defined(_WIN32)
    using namespace std::chrono_literals;
    FakeGamepadDriver driver;
    driver.set_buttons(0);
    tenriff::input::InputThread input;
    auto config = raw_input_test_config();
    config.polling_hz = 1;
    REQUIRE(input.initialize(config));
    driver.attach(input);
    REQUIRE(input.start());
    REQUIRE(wait_for_gamepad([&] { return driver.reads.load() >= 4; }));
    const auto start = std::chrono::steady_clock::now();
    input.shutdown();
    CHECK(std::chrono::steady_clock::now() - start < 500ms);
    CHECK_FALSE(input.is_running());
#else
    SUCCEED();
#endif
}

TEST_CASE("gamepad queue pressure eventually releases every delivered held control") {
#if defined(_WIN32)
    using namespace std::chrono_literals;
    FakeGamepadDriver driver;
    driver.set_buttons(0);
    driver.oscillate = true;
    tenriff::input::InputThread input;
    tenriff::input::InputThreadConfig config;
    config.backend = tenriff::input::InputBackend::Polling;
    config.gate_policy = tenriff::input::InputGatePolicy::AlwaysAllow;
    config.polling_keys = {0xfe};
    REQUIRE(input.initialize(config));
    driver.attach(input);
    REQUIRE(input.start());
    REQUIRE(wait_for_gamepad([&] { return input.events_dropped() > 0; }, 5000ms));
    {
        std::lock_guard<std::mutex> lock(driver.mutex);
        driver.oscillate = false;
        driver.states[0] = tenriff::input::gamepad::GamepadState{};
    }
    uint16_t held = 0;
    std::size_t delivered = 0;
    const auto deadline = std::chrono::steady_clock::now() + 1000ms;
    while (std::chrono::steady_clock::now() < deadline) {
        while (const auto event = input.queue().pop()) {
            if (const auto index = tenriff::input::gamepad::keycode_index(event->keycode)) {
                const auto bit = static_cast<uint16_t>(1u << *index);
                if (event->state == tenriff::input::InputState::Pressed) held |= bit;
                else held &= static_cast<uint16_t>(~bit);
                ++delivered;
            }
        }
        std::this_thread::sleep_for(1ms);
    }
    CHECK(delivered >= 2048);
    CHECK(held == 0);
    CHECK(input.gamepad_mask() == 0);
    CHECK(input.health_snapshot().queue_push_count == 0);
    input.shutdown();
#else
    SUCCEED();
#endif
}
