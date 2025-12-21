#include "doctest/doctest.h"

#include "input/InputEvent.h"
#include "input/SPSCQueue.h"
#include "timing/ClockSync.h"

TEST_CASE("SPSCQueue preserves order and drops when full") {
    tenriff::input::SPSCQueue<int, 4> queue;
    CHECK(queue.capacity() == 3);

    CHECK(queue.push(1));
    CHECK(queue.push(2));
    CHECK(queue.push(3));
    CHECK_FALSE(queue.push(4));

    auto first = queue.pop();
    CHECK(first.has_value());
    if (!first.has_value()) {
        return;
    }
    CHECK(*first == 1);

    auto second = queue.pop();
    CHECK(second.has_value());
    if (!second.has_value()) {
        return;
    }
    CHECK(*second == 2);

    CHECK(queue.push(4));
    auto third = queue.pop();
    CHECK(third.has_value());
    if (!third.has_value()) {
        return;
    }
    CHECK(*third == 3);
}

TEST_CASE("ClockSync estimates slope and intercept for input to audio mapping") {
    tenriff::timing::ClockSync sync(4);
    sync.add_sample(0, 0);
    sync.add_sample(1'000'000'000, 48000);

    auto estimate = sync.input_to_audio_samples(500'000'000);
    CHECK(estimate.has_value());
    if (!estimate.has_value()) {
        return;
    }
    CHECK(*estimate == 24000);
}

TEST_CASE("ClockSync drops oldest samples when exceeding capacity") {
    tenriff::timing::ClockSync sync(2);
    sync.add_sample(0, 0);
    sync.add_sample(1'000'000'000, 48000);
    sync.add_sample(2'000'000'000, 96050);

    auto mapped = sync.input_to_audio_samples(1'500'000'000);
    CHECK(mapped.has_value());
    if (!mapped.has_value()) {
        return;
    }
    CHECK(*mapped > 70000);
}

