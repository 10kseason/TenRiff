#include "doctest/doctest.h"

#include <chrono>
#include <thread>

#include "input/InputThread.h"

TEST_CASE("raw input thread can stop and restart without silently failing") {
#if defined(_WIN32)
    using namespace std::chrono_literals;

    tenriff::input::InputThread input_thread;
    tenriff::input::InputThreadConfig config;
    config.backend = tenriff::input::InputBackend::RawInput;
    config.raw_input.register_keyboard = true;
    config.raw_input.input_sink = true;
    config.raw_input.no_legacy = false;

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
