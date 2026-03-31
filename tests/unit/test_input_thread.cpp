#include "doctest/doctest.h"

#include <chrono>
#include <thread>

#include "input/InputThread.h"

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
    tenriff::input::InputThreadConfig config;
    config.backend = tenriff::input::InputBackend::RawInput;
    config.raw_input.register_keyboard = true;
    config.raw_input.input_sink = true;
    config.raw_input.no_legacy = false;
    config.gate_policy = tenriff::input::InputGatePolicy::AlwaysAllow;

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
