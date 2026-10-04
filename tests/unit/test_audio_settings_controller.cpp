#include "doctest/doctest.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <limits>
#include <memory>
#include <thread>

#ifdef _WIN32
#include "app/MenuApp.h"
#endif

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

#ifdef _WIN32
namespace tenriff::app {

struct MenuAppAudioSettingsTestAccess {
    static render::MenuRenderData build_render_rows() {
        auto menu = std::make_unique<MenuApp>();
        menu->config_.audio.backend = audio::AudioBackend::ASIO;
        // Populate uses the injected list instead of consulting machine drivers.
        menu->audio_settings_controller_.set_asio_drivers({{"fake-driver", "Test Device"}});
        render::MenuRenderData render;
        menu->populate_audio_settings_render_data(render);
        return render;
    }
};

struct MenuAppTitleMusicTestAccess {
    static void check_selection_lifecycle() {
        auto menu = std::make_unique<MenuApp>();
        menu->menu_navigator_.reset(MenuApp::Screen::Title);
        menu->config_.audio_ui.title_music = "random_bms";
        menu->update_title_music_target();
        CHECK(menu->title_music_chart_path_.empty());
        SongEntry osu;
        osu.format = "osu";
        osu.path = "ignored.osu";
        menu->indexed_songs_.push_back(osu);
        SongEntry bms;
        bms.format = "bms";
        bms.path = "title-test.bms";
        bms.audio_preview_path = "preview-test.wav";
        menu->indexed_songs_.push_back(bms);
        ++menu->song_index_revision_;
        menu->update_title_music_target();
        REQUIRE_FALSE(menu->title_music_chart_path_.empty());
        CHECK(menu->title_music_chart_path_.find("title-test.bms") != std::string::npos);
        CHECK(menu->title_music_preview_path_ == "preview-test.wav");
        const auto first_path = menu->title_music_chart_path_;
        const auto first_generation = menu->title_music_generation_;
        menu->indexed_songs_.clear();
        ++menu->song_index_revision_;
        menu->update_title_music_target();
        CHECK(menu->title_music_chart_path_ == first_path);
        CHECK(menu->title_music_generation_ == first_generation);

        menu->menu_navigator_.reset(MenuApp::Screen::OptionsHub);
        menu->update_title_music_target();
        CHECK_FALSE(menu->title_music_visit_active_);
        CHECK(menu->title_music_chart_path_.empty());
        menu->menu_navigator_.reset(MenuApp::Screen::Title);
        menu->indexed_songs_.push_back(bms);
        menu->update_title_music_target();
        CHECK(menu->title_music_generation_ > first_generation);

        menu->config_.audio_ui.title_music = "last_played";
        menu->config_.ui.last_played_chart_path = "saved-song.bms";
        menu->update_title_music_target();
        CHECK(menu->title_music_chart_path_ == "saved-song.bms");
        CHECK(menu->title_music_preview_path_.empty());
        menu->config_.ui.last_played_chart_path = "new-song.bms";
        menu->update_title_music_target();
        CHECK(menu->title_music_chart_path_ == "new-song.bms");
        for (const auto mode : {"none", "default"}) {
            menu->config_.audio_ui.title_music = mode;
            menu->update_title_music_target();
            CHECK(menu->title_music_chart_path_.empty());
        }
        menu->config_.audio_ui.title_music = "last_played";
        menu->menu_navigator_.reset(MenuApp::Screen::QuickSetup);
        menu->update_title_music_target();
        CHECK(menu->title_music_chart_path_ == "new-song.bms");
    }

    static void check_title_decode_sample_rate() {
        for (const auto backend : {audio::AudioBackend::ASIO, audio::AudioBackend::WASAPI}) {
            auto menu = std::make_unique<MenuApp>();
            menu->menu_navigator_.reset(MenuApp::Screen::Title);
            menu->config_.audio.backend = backend;
            menu->config_.audio.sample_rate = 48'000;
            menu->config_.audio_ui.background_sound_enabled = true;
            menu->config_.audio_ui.title_music = "last_played";
            menu->config_.ui.last_played_chart_path = "missing-title-rate-test.bms";
            menu->service_song_preview();
            menu->service_song_preview();
            auto& preview = menu->song_select_screen_;
            std::optional<SongSelectScreen::PreviewDecodeResult> decoded;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!decoded && std::chrono::steady_clock::now() < deadline) {
                decoded = preview.take_ready_preview_decode();
                if (!decoded) std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            REQUIRE(decoded.has_value());
            CHECK(decoded->sample_rate == (backend == audio::AudioBackend::ASIO ? 48'000 : 44'100));
            CHECK_FALSE(decoded->error.empty());
            CHECK_FALSE(menu->audio_thread_.is_running());
        }
    }

    static void check_cancelled_title_decode() {
        auto menu = std::make_unique<MenuApp>();
        menu->menu_navigator_.reset(MenuApp::Screen::Title);
        menu->config_.audio_ui.title_music = "none";
        auto& preview = menu->song_select_screen_;
        preview.set_active(true);
        preview.set_preview_target("old-title-visit", 0);
        preview.begin_preview_decode("old-title-visit", "missing-title-chart.bms", {}, 44'100, true);
        menu->service_song_preview();
        CHECK_FALSE(preview.active());
        CHECK_FALSE(preview.preview_pending());
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (preview.preview_decode_in_flight() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            menu->service_song_preview();
        }
        CHECK_FALSE(preview.preview_decode_in_flight());
        CHECK(preview.preview_active_path().empty());
        CHECK(preview.preview_selection_key().empty());
        CHECK_FALSE(menu->audio_thread_.is_running());
    }
};

struct MenuAppRepeatTestAccess {
    static std::unique_ptr<MenuApp> fixture() {
        auto menu = std::make_unique<MenuApp>();
        menu->key_left_ = 37;
        menu->key_up_ = 38;
        menu->key_right_ = 39;
        menu->key_down_ = 40;
        menu->key_enter_ = 13;
        menu->key_escape_ = 27;
        menu->config_.audio_ui.background_sound_enabled = false;
        menu->config_.audio_ui.master_volume = 0.20;
        menu->config_.audio.backend = audio::AudioBackend::ASIO;
        menu->audio_settings_controller_.set_asio_drivers({{"mock", "Mock driver"}});
        menu->reset_screen(MenuApp::Screen::SettingsAudio);
        select_audio(*menu, menu::settings::AudioSettingId::MasterVolume);
        return menu;
    }

    static void select_audio(MenuApp& menu, menu::settings::AudioSettingId id) {
        static_cast<void>(menu.audio_settings_controller_.select(id));
        menu.snapshot_.screen = menu.current_screen();
        menu.snapshot_.render = {};
        menu.populate_audio_settings_render_data(menu.snapshot_.render);
    }

    static void press(MenuApp& menu, uint32_t key) {
        input::InputEvent event{};
        event.keycode = key;
        event.state = input::InputState::Pressed;
        menu.handle_input_event(event);
    }

    static void release(MenuApp& menu, uint32_t key) {
        input::InputEvent event{};
        event.keycode = key;
        event.state = input::InputState::Released;
        menu.handle_input_event(event);
    }

    static void check_rendered_row_policy() {
        auto menu = fixture();
        for (const auto id : menu::settings::kAudioSettingOrder) {
            select_audio(*menu, id);
            // This is a stable ID, not the reordered visible-row position.
            menu->settings_cursor_ = 999;
            const auto& rows = menu->snapshot_.render.generic.rows;
            const auto selected = std::find_if(rows.begin(), rows.end(), [](const auto& row) { return row.selected; });
            REQUIRE(selected != rows.end());
            CHECK(menu->is_song_select_repeat_key(menu->key_left_) == selected->adjustable);
            CHECK(menu->is_song_select_repeat_key(menu->key_right_) == selected->adjustable);
            if (selected->adjustable) {
                CHECK(menu->selected_adjustment_repeat_row(menu->key_left_) == static_cast<int>(id));
            }
        }
        select_audio(*menu, menu::settings::AudioSettingId::MasterVolume);
        for (auto& row : menu->snapshot_.render.generic.rows) {
            if (row.selected) row.decrement_enabled = false;
        }
        CHECK_FALSE(menu->is_song_select_repeat_key(menu->key_left_));
        CHECK(menu->is_song_select_repeat_key(menu->key_right_));
        menu->snapshot_.screen = MenuApp::Screen::SettingsSkins;
        CHECK_FALSE(menu->is_song_select_repeat_key(menu->key_right_));
    }

    static void check_hold_and_release() {
        auto menu = fixture();
        press(*menu, menu->key_right_);
        const double first = menu->config_.audio_ui.master_volume;
        CHECK(first > 0.20);
        const auto due = menu->song_select_repeat_next_ns_;
        REQUIRE(due > 0);
        menu->service_song_select_repeat(due - 1, true);
        CHECK(menu->config_.audio_ui.master_volume == first);
        // Duplicate platform make events neither change the value nor restart
        // our initial delay; only the common repeat timer owns further steps.
        press(*menu, menu->key_right_);
        CHECK(menu->config_.audio_ui.master_volume == first);
        CHECK(menu->song_select_repeat_next_ns_ == due);
        menu->service_song_select_repeat(due, true);
        const double second = menu->config_.audio_ui.master_volume;
        CHECK(second > first);
        CHECK(menu->song_select_repeat_next_ns_ > due);
        release(*menu, menu->key_right_);
        menu->service_song_select_repeat(due + 1'000'000'000LL, true);
        CHECK(menu->config_.audio_ui.master_volume == second);
        CHECK(menu->song_select_repeat_key_ == 0);

        select_audio(*menu, menu::settings::AudioSettingId::TitleMusic);
        menu->config_.audio_ui.title_music = "default";
        press(*menu, menu->key_right_);
        CHECK(menu->config_.audio_ui.title_music == "random_bms");
        menu->service_song_select_repeat(menu->song_select_repeat_next_ns_, true);
        CHECK(menu->config_.audio_ui.title_music == "last_played");
        press(*menu, menu->key_left_);
        CHECK(menu->config_.audio_ui.title_music == "random_bms");
        menu->service_song_select_repeat(menu->song_select_repeat_next_ns_, true);
        CHECK(menu->config_.audio_ui.title_music == "default");
        release(*menu, menu->key_left_);
    }

    static void check_cancellation_boundaries() {
        auto menu = fixture();
        press(*menu, menu->key_right_);
        const auto due = menu->song_select_repeat_next_ns_;
        select_audio(*menu, menu::settings::AudioSettingId::TitleMusic);
        menu->service_song_select_repeat(due, true);
        CHECK(menu->config_.audio_ui.title_music == "default");
        CHECK(menu->song_select_repeat_key_ == 0);
        release(*menu, menu->key_right_);
        select_audio(*menu, menu::settings::AudioSettingId::MasterVolume);
        press(*menu, menu->key_right_);
        const double value = menu->config_.audio_ui.master_volume;
        menu->service_song_select_repeat(menu->song_select_repeat_next_ns_, false);
        menu->service_song_select_repeat(due + 1'000'000'000LL, true);
        CHECK(menu->config_.audio_ui.master_volume == value);
        CHECK(menu->song_select_repeat_key_ == 0);
        release(*menu, menu->key_right_);
        press(*menu, menu->key_right_);
        menu->push_screen(MenuApp::Screen::SettingsSkins);
        CHECK(menu->song_select_repeat_key_ == 0);

        const std::array<bool MenuApp::*, 8> editing_flags{
            &MenuApp::help_overlay_visible_, &MenuApp::profile_nickname_edit_active_,
            &MenuApp::difficulty_table_url_editing_, &MenuApp::online_records_url_editing_,
            &MenuApp::song_select_search_active_, &MenuApp::chat_overlay_visible_,
            &MenuApp::ranked_account_overlay_visible_, &MenuApp::chat_url_warning_visible_};
        menu->reset_screen(MenuApp::Screen::SettingsAudio);
        select_audio(*menu, menu::settings::AudioSettingId::MasterVolume);
        for (const auto flag : editing_flags) {
            menu.get()->*flag = true;
            CHECK_FALSE(menu->is_song_select_repeat_key(menu->key_right_));
            menu.get()->*flag = false;
        }
        menu->reset_screen(MenuApp::Screen::Gameplay);
        CHECK_FALSE(menu->is_song_select_repeat_key(menu->key_right_));
        menu->reset_screen(MenuApp::Screen::Keymap);
        menu->keymap_settings_controller_.reset(4, "4k");
        static_cast<void>(menu->keymap_settings_controller_.select(1));
        static_cast<void>(menu->keymap_settings_controller_.handle(menu::MenuAction::activate(), 1));
        CHECK(menu->keymap_settings_controller_.capture_active());
        CHECK_FALSE(menu->is_song_select_repeat_key(menu->key_right_));
    }
};

}  // namespace tenriff::app

TEST_CASE("title music selects once per visit handles late library and leaves default modes silent of chart previews") {
    tenriff::app::MenuAppTitleMusicTestAccess::check_selection_lifecycle();
}

TEST_CASE("switching title music to none cancels and drains a stale asynchronous chart decode") {
    tenriff::app::MenuAppTitleMusicTestAccess::check_cancelled_title_decode();
}

TEST_CASE("title music preserves the selected ASIO hardware rate while using a compact shared WASAPI mix") {
    tenriff::app::MenuAppTitleMusicTestAccess::check_title_decode_sample_rate();
}

TEST_CASE("held setting arrows follow rendered stable row IDs and enabled adjustment directions") {
    tenriff::app::MenuAppRepeatTestAccess::check_rendered_row_policy();
}

TEST_CASE("held setting arrows apply one delayed repeat cadence to numeric and choice settings") {
    tenriff::app::MenuAppRepeatTestAccess::check_hold_and_release();
}

TEST_CASE("held setting arrows stop on selection screen focus release and text capture boundaries") {
    tenriff::app::MenuAppRepeatTestAccess::check_cancellation_boundaries();
}

TEST_CASE("rendered audio row hit indices decode to the same settings as keyboard navigation") {
    const auto render = tenriff::app::MenuAppAudioSettingsTestAccess::build_render_rows();
    using namespace tenriff::app::menu::settings;
    REQUIRE(render.generic.rows.size() == kAudioSettingOrder.size());
    for (std::size_t index = 0; index < render.generic.rows.size(); ++index) {
        const auto& row = render.generic.rows[index];
        REQUIRE(row.row_index >= 0);
        const auto click_target = settings_id_from_hit<AudioSettingId>(row.row_index, audio_setting_index);
        REQUIRE(click_target.has_value());
        CHECK(*click_target == kAudioSettingOrder[index]);
    }
}
#endif

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
    static_assert(static_cast<std::uint8_t>(AudioSettingId::Normalize) == 8);
    static_assert(static_cast<std::uint8_t>(AudioSettingId::Backend) == 9);
    static_assert(static_cast<std::uint8_t>(AudioSettingId::AsioDriver) == 10);
    static_assert(static_cast<std::uint8_t>(AudioSettingId::SampleRate) == 11);
    static_assert(static_cast<std::uint8_t>(AudioSettingId::BufferFrames) == 12);

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
        CHECK(runtime.audio_ui.master_volume == doctest::Approx(0.65));
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
    controller.reset(AudioSettingId::KeysoundMode);

    check_no_effects(controller.select(AudioSettingId::KeysoundMode));
    check_no_effects(controller.handle(MenuAction::move(-1), runtime));
    check_no_effects(controller.handle(MenuAction::move(0), runtime));
    check_no_effects(controller.handle(MenuAction::adjust(0), runtime));
    check_no_effects(controller.handle(MenuAction::set_ratio(0.5), runtime));
    check_no_effects(controller.handle(
        MenuAction::activate(), runtime, static_cast<AudioSettingId>(255)));
    CHECK_FALSE(controller.dirty());

    check_render_only(controller.handle(MenuAction::move(8), runtime));
    CHECK(controller.selected_id() == AudioSettingId::BackgroundSound);
    CHECK_FALSE(controller.dirty());
    controller.reset(AudioSettingId::Preset);
    runtime.audio.backend = tenriff::audio::AudioBackend::ASIO;
    check_no_effects(controller.handle(MenuAction::activate(), runtime));
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

TEST_CASE("audio settings view groups related controls and preserves saved values and localization") {
    tenriff::config::RuntimeConfig runtime;
    runtime.audio_ui.master_volume = 1.0; // An existing saved 100% value stays 100%.
    AudioSettingsController controller;
    static_cast<void>(controller.select(AudioSettingId::BgmVolume));

    const auto english = AudioSettingsView::build(controller, runtime, tenriff::ui::Language::English);
    REQUIRE(english.rows.size() == 16);
    REQUIRE(english.notes.size() == 9);
    const std::array<const char*, 16> labels{
        "Keysound Mode", "Background Sound", "Title Music", "Song Ending", "Mute When Inactive", "Master Volume",
        "BGM Volume", "Keysound Volume", "Normalize Audio", "Sound Offset", "Preset",
        "Audio Backend", "ASIO Driver", "ASIO Sample Rate", "ASIO Buffer Size", "Back",
    };
    const std::array<const char*, 16> values{
        "Follow", "On", "Default Music", "Listen to End", "Off", "100%", "75%", "100%", "Off", "+0.0 ms", "High",
        "WASAPI", "No 64-bit ASIO driver", "44100 Hz", "128 samples", "",
    };
    const std::array<const char*, 16> categories{
        "Playback", "Playback", "Playback", "Playback", "Playback", "Volume", "Volume", "Volume", "Volume",
        "Timing", "Output Device", "Output Device", "Output Device", "Output Device", "Output Device", "",
    };
    for (std::size_t index = 0; index < english.rows.size(); ++index) {
        const auto& row = english.rows[index];
        CHECK(row.id == tenriff::app::menu::settings::kAudioSettingOrder[index]);
        CHECK(row.label == labels[index]);
        CHECK(row.value == values[index]);
        CHECK(row.category == categories[index]);
        CHECK(row.selected == (row.id == AudioSettingId::BgmVolume));
        if (row.id == AudioSettingId::MasterVolume || row.id == AudioSettingId::BgmVolume ||
            row.id == AudioSettingId::KeysoundVolume) {
            CHECK(row.kind == SettingsRowKind::Slider);
            CHECK(row.slider_ratio.has_value());
        }
    }
    CHECK(*english.rows[5].slider_ratio == doctest::Approx(1.0));
    CHECK(*english.rows[6].slider_ratio == doctest::Approx(0.375));
    CHECK(*english.rows[7].slider_ratio == doctest::Approx(0.5));
    CHECK(english.rows[9].numeric_range.has_value());
    CHECK(english.rows.back().activatable);
    CHECK_FALSE(english.rows.back().adjustable);
    CHECK(english.rows[11].activatable);
    CHECK(english.rows[11].adjustable);
    for (std::size_t index = 12; index <= 14; ++index) {
        CHECK_FALSE(english.rows[index].activatable);
        CHECK_FALSE(english.rows[index].adjustable);
    }
    const auto korean = AudioSettingsView::build(controller, runtime, tenriff::ui::Language::Korean);
    CHECK(korean.rows[0].label == "키음 모드");
    CHECK(korean.rows[2].label == "타이틀 음악");
    CHECK(korean.rows[4].label == "창 비활성화 시 음소거");
    CHECK(korean.rows[2].category == "재생");
    CHECK(korean.rows[5].category == "음량");
    CHECK(korean.rows[9].category == "타이밍");
    CHECK(korean.rows[10].category == "출력 장치");
    CHECK(korean.rows[14].label == "ASIO 버퍼 사이즈");
    CHECK(korean.rows[14].value == "128 샘플");
    const auto japanese = AudioSettingsView::build(controller, runtime, tenriff::ui::Language::Japanese);
    CHECK(japanese.rows[2].label == "タイトル音楽");
    CHECK(japanese.rows[2].value == "標準の音楽");
    CHECK(japanese.rows[14].label == "ASIO バッファサイズ");
    CHECK(japanese.rows[14].value == "128 サンプル");
}

TEST_CASE("title music choices support both pointer activation and reverse keyboard selection") {
    tenriff::config::RuntimeConfig runtime;
    AudioSettingsController controller;
    CHECK(runtime.audio_ui.title_music == "default");
    check_render_only(controller.handle(MenuAction::activate(), runtime, AudioSettingId::TitleMusic));
    CHECK(runtime.audio_ui.title_music == "random_bms");
    check_render_only(controller.handle(MenuAction::activate(), runtime));
    CHECK(runtime.audio_ui.title_music == "last_played");
    check_render_only(controller.handle(MenuAction::activate(), runtime));
    CHECK(runtime.audio_ui.title_music == "none");
    check_render_only(controller.handle(MenuAction::adjust(-1), runtime));
    CHECK(runtime.audio_ui.title_music == "last_played");
    const auto effects = controller.handle(MenuAction::back(), runtime);
    CHECK(effects.persist_config);
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

TEST_CASE("audio stable pointer IDs adjust the circled controls in their requested direction") {
    using namespace tenriff::app::menu::settings;
    for (int invalid : {-1, 256, 999})
        CHECK_FALSE(settings_id_from_hit<AudioSettingId>(invalid, audio_setting_index).has_value());
    for (const auto id : kAudioSettingOrder) {
        const auto target = settings_id_from_hit<AudioSettingId>(static_cast<int>(id), audio_setting_index);
        REQUIRE(target.has_value());
        CHECK(*target == id);
    }
    tenriff::config::RuntimeConfig runtime;
    AudioSettingsController controller;
    controller.set_asio_drivers({{"first", "Interface A"}, {"second", "Interface B"}});
    const auto minus = [&](AudioSettingId id) {
        const auto target = settings_id_from_hit<AudioSettingId>(static_cast<int>(id), audio_setting_index);
        REQUIRE(target.has_value());
        return controller.handle(MenuAction::adjust(-1), runtime, target);
    };
    runtime.audio_ui.normalize_audio = true;
    check_render_only(minus(AudioSettingId::Normalize));
    CHECK_FALSE(runtime.audio_ui.normalize_audio);
    check_render_only(minus(AudioSettingId::Backend));
    CHECK(runtime.audio.backend == tenriff::audio::AudioBackend::ASIO);
    runtime.audio.asio_driver = "second";
    check_render_only(minus(AudioSettingId::AsioDriver));
    CHECK(runtime.audio.asio_driver == "first");
    runtime.audio.sample_rate = 48000;
    check_render_only(minus(AudioSettingId::SampleRate));
    CHECK(runtime.audio.sample_rate == 44100);
    runtime.audio.frames_per_buffer = 512;
    check_render_only(minus(AudioSettingId::BufferFrames));
    CHECK(runtime.audio.frames_per_buffer == 256);
}

TEST_CASE("inactive audio mute is optional and mouse row activation matches keyboard toggles") {
    tenriff::config::RuntimeConfig runtime;
    AudioSettingsController controller;
    CHECK_FALSE(runtime.audio_ui.mute_when_inactive);
    check_render_only(controller.handle(MenuAction::activate(), runtime, AudioSettingId::MuteWhenInactive));
    CHECK(runtime.audio_ui.mute_when_inactive);
    check_render_only(controller.handle(MenuAction::adjust(-1), runtime, AudioSettingId::MuteWhenInactive));
    CHECK_FALSE(runtime.audio_ui.mute_when_inactive);
    check_render_only(controller.handle(MenuAction::activate(), runtime, AudioSettingId::KeysoundMode));
    CHECK(runtime.audio_ui.bms_keysound_policy == "autoplay");
    CHECK(controller.handle(MenuAction::back(), runtime).persist_config);
}

TEST_CASE("ASIO driver choices cycle through automatic and installed IDs and apply only on Back") {
    tenriff::config::RuntimeConfig runtime;
    runtime.audio.backend = tenriff::audio::AudioBackend::ASIO;
    AudioSettingsController controller;
    const std::string first = "{11111111-1111-1111-1111-111111111111}";
    const std::string second = "{22222222-2222-2222-2222-222222222222}";
    controller.set_asio_drivers({{first, "Interface A"}, {second, "Interface B"}});
    REQUIRE(controller.asio_drivers_loaded());
    static_cast<void>(controller.select(AudioSettingId::AsioDriver));
    auto view = AudioSettingsView::build(controller, runtime, tenriff::ui::Language::English);
    CHECK(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::AsioDriver)].value == "Automatic");
    CHECK(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::AsioDriver)].activatable);
    CHECK(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::AsioDriver)].adjustable);
    CHECK_FALSE(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::Preset)].adjustable);

    check_render_only(controller.handle(MenuAction::adjust(1), runtime));
    CHECK(runtime.audio.asio_driver == first);
    check_render_only(controller.handle(MenuAction::activate(), runtime));
    CHECK(runtime.audio.asio_driver == second);
    view = AudioSettingsView::build(controller, runtime, tenriff::ui::Language::English);
    CHECK(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::AsioDriver)].value == "Interface B");
    check_render_only(controller.handle(MenuAction::adjust(1), runtime));
    CHECK(runtime.audio.asio_driver.empty());
    check_render_only(controller.handle(MenuAction::adjust(-1), runtime));
    CHECK(runtime.audio.asio_driver == second);
    check_render_only(controller.handle(MenuAction::adjust(-1), runtime));
    CHECK(runtime.audio.asio_driver == first);
    check_render_only(controller.handle(MenuAction::adjust(-1), runtime));
    CHECK(runtime.audio.asio_driver.empty());
    REQUIRE(controller.dirty());

    const auto save = controller.handle(MenuAction::activate(), runtime, AudioSettingId::Back);
    CHECK(save.persist_config);
    CHECK(save.restart_audio);
    CHECK(save.navigate_back);
    CHECK_FALSE(controller.dirty());
    const auto clean_back = controller.handle(MenuAction::back(), runtime);
    CHECK_FALSE(clean_back.persist_config);
    CHECK_FALSE(clean_back.restart_audio);
}

TEST_CASE("ASIO driver discovery and unavailable drivers do not discard the saved selection") {
    tenriff::config::RuntimeConfig runtime;
    runtime.audio.backend = tenriff::audio::AudioBackend::ASIO;
    runtime.audio.asio_driver = "{33333333-3333-3333-3333-333333333333}";
    const auto saved_driver = runtime.audio.asio_driver;
    AudioSettingsController controller;
    static_cast<void>(controller.select(AudioSettingId::AsioDriver));
    controller.set_asio_drivers({});
    check_no_effects(controller.handle(MenuAction::adjust(1), runtime));
    check_no_effects(controller.handle(MenuAction::adjust(-1), runtime));
    check_no_effects(controller.handle(MenuAction::activate(), runtime));
    CHECK(runtime.audio.asio_driver == saved_driver);
    CHECK_FALSE(controller.dirty());
    auto view = AudioSettingsView::build(controller, runtime, tenriff::ui::Language::English);
    CHECK(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::AsioDriver)].value == "No 64-bit ASIO driver");
    CHECK_FALSE(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::AsioDriver)].activatable);
    CHECK_FALSE(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::AsioDriver)].adjustable);

    controller.set_asio_drivers({{"{11111111-1111-1111-1111-111111111111}", "Another Device"}});
    view = AudioSettingsView::build(controller, runtime, tenriff::ui::Language::English);
    CHECK(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::AsioDriver)].value == "Saved driver unavailable");
    CHECK(runtime.audio.asio_driver == saved_driver);
    CHECK_FALSE(controller.dirty());
    controller.set_asio_drivers({{saved_driver, "Reconnected Device"}});
    view = AudioSettingsView::build(controller, runtime, tenriff::ui::Language::Korean);
    CHECK(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::AsioDriver)].value == "Reconnected Device");
    CHECK(runtime.audio.asio_driver == saved_driver);
    CHECK_FALSE(controller.dirty());
}

TEST_CASE("audio backend changes retain the selected driver and restore the WASAPI preset") {
    for (const std::string preset : {"basic", "high"}) {
        tenriff::config::RuntimeConfig runtime;
        runtime.audio_ui.preset = preset;
        runtime.audio.asio_driver = "{11111111-1111-1111-1111-111111111111}";
        runtime.audio.sample_rate = 96000;
        runtime.audio.frames_per_buffer = 64;
        runtime.audio.periods = 2;
        runtime.audio.exclusive_mode = false;
        AudioSettingsController controller;
        static_cast<void>(controller.select(AudioSettingId::Backend));
        check_render_only(controller.handle(MenuAction::adjust(1), runtime));
        CHECK(runtime.audio.backend == tenriff::audio::AudioBackend::ASIO);
        CHECK(runtime.audio.frames_per_buffer == 64);
        CHECK(runtime.audio.periods == 2);
        check_render_only(controller.handle(MenuAction::activate(), runtime));
        CHECK(runtime.audio.backend == tenriff::audio::AudioBackend::WASAPI);
        CHECK(runtime.audio.frames_per_buffer == (preset == "basic" ? 256 : 320));
        CHECK(runtime.audio.periods == 3);
        CHECK(runtime.audio.sample_rate == 96000);
        CHECK(runtime.audio.asio_driver == "{11111111-1111-1111-1111-111111111111}");
        CHECK_FALSE(runtime.audio.exclusive_mode);
    }
}

TEST_CASE("ASIO sample rates and sample counts cycle with wraparound and accept non-preset starting values") {
    tenriff::config::RuntimeConfig runtime;
    runtime.audio.backend = tenriff::audio::AudioBackend::ASIO;
    AudioSettingsController controller;
    static_cast<void>(controller.select(AudioSettingId::SampleRate));
    check_no_effects(controller.handle(MenuAction::adjust(0), runtime));
    CHECK_FALSE(controller.dirty());
    runtime.audio.sample_rate = 44100;
    check_render_only(controller.handle(MenuAction::adjust(-1), runtime));
    CHECK(runtime.audio.sample_rate == 192000);
    check_render_only(controller.handle(MenuAction::activate(), runtime));
    CHECK(runtime.audio.sample_rate == 44100);
    check_render_only(controller.handle(MenuAction::adjust(1), runtime));
    CHECK(runtime.audio.sample_rate == 48000);
    runtime.audio.sample_rate = 50000;
    check_render_only(controller.handle(MenuAction::adjust(1), runtime));
    CHECK(runtime.audio.sample_rate == 88200);
    runtime.audio.sample_rate = 50000;
    check_render_only(controller.handle(MenuAction::adjust(-1), runtime));
    CHECK(runtime.audio.sample_rate == 48000);

    static_cast<void>(controller.select(AudioSettingId::BufferFrames));
    runtime.audio.frames_per_buffer = 32;
    check_render_only(controller.handle(MenuAction::adjust(-1), runtime));
    CHECK(runtime.audio.frames_per_buffer == 2048);
    check_render_only(controller.handle(MenuAction::activate(), runtime));
    CHECK(runtime.audio.frames_per_buffer == 32);
    runtime.audio.frames_per_buffer = 320;
    check_render_only(controller.handle(MenuAction::adjust(1), runtime));
    CHECK(runtime.audio.frames_per_buffer == 512);
    runtime.audio.frames_per_buffer = 320;
    check_render_only(controller.handle(MenuAction::adjust(-1), runtime));
    CHECK(runtime.audio.frames_per_buffer == 256);
    const auto view = AudioSettingsView::build(controller, runtime, tenriff::ui::Language::English);
    CHECK(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::SampleRate)].value == "48000 Hz");
    CHECK(view.rows[*tenriff::app::menu::settings::audio_setting_index(AudioSettingId::BufferFrames)].value == "256 samples");
    for (const auto id : {AudioSettingId::SampleRate, AudioSettingId::BufferFrames}) {
        const auto index = *tenriff::app::menu::settings::audio_setting_index(id);
        CHECK(view.rows[index].activatable);
        CHECK(view.rows[index].adjustable);
    }
}

TEST_CASE("disabled audio backend controls cannot change the saved device or buffer configuration") {
    tenriff::config::RuntimeConfig runtime;
    runtime.audio.backend = tenriff::audio::AudioBackend::ASIO;
    runtime.audio_ui.preset = "high";
    runtime.audio.frames_per_buffer = 64;
    runtime.audio.sample_rate = 96000;
    runtime.audio.asio_driver = "saved-device";
    AudioSettingsController controller;
    controller.set_asio_drivers({{"another-device", "Another Device"}});
    static_cast<void>(controller.select(AudioSettingId::Preset));
    check_no_effects(controller.handle(MenuAction::adjust(1), runtime));
    check_no_effects(controller.handle(MenuAction::adjust(-1), runtime));
    check_no_effects(controller.handle(MenuAction::activate(), runtime));
    CHECK(runtime.audio_ui.preset == "high");
    CHECK(runtime.audio.frames_per_buffer == 64);

    runtime.audio.backend = tenriff::audio::AudioBackend::WASAPI;
    for (const auto id : {AudioSettingId::AsioDriver, AudioSettingId::SampleRate, AudioSettingId::BufferFrames}) {
        static_cast<void>(controller.select(id));
        check_no_effects(controller.handle(MenuAction::adjust(1), runtime));
        check_no_effects(controller.handle(MenuAction::adjust(-1), runtime));
        check_no_effects(controller.handle(MenuAction::activate(), runtime));
    }
    CHECK(runtime.audio.asio_driver == "saved-device");
    CHECK(runtime.audio.sample_rate == 96000);
    CHECK(runtime.audio.frames_per_buffer == 64);
    CHECK_FALSE(controller.dirty());
}

TEST_CASE("audio outro choice toggles with keyboard and pointer without changing mix") {
    tenriff::config::RuntimeConfig runtime;
    AudioSettingsController controller;
    check_render_only(controller.handle(MenuAction::activate(), runtime, AudioSettingId::PlayToEnd));
    CHECK_FALSE(runtime.audio_ui.play_to_end);
    check_render_only(controller.handle(MenuAction::adjust(-1), runtime, AudioSettingId::PlayToEnd));
    CHECK(runtime.audio_ui.play_to_end);
    CHECK(runtime.audio_ui.master_volume == doctest::Approx(0.70));
    CHECK(controller.handle(MenuAction::back(), runtime).persist_config);
}
