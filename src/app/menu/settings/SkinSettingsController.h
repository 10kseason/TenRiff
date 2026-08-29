#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/MenuAppSkinUtils.h"
#include "app/menu/MenuAction.h"
#include "config/Config.h"

namespace tenriff::app::menu::settings {

enum class SkinBoundaryAction {
    None,
    ImportSkin,
    CreateSkin,
    OpenSkinFolder,
    ReloadSkin,
};

struct SkinSettingsEffects {
    MenuEffectFlags menu{};
    bool refresh_lr2_skins = false;
    bool refresh_tenriff_skins = false;
    bool increment_skin_revision = false;
    SkinBoundaryAction boundary_action = SkinBoundaryAction::None;

    [[nodiscard]] bool empty() const noexcept;
    void merge(const SkinSettingsEffects& other) noexcept;
};

[[nodiscard]] std::optional<SkinSettingsRowId> skin_setting_id_at(
    std::size_t index,
    bool lr2_source) noexcept;

class SkinSettingsController {
public:
    [[nodiscard]] SkinSettingsRowId selected_id() const noexcept;
    [[nodiscard]] std::string_view edit_mode() const noexcept;
    [[nodiscard]] int edit_lane() const noexcept;
    [[nodiscard]] int edit_gap() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;

    void reset(std::string_view runtime_key_mode);
    [[nodiscard]] SkinSettingsEffects select(
        SkinSettingsRowId target,
        bool lr2_source) noexcept;
    [[nodiscard]] SkinSettingsEffects handle(
        const MenuAction& action,
        config::RuntimeConfig& runtime,
        const std::vector<std::string>& available_lr2_skin_names,
        const std::vector<std::string>& available_tenriff_skin_names,
        std::optional<SkinSettingsRowId> target = std::nullopt);
    [[nodiscard]] SkinSettingsEffects request_reload() const noexcept;
    [[nodiscard]] SkinSettingsEffects mark_external_change() noexcept;

private:
    [[nodiscard]] SkinSettingsEffects move_selection(
        int direction,
        bool lr2_source) noexcept;
    [[nodiscard]] SkinSettingsEffects apply_selected_action(
        const MenuAction& action,
        config::RuntimeConfig& runtime,
        const std::vector<std::string>& available_lr2_skin_names,
        const std::vector<std::string>& available_tenriff_skin_names);
    [[nodiscard]] SkinSettingsEffects leave_screen() noexcept;
    [[nodiscard]] SkinSettingsEffects mark_changed() noexcept;
    void clamp_edit_targets() noexcept;

    SkinSettingsRowId selected_id_ = SkinSettingsRowId::KeyMode;
    std::string edit_mode_ = "10k";
    int edit_lane_ = 0;
    int edit_gap_ = 0;
    bool dirty_ = false;
};

}  // namespace tenriff::app::menu::settings
