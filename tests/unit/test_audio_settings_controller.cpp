#include "doctest/doctest.h"

#include <array>
#include <cstdint>
#include <limits>

#include "app/menu/MenuAction.h"
#include "app/menu/settings/AudioSettingsController.h"
#include "app/menu/settings/AudioSettingsView.h"

namespace {

using tenriff::app::menu::MenuAction;
using tenriff::app::menu::MenuEffectFlags;
using tenriff::app::menu::settings::AudioSettingId;
using tenriff::app::menu::settings::AudioSettingsController;
using tenriff::app::menu::settings::AudioSettingsView;
using tenriff::app::menu::settings::SettingsRowKind;

void check_render_only(const MenuEffectFlags& effects) {
    CHECK(effects.render_changed);
    CHECK_FALSE(effects.persist_config);
    CHECK_FALSE(effects.restart_audio);
    CHECK_FALSE(effects.navigate_back);
    CHECK_FALSE(effects.restart_input);
    CHECK_FALSE(effects.reinitialize_input_backend);
}

void check_no_effects(const MenuEffectFlags& effects) {
    CHECK(effects.empty());
    CHECK_FALSE(effects.render_changed);
    CHECK_FALSE(effects.persist_config);
    CHECK_FALSE(effects.restart_audio);
    CHECK_FALSE(effects.navigate_back);
    CHECK_FALSE(effects.restart_input);
    CHECK_FALSE(effects.reinitialize_input_backend);
}

}  // namespace

TEST_CASE("audio setting identifiers and ranges are stable") {
    using tenriff::app::menu::settings::audio_setting_id_at;
    using tenriff::app::menu::settings::audio_setting_index;
    using tenriff::app::menu::settings::audio_setting_numeric_range;
    using tenriff::app::menu::settings::kAudioSettingOrder;

    static_assert(static_cast<std::uint8_t>(AudioSettingId::Preset) == 0);
    static_assert(static_cast<std::uint8_t>(AudioSettingId::KeysoundMode) == 1);
    static_assert(static_cast<std::uint8_t>(AudioSettingId::BackgroundSound) == 2);
    static_assert(static_cast<std::uint8_t>(AudioSettingId::MasterVolume) == 3);
    static_assert(static_cast<std::uint8_t>(AudioSettingId::BgmVolume) == 4);
    static_assert(static_cast<std::uint8_t>(AudioSettingId::KeysoundVolume) == 5);
    static_assert(static_cast<std::uint8_t>(AudioSettingId::SoundOffset) == 6);
    static_assert(static_cast<std::uint8_t>(AudioSettingId::Back) == 7);

    for (std::size_t index = 0; index < kAudioSettingOrder.size(); ++index) {
        REQUIRE(audio_setting_index(kAudioSettingOrder[index]).has_value());
        CHECK(*audio_setting_index(kAudioSettingOrder[index]) == index);
        REQUIRE(audio_setting_id_at(index).has_value());
        CHECK(*audio_setting_id_at(index) == kAudioSettingOrder[index]);
    }
    CHECK_FALSE(audio_setting_id_at(kAudioSettingOrder.size()).has_value());
    CHECK_FALSE(audio_setting_index(static_cast<AudioSettingId>(255)).has_value());

    const auto master = audio_setting_numeric_range(AudioSettingId::MasterVolume);
    REQUIRE(master.has_value());
    CHECK(master->minimum == doctest::Approx(0.0));
    CHECK(master->maximum == doctest::Approx(1.0));
    CHECK(master->step == doctest::Approx(0.05));

    const auto bgm = audio_setting_numeric_range(AudioSettingId::BgmVolume);
    REQUIRE(bgm.has_value());
    CHECK(bgm->minimum == doctest::Approx(0.0));
    CHECK(bgm->maximum == doctest::Approx(2.0));
    CHECK(bgm->step == doctest::Approx(0.05));
    CHECK(audio_setting_numeric_range(AudioSettingId::KeysoundVolume) == bgm);

    const auto offset = audio_setting_numeric_range(AudioSettingId::SoundOffset);
    REQUIRE(offset.has_value());
    CHECK(offset->minimum == doctest::Approx(-500.0));
    CHECK(offset->maximum == doctest::Approx(500.0));
    CHECK(offset->step == doctest::Approx(1.0));
    CHECK_FALSE(audio_setting_numeric_range(AudioSettingId::Preset).has_value());
}

TEST_CASE("audio settings controller preserves every row behavior") {
    {
        tenriff::config::RuntimeConfig runtime;
        AudioSettingsController controller;
        const auto effects = controller.handle(MenuAction::adjust(-1), runtime);
        check_render_only(effects);
        CHECK(runtime.audio_ui.preset == "basic");
        CHECK(runtime.audio.frames_per_buffer == 256);
        CHECK(runtime.audio.periods == 3);

        static_cast<void>(controller.handle(MenuAction::adjust(1), runtime));
        CHECK(runtime.audio_ui.preset == "high");
        CHECK(runtime.audio.frames_per_buffer == 320);
        CHECK(runtime.audio.periods == 3);
    }

    {
        tenriff::config::RuntimeConfig runtime;
        AudioSettingsController controller;
        static_cast<void>(controller.select(AudioSettingId::KeysoundMode));
        check_render_only(controller.handle(MenuAction::adjust(1), runtime));
        CHECK(runtime.audio_ui.bms_keysound_policy == "autoplay");
        static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime));
        CHECK(runtime.audio_ui.bms_keysound_policy == "follow");
        static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime));
        CHECK(runtime.audio_ui.bms_keysound_policy == "ignore");
    }

    {
        tenriff::config::RuntimeConfig runtime;
        AudioSettingsController controller;
        static_cast<void>(controller.select(AudioSettingId::BackgroundSound));
        check_render_only(controller.handle(MenuAction::activate(), runtime));
        CHECK_FALSE(runtime.audio_ui.background_sound_enabled);
        static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime));
        CHECK(runtime.audio_ui.background_sound_enabled);
    }

    {
        tenriff::config::RuntimeConfig runtime;
        AudioSettingsController controller;
        static_cast<void>(controller.select(AudioSettingId::MasterVolume));
        check_render_only(controller.handle(MenuAction::adjust(-1), runtime));
        CHECK(runtime.audio_ui.master_volume == doctest::Approx(0.95));
    }

    {
        tenriff::config::RuntimeConfig runtime;
        AudioSettingsController controller;
        static_cast<void>(controller.select(AudioSettingId::BgmVolume));
        check_render_only(controller.handle(MenuAction::adjust(1), runtime));
        CHECK(runtime.audio_ui.bgm_volume == doctest::Approx(0.80));
    }

    {
        tenriff::config::RuntimeConfig runtime;
        AudioSettingsController controller;
        static_cast<void>(controller.select(AudioSettingId::KeysoundVolume));
        check_render_only(controller.handle(MenuAction::adjust(1), runtime));
        CHECK(runtime.audio_ui.keysound_volume == doctest::Approx(1.05));
    }

    {
        tenriff::config::RuntimeConfig runtime;
        AudioSettingsController controller;
        static_cast<void>(controller.select(AudioSettingId::SoundOffset));
        check_render_only(controller.handle(MenuAction::adjust(-1), runtime));
        CHECK(runtime.sound_offset_ms == doctest::Approx(-1.0));
    }

    {
        tenriff::config::RuntimeConfig runtime;
        AudioSettingsController controller;
        static_cast<void>(controller.select(AudioSettingId::Back));
        const auto effects = controller.handle(MenuAction::activate(), runtime);
        CHECK(effects.render_changed);
        CHECK(effects.navigate_back);
        CHECK_FALSE(effects.persist_config);
        CHECK_FALSE(effects.restart_audio);
        CHECK(controller.selected_id() == AudioSettingId::Preset);
    }
}

TEST_CASE("audio settings numeric actions clamp and snap") {
    tenriff::config::RuntimeConfig runtime;
    AudioSettingsController controller;

    static_cast<void>(controller.select(AudioSettingId::MasterVolume));
    runtime.audio_ui.master_volume = 1.0;
    check_no_effects(controller.handle(MenuAction::adjust(1), runtime));
    CHECK_FALSE(controller.dirty());

    check_render_only(controller.handle(MenuAction::set_ratio(0.52), runtime));
    CHECK(runtime.audio_ui.master_volume == doctest::Approx(0.50));

    check_render_only(controller.handle(
        MenuAction::set_ratio(0.52), runtime, AudioSettingId::BgmVolume));
    CHECK(runtime.audio_ui.bgm_volume == doctest::Approx(1.05));

    static_cast<void>(controller.handle(MenuAction::set_ratio(4.0), runtime));
    CHECK(runtime.audio_ui.bgm_volume == doctest::Approx(2.0));
    check_no_effects(controller.handle(MenuAction::adjust(1), runtime));

    static_cast<void>(controller.handle(MenuAction::set_ratio(-4.0), runtime));
    CHECK(runtime.audio_ui.bgm_volume == doctest::Approx(0.0));
    static_cast<void>(controller.handle(
        MenuAction::set_ratio(std::numeric_limits<double>::quiet_NaN()), runtime));
    CHECK(runtime.audio_ui.bgm_volume == doctest::Approx(0.0));

    static_cast<void>(controller.select(AudioSettingId::SoundOffset));
    runtime.sound_offset_ms = 500.0;
    check_no_effects(controller.handle(MenuAction::adjust(1), runtime));
    runtime.sound_offset_ms = -500.0;
    check_no_effects(controller.handle(MenuAction::adjust(-1), runtime));
}

TEST_CASE("audio settings keyboard and pointer volume changes have parity") {
    tenriff::config::RuntimeConfig keyboard_runtime;
    tenriff::config::RuntimeConfig pointer_runtime;
    AudioSettingsController keyboard;
    AudioSettingsController pointer;

    keyboard_runtime.audio_ui.master_volume = 0.0;
    pointer_runtime.audio_ui.master_volume = 0.0;
    static_cast<void>(keyboard.select(AudioSettingId::MasterVolume));
    for (int step = 0; step < 10; ++step) {
        static_cast<void>(keyboard.handle(MenuAction::adjust(1), keyboard_runtime));
    }
    static_cast<void>(pointer.handle(
        MenuAction::set_ratio(0.5), pointer_runtime, AudioSettingId::MasterVolume));
    CHECK(keyboard_runtime.audio_ui.master_volume ==
          doctest::Approx(pointer_runtime.audio_ui.master_volume));

    keyboard_runtime.audio_ui.bgm_volume = 0.0;
    pointer_runtime.audio_ui.bgm_volume = 0.0;
    static_cast<void>(keyboard.select(AudioSettingId::BgmVolume));
    for (int step = 0; step < 20; ++step) {
        static_cast<void>(keyboard.handle(MenuAction::adjust(1), keyboard_runtime));
    }
    static_cast<void>(pointer.handle(
        MenuAction::set_ratio(0.5), pointer_runtime, AudioSettingId::BgmVolume));
    CHECK(keyboard_runtime.audio_ui.bgm_volume ==
          doctest::Approx(pointer_runtime.audio_ui.bgm_volume));

    keyboard_runtime.audio_ui.keysound_volume = 0.0;
    pointer_runtime.audio_ui.keysound_volume = 0.0;
    static_cast<void>(keyboard.select(AudioSettingId::KeysoundVolume));
    for (int step = 0; step < 20; ++step) {
        static_cast<void>(keyboard.handle(MenuAction::adjust(1), keyboard_runtime));
    }
    static_cast<void>(pointer.handle(
        MenuAction::set_ratio(0.5), pointer_runtime, AudioSettingId::KeysoundVolume));
    CHECK(keyboard_runtime.audio_ui.keysound_volume ==
          doctest::Approx(pointer_runtime.audio_ui.keysound_volume));
}

TEST_CASE("audio settings no-op actions do not make the controller dirty") {
    tenriff::config::RuntimeConfig runtime;
    AudioSettingsController controller;

    check_no_effects(controller.select(AudioSettingId::Preset));
    check_no_effects(controller.handle(MenuAction::move(-1), runtime));
    check_no_effects(controller.handle(MenuAction::move(0), runtime));
    check_no_effects(controller.handle(MenuAction::adjust(0), runtime));
    check_no_effects(controller.handle(MenuAction::activate(), runtime));
    check_no_effects(controller.handle(MenuAction::set_ratio(0.5), runtime));
    check_no_effects(controller.handle(
        MenuAction::activate(), runtime, static_cast<AudioSettingId>(255)));
    CHECK_FALSE(controller.dirty());

    check_render_only(controller.handle(MenuAction::move(8), runtime));
    CHECK(controller.selected_id() == AudioSettingId::KeysoundMode);
    CHECK_FALSE(controller.dirty());
}

TEST_CASE("audio settings Back distinguishes clean and dirty visits") {
    tenriff::config::RuntimeConfig runtime;
    AudioSettingsController controller;

    const auto clean_back = controller.handle(MenuAction::back(), runtime);
    CHECK(clean_back.render_changed);
    CHECK(clean_back.navigate_back);
    CHECK_FALSE(clean_back.persist_config);
    CHECK_FALSE(clean_back.restart_audio);

    static_cast<void>(controller.handle(
        MenuAction::adjust(-1), runtime, AudioSettingId::MasterVolume));
    REQUIRE(controller.dirty());
    const auto dirty_back = controller.handle(MenuAction::back(), runtime);
    CHECK(dirty_back.render_changed);
    CHECK(dirty_back.navigate_back);
    CHECK(dirty_back.persist_config);
    CHECK(dirty_back.restart_audio);
    CHECK_FALSE(controller.dirty());
    CHECK(controller.selected_id() == AudioSettingId::Preset);

    const auto second_back = controller.handle(MenuAction::back(), runtime);
    CHECK_FALSE(second_back.persist_config);
    CHECK_FALSE(second_back.restart_audio);
}

TEST_CASE("audio settings view preserves rows values and localization") {
    tenriff::config::RuntimeConfig runtime;
    AudioSettingsController controller;
    static_cast<void>(controller.select(AudioSettingId::BgmVolume));

    const auto english = AudioSettingsView::build(controller, runtime, false);
    REQUIRE(english.rows.size() == 9);
    REQUIRE(english.notes.size() == 6);

    const std::array<const char*, 9> english_labels{
        "Preset",
        "Keysound Mode",
        "Background Sound",
        "Master Volume",
        "BGM Volume",
        "Keysound Volume",
        "Sound Offset",
        "Normalize Audio",
        "Back",
    };
    const std::array<const char*, 9> english_values{
        "High",
        "Follow",
        "On",
        "100%",
        "75%",
        "100%",
        "+0.0 ms",
        "Off",
        "",
    };
    const std::array<SettingsRowKind, 9> row_kinds{
        SettingsRowKind::Choice,
        SettingsRowKind::Choice,
        SettingsRowKind::Toggle,
        SettingsRowKind::Slider,
        SettingsRowKind::Slider,
        SettingsRowKind::Slider,
        SettingsRowKind::Numeric,
        SettingsRowKind::Toggle,
        SettingsRowKind::Action,
    };
    for (std::size_t index = 0; index < english.rows.size(); ++index) {
        CHECK(english.rows[index].id ==
              tenriff::app::menu::settings::kAudioSettingOrder[index]);
        CHECK(english.rows[index].label == english_labels[index]);
        CHECK(english.rows[index].value == english_values[index]);
        CHECK(english.rows[index].kind == row_kinds[index]);
        CHECK(english.rows[index].selected ==
              (english.rows[index].id == AudioSettingId::BgmVolume));
    }
    REQUIRE(english.rows[3].slider_ratio.has_value());
    REQUIRE(english.rows[4].slider_ratio.has_value());
    REQUIRE(english.rows[5].slider_ratio.has_value());
    CHECK(*english.rows[3].slider_ratio == doctest::Approx(1.0));
    CHECK(*english.rows[4].slider_ratio == doctest::Approx(0.375));
    CHECK(*english.rows[5].slider_ratio == doctest::Approx(0.5));
    CHECK(english.rows[6].numeric_range.has_value());
    CHECK(english.rows[8].activatable);
    CHECK_FALSE(english.rows[8].adjustable);
    CHECK(english.notes[1] ==
          "Follow: note hits trigger keysounds. Autoplay: note keysounds are mixed into background audio.");

    const auto korean = AudioSettingsView::build(controller, runtime, true);
    REQUIRE(korean.rows.size() == 9);
    CHECK(korean.rows[0].label == "프리셋");
    CHECK(korean.rows[0].value == "고성능");
    CHECK(korean.rows[1].label == "키음 모드");
    CHECK(korean.rows[1].value == "연동");
    CHECK(korean.rows[2].label == "배경음");
    CHECK(korean.rows[2].value == "켜짐");
    CHECK(korean.rows[3].label == "마스터 볼륨");
    CHECK(korean.rows[4].label == "BGM 볼륨");
    CHECK(korean.rows[5].label == "키음 볼륨");
    CHECK(korean.rows[6].label == "사운드 오프셋");
    CHECK(korean.rows[7].label == "오디오 노멀라이즈");
    CHECK(korean.notes.back() ==
          "좌우 키나 볼륨 슬라이더를 클릭해 변경합니다. 뒤로 가면 저장 후 돌아갑니다.");
}

TEST_CASE("audio normalization toggles through keyboard and pointer then persists on Back") {
    tenriff::config::RuntimeConfig runtime;
    AudioSettingsController controller;
    CHECK_FALSE(runtime.audio_ui.normalize_audio);
    (void)controller.handle(MenuAction::adjust(1), runtime, AudioSettingId::Normalize);
    CHECK(runtime.audio_ui.normalize_audio);
    const auto leave = controller.handle(MenuAction::activate(), runtime, AudioSettingId::Back);
    CHECK(leave.persist_config);
    (void)controller.handle(MenuAction::activate(), runtime, AudioSettingId::Normalize);
    CHECK_FALSE(runtime.audio_ui.normalize_audio);
}
