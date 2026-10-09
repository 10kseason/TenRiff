#ifndef NOMINMAX
#define NOMINMAX
#endif

// Developer integration smoke. Run only in a newly created sandbox; never point
// it at an installed game, an account, a real song library, or a user profile.
#include "app/MenuApp.h"
#include "app/GameSession.h"
#include "config/KeycodeMap.h"
#include "config/SimpleJson.h"
#include "timing/HighResClock.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;
using tenriff::config::JsonArray;
using tenriff::config::JsonObject;
using tenriff::config::JsonValue;
using Screen = tenriff::app::menu::Screen;
using Kind = tenriff::render::MenuHitTargetKind;
using Part = tenriff::render::MenuHitPart;

struct Report {
    JsonArray checks;
    unsigned failed = 0;
    void check(const std::string& name, bool passed, std::string detail = {}) {
        checks.emplace_back(JsonObject{{"name", JsonValue(name)}, {"passed", JsonValue(passed)},
                                      {"detail", JsonValue(std::move(detail))}});
        failed += !passed;
        std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
    }
};

// MenuWindow's timestamp is a per-render result: the next render clears it to
// zero before doing any work. Read it on that same render thread immediately
// after render_tick, then publish a cumulative observation for slow test polls.
// This instrumentation exists only in the smoke executable, not the product.
struct PresentProbe {
    std::atomic<std::uint64_t> successful_presents{0};
    std::atomic<std::uint64_t> callbacks_without_present{0};
    std::atomic<std::uint64_t> raw_zero_reads_after_progress{0};
    std::atomic<std::int64_t> last_completion_ns{0};

    void record(std::int64_t completed_ns) {
        if (completed_ns <= 0) {
            callbacks_without_present.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        last_completion_ns.store(completed_ns, std::memory_order_release);
        successful_presents.fetch_add(1, std::memory_order_release);
    }
};
PresentProbe present_probe;

// Little-endian, mono 16-bit PCM containing silence. The fixture exercises the
// real decoder and audio clock without playing a sound or using external media.
void write_silent_wave(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    auto word = [&](std::uint32_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) out.put(static_cast<char>(value >> (8 * i)));
    };
    constexpr std::uint32_t bytes = 960;
    out.write("RIFF", 4); word(36 + bytes, 4); out.write("WAVEfmt ", 8);
    word(16, 4); word(1, 2); word(1, 2); word(48000, 4); word(96000, 4);
    word(2, 2); word(16, 2); out.write("data", 4); word(bytes, 4);
    for (unsigned i = 0; i < bytes; ++i) out.put(0);
    if (!out) throw std::runtime_error("Cannot write synthetic WAV");
}

void write_chart(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    write_silent_wave(directory / "silence.wav");
    std::ofstream out(directory / "smoke.bms", std::ios::binary);
    out << "#PLAYER 1\n#TITLE Menu App Smoke Fixture\n#ARTIST TenRiff QA\n"
           "#BPM 120\n#PLAYLEVEL 1\n#RANK 2\n#4K\n#WAV01 silence.wav\n"
           "#00011:01\n#00112:01\n#00213:01\n#00214:0001\n";
    if (!out) throw std::runtime_error("Cannot write synthetic BMS");
}
}  // namespace

namespace tenriff::app {

// These existing test friends are defined only in this executable. Menu actions
// run on their owner thread; the observer reads only mutex-protected HUD data.
struct MenuAppVisualTestAccess {
    static void attach_present_probe(MenuApp& app) {
        // Replacing a running callback is a data race. Join it first and use
        // the same window/config/shutdown setup as restart_render_thread. The
        // fixture recreates its window once before any transition checks.
        app.render_thread_.shutdown();
        app.menu_window_.set_config(app.current_window_config());
        if (!app.render_thread_.initialize(app.current_render_config(), [&app]() {
                app.render_tick();
                present_probe.record(app.menu_window_.last_present_completion_ns());
            }, [&app]() { app.menu_window_.shutdown(); }) || !app.render_thread_.start()) {
            throw std::runtime_error("Cannot attach smoke Present observer");
        }
    }
    static Screen screen(const MenuApp& app) { return app.current_screen(); }
    static void key(MenuApp& app, const char* name) {
        const auto code = config::KeycodeMap::to_keycode(name);
        if (!code) throw std::runtime_error("Unknown fixture key");
        const auto now = timing::HighResClock::now_ns();
        app.handle_input_event({*code, input::InputState::Pressed, now});
        app.handle_input_event({*code, input::InputState::Released, now + 1});
    }
    static void click(MenuApp& app, Kind kind, int index, Part part = Part::Activate) {
        render::MenuClickEvent event;
        event.kind = kind; event.index = index; event.part = part;
        app.handle_menu_click(event);
    }
    static void isolate(MenuApp& app) {
        // ConfigLoader restores default URLs when reading empty strings. Clear
        // live URLs after initialization too; never open account/online routes.
        app.config_.ui.online_records_server_url.clear();
        app.config_.ui.tenriff_main_server_url.clear();
        app.config_.ui.private_server_url.clear();
        app.ranked_account_main_server_url_.clear();
        app.ranked_account_private_server_url_.clear();
        app.ranked_account_active_server_url_.clear();
        app.global_chat_service_.clear();
        app.global_chat_service_.shutdown();
        app.online_records_service_.shutdown();
        app.sites_leaderboard_service_.shutdown();
        // The existing practice launch also bypasses ranked authorization and
        // automatic Sites replay submission, independently of the empty URLs.
        app.bms_editor_practice_start_seconds_ = 0.0;
    }
    static bool offline(const MenuApp& app) {
        return app.config_.ui.online_records_server_url.empty() &&
               app.config_.ui.tenriff_main_server_url.empty() &&
               app.config_.ui.private_server_url.empty() &&
               app.ranked_account_signed_in_username_.empty() &&
               !app.sites_connection_saved_ && app.bms_editor_practice_start_seconds_.has_value();
    }
    static bool poll_library(MenuApp& app) {
        SongIndex index;
        std::vector<std::string> warnings;
        if (app.song_indexer_.poll_result(index, warnings)) {
            app.update_song_list(std::move(index));
            app.publish_snapshot();
        }
        return app.visible_song_count() == 1;
    }
    static bool present(MenuApp& app, std::string& detail) {
        const auto prior = present_probe.successful_presents.load(std::memory_order_acquire);
        const auto prior_without_present = present_probe.callbacks_without_present.load(std::memory_order_acquire);
        const auto deadline = Clock::now() + std::chrono::seconds(3);
        // Let render_tick consume the newly published snapshot. This verifies
        // presentation liveness, not pixels/hit geometry (a separate fixture).
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        while (Clock::now() < deadline) {
            const auto completed = present_probe.successful_presents.load(std::memory_order_acquire);
            // Keep the old asynchronous sample as diagnostic evidence only.
            // Zero alongside successful progress demonstrates why that sample
            // cannot decide whether the renderer is alive.
            const auto raw = app.menu_window_.last_present_completion_ns();
            const bool progressed = completed > prior;
            if (progressed && raw == 0)
                present_probe.raw_zero_reads_after_progress.fetch_add(1, std::memory_order_relaxed);
            detail = "Successful Presents=" + std::to_string(completed - prior) +
                "; callbacks without Present=" + std::to_string(
                    present_probe.callbacks_without_present.load(std::memory_order_acquire) - prior_without_present) +
                "; polled per-render timestamp=" + std::to_string(raw);
            if (app.menu_window_.had_fatal_error() || app.menu_window_.should_close()) {
                detail += "; window reports fatal error or close";
                return false;
            }
            if (progressed) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        detail += "; no successful Present within 3 seconds";
        return false;
    }
    static std::size_t rows(MenuApp& app) {
        std::lock_guard<std::mutex> lock(app.snapshot_mutex_);
        return app.snapshot_.render.generic.rows.size();
    }
    static const config::RuntimeConfig& config(const MenuApp& app) { return app.config_; }
    static bool result(const MenuApp& app) { return app.has_result_; }
    static void check_loading_pacing(MenuApp& app, Report& report) {
        const auto saved_refresh = app.config_.graphics.refresh_hz;
        app.config_.graphics.refresh_hz = 0;
        app.reset_screen(Screen::Gameplay);
        {
            std::lock_guard<std::mutex> lock(app.gameplay_hud_mutex_);
            app.reset_gameplay_hud_state(app.gameplay_hud_);
        }
        app.update_gameplay_loading_state(56, "Parsing chart");
        report.check("loading.render_cap_60", app.current_render_config().fps_limit == 60);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto previous = present_probe.successful_presents.load(std::memory_order_acquire);
        const auto deadline = Clock::now() + std::chrono::milliseconds(600);
        std::this_thread::sleep_until(deadline);
        const auto presents = present_probe.successful_presents.load(std::memory_order_acquire) - previous;
        report.check("loading.bounded_actual_presents", presents >= 10 && presents <= 42,
                     "Render-thread successful Presents over 600 ms=" + std::to_string(presents));
        app.reset_screen(Screen::SongSelect);
        app.config_.graphics.refresh_hz = saved_refresh;
        {
            std::lock_guard<std::mutex> lock(app.gameplay_hud_mutex_);
            app.reset_gameplay_hud_state(app.gameplay_hud_);
        }
        app.apply_runtime_graphics_config();
        app.publish_snapshot();
        report.check("loading.lobby_pacing_restored", app.current_render_config().fps_limit > 60);
    }
    struct Observation {
        bool loading = false, countdown = false, playing = false;
        int64_t sample = 0;
        int64_t presented_ns = 0;
    };
    static Observation observe(MenuApp& app) {
        std::lock_guard<std::mutex> lock(app.gameplay_hud_mutex_);
        const auto& hud = app.gameplay_hud_;
        return {hud.loading, hud.countdown_active,
                hud.active && !hud.loading && !hud.countdown_active && hud.current_sample > 0,
                hud.current_sample, present_probe.last_completion_ns.load(std::memory_order_acquire)};
    }
};

struct MenuAppFeedbackTestAccess {
    static void stop_physical_input(GameSession& session) {
        // This independent session tests control flow, not a physical device.
        // Stop its sole queue producer before playback to prevent external keys
        // from affecting the deterministic control sequence.
        session.input_thread_.stop();
        while (session.input_thread_.queue().pop().has_value()) {}
    }
    static void escape(GameSession& session) {
        // The normal audio callback applies controls while holding this mutex.
        // Use the same serialization rather than writing pause state directly
        // or introducing a second producer into the input SPSC queue.
        std::lock_guard<std::mutex> lock(session.engine_mutex_);
        const input::InputEvent event{session.escape_keycode_, input::InputState::Pressed,
                                      timing::HighResClock::now_ns()};
        (void)session.handle_control_input(event);
    }
    static void timeout(GameSession& session) {
        session.user_aborted_.store(true, std::memory_order_release);
        session.stop_requested_.store(true, std::memory_order_release);
        session.finished_.store(true, std::memory_order_release);
    }
};
}  // namespace tenriff::app

namespace {
using Access = tenriff::app::MenuAppVisualTestAccess;

void expect_screen(tenriff::app::MenuApp& app, Report& report,
                   const std::string& name, Screen expected) {
    const bool matched = Access::screen(app) == expected;
    report.check(name + ".screen", matched,
        "expected=" + std::to_string(static_cast<int>(expected)) +
        "; actual=" + std::to_string(static_cast<int>(Access::screen(app))));
    std::string present_detail;
    const bool presented = Access::present(app, present_detail);
    report.check(name + ".present", presented, std::move(present_detail));
    if (!matched) throw std::runtime_error(name + " screen transition failed");
}

void menu_flow(tenriff::app::MenuApp& app, Report& report) {
    Access::isolate(app);
    report.check("network.disabled_and_practice_guarded", Access::offline(app));
    expect_screen(app, report, "title", Screen::Title);
    const auto deadline = Clock::now() + std::chrono::seconds(20);
    bool indexed = false;
    while (Clock::now() < deadline && !(indexed = Access::poll_library(app)))
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    report.check("library.indexed_exactly_one_synthetic_chart", indexed);
    if (!indexed) throw std::runtime_error("Synthetic library did not index");

    Access::click(app, Kind::TitleButton, 2);
    expect_screen(app, report, "options", Screen::OptionsHub);
    report.check("options.eight_consolidated_entries", Access::rows(app) == 8);
    struct Page { int index; const char* name; Screen screen; };
    const Page pages[] = {
        {0, "mode", Screen::ModeSelect}, {1, "keymap", Screen::Keymap},
        {2, "skins", Screen::SettingsSkins}, {3, "graphics", Screen::SettingsGraphics},
        {4, "audio", Screen::SettingsAudio}, {5, "input", Screen::SettingsInput},
        {6, "calibration", Screen::SettingsCalibration}, {7, "profile", Screen::QuickSetup}};
    for (const auto& page : pages) {
        Access::click(app, Kind::OptionsItem, page.index);
        expect_screen(app, report, page.name, page.screen);
        if (page.index == 0) {
            Access::click(app, Kind::SettingsRow,
                static_cast<int>(tenriff::app::menu::settings::ModeSettingId::Mods), Part::SelectOnly);
            Access::key(app, "Enter");
            expect_screen(app, report, "mode.nested_mods", Screen::ModeMods);
            Access::key(app, "Esc");
            expect_screen(app, report, "mode.nested_mods.back", Screen::ModeSelect);
            Access::click(app, Kind::SettingsRow,
                static_cast<int>(tenriff::app::menu::settings::ModeSettingId::Mods));
            expect_screen(app, report, "mode.nested_mods.pointer", Screen::ModeMods);
            Access::key(app, "Esc");
            expect_screen(app, report, "mode.nested_mods.pointer.back", Screen::ModeSelect);
        } else if (page.index == 1) {
            Access::click(app, Kind::KeymapButton,
                static_cast<int>(tenriff::app::menu::settings::KeymapActionId::NkroTest));
            expect_screen(app, report, "keymap.nested_test", Screen::KeymapTest);
            Access::key(app, "Esc");
            expect_screen(app, report, "keymap.nested_test.back", Screen::Keymap);
        } else if (page.index == 3) {
            Access::click(app, Kind::SettingsRow,
                static_cast<int>(tenriff::app::menu::settings::GraphicsSettingId::Bga), Part::Increment);
            report.check("graphics.bga_toggle", !Access::config(app).graphics.bga_enabled);
        } else if (page.index == 4) {
            Access::click(app, Kind::SettingsRow,
                static_cast<int>(tenriff::app::menu::settings::AudioSettingId::Normalize), Part::Increment);
            report.check("audio.normalize_toggle", Access::config(app).audio_ui.normalize_audio);
        }
        Access::key(app, "Esc");
        expect_screen(app, report, std::string(page.name) + ".back", Screen::OptionsHub);
    }
    const auto saved = tenriff::config::ConfigLoader{}.load_profile("profiles/menu-smoke");
    report.check("settings.profile_readback", saved.success() &&
        !saved.config.graphics.bga_enabled && saved.config.audio_ui.normalize_audio);
    report.check("settings.live_network_guards_preserved", Access::offline(app));
    Access::key(app, "Esc");
    expect_screen(app, report, "options.back", Screen::Title);
    Access::click(app, Kind::TitleButton, 0);
    expect_screen(app, report, "lobby", Screen::SongSelect);
    Access::check_loading_pacing(app, report);

    std::atomic<bool> observing{true};
    bool loading = false, countdown = false, playing = false, gameplay_present = false;
    int64_t first_play_present = 0, last_sample = 0;
    std::thread observer([&] {
        while (observing.load(std::memory_order_acquire)) {
            const auto value = Access::observe(app);
            loading |= value.loading; countdown |= value.countdown; playing |= value.playing;
            last_sample = std::max(last_sample, value.sample);
            if (value.playing) {
                if (!first_play_present) first_play_present = value.presented_ns;
                gameplay_present |= value.presented_ns > first_play_present;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    try {
        // Actual SongStartButton dispatch -> launch_selected_song -> synchronous
        // MenuApp::launch_gameplay -> real GameSession -> actual Result state.
        Access::click(app, Kind::SongStartButton, 0);
    } catch (...) {
        observing.store(false, std::memory_order_release); observer.join(); throw;
    }
    observing.store(false, std::memory_order_release); observer.join();
    report.check("menu_gameplay.loading_observed", loading);
    report.check("menu_gameplay.start_countdown_observed", countdown);
    report.check("menu_gameplay.audio_clock_advanced", playing && last_sample > 0);
    report.check("menu_gameplay.presents_advanced", gameplay_present);
    expect_screen(app, report, "natural_result", Screen::Result);
    report.check("natural_result.has_stats", Access::result(app));
    Access::key(app, "Space");
    const auto result_rows = Access::rows(app);
    Access::click(app, Kind::SettingsRow, 0);
    const bool mouse_return = Access::screen(app) == Screen::SongSelect;
    report.check("result.continue_mouse_dispatch", mouse_return,
                 "Result generic row count=" + std::to_string(result_rows));
    if (!mouse_return) {
        // Keep the failure above: keyboard recovery must not hide a broken mouse
        // route or make the final process exit successful.
        Access::key(app, "Enter");
        report.check("result.keyboard_recovery", Access::screen(app) == Screen::SongSelect);
    }
    expect_screen(app, report, "result_to_lobby", Screen::SongSelect);
    report.check("lobby.no_stale_gameplay_hud", !Access::observe(app).playing);
    report.check("lobby.network_guards_preserved", Access::offline(app));
}

void separate_pause_flow(const tenriff::app::CommandLineOptions& options, Report& report) {
    using Session = tenriff::app::GameSession;
    using SessionAccess = tenriff::app::MenuAppFeedbackTestAccess;
    auto session = std::make_unique<Session>();
    session->set_practice_no_fail_override(true);
    int phase = 0;
    bool countdown = false, resume_countdown = false, frozen = true, timed_out = false;
    int64_t paused_sample = -1, resumed_sample = -1;
    auto phase_time = Clock::now();
    const auto deadline = Clock::now() + std::chrono::seconds(25);
    session->set_hud_callback([&](const Session::HudSnapshot& hud) {
        countdown |= hud.countdown_active && !hud.paused;
        if (Clock::now() > deadline) {
            timed_out = true; SessionAccess::timeout(*session); return;
        }
        if (phase == 0 && hud.active && !hud.countdown_active && hud.current_sample > hud.sample_rate / 5) {
            SessionAccess::escape(*session); phase = 1; phase_time = Clock::now();
        } else if (phase == 1 && hud.paused && !hud.countdown_active) {
            // Permit the first paused audio buffer to establish the anchor.
            if (Clock::now() - phase_time < std::chrono::milliseconds(80)) return;
            if (paused_sample < 0) paused_sample = hud.current_sample;
            frozen &= hud.current_sample == paused_sample;
            if (Clock::now() - phase_time > std::chrono::milliseconds(300)) {
                session->request_pause_action(static_cast<int>(tenriff::app::GameplayPauseAction::Continue));
                phase = 2;
            }
        } else if (phase == 2) {
            resume_countdown |= hud.paused && hud.countdown_active;
            if (!hud.paused && !hud.countdown_active && hud.current_sample > paused_sample) {
                resumed_sample = hud.current_sample; phase = 3;
            }
        } else if (phase == 3 && hud.current_sample > resumed_sample + hud.sample_rate / 5) {
            SessionAccess::escape(*session); phase = 4;
        } else if (phase == 4 && hud.paused && !hud.countdown_active) {
            session->request_pause_action(static_cast<int>(tenriff::app::GameplayPauseAction::Exit));
            phase = 5;
        }
    });
    const bool initialized = session->initialize(options);
    report.check("separate_session.initialize", initialized);
    if (initialized) {
        SessionAccess::stop_physical_input(*session);
        session->run();
        report.check("separate_session.countdown", countdown);
        report.check("separate_session.pause_freezes_sample", paused_sample >= 0 && frozen);
        report.check("separate_session.resume_countdown", resume_countdown);
        report.check("separate_session.resumed_sample_advances", resumed_sample > paused_sample);
        report.check("separate_session.pause_exit_aborts", phase == 5 && session->was_exit_requested() &&
                                                        session->was_user_aborted());
        report.check("separate_session.bounded_and_audio_ok", !timed_out && session->audio_error().empty(),
                     session->audio_error());
    }
    session->shutdown();
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::cerr << "Usage: menu_app_smoke NEW_SANDBOX_DIRECTORY\n"
                     "Creates only synthetic songs/profile and smoke-report.json in a new directory.\n";
        return 2;
    }
    const auto original_cwd = std::filesystem::current_path();
    const auto sandbox = std::filesystem::absolute(argv[1]).lexically_normal();
    std::error_code error;
    // create_directory is exclusive: existing empty folders also fail closed.
    if (!std::filesystem::create_directory(sandbox, error)) {
        std::cerr << "Refusing an existing or unavailable sandbox directory.\n"; return 2;
    }
    Report report;
    const auto began = Clock::now();
    try {
        std::filesystem::current_path(sandbox);
        write_chart("songs/fixture");
        tenriff::config::ConfigLoader loader;
        auto config = loader.defaults();
        config.graphics.display_mode = "windowed";
        config.graphics.resolution = "1080p";
        config.graphics.vsync = false;
        config.graphics.refresh_hz = -1;
        config.graphics.performance_overlay = false;
        config.audio.exclusive_mode = false;
        config.audio_ui.master_volume = 0.0;
        config.audio_ui.title_music = "none";
        config.audio_ui.play_to_end = false;
        config.ui.profile_nickname = "Synthetic Smoke";
        config.ui.require_enter_to_exit = false;
        config.ui.online_records_server_url.clear();
        config.ui.tenriff_main_server_url.clear();
        config.ui.private_server_url.clear();
        config.ui.difficulty_table_url.clear();
        config.ui.difficulty_table_path.clear();
        config.ui.last_played_chart_path.clear();
        config.ui.active_song_source.clear();
        config.ui.recent_song_sources.clear();
        config.ui.result_tail_ms = 100.0;
        config.mode.practice_no_fail_enabled = true;
        std::string reason;
        tenriff::config::KeymapManager keymaps;
        if (!loader.save_profile("profiles/menu-smoke", config, &reason) ||
            !keymaps.save_profile("profiles/menu-smoke", keymaps.default_keymap(), &reason))
            throw std::runtime_error("Cannot seed sandbox profile: " + reason);
        tenriff::app::CommandLineOptions options;
        options.profile = "menu-smoke";
        options.songs_path = "songs";
        {
            auto app = std::make_unique<tenriff::app::MenuApp>();
            const bool initialized = app->initialize(options);
            report.check("menu.initialize", initialized);
            try {
                if (initialized) {
                    Access::attach_present_probe(*app);
                    menu_flow(*app, report);
                }
            } catch (const std::exception& exception) {
                report.check("menu.flow_exception", false, exception.what());
            }
            app->shutdown();
            report.check("menu.exit_code", app->exit_code() == 0);
        }
        options.chart_path = "songs/fixture/smoke.bms";
        separate_pause_flow(options, report);
        report.check("sandbox.no_account_or_sites_connection",
            !std::filesystem::exists("config/sites-leaderboard.json") &&
            !std::filesystem::exists("config/sites-leaderboard.dpapi") &&
            !std::filesystem::exists("profiles/menu-smoke/ranked-account.dpapi"),
            "Fresh sandbox; account and online routes are never invoked.");
    } catch (const std::exception& exception) {
        report.check("exception", false, exception.what());
    }
    const JsonObject result{
        {"passed", JsonValue(report.failed == 0)}, {"failed_checks", JsonValue(double(report.failed))},
        {"elapsed_seconds", JsonValue(std::chrono::duration<double>(Clock::now() - began).count())},
        {"scope", JsonValue("Actual MenuApp initialize/index/controller/launch/GameSession/result/return with real window, renderer and muted shared audio; owner-thread internal input dispatch, not MenuApp::run main-loop or physical OS input.")},
        {"pause_scope", JsonValue("Separate real GameSession, serialized internal Escape and public pause actions; its physical input producer is stopped. No claim of full MenuApp pause-overlay integration.")},
        {"present_observer", JsonValue(JsonObject{
            {"method", JsonValue("Smoke-only render callback wraps unchanged MenuApp::render_tick and accumulates its successful Present result before the next render clears it. One fixture window restart installs the callback before transition checks; FPS, VSync and overlay settings are unchanged by the observer.")},
            {"successful_presents", JsonValue(double(present_probe.successful_presents.load(std::memory_order_acquire)))},
            {"callbacks_without_present", JsonValue(double(present_probe.callbacks_without_present.load(std::memory_order_acquire)))},
            {"raw_zero_reads_after_progress", JsonValue(double(present_probe.raw_zero_reads_after_progress.load(std::memory_order_acquire)))}})},
        {"limits", JsonValue("Synthetic library only. Presents mean liveness, not screenshot equality. No physical latency, device input, online, account, multiplayer, native file dialog, long-session or multi-PC coverage. External runner should impose a 90-second process timeout for driver hangs.")},
        {"checks", JsonValue(std::move(report.checks))}};
    std::ofstream output(sandbox / "smoke-report.json", std::ios::binary);
    output << tenriff::config::json_stringify(JsonValue(result), 2) << '\n';
    output.flush();
    const bool written = output.good();
    std::filesystem::current_path(original_cwd, error);
    return written ? (report.failed == 0 ? 0 : 4) : 3;
}
