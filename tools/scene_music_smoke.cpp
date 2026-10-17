#include "app/MenuApp.h"
#include "app/MenuMusicController.h"
#include "audio/WasapiBackend.h"
#include "config/SimpleJson.h"
#include "../tests/support/MockAsioDriver.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;
using tenriff::config::JsonArray;
using tenriff::config::JsonObject;
using tenriff::config::JsonValue;
struct Report {
    JsonArray checks;
    int failures = 0;
    void check(const std::string& name, bool passed) {
        checks.emplace_back(JsonObject{{"name",JsonValue(name)}, {"passed",JsonValue(passed)}});
        failures += !passed;
        std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
    }
};
void write_wave(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    const auto put = [&](uint32_t n, int bytes) { for(int i=0;i<bytes;++i) out.put(static_cast<char>(n>>(i*8))); };
    constexpr uint32_t frames=4410, bytes=frames*4;
    out.write("RIFF",4); put(36+bytes,4); out.write("WAVEfmt ",8); put(16,4);
    put(1,2); put(2,2); put(44100,4); put(44100*4,4); put(4,2); put(16,2);
    out.write("data",4); put(bytes,4);
    for(uint32_t i=0;i<frames;++i) { put(i%2 ? 4096:8192,2); put(i%2 ? 8192:4096,2); }
}
}

namespace tenriff::app {
struct MenuMusicControllerTestAccess {
    static bool open(const MenuMusicController& m) { return m.open_ && m.audio_.is_running(); }
    static bool failed(const MenuMusicController& m) { return m.open_failed_; }
    static int64_t played(const MenuMusicController& m) { return m.audio_.playback_samples(); }
    static float gain(const MenuMusicController& m) { return m.output_gain_.load(); }
    static const audio::AudioConfig& config(const MenuMusicController& m) { return m.requested_config_; }
    static void mock(MenuMusicController& m, const std::shared_ptr<tests::MockState>& state) {
        m.audio_.asio_test_factory_ = tests::mock_factory(state);
    }
    static void mock_audio(audio::AudioThread& output, const std::shared_ptr<tests::MockState>& state) {
        output.asio_test_factory_ = tests::mock_factory(state);
    }
};

struct MenuAppAudioSettingsTestAccess {
    static void check_scenes(const std::string& path, Report& report) {
        auto app = std::make_unique<MenuApp>();
        app->config_.audio.backend = audio::AudioBackend::ASIO;
        app->config_.audio.asio_driver = "fixture-scene-driver";
        app->config_.audio_ui.title_music = "default";
        app->config_.audio_ui.background_sound_enabled = true;
        app->config_.audio_ui.master_volume = 0.5;
        app->config_.audio_ui.menu_music_volume = 0.4;
        auto state = std::make_shared<tests::MockState>();
        MenuMusicControllerTestAccess::mock(app->menu_music_, state);
        const std::pair<MenuApp::Screen,const char*> scenes[]{
            {MenuApp::Screen::Title,"title"}, {MenuApp::Screen::OptionsHub,"options"},
            {MenuApp::Screen::Result,"result_clear"}, {MenuApp::Screen::Result,"result_failed"}};
        for(const auto& [screen,key] : scenes) {
            app->menu_navigator_.reset(screen);
            const std::string scene=key;
            const char* file=scene=="title" ? "Main Menu.mp3" : scene=="options" ? "Options.mp3" :
                scene=="result_clear" ? "Clear.mp3" : "Failed.mp3";
            app->menu_music_scene_key_=key;
            app->menu_music_scene_path_=std::filesystem::absolute(std::filesystem::path("Mainmusic")/file).u8string();
            app->last_game_over_=std::string(key)=="result_failed";
            const auto end=Clock::now()+std::chrono::seconds(5);
            do { app->sync_menu_music(); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
            while(!MenuMusicControllerTestAccess::open(app->menu_music_) && Clock::now()<end);
            report.check(std::string("scene.")+key+".selected_driver", MenuMusicControllerTestAccess::open(app->menu_music_) &&
                MenuMusicControllerTestAccess::config(app->menu_music_).asio_driver=="fixture-scene-driver");
        }
        app->menu_navigator_.reset(MenuApp::Screen::SongSelect);
        SongSelectScreen::PreviewDecodeResult preview;
        preview.path="fixture-preview"; preview.sample_rate=44100;
        preview.samples=std::make_shared<std::vector<float>>(8820,0.0f);
        auto preview_state = std::make_shared<tests::MockState>();
        MenuMusicControllerTestAccess::mock_audio(app->audio_thread_, preview_state);
        const int releases=state->releases;
        const bool preview_started=app->start_song_preview_audio(preview);
        report.check("scene.preview_releases_old_owner",preview_started && app->audio_thread_.is_running() &&
            !MenuMusicControllerTestAccess::open(app->menu_music_) && state->releases>releases);
        app->stop_song_preview_audio();
        report.check("scene.preview_stops",!app->audio_thread_.is_running() && preview_state->releases>0);
        const auto reopen_scene=[&] {
            app->menu_navigator_.reset(MenuApp::Screen::Title);
            app->menu_music_scene_key_="title";
            app->menu_music_scene_path_=path;
            const auto end=Clock::now()+std::chrono::seconds(5);
            do { app->sync_menu_music(); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
            while(!MenuMusicControllerTestAccess::open(app->menu_music_) && Clock::now()<end);
            return MenuMusicControllerTestAccess::open(app->menu_music_);
        };
        for(const auto& [screen,key] : {std::pair{MenuApp::Screen::Gameplay,"gameplay"},
                                      std::pair{MenuApp::Screen::BmsEditor,"editor"}}) {
            const bool was_open=reopen_scene();
            const int before_release=state->releases;
            app->menu_navigator_.reset(screen); app->sync_menu_music();
            report.check(std::string("scene.")+key+"_releases_output",was_open &&
                !MenuMusicControllerTestAccess::open(app->menu_music_) && state->releases>before_release);
        }
        // A completed old editor decode outside the editor is discarded before audio opens.
        const bool scene_open=reopen_scene();
        std::promise<BmsEditorPreviewResult> pending;
        app->bms_editor_preview_future_=pending.get_future();
        BmsEditorPreviewResult stale;
        stale.sample_rate=44100;
        stale.stereo_samples=std::make_shared<std::vector<float>>(8820,0.0f);
        pending.set_value(std::move(stale));
        app->menu_navigator_.reset(MenuApp::Screen::Result); app->service_bms_editor_preview();
        report.check("scene.stale_editor_decode_discarded",scene_open &&
            !app->bms_editor_preview_future_.valid() && !app->audio_thread_.is_running() &&
            MenuMusicControllerTestAccess::open(app->menu_music_));
    }
};
}

int wmain(int argc,wchar_t** argv) {
    if(argc!=2) return 2;
    const auto folder=std::filesystem::absolute(argv[1]);
    if(!std::filesystem::create_directory(folder)) return 2;
    const auto path=folder/"fixture.wav"; write_wave(path);
    Report report;
    using namespace tenriff;
    using Access=app::MenuMusicControllerTestAccess;
    {
        const auto devices=audio::WasapiBackend::enumerate_output_devices();
        report.check("wasapi.active_endpoint_available",!devices.empty());
        if(!devices.empty()) {
            app::MenuMusicController music;
            audio::AudioConfig config; config.device_id=devices.front().id;
            music.set_output_muted(true);
            const auto wait_open=[&] {
                const auto end=Clock::now()+std::chrono::seconds(6);
                do { music.play_looping_file(path.u8string(),0.2,config); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
                while(!Access::open(music) && !Access::failed(music) && Clock::now()<end);
                return Access::open(music);
            };
            report.check("wasapi.selected_endpoint_decoded_and_opened",wait_open() && Access::config(music).device_id==devices.front().id);
            const auto before=Access::played(music);
            std::this_thread::sleep_for(std::chrono::milliseconds(180));
            report.check("wasapi.muted_cursor_advances",Access::played(music)>before && Access::gain(music)==0);
            report.check("wasapi.menu_shared_mode",!Access::config(music).exclusive_mode);
            config.device_id="{tenriff-intentionally-missing-endpoint}";
            report.check("wasapi.missing_endpoint_no_fallback",!wait_open() && Access::failed(music));
            config.device_id=devices.front().id;
            report.check("wasapi.same_track_device_recovery",wait_open());
            music.play_looping_file(path.u8string(),0.0,config);
            report.check("wasapi.zero_gain_releases_output",!Access::open(music));
            report.check("wasapi.zero_gain_recovers",wait_open());
            music.stop(); report.check("wasapi.stop_releases_output",!Access::open(music));
        }
    }
    app::MenuAppAudioSettingsTestAccess::check_scenes(path.u8string(),report);
    std::ofstream out(folder/"summary.json");
    out << config::json_stringify(JsonValue(JsonObject{{"passed",JsonValue(report.failures==0)},
        {"physical_listening",JsonValue(false)}, {"checks",JsonValue(report.checks)}}));
    return report.failures ? 1 : 0;
}
