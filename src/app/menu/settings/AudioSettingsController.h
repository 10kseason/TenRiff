#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <utility>

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
    BgmVolume = 4,       // Reserved: retired separate balance control.
    KeysoundVolume = 5,  // Reserved: retired separate balance control.
    SoundOffset = 6,
    Back = 7,
    Normalize = 8,
    Backend = 9,
    AsioDriver = 10,
    SampleRate = 11,
    BufferFrames = 12,
    MuteWhenInactive = 13,
    TitleMusic = 14,
    PlayToEnd = 15,
    GameplayVolume = 16,
    MenuMusicVolume = 17,
    OutputDevice = 18,
    MainMusicFile = 19,
    ClearMusicFile = 20,
    FailMusicFile = 21,
    ButtonSoundFile = 22,
    SliderSoundFile = 23,
    DropSoundFile = 24,
    OptionsSoundFile = 25,
    StartSoundFile = 26,
    DoneSoundFile = 27,
};

inline constexpr std::array<AudioSettingId, 26> kAudioSettingOrder{
    AudioSettingId::MasterVolume,
    AudioSettingId::GameplayVolume,
    AudioSettingId::MenuMusicVolume,
    AudioSettingId::KeysoundMode,
    AudioSettingId::BackgroundSound,
    AudioSettingId::TitleMusic,
    AudioSettingId::MainMusicFile,
    AudioSettingId::ClearMusicFile,
    AudioSettingId::FailMusicFile,
    AudioSettingId::ButtonSoundFile,
    AudioSettingId::SliderSoundFile,
    AudioSettingId::DropSoundFile,
    AudioSettingId::OptionsSoundFile,
    AudioSettingId::StartSoundFile,
    AudioSettingId::DoneSoundFile,
    AudioSettingId::PlayToEnd,
    AudioSettingId::MuteWhenInactive,
    AudioSettingId::Normalize,
    AudioSettingId::SoundOffset,
    AudioSettingId::Preset,
    AudioSettingId::Backend,
    AudioSettingId::OutputDevice,
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
[[nodiscard]] bool is_audio_file_setting(AudioSettingId id) noexcept;
[[nodiscard]] std::string* audio_file_setting_value(config::RuntimeConfig& runtime, AudioSettingId id) noexcept;
[[nodiscard]] const std::string* audio_file_setting_value(const config::RuntimeConfig& runtime, AudioSettingId id) noexcept;

class AudioSettingsController {
public:
    AudioSettingsController() = default;

    [[nodiscard]] AudioSettingId selected_id() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;
    void set_asio_drivers(std::vector<AudioDriverChoice> drivers);
    void set_output_devices(std::vector<AudioDriverChoice> devices) { output_devices_ = std::move(devices); }
    [[nodiscard]] const std::vector<AudioDriverChoice>& output_devices() const noexcept { return output_devices_; }
    [[nodiscard]] bool asio_drivers_loaded() const noexcept { return asio_drivers_loaded_; }
    [[nodiscard]] const std::vector<AudioDriverChoice>& asio_drivers() const noexcept { return asio_drivers_; }

    // Starts a fresh visit to Audio Settings without mutating runtime config.
    void reset(AudioSettingId selected = AudioSettingId::MasterVolume) noexcept;

    // Pointer selection is kept separate because it has no keyboard-equivalent
    // mutation. Pointer activation/adjustment then uses handle() with a target.
    [[nodiscard]] MenuEffectFlags select(AudioSettingId target) noexcept;
    [[nodiscard]] std::optional<AudioSettingId> take_file_pick_request() noexcept;
    [[nodiscard]] MenuEffectFlags set_audio_file(config::RuntimeConfig& runtime, AudioSettingId id, std::string value);

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

    AudioSettingId selected_id_ = AudioSettingId::MasterVolume;
    std::optional<AudioSettingId> file_pick_request_;
    bool dirty_ = false;
    bool asio_drivers_loaded_ = false;
    std::vector<AudioDriverChoice> asio_drivers_;
    std::vector<AudioDriverChoice> output_devices_;
};

}  // namespace tenriff::app::menu::settings
