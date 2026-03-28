#include "doctest/doctest.h"

#include "app/InputBackendStatus.h"

using tenriff::app::InputBackendRuntimeState;
using tenriff::app::InputFallbackOrigin;
using tenriff::app::format_input_backend_status_detail;
using tenriff::app::format_input_backend_status_label;
using tenriff::input::InputBackend;

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
