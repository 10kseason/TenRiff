#include "doctest/doctest.h"

#include "input/RawInputHealthProbe.h"

TEST_CASE("raw input health probe triggers fallback after polled activity without raw events") {
    tenriff::input::RawInputHealthProbe probe(100);
    tenriff::input::InputThreadHealthSnapshot snapshot;
    snapshot.backend = tenriff::input::InputBackend::RawInput;
    snapshot.allowed_event_count = 10;
    snapshot.queue_push_count = 8;
    snapshot.last_allowed_event_time_ns = 500;
    snapshot.last_queue_push_time_ns = 500;

    probe.note_polled_change(1000, snapshot);
    CHECK_FALSE(probe.should_trigger_fallback(1050, snapshot));
    CHECK(probe.should_trigger_fallback(1100, snapshot));
}

TEST_CASE("raw input health probe cancels fallback when raw events resume") {
    tenriff::input::RawInputHealthProbe probe(100);
    tenriff::input::InputThreadHealthSnapshot snapshot;
    snapshot.backend = tenriff::input::InputBackend::RawInput;
    snapshot.allowed_event_count = 4;
    snapshot.queue_push_count = 4;
    snapshot.last_allowed_event_time_ns = 200;
    snapshot.last_queue_push_time_ns = 200;

    probe.note_polled_change(1000, snapshot);

    auto resumed = snapshot;
    resumed.allowed_event_count = 5;
    resumed.queue_push_count = 5;
    resumed.last_allowed_event_time_ns = 1010;
    resumed.last_queue_push_time_ns = 1010;

    CHECK_FALSE(probe.should_trigger_fallback(1150, resumed));
    CHECK_FALSE(probe.pending());
}

TEST_CASE("raw input health probe ignores a polled change already explained by a recent raw event") {
    tenriff::input::RawInputHealthProbe probe(100);
    tenriff::input::InputThreadHealthSnapshot snapshot;
    snapshot.backend = tenriff::input::InputBackend::RawInput;
    snapshot.allowed_event_count = 5;
    snapshot.queue_push_count = 5;
    snapshot.last_allowed_event_time_ns = 950;
    snapshot.last_queue_push_time_ns = 950;

    probe.note_polled_change(1000, snapshot);

    CHECK_FALSE(probe.pending());
    CHECK_FALSE(probe.should_trigger_fallback(1200, snapshot));
}
