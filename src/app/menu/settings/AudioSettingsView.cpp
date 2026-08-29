#include "app/menu/settings/AudioSettingsView.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace tenriff::app::menu::settings {
namespace {

std::string localized(bool use_korean, std::string_view english, std::string_view korean) {
    return std::string(use_korean ? korean : english);
}

std::string to_lower_ascii(std::string_view value) {
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('A') && ch <= static_cast<unsigned char>('Z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('A') + static_cast<unsigned char>('a'));
        }
        return static_cast<char>(ch);
    });
    return normalized;
}

std::string preset_label(std::string_view preset, bool use_korean) {
    return to_lower_ascii(preset) == "high"
        ? localized(use_korean, "High", "고성능")
        : localized(use_korean, "Basic", "기본");
}

std::string keysound_policy_label(std::string_view policy, bool use_korean) {
    const std::string normalized = to_lower_ascii(policy);
    if (normalized == "autoplay") {
        return localized(use_korean, "Autoplay", "자동재생");
    }
    if (normalized == "ignore" || normalized == "off") {
        return localized(use_korean, "Off", "끔");
    }
    return localized(use_korean, "Follow", "연동");
}

std::string on_off(bool enabled, bool use_korean) {
    return localized(
        use_korean,
        enabled ? "On" : "Off",
        enabled ? "켜짐" : "꺼짐");
}

std::string format_percent(double value) {
    const int percent = static_cast<int>(
        std::lround(std::clamp(value, 0.0, 2.0) * 100.0));
    return std::to_string(percent) + "%";
}

std::string format_signed_offset_ms(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(1);
    if (value >= 0.0) {
        stream << '+';
    }
    stream << value << " ms";
    return stream.str();
}

double normalized_value(double value, const NumericSettingRange& range) {
    if (!std::isfinite(value) || range.maximum <= range.minimum) {
        return 0.0;
    }
    return std::clamp(
        (value - range.minimum) / (range.maximum - range.minimum),
        0.0,
        1.0);
}

AudioSettingsRowModel make_row(
    AudioSettingId id,
    SettingsRowKind kind,
    std::string label,
    std::string value,
    const AudioSettingsController& controller,
    bool activatable,
    bool adjustable) {
    AudioSettingsRowModel row;
    row.id = id;
    row.kind = kind;
    row.label = std::move(label);
    row.value = std::move(value);
    row.selected = controller.selected_id() == id;
    row.activatable = activatable;
    row.adjustable = adjustable;
    row.numeric_range = audio_setting_numeric_range(id);
    return row;
}

AudioSettingsRowModel make_slider_row(
    AudioSettingId id,
    std::string label,
    double value,
    const AudioSettingsController& controller) {
    AudioSettingsRowModel row = make_row(
        id,
        SettingsRowKind::Slider,
        std::move(label),
        format_percent(value),
        controller,
        false,
        true);
    row.slider_ratio = normalized_value(value, *row.numeric_range);
    return row;
}

}  // namespace

AudioSettingsViewModel AudioSettingsView::build(
    const AudioSettingsController& controller,
    const config::RuntimeConfig& runtime,
    bool use_korean) {
    AudioSettingsViewModel view;
    view.rows.reserve(kAudioSettingOrder.size());
    view.notes.reserve(5);

    view.rows.push_back(make_row(
        AudioSettingId::Preset,
        SettingsRowKind::Choice,
        localized(use_korean, "Preset", "프리셋"),
        preset_label(runtime.audio_ui.preset, use_korean),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        AudioSettingId::KeysoundMode,
        SettingsRowKind::Choice,
        localized(use_korean, "Keysound Mode", "키음 모드"),
        keysound_policy_label(runtime.audio_ui.bms_keysound_policy, use_korean),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        AudioSettingId::BackgroundSound,
        SettingsRowKind::Toggle,
        localized(use_korean, "Background Sound", "배경음"),
        on_off(runtime.audio_ui.background_sound_enabled, use_korean),
        controller,
        false,
        true));
    view.rows.push_back(make_slider_row(
        AudioSettingId::MasterVolume,
        localized(use_korean, "Master Volume", "마스터 볼륨"),
        runtime.audio_ui.master_volume,
        controller));
    view.rows.push_back(make_slider_row(
        AudioSettingId::BgmVolume,
        localized(use_korean, "BGM Volume", "BGM 볼륨"),
        runtime.audio_ui.bgm_volume,
        controller));
    view.rows.push_back(make_slider_row(
        AudioSettingId::KeysoundVolume,
        localized(use_korean, "Keysound Volume", "키음 볼륨"),
        runtime.audio_ui.keysound_volume,
        controller));
    view.rows.push_back(make_row(
        AudioSettingId::SoundOffset,
        SettingsRowKind::Numeric,
        localized(use_korean, "Sound Offset", "사운드 오프셋"),
        format_signed_offset_ms(runtime.sound_offset_ms),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        AudioSettingId::Back,
        SettingsRowKind::Action,
        localized(use_korean, "Back", "뒤로"),
        "",
        controller,
        true,
        false));

    view.notes.push_back(localized(
        use_korean,
        "Follow: note hits trigger keysounds. Autoplay: note keysounds are mixed into background audio.",
        "연동: 노트를 칠 때 키음이 납니다. 자동재생: 노트 키음이 배경음에 섞여 재생됩니다."));
    view.notes.push_back(localized(
        use_korean,
        "Background Sound controls menu, result, and song-preview music only. Gameplay chart BGM keeps playing.",
        "배경음은 메뉴, 결과, 곡 미리듣기 음악만 제어합니다. 게임플레이 차트 BGM은 계속 재생됩니다."));
    view.notes.push_back(localized(
        use_korean,
        "Off: skip note keysounds. Autoplay mode routes note keysounds through BGM volume.",
        "끔: 노트 키음을 재생하지 않습니다. 자동재생에서는 노트 키음이 BGM 볼륨을 따릅니다."));
    view.notes.push_back(localized(
        use_korean,
        "Sound Offset shifts chart BGM and autoplay keysounds only. Positive delays sound; negative advances it.",
        "사운드 오프셋은 차트 BGM과 자동재생 키음만 이동합니다. 양수는 소리를 늦추고 음수는 앞당깁니다."));
    view.notes.push_back(localized(
        use_korean,
        "Use Left/Right or click a volume slider to change it. Back saves and returns.",
        "좌우 키나 볼륨 슬라이더를 클릭해 변경합니다. 뒤로 가면 저장 후 돌아갑니다."));

    return view;
}

}  // namespace tenriff::app::menu::settings
