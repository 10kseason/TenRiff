#include "doctest/doctest.h"

#include "app/InputBackendStatus.h"

using tenriff::app::InputBackendRuntimeState;
using tenriff::app::InputBackendFallbackPolicy;
using tenriff::app::InputFallbackOrigin;
using tenriff::app::input_backend_for_event;
using tenriff::app::sync_runtime_input_backend_state;
using tenriff::app::format_input_backend_status_detail;
using tenriff::app::format_input_backend_status_label;
using tenriff::input::InputBackend;
using tenriff::input::InputEvent;
using tenriff::input::kPollingAggregateDeviceId;

TEST_CASE("input backend status label shows the effective backend without fallback metadata") {
    InputBackendRuntimeState state;
    state.configured_backend = InputBackend::Polling;
    state.effective_backend = InputBackend::Polling;

    CHECK(format_input_backend_status_label(state) == "Input backend: Polling");
    CHECK(format_input_backend_status_detail(state).empty());
}

TEST_CASE("input backend status detail keeps fallback origin and timestamp") {
    InputBackendRuntimeState state;
    state.configured_backend = InputBackend::RawInput;
    state.effective_backend = InputBackend::Polling;
    state.auto_fallback = true;
    state.fallback_origin = InputFallbackOrigin::Replay;
    state.fallback_timestamp_utc = "20260328_123456Z";
    state.fallback_reason = "RawInput inactive for bound gameplay keys, switching to Polling";

    CHECK(format_input_backend_status_label(state) == "Input backend: Polling (auto-fallback / replay)");
    CHECK(format_input_backend_status_detail(state) ==
          "Auto-fallback: RawInput -> Polling / origin replay / at 20260328_123456Z / reason "
          "RawInput inactive for bound gameplay keys, switching to Polling");
}

TEST_CASE("input backend status keeps rawinput effective when polling shadow events are observed") {
    InputBackendRuntimeState state;
    state.configured_backend = InputBackend::RawInput;
    state.effective_backend = InputBackend::RawInput;

    InputEvent polled_event{};
    polled_event.device_id = kPollingAggregateDeviceId;
    CHECK(input_backend_for_event(polled_event) == InputBackend::Polling);

    sync_runtime_input_backend_state(state, polled_event, InputFallbackOrigin::Gameplay);
    CHECK(state.effective_backend == InputBackend::RawInput);
    CHECK_FALSE(state.auto_fallback);
    CHECK(state.fallback_origin == InputFallbackOrigin::None);
    CHECK(state.fallback_reason.empty());
    CHECK(state.fallback_timestamp_utc.empty());
}

TEST_CASE("input backend status preserves startup fallback diagnostics across later polling events") {
    InputBackendRuntimeState state;
    state.configured_backend = InputBackend::RawInput;
    state.effective_backend = InputBackend::Polling;
    state.auto_fallback = true;
    state.fallback_origin = InputFallbackOrigin::Gameplay;
    state.fallback_reason = "RawInput start failed; Polling fallback kept gameplay input active.";
    state.fallback_timestamp_utc = "20260331_010203Z";

    InputEvent polled_event{};
    polled_event.device_id = kPollingAggregateDeviceId;

    sync_runtime_input_backend_state(state, polled_event, InputFallbackOrigin::Gameplay);
    CHECK(state.effective_backend == InputBackend::Polling);
    CHECK(state.auto_fallback);
    CHECK(state.fallback_origin == InputFallbackOrigin::Gameplay);
    CHECK(state.fallback_reason == "RawInput start failed; Polling fallback kept gameplay input active.");
    CHECK(state.fallback_timestamp_utc == "20260331_010203Z");
}

TEST_CASE("input backend status keeps confirmed fallback across stale rawinput events") {
    InputBackendRuntimeState state;
    state.configured_backend = InputBackend::RawInput;
    state.effective_backend = InputBackend::Polling;
    state.auto_fallback = true;
    state.fallback_origin = InputFallbackOrigin::Gameplay;
    state.fallback_reason = "RawInput start failed; Polling fallback kept gameplay input active.";
    state.fallback_timestamp_utc = "20260331_010203Z";

    InputEvent raw_event{};
    raw_event.device_id = 0x1001;

    sync_runtime_input_backend_state(state, raw_event, InputFallbackOrigin::Gameplay);
    CHECK(state.effective_backend == InputBackend::Polling);
    CHECK(state.auto_fallback);
    CHECK(state.fallback_origin == InputFallbackOrigin::Gameplay);
    CHECK(state.fallback_reason == "RawInput start failed; Polling fallback kept gameplay input active.");
    CHECK(state.fallback_timestamp_utc == "20260331_010203Z");
}

TEST_CASE("runtime input fallback stays on polling without rewriting the configured backend") {
    InputBackendFallbackPolicy policy;

    CHECK(policy.rawinput_enabled(true));
    CHECK_FALSE(policy.polling_latched());

    policy.latch_polling(InputFallbackOrigin::Menu,
                         "RawInput registration was lost",
                         "20260719_070000Z");

    CHECK(policy.polling_latched());
    CHECK_FALSE(policy.rawinput_enabled(true));
    const auto fallback = policy.runtime_state(true);
    CHECK(fallback.configured_backend == InputBackend::RawInput);
    CHECK(fallback.effective_backend == InputBackend::Polling);
    CHECK(fallback.auto_fallback);
    CHECK(fallback.fallback_origin == InputFallbackOrigin::Menu);
    CHECK(fallback.fallback_reason == "RawInput registration was lost");
    CHECK(fallback.fallback_timestamp_utc == "20260719_070000Z");

    policy.clear();
    CHECK(policy.rawinput_enabled(true));
    CHECK_FALSE(policy.runtime_state(true).auto_fallback);
}

TEST_CASE("runtime input fallback does not turn a configured polling backend into a fallback") {
    InputBackendFallbackPolicy policy;
    policy.latch_polling(InputFallbackOrigin::Gameplay, "RawInput failed", "20260719_070100Z");

    const auto state = policy.runtime_state(false);
    CHECK_FALSE(policy.rawinput_enabled(false));
    CHECK(state.configured_backend == InputBackend::Polling);
    CHECK(state.effective_backend == InputBackend::Polling);
    CHECK_FALSE(state.auto_fallback);
}
