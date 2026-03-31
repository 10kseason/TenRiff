#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "input/InputThread.h"

namespace tenriff::app {

enum class InputFallbackOrigin : uint8_t {
    None = 0,
    Menu,
    Gameplay,
    Replay,
};

struct InputBackendRuntimeState {
    input::InputBackend configured_backend = input::InputBackend::Polling;
    input::InputBackend effective_backend = input::InputBackend::Polling;
    bool auto_fallback = false;
    InputFallbackOrigin fallback_origin = InputFallbackOrigin::None;
    std::string fallback_reason;
    std::string fallback_timestamp_utc;
};

[[nodiscard]] inline std::string input_backend_name(input::InputBackend backend) {
    return backend == input::InputBackend::RawInput ? "RawInput" : "Polling";
}

[[nodiscard]] inline input::InputBackend input_backend_for_event(const input::InputEvent& event) {
    return event.device_id == input::kPollingAggregateDeviceId
               ? input::InputBackend::Polling
               : input::InputBackend::RawInput;
}

[[nodiscard]] inline std::string input_fallback_origin_label(InputFallbackOrigin origin, bool korean = false) {
    switch (origin) {
        case InputFallbackOrigin::Menu: return korean ? "메뉴" : "menu";
        case InputFallbackOrigin::Gameplay: return korean ? "게임플레이" : "gameplay";
        case InputFallbackOrigin::Replay: return korean ? "리플레이" : "replay";
        case InputFallbackOrigin::None:
        default: return korean ? "없음" : "none";
    }
}

inline void sync_runtime_input_backend_state(InputBackendRuntimeState& state,
                                             const input::InputEvent& event,
                                             InputFallbackOrigin fallback_origin,
                                             std::string_view fallback_reason = {},
                                             std::string_view fallback_timestamp_utc = {}) {
    state.effective_backend = input_backend_for_event(event);
    if (state.configured_backend == input::InputBackend::RawInput &&
        state.effective_backend == input::InputBackend::Polling) {
        state.auto_fallback = true;
        state.fallback_origin = fallback_origin;
        if (!fallback_reason.empty()) {
            state.fallback_reason = std::string(fallback_reason);
        }
        if (!fallback_timestamp_utc.empty()) {
            state.fallback_timestamp_utc = std::string(fallback_timestamp_utc);
        }
        return;
    }

    state.auto_fallback = false;
    state.fallback_origin = InputFallbackOrigin::None;
    state.fallback_reason.clear();
    state.fallback_timestamp_utc.clear();
}

[[nodiscard]] inline std::string format_input_backend_status_label(const InputBackendRuntimeState& state,
                                                                   bool korean = false) {
    std::string label = korean ? "입력 백엔드: " : "Input backend: ";
    label += input_backend_name(state.effective_backend);
    if (state.auto_fallback) {
        label += korean ? " (자동 폴백 / " : " (auto-fallback / ";
        label += input_fallback_origin_label(state.fallback_origin, korean);
        label += ")";
    }
    return label;
}

[[nodiscard]] inline std::string format_input_backend_status_detail(const InputBackendRuntimeState& state,
                                                                    bool korean = false) {
    if (!state.auto_fallback) {
        return {};
    }

    std::string detail = korean ? "자동 폴백: " : "Auto-fallback: ";
    detail += input_backend_name(state.configured_backend);
    detail += korean ? " -> " : " -> ";
    detail += input_backend_name(state.effective_backend);
    detail += korean ? " / 위치 " : " / origin ";
    detail += input_fallback_origin_label(state.fallback_origin, korean);
    if (!state.fallback_timestamp_utc.empty()) {
        detail += korean ? " / 시각 " : " / at ";
        detail += state.fallback_timestamp_utc;
    }
    if (!state.fallback_reason.empty()) {
        detail += korean ? " / 사유 " : " / reason ";
        detail += state.fallback_reason;
    }
    return detail;
}

}  // namespace tenriff::app
