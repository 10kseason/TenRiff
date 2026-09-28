#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "input/InputThread.h"
#include "ui/Localization.h"

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

// Keeps a confirmed RawInput failure sticky for the current process without
// rewriting the user's profile. This prevents menu/gameplay transitions from
// repeatedly selecting a backend that already proved unhealthy.
class InputBackendFallbackPolicy {
public:
    void latch_polling(InputFallbackOrigin origin, std::string reason, std::string timestamp_utc) {
        polling_latched_ = true;
        origin_ = origin;
        reason_ = std::move(reason);
        timestamp_utc_ = std::move(timestamp_utc);
    }

    void clear() {
        polling_latched_ = false;
        origin_ = InputFallbackOrigin::None;
        reason_.clear();
        timestamp_utc_.clear();
    }

    [[nodiscard]] bool polling_latched() const { return polling_latched_; }

    [[nodiscard]] bool rawinput_enabled(bool configured_rawinput) const {
        return configured_rawinput && !polling_latched_;
    }

    [[nodiscard]] InputBackendRuntimeState runtime_state(bool configured_rawinput) const {
        InputBackendRuntimeState state;
        state.configured_backend = configured_rawinput ? input::InputBackend::RawInput
                                                       : input::InputBackend::Polling;
        state.effective_backend = rawinput_enabled(configured_rawinput) ? input::InputBackend::RawInput
                                                                        : input::InputBackend::Polling;
        if (configured_rawinput && polling_latched_) {
            state.auto_fallback = true;
            state.fallback_origin = origin_;
            state.fallback_reason = reason_;
            state.fallback_timestamp_utc = timestamp_utc_;
        }
        return state;
    }

private:
    bool polling_latched_ = false;
    InputFallbackOrigin origin_ = InputFallbackOrigin::None;
    std::string reason_;
    std::string timestamp_utc_;
};

[[nodiscard]] inline std::string input_backend_name(input::InputBackend backend) {
    return backend == input::InputBackend::RawInput ? "RawInput" : "Polling";
}

[[nodiscard]] constexpr input::InputGatePolicy gameplay_input_gate_policy() noexcept {
    return input::InputGatePolicy::ForegroundProcess;
}

[[nodiscard]] inline input::InputBackend input_backend_for_event(const input::InputEvent& event) {
    return event.device_id == input::kPollingAggregateDeviceId
               ? input::InputBackend::Polling
               : input::InputBackend::RawInput;
}

[[nodiscard]] inline std::string input_fallback_origin_label(InputFallbackOrigin origin, ui::Language language = ui::Language::English) {
    switch (origin) {
        case InputFallbackOrigin::Menu: return ui::text(language, "menu", "메뉴");
        case InputFallbackOrigin::Gameplay: return ui::text(language, "gameplay", "플레이");
        case InputFallbackOrigin::Replay: return ui::text(language, "replay", "리플레이");
        case InputFallbackOrigin::None:
        default: return ui::text(language, "none", "없음");
    }
}

inline void sync_runtime_input_backend_state(InputBackendRuntimeState& state,
                                             const input::InputEvent& event,
                                             InputFallbackOrigin fallback_origin) {
    static_cast<void>(fallback_origin);

    // A confirmed fallback represents a backend lifecycle decision. A queued
    // event from the old RawInput registration must not silently undo it.
    if (state.auto_fallback) {
        return;
    }

    const input::InputBackend event_backend = input_backend_for_event(event);
    if (state.configured_backend == input::InputBackend::RawInput &&
        event_backend == input::InputBackend::Polling) {
        // RawInput can deliberately run polling as a shadow path. A shadow
        // event is not an effective-backend switch and must not make menu
        // diagnostics report Polling unless startup actually fell back.
        return;
    }

    state.effective_backend = event_backend;
    state.auto_fallback = false;
    state.fallback_origin = InputFallbackOrigin::None;
    state.fallback_reason.clear();
    state.fallback_timestamp_utc.clear();
}

[[nodiscard]] inline std::string format_input_backend_status_label(const InputBackendRuntimeState& state,
                                                                   ui::Language language = ui::Language::English) {
    std::string label = ui::text(language, "Input backend: ", "입력 백엔드: ");
    label += input_backend_name(state.effective_backend);
    if (state.auto_fallback) {
        label += ui::text(language, " (auto-fallback / ", " (자동 대체 / ");
        label += input_fallback_origin_label(state.fallback_origin, language);
        label += ")";
    }
    return label;
}

[[nodiscard]] inline std::string format_input_backend_status_detail(const InputBackendRuntimeState& state,
                                                                    ui::Language language = ui::Language::English) {
    if (!state.auto_fallback) {
        return {};
    }

    std::string detail = ui::text(language, "Auto-fallback: ", "자동 대체: ");
    detail += input_backend_name(state.configured_backend);
    detail += " -> ";
    detail += input_backend_name(state.effective_backend);
    detail += ui::text(language, " / origin ", " / 발생 위치 ");
    detail += input_fallback_origin_label(state.fallback_origin, language);
    if (!state.fallback_timestamp_utc.empty()) {
        detail += ui::text(language, " / at ", " / 시각 ");
        detail += state.fallback_timestamp_utc;
    }
    if (!state.fallback_reason.empty()) {
        detail += ui::text(language, " / reason ", " / 사유 ");
        detail += state.fallback_reason;
    }
    return detail;
}

}  // namespace tenriff::app
