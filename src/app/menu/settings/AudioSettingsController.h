#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/MenuAction.h"
#include "app/menu/settings/SettingsRowModel.h"
#include "config/Config.h"

namespace tenriff::app::menu::settings {

// Explicit values are part of the serialized renderer-hit contract. Append new
// identifiers instead of reordering existing ones.
enum class AudioSettingId : std::uint8_t {
    Preset = 0,
    KeysoundMode = 1,
    BackgroundSound = 2,
    MasterVolume = 3,
    BgmVolume = 4,
    KeysoundVolume = 5,
    SoundOffset = 6,
    Back = 7,
    Normalize = 8,
    Backend = 9,
    AsioDriver = 10,
    SampleRate = 11,
    BufferFrames = 12,
};

inline constexpr std::array<AudioSettingId, 13> kAudioSettingOrder{
    AudioSettingId::Preset,
    AudioSettingId::KeysoundMode,
    AudioSettingId::BackgroundSound,
    AudioSettingId::MasterVolume,
    AudioSettingId::BgmVolume,
    AudioSettingId::KeysoundVolume,
    AudioSettingId::SoundOffset,
    AudioSettingId::Normalize,
    AudioSettingId::Backend,
    AudioSettingId::AsioDriver,
    AudioSettingId::SampleRate,
    AudioSettingId::BufferFrames,
    AudioSettingId::Back,
};

struct AudioDriverChoice {
    std::string id;
    std::string name;
};

[[nodiscard]] std::optional<std::size_t> audio_setting_index(AudioSettingId id) noexcept;
[[nodiscard]] std::optional<AudioSettingId> audio_setting_id_at(std::size_t index) noexcept;
[[nodiscard]] std::optional<NumericSettingRange> audio_setting_numeric_range(AudioSettingId id) noexcept;

class AudioSettingsController {
public:
    AudioSettingsController() = default;

    [[nodiscard]] AudioSettingId selected_id() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;
    void set_asio_drivers(std::vector<AudioDriverChoice> drivers);
    [[nodiscard]] bool asio_drivers_loaded() const noexcept { return asio_drivers_loaded_; }
    [[nodiscard]] const std::vector<AudioDriverChoice>& asio_drivers() const noexcept { return asio_drivers_; }

    // Starts a fresh visit to Audio Settings without mutating runtime config.
    void reset(AudioSettingId selected = AudioSettingId::Preset) noexcept;

    // Pointer selection is kept separate because it has no keyboard-equivalent
    // mutation. Pointer activation/adjustment then uses handle() with a target.
    [[nodiscard]] MenuEffectFlags select(AudioSettingId target) noexcept;

    // A target identifies the row hit by a pointer. Keyboard callers omit it and
    // act on the current selection.
    [[nodiscard]] MenuEffectFlags handle(
        const MenuAction& action,
        config::RuntimeConfig& runtime,
        std::optional<AudioSettingId> target = std::nullopt);

private:
    [[nodiscard]] MenuEffectFlags move_selection(int direction) noexcept;
    [[nodiscard]] MenuEffectFlags apply_selected_action(
        const MenuAction& action,
        config::RuntimeConfig& runtime);
    [[nodiscard]] MenuEffectFlags leave_screen() noexcept;
    [[nodiscard]] MenuEffectFlags mark_changed() noexcept;

    AudioSettingId selected_id_ = AudioSettingId::Preset;
    bool dirty_ = false;
    bool asio_drivers_loaded_ = false;
    std::vector<AudioDriverChoice> asio_drivers_;
};

}  // namespace tenriff::app::menu::settings
