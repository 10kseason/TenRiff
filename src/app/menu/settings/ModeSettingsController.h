#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "app/menu/MenuAction.h"
#include "config/Config.h"

namespace tenriff::app::menu::settings {

enum class ModeSettingId : std::uint8_t {
    Indexing = 0,
    IndexDifficulty = 1,
    GhostBattle = 2,
    Autoplay = 3,
    PracticeNoFail = 4,
    SuddenDeath = 5,
    Pacemaker = 6,
    PacemakerTarget = 7,
    KeyMode = 8,
    KeyConverter = 9,
    Nk2Preset = 10,
    Gauge = 11,
    Random = 12,
    RandomSeed = 13,
    Mods = 14,
    Rate = 15,
    HiSpeed = 16,
    Back = 17,
};

inline constexpr std::array<ModeSettingId, 18> kModeSettingOrder{
    ModeSettingId::Indexing,
    ModeSettingId::IndexDifficulty,
    ModeSettingId::GhostBattle,
    ModeSettingId::Autoplay,
    ModeSettingId::PracticeNoFail,
    ModeSettingId::SuddenDeath,
    ModeSettingId::Pacemaker,
    ModeSettingId::PacemakerTarget,
    ModeSettingId::KeyMode,
    ModeSettingId::KeyConverter,
    ModeSettingId::Nk2Preset,
    ModeSettingId::Gauge,
    ModeSettingId::Random,
    ModeSettingId::RandomSeed,
    ModeSettingId::Mods,
    ModeSettingId::Rate,
    ModeSettingId::HiSpeed,
    ModeSettingId::Back,
};

[[nodiscard]] std::optional<std::size_t> mode_setting_index(
    ModeSettingId id) noexcept;
[[nodiscard]] std::optional<ModeSettingId> mode_setting_id_at(
    std::size_t index) noexcept;

struct ModeSettingsEffects {
    MenuEffectFlags menu{};
    bool refresh_song_library = false;
    bool show_mod_manager = false;

    [[nodiscard]] bool empty() const noexcept;
    void merge(const ModeSettingsEffects& other) noexcept;
};

class ModeSettingsController {
public:
    [[nodiscard]] ModeSettingId selected_id() const noexcept;
    [[nodiscard]] std::size_t selected_mod_category() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;
    [[nodiscard]] bool library_dirty() const noexcept;

    void reset() noexcept;
    [[nodiscard]] ModeSettingsEffects select(ModeSettingId target) noexcept;
    [[nodiscard]] ModeSettingsEffects handle(
        const MenuAction& action,
        config::RuntimeConfig& runtime,
        std::optional<ModeSettingId> target = std::nullopt);
    [[nodiscard]] ModeSettingsEffects handle_mod_manager(
        const MenuAction& action,
        config::RuntimeConfig& runtime,
        std::optional<std::size_t> target_category = std::nullopt);

private:
    [[nodiscard]] ModeSettingsEffects move_selection(int direction) noexcept;
    [[nodiscard]] ModeSettingsEffects apply_selected_action(
        const MenuAction& action,
        config::RuntimeConfig& runtime);
    [[nodiscard]] ModeSettingsEffects leave_screen() noexcept;
    [[nodiscard]] ModeSettingsEffects mark_changed(bool library_changed = false) noexcept;

    ModeSettingId selected_id_ = ModeSettingId::Indexing;
    std::size_t selected_mod_category_ = 0;
    bool dirty_ = false;
    bool library_dirty_ = false;
};

}  // namespace tenriff::app::menu::settings
