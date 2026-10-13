#include "doctest/doctest.h"
#include "timing/PreciseWait.h"

#include <chrono>
#include <future>
#include <thread>

TEST_CASE("PreciseWait handles empty durations and repeated timer waits") {
    tenriff::timing::PreciseWait wait;
    wait.wait_for_ns(0);
    wait.wait_for_ns(-1);
    for (int i = 0; i < 3; ++i) {
        const auto start = std::chrono::steady_clock::now();
        wait.wait_for_ns(900'000);
        if (wait.uses_high_resolution_timer()) {
            // Detect an immediate stale signal, not a real-time OS guarantee.
            // The ordinary timer fallback may expire before its requested time.
            CHECK(std::chrono::steady_clock::now() - start >= std::chrono::nanoseconds(450'000));
        }
    }
}

#ifdef _WIN32
TEST_CASE("PreciseWait wakes for an already observed message and rearms") {
    tenriff::timing::PreciseWait wait;
    MSG message{};
    constexpr UINT kMessage = WM_APP + 142;
    PeekMessageW(&message, nullptr, kMessage, kMessage, PM_NOREMOVE);
    REQUIRE(PostThreadMessageW(GetCurrentThreadId(), kMessage, 0, 0) != 0);
    REQUIRE(PeekMessageW(&message, nullptr, kMessage, kMessage, PM_NOREMOVE) != 0);
    CHECK(wait.wait_for_messages_ns(1'000'000'000));
    CHECK(PeekMessageW(&message, nullptr, kMessage, kMessage, PM_REMOVE) != 0);
    const auto start = std::chrono::steady_clock::now();
    wait.wait_for_ns(2'000'000);
    if (wait.uses_high_resolution_timer()) {
        CHECK(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(1));
    }
}

TEST_CASE("PreciseWait wakes when a message arrives during its timer wait") {
    constexpr UINT kMessage = WM_APP + 143;
    constexpr WPARAM kToken = 0x1234;
    std::promise<DWORD> queue_ready;
    auto owner = queue_ready.get_future();
    bool signaled = false;
    bool received_target = false;
    std::chrono::steady_clock::duration elapsed{};
    // Isolate from other tests' windows/WM_QUIT. Capture the exact message
    // immediately on return, before joining, so an unrelated wake cannot pass.
    std::thread waiter([&] {
        tenriff::timing::PreciseWait wait;
        MSG message{};
        PeekMessageW(&message, nullptr, kMessage, kMessage, PM_NOREMOVE);
        queue_ready.set_value(GetCurrentThreadId());
        const auto start = std::chrono::steady_clock::now();
        signaled = wait.wait_for_messages_ns(2'000'000'000);
        elapsed = std::chrono::steady_clock::now() - start;
        received_target = PeekMessageW(&message, nullptr, kMessage, kMessage, PM_REMOVE) &&
                          message.message == kMessage && message.wParam == kToken;
    });
    const DWORD queue_owner = owner.get();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const BOOL posted = PostThreadMessageW(queue_owner, kMessage, kToken, 0);
    waiter.join();
    CHECK(posted != FALSE);
    CHECK(signaled);
    CHECK(elapsed < std::chrono::milliseconds(1500));
    CHECK(received_target);
}
#endif
