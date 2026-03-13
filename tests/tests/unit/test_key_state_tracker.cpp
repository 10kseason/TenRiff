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

TEST_CASE("KeyStateTracker filters chatter within debounce window") {
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

    // Release at t=2ms (within debounce window) - should be filtered as chatter.
    InputEvent release{};
    release.keycode = 0x44;
    release.state = InputState::Released;
    release.input_time_ns = 2'000'000;
    result = tracker.process(release);
    CHECK(!result.has_value());

    // Release at t=10ms (outside debounce window) - should pass.
    release.input_time_ns = 10'000'000;
    // Need to reset first since state wasn't updated due to filter.
    tracker.reset();
    
    // Re-press and then release after window.
    result = tracker.process(press1);
    REQUIRE(result.has_value());
    
    release.input_time_ns = 10'000'000;  // 10ms after
    result = tracker.process(release);
    REQUIRE(result.has_value());
    CHECK(result.value().state == InputState::Released);
}

TEST_CASE("KeyStateTracker tracks pressed key count") {
    KeyStateTracker tracker;

    CHECK(tracker.pressed_count() == 0);

    // Press key 1.
    InputEvent press1{};
    press1.keycode = 0x44;
    press1.state = InputState::Pressed;
    press1.input_time_ns = 1'000'000;
    tracker.process(press1);
    CHECK(tracker.pressed_count() == 1);
    CHECK(tracker.is_key_pressed(0x44));

    // Press key 2.
    InputEvent press2{};
    press2.keycode = 0x46;
    press2.state = InputState::Pressed;
    press2.input_time_ns = 2'000'000;
    tracker.process(press2);
    CHECK(tracker.pressed_count() == 2);
    CHECK(tracker.is_key_pressed(0x46));

    // Release key 1.
    InputEvent release1{};
    release1.keycode = 0x44;
    release1.state = InputState::Released;
    release1.input_time_ns = 10'000'000;
    tracker.process(release1);
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
    tracker.process(press);
    
    CHECK(tracker.pressed_count() == 1);
    CHECK(tracker.is_key_pressed(0x44));

    tracker.reset();

    CHECK(tracker.pressed_count() == 0);
    CHECK(!tracker.is_key_pressed(0x44));
}
