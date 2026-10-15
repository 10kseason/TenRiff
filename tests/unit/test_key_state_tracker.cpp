// Unit tests for KeyStateTracker - debounce and duplicate filtering.
#include "doctest/doctest.h"

#include <limits>

#include "input/GamepadInput.h"
#include "input/KeyStateTracker.h"

using namespace tenriff::input;

namespace {
InputEvent key_edge(uint32_t key, InputSourceToken source, InputState state, int64_t time_ns) {
    InputEvent event{};
    event.keycode = key;
    event.device_id = source;
    event.state = state;
    event.input_time_ns = time_ns;
    return event;
}
}  // namespace

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

TEST_CASE("KeyStateTracker zero debounce preserves release and rapid re-press edges") {
    KeyStateConfig config;
    CHECK(config.debounce_window_ns == 0);
    KeyStateTracker tracker(config);

    // Press at t=0.
    InputEvent press1{};
    press1.keycode = 0x44;
    press1.state = InputState::Pressed;
    press1.input_time_ns = 0;
    auto result = tracker.process(press1);
    REQUIRE(result.has_value());

    // Release at t=2ms must pass so the key cannot get stuck down.
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

TEST_CASE("KeyStateTracker positive debounce releases immediately and never revives a suppressed held press") {
    for (const int64_t window : {1'000'000LL, 5'000'000LL, 20'000'000LL}) {
        for (const InputSourceToken source : {kPollingAggregateDeviceId, InputSourceToken{0x1001}}) {
            KeyStateTracker tracker(KeyStateConfig{window});
            const auto edge = [&](InputState state, int64_t time) {
                return tracker.process(key_edge(0x41, source, state, time));
            };
            REQUIRE(edge(InputState::Pressed, 0).has_value());
            auto released = edge(InputState::Released, 100);
            REQUIRE(released.has_value());
            CHECK(released->state == InputState::Released);
            CHECK(released->input_time_ns == 100);
            CHECK_FALSE(edge(InputState::Pressed, 100 + window / 2).has_value());
            CHECK_FALSE(tracker.is_key_pressed(0x41));
            CHECK(tracker.pressed_count() == 0);
            // An OS held repeat after the deadline is still the rejected DOWN.
            CHECK_FALSE(edge(InputState::Pressed, 100 + window * 2).has_value());
            CHECK_FALSE(edge(InputState::Released, 100 + window * 3).has_value());
            CHECK_FALSE(edge(InputState::Released, 100 + window * 4 - 1).has_value());
            // Repeated UP did not restart the window; its exact boundary is valid.
            REQUIRE(edge(InputState::Pressed, 100 + window * 4).has_value());
            REQUIRE(edge(InputState::Released, 101 + window * 4).has_value());
            CHECK_FALSE(tracker.is_key_pressed(0x41));
            CHECK(tracker.pressed_count() == 0);
        }
    }
}

TEST_CASE("KeyStateTracker debounce is measured from release with an inclusive allowed boundary at twenty ms") {
    constexpr int64_t window = 20'000'000;
    constexpr int64_t released_at = 100'000'000;
    for (const int64_t elapsed : {window - 1, window, window + 1}) {
        KeyStateTracker tracker(KeyStateConfig{window});
        REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, 0)).has_value());
        REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Released, released_at)).has_value());
        CHECK(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, released_at + elapsed)).has_value() ==
              (elapsed >= window));
        CHECK(tracker.is_key_pressed(0x41) == (elapsed >= window));
        const auto release = tracker.process(key_edge(0x41, 0x1001, InputState::Released, released_at + elapsed + 1));
        CHECK(release.has_value() == (elapsed >= window));
        CHECK_FALSE(tracker.is_key_pressed(0x41));
    }
}

TEST_CASE("KeyStateTracker positive debounce preserves independent physical keyboard ownership") {
    KeyStateTracker tracker(KeyStateConfig{20'000'000});
    constexpr InputSourceToken keyboard_a = 0x101;
    constexpr InputSourceToken keyboard_b = 0x201; // Full token identity, same low byte.
    const auto edge = [&](InputSourceToken source, InputState state, int64_t time) {
        return tracker.process(key_edge(0x41, source, state, time));
    };
    REQUIRE(edge(keyboard_a, InputState::Pressed, 0).has_value());
    REQUIRE(edge(keyboard_a, InputState::Released, 1'000'000).has_value());
    CHECK_FALSE(edge(keyboard_a, InputState::Pressed, 2'000'000).has_value());
    REQUIRE(edge(keyboard_b, InputState::Pressed, 3'000'000).has_value());
    CHECK_FALSE(edge(keyboard_a, InputState::Released, 4'000'000).has_value());
    CHECK(tracker.is_key_pressed(0x41));
    CHECK(tracker.pressed_count() == 1);
    REQUIRE(edge(keyboard_b, InputState::Released, 5'000'000).has_value());
    CHECK_FALSE(tracker.is_key_pressed(0x41));
    REQUIRE(edge(keyboard_a, InputState::Pressed, 24'000'000).has_value());
    CHECK_FALSE(edge(keyboard_b, InputState::Pressed, 25'000'000).has_value());
    CHECK_FALSE(edge(keyboard_a, InputState::Released, 25'000'001).has_value());
    CHECK(tracker.is_key_pressed(0x41));
    REQUIRE(edge(keyboard_b, InputState::Released, 25'000'002).has_value());
    CHECK(tracker.pressed_count() == 0);
}

TEST_CASE("KeyStateTracker debounce survives both RawInput and polling arrival orders without delayed revival") {
    for (const int64_t window : {1'000'000LL, 20'000'000LL}) {
        for (const bool polling_first : {false, true}) {
            for (const bool bounce_polling_first : {false, true}) {
                KeyStateTracker tracker(KeyStateConfig{window});
                const auto edge = [&](InputSourceToken source, InputState state, int64_t time) {
                    return tracker.process(key_edge(0x41, source, state, time));
                };
                constexpr InputSourceToken raw = 0x1001;
                const auto first = polling_first ? kPollingAggregateDeviceId : raw;
                const auto second = polling_first ? raw : kPollingAggregateDeviceId;
                REQUIRE(edge(first, InputState::Pressed, 0).has_value());
                CHECK_FALSE(edge(second, InputState::Pressed, 100).has_value());
                CHECK(tracker.pressed_count() == 1);
                REQUIRE(edge(raw, InputState::Released, 200).has_value());
                CHECK_FALSE(edge(kPollingAggregateDeviceId, InputState::Released, 300).has_value());

                const auto bounce_first = bounce_polling_first ? kPollingAggregateDeviceId : raw;
                const auto bounce_second = bounce_polling_first ? raw : kPollingAggregateDeviceId;
                CHECK_FALSE(edge(bounce_first, InputState::Pressed, 400).has_value());
                CHECK_FALSE(edge(bounce_second, InputState::Pressed, window + 500).has_value());
                CHECK_FALSE(edge(raw, InputState::Pressed, window * 2 + 600).has_value());
                CHECK_FALSE(edge(kPollingAggregateDeviceId, InputState::Pressed, window * 2 + 600).has_value());
                CHECK_FALSE(tracker.is_key_pressed(0x41));
                CHECK(tracker.pressed_count() == 0);
                CHECK_FALSE(edge(raw, InputState::Released, window * 3 + 700).has_value());
                CHECK_FALSE(edge(kPollingAggregateDeviceId, InputState::Released, window * 3 + 800).has_value());
                REQUIRE(edge(raw, InputState::Pressed, window * 4 + 700).has_value());
                CHECK_FALSE(edge(kPollingAggregateDeviceId, InputState::Pressed, window * 4 + 701).has_value());
                REQUIRE(edge(raw, InputState::Released, window * 4 + 702).has_value());
                CHECK(tracker.pressed_count() == 0);
            }
        }
    }
}

TEST_CASE("KeyStateTracker suppressed polling-only press cannot revive through its first late RawInput copy") {
    constexpr int64_t window = 20'000'000;
    KeyStateTracker tracker(KeyStateConfig{window});
    REQUIRE(tracker.process(key_edge(0x41, 0, InputState::Pressed, 0)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0, InputState::Released, 100)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0, InputState::Pressed, 200)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, window * 2)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x1001, InputState::Released, window * 2 + 100)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0, InputState::Released, window * 2 + 200)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, window * 3 + 100)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Released, window * 3 + 101)).has_value());
    CHECK(tracker.pressed_count() == 0);
}

TEST_CASE("KeyStateTracker first RawInput edge inherits an unattributed polling release window") {
    constexpr int64_t window = 20'000'000;
    for (const int64_t elapsed : {window - 1, window}) {
        KeyStateTracker tracker(KeyStateConfig{window});
        REQUIRE(tracker.process(key_edge(0x41, 0, InputState::Pressed, 0)).has_value());
        REQUIRE(tracker.process(key_edge(0x41, 0, InputState::Released, 100)).has_value());
        CHECK(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, 100 + elapsed)).has_value() ==
              (elapsed >= window));
        CHECK(tracker.process(key_edge(0x41, 0x1001, InputState::Released, 101 + elapsed)).has_value() ==
              (elapsed >= window));
        CHECK_FALSE(tracker.is_key_pressed(0x41));
    }
}

TEST_CASE("KeyStateTracker another keyboard release cannot revive a suppressed held key through polling") {
    KeyStateTracker tracker(KeyStateConfig{20'000'000});
    REQUIRE(tracker.process(key_edge(0x41, 0x101, InputState::Pressed, 0)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x101, InputState::Released, 1'000'000)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x101, InputState::Pressed, 2'000'000)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x201, InputState::Pressed, 3'000'000)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x201, InputState::Released, 4'000'000)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0, InputState::Pressed, 30'000'000)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x101, InputState::Pressed, 31'000'000)).has_value());
    CHECK_FALSE(tracker.is_key_pressed(0x41));
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x101, InputState::Released, 32'000'000)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0, InputState::Released, 33'000'000)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0, InputState::Pressed, 52'000'000)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x101, InputState::Pressed, 52'000'001)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x101, InputState::Released, 52'000'002)).has_value());
    CHECK(tracker.pressed_count() == 0);
}

TEST_CASE("KeyStateTracker unattributed polling suppression does not latch other recently released keyboards") {
    KeyStateTracker tracker(KeyStateConfig{5'000'000});
    REQUIRE(tracker.process(key_edge(0x41, 0x101, InputState::Pressed, 0)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x201, InputState::Pressed, 100'000)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x101, InputState::Released, 1'000'000)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x201, InputState::Released, 2'000'000)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0, InputState::Pressed, 3'000'000)).has_value());
    // Raw A's missing DOWN is resolved by its UP. B never acquired that DOWN.
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x101, InputState::Released, 4'000'000)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x201, InputState::Pressed, 10'000'000)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x201, InputState::Released, 10'000'001)).has_value());
    CHECK(tracker.pressed_count() == 0);
}

TEST_CASE("KeyStateTracker polling release clears accepted and suppressed sources with debounce enabled") {
    constexpr int64_t window = 20'000'000;
    KeyStateTracker tracker(KeyStateConfig{window});
    REQUIRE(tracker.process(key_edge(0x41, 0x101, InputState::Pressed, 0)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x201, InputState::Pressed, 10)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0, InputState::Released, 100)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x101, InputState::Pressed, 200)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x201, InputState::Pressed, 210)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0, InputState::Released, 300)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0, InputState::Released, window + 299)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x101, InputState::Pressed, window + 300)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x101, InputState::Released, window + 301)).has_value());
    CHECK(tracker.pressed_count() == 0);
}

TEST_CASE("KeyStateTracker reset removes held suppression and release deadlines") {
    for (const int phase : {0, 1, 2}) {
        KeyStateTracker tracker(KeyStateConfig{20'000'000});
        REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, 0)).has_value());
        if (phase > 0) REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Released, 1)).has_value());
        if (phase > 1) CHECK_FALSE(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, 2)).has_value());
        tracker.reset();
        CHECK_FALSE(tracker.is_key_pressed(0x41));
        CHECK(tracker.pressed_count() == 0);
        REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, 3)).has_value());
        REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Released, 4)).has_value());
    }
}

TEST_CASE("KeyStateTracker debounce tolerates clock rollback and full-width timestamp differences") {
    KeyStateTracker tracker(KeyStateConfig{20'000'000});
    constexpr int64_t low = (std::numeric_limits<int64_t>::min)();
    constexpr int64_t high = (std::numeric_limits<int64_t>::max)();
    REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, high - 10)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Released, high - 5)).has_value());
    // A backwards clock starts a new interval rather than waiting for the old epoch.
    REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, low + 5)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Released, low + 6)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, low + 7)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, 0x1001, InputState::Released, low + 8)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Pressed, high)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, 0x1001, InputState::Released, high)).has_value());
    CHECK(tracker.pressed_count() == 0);
}

TEST_CASE("KeyStateTracker debounce history is isolated by key even with the XInput aggregate token") {
    KeyStateTracker tracker(KeyStateConfig{20'000'000});
    const auto source = kPollingAggregateDeviceId;
    const auto pad_a = gamepad::kKeycodeBase + 10;
    REQUIRE(tracker.process(key_edge(0x41, source, InputState::Pressed, 0)).has_value());
    REQUIRE(tracker.process(key_edge(0x41, source, InputState::Released, 100)).has_value());
    CHECK_FALSE(tracker.process(key_edge(0x41, source, InputState::Pressed, 200)).has_value());
    REQUIRE(tracker.process(key_edge(0x42, source, InputState::Pressed, 201)).has_value());
    // XInput's real worker has its own GamepadTracker. Sharing the aggregate
    // source token must also remain safe for callers of this generic tracker.
    REQUIRE(tracker.process(key_edge(pad_a, source, InputState::Pressed, 202)).has_value());
    CHECK(tracker.pressed_count() == 2);
    CHECK_FALSE(tracker.process(key_edge(0x41, source, InputState::Released, 203)).has_value());
    CHECK(tracker.is_key_pressed(pad_a));
    REQUIRE(tracker.process(key_edge(pad_a, source, InputState::Released, 204)).has_value());
    REQUIRE(tracker.process(key_edge(0x42, source, InputState::Released, 205)).has_value());
    CHECK(tracker.pressed_count() == 0);
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
