#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "app/menu/MenuAction.h"
#include "config/Config.h"

namespace tenriff::app::menu::settings {

enum class GraphicsSettingId : std::uint8_t {
    Display = 0,
    Resolution = 1,
    RefreshHz = 2,
    VSync = 3,
    PerformanceHud = 4,
    Bga = 5,
    BgaBehindNotes = 6,
    BgaUpscaler = 7,
    OnnxModel = 8,
    PreferLowPowerDirectX = 9,
    Language = 10,
    Back = 11,
};

inline constexpr std::array<GraphicsSettingId, 12> kGraphicsSettingOrder{
    GraphicsSettingId::Display,
    GraphicsSettingId::Resolution,
    GraphicsSettingId::RefreshHz,
    GraphicsSettingId::VSync,
    GraphicsSettingId::PerformanceHud,
    GraphicsSettingId::Bga,
    GraphicsSettingId::BgaBehindNotes,
    GraphicsSettingId::BgaUpscaler,
    GraphicsSettingId::OnnxModel,
    GraphicsSettingId::PreferLowPowerDirectX,
    GraphicsSettingId::Language,
    GraphicsSettingId::Back,
};

enum class OnnxUpscalerConfirmId : std::uint8_t {
    Enable = 0,
    KeepNative = 1,
};

inline constexpr std::array<OnnxUpscalerConfirmId, 2> kOnnxUpscalerConfirmOrder{
    OnnxUpscalerConfirmId::Enable,
    OnnxUpscalerConfirmId::KeepNative,
};

[[nodiscard]] std::optional<std::size_t> graphics_setting_index(
    GraphicsSettingId id) noexcept;
[[nodiscard]] std::optional<GraphicsSettingId> graphics_setting_id_at(
    std::size_t index) noexcept;
[[nodiscard]] std::optional<OnnxUpscalerConfirmId> onnx_upscaler_confirm_id_at(
    std::size_t index) noexcept;

struct GraphicsSettingsEffects {
    MenuEffectFlags menu{};
    bool apply_runtime_graphics = false;
    bool choose_onnx_model = false;
    bool show_onnx_confirmation = false;

    [[nodiscard]] bool empty() const noexcept;
    void merge(const GraphicsSettingsEffects& other) noexcept;
};

class GraphicsSettingsController {
public:
    [[nodiscard]] GraphicsSettingId selected_id() const noexcept;
    [[nodiscard]] OnnxUpscalerConfirmId selected_confirmation_id() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;

    void reset(GraphicsSettingId selected = GraphicsSettingId::Display) noexcept;
    [[nodiscard]] GraphicsSettingsEffects select(GraphicsSettingId target) noexcept;
    [[nodiscard]] GraphicsSettingsEffects handle(
        const MenuAction& action,
        config::RuntimeConfig& runtime,
        std::optional<GraphicsSettingId> target = std::nullopt);

    [[nodiscard]] GraphicsSettingsEffects set_onnx_model_path(
        config::RuntimeConfig& runtime,
        std::string model_path);
    void prepare_onnx_confirmation() noexcept;
    [[nodiscard]] GraphicsSettingsEffects select_confirmation(
        OnnxUpscalerConfirmId target) noexcept;
    [[nodiscard]] GraphicsSettingsEffects handle_confirmation(
        const MenuAction& action,
        config::RuntimeConfig& runtime,
        std::optional<OnnxUpscalerConfirmId> target = std::nullopt);

private:
    [[nodiscard]] GraphicsSettingsEffects move_selection(int direction) noexcept;
    [[nodiscard]] GraphicsSettingsEffects apply_selected_action(
        const MenuAction& action,
        config::RuntimeConfig& runtime);
    [[nodiscard]] GraphicsSettingsEffects leave_screen() noexcept;
    [[nodiscard]] GraphicsSettingsEffects mark_changed(
        bool apply_runtime_graphics = false) noexcept;

    GraphicsSettingId selected_id_ = GraphicsSettingId::Display;
    OnnxUpscalerConfirmId selected_confirmation_id_ =
        OnnxUpscalerConfirmId::KeepNative;
    bool dirty_ = false;
};

}  // namespace tenriff::app::menu::settings
