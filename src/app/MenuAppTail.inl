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
    target.note_width_scale = std::clamp(
        config::resolved_skin_note_width_scale(config_.skin, skin_mode),
        config::kNoteWidthScaleMin,
        config::kNoteWidthScaleMax);
    target.note_height_scale = std::clamp(
        config::resolved_skin_note_height_scale(config_.skin, skin_mode),
        config::kNoteHeightScaleMin,
        config::kNoteHeightScaleMax);
    target.lane_divider_width_scale = std::clamp(
        config::resolved_skin_lane_divider_width_scale(config_.skin, skin_mode),
        config::kLaneDividerWidthScaleMin,
        config::kLaneDividerWidthScaleMax);
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
    render.generic.heading = "Quick Setup";

    append_menu_row(render.generic,
                    "Songs Folder",
                    safe_ui_text(menu_songs::song_source_display_name(songs_path_), "Choose Folder"),
                    settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow,
                    0,
                    true,
                    false);
    append_menu_row(render.generic,
                    "Gauge",
                    gauge_label(config_.mode.gauge),
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
                    "Display Offset",
                    format_signed_offset_ms(config_.visual_offset_ms),
                    settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow,
                    3,
                    false,
                    true);
    append_menu_row(render.generic,
                    "BMS Keysound",
                    keysound_policy_label(config_.audio_ui.bms_keysound_policy),
                    settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow,
                    4,
                    false,
                    true);
    append_menu_row(render.generic,
                    "Continue to Song Select",
                    "",
                    settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow,
                    5,
                    true,
                    false);
    append_menu_row(render.generic,
                    "Skip to Title",
                    "",
                    settings_cursor_ == 6,
                    render::MenuHitTargetKind::SettingsRow,
                    6,
                    true,
                    false);

    render.generic.notes.push_back("First launch detected. TenRiff already created a default profile and default keymap.");
    render.generic.notes.push_back("Recommended start: Gauge Normal, Rate 1.00x, Display Offset 0ms, BMS Keysound Follow.");
    render.generic.notes.push_back("Songs Folder opens a picker on Enter or F2. You can also drag and drop a folder later.");
    render.generic.notes.push_back("These values stay editable later from Song Select: A=Audio  G=Graphics  I=Input  M=Mode  K=Keymap.");
}

void MenuApp::populate_title_render_data(render::MenuRenderData& render,
                                         const std::string& current_track,
                                         const MenuApp::BestResultRecord& current_best) {
    render.kind = render::MenuScreenKind::TitleMenu;
    render.title.profile = options_.profile;
    render.title.track = current_track;
    render.title.high_score = current_best.has_value ? current_best.best_score : 0;
    render.title.buttons = {
        render::MenuButtonData{"PLAY", u8"▶", title_cursor_ == 0},
        render::MenuButtonData{"EDIT", u8"✎", title_cursor_ == 1},
        render::MenuButtonData{"OPTIONS", u8"⚙", title_cursor_ == 2},
        render::MenuButtonData{"EXIT", u8"⏻", title_cursor_ == 3},
    };
    render.title.guides = {
        "UP / DOWN or mouse to move",
        "ENTER or double-click to open",
        "F5 refreshes the current song source",
        "A/G/I/M/K jumps to Audio / Graphics",
        "Input / Mode / Keymap",
        "F1 opens the control help overlay",
        "ESC exits from the title menu",
    };
}

void MenuApp::populate_result_render_data(render::MenuRenderData& render, const std::string& current_track) {
    render.kind = render::MenuScreenKind::ResultScreen;
    render.result.profile = options_.profile;
    render.result.track = last_chart_title_.empty() ? current_track : last_chart_title_;
    render.result.title = last_chart_title_.empty() ? "Unknown Chart" : last_chart_title_;
    render.result.artist = last_chart_artist_;

    if (!has_result_) {
        render.result.notes.push_back("No result data is available for this run.");
        render.result.notes.push_back("Press Enter or Esc to return to Song Select.");
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
    render.result.status = last_game_over_ ? "GAME OVER" : "CLEAR";
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
    render.result.miss = 0;
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
        render.result.notes.push_back("Replay: " + render.result.replay_file);
    }
    if (!render.result.result_file.empty()) {
        render.result.notes.push_back("Result: " + render.result.result_file);
    }
    if (render.result.replay_available) {
        render.result.notes.push_back("R or click Replay to watch the saved input trace.");
    } else if (!render.result.replay_file.empty()) {
        render.result.notes.push_back("Replay file is missing or unavailable.");
    }
    render.result.notes.push_back("Score x" + format_decimal(last_result_score_multiplier_));
    if (last_result_rate_multiplier_ != 1.0) {
        render.result.notes.push_back("Rate score x" + format_decimal(last_result_rate_multiplier_));
    }
    render.result.notes.push_back("Mods: " + mode_mod_summary(last_result_mods_));
    render.result.notes.push_back("Timing center " + format_signed_ms(last_result_.mean_delta_ms) +
                                  "  spread " + format_decimal(render.result.stddev_delta_ms) + "ms");
    if (last_session_replay_playback_) {
        render.result.notes.push_back("Replay playback session: no new replay/result export was written.");
    }
    if (!last_export_warnings_.empty()) {
        render.result.notes.push_back("Export warnings: " + std::to_string(last_export_warnings_.size()));
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
        append_menu_row(render.generic, "Audio", "", options_cursor_ == 0, render::MenuHitTargetKind::OptionsItem, 0, true, false);
        append_menu_row(render.generic, "Graphics", "", options_cursor_ == 1, render::MenuHitTargetKind::OptionsItem, 1, true, false);
        append_menu_row(render.generic, "Skins", "", options_cursor_ == 2, render::MenuHitTargetKind::OptionsItem, 2, true, false);
        append_menu_row(render.generic, "Input", "", options_cursor_ == 3, render::MenuHitTargetKind::OptionsItem, 3, true, false);
        append_menu_row(render.generic, "Mode", "", options_cursor_ == 4, render::MenuHitTargetKind::OptionsItem, 4, true, false);
        append_menu_row(render.generic, "Keymap", "", options_cursor_ == 5, render::MenuHitTargetKind::OptionsItem, 5, true, false);
        append_menu_row(render.generic, "Back", "", options_cursor_ == 6, render::MenuHitTargetKind::OptionsItem, 6, true, false);
        render.generic.notes.push_back("Up/Down to move, Enter to select, Esc to return.");
        render.generic.notes.push_back("A/G/I/M/K also jumps directly into Audio, Graphics, Input, Mode, and Keymap.");
    } else if (screen_ == Screen::EditStub) {
        render.generic.notes.push_back("Editor is not implemented yet.");
        append_menu_row(render.generic, "Back", "", true, render::MenuHitTargetKind::SettingsRow, 0, true, false);
    } else if (screen_ == Screen::SettingsAudio) {
        populate_audio_settings_render_data(render);
    } else if (screen_ == Screen::SettingsGraphics) {
        populate_graphics_settings_render_data(render);
    } else if (screen_ == Screen::SettingsSkins) {
        populate_skin_settings_render_data(render);
    } else if (screen_ == Screen::SettingsInput) {
        populate_input_settings_render_data(render);
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
    if (screen_ == Screen::SongSelect) {
        sync_song_select_state();
    }
    sync_menu_music();

    MenuSnapshot snapshot;
    render::MenuRenderData render;
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
        advance_gameplay_hud_revisions(gameplay_hud_, diff.motion_changed, diff.text_changed);
    });

    CommandLineOptions play_options = options_;
    play_options.chart_path = chart_path;
    play_options.replay_path = replay_path;
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
        case Screen::QuickSetup: return "Quick Setup";
        case Screen::Title: return "Title";
        case Screen::OptionsHub: return "Options";
        case Screen::EditStub: return "Edit";
        case Screen::SongSelect:
            if (song_select_view_ == SongSelectView::Sources) {
                return "Song Sources";
            }
            if (song_select_view_ == SongSelectView::Records) {
                return "Local Records";
            }
            return "Song Select";
        case Screen::SongBrowser: return "Song Browser";
        case Screen::Gameplay: return "Gameplay";
        case Screen::SettingsAudio: return "Audio Settings";
        case Screen::SettingsGraphics: return "Graphics Settings";
        case Screen::SettingsSkins: return "Skin Settings";
        case Screen::SettingsInput: return "Input Settings";
        case Screen::ModeSelect: return "Mode Select";
        case Screen::ModeMods: return "Mod Manager";
        case Screen::Keymap: return "Keymap";
        case Screen::KeymapConfirm: return "Keymap Confirm";
        case Screen::KeymapTest: return "NKRO Test";
        case Screen::Result: return "Result";
        default: return "Menu";
    }
}

void MenuApp::populate_help_overlay(render::HelpOverlayData& target) const {
    target.visible = help_overlay_visible_ && screen_ != Screen::Gameplay;
    if (!target.visible) {
        return;
    }

    switch (screen_) {
        case Screen::QuickSetup:
            target.title = "Quick Setup";
            target.lines = {
                "TenRiff already created a default profile and default keymap for this first launch.",
                "Songs Folder opens a picker on Enter or F2. You can also drag and drop a folder later.",
                "Recommended starting values are Gauge Normal, Rate 1.00x, Display Offset 0ms, and BMS Keysound Follow.",
                "Left / Right changes the highlighted setting. Continue saves the current values and opens Song Select.",
            };
            target.footer = "Esc or Backspace skips to the title screen. Press F1 again to close help.";
            return;
        case Screen::Title:
            target.title = "Title Controls";
            target.lines = {
                "Up / Down or the mouse selects PLAY, EDIT, OPTIONS, or EXIT.",
                "Enter or double-click opens the selected button.",
                "F5 refreshes the current song source before you enter Song Select.",
                "A / G / I / M / K jumps straight to Audio, Graphics, Input, Mode, and Keymap.",
            };
            target.footer = "Esc exits TenRiff. Press F1 again to close help.";
            return;
        case Screen::SongSelect:
            target.title = "Song Select Controls";
            target.lines = {
                "Up / Down or the mouse wheel moves through the current list. PgUp / PgDn jumps by a page.",
                "Left / Right switches focus between the left navigation rail and the song list.",
                "Enter selects the focused item. Double-click on a song launches it immediately.",
                "Backspace jumps to Sources or back to Songs. Esc returns to the title screen.",
                "F2 chooses a new songs folder. F5 refreshes the active source.",
                "Safe indexing lowers RAM use for very large libraries. Fast rescans quicker on high-memory PCs.",
                "A / G / I / M / K opens Audio, Graphics, Input, Mode, and Keymap from Song Select.",
            };
            target.footer = "Current source, filter, sort, and indexing profile stay visible on the Song Select screen.";
            return;
        case Screen::SongBrowser:
            target.title = "Browse Help";
            target.lines = {
                "Search matches title, artist, and chart path.",
                "Up / Down or the mouse wheel moves the selection. Long lists now show a scrollbar on the right.",
                "Type while Search is selected. Backspace deletes one character. Delete clears the whole query.",
                "Left / Right adjusts key and difficulty filters. Enter activates Clear Filters or Back.",
            };
            target.footer = "Esc or Backspace returns to Song Select. Press F1 again to close help.";
            return;
        case Screen::SettingsAudio:
            target.title = "Audio Settings";
            target.lines = {
                "Up / Down or the mouse wheel selects a row. Long lists show a scrollbar on the right.",
                "Left / Right or the +/- buttons changes the current value.",
                "Follow keeps note keysounds tied to your hits. Autoplay mixes them into background audio instead.",
                "Esc or Backspace saves the current values and returns.",
            };
            target.footer = "A later return to Song Select keeps these values live.";
            return;
        case Screen::SettingsGraphics:
            target.title = "Graphics Settings";
            target.lines = {
                "Up / Down or the mouse wheel selects a row. Long lists show a scrollbar on the right.",
                "Display, Resolution, Refresh Hz, and VSync apply live while you adjust them.",
                "Display Offset shifts visuals only. Positive values draw notes earlier without moving judgement.",
                "Esc or Backspace saves and returns.",
            };
            target.footer = "Use this screen when notes feel visually early or late on your display.";
            return;
        case Screen::SettingsSkins:
            target.title = "Skin Settings";
            target.lines = {
                "Up / Down or the mouse wheel selects a row. Long skin lists now scroll with a right-side scrollbar.",
                "Skin Source swaps between the native vector skin and imported osu!mania assets.",
                "Import OSU Skin opens a folder picker. Drag-and-drop also works on this screen.",
                "Image Aspect keeps imported note heads and tails from stretching to the gameplay note box.",
                "The right preview shows the native fallback lane colors and sizing per layout.",
            };
            target.footer = "Esc or Backspace saves and returns.";
            return;
        case Screen::SettingsInput:
            target.title = "Input Settings";
            target.lines = {
                "Up / Down or the mouse wheel selects a row.",
                "Polling Hz changes how often keyboard state is sampled.",
                "Debounce filters switch chatter before gameplay receives duplicate presses.",
                "Esc or Backspace saves and returns.",
            };
            target.footer = "Input changes apply after the input thread restarts when you leave the screen.";
            return;
        case Screen::ModeSelect:
            target.title = "Mode Settings";
            target.lines = {
                "Up / Down or the mouse wheel selects a row. Long lists show a scrollbar on the right.",
                "Gauge, Random, Mods, Rate, and Hi-Speed change the play feel for the next song.",
                "Indexing Safe minimizes RAM for huge libraries. Fast spends more RAM to speed up rescans.",
                "Chart Filter decides whether Song Select shows BMS, OSU, or both when OSU indexing is enabled.",
                "Enter on Mods opens the registry-backed Mod Manager for score-multiplier presets.",
            };
            target.footer = "Esc or Backspace saves and refreshes the library if needed.";
            return;
        case Screen::ModeMods:
            target.title = "Mod Manager";
            target.lines = {
                "Up / Down selects a mod category. Left / Right cycles Off and the registered preset for that category.",
                "Judge Window, Note Structure, and Hold Rule normalize to one active preset per category.",
                "Final score uses the lowest multiplier between the current Rate and all active mods.",
            };
            target.footer = "Enter, Esc, or Backspace returns to Mode Settings.";
            return;
        case Screen::Keymap:
            target.title = "Keymap Help";
            target.lines = {
                "Up / Down selects a lane. Enter starts key capture for that lane.",
                "Left / Right on Key Mode switches which 4K-10K or 16K layout you are editing.",
                "A saves, R resets, F2 opens NKRO Test, and Esc returns.",
            };
            target.footer = "Duplicate lane bindings are allowed.";
            return;
        case Screen::KeymapTest:
            target.title = "NKRO Test";
            target.lines = {
                "Press multiple keys together to verify rollover and ghosting behavior.",
                "Highlighted lanes show which mapped keys TenRiff is currently receiving.",
            };
            target.footer = "Esc or Backspace returns to Keymap.";
            return;
        case Screen::Result:
            target.title = "Result Screen";
            target.lines = {
                "This panel shows the saved score, timing spread, gauge trace, and export file names for the run.",
                "R or the Replay button starts playback from the saved replay file when one is available.",
                "Enter, Esc, and Backspace all return to Song Select.",
            };
            target.footer = "Back and confirm are intentionally mirrored here for faster recovery after each song.";
            return;
        case Screen::OptionsHub:
            target.title = "Options Hub";
            target.lines = {
                "Enter opens Audio, Graphics, Skins, Input, Mode, or Keymap.",
                "A / G / I / M / K jumps directly to the most common settings screens.",
            };
            target.footer = "Esc or Backspace returns to the previous screen.";
            return;
        default:
            target.title = "Controls";
            target.lines = {
                "Up / Down selects items. Left / Right changes adjustable values.",
                "Enter confirms the focused action. Esc or Backspace returns.",
            };
            target.footer = "Press F1 again to close help.";
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
