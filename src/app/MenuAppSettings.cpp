#include "app/MenuApp.h"

#include <utility>

#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSkinUtils.h"
#include "app/SessionRandomSeed.h"
#include "app/menu/settings/AudioSettingsView.h"

namespace tenriff::app {

void MenuApp::handle_audio_settings_input(uint32_t keycode) {
    menu::MenuEffectFlags effects;
    if (keycode == key_up_) {
        effects = audio_settings_controller_.handle(menu::MenuAction::move(-1), config_);
    } else if (keycode == key_down_) {
        effects = audio_settings_controller_.handle(menu::MenuAction::move(1), config_);
    } else if (keycode == key_left_) {
        effects = audio_settings_controller_.handle(menu::MenuAction::adjust(-1), config_);
    } else if (keycode == key_right_) {
        effects = audio_settings_controller_.handle(menu::MenuAction::adjust(1), config_);
    } else if (keycode == key_enter_) {
        effects = audio_settings_controller_.handle(menu::MenuAction::activate(), config_);
    } else if (keycode == key_escape_ || keycode == key_backspace_) {
        effects = audio_settings_controller_.handle(menu::MenuAction::back(), config_);
    }
    apply_audio_settings_effects(effects);
}

void MenuApp::apply_audio_settings_effects(const menu::MenuEffectFlags& effects) {
    if (effects.empty()) {
        return;
    }
    if (effects.persist_config) {
        persist_runtime_config();
    }
    if (effects.restart_audio) {
        restart_audio_thread();
    }
    if (effects.navigate_back && !pop_screen()) {
        reset_screen(Screen::OptionsHub);
    }
    settings_cursor_ = static_cast<int>(audio_settings_controller_.selected_id());
    publish_snapshot();
}

void MenuApp::handle_mode_settings_input(uint32_t keycode) {
    menu::settings::ModeSettingsEffects effects;
    if (keycode == key_up_) {
        effects = mode_settings_controller_.handle(menu::MenuAction::move(-1), config_);
    } else if (keycode == key_down_) {
        effects = mode_settings_controller_.handle(menu::MenuAction::move(1), config_);
    } else if (keycode == key_left_) {
        effects = mode_settings_controller_.handle(menu::MenuAction::adjust(-1), config_);
    } else if (keycode == key_right_) {
        effects = mode_settings_controller_.handle(menu::MenuAction::adjust(1), config_);
    } else if (keycode == key_enter_) {
        effects = mode_settings_controller_.handle(menu::MenuAction::activate(), config_);
    } else if (keycode == key_escape_ || keycode == key_backspace_) {
        effects = mode_settings_controller_.handle(menu::MenuAction::back(), config_);
    }
    apply_mode_settings_effects(effects);
}

void MenuApp::handle_mode_mods_input(uint32_t keycode) {
    menu::settings::ModeSettingsEffects effects;
    if (keycode == key_up_) {
        effects = mode_settings_controller_.handle_mod_manager(
            menu::MenuAction::move(-1), config_);
    } else if (keycode == key_down_) {
        effects = mode_settings_controller_.handle_mod_manager(
            menu::MenuAction::move(1), config_);
    } else if (keycode == key_left_) {
        effects = mode_settings_controller_.handle_mod_manager(
            menu::MenuAction::adjust(-1), config_);
    } else if (keycode == key_right_) {
        effects = mode_settings_controller_.handle_mod_manager(
            menu::MenuAction::adjust(1), config_);
    } else if (keycode == key_enter_) {
        effects = mode_settings_controller_.handle_mod_manager(
            menu::MenuAction::activate(), config_);
    } else if (keycode == key_escape_ || keycode == key_backspace_) {
        effects = mode_settings_controller_.handle_mod_manager(
            menu::MenuAction::back(), config_);
    }
    apply_mode_settings_effects(effects);
}

void MenuApp::apply_mode_settings_effects(
    const menu::settings::ModeSettingsEffects& effects) {
    if (effects.empty()) {
        return;
    }
    if (effects.show_mod_manager) {
        push_screen(Screen::ModeMods);
    }
    if (effects.menu.persist_config) {
        persist_runtime_config();
    }
    if (effects.refresh_song_library) {
        refresh_song_source(true);
    }
    if (effects.menu.navigate_back && !pop_screen()) {
        reset_screen(
            current_screen() == Screen::ModeMods
                ? Screen::ModeSelect
                : Screen::OptionsHub);
    }
    settings_cursor_ = current_screen() == Screen::ModeMods
        ? static_cast<int>(mode_settings_controller_.selected_mod_category())
        : static_cast<int>(mode_settings_controller_.selected_id());
    publish_snapshot();
}

void MenuApp::populate_audio_settings_render_data(render::MenuRenderData& render) {
    auto view = menu::settings::AudioSettingsView::build(
        audio_settings_controller_, config_, ui_uses_korean());
    render.generic.rows.reserve(render.generic.rows.size() + view.rows.size());
    for (auto& source : view.rows) {
        render::MenuRowData row;
        row.label = std::move(source.label);
        row.value = std::move(source.value);
        row.selected = source.selected;
        row.activatable = source.activatable;
        row.adjustable = source.adjustable;
        row.increment_enabled = source.adjustable;
        row.decrement_enabled = source.adjustable;
        row.slider = source.slider_ratio.has_value();
        row.slider_ratio = source.slider_ratio.value_or(0.0);
        row.target_kind = render::MenuHitTargetKind::SettingsRow;
        row.row_index = static_cast<int>(source.id);
        render.generic.rows.push_back(std::move(row));
    }
    render.generic.notes = std::move(view.notes);
}

void MenuApp::populate_mode_settings_render_data(render::MenuRenderData& render) {
    append_menu_row(render.generic, ui_text("Indexing", "인덱싱"),
                    ui_song_index_profile_label(config_.mode.song_index_profile),
                    settings_cursor_ == 0, render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, ui_text("Index Difficulty", "인덱스 난이도"),
                    ui_on_off(config_.mode.calculate_song_index_difficulty),
                    settings_cursor_ == 1, render::MenuHitTargetKind::SettingsRow, 1, false, true);
    append_menu_row(render.generic, ui_text("Ghost Battle", "고스트 배틀"), ui_on_off(config_.mode.ghost_battle_enabled),
                    settings_cursor_ == 2, render::MenuHitTargetKind::SettingsRow, 2, false, true);
    append_menu_row(render.generic, ui_text("Autoplay", "오토플레이"), ui_on_off(config_.mode.autoplay_enabled),
                    settings_cursor_ == 3, render::MenuHitTargetKind::SettingsRow, 3, false, true);
    append_menu_row(render.generic, ui_text("Practice (No Fail)", "연습 모드 (실패 없음)"),
                    ui_on_off(config_.mode.practice_no_fail_enabled),
                    settings_cursor_ == 4, render::MenuHitTargetKind::SettingsRow, 4, false, true);
    append_menu_row(render.generic, ui_text("Sudden Death (1 MISS)", "서든 데스 (1미스 즉사)"),
                    ui_on_off(config_.mode.one_miss_fail_enabled),
                    settings_cursor_ == 5, render::MenuHitTargetKind::SettingsRow, 5, false, true);
    const std::string pacemaker_mode =
        config::normalize_pacemaker_mode_token(config_.mode.pacemaker_mode);
    const std::string pacemaker_mode_label =
        pacemaker_mode == "accuracy"
            ? ui_text("Accuracy", "정확도")
            : (pacemaker_mode == "score" ? ui_text("Score", "점수") : ui_text("Off", "끔"));
    const std::string pacemaker_target_label =
        pacemaker_mode == "accuracy"
            ? (format_decimal(config_.mode.pacemaker_target_accuracy) + "%")
            : (pacemaker_mode == "score"
                   ? std::to_string(config_.mode.pacemaker_target_score)
                   : "-");
    append_menu_row(render.generic, ui_text("Pacemaker", "페이스메이커"), pacemaker_mode_label,
                    settings_cursor_ == 6, render::MenuHitTargetKind::SettingsRow, 6, false, true);
    append_menu_row(render.generic, ui_text("Pacemaker Target", "페이스메이커 목표"), pacemaker_target_label,
                    settings_cursor_ == 7, render::MenuHitTargetKind::SettingsRow, 7, false,
                    pacemaker_mode != "off");
    append_menu_row(render.generic, ui_text("Key Mode", "키 모드"),
                    ui_key_mode_label(config_.mode.key_mode),
                    settings_cursor_ == 8, render::MenuHitTargetKind::SettingsRow, 8, false, true);
    append_menu_row(render.generic, ui_text("Key Converter", "키 컨버터"),
                    ui_key_conversion_algorithm_label(config_.mode.key_conversion_algorithm),
                    settings_cursor_ == 9, render::MenuHitTargetKind::SettingsRow, 9, false, true);
    const bool nk2_selected =
        normalize_key_conversion_algorithm(config_.mode.key_conversion_algorithm) !=
        "krrcream";
    append_menu_row(render.generic, "nK2 Preset",
                    nk2_selected
                        ? ui_key_conversion_nk2_preset_label(config_.mode.key_conversion_nk2_preset)
                        : ui_text("Locked (Krrcream)", "잠김 (Krrcream)"),
                    settings_cursor_ == 10, render::MenuHitTargetKind::SettingsRow, 10,
                    false, nk2_selected);
    append_menu_row(render.generic, ui_text("Gauge Shift Start", "게이지 시프트 시작"), ui_gauge_label(config_.mode.gauge), settings_cursor_ == 11,
                    render::MenuHitTargetKind::SettingsRow, 11, false, true);
    append_menu_row(render.generic, ui_text("Random", "랜덤"), ui_random_label(config_.mode.random), settings_cursor_ == 12,
                    render::MenuHitTargetKind::SettingsRow, 12, false, true);
    const std::string random_seed_value =
        random_mode_uses_fresh_session_seed(config_.mode.random)
            ? ui_text("Auto each play", "플레이마다 자동")
            : std::to_string(config_.mode.random_seed);
    append_menu_row(render.generic, ui_text("Random Seed", "랜덤 시드"), random_seed_value, settings_cursor_ == 13,
                    render::MenuHitTargetKind::SettingsRow, 13, false, true);
    append_menu_row(render.generic, ui_text("Mods", "모드"), mode_score_summary(config_.mode.mods, config_.speed.rate),
                    settings_cursor_ == 14, render::MenuHitTargetKind::SettingsRow, 14, true, false);
    append_menu_row(render.generic, "Rate", format_multiplier(config_.speed.rate), settings_cursor_ == 15,
                    render::MenuHitTargetKind::SettingsRow, 15, false, true);
    append_menu_row(render.generic, ui_text("Hi-Speed", "하이스피드"), format_decimal(config_.speed.hi_speed), settings_cursor_ == 16,
                    render::MenuHitTargetKind::SettingsRow, 16, false, true);
    append_menu_row(render.generic, ui_text("Back", "뒤로"), "", settings_cursor_ == 17, render::MenuHitTargetKind::SettingsRow, 17, true, false);
    render.generic.notes.push_back(ui_text("Fast (Minimal) keeps title, artist, key count, level, and BPM only; it skips hashes, previews, difficulty tables, and native LV/CR.",
                                           "빠름(최소)은 제목, 아티스트, 키 수, 레벨, BPM만 유지하고 해시, 미리보기, 난이도표, 자체 LV/CR을 건너뜁니다."));
    render.generic.notes.push_back(ui_text("Choosing a difficulty table automatically switches Fast to Safe so MD5/SHA-256 matches can be applied.",
                                           "난이도표를 선택하면 MD5/SHA-256 매칭을 위해 빠름에서 안전으로 자동 전환합니다."));
    render.generic.notes.push_back(ui_text("Index Difficulty applies to Safe scans. Fast always skips native LV/CR even when this option is On.",
                                           "인덱스 난이도는 안전 스캔에 적용되며, 빠름은 이 옵션을 켜도 자체 LV/CR을 항상 건너뜁니다."));
    render.generic.notes.push_back(ui_text("Ghost Battle automatically loads the selected chart's best compatible replay into the split ghost comparison view.",
                                           "고스트 배틀은 선택한 차트의 호환되는 최고 리플레이를 자동으로 불러와 분할 비교 화면에 띄웁니다."));
    render.generic.notes.push_back(ui_text("Autoplay injects perfect note hits through the normal gameplay path and marks the result as ASSIST.",
                                           "오토플레이는 일반 플레이 경로로 퍼펙트 판정을 주입하고 결과를 ASSIST로 표시합니다."));
    render.generic.notes.push_back(ui_text("Practice (No Fail) prevents early game over but still keeps judgement, gauge, replay, and result export active until the end.",
                                           "연습 모드(실패 없음)는 중간 게임오버를 막지만 판정, 게이지, 리플레이, 결과 저장은 끝까지 유지합니다."));
    render.generic.notes.push_back(ui_text("Sudden Death ends the run on the first osu!mania OD8 MISS. Native BAD timing alone and empty-key POOR judgements do not trigger it, and enabling it disables Practice No-Fail.",
                                           "서든 데스는 첫 osu!mania OD8 MISS에서 즉시 종료합니다. 네이티브 BAD만으로는 즉사하지 않고 빈 키 POOR도 세지 않으며, 켜면 연습 모드가 꺼집니다."));
    render.generic.notes.push_back(ui_text(
        "Pacemaker plays through gauge failure and clears only when the final Accuracy or displayed final Score reaches the selected target. It is mutually exclusive with Practice and Sudden Death.",
        "페이스메이커는 게이지 실패를 넘겨 끝까지 플레이하고, 종료 정확도 또는 화면의 최종 점수가 설정한 목표 이상일 때만 클리어합니다. 연습 모드·서든 데스와는 동시에 사용할 수 없습니다."));
    render.generic.notes.push_back(ui_text("Key Mode selects None/native plus 4K-10K, 12K, 14K, or 16K BMS layouts.",
                                           "키 모드는 BMS 차트의 원본 또는 4K~10K, 12K, 14K, 16K 레이아웃을 고릅니다."));
    render.generic.notes.push_back(ui_text("None keeps the chart's original key count and pattern layout instead of forcing a conversion.",
                                           "원본은 강제 변환 없이 차트의 원래 키 수와 패턴 배치를 유지합니다."));
    render.generic.notes.push_back(ui_text(
        "Krrcream remaps source notes only. nK2 keeps the legacy engine; NK3 always uses strict P64 plus the host beam solver, and adds the NPU/GPU/CPU MLP only for non-10K sources converted to 10K.",
        "Krrcream은 원본 노트만 재배치합니다. nK2는 기존 엔진을 유지합니다. NK3는 항상 strict P64와 호스트 빔 솔버를 사용하며, 10K가 아닌 원본을 10K로 변환할 때만 NPU/GPU/CPU MLP를 추가합니다."));
    render.generic.notes.push_back(ui_text(
        "Mirror itself is seedless. Key Mode conversion runs first and may still use Random Seed.",
        "미러 자체는 시드를 쓰지 않지만, 먼저 실행되는 키 모드 변환은 랜덤 시드를 사용할 수 있습니다."));
    render.generic.notes.push_back(ui_text("Mods opens the registry-backed Mod Manager and shows the current score multiplier.",
                                           "모드는 현재 점수 배율을 보여주고, 등록 기반 Mod Manager를 엽니다."));
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
