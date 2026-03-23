#include "input/InputThread.h"

#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "config/KeycodeMap.h"
#include "timing/HighResClock.h"

namespace tenriff::input {

namespace {

// Window class name for the hidden message window.
constexpr wchar_t kWindowClassName[] = L"TenRiff_InputWindow";

// Custom message for shutdown.
constexpr UINT WM_TENRIFF_QUIT = WM_USER + 1;

// Global pointer for window procedure (thread-local would be cleaner but adds complexity).
thread_local InputThread* g_input_thread = nullptr;

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

        case WM_DESTROY:
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

InputThread::InputThread() = default;

InputThread::~InputThread() {
    shutdown();
}

bool InputThread::initialize(const InputThreadConfig& config) {
    config_ = config;
    last_input_allowed_ = false;
    
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

bool InputThread::is_input_allowed() const {
    return is_current_process_foreground();
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
    g_input_thread = this;
    if (config_.backend == InputBackend::Polling) {
        signal_start_result(true);
        thread_main_polling();
        g_input_thread = nullptr;
        return;
    }
    thread_main_rawinput();
    g_input_thread = nullptr;
}

void InputThread::thread_main_rawinput() {
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
        signal_start_result(false);
        is_running_.store(false, std::memory_order_release);
        return;
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
        signal_start_result(false);
        if (registered_class) {
            UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
        }
        is_running_.store(false, std::memory_order_release);
        return;
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
        return;
    }

    signal_start_result(true);

    // Set thread priority (above normal for input responsiveness).
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    // Message pump.
    MSG msg;
    while (!should_stop_.load(std::memory_order_acquire)) {
        BOOL result = GetMessageW(&msg, nullptr, 0, 0);
        
        if (result == 0 || result == -1) {
            break;  // WM_QUIT or error.
        }

        // Handle WM_INPUT directly.
        if (msg.message == WM_INPUT) {
            raw_input_handler_->process_message(msg.lParam);
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup.
    raw_input_handler_->shutdown();
    DestroyWindow(hwnd);
    hwnd_ = nullptr;
    if (registered_class) {
        UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
    }
}

void InputThread::thread_main_polling() {
    const int polling_hz = config_.polling_hz <= 0 ? 1000 : config_.polling_hz;
    const int64_t interval_ns = 1'000'000'000LL / static_cast<int64_t>(polling_hz);
    std::vector<uint32_t> keys = config_.polling_keys;
    if (keys.empty()) {
        keys.reserve(256);
        for (uint32_t keycode = 0; keycode <= 0xFF; ++keycode) {
            keys.push_back(keycode);
        }
    }

    std::vector<uint8_t> last_state(keys.size(), 0);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    int64_t next_tick_ns = timing::HighResClock::now_ns();
    while (!should_stop_.load(std::memory_order_acquire)) {
        int64_t now_ns = timing::HighResClock::now_ns();
        if (now_ns < next_tick_ns) {
            int64_t remaining_ns = next_tick_ns - now_ns;
            if (remaining_ns > 200'000) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(remaining_ns - 100'000));
            } else {
                std::this_thread::yield();
            }
            continue;
        }

        const bool allowed = is_input_allowed();
        if (allowed != last_input_allowed_) {
            sync_input_gate(allowed);
            for (std::size_t i = 0; i < keys.size(); ++i) {
                const SHORT state = GetAsyncKeyState(static_cast<int>(keys[i]));
                last_state[i] = (state & 0x8000) ? 1 : 0;
            }
        }

        if (!allowed) {
            next_tick_ns += interval_ns;
            if (next_tick_ns <= now_ns) {
                next_tick_ns = now_ns + interval_ns;
            }
            continue;
        }

        const int64_t stamp_ns = now_ns;
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
            event.device_id = 0;
            on_input_event(event);
        }

        next_tick_ns += interval_ns;
        if (next_tick_ns <= now_ns) {
            next_tick_ns = now_ns + interval_ns;
        }
    }
}

}  // namespace tenriff::input
