void MenuApp::populate_gameplay_render_data(render::GameplayHudData& target,
                                            uint64_t* out_motion_revision,
                                            uint64_t* out_text_revision) {
    std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);

    target.motion_revision = gameplay_hud_.motion_revision;
    target.text_revision = gameplay_hud_.text_revision;
    target.active = gameplay_hud_.active;
    target.loading = gameplay_hud_.loading;
    target.countdown_active = gameplay_hud_.countdown_active;
    target.countdown_value = gameplay_hud_.countdown_value;
    target.loading_percent = gameplay_hud_.loading_percent;
    target.loading_stage = gameplay_hud_.loading_stage;
    target.lane_count = clamp_int(gameplay_hud_.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
    target.current_sample = gameplay_hud_.current_sample;
    target.duration_samples = gameplay_hud_.duration_samples;
    target.sample_rate = gameplay_hud_.sample_rate;
    target.audio_sample_time_ns = gameplay_hud_.audio_sample_time_ns;
    target.audio_buffer_frames = gameplay_hud_.audio_buffer_frames;
    target.lookahead_samples = gameplay_hud_.lookahead_samples;
    target.past_samples = gameplay_hud_.past_samples;
    const double clamped_judgement_line_position = std::clamp(
        config_.skin.judgement_line_position,
        config::kJudgementLinePositionMin,
        config::kJudgementLinePositionMax);
    const double clamped_combo_position = std::clamp(
        config_.skin.combo_position,
        config::kComboPositionMin,
        config::kComboPositionMax);
    target.judgement_line_position = clamped_judgement_line_position;
    target.combo_position = clamped_combo_position;
    const std::string skin_mode = std::to_string(target.lane_count) + "k";
    const auto resolved_lane_widths = config::resolved_skin_lane_width_scales(config_.skin, skin_mode);
    target.lane_width_scale_count = std::min(resolved_lane_widths.size(), target.lane_width_scales.size());
    target.lane_width_scales.fill(config::kLaneWidthScaleDefault);
    for (std::size_t i = 0; i < target.lane_width_scale_count; ++i) {
        target.lane_width_scales[i] = resolved_lane_widths[i];
    }
    target.note_width_scale = std::clamp(
        config::resolved_skin_note_width_scale(config_.skin, skin_mode),
        config::kNoteWidthScaleMin,
        config::kNoteWidthScaleMax);
    const auto resolved_lane_spacings = config::resolved_skin_lane_spacing_scales(config_.skin, skin_mode);
    target.lane_spacing_scale_count = std::min(resolved_lane_spacings.size(), target.lane_spacing_scales.size());
    target.lane_spacing_scales.fill(config::kLaneSpacingScaleDefault);
    for (std::size_t i = 0; i < target.lane_spacing_scale_count; ++i) {
        target.lane_spacing_scales[i] = resolved_lane_spacings[i];
    }
    target.note_height_scale = std::clamp(
        config::resolved_skin_note_height_scale(config_.skin, skin_mode),
        config::kNoteHeightScaleMin,
        config::kNoteHeightScaleMax);
    target.lane_divider_width_scale = std::clamp(
        config::resolved_skin_lane_divider_width_scale(config_.skin, skin_mode),
        config::kLaneDividerWidthScaleMin,
        config::kLaneDividerWidthScaleMax);
    target.lane_center_gap_scale = std::clamp(
        config::resolved_skin_lane_center_gap_scale(config_.skin, skin_mode),
        config::kLaneCenterGapScaleMin,
        config::kLaneCenterGapScaleMax);
    target.hold_body_width_scale = std::clamp(
        config_.skin.hold_body_width_scale,
        config::kHoldBodyWidthScaleMin,
        config::kHoldBodyWidthScaleMax);
    target.show_lane_dividers = config_.skin.show_lane_dividers;
    target.show_judgement_line = config_.skin.show_judgement_line;
    target.show_gear_boundary_line = config_.skin.show_gear_boundary_line;
    target.hold_tail_taper_enabled = config_.skin.hold_tail_taper_enabled;
    target.note_border_enabled = config_.skin.note_border_enabled;
    target.note_shape = config::normalize_skin_note_shape_token(config_.skin.note_shape);
    target.preserve_note_image_aspect_ratio = config_.skin.preserve_note_image_aspect_ratio;
    target.skin_source = config::normalize_skin_source_token(config_.skin.source);
    target.external_skin_root = active_external_skin_root();
    target.external_skin_name = active_external_skin_name();
    target.lr2_resolution_override =
        config::normalize_skin_lr2_resolution_mode_token(config_.skin.lr2_resolution_mode);
    target.visual_offset_ms = std::clamp(config_.visual_offset_ms, kVisualOffsetMin, kVisualOffsetMax);
    target.rate = gameplay_hud_.rate;
    target.hispeed = gameplay_hud_.hispeed;
    target.combo = gameplay_hud_.combo;
    target.max_combo = gameplay_hud_.max_combo;
    target.pg = gameplay_hud_.counts.pg;
    target.gr = gameplay_hud_.counts.gr;
    target.gd = gameplay_hud_.counts.gd;
    target.bd = gameplay_hud_.counts.bd;
    target.pr = gameplay_hud_.counts.pr;
    target.total_notes = gameplay_hud_.counts.pg + gameplay_hud_.counts.gr +
                         gameplay_hud_.counts.gd + gameplay_hud_.counts.bd;
    target.gauge = gameplay_hud_.gauge;
    target.gauge_label = gauge_type_label(gameplay_hud_.gauge_type);
    target.has_feedback = gameplay_hud_.has_feedback;
    target.feedback = judgement_label(gameplay_hud_.feedback);
    target.feedback_delta_ms = gameplay_hud_.feedback_delta_ms;
    target.finished = gameplay_hud_.finished;
    target.game_over = gameplay_hud_.game_over;

    target.lane_activity_count = gameplay_hud_.lane_activity_count;
    target.lane_activity.fill(0.0f);
    std::copy_n(gameplay_hud_.lane_activity.begin(), gameplay_hud_.lane_activity_count, target.lane_activity.begin());

    target.lane_color_count = 0;
    target.lane_colors.fill(0);
    const auto lane_colors = config::resolved_skin_lane_colors(config_.skin, skin_mode);
    target.lane_color_count = std::min<std::size_t>(lane_colors.size(), static_cast<std::size_t>(target.lane_count));
    for (std::size_t i = 0; i < target.lane_color_count; ++i) {
        target.lane_colors[i] = config::skin_color_rgb(lane_colors[i]);
    }

    target.note_count = gameplay_hud_.note_count;
    for (std::size_t i = 0; i < gameplay_hud_.note_count; ++i) {
        const auto& note = gameplay_hud_.notes[i];
        render::GameplayNoteData out_note;
        out_note.lane = note.lane;
        out_note.start_sample = note.start_sample;
        out_note.tail_sample = note.tail_sample;
        out_note.hold = note.hold;
        out_note.head_visible = note.head_visible;
        target.notes[i] = out_note;
    }
    target.score = gameplay_hud_.score;

    const int judged_total = gameplay_hud_.counts.pg + gameplay_hud_.counts.gr + gameplay_hud_.counts.gd +
                             gameplay_hud_.counts.bd;
    if (judged_total > 0) {
        const double weighted = gameplay_hud_.counts.pg * 1.0 +
                                gameplay_hud_.counts.gr * 0.80 +
                                gameplay_hud_.counts.gd * 0.50 +
                                gameplay_hud_.counts.bd * 0.20;
        target.accuracy = std::clamp(weighted / static_cast<double>(judged_total) * 100.0, 0.0, 100.0);
    } else {
        target.accuracy = 0.0;
    }

    target.ghost_visible = gameplay_hud_.ghost_visible;
    target.ghost_score = gameplay_hud_.ghost_score;
    target.ghost_combo = gameplay_hud_.ghost_combo;
    target.ghost_max_combo = gameplay_hud_.ghost_max_combo;
    target.ghost_pg = gameplay_hud_.ghost_counts.pg;
    target.ghost_gr = gameplay_hud_.ghost_counts.gr;
    target.ghost_gd = gameplay_hud_.ghost_counts.gd;
    target.ghost_bd = gameplay_hud_.ghost_counts.bd;
    target.ghost_pr = gameplay_hud_.ghost_counts.pr;
    target.ghost_gauge = gameplay_hud_.ghost_gauge;
    target.ghost_gauge_label = gauge_type_label(gameplay_hud_.ghost_gauge_type);
    target.ghost_has_feedback = gameplay_hud_.ghost_has_feedback;
    target.ghost_feedback = judgement_label(gameplay_hud_.ghost_feedback);
    target.ghost_feedback_delta_ms = gameplay_hud_.ghost_feedback_delta_ms;
    target.ghost_finished = gameplay_hud_.ghost_finished;
    target.ghost_game_over = gameplay_hud_.ghost_game_over;
    target.ghost_lane_activity_count = gameplay_hud_.ghost_lane_activity_count;
    target.ghost_lane_activity.fill(0.0f);
    std::copy_n(gameplay_hud_.ghost_lane_activity.begin(),
                gameplay_hud_.ghost_lane_activity_count,
                target.ghost_lane_activity.begin());
    target.ghost_note_count = gameplay_hud_.ghost_note_count;
    for (std::size_t i = 0; i < gameplay_hud_.ghost_note_count; ++i) {
        const auto& note = gameplay_hud_.ghost_notes[i];
        render::GameplayNoteData out_note;
        out_note.lane = note.lane;
        out_note.start_sample = note.start_sample;
        out_note.tail_sample = note.tail_sample;
        out_note.hold = note.hold;
        out_note.head_visible = note.head_visible;
        target.ghost_notes[i] = out_note;
    }
    const int ghost_judged_total = gameplay_hud_.ghost_counts.pg + gameplay_hud_.ghost_counts.gr +
                                   gameplay_hud_.ghost_counts.gd + gameplay_hud_.ghost_counts.bd;
    if (ghost_judged_total > 0) {
        const double weighted = gameplay_hud_.ghost_counts.pg * 1.0 +
                                gameplay_hud_.ghost_counts.gr * 0.80 +
                                gameplay_hud_.ghost_counts.gd * 0.50 +
                                gameplay_hud_.ghost_counts.bd * 0.20;
        target.ghost_accuracy = std::clamp(weighted / static_cast<double>(ghost_judged_total) * 100.0, 0.0, 100.0);
    } else {
        target.ghost_accuracy = 0.0;
    }

    if (out_motion_revision) {
        *out_motion_revision = gameplay_hud_.motion_revision;
    }
    if (out_text_revision) {
        *out_text_revision = gameplay_hud_.text_revision;
    }
}

void MenuApp::update_gameplay_loading_state(int percent, std::string_view stage) {
    {
        std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
        const GameplayHudRevisionInput previous = gameplay_hud_revision_input(gameplay_hud_);
        GameplayHudRevisionInput next = previous;
        next.loading = true;
        next.loading_percent = clamp_int(percent, 0, 100);
        next.loading_stage = std::string(stage);
        const GameplayHudRevisionFlags diff = diff_gameplay_hud_revisions(previous, next);

        gameplay_hud_.loading = next.loading;
        gameplay_hud_.loading_percent = next.loading_percent;
        gameplay_hud_.loading_stage = next.loading_stage;
        advance_gameplay_hud_revisions(gameplay_hud_, diff.motion_changed, false);
    }
    publish_snapshot();
}

std::string MenuApp::current_track_label() const {
    if (screen_ == Screen::SongSelect && song_select_view_ == SongSelectView::Sources &&
        !config_.ui.recent_song_sources.empty() &&
        selected_source_ >= 0 &&
        selected_source_ < static_cast<int>(config_.ui.recent_song_sources.size())) {
        return menu_songs::song_source_display_name(
            config_.ui.recent_song_sources[static_cast<std::size_t>(selected_source_)]);
    }
    if (selected_song_ >= 0 && selected_song_ < static_cast<int>(visible_song_count())) {
        if (const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(selected_song_))) {
            return song_title_for_ui(*entry);
        }
    }
    if (!last_chart_title_.empty()) {
        return safe_ui_text(last_chart_title_, "-");
    }
    return "-";
}

void MenuApp::sync_menu_music() {
    if (screen_ == Screen::Gameplay) {
        menu_music_.stop();
        return;
    }

    const bool song_select_context =
        screen_ == Screen::SongSelect ||
        screen_ == Screen::SongBrowser ||
        screen_ == Screen::Result ||
        ((screen_ == Screen::OptionsHub ||
          screen_ == Screen::SettingsAudio ||
          screen_ == Screen::SettingsGraphics ||
          screen_ == Screen::SettingsSkins ||
          screen_ == Screen::SettingsInput ||
          screen_ == Screen::ModeSelect ||
          screen_ == Screen::ModeMods ||
          screen_ == Screen::Keymap ||
          screen_ == Screen::KeymapConfirm ||
          screen_ == Screen::KeymapTest) &&
         submenu_return_screen_ == Screen::SongSelect);

    std::string music_path = resolve_menu_music_file_path(song_select_context ? "Song Selecte.mp3" : "Main Menu.mp3");
    if (music_path.empty() && song_select_context) {
        music_path = resolve_menu_music_file_path("Song Select.mp3");
    }
    if (music_path.empty()) {
        menu_music_.stop();
        return;
    }

    const double gain = std::clamp(config_.audio_ui.master_volume * config_.audio_ui.bgm_volume, 0.0, 1.0);
    menu_music_.play_looping_file(music_path, gain);
}

void MenuApp::populate_quick_setup_render_data(render::MenuRenderData& render) {
    render.kind = render::MenuScreenKind::GenericList;
    render.generic.heading = ui_text("Quick Setup", "빠른 설정");

    append_menu_row(render.generic,
                    ui_text("Songs Folder", "곡 폴더"),
                    safe_ui_text(menu_songs::song_source_display_name(songs_path_),
                                 ui_text("Choose Folder", "폴더 선택")),
                    settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow,
                    0,
                    true,
                    false);
    append_menu_row(render.generic,
                    ui_text("Gauge", "게이지"),
                    ui_gauge_label(config_.mode.gauge),
                    settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow,
                    1,
                    false,
                    true);
    append_menu_row(render.generic,
                    "Rate",
                    format_multiplier(config_.speed.rate),
                    settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow,
                    2,
                    false,
                    true);
    append_menu_row(render.generic,
                    ui_text("Display Offset", "표시 오프셋"),
                    format_signed_offset_ms(config_.visual_offset_ms),
                    settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow,
                    3,
                    false,
                    true);
    append_menu_row(render.generic,
                    ui_text("BMS Keysound", "BMS 키음"),
                    ui_keysound_policy_label(config_.audio_ui.bms_keysound_policy),
                    settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow,
                    4,
                    false,
                    true);
    append_menu_row(render.generic,
                    ui_text("Continue to Song Select", "Song Select로 계속"),
                    "",
                    settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow,
                    5,
                    true,
                    false);
    append_menu_row(render.generic,
                    ui_text("Skip to Title", "타이틀로 건너뛰기"),
                    "",
                    settings_cursor_ == 6,
                    render::MenuHitTargetKind::SettingsRow,
                    6,
                    true,
                    false);

    render.generic.notes.push_back(ui_text("First launch detected. TenRiff already created a default profile and default keymap.",
                                           "첫 실행이 감지되었습니다. TenRiff가 기본 프로필과 기본 키맵을 이미 만들었습니다."));
    render.generic.notes.push_back(ui_text("Recommended start: Gauge Normal, Rate 1.00x, Display Offset 0ms, BMS Keysound Follow.",
                                           "권장 시작값: 노말 게이지, Rate 1.00x, 표시 오프셋 0ms, BMS 키음 연동."));
    render.generic.notes.push_back(ui_text("Songs Folder opens a picker on Enter or F2. You can also drag and drop a folder later.",
                                           "곡 폴더는 Enter 또는 F2로 선택 창을 엽니다. 나중에 폴더를 드래그 앤 드롭해도 됩니다."));
    render.generic.notes.push_back(ui_text("These values stay editable later from Song Select: A=Audio  G=Graphics  I=Input  M=Mode  K=Keymap.",
                                           "이 값들은 나중에 Song Select에서도 수정할 수 있습니다: A=Audio  G=Graphics  I=Input  M=Mode  K=Keymap."));
}

void MenuApp::populate_title_render_data(render::MenuRenderData& render,
                                         const std::string& current_track,
                                         const MenuApp::BestResultRecord& current_best) {
    render.kind = render::MenuScreenKind::TitleMenu;
    render.title.profile = options_.profile;
    render.title.track = current_track;
    render.title.high_score = current_best.has_value ? current_best.best_score : 0;
    render.title.buttons = {
        render::MenuButtonData{ui_text("PLAY", "플레이"), u8"▶", title_cursor_ == 0},
        render::MenuButtonData{ui_text("EDIT", "에디트"), u8"✎", title_cursor_ == 1},
        render::MenuButtonData{ui_text("OPTIONS", "옵션"), u8"⚙", title_cursor_ == 2},
        render::MenuButtonData{ui_text("EXIT", "종료"), u8"⏻", title_cursor_ == 3},
    };
    render.title.guides = {
        ui_text("UP / DOWN or mouse to move", "위 / 아래 또는 마우스로 이동"),
        ui_text("ENTER or double-click to open", "ENTER 또는 더블클릭으로 열기"),
        ui_text("F5 refreshes the current song source", "F5로 현재 곡 소스를 새로고침"),
        ui_text("A/G/I/M/K jumps to Audio / Graphics", "A/G/I/M/K로 Audio / Graphics"),
        ui_text("Input / Mode / Keymap", "Input / Mode / Keymap 바로 이동"),
        ui_text("F1 opens the control help overlay", "F1로 조작 도움말 열기"),
        ui_text("ESC exits from the title menu", "ESC로 타이틀 메뉴 종료"),
    };
}

void MenuApp::populate_result_render_data(render::MenuRenderData& render, const std::string& current_track) {
    render.kind = render::MenuScreenKind::ResultScreen;
    render.result.profile = options_.profile;
    render.result.track = last_chart_title_.empty() ? current_track : last_chart_title_;
    render.result.title = last_chart_title_.empty() ? ui_text("Unknown Chart", "알 수 없는 차트") : last_chart_title_;
    render.result.artist = last_chart_artist_;

    if (!has_result_) {
        render.result.notes.push_back(ui_text("No result data is available for this run.", "이번 플레이의 결과 데이터가 없습니다."));
        render.result.notes.push_back(ui_text("Left restarts the same chart. Enter or Esc returns to Song Select.",
                                              "Left로 같은 차트를 재시작합니다. Enter 또는 Esc로 Song Select로 돌아갑니다."));
        return;
    }

    const int judged = menu_records::judged_total(last_result_.counts);
    const int total_notes = (last_result_.total_notes > 0) ? last_result_.total_notes : judged;
    const double accuracy = menu_records::calculate_accuracy(last_result_);

    game::GaugeType final_gauge_type = gauge_type_from_mode_string(config_.mode.gauge);
    if (!last_result_.shifts.empty()) {
        final_gauge_type = last_result_.shifts.back().to;
    }

    render.result.rank = menu_records::calculate_rank(last_result_, last_game_over_);
    render.result.status = !last_clear_status_.empty() ? last_clear_status_
                                                       : (last_game_over_ ? "GAME OVER" : "CLEAR");
    render.result.gauge_label = gauge_type_label(final_gauge_type);
    render.result.score = last_result_final_score_;
    render.result.accuracy = accuracy;
    render.result.gauge_value =
        last_result_.gauge_history.empty() ? 0.0 : last_result_.gauge_history.back().value;
    render.result.max_combo = last_result_.max_combo;
    render.result.total_notes = total_notes;
    render.result.judged_notes = judged;
    render.result.perfect = last_result_.counts.pg;
    render.result.great = last_result_.counts.gr;
    render.result.good = last_result_.counts.gd;
    render.result.bad = last_result_.counts.bd;
    render.result.poor = last_result_.counts.pr;
    render.result.mean_delta_ms = last_result_.mean_delta_ms;
    render.result.stddev_delta_ms = last_result_.stddev_delta_ms();
    render.result.shift_count = static_cast<int>(last_result_.shifts.size());
    render.result.export_warning_count = static_cast<int>(last_export_warnings_.size());
    render.result.replay_file = filename_only(last_replay_path_);
    render.result.replay_available = false;
    render.result.result_file = filename_only(last_result_path_);

    if (!last_replay_path_.empty()) {
        std::error_code ec;
        render.result.replay_available = std::filesystem::exists(path_from_utf8(last_replay_path_), ec) && !ec;
    }

    if (!render.result.replay_file.empty()) {
        render.result.notes.push_back(ui_text("Replay: ", "리플레이: ") + render.result.replay_file);
    }
    if (!render.result.result_file.empty()) {
        render.result.notes.push_back(ui_text("Result: ", "결과 파일: ") + render.result.result_file);
    }
    render.result.notes.push_back(ui_text("Left restarts the same chart immediately.", "Left로 같은 차트를 즉시 재시작합니다."));
    if (render.result.replay_available) {
        render.result.notes.push_back(ui_text("F1 or click Replay watches the saved input trace.",
                                              "F1 또는 Replay 클릭으로 저장된 입력 리플레이를 재생합니다."));
    } else if (!render.result.replay_file.empty()) {
        render.result.notes.push_back(ui_text("Replay file is missing or unavailable.", "리플레이 파일이 없거나 사용할 수 없습니다."));
    }
    render.result.notes.push_back(ui_text("Score x", "점수 x") + format_decimal(last_result_score_multiplier_));
    if (last_result_rate_multiplier_ != 1.0) {
        render.result.notes.push_back(ui_text("Rate score x", "Rate 점수 x") + format_decimal(last_result_rate_multiplier_));
    }
    render.result.notes.push_back(ui_text("Mods: ", "모드: ") + mode_mod_summary(last_result_mods_));
    render.result.notes.push_back(ui_text("Timing center ", "타이밍 중심 ") + format_signed_ms(last_result_.mean_delta_ms) +
                                  ui_text("  spread ", "  분산 ") + format_decimal(render.result.stddev_delta_ms) + "ms");
    if (last_session_replay_playback_) {
        render.result.notes.push_back(ui_text("Replay playback session: no new replay/result export was written.",
                                              "리플레이 재생 세션: 새로운 리플레이/결과 파일은 저장되지 않았습니다."));
    }
    if (!last_export_warnings_.empty()) {
        render.result.notes.push_back(ui_text("Export warnings: ", "내보내기 경고: ") + std::to_string(last_export_warnings_.size()));
        const std::size_t preview_count = std::min<std::size_t>(2, last_export_warnings_.size());
        for (std::size_t i = 0; i < preview_count; ++i) {
            render.result.notes.push_back(last_export_warnings_[i]);
        }
    }

    int64_t graph_end_sample = 1;
    for (const auto& sample : last_result_.gauge_history) {
        graph_end_sample = std::max(graph_end_sample, sample.sample);
    }
    for (const auto& shift : last_result_.shifts) {
        graph_end_sample = std::max(graph_end_sample, shift.sample);
    }

    const std::size_t gauge_count = last_result_.gauge_history.size();
    std::size_t gauge_stride = 1;
    if (gauge_count > 240) {
        gauge_stride = (gauge_count + 239) / 240;
    }

    for (std::size_t i = 0; i < gauge_count; i += gauge_stride) {
        const auto& sample = last_result_.gauge_history[i];
        const float position = (gauge_count == 1)
                                   ? 1.0f
                                   : static_cast<float>(
                                         std::clamp(static_cast<double>(sample.sample) /
                                                        static_cast<double>(graph_end_sample),
                                                    0.0, 1.0));
        const float value = static_cast<float>(std::clamp(sample.value / 100.0, 0.0, 1.0));
        render.result.gauge_points.push_back(render::ResultGaugePoint{position, value});
    }
    if (gauge_count > 1) {
        const auto& last_sample = last_result_.gauge_history.back();
        const float last_position = static_cast<float>(
            std::clamp(static_cast<double>(last_sample.sample) / static_cast<double>(graph_end_sample),
                       0.0, 1.0));
        const float last_value = static_cast<float>(std::clamp(last_sample.value / 100.0, 0.0, 1.0));
        if (render.result.gauge_points.empty() ||
            render.result.gauge_points.back().position != last_position) {
            render.result.gauge_points.push_back(render::ResultGaugePoint{last_position, last_value});
        }
    }

            for (const auto& shift : last_result_.shifts) {
                const double shift_ratio = static_cast<double>(shift.sample) / static_cast<double>(graph_end_sample);
                const double clamped_shift_ratio =
                    shift_ratio < 0.0 ? 0.0 : (shift_ratio > 1.0 ? 1.0 : shift_ratio);
                const float position = static_cast<float>(clamped_shift_ratio);
                render.result.gauge_shifts.push_back(
                    render::ResultShiftMarker{position,
                                              short_gauge_type_label(shift.from) + "->" +
                                          short_gauge_type_label(shift.to)});
    }
    std::stable_sort(render.result.gauge_points.begin(),
                     render.result.gauge_points.end(),
                     [](const render::ResultGaugePoint& lhs, const render::ResultGaugePoint& rhs) {
                         return lhs.position < rhs.position;
                     });
    std::stable_sort(render.result.gauge_shifts.begin(),
                     render.result.gauge_shifts.end(),
                     [](const render::ResultShiftMarker& lhs, const render::ResultShiftMarker& rhs) {
                         return lhs.position < rhs.position;
                     });
}

void MenuApp::populate_generic_screen_render_data(render::MenuRenderData& render) {
    render.kind = render::MenuScreenKind::GenericList;
    render.generic.heading = screen_title();

    if (screen_ == Screen::OptionsHub) {
        append_menu_row(render.generic, ui_text("Audio", "오디오"), "", options_cursor_ == 0, render::MenuHitTargetKind::OptionsItem, 0, true, false);
        append_menu_row(render.generic, ui_text("Graphics", "그래픽"), "", options_cursor_ == 1, render::MenuHitTargetKind::OptionsItem, 1, true, false);
        append_menu_row(render.generic, ui_text("Skins", "스킨"), "", options_cursor_ == 2, render::MenuHitTargetKind::OptionsItem, 2, true, false);
        append_menu_row(render.generic, ui_text("Input", "입력"), "", options_cursor_ == 3, render::MenuHitTargetKind::OptionsItem, 3, true, false);
        append_menu_row(render.generic, ui_text("Calibration Wizard", "캘리브레이션 위저드"), "", options_cursor_ == 4, render::MenuHitTargetKind::OptionsItem, 4, true, false);
        append_menu_row(render.generic, ui_text("Mode", "모드"), "", options_cursor_ == 5, render::MenuHitTargetKind::OptionsItem, 5, true, false);
        append_menu_row(render.generic, ui_text("Keymap", "키 설정"), "", options_cursor_ == 6, render::MenuHitTargetKind::OptionsItem, 6, true, false);
        append_menu_row(render.generic, ui_text("Back", "뒤로"), "", options_cursor_ == 7, render::MenuHitTargetKind::OptionsItem, 7, true, false);
        render.generic.notes.push_back(ui_text("Up/Down to move, Enter to select, Esc to return.",
                                               "위/아래로 이동하고 Enter로 선택, Esc로 돌아갑니다."));
        render.generic.notes.push_back(ui_text("A/G/I/M/K also jumps directly into Audio, Graphics, Input, Mode, and Keymap.",
                                               "A/G/I/M/K로 Audio, Graphics, Input, Mode, Keymap으로 바로 이동할 수도 있습니다."));
    } else if (screen_ == Screen::EditStub) {
        render.generic.notes.push_back(ui_text("Editor is not implemented yet.", "에디터는 아직 구현되지 않았습니다."));
        append_menu_row(render.generic, ui_text("Back", "뒤로"), "", true, render::MenuHitTargetKind::SettingsRow, 0, true, false);
    } else if (screen_ == Screen::SettingsAudio) {
        populate_audio_settings_render_data(render);
    } else if (screen_ == Screen::SettingsGraphics) {
        populate_graphics_settings_render_data(render);
    } else if (screen_ == Screen::SettingsSkins) {
        populate_skin_settings_render_data(render);
    } else if (screen_ == Screen::SettingsInput) {
        populate_input_settings_render_data(render);
    } else if (screen_ == Screen::SettingsCalibration) {
        populate_calibration_settings_render_data(render);
    } else if (screen_ == Screen::ModeSelect) {
        populate_mode_settings_render_data(render);
    } else if (screen_ == Screen::ModeMods) {
        populate_mode_mods_render_data(render);
    } else if (screen_ == Screen::Keymap) {
        populate_keymap_render_data(render);
    } else if (screen_ == Screen::KeymapConfirm) {
        populate_keymap_confirm_render_data(render);
    } else if (screen_ == Screen::KeymapTest) {
        populate_keymap_test_render_data(render);
    }
}

void MenuApp::publish_snapshot() {
    const int64_t publish_start_ns = timing::HighResClock::now_ns();
    if (screen_ == Screen::SongSelect) {
        sync_song_select_state();
    }
    sync_menu_music();

    MenuSnapshot snapshot;
    render::MenuRenderData render;
    render.ui_korean = ui_uses_korean();
    render.screen_title = screen_title();
    render.performance.visible = config_.graphics.performance_overlay;

    const std::string current_track = current_track_label();
    const BestResultRecord current_best = current_song_best_result();
    rebuild_current_song_record_indices();
    const LocalPlayRecord* selected_record = current_selected_record();

    if (screen_ == Screen::QuickSetup) {
        populate_quick_setup_render_data(render);
    } else if (screen_ == Screen::Title) {
        populate_title_render_data(render, current_track, current_best);
    } else if (screen_ == Screen::SongSelect) {
        populate_song_select_render_data(render, current_track, current_best, selected_record);
    } else if (screen_ == Screen::SongBrowser) {
        populate_song_browser_render_data(render);
    } else if (screen_ == Screen::Gameplay) {
        render.kind = render::MenuScreenKind::GameplayHud;
        render.gameplay.title = last_chart_title_;
        render.gameplay.artist = last_chart_artist_;
        render.gameplay.bpm = last_chart_bpm_;
        populate_gameplay_render_data(render.gameplay);
        if (last_chart_bpm_ > 0.0 && render.gameplay.rate > 0.0) {
            render.gameplay.scroll_speed = (last_chart_bpm_ * render.gameplay.hispeed) / render.gameplay.rate;
        } else {
            render.gameplay.scroll_speed = 0.0;
        }
    } else if (screen_ == Screen::Result) {
        populate_result_render_data(render, current_track);
    } else {
        populate_generic_screen_render_data(render);
    }

    populate_help_overlay(render.help_overlay);
    snapshot.render = std::move(render);
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        snapshot_ = std::move(snapshot);
        ++snapshot_version_;
    }

    if (screen_ == Screen::SongSelect && song_select_view_ == SongSelectView::Songs) {
        const int64_t publish_elapsed_ns = timing::HighResClock::now_ns() - publish_start_ns;
        constexpr int64_t kSlowSongSelectSnapshotNs = 8'000'000LL;
        constexpr int64_t kSlowSnapshotLogCooldownNs = 2'000'000'000LL;
        if (publish_elapsed_ns >= kSlowSongSelectSnapshotNs &&
            (publish_start_ns - last_song_select_slow_snapshot_log_ns_) >= kSlowSnapshotLogCooldownNs) {
            last_song_select_slow_snapshot_log_ns_ = publish_start_ns;
            std::cerr << "[MenuApp] Slow Song Select snapshot " 
                      << (static_cast<double>(publish_elapsed_ns) / 1'000'000.0)
                      << " ms selected=" << selected_song_
                      << " visible=" << visible_song_count()
                      << " path=" << selected_song_path() << std::endl;
        }
    }
}

void MenuApp::render_tick() {
    const bool show_performance_overlay = config_.graphics.performance_overlay;
    render::RenderPerformanceSnapshot perf_snapshot =
        show_performance_overlay ? render_thread_.performance_snapshot() : render::RenderPerformanceSnapshot{};
    bool snapshot_changed = false;

    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        if (!render_cache_ready_ || rendered_snapshot_version_ != snapshot_version_) {
            render_cache_ = snapshot_.render;
            rendered_snapshot_version_ = snapshot_version_;
            render_cache_ready_ = true;
            snapshot_changed = true;
        }
        render_cache_.performance.visible =
            show_performance_overlay && render_cache_.kind != render::MenuScreenKind::ResultScreen;
    }

    if (render_cache_.kind == render::MenuScreenKind::GameplayHud) {
        uint64_t gameplay_motion_revision = 0;
        uint64_t gameplay_text_revision = 0;
        int64_t gameplay_hud_publish_time_ns = 0;
        bool gameplay_perf_active = false;
        {
            std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
            gameplay_motion_revision = gameplay_hud_.motion_revision;
            gameplay_text_revision = gameplay_hud_.text_revision;
            gameplay_hud_publish_time_ns = gameplay_hud_.hud_publish_time_ns;
            gameplay_perf_active = gameplay_hud_.active && !gameplay_hud_.loading;
        }
        if (gameplay_perf_active) {
            if (!gameplay_performance_active_) {
                gameplay_performance_tracker_.reset();
                gameplay_performance_last_motion_revision_ = 0;
                gameplay_performance_active_ = true;
            }
            if (gameplay_hud_publish_time_ns > 0 && gameplay_motion_revision != 0 &&
                gameplay_motion_revision != gameplay_performance_last_motion_revision_) {
                gameplay_performance_tracker_.record_frame_start_ns(gameplay_hud_publish_time_ns);
                gameplay_performance_last_motion_revision_ = gameplay_motion_revision;
            }
            perf_snapshot = gameplay_performance_tracker_.snapshot();
        } else if (gameplay_performance_active_) {
            gameplay_performance_tracker_.reset();
            gameplay_performance_last_motion_revision_ = 0;
            gameplay_performance_active_ = false;
        }
        if (snapshot_changed ||
            rendered_gameplay_motion_version_ != gameplay_motion_revision ||
            rendered_gameplay_text_version_ != gameplay_text_revision) {
            populate_gameplay_render_data(render_cache_.gameplay,
                                          &gameplay_motion_revision,
                                          &gameplay_text_revision);
            if (render_cache_.gameplay.bpm > 0.0 && render_cache_.gameplay.rate > 0.0) {
                render_cache_.gameplay.scroll_speed =
                    (render_cache_.gameplay.bpm * render_cache_.gameplay.hispeed) / render_cache_.gameplay.rate;
            } else {
                render_cache_.gameplay.scroll_speed = 0.0;
            }
            rendered_gameplay_motion_version_ = gameplay_motion_revision;
            rendered_gameplay_text_version_ = gameplay_text_revision;
        }

        const bool had_gameplay_metrics_visible = render_cache_.performance.gameplay_metrics_visible;
        render_cache_.performance.gameplay_metrics_visible = gameplay_perf_active;
        if (gameplay_perf_active) {
            const uint64_t gameplay_metrics_revision = (perf_snapshot.metrics_revision != 0) ? perf_snapshot.metrics_revision : 1;
            if (!had_gameplay_metrics_visible ||
                render_cache_.performance.gameplay_metrics_revision != gameplay_metrics_revision) {
                const auto diagnostics = render::compute_gameplay_motion_diagnostics(
                    render::GameplayMotionState{
                        render_cache_.gameplay.current_sample,
                        render_cache_.gameplay.duration_samples,
                        render_cache_.gameplay.sample_rate,
                        render_cache_.gameplay.audio_sample_time_ns,
                        gameplay_hud_publish_time_ns,
                        render_cache_.gameplay.audio_buffer_frames,
                        render_cache_.gameplay.visual_offset_ms,
                        render_cache_.gameplay.finished,
                        render_cache_.gameplay.game_over,
                    },
                    timing::HighResClock::now_ns());
                render_cache_.performance.gameplay_audio_age_ms = diagnostics.audio_age_ms;
                render_cache_.performance.gameplay_hud_delta_ms = diagnostics.hud_delta_ms;
                render_cache_.performance.gameplay_extrapolated_ms = diagnostics.extrapolated_ms;
                render_cache_.performance.gameplay_buffer_ms = diagnostics.buffer_ms;
                render_cache_.performance.gameplay_metrics_revision = gameplay_metrics_revision;
            }
        } else {
            render_cache_.performance.gameplay_metrics_revision = 0;
            render_cache_.performance.gameplay_audio_age_ms = 0.0;
            render_cache_.performance.gameplay_hud_delta_ms = 0.0;
            render_cache_.performance.gameplay_extrapolated_ms = 0.0;
            render_cache_.performance.gameplay_buffer_ms = 0.0;
        }
    } else {
        if (gameplay_performance_active_) {
            gameplay_performance_tracker_.reset();
            gameplay_performance_last_motion_revision_ = 0;
            gameplay_performance_active_ = false;
        }
        render_cache_.performance.gameplay_metrics_visible = false;
        render_cache_.performance.gameplay_metrics_revision = 0;
        render_cache_.performance.gameplay_audio_age_ms = 0.0;
        render_cache_.performance.gameplay_hud_delta_ms = 0.0;
        render_cache_.performance.gameplay_extrapolated_ms = 0.0;
        render_cache_.performance.gameplay_buffer_ms = 0.0;
        rendered_gameplay_motion_version_ = 0;
        rendered_gameplay_text_version_ = 0;
    }

    render_cache_.performance.valid = perf_snapshot.valid;
    render_cache_.performance.sample_count = perf_snapshot.sample_count;
    render_cache_.performance.graph_sample_count = perf_snapshot.graph_sample_count;
    render_cache_.performance.graph_revision = perf_snapshot.graph_revision;
    render_cache_.performance.metrics_revision = perf_snapshot.metrics_revision;
    render_cache_.performance.average_frame_ms = perf_snapshot.average_frame_ms;
    render_cache_.performance.average_fps = perf_snapshot.average_fps;
    render_cache_.performance.max_fps = perf_snapshot.max_fps;
    render_cache_.performance.fps_0_1_low = perf_snapshot.fps_0_1_low;
    render_cache_.performance.fps_0_01_low = perf_snapshot.fps_0_01_low;
    render_cache_.performance.frame_times_ms = perf_snapshot.frame_times_ms;
    menu_window_.render(render_cache_);
}

void MenuApp::render_snapshot(const MenuSnapshot& snapshot) {
    menu_window_.render(snapshot.render);
}

void MenuApp::update_pressed_keys(const input::InputEvent& event) {
    if (event.state == input::InputState::Pressed) {
        pressed_keys_.insert(event.keycode);
        if (screen_ == Screen::SongSelect && is_song_select_repeat_key(event.keycode)) {
            song_select_repeat_key_ = event.keycode;
            song_select_repeat_next_ns_ =
                timing::HighResClock::now_ns() + kSongSelectRepeatInitialDelayNs;
        }
    } else {
        pressed_keys_.erase(event.keycode);
        if (event.keycode == song_select_repeat_key_) {
            reset_song_select_repeat();
        }
    }
    if (screen_ == Screen::KeymapTest) {
        publish_snapshot();
    }
}

void MenuApp::update_song_select_repeat() {
    if (screen_ != Screen::SongSelect) {
        reset_song_select_repeat();
        return;
    }
    if (song_select_repeat_key_ == 0 || !is_song_select_repeat_key(song_select_repeat_key_)) {
        reset_song_select_repeat();
        return;
    }
    if (pressed_keys_.find(song_select_repeat_key_) == pressed_keys_.end()) {
        reset_song_select_repeat();
        return;
    }

    const int64_t now_ns = timing::HighResClock::now_ns();
    if (now_ns < song_select_repeat_next_ns_) {
        return;
    }

    handle_song_select_input(song_select_repeat_key_);
    song_select_repeat_next_ns_ = now_ns + kSongSelectRepeatIntervalNs;
}

void MenuApp::reset_song_select_repeat() {
    song_select_repeat_key_ = 0;
    song_select_repeat_next_ns_ = 0;
}

bool MenuApp::is_song_select_repeat_key(uint32_t keycode) const {
    return keycode == key_up_ || keycode == key_down_ ||
           keycode == key_page_up_ || keycode == key_page_down_;
}

void MenuApp::launch_gameplay(const std::string& chart_path, const std::string& replay_path) {
    const bool replay_playback = !replay_path.empty();
    const std::string preserved_result_path = last_result_path_;
    last_chart_path_ = chart_path;
    if (selected_song_ >= 0 && selected_song_ < static_cast<int>(visible_song_count())) {
        if (const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(selected_song_))) {
            last_chart_title_ = entry->title.empty() ? entry->path : entry->title;
            last_chart_artist_ = entry->artist;
            last_chart_bpm_ = entry->bpm;
        }
    } else {
        last_chart_title_.clear();
        last_chart_artist_.clear();
        last_chart_bpm_ = 0.0;
    }

    screen_ = Screen::Gameplay;
    apply_runtime_graphics_config();
    {
        std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
        reset_gameplay_hud_state(gameplay_hud_);
        gameplay_hud_.loading = true;
        gameplay_hud_.loading_percent = 0;
        gameplay_hud_.loading_stage = "Preparing gameplay";
    }
    publish_snapshot();

    input_thread_.stop();
    audio_thread_.shutdown();

    GameSession session;
    session.set_loading_progress_callback([this](const GameSession::LoadingProgress& progress) {
        update_gameplay_loading_state(progress.percent, progress.stage);
    });
    session.set_screenshot_callback([this]() {
        menu_window_.request_screenshot();
    });
#ifdef _WIN32
    auto escape_was_down = std::make_shared<std::atomic<bool>>((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0);
    session.set_loading_cancel_callback([escape_was_down]() {
        const bool escape_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        const bool fresh_press = escape_down && !escape_was_down->exchange(escape_down, std::memory_order_acq_rel);
        return fresh_press;
    });
#else
    session.set_loading_cancel_callback([]() { return false; });
#endif
    session.set_hud_callback([this](const GameSession::HudSnapshot& hud) {
        std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
        const GameplayHudRevisionInput previous = gameplay_hud_revision_input(gameplay_hud_);
        GameplayHudRevisionInput next = previous;
        next.loading = false;
        next.loading_percent = 100;
        next.loading_stage = "Ready";
        next.active = hud.active;
        next.finished = hud.finished;
        next.game_over = hud.game_over;
        next.user_aborted = hud.user_aborted;
        next.countdown_active = hud.countdown_active;
        next.countdown_value = hud.countdown_value;
        next.lane_count = hud.lane_count;
        next.current_sample = hud.current_sample;
        next.duration_samples = hud.duration_samples;
        next.sample_rate = hud.sample_rate;
        next.audio_sample_time_ns = hud.audio_sample_time_ns;
        next.audio_buffer_frames = hud.audio_buffer_frames;
        next.lookahead_samples = hud.lookahead_samples;
        next.past_samples = hud.past_samples;
        next.combo = hud.combo;
        next.max_combo = hud.max_combo;
        next.counts = hud.counts;
        next.score = hud.score;
        next.gauge = hud.gauge;
        next.gauge_type = hud.gauge_type;
        next.rate = hud.rate;
        next.hispeed = hud.hispeed;
        next.has_feedback = hud.has_feedback;
        next.feedback = hud.feedback_judgement;
        next.feedback_delta_ms = hud.feedback_delta_ms;
        next.lane_activity_count = hud.lane_activity_count;
        next.lane_activity.fill(0.0f);
        std::copy_n(hud.lane_activity.begin(), hud.lane_activity_count, next.lane_activity.begin());
        next.note_count = hud.note_count;
        for (std::size_t i = 0; i < hud.note_count; ++i) {
            next.notes[i] = GameplayHudRevisionNote{
                hud.notes[i].lane,
                hud.notes[i].start_sample,
                hud.notes[i].tail_sample,
                hud.notes[i].hold,
                hud.notes[i].head_visible,
            };
        }
        next.ghost_visible = hud.ghost_visible;
        next.ghost_score = hud.ghost_score;
        next.ghost_combo = hud.ghost_combo;
        next.ghost_max_combo = hud.ghost_max_combo;
        next.ghost_counts = hud.ghost_counts;
        next.ghost_gauge = hud.ghost_gauge;
        next.ghost_gauge_type = hud.ghost_gauge_type;
        next.ghost_has_feedback = hud.ghost_has_feedback;
        next.ghost_feedback = hud.ghost_feedback_judgement;
        next.ghost_feedback_delta_ms = hud.ghost_feedback_delta_ms;
        next.ghost_finished = hud.ghost_finished;
        next.ghost_game_over = hud.ghost_game_over;
        next.ghost_lane_activity_count = hud.ghost_lane_activity_count;
        next.ghost_lane_activity.fill(0.0f);
        std::copy_n(hud.ghost_lane_activity.begin(),
                    hud.ghost_lane_activity_count,
                    next.ghost_lane_activity.begin());
        next.ghost_note_count = hud.ghost_note_count;
        for (std::size_t i = 0; i < hud.ghost_note_count; ++i) {
            next.ghost_notes[i] = GameplayHudRevisionNote{
                hud.ghost_notes[i].lane,
                hud.ghost_notes[i].start_sample,
                hud.ghost_notes[i].tail_sample,
                hud.ghost_notes[i].hold,
                hud.ghost_notes[i].head_visible,
            };
        }
        const GameplayHudRevisionFlags diff = diff_gameplay_hud_revisions(previous, next);

        gameplay_hud_.loading = false;
        gameplay_hud_.loading_percent = 100;
        gameplay_hud_.loading_stage = "Ready";
        gameplay_hud_.active = hud.active;
        gameplay_hud_.finished = hud.finished;
        gameplay_hud_.game_over = hud.game_over;
        gameplay_hud_.user_aborted = hud.user_aborted;
        gameplay_hud_.countdown_active = hud.countdown_active;
        gameplay_hud_.countdown_value = hud.countdown_value;
        gameplay_hud_.lane_count = hud.lane_count;
        gameplay_hud_.current_sample = hud.current_sample;
        gameplay_hud_.duration_samples = hud.duration_samples;
        gameplay_hud_.sample_rate = hud.sample_rate;
        gameplay_hud_.audio_sample_time_ns = hud.audio_sample_time_ns;
        gameplay_hud_.hud_publish_time_ns = hud.hud_publish_time_ns;
        gameplay_hud_.audio_buffer_frames = hud.audio_buffer_frames;
        gameplay_hud_.lookahead_samples = hud.lookahead_samples;
        gameplay_hud_.past_samples = hud.past_samples;
        gameplay_hud_.combo = hud.combo;
        gameplay_hud_.max_combo = hud.max_combo;
        gameplay_hud_.counts = hud.counts;
        gameplay_hud_.score = hud.score;
        gameplay_hud_.gauge = hud.gauge;
        gameplay_hud_.gauge_type = hud.gauge_type;
        gameplay_hud_.rate = hud.rate;
        gameplay_hud_.hispeed = hud.hispeed;
        gameplay_hud_.has_feedback = hud.has_feedback;
        gameplay_hud_.feedback = hud.feedback_judgement;
        gameplay_hud_.feedback_delta_ms = hud.feedback_delta_ms;
        gameplay_hud_.lane_activity_count = hud.lane_activity_count;
        gameplay_hud_.lane_activity.fill(0.0f);
        std::copy_n(hud.lane_activity.begin(), hud.lane_activity_count, gameplay_hud_.lane_activity.begin());

        gameplay_hud_.note_count = hud.note_count;
        for (std::size_t i = 0; i < hud.note_count; ++i) {
            GameplayHudState::Note out;
            out.lane = hud.notes[i].lane;
            out.start_sample = hud.notes[i].start_sample;
            out.tail_sample = hud.notes[i].tail_sample;
            out.hold = hud.notes[i].hold;
            out.head_visible = hud.notes[i].head_visible;
            gameplay_hud_.notes[i] = out;
        }
        gameplay_hud_.ghost_visible = hud.ghost_visible;
        gameplay_hud_.ghost_score = hud.ghost_score;
        gameplay_hud_.ghost_combo = hud.ghost_combo;
        gameplay_hud_.ghost_max_combo = hud.ghost_max_combo;
        gameplay_hud_.ghost_counts = hud.ghost_counts;
        gameplay_hud_.ghost_gauge = hud.ghost_gauge;
        gameplay_hud_.ghost_gauge_type = hud.ghost_gauge_type;
        gameplay_hud_.ghost_has_feedback = hud.ghost_has_feedback;
        gameplay_hud_.ghost_feedback = hud.ghost_feedback_judgement;
        gameplay_hud_.ghost_feedback_delta_ms = hud.ghost_feedback_delta_ms;
        gameplay_hud_.ghost_finished = hud.ghost_finished;
        gameplay_hud_.ghost_game_over = hud.ghost_game_over;
        gameplay_hud_.ghost_lane_activity_count = hud.ghost_lane_activity_count;
        gameplay_hud_.ghost_lane_activity.fill(0.0f);
        std::copy_n(hud.ghost_lane_activity.begin(),
                    hud.ghost_lane_activity_count,
                    gameplay_hud_.ghost_lane_activity.begin());
        gameplay_hud_.ghost_note_count = hud.ghost_note_count;
        for (std::size_t i = 0; i < hud.ghost_note_count; ++i) {
            GameplayHudState::Note out;
            out.lane = hud.ghost_notes[i].lane;
            out.start_sample = hud.ghost_notes[i].start_sample;
            out.tail_sample = hud.ghost_notes[i].tail_sample;
            out.hold = hud.ghost_notes[i].hold;
            out.head_visible = hud.ghost_notes[i].head_visible;
            gameplay_hud_.ghost_notes[i] = out;
        }
        advance_gameplay_hud_revisions(gameplay_hud_, diff.motion_changed, diff.text_changed);
    });

    CommandLineOptions play_options = options_;
    play_options.chart_path = chart_path;
    play_options.replay_path = replay_path;
    if (replay_path.empty() && config_.mode.ghost_battle_enabled) {
        play_options.ghost_replay_path = best_replay_path_for_selected_song();
    }
    if (!session.initialize(play_options)) {
        const bool loading_canceled = session.was_user_aborted();
        session.shutdown();
        if (!loading_canceled) {
            std::cerr << "[error] Failed to initialize gameplay session." << std::endl;
        }
        restart_input_thread();
        restart_audio_thread();
        {
            std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
            reset_gameplay_hud_state(gameplay_hud_);
        }
        screen_ = Screen::SongSelect;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }

    session.run();
    session.shutdown();

    const double session_hispeed = session.final_hispeed();
    if (std::abs(session_hispeed - config_.speed.hi_speed) > 0.0001) {
        config_.speed.hi_speed = session_hispeed;
        persist_runtime_config();
    }

    restart_input_thread();
    restart_audio_thread();
    const auto& result = session.result();
    if (result.has_value) {
        last_result_ = result.stats;
        last_game_over_ = result.game_over;
        last_clear_status_ = result.clear_status;
        has_result_ = true;
        last_result_mods_ = result.mods;
        last_result_rate_multiplier_ = result.rate_multiplier;
        last_result_score_multiplier_ = result.score_multiplier;
        last_result_final_score_ = result.final_score;
        last_replay_path_ = result.replay_path;
        last_result_path_ = (!result.result_path.empty() || !replay_playback) ? result.result_path : preserved_result_path;
        last_export_warnings_ = result.export_warnings;
        last_session_replay_playback_ = replay_playback;
        reload_chart_best_results();
        screen_ = Screen::Result;
    } else {
        has_result_ = false;
        last_clear_status_.clear();
        last_result_mods_.clear();
        last_result_rate_multiplier_ = 1.0;
        last_result_score_multiplier_ = 1.0;
        last_result_final_score_ = 0;
        last_replay_path_.clear();
        last_result_path_.clear();
        last_export_warnings_.clear();
        last_session_replay_playback_ = false;
        screen_ = Screen::SongSelect;
    }
    apply_runtime_graphics_config();
    {
        std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
        reset_gameplay_hud_state(gameplay_hud_);
    }
    publish_snapshot();
}

std::string MenuApp::screen_title() const {
    switch (screen_) {
        case Screen::QuickSetup: return ui_text("Quick Setup", "빠른 설정");
        case Screen::Title: return ui_text("Title", "타이틀");
        case Screen::OptionsHub: return ui_text("Options", "옵션");
        case Screen::EditStub: return ui_text("Edit", "에디트");
        case Screen::SongSelect:
            if (song_select_view_ == SongSelectView::Sources) {
                return ui_text("Song Sources", "곡 소스");
            }
            if (song_select_view_ == SongSelectView::Records) {
                return ui_text("Local Records", "로컬 기록");
            }
            return ui_text("Song Select", "곡 선택");
        case Screen::SongBrowser: return ui_text("Song Browser", "곡 탐색");
        case Screen::Gameplay: return ui_text("Gameplay", "게임플레이");
        case Screen::SettingsAudio: return ui_text("Audio Settings", "오디오 설정");
        case Screen::SettingsGraphics: return ui_text("Graphics Settings", "그래픽 설정");
        case Screen::SettingsSkins: return ui_text("Skin Settings", "스킨 설정");
        case Screen::SettingsInput: return ui_text("Input Settings", "입력 설정");
        case Screen::SettingsCalibration: return ui_text("Calibration Wizard", "캘리브레이션 위저드");
        case Screen::ModeSelect: return ui_text("Mode Select", "모드 설정");
        case Screen::ModeMods: return ui_text("Mod Manager", "모드 관리자");
        case Screen::Keymap: return ui_text("Keymap", "키 설정");
        case Screen::KeymapConfirm: return ui_text("Keymap Confirm", "키 설정 확인");
        case Screen::KeymapTest: return "NKRO Test";
        case Screen::Result: return ui_text("Result", "결과");
        default: return ui_text("Menu", "메뉴");
    }
}

void MenuApp::populate_help_overlay(render::HelpOverlayData& target) const {
    target.visible = help_overlay_visible_ && screen_ != Screen::Gameplay &&
                     screen_ != Screen::Result;
    if (!target.visible) {
        return;
    }

    switch (screen_) {
        case Screen::QuickSetup:
            target.title = ui_text("Quick Setup", "빠른 설정");
            target.lines = {
                ui_text("TenRiff already created a default profile and default keymap for this first launch.",
                        "TenRiff가 첫 실행용 기본 프로필과 기본 키맵을 이미 만들었습니다."),
                ui_text("Songs Folder opens a picker on Enter or F2. You can also drag and drop a folder later.",
                        "곡 폴더는 Enter 또는 F2로 선택 창을 엽니다. 나중에 폴더를 드래그 앤 드롭해도 됩니다."),
                ui_text("Recommended starting values are Gauge Normal, Rate 1.00x, Display Offset 0ms, and BMS Keysound Follow.",
                        "권장 시작값은 노말 게이지, Rate 1.00x, 표시 오프셋 0ms, BMS 키음 연동입니다."),
                ui_text("Left / Right changes the highlighted setting. Continue saves the current values and opens Song Select.",
                        "좌우 키로 선택된 설정을 바꾸고, Continue로 현재 값을 저장한 뒤 Song Select로 이동합니다."),
            };
            target.footer = ui_text("Esc or Backspace skips to the title screen. Press F1 again to close help.",
                                    "Esc 또는 Backspace로 타이틀 화면으로 건너뜁니다. 도움말을 닫으려면 F1을 다시 누르세요.");
            return;
        case Screen::Title:
            target.title = ui_text("Title Controls", "타이틀 조작");
            target.lines = {
                ui_text("Up / Down or the mouse selects PLAY, EDIT, OPTIONS, or EXIT.",
                        "위 / 아래 키 또는 마우스로 PLAY, EDIT, OPTIONS, EXIT를 선택합니다."),
                ui_text("Enter or double-click opens the selected button.",
                        "Enter 또는 더블클릭으로 선택한 버튼을 엽니다."),
                ui_text("F5 refreshes the current song source before you enter Song Select.",
                        "F5로 Song Select에 들어가기 전 현재 곡 소스를 새로고침합니다."),
                ui_text("A / G / I / M / K jumps straight to Audio, Graphics, Input, Mode, and Keymap.",
                        "A / G / I / M / K로 Audio, Graphics, Input, Mode, Keymap으로 바로 이동합니다."),
            };
            target.footer = ui_text("Esc exits TenRiff. Press F1 again to close help.",
                                    "Esc로 TenRiff를 종료합니다. 도움말을 닫으려면 F1을 다시 누르세요.");
            return;
        case Screen::SongSelect:
            target.title = ui_text("Song Select Controls", "곡 선택 조작");
            target.lines = {
                ui_text("Up / Down or the mouse wheel moves through the current list. PgUp / PgDn jumps by a page.",
                        "위 / 아래 키 또는 마우스 휠로 현재 목록을 이동합니다. PgUp / PgDn은 페이지 단위로 이동합니다."),
                ui_text("Left / Right switches focus between the left navigation rail and the song list.",
                        "좌 / 우 키로 왼쪽 내비게이션과 곡 목록 사이의 포커스를 전환합니다."),
                ui_text("Enter selects the focused item. Double-click on a song launches it immediately.",
                        "Enter는 선택된 항목을 열고, 곡을 더블클릭하면 즉시 시작합니다."),
                ui_text("Backspace jumps to Sources or back to Songs. Esc returns to the title screen.",
                        "Backspace는 Sources 또는 Songs로 이동하고, Esc는 타이틀 화면으로 돌아갑니다."),
                ui_text("F2 chooses a new songs folder. F5 refreshes the active source.",
                        "F2로 새 곡 폴더를 고르고, F5로 활성 소스를 새로고침합니다."),
                ui_text("Safe indexing lowers RAM use for very large libraries. Fast rescans quicker on high-memory PCs.",
                        "안전 인덱싱은 매우 큰 라이브러리에서 RAM 사용량을 줄이고, 빠름은 메모리가 많은 PC에서 재스캔 속도를 높입니다."),
                ui_text("A / G / I / M / K opens Audio, Graphics, Input, Mode, and Keymap from Song Select.",
                        "A / G / I / M / K로 Song Select에서 Audio, Graphics, Input, Mode, Keymap을 바로 엽니다."),
            };
            target.footer = ui_text("Current source, filter, sort, and indexing profile stay visible on the Song Select screen.",
                                    "현재 소스, 필터, 정렬, 인덱싱 프로필은 Song Select 화면에 계속 표시됩니다.");
            return;
        case Screen::SongBrowser:
            target.title = ui_text("Browse Help", "탐색 도움말");
            target.lines = {
                ui_text("Search matches title, artist, and chart path.",
                        "검색은 제목, 아티스트, 차트 경로를 대상으로 합니다."),
                ui_text("Up / Down or the mouse wheel moves the selection. Long lists now show a scrollbar on the right.",
                        "위 / 아래 키 또는 마우스 휠로 선택을 이동합니다. 긴 목록은 오른쪽 스크롤바가 표시됩니다."),
                ui_text("Type while Search is selected. Backspace deletes one character. Delete clears the whole query.",
                        "Search가 선택된 상태에서 입력하면 검색됩니다. Backspace는 한 글자 삭제, Delete는 전체 삭제입니다."),
                ui_text("Left / Right adjusts key and difficulty filters. Enter activates Clear Filters or Back.",
                        "좌 / 우 키로 키 수와 난이도 필터를 조정하고, Enter는 필터 지우기 또는 뒤로를 실행합니다."),
            };
            target.footer = ui_text("Esc or Backspace returns to Song Select. Press F1 again to close help.",
                                    "Esc 또는 Backspace로 Song Select로 돌아갑니다. 도움말을 닫으려면 F1을 다시 누르세요.");
            return;
        case Screen::SettingsAudio:
            target.title = ui_text("Audio Settings", "오디오 설정");
            target.lines = {
                ui_text("Up / Down or the mouse wheel selects a row. Long lists show a scrollbar on the right.",
                        "위 / 아래 키 또는 마우스 휠로 행을 선택합니다. 긴 목록은 오른쪽 스크롤바가 표시됩니다."),
                ui_text("Left / Right or the +/- buttons changes the current value.",
                        "좌 / 우 키 또는 +/- 버튼으로 현재 값을 변경합니다."),
                ui_text("Follow keeps note keysounds tied to your hits. Autoplay mixes them into background audio instead.",
                        "연동은 입력 시 키음을 재생하고, 자동재생은 키음을 배경음에 섞어 재생합니다."),
                ui_text("Esc or Backspace saves the current values and returns.",
                        "Esc 또는 Backspace로 현재 값을 저장하고 돌아갑니다."),
            };
            target.footer = ui_text("A later return to Song Select keeps these values live.",
                                    "이 값들은 이후 Song Select로 돌아가도 그대로 유지됩니다.");
            return;
        case Screen::SettingsGraphics:
            target.title = ui_text("Graphics Settings", "그래픽 설정");
            target.lines = {
                ui_text("Up / Down or the mouse wheel selects a row. Long lists show a scrollbar on the right.",
                        "위 / 아래 키 또는 마우스 휠로 행을 선택합니다. 긴 목록은 오른쪽 스크롤바가 표시됩니다."),
                ui_text("Display, Resolution, Refresh Hz, and VSync apply live while you adjust them.",
                        "표시 모드, 해상도, 주사율, VSync는 조정 중에도 즉시 적용됩니다."),
                ui_text("Language switches the menu UI immediately. Display Offset shifts visuals only.",
                        "언어는 메뉴 UI에 즉시 반영되고, 표시 오프셋은 시각 요소만 이동합니다."),
                ui_text("Esc or Backspace saves and returns.",
                        "Esc 또는 Backspace로 저장하고 돌아갑니다."),
            };
            target.footer = ui_text("Use this screen when notes feel visually early or late on your display.",
                                    "노트가 화면에서 너무 빠르거나 늦게 보일 때 이 화면에서 조정하세요.");
            return;
        case Screen::SettingsSkins:
            target.title = ui_text("Skin Settings", "스킨 설정");
            target.lines = {
                ui_text("Up / Down or the mouse wheel selects a row. Long skin lists now scroll with a right-side scrollbar.",
                        "위 / 아래 키 또는 마우스 휠로 행을 선택합니다. 긴 스킨 목록은 오른쪽 스크롤바로 스크롤됩니다."),
                ui_text("Skin Source swaps between the native vector skin and imported osu!mania assets.",
                        "스킨 소스는 기본 벡터 스킨과 가져온 osu!mania 자산 사이를 전환합니다."),
                ui_text("Import OSU Skin opens a folder picker. Drag-and-drop also works on this screen.",
                        "OSU 스킨 가져오기는 폴더 선택 창을 엽니다. 이 화면에서는 드래그 앤 드롭도 지원합니다."),
                ui_text("Image Aspect keeps imported note heads and tails from stretching to the gameplay note box.",
                        "이미지 비율은 가져온 노트 헤드와 테일이 게임 노트 박스에 맞춰 늘어나지 않게 유지합니다."),
                ui_text("The right preview shows the native fallback lane colors and sizing per layout.",
                        "오른쪽 미리보기는 레이아웃별 기본 대체 레인 색상과 크기를 보여줍니다."),
            };
            target.footer = ui_text("Esc or Backspace saves and returns.",
                                    "Esc 또는 Backspace로 저장하고 돌아갑니다.");
            return;
        case Screen::SettingsInput:
            target.title = ui_text("Input Settings", "입력 설정");
            target.lines = {
                ui_text("Up / Down or the mouse wheel selects a row.",
                        "위 / 아래 키 또는 마우스 휠로 행을 선택합니다."),
                ui_text("Polling Hz changes how often keyboard state is sampled.",
                        "Polling Hz는 키보드 상태를 얼마나 자주 읽을지 바꿉니다."),
                ui_text("Debounce filters switch chatter before gameplay receives duplicate presses.",
                        "디바운스는 게임플레이가 중복 입력을 받기 전에 스위치 떨림을 걸러냅니다."),
                ui_text("Esc or Backspace saves and returns.",
                        "Esc 또는 Backspace로 저장하고 돌아갑니다."),
            };
            target.footer = ui_text("Input changes apply after the input thread restarts when you leave the screen.",
                                    "입력 변경 사항은 이 화면을 나가며 입력 스레드가 다시 시작된 뒤 적용됩니다.");
            return;
        case Screen::SettingsCalibration:
            target.title = ui_text("Calibration Wizard", "캘리브레이션 위저드");
            target.lines = {
                ui_text("Adjustment Step changes how much each Left / Right press moves the offset rows.",
                        "조정 단위는 좌 / 우 키 한 번에 오프셋이 얼마나 움직일지 정합니다."),
                ui_text("Input Offset changes judgement timing. Display Offset changes only visuals.",
                        "입력 오프셋은 판정 타이밍을 바꾸고, 표시 오프셋은 화면만 바꿉니다."),
                ui_text("Use a familiar chart, retry quickly from Result, and keep adjusting until fast/slow feedback feels centered.",
                        "익숙한 차트를 고른 뒤 결과 화면에서 빠르게 재시작하면서 빠름/느림 피드백이 중앙에 올 때까지 조정하세요."),
                ui_text("Reset Offsets returns both values to 0 ms immediately.",
                        "오프셋 초기화는 두 값을 즉시 0 ms로 되돌립니다."),
            };
            target.footer = ui_text("Changes save immediately so the next play uses the same calibration.",
                                    "변경 사항은 즉시 저장되므로 다음 플레이에도 같은 보정값이 적용됩니다.");
            return;
        case Screen::ModeSelect:
            target.title = ui_text("Mode Settings", "모드 설정");
            target.lines = {
                ui_text("Up / Down or the mouse wheel selects a row. Long lists show a scrollbar on the right.",
                        "위 / 아래 키 또는 마우스 휠로 행을 선택합니다. 긴 목록은 오른쪽 스크롤바가 표시됩니다."),
                ui_text("Ghost Battle, Gauge, Random, Mods, Rate, and Hi-Speed change the next-song compare/play feel.",
                        "고스트 배틀, 게이지, 랜덤, 모드, Rate, Hi-Speed는 다음 곡의 비교/플레이 감각을 바꿉니다."),
                ui_text("Indexing Safe minimizes RAM for huge libraries. Fast spends more RAM to speed up rescans.",
                        "인덱싱 안전은 큰 라이브러리에서 RAM 사용을 줄이고, 빠름은 더 많은 RAM으로 재스캔을 빠르게 합니다."),
                ui_text("Chart Filter decides whether Song Select shows BMS, OSU, or both when OSU indexing is enabled.",
                        "Chart Filter는 OSU 인덱싱이 켜졌을 때 Song Select에 BMS, OSU, 둘 다 표시할지 정합니다."),
                ui_text("Ghost Battle uses the selected chart's best compatible replay when one exists; turn it off to keep single-field play.",
                        "고스트 배틀은 선택한 차트의 호환되는 최고 리플레이가 있으면 사용하며, 끄면 단일 플레이 필드로 유지됩니다."),
                ui_text("Enter on Mods opens the registry-backed Mod Manager for score-multiplier presets.",
                        "Mods에서 Enter를 누르면 점수 배율 프리셋용 Mod Manager를 엽니다."),
            };
            target.footer = ui_text("Esc or Backspace saves and refreshes the library if needed.",
                                    "Esc 또는 Backspace로 저장하고, 필요하면 라이브러리를 새로고칩니다.");
            return;
        case Screen::ModeMods:
            target.title = ui_text("Mod Manager", "모드 관리자");
            target.lines = {
                ui_text("Up / Down selects a mod category. Left / Right cycles Off and the registered preset for that category.",
                        "위 / 아래 키로 모드 카테고리를 선택하고, 좌 / 우 키로 끔과 해당 카테고리의 등록 프리셋을 순환합니다."),
                ui_text("Judge Window, Note Structure, and Hold Rule normalize to one active preset per category.",
                        "Judge Window, Note Structure, Hold Rule은 카테고리마다 하나의 활성 프리셋만 유지합니다."),
                ui_text("Final score uses the lowest multiplier between the current Rate and all active mods.",
                        "최종 점수는 현재 Rate와 활성 모드 중 가장 낮은 배율을 사용합니다."),
            };
            target.footer = ui_text("Enter, Esc, or Backspace returns to Mode Settings.",
                                    "Enter, Esc, Backspace로 모드 설정으로 돌아갑니다.");
            return;
        case Screen::Keymap:
            target.title = ui_text("Keymap Help", "키 설정 도움말");
            target.lines = {
                ui_text("Up / Down selects a lane. Enter starts key capture for that lane.",
                        "위 / 아래 키로 레인을 선택하고, Enter로 해당 레인의 키 입력 대기를 시작합니다."),
                ui_text("Left / Right on Key Mode switches which 4K-10K or 16K layout you are editing.",
                        "Key Mode에서 좌 / 우 키를 누르면 편집할 4K~10K 또는 16K 레이아웃을 바꿉니다."),
                ui_text("A saves, R resets, F2 opens NKRO Test, and Esc returns.",
                        "A는 저장, R은 초기화, F2는 NKRO Test, Esc는 뒤로 이동입니다."),
            };
            target.footer = ui_text("Duplicate lane bindings are allowed.",
                                    "같은 키를 여러 레인에 중복으로 배치할 수 있습니다.");
            return;
        case Screen::KeymapTest:
            target.title = "NKRO Test";
            target.lines = {
                ui_text("Press multiple keys together to verify rollover and ghosting behavior.",
                        "여러 키를 동시에 눌러 롤오버와 고스팅 동작을 확인하세요."),
                ui_text("Highlighted lanes show which mapped keys TenRiff is currently receiving.",
                        "강조된 레인은 TenRiff가 현재 입력으로 받고 있는 매핑 키를 보여줍니다."),
            };
            target.footer = ui_text("Esc or Backspace returns to Keymap.",
                                    "Esc 또는 Backspace로 키 설정 화면으로 돌아갑니다.");
            return;
        case Screen::Result:
            target.title = ui_text("Result Screen", "결과 화면");
            target.lines = {
                ui_text("This panel shows the saved score, timing spread, gauge trace, and export file names for the run.",
                        "이 패널은 저장된 점수, 타이밍 분포, 게이지 추적, 내보낸 파일 이름을 보여줍니다."),
                ui_text("Left immediately restarts the same chart with the current settings.",
                        "Left로 현재 설정 그대로 같은 차트를 즉시 다시 시작합니다."),
                ui_text("F1 or the Replay button starts playback from the saved replay file when one is available.",
                        "F1 또는 Replay 버튼으로 저장된 리플레이 파일이 있을 때 재생을 시작합니다."),
                ui_text("Enter, Esc, and Backspace all return to Song Select.",
                        "Enter, Esc, Backspace 모두 Song Select로 돌아갑니다."),
            };
            target.footer = ui_text("Back and confirm are intentionally mirrored here for faster recovery after each song.",
                                    "곡이 끝난 뒤 빠르게 복구할 수 있도록 뒤로/확인 입력을 이 화면에서는 일부러 겹치게 배치했습니다.");
            return;
        case Screen::OptionsHub:
            target.title = ui_text("Options Hub", "옵션 허브");
            target.lines = {
                ui_text("Enter opens Audio, Graphics, Skins, Input, Calibration Wizard, Mode, or Keymap.",
                        "Enter로 Audio, Graphics, Skins, Input, 캘리브레이션 위저드, Mode, Keymap을 엽니다."),
                ui_text("A / G / I / M / K jumps directly to the most common settings screens.",
                        "A / G / I / M / K로 자주 쓰는 설정 화면으로 바로 이동합니다."),
            };
            target.footer = ui_text("Esc or Backspace returns to the previous screen.",
                                    "Esc 또는 Backspace로 이전 화면으로 돌아갑니다.");
            return;
        default:
            target.title = ui_text("Controls", "조작");
            target.lines = {
                ui_text("Up / Down selects items. Left / Right changes adjustable values.",
                        "위 / 아래 키로 항목을 선택하고, 좌 / 우 키로 조절 가능한 값을 바꿉니다."),
                ui_text("Enter confirms the focused action. Esc or Backspace returns.",
                        "Enter로 선택을 확정하고, Esc 또는 Backspace로 돌아갑니다."),
            };
            target.footer = ui_text("Press F1 again to close help.",
                                    "도움말을 닫으려면 F1을 다시 누르세요.");
            return;
    }
}

const SongEntry* MenuApp::visible_song_entry(std::size_t visible_index) const {
    if (visible_index >= visible_song_indices_.size()) {
        return nullptr;
    }
    const std::size_t song_index = visible_song_indices_[visible_index];
    if (song_index >= indexed_songs_.size()) {
        return nullptr;
    }
    return &indexed_songs_[song_index];
}

}  // namespace tenriff::app
