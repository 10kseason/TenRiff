#include "app/MenuApp.h"

#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSkinUtils.h"

namespace tenriff::app {

void MenuApp::handle_audio_settings_input(uint32_t keycode) {
    const int item_count = 6;
    if (keycode == key_up_) {
        settings_cursor_ = clamp_int(settings_cursor_ - 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        settings_cursor_ = clamp_int(settings_cursor_ + 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 0 && (keycode == key_left_ || keycode == key_right_)) {
        config_.audio_ui.preset = (config_.audio_ui.preset == "basic") ? "high" : "basic";
        apply_audio_preset(config_);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 1 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.audio_ui.bms_keysound_policy =
            cycle_bms_keysound_policy(config_.audio_ui.bms_keysound_policy, direction);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 2 && (keycode == key_left_ || keycode == key_right_)) {
        const double direction = (keycode == key_left_) ? -1.0 : 1.0;
        config_.audio_ui.master_volume = clamp_step_value(config_.audio_ui.master_volume + direction * kVolumeStep,
                                                          kVolumeMin, kVolumeMax, kVolumeStep);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 3 && (keycode == key_left_ || keycode == key_right_)) {
        const double direction = (keycode == key_left_) ? -1.0 : 1.0;
        config_.audio_ui.bgm_volume = clamp_step_value(config_.audio_ui.bgm_volume + direction * kChartMixVolumeStep,
                                                       kChartMixVolumeMin, kChartMixVolumeMax, kChartMixVolumeStep);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 4 && (keycode == key_left_ || keycode == key_right_)) {
        const double direction = (keycode == key_left_) ? -1.0 : 1.0;
        config_.audio_ui.keysound_volume =
            clamp_step_value(config_.audio_ui.keysound_volume + direction * kChartMixVolumeStep,
                             kChartMixVolumeMin, kChartMixVolumeMax, kChartMixVolumeStep);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        if (audio_dirty_) {
            persist_runtime_config();
            restart_audio_thread();
            audio_dirty_ = false;
        }
        publish_snapshot();
    }
}

void MenuApp::handle_mode_settings_input(uint32_t keycode) {
    const int item_count = 11;
    if (keycode == key_up_) {
        settings_cursor_ = clamp_int(settings_cursor_ - 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        settings_cursor_ = clamp_int(settings_cursor_ + 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }

    if (keycode == key_left_ || keycode == key_right_) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        if (settings_cursor_ == 0) {
            config_.mode.enable_osu_charts = !config_.mode.enable_osu_charts;
            if (config_.mode.enable_osu_charts) {
                if (to_lower_ascii(config_.mode.format) == "bms") {
                    config_.mode.format = "auto";
                }
            } else {
                config_.mode.format = "bms";
            }
            mode_dirty_ = true;
            mode_library_dirty_ = true;
            rebuild_visible_song_list();
        } else if (settings_cursor_ == 1) {
            config_.mode.song_index_profile = cycle_song_index_profile(config_.mode.song_index_profile, direction);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 2) {
            if (!config_.mode.enable_osu_charts) {
                config_.mode.format = "bms";
            } else {
                config_.mode.format = cycle_chart_filter(config_.mode.format, direction);
            }
            mode_dirty_ = true;
            rebuild_visible_song_list();
        } else if (settings_cursor_ == 3) {
            config_.mode.key_mode = cycle_runtime_key_mode(config_.mode.key_mode, direction, true);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 4) {
            if (config_.mode.gauge == "normal") {
                config_.mode.gauge = (direction > 0) ? "hard" : "easy";
            } else if (config_.mode.gauge == "hard") {
                config_.mode.gauge = (direction > 0) ? "easy" : "normal";
            } else {
                config_.mode.gauge = (direction > 0) ? "normal" : "hard";
            }
            mode_dirty_ = true;
        } else if (settings_cursor_ == 5) {
            if (config_.mode.random == "off") {
                config_.mode.random = (direction > 0) ? "fr" : "sr";
            } else if (config_.mode.random == "fr") {
                config_.mode.random = (direction > 0) ? "sr" : "off";
            } else {
                config_.mode.random = (direction > 0) ? "off" : "fr";
            }
            mode_dirty_ = true;
        } else if (settings_cursor_ == 6) {
            int next_value = static_cast<int>(config_.mode.random_seed) + direction;
            next_value = clamp_int(next_value, kSeedMin, kSeedMax);
            config_.mode.random_seed = static_cast<uint32_t>(next_value);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 7) {
            publish_snapshot();
            return;
        } else if (settings_cursor_ == 8) {
            config_.speed.rate = clamp_step_value(config_.speed.rate + static_cast<double>(direction) * kRateStep,
                                                  kRateMin, kRateMax, kRateStep);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 9) {
            config_.speed.hi_speed = clamp_step_value(
                config_.speed.hi_speed + static_cast<double>(direction) * kHiSpeedStep,
                kHiSpeedMin, kHiSpeedMax, kHiSpeedStep);
            mode_dirty_ = true;
        }
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        if (keycode == key_enter_ && settings_cursor_ == 7) {
            screen_ = Screen::ModeMods;
            settings_cursor_ = 0;
            publish_snapshot();
            return;
        }
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        if (mode_dirty_) {
            persist_runtime_config();
            if (mode_library_dirty_) {
                refresh_song_source(true);
            }
            mode_dirty_ = false;
            mode_library_dirty_ = false;
        }
        publish_snapshot();
    }
}

void MenuApp::handle_mode_mods_input(uint32_t keycode) {
    const auto& categories = mode_mod_categories();
    const int item_count = static_cast<int>(categories.size()) + 1;
    if (keycode == key_up_) {
        settings_cursor_ = clamp_int(settings_cursor_ - 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        settings_cursor_ = clamp_int(settings_cursor_ + 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }

    if ((keycode == key_left_ || keycode == key_right_) && settings_cursor_ < static_cast<int>(categories.size())) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.mode.mods = cycle_mode_mod_category(config_.mode.mods, categories[static_cast<std::size_t>(settings_cursor_)], direction);
        mode_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = Screen::ModeSelect;
        settings_cursor_ = 7;
        publish_snapshot();
    }
}

void MenuApp::populate_audio_settings_render_data(render::MenuRenderData& render) {
    append_menu_row(render.generic, "Preset", preset_label(config_.audio_ui.preset), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, "Keysound Mode", keysound_policy_label(config_.audio_ui.bms_keysound_policy), settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow, 1, false, true);
    append_menu_row(render.generic, "Master Volume", format_percent(config_.audio_ui.master_volume), settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow, 2, false, true);
    append_menu_row(render.generic, "BGM Volume", format_percent(config_.audio_ui.bgm_volume), settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow, 3, false, true);
    append_menu_row(render.generic, "Keysound Volume", format_percent(config_.audio_ui.keysound_volume), settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow, 4, false, true);
    append_menu_row(render.generic, "Back", "", settings_cursor_ == 5, render::MenuHitTargetKind::SettingsRow, 5, true, false);
    render.generic.notes.push_back("Follow: note hits trigger keysounds. Autoplay: note keysounds are mixed into background audio.");
    render.generic.notes.push_back("Off: skip note keysounds. Autoplay mode routes note keysounds through BGM volume.");
    render.generic.notes.push_back("Left/Right or click +/- to change. Back saves and returns.");
}

void MenuApp::populate_mode_settings_render_data(render::MenuRenderData& render) {
    append_menu_row(render.generic, "OSU Charts", on_off(config_.mode.enable_osu_charts), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, "Indexing",
                    song_index_profile_label(config_.mode.song_index_profile),
                    settings_cursor_ == 1, render::MenuHitTargetKind::SettingsRow, 1, false, true);
    append_menu_row(render.generic, "Chart Filter",
                    config_.mode.enable_osu_charts ? format_label(config_.mode.format) : std::string("BMS"),
                    settings_cursor_ == 2, render::MenuHitTargetKind::SettingsRow, 2, false, true);
    append_menu_row(render.generic, "Key Mode",
                    key_mode_label(config_.mode.key_mode),
                    settings_cursor_ == 3, render::MenuHitTargetKind::SettingsRow, 3, false, true);
    append_menu_row(render.generic, "Gauge", gauge_label(config_.mode.gauge), settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow, 4, false, true);
    append_menu_row(render.generic, "Random", random_label(config_.mode.random), settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow, 5, false, true);
    append_menu_row(render.generic, "Random Seed", std::to_string(config_.mode.random_seed), settings_cursor_ == 6,
                    render::MenuHitTargetKind::SettingsRow, 6, false, true);
    append_menu_row(render.generic, "Mods", mode_score_summary(config_.mode.mods, config_.speed.rate),
                    settings_cursor_ == 7, render::MenuHitTargetKind::SettingsRow, 7, true, false);
    append_menu_row(render.generic, "Rate", format_multiplier(config_.speed.rate), settings_cursor_ == 8,
                    render::MenuHitTargetKind::SettingsRow, 8, false, true);
    append_menu_row(render.generic, "Hi-Speed", format_decimal(config_.speed.hi_speed), settings_cursor_ == 9,
                    render::MenuHitTargetKind::SettingsRow, 9, false, true);
    append_menu_row(render.generic, "Back", "", settings_cursor_ == 10, render::MenuHitTargetKind::SettingsRow, 10, true, false);
    render.generic.notes.push_back("OSU Charts adds 4K-10K .osu beatmaps to song indexing and runtime loading.");
    render.generic.notes.push_back("Indexing Safe keeps RAM low for large scans; Fast uses more RAM for quicker rescans on 32GB+ PCs.");
    render.generic.notes.push_back("Chart Filter switches the visible library between BMS, OSU, or All.");
    render.generic.notes.push_back("Key Mode selects None/native plus 4K-10K/16K runtime layouts; osu charts still top out at 10K.");
    render.generic.notes.push_back("None keeps the chart's original key count and pattern layout instead of forcing a conversion.");
    render.generic.notes.push_back("Mods opens the registry-backed Mod Manager and shows the current score multiplier.");
    render.generic.notes.push_back("Back saves the toggle/filter and refreshes the song library cache when needed.");
}

void MenuApp::populate_mode_mods_render_data(render::MenuRenderData& render) {
    const auto& categories = mode_mod_categories();
    for (std::size_t i = 0; i < categories.size(); ++i) {
        append_menu_row(render.generic,
                        std::string(categories[i].label),
                        mode_mod_category_value(categories[i].token, config_.mode.mods),
                        settings_cursor_ == static_cast<int>(i),
                        render::MenuHitTargetKind::SettingsRow,
                        static_cast<int>(i),
                        false,
                        true);
    }
    append_menu_row(render.generic,
                    "Back",
                    "",
                    settings_cursor_ == static_cast<int>(categories.size()),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(categories.size()),
                    true,
                    false);
    render.generic.notes.push_back("Final score uses the lowest multiplier between active mods and the current Rate.");
    render.generic.notes.push_back("Current: " + mode_score_summary(config_.mode.mods, config_.speed.rate));
    std::vector<std::string> mod_warnings;
    (void)normalize_mode_mod_tokens(config_.mode.mods, &mod_warnings);
    for (const auto& warning : mod_warnings) {
        render.generic.notes.push_back(warning);
    }
}

}  // namespace tenriff::app
