#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <utility>
#include "config/GraphicsResolution.h"

#include "app/menu/MenuAction.h"
#include "config/Config.h"

namespace tenriff::app::menu::settings {

enum class GraphicsSettingId : std::uint8_t {
    Display = 0,
    Resolution = 1,
    RefreshHz = 2,
    PerformanceHud = 3,
    Bga = 4,
    BgaBrightness = 5,
    BgaBehindNotes = 6,
    BgaUpscaler = 7,
    OnnxModel = 8,
    OnnxOutput = 9,
    PreferLowPowerDirectX = 10,
    // Pointer events use the row ID as the visible row index.
    Back = 11,
};

inline constexpr std::array<GraphicsSettingId, 12> kGraphicsSettingOrder{
    GraphicsSettingId::Display,
    GraphicsSettingId::Resolution,
    GraphicsSettingId::RefreshHz,
    GraphicsSettingId::PerformanceHud,
    GraphicsSettingId::Bga,
    GraphicsSettingId::BgaBrightness,
    GraphicsSettingId::BgaBehindNotes,
    GraphicsSettingId::BgaUpscaler,
    GraphicsSettingId::OnnxModel,
    GraphicsSettingId::OnnxOutput,
    GraphicsSettingId::PreferLowPowerDirectX,
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
    [[nodiscard]] bool editing_resolution() const noexcept { return resolution_editing_; }
    [[nodiscard]] int resolution_field() const noexcept { return resolution_field_; }
    [[nodiscard]] const std::array<std::string, 2>& resolution_fields() const noexcept { return resolution_fields_; }
    [[nodiscard]] bool resolution_error() const noexcept { return resolution_error_; }
    [[nodiscard]] GraphicsSettingsEffects resolution_text(std::string_view text);
    [[nodiscard]] GraphicsSettingsEffects resolution_erase(bool clear_all = false);
    void set_native_resolution(int width, int height) noexcept;

    void set_display_resolutions(const std::vector<std::pair<int, int>>& modes);
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
    bool resolution_editing_ = false;
    bool resolution_error_ = false;
    bool resolution_replace_ = true;
    int resolution_field_ = 0;
    int native_width_ = 1280, native_height_ = 720;
    std::array<std::string, 2> resolution_fields_{};
    std::vector<std::string> resolution_choices_ = config::graphics_resolution_choices();
};

}  // namespace tenriff::app::menu::settings
