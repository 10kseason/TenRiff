#pragma once

#include <cstdint>
#include <string>

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

[[nodiscard]] inline std::string input_fallback_origin_label(InputFallbackOrigin origin, bool korean = false) {
    switch (origin) {
        case InputFallbackOrigin::Menu: return korean ? "메뉴" : "menu";
        case InputFallbackOrigin::Gameplay: return korean ? "게임플레이" : "gameplay";
        case InputFallbackOrigin::Replay: return korean ? "리플레이" : "replay";
        case InputFallbackOrigin::None:
        default: return korean ? "없음" : "none";
    }
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
