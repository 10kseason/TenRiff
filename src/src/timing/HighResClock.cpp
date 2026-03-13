#include "timing/HighResClock.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <chrono>
#endif

namespace tenriff::timing {

int64_t HighResClock::now_ns() {
#ifdef _WIN32
    static LARGE_INTEGER frequency = []() {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        return freq;
    }();

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    double ns = static_cast<double>(counter.QuadPart) * 1'000'000'000.0 /
                static_cast<double>(frequency.QuadPart);
    return static_cast<int64_t>(ns);
#else
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
#endif
}

}  // namespace tenriff::timing
