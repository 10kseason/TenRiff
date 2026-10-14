#include "doctest/doctest.h"

#include <array>
#include <cstdint>
#include <optional>

#include "input/GamepadInput.h"

using tenriff::input::InputState;
namespace pad = tenriff::input::gamepad;

namespace {
using Pads = std::array<std::optional<pad::GamepadState>, pad::kControllerCount>;
constexpr std::array<std::uint16_t, 14> button_bits{
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040,
    0x0080, 0x0100, 0x0200, 0x1000, 0x2000, 0x4000, 0x8000,
};

pad::GamepadState one_control(std::size_t index) {
    pad::GamepadState state;
    if (index < button_bits.size()) state.buttons = button_bits[index];
    else if (index == 14) state.left_trigger = 31;
    else state.right_trigger = 31;
    return state;
}
}  // namespace

TEST_CASE("gamepad keycodes have stable names and an isolated bounded range") {
    const std::array<std::string_view, 16> expected{
        "PadUp", "PadDown", "PadLeft", "PadRight", "PadStart", "PadBack",
        "PadLS", "PadRS", "PadLB", "PadRB", "PadA", "PadB", "PadX", "PadY", "PadLT", "PadRT"};
    CHECK(pad::kKeycodeBase == 0x20000u);
    CHECK(pad::kKeyNames == expected);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto keycode = pad::kKeycodeBase + static_cast<std::uint32_t>(index);
        REQUIRE(pad::keycode_index(keycode).has_value());
        CHECK(*pad::keycode_index(keycode) == index);
        CHECK(pad::is_gamepad_keycode(keycode));
    }
    for (const auto keycode : {0u, 0x41u, pad::kKeycodeBase - 1u, pad::kKeycodeBase + 16u, 0xffffffffu}) {
        CHECK_FALSE(pad::keycode_index(keycode).has_value());
        CHECK_FALSE(pad::is_gamepad_keycode(keycode));
    }
}

TEST_CASE("gamepad decoder maps every XInput button and trigger without reserved bits") {
    for (std::size_t index = 0; index < pad::kControlCount; ++index)
        CHECK(pad::decode_mask(one_control(index)) == (1u << index));
    CHECK(pad::decode_mask({0x0c00, 0, 0}) == 0);
    CHECK(pad::decode_mask({0xffff, 255, 255}) == 0xffff);
    CHECK(pad::decode_mask({}) == 0);
}

TEST_CASE("gamepad trigger threshold is strictly above thirty for both triggers") {
    for (int value = 0; value <= 255; ++value) {
        const auto trigger = static_cast<std::uint8_t>(value);
        const auto expected = value > 30;
        CHECK(pad::decode_mask({0, trigger, 0}) == (expected ? 0x4000 : 0));
        CHECK(pad::decode_mask({0, 0, trigger}) == (expected ? 0x8000 : 0));
    }
}

TEST_CASE("gamepad tracker emits one timestamped edge per control without held repeats") {
    for (std::size_t index = 0; index < pad::kControlCount; ++index) {
        pad::GamepadTracker tracker;
        Pads states{};
        states[0] = pad::GamepadState{};
        CHECK(tracker.update(states, true, 1).empty());
        states[0] = one_control(index);
        auto events = tracker.update(states, true, 2);
        REQUIRE(events.size() == 1);
        CHECK(events[0].keycode == pad::kKeycodeBase + index);
        CHECK(events[0].state == InputState::Pressed);
        CHECK(events[0].input_time_ns == 2);
        CHECK(events[0].device_id == tenriff::input::kPollingAggregateDeviceId);
        CHECK(tracker.current_mask() == (1u << index));
        CHECK(tracker.update(states, true, 3).empty());
        states[0] = pad::GamepadState{};
        events = tracker.update(states, true, 4);
        REQUIRE(events.size() == 1);
        CHECK(events[0].keycode == pad::kKeycodeBase + index);
        CHECK(events[0].state == InputState::Released);
        CHECK(events[0].input_time_ns == 4);
        CHECK(tracker.current_mask() == 0);
        CHECK(tracker.update(states, true, 5).empty());
    }
}

TEST_CASE("gamepad tracker supports simultaneous controls and releases all on disconnect") {
    pad::GamepadTracker tracker;
    Pads states{};
    states[3] = pad::GamepadState{};
    CHECK(tracker.update(states, true, 10).empty());
    states[3] = pad::GamepadState{0xffff, 255, 255};
    auto events = tracker.update(states, true, 20);
    REQUIRE(events.size() == 16);
    CHECK(tracker.current_mask() == 0xffff);
    for (std::size_t index = 0; index < events.size(); ++index) {
        CHECK(events[index].keycode == pad::kKeycodeBase + index);
        CHECK(events[index].state == InputState::Pressed);
    }
    states[3].reset();
    events = tracker.update(states, true, 30);
    REQUIRE(events.size() == 16);
    for (const auto& event : events) {
        CHECK(event.state == InputState::Released);
        CHECK(event.input_time_ns == 30);
    }
    CHECK(tracker.current_mask() == 0);
    CHECK(tracker.update(states, true, 40).empty());
}

TEST_CASE("gamepad tracker aggregates controller ownership until the last release") {
    pad::GamepadTracker tracker;
    Pads states{};
    states[0] = states[1] = pad::GamepadState{};
    CHECK(tracker.update(states, true, 0).empty());
    states[0] = one_control(10);
    REQUIRE(tracker.update(states, true, 1).size() == 1);
    states[1] = one_control(10);
    CHECK(tracker.update(states, true, 2).empty());
    states[0].reset();
    CHECK(tracker.update(states, true, 3).empty());
    CHECK(tracker.current_mask() == (1u << 10));
    states[1] = one_control(11);
    const auto events = tracker.update(states, true, 4);
    REQUIRE(events.size() == 2);
    CHECK(events[0].keycode == pad::kKeycodeBase + 10);
    CHECK(events[0].state == InputState::Released);
    CHECK(events[1].keycode == pad::kKeycodeBase + 11);
    CHECK(events[1].state == InputState::Pressed);
    CHECK(tracker.current_mask() == (1u << 11));
}

TEST_CASE("gamepad tracker suppresses initially held controls until each is released") {
    pad::GamepadTracker tracker;
    Pads states{};
    states[0] = pad::GamepadState{0xffff, 255, 255};
    CHECK(tracker.update(states, true, 1).empty());
    CHECK(tracker.current_mask() == 0);
    CHECK(tracker.update(states, true, 2).empty());
    states[0] = pad::GamepadState{0xefff, 255, 255}; // Release A only.
    CHECK(tracker.update(states, true, 3).empty());
    states[0] = pad::GamepadState{0xffff, 255, 255};
    const auto events = tracker.update(states, true, 4);
    REQUIRE(events.size() == 1);
    CHECK(events[0].keycode == pad::kKeycodeBase + 10);
    CHECK(events[0].state == InputState::Pressed);
    CHECK(tracker.current_mask() == (1u << 10));
}

TEST_CASE("gamepad new connection suppression is per slot even during aggregate handoffs") {
    pad::GamepadTracker tracker;
    Pads states{};
    states[0] = pad::GamepadState{};
    CHECK(tracker.update(states, true, 0).empty());
    states[0] = one_control(10);
    REQUIRE(tracker.update(states, true, 1).size() == 1);
    states[1] = one_control(10); // A newly connected held A cannot take ownership.
    CHECK(tracker.update(states, true, 2).empty());
    states[0] = pad::GamepadState{};
    auto events = tracker.update(states, true, 3);
    REQUIRE(events.size() == 1);
    CHECK(events[0].state == InputState::Released);
    CHECK(tracker.current_mask() == 0);
    CHECK(tracker.update(states, true, 4).empty());
    states[1] = pad::GamepadState{};
    CHECK(tracker.update(states, true, 5).empty());
    states[1] = one_control(10);
    events = tracker.update(states, true, 6);
    REQUIRE(events.size() == 1);
    CHECK(events[0].state == InputState::Pressed);
}

TEST_CASE("gamepad disconnect and reconnect suppress a held trigger until released") {
    pad::GamepadTracker tracker;
    Pads states{};
    states[2] = pad::GamepadState{};
    CHECK(tracker.update(states, true, 0).empty());
    states[2] = one_control(14);
    REQUIRE(tracker.update(states, true, 1).size() == 1);
    states[2].reset();
    auto events = tracker.update(states, true, 2);
    REQUIRE(events.size() == 1);
    CHECK(events[0].state == InputState::Released);
    states[2] = one_control(14);
    CHECK(tracker.update(states, true, 3).empty());
    states[2]->left_trigger = 30;
    CHECK(tracker.update(states, true, 4).empty());
    states[2]->left_trigger = 31;
    events = tracker.update(states, true, 5);
    REQUIRE(events.size() == 1);
    CHECK(events[0].keycode == pad::kKeycodeBase + 14);
    CHECK(events[0].state == InputState::Pressed);
}

TEST_CASE("gamepad focus loss releases input and focus regain waits for held controls") {
    pad::GamepadTracker tracker;
    Pads states{};
    states[0] = states[1] = pad::GamepadState{};
    CHECK(tracker.update(states, true, 0).empty());
    states[0] = one_control(10);
    states[1] = one_control(15);
    REQUIRE(tracker.update(states, true, 1).size() == 2);
    auto events = tracker.update(states, false, 2);
    REQUIRE(events.size() == 2);
    for (const auto& event : events) CHECK(event.state == InputState::Released);
    CHECK(tracker.current_mask() == 0);
    CHECK(tracker.update(states, false, 3).empty());
    CHECK(tracker.update(states, true, 4).empty());
    states[0] = pad::GamepadState{};
    CHECK(tracker.update(states, true, 5).empty());
    states[0] = one_control(10);
    events = tracker.update(states, true, 6);
    REQUIRE(events.size() == 1);
    CHECK(events[0].keycode == pad::kKeycodeBase + 10);
    CHECK(tracker.current_mask() == (1u << 10));
    states[1] = pad::GamepadState{};
    CHECK(tracker.update(states, true, 7).empty());
    states[1] = one_control(15);
    events = tracker.update(states, true, 8);
    REQUIRE(events.size() == 1);
    CHECK(events[0].keycode == pad::kKeycodeBase + 15);
}

TEST_CASE("gamepad controls pressed while unfocused cannot activate on focus regain") {
    pad::GamepadTracker tracker;
    Pads states{};
    states[0] = pad::GamepadState{};
    CHECK(tracker.update(states, false, 0).empty());
    states[0] = one_control(12);
    CHECK(tracker.update(states, false, 1).empty());
    CHECK(tracker.update(states, true, 2).empty());
    CHECK(tracker.current_mask() == 0);
    states[0] = pad::GamepadState{};
    CHECK(tracker.update(states, true, 3).empty());
    states[0] = one_control(12);
    const auto events = tracker.update(states, true, 4);
    REQUIRE(events.size() == 1);
    CHECK(events[0].state == InputState::Pressed);
    CHECK(events[0].keycode == pad::kKeycodeBase + 12);
}
