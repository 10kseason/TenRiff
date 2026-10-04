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

std::string localized(ui::Language language, std::string_view english, std::string_view korean) {
    return ui::text(language, english, korean);
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

std::string preset_label(std::string_view preset, ui::Language language) {
    return to_lower_ascii(preset) == "high"
        ? localized(language, "High", "고성능")
        : localized(language, "Basic", "기본");
}

std::string keysound_policy_label(std::string_view policy, ui::Language language) {
    const std::string normalized = to_lower_ascii(policy);
    if (normalized == "autoplay") {
        return localized(language, "Autoplay", "자동재생");
    }
    if (normalized == "ignore" || normalized == "off") {
        return localized(language, "Off", "끔");
    }
    return localized(language, "Follow", "연동");
}

std::string on_off(bool enabled, ui::Language language) {
    return localized(
        language,
        enabled ? "On" : "Off",
        enabled ? "켜짐" : "꺼짐");
}

std::string title_music_label(std::string_view mode, ui::Language language) {
    if (mode == "none") return localized(language, "None", "없음");
    if (mode == "random_bms") return localized(language, "Random BMS Music", "랜덤 BMS 음악");
    if (mode == "last_played") return localized(language, "Last Played Music", "마지막 플레이한 음악");
    return localized(language, "Default Music", "기본 음악");
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
    ui::Language language) {
    AudioSettingsViewModel view;
    view.rows.reserve(kAudioSettingOrder.size());
    view.notes.reserve(8);
    const bool asio = runtime.audio.backend == audio::AudioBackend::ASIO;

    view.rows.push_back(make_row(
        AudioSettingId::Preset,
        SettingsRowKind::Choice,
        localized(language, "Preset", "프리셋"),
        preset_label(runtime.audio_ui.preset, language),
        controller,
        !asio,
        !asio));
    view.rows.push_back(make_row(
        AudioSettingId::KeysoundMode,
        SettingsRowKind::Choice,
        localized(language, "Keysound Mode", "키음 모드"),
        keysound_policy_label(runtime.audio_ui.bms_keysound_policy, language),
        controller,
        true,
        true));
    view.rows.push_back(make_row(
        AudioSettingId::BackgroundSound,
        SettingsRowKind::Toggle,
        localized(language, "Background Sound", "배경음"),
        on_off(runtime.audio_ui.background_sound_enabled, language),
        controller,
        true,
        true));
    view.rows.push_back(make_row(
        AudioSettingId::MuteWhenInactive, SettingsRowKind::Toggle,
        localized(language, "Mute When Inactive", "창 비활성화 시 음소거"),
        on_off(runtime.audio_ui.mute_when_inactive, language), controller, true, true));
    view.rows.push_back(make_row(
        AudioSettingId::PlayToEnd, SettingsRowKind::Choice,
        localized(language, "Song Ending", "곡 종료 방식"),
        runtime.audio_ui.play_to_end ? localized(language, "Listen to End", "끝까지 듣기")
                                    : localized(language, "Skip Outro", "후주 스킵"), controller, true, true));
    view.rows.push_back(make_row(
        AudioSettingId::TitleMusic, SettingsRowKind::Choice,
        localized(language, "Title Music", "타이틀 음악"),
        title_music_label(runtime.audio_ui.title_music, language), controller, true, true));
    view.rows.push_back(make_slider_row(
        AudioSettingId::MasterVolume,
        localized(language, "Master Volume", "마스터 볼륨"),
        runtime.audio_ui.master_volume,
        controller));
    view.rows.push_back(make_slider_row(
        AudioSettingId::BgmVolume,
        localized(language, "BGM Volume", "BGM 볼륨"),
        runtime.audio_ui.bgm_volume,
        controller));
    view.rows.push_back(make_slider_row(
        AudioSettingId::KeysoundVolume,
        localized(language, "Keysound Volume", "키음 볼륨"),
        runtime.audio_ui.keysound_volume,
        controller));
    view.rows.push_back(make_row(
        AudioSettingId::SoundOffset,
        SettingsRowKind::Numeric,
        localized(language, "Sound Offset", "사운드 오프셋"),
        format_signed_offset_ms(runtime.sound_offset_ms),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        AudioSettingId::Normalize, SettingsRowKind::Toggle,
        localized(language, "Normalize Audio", "오디오 노멀라이즈"),
        on_off(runtime.audio_ui.normalize_audio, language), controller, true, true));
    view.notes.push_back(localized(language,
        "Normalize Audio gently levels the gameplay mix. OFF preserves the original mix; master volume still applies.",
        "노멀라이즈는 인게임 음량을 완만하게 보정합니다. OFF는 원래 믹스를 유지하며 마스터 볼륨은 그대로 적용됩니다."));
    view.rows.push_back(make_row(
        AudioSettingId::Backend, SettingsRowKind::Choice,
        localized(language, "Audio Backend", "오디오 출력 방식"),
        asio ? "ASIO" : "WASAPI", controller, true, true));
    std::string driver_name;
    if (controller.asio_drivers().empty()) {
        driver_name = localized(language, "No 64-bit ASIO driver", "64비트 ASIO 드라이버 없음");
    } else if (runtime.audio.asio_driver.empty()) {
        driver_name = localized(language, "Automatic", "자동 선택");
    } else {
        driver_name = localized(language, "Saved driver unavailable", "저장한 드라이버 없음");
        for (const auto& driver : controller.asio_drivers()) {
            if (driver.id == runtime.audio.asio_driver) driver_name = driver.name;
        }
    }
    view.rows.push_back(make_row(
        AudioSettingId::AsioDriver, SettingsRowKind::Choice,
        localized(language, "ASIO Driver", "ASIO 드라이버"),
        std::move(driver_name), controller, asio && !controller.asio_drivers().empty(),
        asio && !controller.asio_drivers().empty()));
    view.rows.push_back(make_row(
        AudioSettingId::SampleRate, SettingsRowKind::Choice,
        localized(language, "ASIO Sample Rate", "ASIO 샘플레이트"),
        std::to_string(runtime.audio.sample_rate) + " Hz", controller, asio, asio));
    view.rows.push_back(make_row(
        AudioSettingId::BufferFrames, SettingsRowKind::Choice,
        localized(language, "ASIO Buffer Size", "ASIO 버퍼 사이즈"),
        std::to_string(runtime.audio.frames_per_buffer) + localized(language, " samples", " 샘플"),
        controller, asio, asio));
    view.rows.push_back(make_row(
        AudioSettingId::Back,
        SettingsRowKind::Action,
        localized(language, "Back", "뒤로"),
        "",
        controller,
        true,
        false));

    view.notes.push_back(localized(
        language,
        "Follow: note hits trigger keysounds. Autoplay: note keysounds are mixed into background audio.",
        "연동: 노트를 칠 때 키음이 납니다. 자동재생: 노트 키음이 배경음에 섞여 재생됩니다."));
    view.notes.push_back(localized(
        language,
        "Background Sound controls menu, result, and song-preview music only. Gameplay chart BGM keeps playing.",
        "배경음은 메뉴, 결과, 곡 미리듣기 음악만 제어합니다. 게임플레이 차트 BGM은 계속 재생됩니다."));
    view.notes.push_back(localized(
        language,
        "Off: skip note keysounds. Autoplay mode routes note keysounds through BGM volume.",
        "끔: 노트 키음을 재생하지 않습니다. 자동재생에서는 노트 키음이 BGM 볼륨을 따릅니다."));
    view.notes.push_back(localized(
        language,
        "Sound Offset shifts chart BGM and autoplay keysounds only. Positive delays sound; negative advances it.",
        "사운드 오프셋은 차트 BGM과 자동재생 키음만 이동합니다. 양수는 소리를 늦추고 음수는 앞당깁니다."));
    view.notes.push_back(localized(
        language,
        "ASIO uses the selected installed 64-bit driver and its first stereo output pair. The driver negotiates the requested buffer. F5 refreshes the driver list; WASAPI presets do not apply to ASIO.",
        "ASIO는 선택한 설치 드라이버의 첫 스테레오 출력을 사용합니다. 버퍼는 드라이버의 지원값으로 맞춥니다. F5로 드라이버 목록을 새로고침하며 WASAPI 프리셋은 ASIO에 적용되지 않습니다."));
    view.notes.push_back(localized(
        language,
        "Mute When Inactive silences all game audio while another window is active. Saved volume and song timing stay unchanged.",
        "창 비활성화 시 음소거는 다른 창을 사용할 때 게임 소리를 끕니다. 저장한 음량과 곡 타이밍은 유지됩니다."));
    view.notes.push_back(localized(
        language,
        "Use Left/Right or click a volume slider to change it. Back saves and returns.",
        "좌우 키나 볼륨 슬라이더를 클릭해 변경합니다. 뒤로 가면 저장 후 돌아갑니다."));
    view.notes.push_back(localized(language,
        "Title Music: Random BMS picks one library chart per title visit. Last Played remembers the last game across launches. Unavailable charts use default music; chart music loops up to five minutes including keysounds.",
        "타이틀 음악: 랜덤 BMS는 타이틀에 올 때 라이브러리에서 한 곡을 고릅니다. 마지막 플레이 곡은 재실행 후에도 유지됩니다. 곡을 읽지 못하면 기본 음악을 재생하며, 차트 음악은 키음 포함 최대 5분을 반복합니다."));

    std::sort(view.rows.begin(), view.rows.end(), [](const auto& left, const auto& right) {
        return audio_setting_index(left.id).value_or(kAudioSettingOrder.size()) <
               audio_setting_index(right.id).value_or(kAudioSettingOrder.size());
    });
    for (auto& row : view.rows) {
        switch (row.id) {
            case AudioSettingId::KeysoundMode:
            case AudioSettingId::BackgroundSound:
            case AudioSettingId::TitleMusic:
            case AudioSettingId::PlayToEnd:
            case AudioSettingId::MuteWhenInactive:
                row.category = localized(language, "Playback", "재생");
                break;
            case AudioSettingId::MasterVolume:
            case AudioSettingId::BgmVolume:
            case AudioSettingId::KeysoundVolume:
            case AudioSettingId::Normalize:
                row.category = localized(language, "Volume", "음량");
                break;
            case AudioSettingId::SoundOffset:
                row.category = localized(language, "Timing", "타이밍");
                break;
            case AudioSettingId::Preset:
            case AudioSettingId::Backend:
            case AudioSettingId::AsioDriver:
            case AudioSettingId::SampleRate:
            case AudioSettingId::BufferFrames:
                row.category = localized(language, "Output Device", "출력 장치");
                break;
            case AudioSettingId::Back: break;
        }
    }
    return view;
}

}  // namespace tenriff::app::menu::settings
