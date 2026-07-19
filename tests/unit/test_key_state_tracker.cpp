// Unit tests for KeyStateTracker - debounce and duplicate filtering.
#include "doctest/doctest.h"

#include "input/KeyStateTracker.h"

using namespace tenriff::input;

TEST_CASE("KeyStateTracker filters duplicate key events") {
    KeyStateTracker tracker;

    InputEvent press1{};
    press1.keycode = 0x44;  // 'D'
    press1.state = InputState::Pressed;
    press1.input_time_ns = 1'000'000;

    // First press should pass through.
    auto result = tracker.process(press1);
    REQUIRE(result.has_value());
    CHECK(result.value().keycode == 0x44);
    CHECK(result.value().state == InputState::Pressed);

    // Duplicate press should be filtered.
    InputEvent press2 = press1;
    press2.input_time_ns = 2'000'000;
    result = tracker.process(press2);
    CHECK(!result.has_value());

    // Release should pass through.
    InputEvent release{};
    release.keycode = 0x44;
    release.state = InputState::Released;
    release.input_time_ns = 10'000'000;  // 10ms - outside debounce window
    result = tracker.process(release);
    REQUIRE(result.has_value());
    CHECK(result.value().state == InputState::Released);

    // Duplicate release should be filtered.
    InputEvent release2 = release;
    release2.input_time_ns = 20'000'000;  // 20ms
    result = tracker.process(release2);
    CHECK(!result.has_value());
}

TEST_CASE("KeyStateTracker does not swallow release edges within debounce window") {
    KeyStateConfig config;
    config.debounce_window_ns = 5'000'000;  // 5ms
    KeyStateTracker tracker(config);

    // Press at t=0.
    InputEvent press1{};
    press1.keycode = 0x44;
    press1.state = InputState::Pressed;
    press1.input_time_ns = 0;
    auto result = tracker.process(press1);
    REQUIRE(result.has_value());

    // Release at t=2ms (within debounce window) must still pass so the key cannot get stuck down.
    InputEvent release{};
    release.keycode = 0x44;
    release.state = InputState::Released;
    release.input_time_ns = 2'000'000;
    result = tracker.process(release);
    REQUIRE(result.has_value());
    CHECK(result->state == InputState::Released);
    CHECK_FALSE(tracker.is_key_pressed(0x44));

    // A quick re-press should also pass; fast jacks are gameplay input, not chatter.
    InputEvent press2 = press1;
    press2.input_time_ns = 4'000'000;
    result = tracker.process(press2);
    REQUIRE(result.has_value());
    CHECK(result->state == InputState::Pressed);
    CHECK(tracker.is_key_pressed(0x44));
}

TEST_CASE("KeyStateTracker tracks pressed key count") {
    KeyStateTracker tracker;

    CHECK(tracker.pressed_count() == 0);

    // Press key 1.
    InputEvent press1{};
    press1.keycode = 0x44;
    press1.state = InputState::Pressed;
    press1.input_time_ns = 1'000'000;
    CHECK(tracker.process(press1).has_value());
    CHECK(tracker.pressed_count() == 1);
    CHECK(tracker.is_key_pressed(0x44));

    // Press key 2.
    InputEvent press2{};
    press2.keycode = 0x46;
    press2.state = InputState::Pressed;
    press2.input_time_ns = 2'000'000;
    CHECK(tracker.process(press2).has_value());
    CHECK(tracker.pressed_count() == 2);
    CHECK(tracker.is_key_pressed(0x46));

    // Release key 1.
    InputEvent release1{};
    release1.keycode = 0x44;
    release1.state = InputState::Released;
    release1.input_time_ns = 10'000'000;
    CHECK(tracker.process(release1).has_value());
    CHECK(tracker.pressed_count() == 1);
    CHECK(!tracker.is_key_pressed(0x44));
    CHECK(tracker.is_key_pressed(0x46));
}

TEST_CASE("KeyStateTracker reset clears all state") {
    KeyStateTracker tracker;

    InputEvent press{};
    press.keycode = 0x44;
    press.state = InputState::Pressed;
    press.input_time_ns = 1'000'000;
    CHECK(tracker.process(press).has_value());
    
    CHECK(tracker.pressed_count() == 1);
    CHECK(tracker.is_key_pressed(0x44));

    tracker.reset();

    CHECK(tracker.pressed_count() == 0);
    CHECK(!tracker.is_key_pressed(0x44));
}

TEST_CASE("KeyStateTracker keeps a shared key pressed until every source releases") {
    KeyStateConfig config;
    config.debounce_window_ns = 0;
    KeyStateTracker tracker(config);

    InputEvent keyboard_a_press{};
    keyboard_a_press.keycode = 0x41;
    keyboard_a_press.state = InputState::Pressed;
    keyboard_a_press.input_time_ns = 1'000'000;
    keyboard_a_press.device_id = 0x101;
    REQUIRE(tracker.process(keyboard_a_press).has_value());

    InputEvent keyboard_b_press = keyboard_a_press;
    keyboard_b_press.input_time_ns = 1'100'000;
    keyboard_b_press.device_id = 0x201;  // Same low byte, different full-width source token.
    CHECK_FALSE(tracker.process(keyboard_b_press).has_value());
    CHECK(tracker.is_key_pressed(0x41));
    CHECK(tracker.pressed_count() == 1);

    InputEvent keyboard_a_release = keyboard_a_press;
    keyboard_a_release.state = InputState::Released;
    keyboard_a_release.input_time_ns = 2'000'000;
    CHECK_FALSE(tracker.process(keyboard_a_release).has_value());
    CHECK(tracker.is_key_pressed(0x41));
    CHECK(tracker.pressed_count() == 1);

    InputEvent keyboard_b_release = keyboard_b_press;
    keyboard_b_release.state = InputState::Released;
    keyboard_b_release.input_time_ns = 2'100'000;
    auto result = tracker.process(keyboard_b_release);
    REQUIRE(result.has_value());
    CHECK(result->state == InputState::Released);
    CHECK_FALSE(tracker.is_key_pressed(0x41));
    CHECK(tracker.pressed_count() == 0);
}

TEST_CASE("KeyStateTracker filters duplicate edges from raw and polling sources") {
    KeyStateConfig config;
    config.debounce_window_ns = 0;
    KeyStateTracker tracker(config);

    InputEvent raw_press{};
    raw_press.keycode = 0x41;
    raw_press.state = InputState::Pressed;
    raw_press.input_time_ns = 1'000'000;
    raw_press.device_id = 0x1001;
    REQUIRE(tracker.process(raw_press).has_value());

    InputEvent polled_press = raw_press;
    polled_press.input_time_ns = 1'100'000;
    polled_press.device_id = kPollingAggregateDeviceId;
    CHECK_FALSE(tracker.process(polled_press).has_value());

    InputEvent raw_release = raw_press;
    raw_release.state = InputState::Released;
    raw_release.input_time_ns = 2'000'000;
    auto result = tracker.process(raw_release);
    REQUIRE(result.has_value());
    CHECK(result->state == InputState::Released);
    CHECK_FALSE(tracker.is_key_pressed(0x41));

    InputEvent polled_release = polled_press;
    polled_release.state = InputState::Released;
    polled_release.input_time_ns = 2'100'000;
    result = tracker.process(polled_release);
    CHECK_FALSE(result.has_value());
    CHECK_FALSE(tracker.is_key_pressed(0x41));
}

TEST_CASE("KeyStateTracker lets polling release clear stale raw input sources") {
    KeyStateConfig config;
    config.debounce_window_ns = 0;
    KeyStateTracker tracker(config);

    InputEvent raw_press{};
    raw_press.keycode = 0x41;
    raw_press.state = InputState::Pressed;
    raw_press.input_time_ns = 1'000'000;
    raw_press.device_id = 0x1001;
    REQUIRE(tracker.process(raw_press).has_value());

    InputEvent polled_press = raw_press;
    polled_press.input_time_ns = 1'100'000;
    polled_press.device_id = kPollingAggregateDeviceId;
    CHECK_FALSE(tracker.process(polled_press).has_value());
    CHECK(tracker.is_key_pressed(0x41));

    InputEvent polled_release = polled_press;
    polled_release.state = InputState::Released;
    polled_release.input_time_ns = 2'000'000;
    auto result = tracker.process(polled_release);
    REQUIRE(result.has_value());
    CHECK(result->state == InputState::Released);
    CHECK_FALSE(tracker.is_key_pressed(0x41));
    CHECK(tracker.pressed_count() == 0);
}

TEST_CASE("KeyStateTracker transfers a polling-first press to the late RawInput source") {
    KeyStateConfig config;
    config.debounce_window_ns = 0;
    KeyStateTracker tracker(config);

    InputEvent polled_press{};
    polled_press.keycode = 0x41;
    polled_press.state = InputState::Pressed;
    polled_press.input_time_ns = 1'000'000;
    polled_press.device_id = kPollingAggregateDeviceId;
    auto result = tracker.process(polled_press);
    REQUIRE(result.has_value());
    CHECK(result->state == InputState::Pressed);

    InputEvent raw_press = polled_press;
    raw_press.input_time_ns = 1'100'000;
    raw_press.device_id = 0x1001;
    CHECK_FALSE(tracker.process(raw_press).has_value());
    CHECK(tracker.is_key_pressed(0x41));
    CHECK(tracker.pressed_count() == 1);

    InputEvent raw_release = raw_press;
    raw_release.state = InputState::Released;
    raw_release.input_time_ns = 1'500'000;
    result = tracker.process(raw_release);
    REQUIRE(result.has_value());
    CHECK(result->state == InputState::Released);
    CHECK_FALSE(tracker.is_key_pressed(0x41));
    CHECK(tracker.pressed_count() == 0);

    InputEvent polled_release = polled_press;
    polled_release.state = InputState::Released;
    polled_release.input_time_ns = 1'600'000;
    CHECK_FALSE(tracker.process(polled_release).has_value());
}

TEST_CASE("KeyStateTracker preserves every rapid jack edge with mixed RawInput and polling order") {
    KeyStateConfig config;
    config.debounce_window_ns = 0;
    KeyStateTracker tracker(config);
    std::size_t logical_edges = 0;

    for (int cycle = 0; cycle < 100; ++cycle) {
        const int64_t base_ns = static_cast<int64_t>(cycle) * 1'000'000;
        InputEvent raw_press{};
        raw_press.keycode = 0x41;
        raw_press.state = InputState::Pressed;
        raw_press.input_time_ns = base_ns + 100'000;
        raw_press.device_id = 0x1001;

        InputEvent polled_press = raw_press;
        polled_press.device_id = kPollingAggregateDeviceId;
        polled_press.input_time_ns = base_ns + (cycle % 2 == 0 ? 50'000 : 150'000);

        if (cycle % 2 == 0) {
            logical_edges += tracker.process(polled_press).has_value() ? 1u : 0u;
            logical_edges += tracker.process(raw_press).has_value() ? 1u : 0u;
        } else {
            logical_edges += tracker.process(raw_press).has_value() ? 1u : 0u;
            logical_edges += tracker.process(polled_press).has_value() ? 1u : 0u;
        }

        InputEvent raw_release = raw_press;
        raw_release.state = InputState::Released;
        raw_release.input_time_ns = base_ns + 400'000;
        logical_edges += tracker.process(raw_release).has_value() ? 1u : 0u;

        InputEvent polled_release = polled_press;
        polled_release.state = InputState::Released;
        polled_release.input_time_ns = base_ns + 450'000;
        logical_edges += tracker.process(polled_release).has_value() ? 1u : 0u;
    }

    CHECK(logical_edges == 200);
    CHECK_FALSE(tracker.is_key_pressed(0x41));
    CHECK(tracker.pressed_count() == 0);
}

TEST_CASE("KeyStateTracker keeps different keys independent across multiple sources") {
    KeyStateConfig config;
    config.debounce_window_ns = 0;
    KeyStateTracker tracker(config);

    InputEvent left_press{};
    left_press.keycode = 0x41;
    left_press.state = InputState::Pressed;
    left_press.input_time_ns = 1'000'000;
    left_press.device_id = 0x10;
    REQUIRE(tracker.process(left_press).has_value());

    InputEvent right_press{};
    right_press.keycode = 0x42;
    right_press.state = InputState::Pressed;
    right_press.input_time_ns = 1'100'000;
    right_press.device_id = 0x20;
    REQUIRE(tracker.process(right_press).has_value());

    CHECK(tracker.is_key_pressed(0x41));
    CHECK(tracker.is_key_pressed(0x42));
    CHECK(tracker.pressed_count() == 2);

    InputEvent left_release = left_press;
    left_release.state = InputState::Released;
    left_release.input_time_ns = 2'000'000;
    REQUIRE(tracker.process(left_release).has_value());

    CHECK_FALSE(tracker.is_key_pressed(0x41));
    CHECK(tracker.is_key_pressed(0x42));
    CHECK(tracker.pressed_count() == 1);
}
