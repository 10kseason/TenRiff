#include "input/InputThread.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <unordered_set>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <avrt.h>
#include <xinput.h>

#include "config/KeycodeMap.h"
#include "timing/HighResClock.h"
#include "timing/PreciseWait.h"

namespace tenriff::input {

namespace {

// Window class name for the hidden message window.
constexpr wchar_t kWindowClassName[] = L"TenRiff_InputWindow";

// Custom message for shutdown.
constexpr UINT WM_TENRIFF_QUIT = WM_USER + 1;
constexpr int64_t kRawInputRegistrationHealthIntervalNs = 100'000'000LL;

// Global pointer for window procedure (thread-local would be cleaner but adds complexity).
thread_local InputThread* g_input_thread = nullptr;

// Own scheduling on the producer thread, including its in-place RawInput ->
// Polling fallback. Never change process/global power policy or the audio task.
class ScopedInputScheduling {
public:
    ScopedInputScheduling() {
        if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL)) {
            std::cerr << "[warn] Input fallback priority failed error=" << GetLastError() << '\n';
        }
        DWORD task_index = 0;
        mmcss_ = AvSetMmThreadCharacteristicsW(L"Games", &task_index);
        if (!mmcss_) {
            std::cerr << "[warn] Input MMCSS registration failed error=" << GetLastError() << '\n';
        } else if (!AvSetMmThreadPriority(mmcss_, AVRT_PRIORITY_HIGH)) {
            std::cerr << "[warn] Input MMCSS High priority failed error=" << GetLastError() << '\n';
        } else {
            std::cerr << "[info] Input scheduling: MMCSS Games/High active.\n";
        }

        // Resolve these APIs at runtime so older Windows can retain the existing
        // input path. Save the complete state and restore it on this same thread.
        const auto kernel = GetModuleHandleW(L"kernel32.dll");
        const auto get_info = reinterpret_cast<GetThreadInformationFn>(
            GetProcAddress(kernel, "GetThreadInformation"));
        set_info_ = reinterpret_cast<SetThreadInformationFn>(
            GetProcAddress(kernel, "SetThreadInformation"));
        previous_power_.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
        if (!get_info || !set_info_) {
            std::cerr << "[warn] Input power throttling API unavailable; keeping system policy.\n";
        } else if (!get_info(GetCurrentThread(), ThreadPowerThrottling,
                             &previous_power_, sizeof(previous_power_))) {
            std::cerr << "[warn] Input power policy query failed error=" << GetLastError() << '\n';
        } else {
            auto power = previous_power_;
            power.ControlMask |= THREAD_POWER_THROTTLING_EXECUTION_SPEED;
            power.StateMask &= ~THREAD_POWER_THROTTLING_EXECUTION_SPEED;
            power_changed_ = set_info_(GetCurrentThread(), ThreadPowerThrottling,
                                       &power, sizeof(power)) != FALSE;
            if (power_changed_) {
                std::cerr << "[info] Input scheduling: execution-speed power throttling off.\n";
            } else {
                std::cerr << "[warn] Input power policy update failed error=" << GetLastError() << '\n';
            }
        }
    }

    ~ScopedInputScheduling() {
        if (power_changed_ && !set_info_(GetCurrentThread(), ThreadPowerThrottling,
                                         &previous_power_, sizeof(previous_power_))) {
            std::cerr << "[warn] Input power policy restore failed error=" << GetLastError() << '\n';
        }
        if (mmcss_ && !AvRevertMmThreadCharacteristics(mmcss_)) {
            std::cerr << "[warn] Input MMCSS cleanup failed error=" << GetLastError() << '\n';
        }
    }

    ScopedInputScheduling(const ScopedInputScheduling&) = delete;
    ScopedInputScheduling& operator=(const ScopedInputScheduling&) = delete;

private:
    using GetThreadInformationFn = BOOL(WINAPI*)(HANDLE, THREAD_INFORMATION_CLASS, LPVOID, DWORD);
    using SetThreadInformationFn = BOOL(WINAPI*)(HANDLE, THREAD_INFORMATION_CLASS, LPVOID, DWORD);
    HANDLE mmcss_ = nullptr;
    SetThreadInformationFn set_info_ = nullptr;
    THREAD_POWER_THROTTLING_STATE previous_power_{};
    bool power_changed_ = false;
};

LRESULT CALLBACK input_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_INPUT:
            if (g_input_thread && g_input_thread->is_running()) {
                // Process raw input - the handler will invoke our callback.
                // Note: We need to access the raw input handler through the thread.
                // This is handled in thread_main via the callback.
            }
            // Fall through to DefWindowProc to properly handle WM_INPUT.
            break;

        case WM_TENRIFF_QUIT:
            PostQuitMessage(0);
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

bool is_current_process_foreground() {
    const HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return false;
    }

    DWORD process_id = 0;
    GetWindowThreadProcessId(foreground, &process_id);
    return process_id == GetCurrentProcessId();
}

}  // namespace

// Even an empty XInput slot can block for milliseconds. The owned worker does
// all driver calls; the existing keyboard producer alone writes event_queue_.
struct InputThread::GamepadRuntime {
    using GetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
    InputThread& owner;
    const GamepadTestHooks hooks;
    const int64_t interval_ns;
    HMODULE module = nullptr;
    GetState get_state = nullptr;
    gamepad::GamepadTracker tracker;
    std::array<int64_t, 4> next_probe{};
    std::array<std::optional<gamepad::GamepadState>, 4> states{};
    SPSCQueue<InputEvent, InputThread::kQueueCapacity> events;
    std::optional<InputEvent> pending_event; // Read/written only by InputThread.
    uint16_t queued_mask = 0; // Read/written only by the gamepad worker.
    std::atomic<bool> stopping{false};
    std::mutex wait_mutex;
    std::condition_variable wait_cv;
    std::thread worker;

    explicit GamepadRuntime(InputThread& input)
        : owner(input), hooks(input.gamepad_test_hooks_),
          interval_ns(1'000'000'000LL / (input.config_.polling_hz > 0 ? input.config_.polling_hz : 1000)) {
        worker = std::thread([this] { run(); });
    }

    ~GamepadRuntime() {
        {
            std::lock_guard<std::mutex> lock(wait_mutex);
            stopping.store(true, std::memory_order_release);
        }
        wait_cv.notify_one();
        if (worker.joinable()) worker.join();
        if (module) FreeLibrary(module);
    }

    void load_api() {
        for (const wchar_t* name : {L"xinput1_4.dll", L"xinput9_1_0.dll"}) {
            module = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!module) continue;
            get_state = reinterpret_cast<GetState>(GetProcAddress(module, "XInputGetState"));
            if (get_state) break;
            FreeLibrary(module);
            module = nullptr;
        }
    }

    bool read(DWORD slot, gamepad::GamepadState& result) {
        if (hooks.read) return hooks.read(hooks.context, slot, result);
        XINPUT_STATE native{};
        if (get_state(slot, &native) != ERROR_SUCCESS) return false;
        result = {native.Gamepad.wButtons, native.Gamepad.bLeftTrigger, native.Gamepad.bRightTrigger};
        return true;
    }

    bool publish_mask(uint16_t mask, int64_t observed_ns) noexcept {
        owner.gamepad_mask_.store(mask, std::memory_order_release);
        // Retain ordinary edges in order. If the internal queue is full,
        // retry its unsent state changes on the next poll; in particular a
        // dropped release must not leave a held lane stuck indefinitely.
        for (std::size_t index = 0; index < gamepad::kControlCount; ++index) {
            const auto bit = static_cast<uint16_t>(1u << index);
            if (((queued_mask ^ mask) & bit) == 0) continue;
            const InputEvent event{gamepad::kKeycodeBase + static_cast<uint32_t>(index),
                (mask & bit) != 0 ? InputState::Pressed : InputState::Released,
                observed_ns, kPollingAggregateDeviceId};
            if (events.push(event)) queued_mask ^= bit;
            else owner.events_dropped_.fetch_add(1, std::memory_order_relaxed);
        }
        return queued_mask == mask;
    }

    void run() noexcept {
        try {
            run_loop();
        } catch (...) {
            // An allocation or test-driver failure must not terminate the
            // keyboard producer. Preserve queued order, then release its pads.
            std::cerr << "[warn] XInput worker stopped after an internal error.\n";
            timing::PreciseWait precise_wait;
            while (!stopping.load(std::memory_order_acquire) &&
                   !publish_mask(0, timing::HighResClock::now_ns())) {
                precise_wait.wait_for_ns(1'000'000);
            }
        }
    }

    void run_loop() {
        if (!hooks.read) {
            load_api();
            if (!get_state) return;
        }
        timing::PreciseWait precise_wait;
        while (!stopping.load(std::memory_order_acquire)) {
            const auto cycle_start = timing::HighResClock::now_ns();
            for (DWORD slot = 0; slot < states.size(); ++slot) {
                if (stopping.load(std::memory_order_acquire)) return;
                if (cycle_start < next_probe[slot]) continue;
                gamepad::GamepadState state;
                if (read(slot, state)) {
                    states[slot] = state;
                    next_probe[slot] = 0;
                } else {
                    states[slot].reset();
                    next_probe[slot] = timing::HighResClock::now_ns() + 2'000'000'000LL;
                }
            }
            // Focus can change during a slow driver call. Gate and timestamp
            // the completed observation, not the state before entering XInput.
            const bool allowed = hooks.allowed ? hooks.allowed(hooks.context) : owner.is_input_allowed();
            const auto observed_ns = timing::HighResClock::now_ns();
            static_cast<void>(tracker.update(states, allowed, observed_ns));
            const auto mask = tracker.current_mask();
            publish_mask(mask, observed_ns);

            const bool connected = std::any_of(states.begin(), states.end(),
                [](const auto& state) { return state.has_value(); });
            const auto now_ns = timing::HighResClock::now_ns();
            const auto wake_ns = connected || queued_mask != mask
                ? std::max(cycle_start + interval_ns, now_ns + 1)
                : *std::min_element(next_probe.begin(), next_probe.end());
            if (connected || queued_mask != mask) {
                // Keep sub-millisecond polling off Windows' coarse CV timer.
                // Small chunks also bound shutdown when a low rate is chosen.
                while (!stopping.load(std::memory_order_acquire)) {
                    const auto remaining_ns = wake_ns - timing::HighResClock::now_ns();
                    if (remaining_ns <= 0) break;
                    precise_wait.wait_for_ns(std::min<int64_t>(remaining_ns, 2'000'000));
                }
            } else {
                std::unique_lock<std::mutex> lock(wait_mutex);
                wait_cv.wait_for(lock, std::chrono::nanoseconds(std::max<int64_t>(1, wake_ns - now_ns)),
                    [this] { return stopping.load(std::memory_order_acquire); });
            }
        }
    }
};

void InputThread::poll_gamepads() {
    if (!gamepad_runtime_) return;
    auto& pad = *gamepad_runtime_;
    while (true) {
        const auto event = pad.pending_event ? pad.pending_event : pad.events.pop();
        if (!event) return;
        if (!event_queue_.push(*event)) {
            // Keep one deferred edge while the gameplay consumer catches up.
            // No worker ever writes this outer queue or its keyboard counters.
            pad.pending_event = event;
            return;
        }
        pad.pending_event.reset();
        events_processed_.fetch_add(1, std::memory_order_relaxed);
    }
}

InputThread::InputThread() = default;

InputThread::~InputThread() {
    shutdown();
}

bool InputThread::initialize(const InputThreadConfig& config) {
    config_ = config;
    last_input_allowed_ = false;
    gamepad_mask_.store(0, std::memory_order_release);
    active_backend_.store(static_cast<uint8_t>(config_.backend), std::memory_order_release);
    last_allowed_event_time_ns_.store(0, std::memory_order_release);
    last_queue_push_time_ns_.store(0, std::memory_order_release);
    allowed_event_count_.store(0, std::memory_order_release);
    events_processed_.store(0, std::memory_order_release);
    keyboard_events_processed_.store(0, std::memory_order_release);
    events_dropped_.store(0, std::memory_order_release);
    event_queue_.reset();

    // Create components (will be used by the thread).
    key_state_tracker_ = std::make_unique<KeyStateTracker>(config.key_state);
    if (config_.backend == InputBackend::RawInput) {
        raw_input_handler_ = std::make_unique<RawInputHandler>();
    }

    return true;
}

bool InputThread::start() {
    if (is_running_.load(std::memory_order_acquire)) {
        return true;  // Already running.
    }

    if (thread_.joinable()) {
        thread_.join();
    }

    should_stop_.store(false, std::memory_order_release);
    is_running_.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(start_mutex_);
        start_result_ready_ = false;
        start_result_success_ = false;
    }

    try {
        thread_ = std::thread(&InputThread::thread_main, this);
    } catch (...) {
        is_running_.store(false, std::memory_order_release);
        return false;
    }

    std::unique_lock<std::mutex> lock(start_mutex_);
    start_cv_.wait(lock, [this]() { return start_result_ready_; });
    const bool success = start_result_success_;
    lock.unlock();
    if (!success) {
        if (thread_.joinable()) {
            thread_.join();
        }
        gamepad_runtime_.reset();
        gamepad_mask_.store(0, std::memory_order_release);
        is_running_.store(false, std::memory_order_release);
        return false;
    }

    return true;
}

void InputThread::stop() {
    const bool running = is_running_.load(std::memory_order_acquire);
    if (!running && !thread_.joinable()) {
        return;
    }

    should_stop_.store(true, std::memory_order_release);

    // Post quit message to the window.
    if (running && hwnd_) {
        PostMessageW(static_cast<HWND>(hwnd_), WM_TENRIFF_QUIT, 0, 0);
    }

    if (thread_.joinable()) {
        thread_.join();
    }

    is_running_.store(false, std::memory_order_release);
    event_queue_.reset();
    if (key_state_tracker_) {
        key_state_tracker_->reset();
    }
    last_input_allowed_ = false;
    gamepad_runtime_.reset();
    gamepad_mask_.store(0, std::memory_order_release);
}

void InputThread::shutdown() {
    stop();

    raw_input_handler_.reset();
    key_state_tracker_.reset();
}

void InputThread::reset_key_states() {
    // This will be picked up by the input thread on next event.
    // For thread safety, we could use an atomic flag, but KeyStateTracker
    // is only accessed from the input thread.
    if (key_state_tracker_) {
        // Note: This is technically a race condition, but reset() is simple enough.
        key_state_tracker_->reset();
    }
}

InputThreadHealthSnapshot InputThread::health_snapshot() const {
    InputThreadHealthSnapshot snapshot;
    snapshot.backend = current_backend();
    snapshot.last_allowed_event_time_ns = last_allowed_event_time_ns_.load(std::memory_order_acquire);
    snapshot.last_queue_push_time_ns = last_queue_push_time_ns_.load(std::memory_order_acquire);
    snapshot.allowed_event_count = allowed_event_count_.load(std::memory_order_acquire);
    snapshot.queue_push_count = keyboard_events_processed_.load(std::memory_order_acquire);
    snapshot.dropped_count = events_dropped_.load(std::memory_order_acquire);
    return snapshot;
}

bool InputThread::is_input_allowed() const {
    return input_gate_policy_allows(config_.gate_policy, is_current_process_foreground());
}

void InputThread::sync_input_gate(bool allowed) {
    if (allowed == last_input_allowed_) {
        return;
    }

    last_input_allowed_ = allowed;
    if (key_state_tracker_) {
        key_state_tracker_->reset();
    }
}

void InputThread::on_input_event(const InputEvent& event) {
    const bool allowed = is_input_allowed();
    sync_input_gate(allowed);
    if (!allowed) {
        return;
    }

    allowed_event_count_.fetch_add(1, std::memory_order_relaxed);
    last_allowed_event_time_ns_.store(event.input_time_ns, std::memory_order_release);

    // Process through key state tracker for debouncing.
    auto filtered = key_state_tracker_->process(event);

    if (!filtered.has_value()) {
        return;  // Filtered out.
    }

    // Push to queue.
    if (!event_queue_.push(filtered.value())) {
        events_dropped_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    last_queue_push_time_ns_.store(filtered->input_time_ns, std::memory_order_release);
    keyboard_events_processed_.fetch_add(1, std::memory_order_relaxed);
    events_processed_.fetch_add(1, std::memory_order_relaxed);
}

void InputThread::signal_start_result(bool success) {
    std::lock_guard<std::mutex> lock(start_mutex_);
    if (start_result_ready_) {
        return;
    }
    start_result_success_ = success;
    start_result_ready_ = true;
    start_cv_.notify_one();
}

void InputThread::thread_main() {
    ScopedInputScheduling scheduling;
    g_input_thread = this;
    try {
        gamepad_runtime_ = std::make_unique<GamepadRuntime>(*this);
    } catch (const std::exception& error) {
        std::cerr << "[warn] XInput worker unavailable: " << error.what() << std::endl;
    }
    if (config_.backend == InputBackend::Polling) {
        signal_start_result(true);
        thread_main_polling(false);
        gamepad_runtime_.reset();
        gamepad_mask_.store(0, std::memory_order_release);
        g_input_thread = nullptr;
        is_running_.store(false, std::memory_order_release);
        return;
    }
    const bool rawinput_failed_at_runtime = thread_main_rawinput();
    if (rawinput_failed_at_runtime && !should_stop_.load(std::memory_order_acquire)) {
        // Stay on the same producer thread and preserve the queue/tracker. A
        // GameSession-side restart would race the audio callback and lose held
        // key state; in-place Polling keeps the next edge playable.
        std::cerr << "[warn] RawInput message pump exited unexpectedly; continuing with Polling on the same input thread."
                  << std::endl;
        active_backend_.store(static_cast<uint8_t>(InputBackend::Polling), std::memory_order_release);
        thread_main_polling(true);
    }
    gamepad_runtime_.reset();
    gamepad_mask_.store(0, std::memory_order_release);
    g_input_thread = nullptr;
    is_running_.store(false, std::memory_order_release);
}

std::vector<uint32_t> InputThread::build_polling_key_list() const {
    std::vector<uint32_t> keys;
    if (!config_.polling_keys.empty()) {
        keys.reserve(config_.polling_keys.size());
        std::unordered_set<uint32_t> seen_keys;
        std::unordered_set<uint32_t> warned_unpollable_keys;
        for (uint32_t keycode : config_.polling_keys) {
            if (gamepad::is_gamepad_keycode(keycode)) continue;
#ifdef _WIN32
            const auto poll_vk = config::KeycodeMap::polling_vk_for_keycode(keycode);
            if (!poll_vk.has_value()) {
                if (warned_unpollable_keys.insert(keycode).second) {
                    std::cerr << "[warn] Input polling cannot track key "
                              << config::KeycodeMap::to_name(keycode) << " (" << keycode << ")" << std::endl;
                }
                continue;
            }
            if (seen_keys.insert(*poll_vk).second) {
                keys.push_back(*poll_vk);
            }
#else
            if (seen_keys.insert(keycode).second) {
                keys.push_back(keycode);
            }
#endif
        }
    }

    if (keys.empty()) {
        keys.reserve(256);
        for (uint32_t keycode = 0; keycode <= 0xFF; ++keycode) {
            keys.push_back(keycode);
        }
    }
    return keys;
}

void InputThread::refresh_polling_key_state(const std::vector<uint32_t>& keys,
                                            std::vector<uint8_t>& last_state) {
    last_state.resize(keys.size(), 0);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const SHORT state = GetAsyncKeyState(static_cast<int>(keys[i]));
        last_state[i] = (state & 0x8000) ? 1 : 0;
    }
}

void InputThread::poll_input_keys(const std::vector<uint32_t>& keys,
                                  std::vector<uint8_t>& last_state,
                                  int64_t stamp_ns) {
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const uint32_t poll_vk = keys[i];
        const SHORT state = GetAsyncKeyState(static_cast<int>(poll_vk));
        const uint8_t pressed = (state & 0x8000) ? 1 : 0;
        if (pressed == last_state[i]) {
            continue;
        }
        last_state[i] = pressed;

        InputEvent event;
        event.keycode = config::KeycodeMap::normalize_windows_polling_keycode(poll_vk);
        event.state = pressed ? InputState::Pressed : InputState::Released;
        event.input_time_ns = stamp_ns;
        event.device_id = kPollingAggregateDeviceId;
        on_input_event(event);
    }
}

bool InputThread::thread_main_rawinput() {
    // Register window class.
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = input_window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClassName;

    SetLastError(ERROR_SUCCESS);
    ATOM class_atom = RegisterClassExW(&wc);
    const DWORD class_error = class_atom == 0 ? GetLastError() : ERROR_SUCCESS;
    const bool registered_class = class_atom != 0;
    if (!registered_class && class_error != ERROR_CLASS_ALREADY_EXISTS) {
        std::cerr << "[warn] RawInput hidden window class registration failed error="
                  << class_error << std::endl;
        signal_start_result(false);
        is_running_.store(false, std::memory_order_release);
        return false;
    }

    // Create hidden message window.
    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        L"TenRiff Input",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,  // Message-only window.
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    if (!hwnd) {
        std::cerr << "[warn] RawInput hidden message window creation failed error="
                  << GetLastError() << std::endl;
        signal_start_result(false);
        if (registered_class) {
            UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
        }
        is_running_.store(false, std::memory_order_release);
        return false;
    }

    hwnd_ = hwnd;

    // Initialize RawInput handler.
    raw_input_handler_->set_callback([this](const InputEvent& event) {
        on_input_event(event);
    });

    if (!raw_input_handler_->initialize(hwnd, config_.raw_input)) {
        signal_start_result(false);
        DestroyWindow(hwnd);
        if (registered_class) {
            UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
        }
        hwnd_ = nullptr;
        is_running_.store(false, std::memory_order_release);
        return false;
    }

    signal_start_result(true);

    const bool polling_shadow_enabled = config_.rawinput_polling_shadow;
    const int polling_hz = config_.polling_hz <= 0 ? 1000 : config_.polling_hz;
    const int64_t interval_ns = 1'000'000'000LL / static_cast<int64_t>(polling_hz);
    const std::vector<uint32_t> keys = polling_shadow_enabled ? build_polling_key_list()
                                                              : std::vector<uint32_t>{};
    std::vector<uint8_t> last_state;
    last_state.reserve(keys.size());
    if (polling_shadow_enabled) {
        refresh_polling_key_state(keys, last_state);
    }

    bool runtime_failure = false;
    timing::PreciseWait precise_wait;
    int64_t next_poll_ns = timing::HighResClock::now_ns();
    int64_t next_registration_health_check_ns = next_poll_ns + kRawInputRegistrationHealthIntervalNs;
    while (!should_stop_.load(std::memory_order_acquire)) {
        MSG msg;
        bool had_message = false;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            had_message = true;
            if (msg.message == WM_QUIT) {
                runtime_failure = !should_stop_.load(std::memory_order_acquire);
                break;
            }

            if (msg.message == WM_INPUT) {
                raw_input_handler_->process_message(msg.lParam);
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (runtime_failure || should_stop_.load(std::memory_order_acquire)) {
            break;
        }

        const int64_t now_ns = timing::HighResClock::now_ns();
        if (now_ns >= next_registration_health_check_ns) {
            if (!raw_input_handler_->registration_target_is_healthy()) {
                std::cerr << "[warn] RawInput registration target became invalid; switching to Polling."
                          << std::endl;
                runtime_failure = true;
                break;
            }
            next_registration_health_check_ns = now_ns + kRawInputRegistrationHealthIntervalNs;
        }

        const bool allowed = is_input_allowed();
        if (allowed != last_input_allowed_) {
            sync_input_gate(allowed);
            if (polling_shadow_enabled) {
                refresh_polling_key_state(keys, last_state);
            }
        }

        poll_gamepads();

        if (!polling_shadow_enabled) {
            next_poll_ns = now_ns + interval_ns;
        } else if (!allowed) {
            next_poll_ns = now_ns + interval_ns;
        } else if (now_ns >= next_poll_ns) {
            poll_input_keys(keys, last_state, now_ns);
            next_poll_ns = now_ns + interval_ns;
        }

        if (!had_message) {
            if (polling_shadow_enabled) {
                const int64_t remaining_ns = next_poll_ns - now_ns;
                if (remaining_ns > 200'000) {
                    // Process queued input promptly: its timestamp is captured by the message pump.
                    static_cast<void>(precise_wait.wait_for_messages_ns(remaining_ns - 100'000));
                } else {
                    std::this_thread::yield();
                }
            } else {
                static_cast<void>(precise_wait.wait_for_messages_ns(1'000'000));
            }
        }
    }

    // Cleanup.
    raw_input_handler_->shutdown();
    if (IsWindow(hwnd) != FALSE) {
        DestroyWindow(hwnd);
    }
    hwnd_ = nullptr;
    if (registered_class) {
        UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
    }
    return runtime_failure;
}

void InputThread::thread_main_polling(bool reconcile_existing_state) {
    const int polling_hz = config_.polling_hz <= 0 ? 1000 : config_.polling_hz;
    const int64_t interval_ns = 1'000'000'000LL / static_cast<int64_t>(polling_hz);
    const std::vector<uint32_t> keys = build_polling_key_list();
    std::vector<uint8_t> last_state;
    last_state.reserve(keys.size());
    if (reconcile_existing_state) {
        // RawInput may have died after losing the final release edge. Force one
        // polling transition per tracked key so the preserved KeyStateTracker
        // is reconciled with the keyboard's current physical state instead of
        // baselining a stale logical press forever.
        last_state.resize(keys.size(), 0);
        for (std::size_t i = 0; i < keys.size(); ++i) {
            const SHORT state = GetAsyncKeyState(static_cast<int>(keys[i]));
            const bool pressed = (state & 0x8000) != 0;
            last_state[i] = pressed ? 0 : 1;
        }
        poll_input_keys(keys, last_state, timing::HighResClock::now_ns());
    } else {
        refresh_polling_key_state(keys, last_state);
    }
    timing::PreciseWait precise_wait;
    int64_t next_tick_ns = timing::HighResClock::now_ns();
    while (!should_stop_.load(std::memory_order_acquire)) {
        int64_t now_ns = timing::HighResClock::now_ns();
        if (now_ns < next_tick_ns) {
            int64_t remaining_ns = next_tick_ns - now_ns;
            if (remaining_ns > 200'000) {
                precise_wait.wait_for_ns(remaining_ns - 100'000);
            } else {
                std::this_thread::yield();
            }
            continue;
        }

        const bool allowed = is_input_allowed();
        if (allowed != last_input_allowed_) {
            sync_input_gate(allowed);
            refresh_polling_key_state(keys, last_state);
        }

        poll_gamepads();

        if (!allowed) {
            next_tick_ns += interval_ns;
            if (next_tick_ns <= now_ns) {
                next_tick_ns = now_ns + interval_ns;
            }
            continue;
        }

        poll_input_keys(keys, last_state, now_ns);

        next_tick_ns += interval_ns;
        if (next_tick_ns <= now_ns) {
            next_tick_ns = now_ns + interval_ns;
        }
    }
}

}  // namespace tenriff::input
