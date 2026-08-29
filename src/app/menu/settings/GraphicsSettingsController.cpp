#include "app/menu/settings/GraphicsSettingsController.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <utility>

#include "app/GraphicsTiming.h"

namespace tenriff::app::menu::settings {
namespace {

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string normalize_display_mode(std::string value) {
    value = to_lower_ascii(std::move(value));
    if (value == "windowed" || value == "fullscreen") {
        return value;
    }
    return "borderless";
}

std::string normalize_resolution_preset(std::string value) {
    value = to_lower_ascii(std::move(value));
    if (value == "720p" || value == "1080p" || value == "qhd") {
        return value;
    }
    return "native";
}

template <std::size_t Size>
std::string cycle_token(
    const std::array<std::string_view, Size>& tokens,
    std::string current,
    int direction) {
    std::size_t index = 0;
    for (std::size_t candidate = 0; candidate < tokens.size(); ++candidate) {
        if (current == tokens[candidate]) {
            index = candidate;
            break;
        }
    }
    index = direction < 0
        ? (index == 0 ? tokens.size() - 1 : index - 1)
        : (index + 1) % tokens.size();
    return std::string(tokens[index]);
}

std::string cycle_display_mode(std::string current, int direction) {
    static constexpr std::array<std::string_view, 3> kModes{
        "borderless", "windowed", "fullscreen"};
    return cycle_token(kModes, normalize_display_mode(std::move(current)), direction);
}

std::string cycle_resolution_preset(std::string current, int direction) {
    static constexpr std::array<std::string_view, 4> kPresets{
        "native", "720p", "1080p", "qhd"};
    return cycle_token(kPresets, normalize_resolution_preset(std::move(current)), direction);
}

}  // namespace

std::optional<std::size_t> graphics_setting_index(GraphicsSettingId id) noexcept {
    for (std::size_t index = 0; index < kGraphicsSettingOrder.size(); ++index) {
        if (kGraphicsSettingOrder[index] == id) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<GraphicsSettingId> graphics_setting_id_at(std::size_t index) noexcept {
    if (index >= kGraphicsSettingOrder.size()) {
        return std::nullopt;
    }
    return kGraphicsSettingOrder[index];
}

std::optional<OnnxUpscalerConfirmId> onnx_upscaler_confirm_id_at(
    std::size_t index) noexcept {
    if (index >= kOnnxUpscalerConfirmOrder.size()) {
        return std::nullopt;
    }
    return kOnnxUpscalerConfirmOrder[index];
}

bool GraphicsSettingsEffects::empty() const noexcept {
    return menu.empty() && !apply_runtime_graphics && !choose_onnx_model &&
           !show_onnx_confirmation;
}

void GraphicsSettingsEffects::merge(const GraphicsSettingsEffects& other) noexcept {
    menu.merge(other.menu);
    apply_runtime_graphics = apply_runtime_graphics || other.apply_runtime_graphics;
    choose_onnx_model = choose_onnx_model || other.choose_onnx_model;
    show_onnx_confirmation =
        show_onnx_confirmation || other.show_onnx_confirmation;
}

GraphicsSettingId GraphicsSettingsController::selected_id() const noexcept {
    return selected_id_;
}

OnnxUpscalerConfirmId
GraphicsSettingsController::selected_confirmation_id() const noexcept {
    return selected_confirmation_id_;
}

bool GraphicsSettingsController::dirty() const noexcept {
    return dirty_;
}

void GraphicsSettingsController::reset(GraphicsSettingId selected) noexcept {
    selected_id_ = graphics_setting_index(selected).has_value()
        ? selected
        : GraphicsSettingId::Display;
    selected_confirmation_id_ = OnnxUpscalerConfirmId::KeepNative;
    dirty_ = false;
}

GraphicsSettingsEffects GraphicsSettingsController::select(
    GraphicsSettingId target) noexcept {
    if (!graphics_setting_index(target).has_value() || selected_id_ == target) {
        return {};
    }
    selected_id_ = target;
    GraphicsSettingsEffects effects;
    effects.menu.render_changed = true;
    return effects;
}

GraphicsSettingsEffects GraphicsSettingsController::handle(
    const MenuAction& action,
    config::RuntimeConfig& runtime,
    std::optional<GraphicsSettingId> target) {
    if (action.kind == MenuActionKind::Back) {
        return leave_screen();
    }
    if (action.kind == MenuActionKind::Move) {
        return move_selection(action.direction);
    }

    GraphicsSettingsEffects effects;
    if (target.has_value()) {
        if (!graphics_setting_index(*target).has_value()) {
            return effects;
        }
        effects.merge(select(*target));
    }
    effects.merge(apply_selected_action(action, runtime));
    return effects;
}

GraphicsSettingsEffects GraphicsSettingsController::set_onnx_model_path(
    config::RuntimeConfig& runtime,
    std::string model_path) {
    if (model_path.empty() || runtime.graphics.background_upscale_model_path == model_path) {
        return {};
    }
    runtime.graphics.background_upscale_model_path = std::move(model_path);
    return mark_changed();
}

void GraphicsSettingsController::prepare_onnx_confirmation() noexcept {
    selected_confirmation_id_ = OnnxUpscalerConfirmId::KeepNative;
}

GraphicsSettingsEffects GraphicsSettingsController::select_confirmation(
    OnnxUpscalerConfirmId target) noexcept {
    if (target != OnnxUpscalerConfirmId::Enable &&
        target != OnnxUpscalerConfirmId::KeepNative) {
        return {};
    }
    if (selected_confirmation_id_ == target) {
        return {};
    }
    selected_confirmation_id_ = target;
    GraphicsSettingsEffects effects;
    effects.menu.render_changed = true;
    return effects;
}

GraphicsSettingsEffects GraphicsSettingsController::handle_confirmation(
    const MenuAction& action,
    config::RuntimeConfig& runtime,
    std::optional<OnnxUpscalerConfirmId> target) {
    if (action.kind == MenuActionKind::Back) {
        selected_id_ = GraphicsSettingId::BgaUpscaler;
        selected_confirmation_id_ = OnnxUpscalerConfirmId::KeepNative;
        GraphicsSettingsEffects effects;
        effects.menu.render_changed = true;
        effects.menu.navigate_back = true;
        return effects;
    }

    GraphicsSettingsEffects effects;
    if (target.has_value()) {
        effects.merge(select_confirmation(*target));
    }
    if (action.kind == MenuActionKind::Move && action.direction != 0) {
        const auto next = selected_confirmation_id_ == OnnxUpscalerConfirmId::Enable
            ? OnnxUpscalerConfirmId::KeepNative
            : OnnxUpscalerConfirmId::Enable;
        effects.merge(select_confirmation(next));
        return effects;
    }
    if (action.kind != MenuActionKind::Activate) {
        return effects;
    }

    if (selected_confirmation_id_ == OnnxUpscalerConfirmId::Enable &&
        config::normalize_background_upscale_mode(
            runtime.graphics.background_upscale_mode) != "onnx") {
        runtime.graphics.background_upscale_mode = "onnx";
        effects.merge(mark_changed());
    }
    selected_id_ = GraphicsSettingId::BgaUpscaler;
    selected_confirmation_id_ = OnnxUpscalerConfirmId::KeepNative;
    effects.menu.render_changed = true;
    effects.menu.navigate_back = true;
    return effects;
}

GraphicsSettingsEffects GraphicsSettingsController::move_selection(
    int direction) noexcept {
    if (direction == 0) {
        return {};
    }
    const auto current = graphics_setting_index(selected_id_);
    if (!current.has_value()) {
        selected_id_ = GraphicsSettingId::Display;
        GraphicsSettingsEffects effects;
        effects.menu.render_changed = true;
        return effects;
    }
    const std::size_t next = direction < 0
        ? (*current == 0 ? 0 : *current - 1)
        : std::min(*current + 1, kGraphicsSettingOrder.size() - 1);
    if (next == *current) {
        return {};
    }
    selected_id_ = kGraphicsSettingOrder[next];
    GraphicsSettingsEffects effects;
    effects.menu.render_changed = true;
    return effects;
}

GraphicsSettingsEffects GraphicsSettingsController::apply_selected_action(
    const MenuAction& action,
    config::RuntimeConfig& runtime) {
    const bool is_adjust = action.kind == MenuActionKind::Adjust && action.direction != 0;
    const bool is_activate = action.kind == MenuActionKind::Activate;
    if (selected_id_ == GraphicsSettingId::Back && is_activate) {
        return leave_screen();
    }
    if (!is_adjust && !is_activate) {
        return {};
    }
    const int direction = is_adjust ? action.direction : 1;

    switch (selected_id_) {
        case GraphicsSettingId::Display:
            runtime.graphics.display_mode =
                cycle_display_mode(runtime.graphics.display_mode, direction);
            return mark_changed(true);
        case GraphicsSettingId::Resolution:
            runtime.graphics.resolution =
                cycle_resolution_preset(runtime.graphics.resolution, direction);
            return mark_changed(true);
        case GraphicsSettingId::RefreshHz:
            runtime.graphics.refresh_hz =
                cycle_graphics_refresh_hz(runtime.graphics.refresh_hz, direction);
            return mark_changed(true);
        case GraphicsSettingId::VSync:
            runtime.graphics.vsync = !runtime.graphics.vsync;
            return mark_changed(true);
        case GraphicsSettingId::PerformanceHud:
            runtime.graphics.performance_overlay = !runtime.graphics.performance_overlay;
            return mark_changed();
        case GraphicsSettingId::Bga:
            runtime.graphics.bga_enabled = !runtime.graphics.bga_enabled;
            return mark_changed();
        case GraphicsSettingId::BgaBehindNotes:
            runtime.skin.black_playfield_enabled = !runtime.skin.black_playfield_enabled;
            return mark_changed();
        case GraphicsSettingId::BgaUpscaler: {
            if (config::normalize_background_upscale_mode(
                    runtime.graphics.background_upscale_mode) == "onnx") {
                runtime.graphics.background_upscale_mode = "off";
                return mark_changed();
            }
            prepare_onnx_confirmation();
            GraphicsSettingsEffects effects;
            effects.choose_onnx_model =
                runtime.graphics.background_upscale_model_path.empty();
            effects.show_onnx_confirmation = true;
            return effects;
        }
        case GraphicsSettingId::OnnxModel: {
            GraphicsSettingsEffects effects;
            effects.choose_onnx_model = true;
            return effects;
        }
        case GraphicsSettingId::PreferLowPowerDirectX:
            runtime.graphics.background_upscale_prefer_npu =
                !runtime.graphics.background_upscale_prefer_npu;
            return mark_changed();
        case GraphicsSettingId::Language:
            runtime.ui.language =
                config::normalize_ui_language_token(runtime.ui.language) == "ko" ? "en" : "ko";
            return mark_changed();
        case GraphicsSettingId::Back:
            return {};
    }
    return {};
}

GraphicsSettingsEffects GraphicsSettingsController::leave_screen() noexcept {
    const bool was_dirty = dirty_;
    selected_id_ = GraphicsSettingId::Display;
    selected_confirmation_id_ = OnnxUpscalerConfirmId::KeepNative;
    dirty_ = false;
    GraphicsSettingsEffects effects;
    effects.menu.render_changed = true;
    effects.menu.persist_config = was_dirty;
    effects.menu.navigate_back = true;
    effects.apply_runtime_graphics = was_dirty;
    return effects;
}

GraphicsSettingsEffects GraphicsSettingsController::mark_changed(
    bool apply_runtime_graphics) noexcept {
    dirty_ = true;
    GraphicsSettingsEffects effects;
    effects.menu.render_changed = true;
    effects.apply_runtime_graphics = apply_runtime_graphics;
    return effects;
}

}  // namespace tenriff::app::menu::settings
