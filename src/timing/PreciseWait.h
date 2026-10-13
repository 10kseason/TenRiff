#pragma once

#include <chrono>
#include <cstdint>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace tenriff::timing {

// Own one instance per waiting thread. Reusing an unnamed timer avoids handle
// churn in input/HUD loops and does not change the process/global timer period.
// This removes coarse Sleep rounding; it cannot guarantee scheduler latency.
class PreciseWait {
public:
    PreciseWait() noexcept {
#ifdef _WIN32
        constexpr DWORD kHighResolution = 0x00000002;
        timer_ = CreateWaitableTimerExW(nullptr, nullptr, kHighResolution,
                                       TIMER_MODIFY_STATE | SYNCHRONIZE);
        high_resolution_ = timer_ != nullptr;
        if (!timer_) {
            // Windows versions before 10 1803 do not support the high-resolution
            // flag. Preserve functionality there without changing timer policy.
            timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0,
                                           TIMER_MODIFY_STATE | SYNCHRONIZE);
        }
#endif
    }

    ~PreciseWait() {
#ifdef _WIN32
        if (timer_) {
            CloseHandle(timer_);
        }
#endif
    }

    PreciseWait(const PreciseWait&) = delete;
    PreciseWait& operator=(const PreciseWait&) = delete;

    [[nodiscard]] bool uses_high_resolution_timer() const noexcept {
#ifdef _WIN32
        return high_resolution_;
#else
        return false;
#endif
    }

    void wait_for_ns(int64_t duration_ns) noexcept {
        if (duration_ns <= 0) {
            return;
        }
#ifdef _WIN32
        if (arm(duration_ns)) {
            if (WaitForSingleObject(timer_, INFINITE) == WAIT_OBJECT_0) {
                return;
            }
            disable_timer();
        }
#endif
        std::this_thread::sleep_for(std::chrono::nanoseconds(duration_ns));
    }

#ifdef _WIN32
    // Return early when the owning thread's message queue needs servicing.
    // The caller still owns PeekMessage/DispatchMessage and stop handling.
    // MWMO_INPUTAVAILABLE includes messages already observed by PeekMessage.
    [[nodiscard]] bool wait_for_messages_ns(int64_t duration_ns) noexcept {
        if (duration_ns <= 0) {
            return false;
        }
        if (arm(duration_ns)) {
            const DWORD result = MsgWaitForMultipleObjectsEx(
                1, &timer_, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (result == WAIT_OBJECT_0) {
                return false;
            }
            if (result == WAIT_OBJECT_0 + 1) {
                CancelWaitableTimer(timer_);
                return true;
            }
            disable_timer();
        }
        // Resource failure still permits WM_INPUT/WM_QUIT to wake the thread.
        const int64_t milliseconds = duration_ns / 1'000'000 +
                                     (duration_ns % 1'000'000 != 0);
        const DWORD timeout = milliseconds >= static_cast<int64_t>(INFINITE)
                                  ? INFINITE - 1
                                  : static_cast<DWORD>(milliseconds);
        return MsgWaitForMultipleObjectsEx(0, nullptr, timeout, QS_ALLINPUT,
                                          MWMO_INPUTAVAILABLE) == WAIT_OBJECT_0;
    }
#endif

private:
#ifdef _WIN32
    bool arm(int64_t duration_ns) noexcept {
        if (!timer_) {
            return false;
        }
        LARGE_INTEGER due;
        // Relative times are negative 100 ns units; round upward without an
        // overflowing duration_ns + 99 and never accidentally request zero.
        due.QuadPart = -(duration_ns / 100 + (duration_ns % 100 != 0));
        if (SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE)) {
            return true;
        }
        disable_timer();
        return false;
    }

    void disable_timer() noexcept {
        if (timer_) {
            CloseHandle(timer_);
            timer_ = nullptr;
        }
        high_resolution_ = false;
    }

    HANDLE timer_ = nullptr;
    bool high_resolution_ = false;
#endif
};

}  // namespace tenriff::timing
