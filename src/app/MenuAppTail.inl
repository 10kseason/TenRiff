void MenuApp::populate_gameplay_render_data(render::GameplayHudData& target,
                                            uint64_t* out_motion_revision,
                                            uint64_t* out_text_revision) {
    std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);

    target.motion_revision = gameplay_hud_.motion_revision;
    target.text_revision = gameplay_hud_.text_revision;
    target.active = gameplay_hud_.active;
    target.loading = gameplay_hud_.loading;
    target.paused = gameplay_hud_.paused;
    target.pause_menu_cursor = gameplay_hud_.pause_menu_cursor;
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
    target.current_visual_position = gameplay_hud_.current_visual_position;
    target.visual_velocity = gameplay_hud_.visual_velocity;
    target.future_visual_span = gameplay_hud_.future_visual_span;
    target.past_visual_span = gameplay_hud_.past_visual_span;
    const render::GameplayBackgroundPolicy background_policy =
        render::resolve_gameplay_background_policy(
            config_.graphics.bga_enabled,
            gameplay_hud_.background_base_path,
            gameplay_hud_.background_overlay_path,
            gameplay_hud_.background_base_start_sample,
            gameplay_hud_.background_overlay_start_sample,
            config_.graphics.background_upscale_mode);
    target.background_base_path = background_policy.base_path;
    target.background_overlay_path = background_policy.overlay_path;
    target.background_base_start_sample = background_policy.base_start_sample;
    target.background_overlay_start_sample = background_policy.overlay_start_sample;
    target.background_upscale_mode = background_policy.upscale_mode;
    target.background_upscale_model_path = config_.graphics.background_upscale_model_path;
    target.background_upscale_prefer_npu = config_.graphics.background_upscale_prefer_npu;
    // While a chart is running the session owns these two: F1/F2 and F7/F8 retune
    // them mid-song and the stored config only catches up when the song ends.
    const double clamped_judgement_line_position = std::clamp(
        gameplay_hud_.active ? gameplay_hud_.judgement_line_position
                             : config_.skin.judgement_line_position,
        config::kJudgementLinePositionMin,
        config::kJudgementLinePositionMax);
    const double clamped_combo_position = std::clamp(
        config_.skin.combo_position,
        config::kComboPositionMin,
        config::kComboPositionMax);
    target.judgement_line_position = clamped_judgement_line_position;
    target.gameplay_field_offset_x = std::clamp(
        config_.skin.gameplay_field_offset_x,
        config::kGameplayFieldOffsetXMin,
        config::kGameplayFieldOffsetXMax);
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
    target.show_cursor_in_gameplay = config_.ui.show_cursor_in_gameplay;
    target.show_lane_dividers = config_.skin.show_lane_dividers;
    target.expand_notes_to_dividers = config_.skin.expand_notes_to_dividers;
    target.show_judgement_line = config_.skin.show_judgement_line;
    target.show_gear_boundary_line = config_.skin.show_gear_boundary_line;
    target.show_hold_tail = config_.skin.show_hold_tail;
    target.hold_tail_taper_enabled = config_.skin.hold_tail_taper_enabled;
    target.judgement_line_glow_enabled = config_.skin.judgement_line_glow_enabled;
    target.key_pulse_enabled = config_.skin.key_pulse_enabled;
    target.key_pulse_brightness = static_cast<float>(config_.skin.key_pulse_brightness);
    target.key_label_position = config::normalize_skin_key_label_position_token(config_.skin.key_label_position);
    target.note_border_enabled = config_.skin.note_border_enabled;
    target.note_shape = config::normalize_skin_note_shape_token(config_.skin.note_shape);
    target.preserve_note_image_aspect_ratio = config_.skin.preserve_note_image_aspect_ratio;
    target.skin_source = config::normalize_skin_source_token(config_.skin.source);
    target.external_skin_root = active_external_skin_root();
    target.external_skin_name = active_external_skin_name();
    target.skin_background_path =
        (target.skin_source == "tenriff" && active_tenriff_skin_.found)
            ? active_tenriff_skin_.gameplay_background_path
            : std::string{};
    target.skin_background_opacity =
        (target.skin_source == "tenriff" && active_tenriff_skin_.found)
            ? active_tenriff_skin_.gameplay_background_opacity
            : 0.66f;
    target.lr2_resolution_override =
        config::normalize_skin_lr2_resolution_mode_token(config_.skin.lr2_resolution_mode);
    target.lane_background_opacity = std::clamp(
        config_.skin.lane_background_opacity,
        config::kSkinLaneBackgroundOpacityMin,
        config::kSkinLaneBackgroundOpacityMax);
    target.black_playfield_enabled = config_.skin.black_playfield_enabled;
    target.visual_opacity = std::clamp(
        config_.skin.visual_opacity,
        config::kSkinVisualOpacityMin,
        config::kSkinVisualOpacityMax);
    target.note_outline_opacity = std::clamp(
        config_.skin.note_outline_opacity,
        config::kSkinNoteOutlineOpacityMin,
        config::kSkinNoteOutlineOpacityMax);
    target.hold_body_opacity = std::clamp(
        config_.skin.hold_body_opacity,
        config::kSkinHoldBodyOpacityMin,
        config::kSkinHoldBodyOpacityMax);
    target.visual_offset_ms = std::clamp(
        gameplay_hud_.active ? gameplay_hud_.visual_offset_ms : config_.visual_offset_ms,
        kVisualOffsetMin,
        kVisualOffsetMax);
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
    target.timing_history_count = gameplay_hud_.timing_history_count;
    target.timing_history_delta_ms.fill(0.0);
    std::copy_n(gameplay_hud_.timing_history_delta_ms.begin(),
                gameplay_hud_.timing_history_count,
                target.timing_history_delta_ms.begin());
    target.finished = gameplay_hud_.finished;
    target.game_over = gameplay_hud_.game_over;
    target.spectating_peer = gameplay_hud_.spectating_peer;

    const network::PeerSessionSnapshot peer = peer_session_.snapshot();
    target.peer_visible = multiplayer_match_active_ && peer.role != network::PeerRole::None;
    target.peer_score_available = target.peer_visible && peer.has_remote_score;
    target.peer_name = peer.peer_name;
    target.peer_disconnected = target.peer_visible && peer.state != network::PeerSessionState::Connected;
    target.peer_status = target.peer_disconnected ? ui_text("DISCONNECTED", "연결 끊김")
                                                   : ui_text("PLAYING", "플레이 중");
    target.peer_current_sample = 0;
    target.peer_score = 0;
    target.peer_combo = 0;
    target.peer_max_combo = 0;
    target.peer_pg = 0;
    target.peer_gr = 0;
    target.peer_gd = 0;
    target.peer_bd = 0;
    target.peer_pr = 0;
    target.peer_gauge = 0.0;
    target.peer_finished = false;
    target.peer_game_over = false;
    target.peer_aborted = false;
    if (target.peer_visible && peer.has_remote_score) {
        target.peer_current_sample = peer.latest_remote_score.current_sample;
        target.peer_score = peer.latest_remote_score.score;
        target.peer_combo = peer.latest_remote_score.combo;
        target.peer_max_combo = peer.latest_remote_score.max_combo;
        target.peer_pg = peer.latest_remote_score.perfect;
        target.peer_gr = peer.latest_remote_score.great;
        target.peer_gd = peer.latest_remote_score.good;
        target.peer_bd = peer.latest_remote_score.bad;
        target.peer_pr = peer.latest_remote_score.poor;
        target.peer_gauge = static_cast<double>(peer.latest_remote_score.gauge_milli) / 1000.0;
        target.peer_finished = peer.latest_remote_score.finished;
        target.peer_game_over = peer.latest_remote_score.game_over;
        target.peer_aborted = peer.latest_remote_score.aborted;
        if (target.peer_finished) {
            target.peer_status = peer.latest_remote_score.aborted
                                     ? ui_text("ABORTED", "중단")
                                     : (peer.latest_remote_score.game_over
                                            ? ui_text("FAILED", "실패")
                                            : ui_text("FINISHED", "완료"));
        }
    }
    if (!target.peer_disconnected && target.peer_game_over) {
        target.peer_status = ui_text("GAME OVER", "\uAC8C\uC784 \uC624\uBC84");
    } else if (!target.peer_disconnected && target.peer_aborted) {
        target.peer_status = ui_text("ABORTED", "\uC911\uB2E8");
    } else if (!target.peer_disconnected && target.peer_finished) {
        target.peer_status = ui_text("FINISHED", "\uC644\uB8CC");
    }
    const PeerBattleScoreLead versus_lead =
        target.peer_score_available
            ? peer_battle_score_lead(gameplay_hud_.score, target.peer_score)
            : PeerBattleScoreLead{};
    target.versus_score_difference = versus_lead.difference;
    target.versus_score_position = versus_lead.position;

    target.lane_activity_count = gameplay_hud_.lane_activity_count;
    target.lane_activity.fill(0.0f);
    std::copy_n(gameplay_hud_.lane_activity.begin(), gameplay_hud_.lane_activity_count, target.lane_activity.begin());

    target.lane_pressed_count = gameplay_hud_.lane_pressed_count;
    target.lane_pressed.fill(0);
    std::copy_n(gameplay_hud_.lane_pressed.begin(), gameplay_hud_.lane_pressed_count, target.lane_pressed.begin());

    target.lane_color_count = 0;
    target.lane_colors.fill(0);
    const auto lane_colors = config::resolved_skin_lane_colors(config_.skin, skin_mode);
    target.lane_color_count = std::min<std::size_t>(lane_colors.size(), static_cast<std::size_t>(target.lane_count));
    for (std::size_t i = 0; i < target.lane_color_count; ++i) {
        target.lane_colors[i] = config::skin_color_rgb(lane_colors[i]);
    }

    auto compact_key_label = [](std::string value) -> std::string {
        if (value == "Semicolon") {
            return ";";
        }
        if (value == "LBracket") {
            return "[";
        }
        if (value == "RBracket") {
            return "]";
        }
        if (value == "Apostrophe") {
            return "'";
        }
        if (value == "Comma") {
            return ",";
        }
        if (value == "Period") {
            return ".";
        }
        if (value == "Slash") {
            return "/";
        }
        if (value == "Backslash") {
            return "\\";
        }
        if (value == "Grave") {
            return "`";
        }
        if (value == "Space") {
            return "SP";
        }
        if (value == "Backspace") {
            return "Bksp";
        }
        if (value == "PageUp") {
            return "PgUp";
        }
        if (value == "PageDown") {
            return "PgDn";
        }
        return value;
    };

    target.key_label_count = 0;
    target.key_labels.fill(std::string{});
    if (target.key_label_position != "off") {
        config::KeymapManager keymap_manager;
        const auto bindings = keymap_manager.bindings_for_mode(keymap_, skin_mode);
        target.key_label_count = static_cast<std::size_t>(target.lane_count);
        for (std::size_t i = 0; i < target.key_label_count && i < target.key_labels.size(); ++i) {
            const std::string lane_id = "lane" + std::to_string(i + 1);
            const auto binding = bindings.find(lane_id);
            if (binding != bindings.end()) {
                target.key_labels[i] = compact_key_label(binding->second);
            }
        }
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
        out_note.pending = note.pending;
        out_note.mine = note.mine;
        out_note.visual_position = note.visual_position;
        out_note.tail_visual_position = note.tail_visual_position;
        target.notes[i] = out_note;
    }
    target.score = gameplay_hud_.score;
    target.osu_od8_score_available = gameplay_hud_.osu_od8_score_available;
    target.osu_od8_score = gameplay_hud_.osu_od8_score;

    target.accuracy = gameplay_hud_.accuracy;
    target.detailed_accuracy = gameplay_hud_.detailed_accuracy;

    if (target.spectating_peer && target.peer_score_available) {
        // Render-only perspective switch. Local result data stays untouched,
        // while the visible score/gauge panel follows the surviving peer.
        target.score = target.peer_score;
        target.osu_od8_score_available = false;
        target.osu_od8_score = 0;
        target.combo = target.peer_combo;
        target.max_combo = target.peer_max_combo;
        target.pg = target.peer_pg;
        target.gr = target.peer_gr;
        target.gd = target.peer_gd;
        target.bd = target.peer_bd;
        target.pr = target.peer_pr;
        target.total_notes = target.peer_pg + target.peer_gr + target.peer_gd + target.peer_bd;
        target.gauge = target.peer_gauge;
        target.gauge_label = ui_text("OPPONENT", "\uC0C1\uB300");
        target.has_feedback = false;
        target.timing_history_count = 0;
        target.lane_activity_count = 0;
        target.lane_pressed_count = 0;
        const int peer_judged_total =
            target.peer_pg + target.peer_gr + target.peer_gd + target.peer_bd;
        if (peer_judged_total > 0) {
            const double peer_weighted =
                target.peer_pg * 1.0 + target.peer_gr * 0.80 +
                target.peer_gd * 0.50 + target.peer_bd * 0.20;
            target.accuracy =
                std::clamp(peer_weighted / static_cast<double>(peer_judged_total) * 100.0, 0.0, 100.0);
        } else {
            target.accuracy = 0.0;
        }
    }

    target.ghost_visible = gameplay_hud_.ghost_visible;
    target.ghost_score = gameplay_hud_.ghost_score;
    target.ghost_osu_od8_score_available = gameplay_hud_.ghost_osu_od8_score_available;
    target.ghost_osu_od8_score = gameplay_hud_.ghost_osu_od8_score;
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
    target.ghost_timing_history_count = gameplay_hud_.ghost_timing_history_count;
    target.ghost_timing_history_delta_ms.fill(0.0);
    std::copy_n(gameplay_hud_.ghost_timing_history_delta_ms.begin(),
                gameplay_hud_.ghost_timing_history_count,
                target.ghost_timing_history_delta_ms.begin());
    target.ghost_finished = gameplay_hud_.ghost_finished;
    target.ghost_game_over = gameplay_hud_.ghost_game_over;
    target.ghost_lane_activity_count = gameplay_hud_.ghost_lane_activity_count;
    target.ghost_lane_activity.fill(0.0f);
    std::copy_n(gameplay_hud_.ghost_lane_activity.begin(),
                gameplay_hud_.ghost_lane_activity_count,
                target.ghost_lane_activity.begin());
    target.ghost_lane_pressed_count = gameplay_hud_.ghost_lane_pressed_count;
    target.ghost_lane_pressed.fill(0);
    std::copy_n(gameplay_hud_.ghost_lane_pressed.begin(),
                gameplay_hud_.ghost_lane_pressed_count,
                target.ghost_lane_pressed.begin());
    target.ghost_note_count = gameplay_hud_.ghost_note_count;
    for (std::size_t i = 0; i < gameplay_hud_.ghost_note_count; ++i) {
        const auto& note = gameplay_hud_.ghost_notes[i];
        render::GameplayNoteData out_note;
        out_note.lane = note.lane;
        out_note.start_sample = note.start_sample;
        out_note.tail_sample = note.tail_sample;
        out_note.hold = note.hold;
        out_note.head_visible = note.head_visible;
        out_note.pending = note.pending;
        out_note.mine = note.mine;
        out_note.visual_position = note.visual_position;
        out_note.tail_visual_position = note.tail_visual_position;
        target.ghost_notes[i] = out_note;
    }
    target.ghost_accuracy = gameplay_hud_.ghost_accuracy;
    target.ghost_detailed_accuracy = gameplay_hud_.ghost_detailed_accuracy;

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
        gameplay_hud_.loading = true;
        gameplay_hud_.active = false;
        gameplay_hud_.loading_percent = clamp_int(percent, 0, 100);
        gameplay_hud_.loading_stage = std::string(stage);
        const GameplayHudRevisionInput next = gameplay_hud_revision_input(gameplay_hud_);
        const GameplayHudRevisionFlags diff = diff_gameplay_hud_revisions(previous, next);
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

void MenuApp::service_song_preview() {
    constexpr int64_t kPreviewDelayNs = 750'000'000LL;
    const int64_t now_ns = timing::HighResClock::now_ns();

    const SongEntry* entry = nullptr;
    if (screen_ == Screen::SongSelect &&
        song_select_view_ == SongSelectView::Songs &&
        config_.audio_ui.background_sound_enabled &&
        selected_song_ >= 0) {
        entry = visible_song_entry(static_cast<std::size_t>(selected_song_));
    }

    const std::string preview_path = entry ? entry->audio_preview_path : std::string{};
    const std::string selection_key =
        entry ? entry->path + "\n" + preview_path : std::string{};

    if (!entry || preview_path.empty()) {
        const bool was_active = !song_preview_active_path_.empty();
        song_preview_selection_key_.clear();
        song_preview_active_path_.clear();
        song_preview_due_ns_ = 0;
        song_preview_pending_ = false;
        if (was_active) {
            sync_menu_music();
        }
        return;
    }

    if (selection_key != song_preview_selection_key_) {
        const bool was_active = !song_preview_active_path_.empty();
        song_preview_selection_key_ = selection_key;
        song_preview_active_path_.clear();
        song_preview_due_ns_ = now_ns + kPreviewDelayNs;
        song_preview_pending_ = true;
        if (was_active) {
            sync_menu_music();
        }
        return;
    }

    if (!song_preview_pending_ || now_ns < song_preview_due_ns_) {
        return;
    }

    song_preview_pending_ = false;
    song_preview_active_path_ = preview_path;
    const double gain = std::clamp(
        config_.audio_ui.master_volume * config_.audio_ui.bgm_volume, 0.0, 1.0);
    menu_music_.play_looping_file(song_preview_active_path_, gain);
}


void MenuApp::sync_menu_music() {
    if (screen_ == Screen::Gameplay) {
        menu_music_.stop();
        menu_music_scene_key_.clear();
        menu_music_scene_path_.clear();
        return;
    }

    if (screen_ == Screen::SongSelect && !song_preview_active_path_.empty()) {
        if (!config_.audio_ui.background_sound_enabled) {
            menu_music_.stop();
            return;
        }
        const double gain = std::clamp(
            config_.audio_ui.master_volume * config_.audio_ui.bgm_volume, 0.0, 1.0);
        menu_music_.play_looping_file(song_preview_active_path_, gain);
        return;
    }


    // Mainmusic filenames are stable scene slots. Numbered siblings such as
    // "Main Menu 2.mp3" are discovered automatically and rotate per visit.
    const auto resolve_variants = [](std::string_view filename) {
        std::vector<std::string> paths;
        const auto add_if_present = [&](std::string_view candidate) {
            std::string path = resolve_menu_music_file_path(candidate);
            if (!path.empty()) paths.push_back(std::move(path));
        };

        add_if_present(filename);
        const std::string base(filename);
        const std::size_t extension_offset = base.find_last_of('.');
        if (extension_offset == std::string::npos) return paths;

        const std::string stem = base.substr(0, extension_offset);
        const std::string extension = base.substr(extension_offset);
        for (int variant = 2; variant <= 64; ++variant) {
            add_if_present(stem + " " + std::to_string(variant) + extension);
        }
        return paths;
    };

    const auto select_scene_slot = [&](std::string_view scene_key,
                                       std::string_view primary,
                                       std::string_view fallback = {},
                                       std::string_view final_fallback = {}) {
        if (menu_music_scene_key_ == scene_key) {
            return menu_music_scene_path_;
        }

        std::vector<std::string> paths = resolve_variants(primary);
        if (paths.empty() && !fallback.empty()) paths = resolve_variants(fallback);
        if (paths.empty() && !final_fallback.empty()) paths = resolve_variants(final_fallback);

        menu_music_scene_key_ = std::string(scene_key);
        menu_music_scene_path_.clear();
        if (!paths.empty()) {
            std::size_t& cursor = menu_music_variant_cursors_[menu_music_scene_key_];
            menu_music_scene_path_ = paths[cursor % paths.size()];
            cursor = (cursor + 1) % paths.size();
        }
        return menu_music_scene_path_;
    };

    const bool options_screen =
        screen_ == Screen::OptionsHub ||
        screen_ == Screen::SettingsAudio ||
        screen_ == Screen::SettingsGraphics ||
        screen_ == Screen::SettingsSkins ||
        screen_ == Screen::SettingsInput ||
        screen_ == Screen::SettingsCalibration ||
        screen_ == Screen::ModeSelect ||
        screen_ == Screen::ModeMods ||
        screen_ == Screen::Keymap ||
        screen_ == Screen::KeymapConfirm ||
        screen_ == Screen::OnnxUpscalerConfirm ||
        screen_ == Screen::KeymapTest;

    std::string music_path;
    if (screen_ == Screen::Result) {
        const bool failed = last_game_over_ || last_clear_status_ == "FAILED";
        music_path = failed
                         ? select_scene_slot("result_failed", "Failed.mp3", "Song Selecte.mp3", "Main Menu.mp3")
                         : select_scene_slot("result_clear", "Clear.mp3", "Song Selecte.mp3", "Main Menu.mp3");
    } else if (screen_ == Screen::Multiplayer) {
        music_path = select_scene_slot("multiplayer", "Multiplayer Lobby.mp3", {}, "Main Menu.mp3");
    } else if (options_screen) {
        const std::string_view fallback =
            submenu_return_screen_ == Screen::SongSelect ? "Song Selecte.mp3" : "Main Menu.mp3";
        music_path = select_scene_slot("options", "Options.mp3", fallback, "Main Menu.mp3");
    } else if (screen_ == Screen::SongSelect || screen_ == Screen::SongBrowser) {
        music_path = select_scene_slot("song_select", "Song Selecte.mp3", "Song Select.mp3", "Main Menu.mp3");
    } else if (screen_ == Screen::QuickSetup) {
        music_path = select_scene_slot("quick_setup", "Main Menu.mp3");
    } else {
        music_path = select_scene_slot("title", "Main Menu.mp3");
    }

    if (music_path.empty()) {
        menu_music_.stop();
        return;
    }

    if (!config_.audio_ui.background_sound_enabled) {
        menu_music_.stop();
        return;
    }

    const double gain = std::clamp(config_.audio_ui.master_volume * config_.audio_ui.bgm_volume, 0.0, 1.0);
    menu_music_.play_looping_file(music_path, gain);
}

void MenuApp::populate_quick_setup_render_data(render::MenuRenderData& render) {
    const auto setup_entry = profile_setup::entry(first_run_profile_);
    const bool first_run = setup_entry == profile_setup::Entry::FirstRun;
    render.kind = render::MenuScreenKind::GenericList;
    render.generic.heading = first_run ? ui_text("Quick Setup", "빠른 설정")
                                       : ui_text("Profile Setup", "프로필 설정");

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
                    ui_text("Visual Latency", "비주얼 레이턴시"),
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
    std::string profile_backend_value = config_.input.rawinput ? "RawInput" : "Polling";
    if (config_.input.rawinput && input_backend_fallback_policy_.polling_latched()) {
        profile_backend_value += " (active: Polling)";
    }
    append_menu_row(render.generic,
                    ui_text("Input Backend", "입력 백엔드"),
                    profile_backend_value,
                    settings_cursor_ == profile_setup::kBackendRow,
                    render::MenuHitTargetKind::SettingsRow,
                    profile_setup::kBackendRow,
                    false,
                    true);
    append_menu_row(render.generic,
                    ui_text("Nickname", "닉네임"),
                    profile_display_name() + (profile_nickname_edit_active_ ? " _" : ""),
                    settings_cursor_ == profile_setup::kNicknameRow,
                    render::MenuHitTargetKind::SettingsRow,
                    profile_setup::kNicknameRow,
                    true,
                    false);
    append_menu_row(render.generic,
                    ui_text("Avatar Image", "프로필 사진"),
                    config_.ui.profile_avatar_path.empty()
                        ? ui_text("Select PNG/JPG", "PNG/JPG 선택")
                        : safe_ui_text(filename_only(config_.ui.profile_avatar_path), "<image>"),
                    settings_cursor_ == profile_setup::kAvatarRow,
                    render::MenuHitTargetKind::SettingsRow,
                    profile_setup::kAvatarRow,
                    true,
                    false);
    append_menu_row(render.generic,
                    ui_text("Clear Avatar", "프로필 사진 지우기"),
                    config_.ui.profile_avatar_path.empty() ? ui_text("Not set", "설정 안 됨") : ui_text("Ready", "사용 중"),
                    settings_cursor_ == profile_setup::kClearAvatarRow,
                    render::MenuHitTargetKind::SettingsRow,
                    profile_setup::kClearAvatarRow,
                    !config_.ui.profile_avatar_path.empty(),
                    false);
    append_menu_row(render.generic,
                    first_run ? ui_text("Continue to Song Select", "곡 선택으로 계속")
                              : ui_text("Done", "완료"),
                    "",
                    settings_cursor_ == profile_setup::kDoneRow,
                    render::MenuHitTargetKind::SettingsRow,
                    profile_setup::kDoneRow,
                    true,
                    false);
    if (first_run) {
        append_menu_row(render.generic,
                        ui_text("Skip to Title", "타이틀로 건너뛰기"),
                        "",
                        settings_cursor_ == profile_setup::kFirstRunSkipRow,
                        render::MenuHitTargetKind::SettingsRow,
                        profile_setup::kFirstRunSkipRow,
                        true,
                        false);
        render.generic.notes.push_back(ui_text("First launch detected. TenRiff already created a default profile and default keymap.",
                                               "첫 실행을 감지했습니다. TenRiff가 기본 프로필과 기본 키 설정을 만들었습니다."));
    } else {
        render.generic.notes.push_back(ui_text("Active profile: ", "현재 프로필: ") + safe_ui_text(options_.profile));
        render.generic.notes.push_back(ui_text("Changes on this screen are saved immediately to the active profile.",
                                               "이 화면의 변경 사항은 현재 프로필에 즉시 저장됩니다."));
    }
    render.generic.notes.push_back(ui_text("Nickname is shown in saved records and multiplayer. Avatar Image accepts local PNG/JPG files.",
                                           "닉네임은 저장 기록과 멀티플레이에 표시됩니다. 프로필 사진은 로컬 PNG/JPG 파일을 사용합니다."));
    render.generic.notes.push_back(ui_text("Recommended start: Gauge Normal, Rate 1.00x, Visual Latency 0ms, BMS Keysound Follow.",
                                           "권장 시작값: 노말 게이지, Rate 1.00x, 비주얼 레이턴시 0ms, BMS 키음 연동."));
    render.generic.notes.push_back(ui_text("Songs Folder opens a picker on Enter or F2. You can also drag and drop a folder later.",
                                           "곡 폴더는 Enter 또는 F2로 선택 창을 엽니다. 나중에 폴더를 드래그 앤 드롭해도 됩니다."));
    render.generic.notes.push_back(ui_text("You can keep adjusting these later from Song Select > Options and Mode Settings.",
                                           "이 값들은 나중에 Song Select > 옵션과 모드 설정에서도 계속 조정할 수 있습니다."));
}

void MenuApp::populate_title_render_data(render::MenuRenderData& render,
                                         const std::string& current_track,
                                         const MenuApp::BestResultRecord& current_best) {
    render.kind = render::MenuScreenKind::TitleMenu;
    render.title.profile = profile_display_name();
    render.title.track = current_track;
    render.title.high_score = current_best.has_value ? current_best.best_score : 0;
    const bool no_songs_indexed = visible_song_count() == 0;
    render.title.buttons = {
        render::MenuButtonData{no_songs_indexed ? ui_text("ADD SONGS FOLDER", "곡 폴더 추가")
                                                : ui_text("PLAY", "플레이"),
                               no_songs_indexed ? u8"＋" : u8"▶",
                               title_cursor_ == 0},
        render::MenuButtonData{ui_text("MULTIPLAYER", "멀티플레이"), "P2P", title_cursor_ == 1},
        render::MenuButtonData{ui_text("OPTIONS", "옵션"), u8"⚙", title_cursor_ == 2},
        render::MenuButtonData{ui_text("EXIT", "종료"), u8"⏻", title_cursor_ == 3},
    };
    render.title.guides = {
        ui_text("UP / DOWN or mouse to move", "위 / 아래 또는 마우스로 이동"),
        ui_text("ENTER or double-click to open", "ENTER 또는 더블클릭으로 열기"),
        no_songs_indexed
            ? ui_text("PLAY becomes Add Songs Folder until a library is indexed",
                      "라이브러리가 생기기 전까지 PLAY가 곡 폴더 추가로 바뀝니다")
            : ui_text("F2 selects a songs folder; -/+ adjusts Rate", "F2로 곡 폴더 선택, -/+로 배속 조절"),
        ui_text("F5 refreshes the current song source", "F5로 현재 곡 소스를 새로고침"),
        ui_text("F1 opens the control help overlay", "F1로 조작 도움말 열기"),
        ui_text("ESC exits from the title menu", "ESC로 타이틀 메뉴 종료"),
    };
    render.title.guides.push_back(current_input_backend_status_label());
    if (const std::string detail = current_input_backend_status_detail(); !detail.empty()) {
        render.title.guides.push_back(detail);
    }
}

void MenuApp::populate_result_render_data(render::MenuRenderData& render, const std::string& current_track) {
    constexpr int kResultTimingGuidanceMinSignedSamples = 16;
    constexpr int kResultTimingGuidanceDominantNumerator = 6;
    constexpr int kResultTimingGuidanceDominantDenominator = 10;
    constexpr double kResultTimingGuidanceMinMeanMs = 2.0;
    constexpr double kResultTimingGuidanceFallbackMeanMs = 6.0;

    render.kind = render::MenuScreenKind::ResultScreen;
    render.result.peer_battle = last_game_was_multiplayer_;
    render.result.profile_avatar_path = config_.ui.profile_avatar_path;
    render.result.presentation_start_ns = result_presentation_start_ns_;
    render.result.presentation_skipped = result_presentation_skipped_;
    render.result.profile =
        last_result_player_name_.empty() ? profile_display_name() : last_result_player_name_;
    render.result.track = last_chart_title_.empty() ? current_track : last_chart_title_;
    render.result.title = last_chart_title_.empty() ? ui_text("Unknown Chart", "알 수 없는 차트") : last_chart_title_;
    render.result.artist = last_chart_artist_;
    if (last_chart_entry_valid_) {
        render.result.background_path =
            song_background_preview_path_for_entry(last_chart_entry_);
        render.result.chart_name = last_chart_entry_.chart_name;
        render.result.layout_label = last_chart_entry_.layout_label;
        render.result.key_count = last_chart_entry_.key_count;
        render.result.level = last_chart_entry_.level;
        render.result.native_level = last_chart_entry_.native_level;
        render.result.rating = last_chart_entry_.rating;
        render.result.bpm = last_chart_entry_.bpm;
        if (!last_chart_entry_.difficulty_table_level.empty()) {
            render.result.difficulty_table_label =
                last_chart_entry_.difficulty_table_symbol +
                last_chart_entry_.difficulty_table_level;
            if (!last_chart_entry_.difficulty_table_name.empty()) {
                render.result.difficulty_table_label += " / " +
                    last_chart_entry_.difficulty_table_name;
            }
        }
    } else if (!last_chart_path_.empty()) {
        render.result.background_path =
            menu_songs::resolve_song_background_preview_path(last_chart_path_);
        render.result.bpm = last_chart_bpm_;
    }

    if (!has_result_) {
        render.result.notes.push_back(ui_text("No result data is available for this run.", "이번 플레이의 결과 데이터가 없습니다."));
        render.result.notes.push_back(ui_text("Left restarts the same chart. Enter or Esc returns to Song Select.",
                                              "Left로 같은 차트를 재시작합니다. Enter 또는 Esc로 Song Select로 돌아갑니다."));
        return;
    }

    const int judged = menu_records::judged_total(last_result_.counts);
    const int total_notes = (last_result_.total_notes > 0) ? last_result_.total_notes : judged;
    const double accuracy = menu_records::calculate_accuracy(last_result_);

    const game::GaugeType final_gauge_type = gauge_type_from_mode_string(last_final_gauge_);

    render.result.rank = last_clear_status_ == "AUTOPLAY"
                             ? "AUTO"
                             : menu_records::calculate_rank(last_result_, last_game_over_);
    render.result.status = !last_clear_status_.empty() ? last_clear_status_
                                                       : (last_game_over_ ? "GAME OVER" : "CLEAR");
    render.result.gauge_label = gauge_type_label(final_gauge_type);
    render.result.cleared = !last_game_over_ &&
                            !menu_records::clear_status_is_autoplay(last_clear_status_);
    render.result.score = last_result_final_score_;
    render.result.detail_score = menu_records::calculate_detail_score(last_result_);
    render.result.pause_used = last_pause_used_;
    render.result.accuracy = accuracy;
    render.result.detailed_accuracy = menu_records::calculate_detailed_accuracy(last_result_);
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
    const int combo_target =
        last_result_.total_combo_steps > 0 ? last_result_.total_combo_steps : total_notes;
    render.result.full_combo =
        render.result.cleared && combo_target > 0 &&
        last_result_.max_combo >= combo_target && last_result_.counts.bd == 0;
    render.result.all_perfect =
        render.result.cleared && judged > 0 &&
        last_result_.counts.pg == judged && last_result_.counts.gr == 0 &&
        last_result_.counts.gd == 0 && last_result_.counts.bd == 0 &&
        last_result_.counts.pr == 0;
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
    if (render.result.peer_battle) {
        render.result.replay_available = false;
    }

    const int positive_delta_count = last_result_.positive_delta_count;
    const int negative_delta_count = last_result_.negative_delta_count;
    const int signed_delta_count = positive_delta_count + negative_delta_count;
    int timing_guidance_direction = 0;
    if (signed_delta_count >= kResultTimingGuidanceMinSignedSamples &&
        std::abs(last_result_.mean_delta_ms) >= kResultTimingGuidanceMinMeanMs) {
        const int dominant_count = (std::max)(positive_delta_count, negative_delta_count);
        if (dominant_count * kResultTimingGuidanceDominantDenominator >=
            signed_delta_count * kResultTimingGuidanceDominantNumerator) {
            if (positive_delta_count > negative_delta_count && last_result_.mean_delta_ms >= 0.0) {
                timing_guidance_direction = 1;
            } else if (negative_delta_count > positive_delta_count && last_result_.mean_delta_ms <= 0.0) {
                timing_guidance_direction = -1;
            }
        }
    }
    if (timing_guidance_direction == 0 &&
        judged >= kResultTimingGuidanceMinSignedSamples &&
        std::abs(last_result_.mean_delta_ms) >= kResultTimingGuidanceFallbackMeanMs) {
        timing_guidance_direction = (last_result_.mean_delta_ms > 0.0) ? 1 : -1;
    }
    render.result.timing_guidance_visible = timing_guidance_direction != 0;
    render.result.timing_guidance_direction = timing_guidance_direction;
    if (render.result.timing_guidance_visible) {
        const std::string bias_summary =
            (signed_delta_count > 0)
                ? ("+" + std::to_string(positive_delta_count) + " / -" + std::to_string(negative_delta_count))
                : format_signed_offset_ms(last_result_.mean_delta_ms);
        if (timing_guidance_direction > 0) {
            render.result.timing_guidance_title =
                ui_text("Timing Advice ", "타이밍 알림 ") + "(" + bias_summary + ")";
            render.result.timing_guidance_message =
                ui_text("Step 1: Skins > Judge Line -> move '-' first.",
                        "1단계: Skins > 판정선 위치를 먼저 '-' 쪽으로 조절하세요.");
            render.result.timing_guidance_detail =
                ui_text("Step 2: If '+' still stays dominant, Skins > Visual Latency -> move '+'.",
                        "2단계: 그래도 '+' 쪽이 남으면 스킨 > 비주얼 레이턴시를 '+' 쪽으로 조절하세요.");
        } else {
            render.result.timing_guidance_title =
                ui_text("Timing Advice ", "타이밍 알림 ") + "(" + bias_summary + ")";
            render.result.timing_guidance_message =
                ui_text("Step 1: Skins > Judge Line -> move '+' first.",
                        "1단계: Skins > 판정선 위치를 먼저 '+' 쪽으로 조절하세요.");
            render.result.timing_guidance_detail =
                ui_text("Step 2: If '-' still stays dominant, Skins > Visual Latency -> move '-'.",
                        "2단계: 그래도 '-' 쪽이 남으면 스킨 > 비주얼 레이턴시를 '-' 쪽으로 조절하세요.");
        }
    }

    if (!render.result.replay_file.empty()) {
        render.result.notes.push_back(ui_text("Replay: ", "리플레이: ") + render.result.replay_file);
    }
    if (!render.result.result_file.empty()) {
        render.result.notes.push_back(ui_text("Result: ", "결과 파일: ") + render.result.result_file);
    }
    render.result.notes.push_back(ui_text("Gameplay ", "게임플레이 ") +
                                  format_input_backend_status_label(last_gameplay_input_backend_state_,
                                                                    ui_uses_korean()));
    if (const std::string detail =
            format_input_backend_status_detail(last_gameplay_input_backend_state_, ui_uses_korean());
        !detail.empty()) {
        render.result.notes.push_back(ui_text("Gameplay ", "게임플레이 ") + detail);
    }
    if (render.result.peer_battle) {
        const network::PeerSessionSnapshot peer = peer_session_.snapshot();
        const std::string peer_label =
            peer.peer_name.empty() ? ui_text("Opponent", "상대") : peer.peer_name;
        render.result.peer_name = peer_label;
        if (peer.has_remote_score && peer.latest_remote_score.finished) {
            const network::PeerScore& peer_score = peer.latest_remote_score;
            const MultiplayerScoreComparison comparison =
                compare_multiplayer_scores(last_result_final_score_, peer_score.score);
            render.result.peer_result_available = true;
            render.result.peer_score = peer_score.score;
            render.result.peer_score_difference = comparison.difference;
            render.result.peer_gauge_value =
                std::clamp(static_cast<double>(peer_score.gauge_milli) / 1000.0, 0.0, 100.0);
            render.result.peer_max_combo = peer_score.max_combo;
            render.result.peer_perfect = peer_score.perfect;
            render.result.peer_great = peer_score.great;
            render.result.peer_good = peer_score.good;
            render.result.peer_bad = peer_score.bad;
            render.result.peer_poor = peer_score.poor;
            render.result.peer_status = peer_score.aborted
                                            ? "ABORTED"
                                            : (peer_score.game_over ? "GAME OVER" : "FINISHED");
            render.result.peer_outcome =
                comparison.outcome == MultiplayerScoreOutcome::Win
                    ? "WIN"
                    : (comparison.outcome == MultiplayerScoreOutcome::Loss ? "LOSE" : "DRAW");
        } else {
            render.result.peer_result_available = false;
            render.result.peer_status =
                peer.state == network::PeerSessionState::Connected ? "WAITING" : "DISCONNECTED";
            render.result.peer_outcome = "NO CONTEST";
        }
        std::vector<std::string> peer_notes;
        std::vector<const network::PeerParticipantSnapshot*> standings;
        standings.reserve(peer.participants.size());
        for (const auto& participant : peer.participants) {
            standings.push_back(&participant);
        }
        std::stable_sort(
            standings.begin(), standings.end(),
            [](const auto* lhs, const auto* rhs) {
                if (lhs->has_score != rhs->has_score) return lhs->has_score;
                if (lhs->has_score &&
                    lhs->latest_score.score != rhs->latest_score.score) {
                    return lhs->latest_score.score > rhs->latest_score.score;
                }
                return lhs->player_id < rhs->player_id;
            });
        for (std::size_t index = 0; index < standings.size(); ++index) {
            const auto& participant = *standings[index];
            std::string summary =
                std::to_string(index + 1) + ". #" +
                std::to_string(participant.player_id) + " " + participant.name;
            if (participant.local) summary += ui_text(" [YOU]", " [나]");
            if (participant.has_score) {
                summary += " / " + ui_text("Score ", "점수 ") +
                           std::to_string(participant.latest_score.score);
                summary += participant.latest_score.finished
                               ? ui_text(" / FINISHED", " / 종료")
                               : ui_text(" / PLAYING", " / 플레이 중");
            } else {
                summary += ui_text(" / RESULT WAIT", " / 결과 대기");
            }
            peer_notes.push_back(std::move(summary));
        }        peer_notes.push_back(ui_text("Enter or Esc returns to the multiplayer lobby.",
                                     "Enter 또는 Esc로 멀티플레이 로비로 돌아갑니다."));
        render.result.notes.insert(render.result.notes.begin(),
                                   std::make_move_iterator(peer_notes.begin()),
                                   std::make_move_iterator(peer_notes.end()));
    } else {
        render.result.notes.push_back(ui_text("Left restarts the same chart immediately.", "Left로 같은 차트를 즉시 재시작합니다."));
        if (render.result.replay_available) {
            render.result.notes.push_back(ui_text("F1 or click Replay watches the saved input trace.",
                                                  "F1 또는 Replay 클릭으로 저장된 입력 리플레이를 재생합니다."));
        } else if (!render.result.replay_file.empty()) {
            render.result.notes.push_back(ui_text("Replay file is missing or unavailable.", "리플레이 파일이 없거나 사용할 수 없습니다."));
        }
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
        append_menu_row(render.generic, ui_text("Profile Setup", "프로필 설정"), safe_ui_text(options_.profile), options_cursor_ == profile_setup::kOptionsProfileSetupRow, render::MenuHitTargetKind::OptionsItem, profile_setup::kOptionsProfileSetupRow, true, false);
        append_menu_row(render.generic, ui_text("Back", "뒤로"), "", options_cursor_ == profile_setup::kOptionsBackRow, render::MenuHitTargetKind::OptionsItem, profile_setup::kOptionsBackRow, true, false);
        render.generic.notes.push_back(ui_text("Up/Down to move, Enter to select, Esc to return.",
                                               "위/아래로 이동하고 Enter로 선택, Esc로 돌아갑니다."));
        render.generic.notes.push_back(ui_text("Use the listed rows directly here. F1 opens help, F2 opens the songs-folder picker, and F5 refreshes the library.",
                                               "여기서는 보이는 항목을 직접 여세요. F1은 도움말, F2는 곡 폴더 선택, F5는 라이브러리 새로고침입니다."));
    } else if (screen_ == Screen::Multiplayer) {
        populate_multiplayer_render_data(render);
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
    } else if (screen_ == Screen::OnnxUpscalerConfirm) {
        populate_onnx_upscaler_confirm_render_data(render);
    } else if (screen_ == Screen::KeymapTest) {
        populate_keymap_test_render_data(render);
    }

    if (screen_ != Screen::Keymap && screen_ != Screen::KeymapTest) {
        render.generic.footer_notes.push_back(current_input_backend_status_label());
        if (const std::string detail = current_input_backend_status_detail(); !detail.empty()) {
            render.generic.footer_notes.push_back(detail);
        }
    }
}

void MenuApp::publish_snapshot() {
    const int64_t publish_start_ns = timing::HighResClock::now_ns();
    if (screen_ == Screen::SongSelect) {
        sync_song_select_state();
    }
    service_song_preview();
    sync_menu_music();

    MenuSnapshot snapshot;
    render::MenuRenderData render;
    render.ui_korean = ui_uses_korean();
    render.ui_font = config::normalize_skin_ui_font_token(config_.skin.ui_font);
    if (config::normalize_skin_source_token(config_.skin.source) == "tenriff" &&
        active_tenriff_skin_.found) {
        render.lobby_skin.enabled = true;
        render.lobby_skin.background_path = active_tenriff_skin_.lobby_background_path;
        render.lobby_skin.logo_path = active_tenriff_skin_.lobby_logo_path;
        render.lobby_skin.background_opacity = active_tenriff_skin_.lobby_background_opacity;
        render.lobby_skin.layout_rects.reserve(active_tenriff_skin_.layout_rects.size());
        for (const auto& [key, rect] : active_tenriff_skin_.layout_rects) {
            render.lobby_skin.layout_rects.insert_or_assign(
                key, std::array<float, 4>{rect.left, rect.top, rect.right, rect.bottom});
        }
    }
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
        render.gameplay.scroll_speed =
            game::SpeedManager::scrollBps(last_chart_bpm_,
                                          render.gameplay.rate,
                                          render.gameplay.hispeed)
                .value_or(0.0);
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
            render_cache_.gameplay.scroll_speed =
                game::SpeedManager::scrollBps(render_cache_.gameplay.bpm,
                                              render_cache_.gameplay.rate,
                                              render_cache_.gameplay.hispeed)
                    .value_or(0.0);
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
                        render_cache_.gameplay.game_over &&
                            !render_cache_.gameplay.spectating_peer,
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

void MenuApp::launch_gameplay(const std::string& chart_path,
                              const std::string& replay_path,
                              GameplayLaunchKind launch_kind) {
    // Restart iteratively so each completed GameSession leaves the stack before relaunch.
    for (;;) {
    const bool replay_playback = !replay_path.empty();
    // The caller owns the launch intent. Inferring it from lobby flags makes a
    // stale multiplayer screen/result state capable of hijacking a local run.
    const bool peer_battle = gameplay_launch_uses_peer_battle(launch_kind, replay_playback);
    if (!peer_battle) {
        reset_multiplayer_for_single_player();
    }
    const std::string preserved_result_path = last_result_path_;
    const SongEntry* selected_entry =
        (selected_song_ >= 0 && selected_song_ < static_cast<int>(visible_song_count()))
            ? visible_song_entry(static_cast<std::size_t>(selected_song_))
            : nullptr;
    update_last_chart_metadata(chart_path, selected_entry);
    if (peer_battle && !last_chart_entry_valid_ && !multiplayer_chart_title_.empty()) {
        last_chart_title_ = multiplayer_chart_title_;
    }

    screen_ = Screen::Gameplay;
    last_gameplay_input_backend_state_ =
        input_backend_fallback_policy_.runtime_state(config_.input.rawinput);
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
    session.set_peer_battle_mode(peer_battle);
    session.set_input_backend_fallback_override(
        input_backend_fallback_policy_.runtime_state(config_.input.rawinput));
    session.set_peer_spectator_done_callback([this, peer_battle]() {
        if (!peer_battle) {
            return true;
        }
        const network::PeerSessionSnapshot peer = peer_session_.snapshot();
        const PeerBattleSpectatorState state{
            true,
            peer.state == network::PeerSessionState::Connected,
            peer.round_active,
            peer.has_remote_score && peer.latest_remote_score.finished,
            peer.has_remote_score && peer.latest_remote_score.game_over,
        };
        return peer_battle_spectator_decision(state) ==
               PeerBattleSpectatorDecision::FinishSession;
    });
    session.set_loading_progress_callback([this](const GameSession::LoadingProgress& progress) {
        update_gameplay_loading_state(progress.percent, progress.stage);
    });
    session.set_screenshot_callback([this]() {
        menu_window_.request_screenshot();
    });
#ifdef _WIN32
    auto escape_was_down = std::make_shared<std::atomic<bool>>((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0);
    session.set_loading_cancel_callback([this, escape_was_down, peer_battle]() {
        if (peer_battle) {
            const network::PeerSessionSnapshot peer = peer_session_.snapshot();
            if (peer.state != network::PeerSessionState::Connected || !peer.round_active) {
                return true;
            }
        }
        const bool escape_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        const bool fresh_press = escape_down && !escape_was_down->exchange(escape_down, std::memory_order_acq_rel);
        if (!fresh_press) return false;
        const HWND foreground = GetForegroundWindow();
        DWORD foreground_process_id = 0;
        if (foreground) GetWindowThreadProcessId(foreground, &foreground_process_id);
        return foreground_process_id == GetCurrentProcessId();
    });
#else
    session.set_loading_cancel_callback([]() { return false; });
#endif
    session.set_hud_callback([this, peer_battle](const GameSession::HudSnapshot& hud) {
        uint64_t peer_revision = 0;
        if (peer_battle) {
            network::PeerScore score;
            score.score = hud.score;
            score.current_sample = hud.current_sample;
            score.combo = hud.combo;
            score.max_combo = hud.max_combo;
            score.perfect = hud.counts.pg;
            score.great = hud.counts.gr;
            score.good = hud.counts.gd;
            score.bad = hud.counts.bd;
            score.poor = hud.counts.pr;
            score.gauge_milli = static_cast<int>(std::llround(std::clamp(hud.gauge, 0.0, 100.0) * 1000.0));
            // Live HUD frames must never become FinalScore. The authoritative
            // final packet is emitted once after GameSession shutdown/export.
            score.finished = false;
            score.game_over = hud.game_over;
            score.aborted = false;
            (void)peer_session_.publish_score(score, false);
            peer_revision = peer_session_.snapshot().revision;
        }
        std::unique_lock<std::mutex> lock(gameplay_hud_mutex_);
        const GameplayHudRevisionInput previous = gameplay_hud_revision_input(gameplay_hud_);


        gameplay_hud_.loading = false;
        gameplay_hud_.loading_percent = 100;
        gameplay_hud_.loading_stage = "Ready";
        gameplay_hud_.active = hud.active;
        gameplay_hud_.finished = hud.finished;
        gameplay_hud_.game_over = hud.game_over;
        gameplay_hud_.spectating_peer = hud.spectating_peer;
        gameplay_hud_.user_aborted = hud.user_aborted;
        gameplay_hud_.paused = hud.paused;
        gameplay_hud_.pause_menu_cursor = hud.pause_menu_cursor;
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
        gameplay_hud_.current_visual_position = hud.current_visual_position;
        gameplay_hud_.visual_velocity = hud.visual_velocity;
        gameplay_hud_.future_visual_span = hud.future_visual_span;
        gameplay_hud_.past_visual_span = hud.past_visual_span;
        gameplay_hud_.background_base_path = hud.background_base_path;
        gameplay_hud_.background_overlay_path = hud.background_overlay_path;
        gameplay_hud_.background_base_start_sample = hud.background_base_start_sample;
        gameplay_hud_.background_overlay_start_sample = hud.background_overlay_start_sample;
        gameplay_hud_.combo = hud.combo;
        gameplay_hud_.max_combo = hud.max_combo;
        gameplay_hud_.counts = hud.counts;
        gameplay_hud_.score = hud.score;
        gameplay_hud_.accuracy = hud.accuracy;
        gameplay_hud_.detailed_accuracy = hud.detailed_accuracy;
        gameplay_hud_.osu_od8_score_available = hud.osu_od8_score_available;
        gameplay_hud_.osu_od8_score = hud.osu_od8_score;
        gameplay_hud_.gauge = hud.gauge;
        gameplay_hud_.gauge_type = hud.gauge_type;
        gameplay_hud_.rate = hud.rate;
        gameplay_hud_.hispeed = hud.hispeed;
        gameplay_hud_.judgement_line_position = hud.judgement_line_position;
        gameplay_hud_.visual_offset_ms = hud.visual_offset_ms;
        gameplay_hud_.has_feedback = hud.has_feedback;
        gameplay_hud_.feedback = hud.feedback_judgement;
        gameplay_hud_.feedback_delta_ms = hud.feedback_delta_ms;
        gameplay_hud_.peer_revision = peer_revision;
        gameplay_hud_.timing_history_count = hud.timing_history_count;
        gameplay_hud_.timing_history_delta_ms.fill(0.0);
        std::copy_n(hud.timing_history_delta_ms.begin(),
                    hud.timing_history_count,
                    gameplay_hud_.timing_history_delta_ms.begin());
        gameplay_hud_.lane_activity_count = hud.lane_activity_count;
        gameplay_hud_.lane_activity.fill(0.0f);
        std::copy_n(hud.lane_activity.begin(), hud.lane_activity_count, gameplay_hud_.lane_activity.begin());

        gameplay_hud_.lane_pressed_count = hud.lane_pressed_count;
        gameplay_hud_.lane_pressed.fill(0);
        std::copy_n(hud.lane_pressed.begin(), hud.lane_pressed_count, gameplay_hud_.lane_pressed.begin());

        gameplay_hud_.note_count = hud.note_count;
        for (std::size_t i = 0; i < hud.note_count; ++i) {
            GameplayHudState::Note out;
            out.lane = hud.notes[i].lane;
            out.start_sample = hud.notes[i].start_sample;
            out.tail_sample = hud.notes[i].tail_sample;
            out.hold = hud.notes[i].hold;
            out.head_visible = hud.notes[i].head_visible;
            out.pending = hud.notes[i].pending;
            out.mine = hud.notes[i].mine;
            out.visual_position = hud.notes[i].visual_position;
            out.tail_visual_position = hud.notes[i].tail_visual_position;
            gameplay_hud_.notes[i] = out;
        }
        gameplay_hud_.ghost_visible = hud.ghost_visible;
        gameplay_hud_.ghost_score = hud.ghost_score;
        gameplay_hud_.ghost_accuracy = hud.ghost_accuracy;
        gameplay_hud_.ghost_detailed_accuracy = hud.ghost_detailed_accuracy;
        gameplay_hud_.ghost_osu_od8_score_available = hud.ghost_osu_od8_score_available;
        gameplay_hud_.ghost_osu_od8_score = hud.ghost_osu_od8_score;
        gameplay_hud_.ghost_combo = hud.ghost_combo;
        gameplay_hud_.ghost_max_combo = hud.ghost_max_combo;
        gameplay_hud_.ghost_counts = hud.ghost_counts;
        gameplay_hud_.ghost_gauge = hud.ghost_gauge;
        gameplay_hud_.ghost_gauge_type = hud.ghost_gauge_type;
        gameplay_hud_.ghost_has_feedback = hud.ghost_has_feedback;
        gameplay_hud_.ghost_feedback = hud.ghost_feedback_judgement;
        gameplay_hud_.ghost_feedback_delta_ms = hud.ghost_feedback_delta_ms;
        gameplay_hud_.ghost_timing_history_count = hud.ghost_timing_history_count;
        gameplay_hud_.ghost_timing_history_delta_ms.fill(0.0);
        std::copy_n(hud.ghost_timing_history_delta_ms.begin(),
                    hud.ghost_timing_history_count,
                    gameplay_hud_.ghost_timing_history_delta_ms.begin());
        gameplay_hud_.ghost_finished = hud.ghost_finished;
        gameplay_hud_.ghost_game_over = hud.ghost_game_over;
        gameplay_hud_.ghost_lane_activity_count = hud.ghost_lane_activity_count;
        gameplay_hud_.ghost_lane_activity.fill(0.0f);
        std::copy_n(hud.ghost_lane_activity.begin(),
                    hud.ghost_lane_activity_count,
                    gameplay_hud_.ghost_lane_activity.begin());
        gameplay_hud_.ghost_lane_pressed_count = hud.ghost_lane_pressed_count;
        gameplay_hud_.ghost_lane_pressed.fill(0);
        std::copy_n(hud.ghost_lane_pressed.begin(),
                    hud.ghost_lane_pressed_count,
                    gameplay_hud_.ghost_lane_pressed.begin());
        gameplay_hud_.ghost_note_count = hud.ghost_note_count;
        for (std::size_t i = 0; i < hud.ghost_note_count; ++i) {
            GameplayHudState::Note out;
            out.lane = hud.ghost_notes[i].lane;
            out.start_sample = hud.ghost_notes[i].start_sample;
            out.tail_sample = hud.ghost_notes[i].tail_sample;
            out.hold = hud.ghost_notes[i].hold;
            out.head_visible = hud.ghost_notes[i].head_visible;
            out.pending = hud.ghost_notes[i].pending;
            out.mine = hud.ghost_notes[i].mine;
            out.visual_position = hud.ghost_notes[i].visual_position;
            out.tail_visual_position = hud.ghost_notes[i].tail_visual_position;
            gameplay_hud_.ghost_notes[i] = out;
        }
        const GameplayHudRevisionInput next = gameplay_hud_revision_input(gameplay_hud_);
        const GameplayHudRevisionFlags diff = diff_gameplay_hud_revisions(previous, next);
        advance_gameplay_hud_revisions(gameplay_hud_, diff.motion_changed, diff.text_changed);
        lock.unlock();

        while (true) {
            auto click = menu_window_.poll_click_event();
            if (!click.has_value()) {
                break;
            }
            handle_menu_click(click.value());
        }
    });

    CommandLineOptions play_options = options_;
    play_options.chart_path = chart_path;
    play_options.replay_path = replay_path;
    if (!peer_battle && replay_path.empty() && config_.mode.ghost_battle_enabled) {
        play_options.ghost_replay_path = best_replay_path_for_selected_song();
    }
    if (!session.initialize(play_options)) {
        const bool loading_canceled = session.was_user_aborted();
        session.shutdown();
        last_gameplay_input_backend_state_ = session.input_backend_state();
        remember_input_backend_fallback(last_gameplay_input_backend_state_);
        if (!loading_canceled) {
            std::cerr << "[error] Failed to initialize gameplay session." << std::endl;
        }
        restart_input_thread();
        restart_audio_thread();
        {
            std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
            reset_gameplay_hud_state(gameplay_hud_);
        }
        if (peer_battle) {
            const network::PeerSessionSnapshot peer_after_loading = peer_session_.snapshot();
            const bool launch_canceled_cleanly =
                loading_canceled &&
                peer_after_loading.state == network::PeerSessionState::Connected &&
                !peer_after_loading.round_active;
            if (!launch_canceled_cleanly) {
                peer_session_.disconnect(loading_canceled ? "Peer canceled chart loading"
                                                          : "Peer failed to load the chart");
            }
            multiplayer_match_active_.store(false, std::memory_order_release);
            last_game_was_multiplayer_ = false;
            multiplayer_waiting_for_result_exit_ = false;
            multiplayer_status_message_ =
                launch_canceled_cleanly
                    ? ui_text("Match start canceled because Ready changed. Connection kept.",
                              "준비 상태가 바뀌어 시작을 취소했습니다. 연결은 유지됩니다.")
                    : loading_canceled
                    ? ui_text("Multiplayer loading was canceled; reconnect to try again.",
                              "멀티플레이 로딩을 취소했습니다. 다시 시도하려면 재연결하세요.")
                    : ui_text("This PC could not load the battle chart; reconnect after fixing it.",
                              "이 PC에서 대전 차트를 로드하지 못했습니다. 문제를 고친 뒤 재연결하세요.");
            screen_ = Screen::Multiplayer;
        } else {
            screen_ = Screen::SongSelect;
        }
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }

    if (peer_battle && !coordinate_multiplayer_start()) {
        session.shutdown();
        last_gameplay_input_backend_state_ = session.input_backend_state();
        remember_input_backend_fallback(last_gameplay_input_backend_state_);
        const network::PeerSessionSnapshot peer_after_sync = peer_session_.snapshot();
        const bool launch_canceled_cleanly =
            peer_after_sync.state == network::PeerSessionState::Connected &&
            !peer_after_sync.round_active;
        if (!launch_canceled_cleanly) {
            peer_session_.disconnect("Multiplayer start synchronization failed");
        } else {
            multiplayer_status_message_ =
                ui_text("Match start canceled because Ready changed. Connection kept.",
                        "준비 상태가 바뀌어 시작을 취소했습니다. 연결은 유지됩니다.");
        }
        multiplayer_match_active_.store(false, std::memory_order_release);
        last_game_was_multiplayer_ = false;
        multiplayer_waiting_for_result_exit_ = false;
        restart_input_thread();
        restart_audio_thread();
        {
            std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
            reset_gameplay_hud_state(gameplay_hud_);
        }
        screen_ = Screen::Multiplayer;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }

    session.run();
    const bool session_aborted = session.was_user_aborted();
    const bool session_restart_requested = session.was_restart_requested();
    const bool session_exit_requested = session.was_exit_requested();
    session.shutdown();
    last_gameplay_input_backend_state_ = session.input_backend_state();
    remember_input_backend_fallback(last_gameplay_input_backend_state_);

    const double session_hispeed = session.final_hispeed();
    const double session_judgement_line = session.final_judgement_line_position();
    const double session_visual_offset = session.final_visual_offset_ms();
    bool tuning_changed = false;
    if (std::abs(session_hispeed - config_.speed.hi_speed) > 0.0001) {
        config_.speed.hi_speed = session_hispeed;
        tuning_changed = true;
    }
    if (std::abs(session_judgement_line - config_.skin.judgement_line_position) > 0.0001) {
        config_.skin.judgement_line_position = session_judgement_line;
        tuning_changed = true;
    }
    if (std::abs(session_visual_offset - config_.visual_offset_ms) > 0.0001) {
        config_.visual_offset_ms = session_visual_offset;
        tuning_changed = true;
    }
    if (tuning_changed) {
        persist_runtime_config();
    }
    if (session_restart_requested) {
        {
            std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
            reset_gameplay_hud_state(gameplay_hud_);
            gameplay_hud_.loading = true;
            gameplay_hud_.loading_percent = 0;
            gameplay_hud_.loading_stage = "Restarting gameplay";
        }
        publish_snapshot();
        continue;
    }
    if (session_exit_requested) {
        restart_input_thread();
        restart_audio_thread();
        has_result_ = false;
        last_clear_status_.clear();
        last_final_gauge_ = "normal";
        last_result_mods_.clear();
        last_result_rate_multiplier_ = 1.0;
        last_result_score_multiplier_ = 1.0;
        last_result_final_score_ = 0;
        last_pause_used_ = false;
        last_result_player_name_.clear();
        last_replay_path_.clear();
        last_result_path_.clear();
        last_export_warnings_.clear();
        last_session_replay_playback_ = false;
        screen_ = Screen::SongSelect;
        apply_runtime_graphics_config();
        {
            std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
            reset_gameplay_hud_state(gameplay_hud_);
        }
        publish_snapshot();
        return;
    }
    const auto& result = session.result();
    if (peer_battle) {
        network::PeerScore final_score;
        final_score.finished = true;
        final_score.aborted = session_aborted || !result.has_value;
        if (result.has_value) {
            final_score.score = result.final_score;
            final_score.combo = result.stats.combo;
            final_score.max_combo = result.stats.max_combo;
            final_score.perfect = result.stats.counts.pg;
            final_score.great = result.stats.counts.gr;
            final_score.good = result.stats.counts.gd;
            final_score.bad = result.stats.counts.bd;
            final_score.poor = result.stats.counts.pr;
            final_score.game_over = result.game_over;
            if (!result.stats.gauge_history.empty()) {
                final_score.current_sample = result.stats.gauge_history.back().sample;
                final_score.gauge_milli = static_cast<int>(std::llround(
                    std::clamp(result.stats.gauge_history.back().value, 0.0, 100.0) * 1000.0));
            }
        }
        if (peer_session_.publish_score(final_score, true)) {
            (void)wait_for_multiplayer_result();
        } else {
            multiplayer_status_message_ =
                ui_text("Could not send the local final result.",
                        "내 최종 결과를 상대에게 전송하지 못했습니다.");
            peer_session_.disconnect("Local final result could not be sent");
        }
        multiplayer_match_active_.store(false, std::memory_order_release);
    }
    // Restart menu input only after the peer-result barrier. Otherwise keys
    // pressed while waiting accumulate in the blocked menu loop and can close
    // the comparison result as soon as it appears.
    restart_input_thread();
    restart_audio_thread();
    if (result.has_value) {
        last_result_ = result.stats;
        last_game_over_ = result.game_over;
        last_clear_status_ = result.clear_status;
        last_final_gauge_ = result.final_gauge;
        has_result_ = true;
        last_result_mods_ = result.mods;
        last_result_rate_multiplier_ = result.rate_multiplier;
        last_result_score_multiplier_ = result.score_multiplier;
        last_result_final_score_ = result.final_score;
        last_pause_used_ = result.pause_used;
        last_result_player_name_ =
            result.player_name.empty() ? profile_display_name() : result.player_name;
        last_replay_path_ = result.replay_path;
        last_result_path_ = (!result.result_path.empty() || !replay_playback) ? result.result_path : preserved_result_path;
        last_export_warnings_ = result.export_warnings;
        last_session_replay_playback_ = replay_playback;
        reload_chart_best_results();
        result_presentation_start_ns_ = timing::HighResClock::now_ns();
        result_presentation_skipped_ = false;
        screen_ = Screen::Result;
    } else {
        has_result_ = false;
        last_clear_status_.clear();
        last_final_gauge_ = "normal";
        last_result_mods_.clear();
        last_result_rate_multiplier_ = 1.0;
        last_result_score_multiplier_ = 1.0;
        last_result_final_score_ = 0;
        last_pause_used_ = false;
        last_result_player_name_.clear();
        last_replay_path_.clear();
        last_result_path_.clear();
        last_export_warnings_.clear();
        last_session_replay_playback_ = false;
        screen_ = peer_battle ? Screen::Multiplayer : Screen::SongSelect;
        if (peer_battle) {
            multiplayer_status_message_ =
                ui_text("Local play ended. Waiting for the peer before rematch.",
                        "로컬 플레이가 끝났습니다. 재대전 전에 상대를 기다리는 중입니다.");
        }
    }
    apply_runtime_graphics_config();
    {
        std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
        reset_gameplay_hud_state(gameplay_hud_);
    }
    publish_snapshot();
        return;
    }
}

std::string MenuApp::screen_title() const {
    switch (screen_) {
        case Screen::QuickSetup:
            return first_run_profile_ ? ui_text("Quick Setup", "빠른 설정")
                                      : ui_text("Profile Setup", "프로필 설정");
        case Screen::Title: return ui_text("Title", "타이틀");
        case Screen::OptionsHub: return ui_text("Options", "옵션");
        case Screen::Multiplayer: return ui_text("Peer Multiplayer", "P2P 멀티플레이");
        case Screen::SongSelect:
            if (song_select_view_ == SongSelectView::Sources) {
                return ui_text("Song Sources", "곡 소스");
            }
            if (song_select_view_ == SongSelectView::Records) {
                return ui_text("Local Records", "로컬 기록");
            }
            return ui_text("Song Select", "곡 선택");
        case Screen::SongBrowser: return ui_text("Song Filters", "곡 필터");
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
        case Screen::OnnxUpscalerConfirm:
            return ui_text("Enable ONNX Upscaler?", "ONNX 업스케일러를 켤까요?");
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
            if (first_run_profile_) {
                target.title = ui_text("Quick Setup", "빠른 설정");
                target.lines = {
                    ui_text("TenRiff already created a default profile and default keymap for this first launch.",
                            "TenRiff가 첫 실행용 기본 프로필과 기본 키 설정을 만들었습니다."),
                    ui_text("Songs Folder opens a picker on Enter or F2. You can also drag and drop a folder later.",
                            "곡 폴더는 Enter 또는 F2로 선택 창을 엽니다. 나중에 폴더를 드래그 앤 드롭해도 됩니다."),
                    ui_text("Recommended starting values are Gauge Normal, Rate 1.00x, Visual Latency 0ms, and BMS Keysound Follow.",
                            "권장 시작값은 노말 게이지, Rate 1.00x, 비주얼 레이턴시 0ms, BMS 키음 연동입니다."),
                    ui_text("Left / Right changes the highlighted setting. Continue opens Song Select.",
                            "좌우 키로 선택된 설정을 바꾸고, 계속을 누르면 곡 선택으로 이동합니다."),
                };
                target.footer = ui_text("Esc or Backspace skips to the title screen. Press F1 again to close help.",
                                        "Esc 또는 Backspace로 타이틀 화면으로 건너뜁니다. 도움말을 닫으려면 F1을 다시 누르세요.");
            } else {
                target.title = ui_text("Profile Setup", "프로필 설정");
                target.lines = {
                    ui_text("This screen edits the active profile shown at the top of the list.",
                            "이 화면은 목록 위에 표시된 현재 프로필을 편집합니다."),
                    ui_text("Songs Folder, Gauge, Rate, Visual Latency, and BMS Keysound are saved immediately.",
                            "곡 폴더, 게이지, Rate, 비주얼 레이턴시, BMS 키음 설정은 즉시 저장됩니다."),
                    ui_text("Use Keymap and the other Options screens for the remaining profile settings.",
                            "나머지 프로필 설정은 키 설정과 다른 옵션 화면에서 조정할 수 있습니다."),
                };
                target.footer = ui_text("Done, Esc, or Backspace returns to Options. Press F1 again to close help.",
                                        "완료, Esc 또는 Backspace로 옵션에 돌아갑니다. 도움말을 닫으려면 F1을 다시 누르세요.");
            }
            return;
        case Screen::Title:
            target.title = ui_text("Title Controls", "타이틀 조작");
            target.lines = {
                ui_text("Up / Down or the mouse selects PLAY, MULTIPLAYER, OPTIONS, or EXIT.",
                        "위 / 아래 키 또는 마우스로 PLAY, MULTIPLAYER, OPTIONS, EXIT를 선택합니다."),
                ui_text("Enter or double-click opens the selected button.",
                        "Enter 또는 더블클릭으로 선택한 버튼을 엽니다."),
                ui_text("If no songs are indexed yet, PLAY becomes Add Songs Folder so recovery stays visible on the first screen.",
                        "아직 인덱싱된 곡이 없으면 PLAY가 곡 폴더 추가로 바뀌어 첫 화면에서 바로 복구할 수 있습니다."),
                ui_text("F2 opens the songs-folder picker, -/+ adjusts Rate, and F5 refreshes the source.",
                        "F2는 곡 폴더 선택, -/+는 배속 조절, F5는 소스 새로고침입니다."),
            };
            target.footer = ui_text("Esc exits TenRiff. Press F1 again to close help.",
                                    "Esc로 TenRiff를 종료합니다. 도움말을 닫으려면 F1을 다시 누르세요.");
            return;
        case Screen::Multiplayer:
            target.title = ui_text("Peer Multiplayer", "P2P 멀티플레이");
            target.lines = {
                ui_text("Host runs the direct TCP room coordinator; up to seven others join its IP and port.",
                        "호스트는 TCP 방 코디네이터를 열고, 최대 7명이 호스트 IP와 포트로 연결합니다."),
                ui_text("Only byte-identical BMS charts owned by every player can be selected.",
                        "전원이 바이트 단위로 동일한 BMS를 가져야 선곡할 수 있습니다."),
                ui_text("The current leader starts after every player is ready. Esc disconnects and returns to Title.",
                        "전원 준비 후 현재 리더가 시작합니다. Esc는 연결을 끊고 타이틀로 돌아갑니다."),
                ui_text("Internet play needs router TCP forwarding and a firewall rule on the host.",
                        "인터넷 대전은 호스트 공유기의 TCP 포트 포워딩과 방화벽 허용이 필요합니다."),
            };
            target.footer = ui_text("This is direct-IP play for trusted peers; charts are not transferred.",
                                    "신뢰하는 사람들과 쓰는 직접 IP 연결이며 BMS 파일은 전송하지 않습니다.");
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
                ui_text("The left rail is now just Songs, Sources, Search, Filter, Records, and Options.",
                        "왼쪽 레일은 이제 Songs, Sources, Search, Filter, Records, Options만 남습니다."),
                ui_text("SEARCH filters title, artist, and path. FILTER now owns sort, group, key, difficulty, and collections.",
                        "SEARCH는 제목, 아티스트, 경로를 검색하고 FILTER는 정렬, 그룹, 키, 난이도, 컬렉션을 담당합니다."),
                ui_text("Backspace returns from Sources or Records to Songs. Esc returns to the title screen.",
                        "Backspace는 Sources나 Records에서 Songs로 돌아가고 Esc는 타이틀 화면으로 돌아갑니다."),
                ui_text("F2 chooses a songs folder. -/+ adjusts Rate. F5 refreshes the active source.",
                        "F2로 곡 폴더 선택, -/+로 배속 조절, F5로 활성 소스를 새로고침합니다."),
                ui_text("Safe indexing lowers RAM use for very large libraries. Fast rescans quicker on high-memory PCs.",
                        "안전 인덱싱은 매우 큰 라이브러리에서 RAM 사용량을 줄이고, 빠름은 메모리가 많은 PC에서 재스캔 속도를 높입니다."),
            };
            target.footer = ui_text("Current source, group, filter, sort, and indexing profile stay visible on the Song Select screen.",
                                    "현재 소스, 그룹, 필터, 정렬, 인덱싱 프로필은 Song Select 화면에 계속 표시됩니다.");
            return;
        case Screen::SongBrowser:
            target.title = ui_text("Filter Help", "필터 도움말");
            target.lines = {
                ui_text("This screen now handles filters only. Search moved to the Song Select SEARCH item.",
                        "이 화면은 이제 필터 전용입니다. 검색은 Song Select의 SEARCH 항목으로 이동했습니다."),
                ui_text("Up / Down or the mouse wheel moves the selection. Long lists show a clickable scrollbar on the right.",
                        "위 / 아래 키 또는 마우스 휠로 선택을 이동합니다. 긴 목록은 오른쪽의 클릭 가능한 스크롤바를 표시합니다."),
                ui_text("Left / Right adjusts sort, group, key, difficulty, and collection filters in place.",
                        "좌 / 우 키로 정렬, 그룹, 키 수, 난이도, 컬렉션 필터를 이 화면에서 바로 조정합니다."),
                ui_text("Enter toggles collection membership, creates the next collection, clears filters, or goes back.",
                        "Enter로 컬렉션 토글, 다음 컬렉션 생성, 필터 초기화, 뒤로 가기를 실행합니다."),
            };
            target.footer = ui_text("Esc or Backspace returns to Song Select. Press F1 again to close help.",
                                    "Esc 또는 Backspace로 Song Select로 돌아갑니다. 도움말을 닫으려면 F1을 다시 누르세요.");
            return;
        case Screen::SettingsAudio:
            target.title = ui_text("Audio Settings", "오디오 설정");
            target.lines = {
                ui_text("Up / Down or the mouse wheel selects a row. Long lists show a clickable scrollbar on the right.",
                        "위 / 아래 키 또는 마우스 휠로 행을 선택합니다. 긴 목록은 오른쪽의 클릭 가능한 스크롤바를 표시합니다."),
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
                ui_text("Up / Down or the mouse wheel selects a row. Long lists show a clickable scrollbar on the right.",
                        "위 / 아래 키 또는 마우스 휠로 행을 선택합니다. 긴 목록은 오른쪽의 클릭 가능한 스크롤바를 표시합니다."),
                ui_text("Display, Resolution, Refresh Hz, and VSync apply live while you adjust them.",
                        "표시 모드, 해상도, 주사율, VSync는 조정 중에도 즉시 적용됩니다."),
                ui_text("Language switches the menu UI immediately. Visual Latency is in Skin Settings.",
                        "언어는 메뉴 UI에 즉시 반영되고, 비주얼 레이턴시는 스킨 설정에 있습니다."),
                ui_text("For Discord voice overlay, use Borderless or Windowed and pin the Voice widget at bottom-left.",
                        "Discord 음성 오버레이는 테두리 없음 또는 창 모드를 쓰고 Voice 위젯을 좌하단에 고정하세요."),
                ui_text("Esc or Backspace saves and returns.",
                        "Esc 또는 Backspace로 저장하고 돌아갑니다."),
            };
            target.footer = ui_text("Use this screen when notes feel visually early or late on your display.",
                                    "노트가 화면에서 너무 빠르거나 늦게 보일 때 이 화면에서 조정하세요.");
            return;
        case Screen::SettingsSkins:
            target.title = ui_text("Skin Settings", "스킨 설정");
            target.lines = {
                ui_text("Up / Down or the mouse wheel selects a row. Long skin lists have a clickable scrollbar on the right.",
                        "위 / 아래 키 또는 마우스 휠로 행을 선택합니다. 긴 스킨 목록은 오른쪽의 클릭 가능한 스크롤바를 표시합니다."),
                ui_text("Skin Source swaps between Native, TenRiff skin.json, and imported LR2 playskins.",
                        "스킨 소스는 Native, TenRiff skin.json, 가져온 LR2 플레이스킨을 전환합니다."),
                ui_text("Visual Latency keeps the existing visual-only offset and saved value.",
                        "비주얼 레이턴시는 기존 화면 전용 보정값과 저장값을 그대로 사용합니다."),
                ui_text("Import Skin accepts a TenRiff skin.json folder or an LR2 folder. Drag-and-drop also works.",
                        "스킨 가져오기는 TenRiff skin.json 또는 LR2 폴더를 받으며 드래그 앤 드롭도 지원합니다."),
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
                ui_text("Backend selects RawInput or Polling for this profile.",
                        "입력 백엔드는 이 프로필에서 사용할 RawInput 또는 Polling을 선택합니다."),
                ui_text("A confirmed failure stays on Polling; Left selects Polling and Right retries RawInput.",
                        "고장이 확인되면 Polling을 유지하며, 왼쪽은 Polling 선택, 오른쪽은 RawInput 재시도입니다."),
                ui_text("Polling Hz controls the Polling backend and gameplay note-key backup cadence.",
                        "Polling Hz는 폴링 백엔드와 플레이 중 노트 키 보조 감시 주기를 정합니다."),
                ui_text("Debounce filters switch chatter before gameplay receives duplicate presses.",
                        "디바운스는 플레이가 중복 입력을 받기 전에 스위치 채터링을 걸러냅니다."),
                ui_text("Esc or Backspace saves and returns.",
                        "Esc 또는 Backspace로 저장하고 돌아갑니다."),
            };
            target.footer = ui_text("Input changes apply after the input thread restarts when you leave the screen.",
                                    "화면을 나갈 때 입력 스레드를 다시 시작한 뒤 변경 사항이 적용됩니다.");
            return;
        case Screen::SettingsCalibration:
            target.title = ui_text("Calibration Wizard", "캘리브레이션 위저드");
            target.lines = {
                ui_text("Adjustment Step changes how much each Left / Right press moves the offset rows.",
                        "조정 단위는 좌 / 우 키 한 번에 오프셋이 얼마나 움직일지 정합니다."),
                ui_text("Input Offset changes judgement timing. Visual Latency changes only visuals.",
                        "입력 오프셋은 판정 타이밍을 바꾸고, 비주얼 레이턴시는 화면만 바꿉니다."),
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
                ui_text("Up / Down or the mouse wheel selects a row. Long lists show a clickable scrollbar on the right.",
                        "위 / 아래 키 또는 마우스 휠로 행을 선택합니다. 긴 목록은 오른쪽의 클릭 가능한 스크롤바를 표시합니다."),
                ui_text("Ghost Battle, Autoplay, Practice, Sudden Death, Gauge, Random, Mods, Rate, and Hi-Speed change the next-song compare/play feel.",
                        "고스트 배틀, 오토플레이, 연습 모드, 서든 데스, 게이지, 랜덤, 모드, Rate, Hi-Speed는 다음 곡의 비교/플레이 감각을 바꿉니다."),
                ui_text("Indexing Safe minimizes RAM for huge libraries. Fast spends more RAM to speed up rescans.",
                        "인덱싱 안전은 큰 라이브러리에서 RAM 사용을 줄이고, 빠름은 더 많은 RAM으로 재스캔을 빠르게 합니다."),
                ui_text("TenRiff 1.2.92 indexes and plays BMS-family charts only.",
                        "TenRiff 1.2.92는 BMS 계열 차트만 인덱싱하고 플레이합니다."),
                ui_text("Ghost Battle uses the selected chart's best compatible replay when one exists; turn it off to keep single-field play.",
                        "고스트 배틀은 선택한 차트의 호환되는 최고 리플레이가 있으면 사용하며, 끄면 단일 플레이 필드로 유지됩니다."),
                ui_text("Autoplay is saved as AUTOPLAY but never awards a clear. Practice remains an ASSIST clear; neither is used as a default ghost best.",
                        "오토플레이 결과는 AUTOPLAY로 저장되지만 클리어로 인정되지 않습니다. 연습 모드는 ASSIST 클리어로 남으며 둘 다 기본 고스트 최고 기록으로 쓰이지 않습니다."),
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
                ui_text("Left / Right on Key Mode switches among 4K-10K, 12K, 14K, and 16K layouts.",
                        "Key Mode에서 좌 / 우 키를 누르면 4K~10K, 12K, 14K, 16K 레이아웃을 바꿉니다."),
                ui_text("Captured keys save immediately. Reset also saves immediately, and NKRO Test stays visible as a normal button.",
                        "입력한 키는 즉시 저장됩니다. 초기화도 바로 저장되고 NKRO Test는 일반 버튼으로 계속 보입니다."),
                ui_text("When you opened Keymap from Song Select, the editor defaults to the selected chart's key mode.",
                        "Song Select에서 Keymap을 열면 선택한 차트의 키 모드가 기본 편집 대상으로 잡힙니다."),
            };
            target.footer = ui_text("Duplicate lane bindings are allowed. Esc or Backspace returns.",
                                    "같은 키를 여러 레인에 중복으로 배치할 수 있습니다. Esc 또는 Backspace로 돌아갑니다.");
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
                ui_text("Keep shortcuts surfaced here: use the listed rows, F1 for help, F2 for songs folder, and F5 for reindex.",
                        "여기서는 보이는 조작만 유지합니다. 표시된 행, F1 도움말, F2 곡 폴더, F5 재인덱스를 사용하세요."),
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
