#include "app/MenuApp.h"

#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSkinUtils.h"

namespace tenriff::app {

void MenuApp::handle_audio_settings_input(uint32_t keycode) {
    const int item_count = 7;
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

    if (settings_cursor_ == 2 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        config_.audio_ui.background_sound_enabled = !config_.audio_ui.background_sound_enabled;
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 3 && (keycode == key_left_ || keycode == key_right_)) {
        const double direction = (keycode == key_left_) ? -1.0 : 1.0;
        config_.audio_ui.master_volume = clamp_step_value(config_.audio_ui.master_volume + direction * kVolumeStep,
                                                          kVolumeMin, kVolumeMax, kVolumeStep);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 4 && (keycode == key_left_ || keycode == key_right_)) {
        const double direction = (keycode == key_left_) ? -1.0 : 1.0;
        config_.audio_ui.bgm_volume = clamp_step_value(config_.audio_ui.bgm_volume + direction * kChartMixVolumeStep,
                                                       kChartMixVolumeMin, kChartMixVolumeMax, kChartMixVolumeStep);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 5 && (keycode == key_left_ || keycode == key_right_)) {
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
    const int item_count = 15;
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
            config_.mode.song_index_profile = cycle_song_index_profile(config_.mode.song_index_profile, direction);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 1) {
            config_.mode.ghost_battle_enabled = !config_.mode.ghost_battle_enabled;
            mode_dirty_ = true;
        } else if (settings_cursor_ == 2) {
            config_.mode.autoplay_enabled = !config_.mode.autoplay_enabled;
            mode_dirty_ = true;
        } else if (settings_cursor_ == 3) {
            config_.mode.practice_no_fail_enabled = !config_.mode.practice_no_fail_enabled;
            if (config_.mode.practice_no_fail_enabled) {
                config_.mode.one_miss_fail_enabled = false;
            }
            mode_dirty_ = true;
        } else if (settings_cursor_ == 4) {
            config_.mode.one_miss_fail_enabled = !config_.mode.one_miss_fail_enabled;
            if (config_.mode.one_miss_fail_enabled) {
                config_.mode.practice_no_fail_enabled = false;
            }
            mode_dirty_ = true;
        } else if (settings_cursor_ == 5) {
            config_.mode.key_mode = cycle_runtime_key_mode(config_.mode.key_mode, direction, true);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 6) {
            config_.mode.key_conversion_algorithm =
                cycle_key_conversion_algorithm(config_.mode.key_conversion_algorithm);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 7) {
            config_.mode.gauge = cycle_gauge_mode(config_.mode.gauge, direction);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 8) {
            if (config_.mode.random == "off") {
                config_.mode.random = (direction > 0) ? "mirror" : "sr";
            } else if (config_.mode.random == "mirror") {
                config_.mode.random = (direction > 0) ? "rr" : "off";
            } else if (config_.mode.random == "rr") {
                config_.mode.random = (direction > 0) ? "fr" : "mirror";
            } else if (config_.mode.random == "fr") {
                config_.mode.random = (direction > 0) ? "sr" : "rr";
            } else {
                config_.mode.random = (direction > 0) ? "off" : "fr";
            }
            mode_dirty_ = true;
        } else if (settings_cursor_ == 9) {
            int next_value = static_cast<int>(config_.mode.random_seed) + direction;
            next_value = clamp_int(next_value, kSeedMin, kSeedMax);
            config_.mode.random_seed = static_cast<uint32_t>(next_value);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 10) {
            publish_snapshot();
            return;
        } else if (settings_cursor_ == 11) {
            config_.speed.rate = clamp_step_value(config_.speed.rate + static_cast<double>(direction) * kRateStep,
                                                  kRateMin, kRateMax, kRateStep);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 12) {
            config_.speed.hi_speed = clamp_step_value(
                config_.speed.hi_speed + static_cast<double>(direction) * kHiSpeedStep,
                kHiSpeedMin, kHiSpeedMax, kHiSpeedStep);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 13) {
            config_.mode.enable_osu_charts = !config_.mode.enable_osu_charts;
            mode_dirty_ = true;
            mode_library_dirty_ = true;
        }
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        if (keycode == key_enter_ && settings_cursor_ == 10) {
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
        settings_cursor_ = 10;
        publish_snapshot();
    }
}

void MenuApp::populate_audio_settings_render_data(render::MenuRenderData& render) {
    append_menu_row(render.generic, ui_text("Preset", "프리셋"), ui_preset_label(config_.audio_ui.preset), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, ui_text("Keysound Mode", "키음 모드"), ui_keysound_policy_label(config_.audio_ui.bms_keysound_policy), settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow, 1, false, true);
    append_menu_row(render.generic, ui_text("Background Sound", "배경음"), ui_on_off(config_.audio_ui.background_sound_enabled), settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow, 2, false, true);
    append_menu_row(render.generic, ui_text("Master Volume", "마스터 볼륨"), format_percent(config_.audio_ui.master_volume), settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow, 3, false, true);
    append_menu_row(render.generic, ui_text("BGM Volume", "BGM 볼륨"), format_percent(config_.audio_ui.bgm_volume), settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow, 4, false, true);
    append_menu_row(render.generic, ui_text("Keysound Volume", "키음 볼륨"), format_percent(config_.audio_ui.keysound_volume), settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow, 5, false, true);
    append_menu_row(render.generic, ui_text("Back", "뒤로"), "", settings_cursor_ == 6, render::MenuHitTargetKind::SettingsRow, 6, true, false);
    render.generic.notes.push_back(ui_text("Follow: note hits trigger keysounds. Autoplay: note keysounds are mixed into background audio.",
                                           "연동: 노트를 칠 때 키음이 납니다. 자동재생: 노트 키음이 배경음에 섞여 재생됩니다."));
    render.generic.notes.push_back(ui_text("Background Sound controls menu BGM and chart background audio. Keysounds stay separate.",
                                           "배경음은 메뉴 BGM과 차트 배경음을 켜고 끕니다. 키음은 별도로 유지됩니다."));
    render.generic.notes.push_back(ui_text("Off: skip note keysounds. Autoplay mode routes note keysounds through BGM volume.",
                                           "끔: 노트 키음을 재생하지 않습니다. 자동재생에서는 노트 키음이 BGM 볼륨을 따릅니다."));
    render.generic.notes.push_back(ui_text("Left/Right or click +/- to change. Back saves and returns.",
                                           "좌우 키나 +/- 클릭으로 변경합니다. 뒤로 가면 저장 후 돌아갑니다."));
}

void MenuApp::populate_mode_settings_render_data(render::MenuRenderData& render) {
    append_menu_row(render.generic, ui_text("Indexing", "인덱싱"),
                    ui_song_index_profile_label(config_.mode.song_index_profile),
                    settings_cursor_ == 0, render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, ui_text("Ghost Battle", "고스트 배틀"), ui_on_off(config_.mode.ghost_battle_enabled),
                    settings_cursor_ == 1, render::MenuHitTargetKind::SettingsRow, 1, false, true);
    append_menu_row(render.generic, ui_text("Autoplay", "오토플레이"), ui_on_off(config_.mode.autoplay_enabled),
                    settings_cursor_ == 2, render::MenuHitTargetKind::SettingsRow, 2, false, true);
    append_menu_row(render.generic, ui_text("Practice (No Fail)", "연습 모드 (실패 없음)"),
                    ui_on_off(config_.mode.practice_no_fail_enabled),
                    settings_cursor_ == 3, render::MenuHitTargetKind::SettingsRow, 3, false, true);
    append_menu_row(render.generic, ui_text("Sudden Death (1 MISS)", "서든 데스 (1미스 즉사)"),
                    ui_on_off(config_.mode.one_miss_fail_enabled),
                    settings_cursor_ == 4, render::MenuHitTargetKind::SettingsRow, 4, false, true);
    append_menu_row(render.generic, ui_text("Key Mode", "키 모드"),
                    ui_key_mode_label(config_.mode.key_mode),
                    settings_cursor_ == 5, render::MenuHitTargetKind::SettingsRow, 5, false, true);
    append_menu_row(render.generic, ui_text("Key Converter", "키 컨버터"),
                    ui_key_conversion_algorithm_label(config_.mode.key_conversion_algorithm),
                    settings_cursor_ == 6, render::MenuHitTargetKind::SettingsRow, 6, false, true);
    append_menu_row(render.generic, ui_text("Gauge", "게이지"), ui_gauge_label(config_.mode.gauge), settings_cursor_ == 7,
                    render::MenuHitTargetKind::SettingsRow, 7, false, true);
    append_menu_row(render.generic, ui_text("Random", "랜덤"), ui_random_label(config_.mode.random), settings_cursor_ == 8,
                    render::MenuHitTargetKind::SettingsRow, 8, false, true);
    append_menu_row(render.generic, ui_text("Random Seed", "랜덤 시드"), std::to_string(config_.mode.random_seed), settings_cursor_ == 9,
                    render::MenuHitTargetKind::SettingsRow, 9, false, true);
    append_menu_row(render.generic, ui_text("Mods", "모드"), mode_score_summary(config_.mode.mods, config_.speed.rate),
                    settings_cursor_ == 10, render::MenuHitTargetKind::SettingsRow, 10, true, false);
    append_menu_row(render.generic, "Rate", format_multiplier(config_.speed.rate), settings_cursor_ == 11,
                    render::MenuHitTargetKind::SettingsRow, 11, false, true);
    append_menu_row(render.generic, ui_text("Hi-Speed", "하이스피드"), format_decimal(config_.speed.hi_speed), settings_cursor_ == 12,
                    render::MenuHitTargetKind::SettingsRow, 12, false, true);
    append_menu_row(render.generic, ui_text("OSU Charts", "OSU 차트"), ui_on_off(config_.mode.enable_osu_charts), settings_cursor_ == 13,
                    render::MenuHitTargetKind::SettingsRow, 13, false, true);
    append_menu_row(render.generic, ui_text("Back", "뒤로"), "", settings_cursor_ == 14, render::MenuHitTargetKind::SettingsRow, 14, true, false);
    render.generic.notes.push_back(ui_text("Indexing Safe keeps RAM low for large scans; Fast uses more RAM for quicker rescans on 32GB+ PCs.",
                                           "인덱싱 안전은 대형 스캔에서 RAM 사용을 낮추고, 빠름은 32GB+ 환경에서 더 많은 RAM으로 재스캔을 가속합니다."));
    render.generic.notes.push_back(ui_text("Ghost Battle automatically loads the selected chart's best compatible replay into the split ghost comparison view.",
                                           "고스트 배틀은 선택한 차트의 호환되는 최고 리플레이를 자동으로 불러와 분할 비교 화면에 띄웁니다."));
    render.generic.notes.push_back(ui_text("Autoplay injects perfect note hits through the normal gameplay path and marks the result as ASSIST.",
                                           "오토플레이는 일반 플레이 경로로 완벽 판정을 주입하고 결과를 ASSIST로 표시합니다."));
    render.generic.notes.push_back(ui_text("Practice (No Fail) prevents early game over but still keeps judgement, gauge, replay, and result export active until the end.",
                                           "연습 모드(실패 없음)는 중간 게임오버를 막지만 판정, 게이지, 리플레이, 결과 저장은 끝까지 유지합니다."));
    render.generic.notes.push_back(ui_text("Sudden Death ends the run on the first osu!mania OD8 MISS. Native BAD timing alone and empty-key POOR judgements do not trigger it, and enabling it disables Practice No-Fail.",
                                           "서든 데스는 첫 osu!mania OD8 MISS에서 즉시 종료합니다. 네이티브 BAD만으로는 즉사하지 않고 빈 키 POOR도 세지 않으며, 켜면 연습 모드가 꺼집니다."));
    render.generic.notes.push_back(ui_text("Key Mode selects None/native plus 4K-10K, 12K, 14K, or 16K BMS layouts.",
                                           "키 모드는 BMS 차트의 원본 또는 4K~10K, 12K, 14K, 16K 레이아웃을 고릅니다."));
    render.generic.notes.push_back(ui_text("None keeps the chart's original key count and pattern layout instead of forcing a conversion.",
                                           "원본은 강제 변환 없이 차트의 원래 키 수와 패턴 배치를 유지합니다."));
    render.generic.notes.push_back(ui_text("Key Converter selects the legacy Krrcream path or the embedded deterministic KeyWeaver nK2 engine when Key Mode changes the lane count.",
                                           "키 컨버터는 키 모드가 레인 수를 바꿀 때 기존 Krrcream 또는 내장된 결정론적 KeyWeaver nK2 엔진을 선택합니다."));
    render.generic.notes.push_back(ui_text(
        "Mirror itself is seedless. Key Mode conversion runs first and may still use Random Seed.",
        "미러 자체는 시드를 쓰지 않지만, 먼저 실행되는 키 모드 변환은 랜덤 시드를 사용할 수 있습니다."));
    render.generic.notes.push_back(ui_text("Mods opens the registry-backed Mod Manager and shows the current score multiplier.",
                                           "모드는 현재 점수 배율을 보여주고, 등록 기반 Mod Manager를 엽니다."));
    render.generic.notes.push_back(ui_text("OSU Charts adds 4K-10K .osu beatmaps to indexing and runtime loading after Back refreshes the library.",
                                           "OSU 차트를 켜면 뒤로 나갈 때 라이브러리를 갱신하고 4K~10K .osu 비트맵을 인덱싱·실행합니다."));
    render.generic.notes.push_back(ui_text("Back saves the current mode settings.",
                                           "뒤로 가면 현재 모드 설정을 저장합니다."));
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
                    ui_text("Back", "뒤로"),
                    "",
                    settings_cursor_ == static_cast<int>(categories.size()),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(categories.size()),
                    true,
                    false);
    render.generic.notes.push_back(ui_text("Final score uses the lowest multiplier between active mods and the current Rate.",
                                           "최종 점수는 활성 모드와 현재 Rate 중 더 낮은 배율을 사용합니다."));
    render.generic.notes.push_back(ui_text(
        "LN Mix lengths use 60% long (1/8), 20% medium (1/16), and 20% short (alternating 1/24-1/32) notes. Random Seed keeps the result deterministic.",
        "LN Mix 길이는 긴 8비트 60%, 중간 16비트 20%, 짧은 24~32비트 20%로 배분합니다. Random Seed로 같은 결과를 재현합니다."));
    render.generic.notes.push_back(ui_text("Current: ", "현재: ") + mode_score_summary(config_.mode.mods, config_.speed.rate));
    std::vector<std::string> mod_warnings;
    (void)normalize_mode_mod_tokens(config_.mode.mods, &mod_warnings);
    for (const auto& warning : mod_warnings) {
        render.generic.notes.push_back(warning);
    }
}

}  // namespace tenriff::app
