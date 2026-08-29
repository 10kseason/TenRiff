#include "app/menu/MenuScreenDescriptor.h"

#include <array>
#include <cstddef>

namespace tenriff::app::menu {
namespace {

using Snapshot = SnapshotViewKind;
using Generic = GenericViewKind;

constexpr std::array<MenuScreenDescriptor, 20> kDescriptors{{
    {Screen::QuickSetup, "Quick Setup", "빠른 설정", "quick_setup", "", Snapshot::QuickSetup, Generic::None, false, true},
    {Screen::Title, "Title", "타이틀", "title", "", Snapshot::Title, Generic::None, false, true},
    {Screen::OptionsHub, "Options", "옵션", "options", "", Snapshot::Generic, Generic::OptionsHub, true, true},
    {Screen::Multiplayer, "Peer Multiplayer", "P2P 멀티플레이", "multiplayer", "song_select", Snapshot::Generic, Generic::Multiplayer, false, true},
    {Screen::SongSelect, "Song Select", "곡 선택", "song_select", "", Snapshot::SongSelect, Generic::None, false, true},
    {Screen::SessionMix, "Session Mix", "세션 믹스", "session_mix", "", Snapshot::Generic, Generic::SessionMix, false, true},
    {Screen::SongBrowser, "Song Filters", "곡 필터", "song_browser", "song_select", Snapshot::SongBrowser, Generic::None, false, true},
    {Screen::Gameplay, "Gameplay", "게임플레이", "", "", Snapshot::Gameplay, Generic::None, false, false},
    {Screen::SettingsAudio, "Audio Settings", "오디오 설정", "settings_audio", "settings", Snapshot::Generic, Generic::AudioSettings, true, true},
    {Screen::SettingsGraphics, "Graphics Settings", "그래픽 설정", "settings_graphics", "settings", Snapshot::Generic, Generic::GraphicsSettings, true, true},
    {Screen::SettingsSkins, "Skin Settings", "스킨 설정", "settings_skins", "settings", Snapshot::Generic, Generic::SkinSettings, true, true},
    {Screen::SettingsInput, "Input Settings", "입력 설정", "settings_input", "settings", Snapshot::Generic, Generic::InputSettings, true, true},
    {Screen::SettingsCalibration, "Calibration Wizard", "캘리브레이션 위저드", "settings_calibration", "settings", Snapshot::Generic, Generic::CalibrationSettings, true, true},
    {Screen::ModeSelect, "Mode Select", "모드 설정", "mode_select", "", Snapshot::Generic, Generic::ModeSettings, true, true},
    {Screen::ModeMods, "Mod Manager", "모드 관리자", "mode_mods", "", Snapshot::Generic, Generic::ModManager, true, true},
    {Screen::Keymap, "Keymap", "키 설정", "keymap", "", Snapshot::Generic, Generic::Keymap, true, false},
    {Screen::KeymapConfirm, "Keymap Confirm", "키 설정 확인", "keymap_confirm", "", Snapshot::Generic, Generic::KeymapConfirm, true, true},
    {Screen::OnnxUpscalerConfirm, "Enable ONNX Upscaler?", "ONNX 업스케일러를 켤까요?", "onnx_upscaler_confirm", "", Snapshot::Generic, Generic::OnnxUpscalerConfirm, true, true},
    {Screen::KeymapTest, "NKRO Test", "NKRO Test", "keymap_test", "", Snapshot::Generic, Generic::KeymapTest, true, false},
    {Screen::Result, "Result", "결과", "result", "", Snapshot::Result, Generic::None, false, true},
}};

static_assert(kDescriptors.size() == static_cast<std::size_t>(Screen::Result) + 1);

}  // namespace

const MenuScreenDescriptor& screen_descriptor(Screen screen) noexcept {
    const std::size_t index = static_cast<std::size_t>(screen);
    if (index >= kDescriptors.size() || kDescriptors[index].screen != screen) {
        return kDescriptors[static_cast<std::size_t>(Screen::Title)];
    }
    return kDescriptors[index];
}

}  // namespace tenriff::app::menu
