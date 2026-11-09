bool GameSession::refresh_foreground_cache() {
    const bool foreground = process_owns_foreground_window();
    const int64_t now_ns = (std::max)(int64_t{1}, timing::HighResClock::now_ns());
    foreground_cache_.store((now_ns << 1) | (foreground ? 1 : 0), std::memory_order_release);
    return foreground;
}

bool GameSession::foreground_for_realtime(int64_t now_ns) const {
    const int64_t cached = foreground_cache_.load(std::memory_order_acquire);
    const int64_t cached_ns = cached >> 1;
    if (cached != 0 && now_ns >= cached_ns && now_ns - cached_ns <= kForegroundCacheMaxAgeNs) {
        return (cached & 1) != 0;
    }
    return process_owns_foreground_window();
}

void GameSession::flush_audio_timing_diagnostics() {
    if (!audio_timing_diagnostics_pending_.exchange(false, std::memory_order_acq_rel)) return;
    const int64_t write_ahead_frames =
        audio_timing_diagnostics_write_ahead_frames_.load(std::memory_order_acquire);
    const double write_ahead_ms =
        (sample_rate_ > 0)
            ? (static_cast<double>(write_ahead_frames) * 1000.0 /
               static_cast<double>(sample_rate_))
            : 0.0;
    std::cerr << "[info] Gameplay audio timing mode="
              << (config_.audio.backend == audio::AudioBackend::ASIO ? "ASIO" :
                  audio_thread_.is_exclusive() ? "exclusive" : "shared")
              << " sample_rate=" << sample_rate_
              << " buffer_frames=" << audio_thread_.buffer_frames()
              << " callback_frames=" << audio_timing_diagnostics_frames_.load(std::memory_order_acquire)
              << " write_ahead_frames=" << write_ahead_frames
              << " write_ahead_ms=" << write_ahead_ms
              << std::endl;
}

void GameSession::run() {
    auto next_hud_tick = std::chrono::steady_clock::now();
    // A coarse 2 ms sleep can round to a system tick and starve the 8 ms HUD.
    // Reuse one thread-owned timer without changing global timer resolution.
    timing::PreciseWait hud_wait;

    if (countdown_active_ && !stop_requested_.load(std::memory_order_acquire)) {
        countdown_started_ns_ = timing::HighResClock::now_ns();
        const int64_t countdown_duration_ns = kGameplayStartCountdownSeconds * 1'000'000'000LL;
        while (!stop_requested_.load(std::memory_order_acquire)) {
            if (finished_.load(std::memory_order_acquire)) {
                break;
            }

            process_countdown_input_queue();
            if (finished_.load(std::memory_order_acquire) || stop_requested_.load(std::memory_order_acquire)) {
                break;
            }
            const int64_t countdown_repeat_now_ns = timing::HighResClock::now_ns();
            service_control_repeats(countdown_repeat_now_ns, refresh_foreground_cache());

            const int64_t now_ns = timing::HighResClock::now_ns();
            const int64_t elapsed_ns = std::max<int64_t>(0, now_ns - countdown_started_ns_);
            const int64_t remaining_ns = std::max<int64_t>(0, countdown_duration_ns - elapsed_ns);
            if (remaining_ns <= 0) {
                countdown_active_ = false;
                countdown_value_ = 0;
                break;
            }

            countdown_value_ = std::clamp(
                static_cast<int>((remaining_ns + 999'999'999LL) / 1'000'000'000LL),
                1,
                static_cast<int>(kGameplayStartCountdownSeconds));

            const auto now = std::chrono::steady_clock::now();
            if (now >= next_hud_tick) {
                if (hud_callback_) {
                    hud_callback_(hud_snapshot());
                }
                next_hud_tick += std::chrono::milliseconds(kHudRefreshMs);
                if (next_hud_tick < now) {
                    next_hud_tick = now + std::chrono::milliseconds(kHudRefreshMs);
                }
            }

            hud_wait.wait_for_ns(2'000'000);
        }
    }

    if (!stop_requested_.load(std::memory_order_acquire) && !finished_.load(std::memory_order_acquire)) {
        rebaseline_gameplay_start_input_state(0);
        current_playback_sample_ = 0;
        startup_input_timing_anchor_ = {};
        audio_timing_diagnostics_logged_ = false;
        if (audio_thread_.start() != audio::AudioResult::Success) {
            audio_error_message_ = audio_thread_.error_message();
            if (audio_error_message_.empty()) audio_error_message_ = "Failed to start gameplay audio.";
            std::cerr << "[error] " << audio_error_message_ << std::endl;
            stop_requested_.store(true, std::memory_order_release);
            finished_.store(true, std::memory_order_release);
        } else {
            gameplay_started_ = true;
        }
    }

    while (!stop_requested_.load(std::memory_order_acquire)) {
        if (finished_.load(std::memory_order_acquire)) {
            break;
        }
        if (audio_thread_.has_runtime_error()) {
            audio_error_message_ = audio_thread_.error_message();
            if (audio_error_message_.empty()) audio_error_message_ = "The audio driver stopped. Reopen Audio Settings before retrying.";
            std::cerr << "[error] " << audio_error_message_ << std::endl;
            stop_requested_.store(true, std::memory_order_release);
            finished_.store(true, std::memory_order_release);
            break;
        }

        if (spectating_peer_.load(std::memory_order_acquire) &&
            peer_spectator_done_callback_ && peer_spectator_done_callback_()) {
            finished_.store(true, std::memory_order_release);
            break;
        }

        service_chart_audio_streaming(last_audio_sample_.load(std::memory_order_acquire));
        // Keep window-manager queries and console output off the audio thread.
        refresh_foreground_cache();
        flush_audio_timing_diagnostics();

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_hud_tick) {
            if (hud_callback_) {
                hud_callback_(hud_snapshot());
            }
            next_hud_tick += std::chrono::milliseconds(kHudRefreshMs);
            if (next_hud_tick < now) {
                next_hud_tick = now + std::chrono::milliseconds(kHudRefreshMs);
            }
        }

        hud_wait.wait_for_ns(2'000'000);
    }

    if (hud_callback_) {
        hud_callback_(hud_snapshot());
    }
}

void GameSession::set_hud_callback(HudCallback callback) {
    hud_callback_ = std::move(callback);
}

void GameSession::set_loading_progress_callback(LoadingProgressCallback callback) {
    loading_progress_callback_ = std::move(callback);
}

void GameSession::set_loading_cancel_callback(LoadingCancelCallback callback) {
    loading_cancel_callback_ = std::move(callback);
}

void GameSession::set_screenshot_callback(ScreenshotCallback callback) {
    screenshot_callback_ = std::move(callback);
}

void GameSession::set_peer_spectator_done_callback(PeerSpectatorDoneCallback callback) {
    peer_spectator_done_callback_ = std::move(callback);
}

void GameSession::set_control_input_callback(ControlInputCallback callback) {
    control_input_callback_ = std::move(callback);
}

void GameSession::report_loading_progress(int percent, std::string_view stage) {
    const int clamped_percent = std::clamp(percent, 0, 100);
    if (last_loading_percent_ == clamped_percent && last_loading_stage_ == stage) {
        return;
    }

    last_loading_percent_ = clamped_percent;
    last_loading_stage_ = std::string(stage);
    if (!loading_progress_callback_) {
        return;
    }

    LoadingProgress progress;
    progress.percent = clamped_percent;
    progress.stage = last_loading_stage_;
    loading_progress_callback_(progress);
}

bool GameSession::loading_cancel_requested() {
    if (user_aborted_.load(std::memory_order_acquire)) {
        return true;
    }
    if (!loading_cancel_callback_ || !loading_cancel_callback_()) {
        return false;
    }

    (void)user_aborted_.exchange(true, std::memory_order_acq_rel);
    return true;
}

GameSession::HudSnapshot GameSession::hud_snapshot() {
    HudSnapshot snapshot;
    snapshot.finished = finished_.load(std::memory_order_acquire);
    snapshot.spectating_peer = spectating_peer_.load(std::memory_order_acquire);
    snapshot.user_aborted = user_aborted_.load(std::memory_order_acquire);
    snapshot.paused = paused_.load(std::memory_order_acquire);
    snapshot.pause_menu_cursor = pause_menu_cursor_.load(std::memory_order_acquire);
    snapshot.sample_rate = sample_rate_;
    snapshot.hud_publish_time_ns = timing::HighResClock::now_ns();
    snapshot.countdown_active = countdown_active_;
    snapshot.countdown_value = countdown_value_;
    const int resume_countdown = resume_countdown_value_.load(std::memory_order_acquire);
    if (snapshot.paused && resume_countdown > 0) {
        snapshot.countdown_active = true;
        snapshot.countdown_value = resume_countdown;
    }

    for (;;) {
        const uint64_t begin = audio_timing_sequence_.load(std::memory_order_acquire);
        if ((begin & 1u) != 0u) {
            continue;
        }

        const AudioTimingState timing = last_audio_timing_.load();
        // Keep payload reads before the final sequence check, including on
        // platforms where relaxed atomic loads can be reordered.
        std::atomic_thread_fence(std::memory_order_acquire);
        const uint64_t end = audio_timing_sequence_.load(std::memory_order_acquire);
        if (begin != end) {
            continue;
        }

        // R5 publishes the same filtered base clock used by input mapping.
        // The audio write cursor and intentional input calibration stay separate.
        snapshot.current_sample = timing.playback_sample;
        snapshot.audio_sample_time_ns = timing.time_ns;
        snapshot.audio_clock_rate = timing.clock_rate;
        snapshot.audio_buffer_frames = timing.buffer_frames;
        break;
    }

    // Query the window manager before taking the lock shared with the audio
    // callback, and never while holding it.
    const int64_t probe_foreground_start_ns = RtLockProbe::begin();
    const bool owns_foreground = refresh_foreground_cache();
    RtLockProbe::end(rt_probe_.foreground_query, probe_foreground_start_ns);
    {
        const HudLockPriorityCeiling lock_ceiling;
        const int64_t probe_lock_start_ns = RtLockProbe::begin();
        std::lock_guard<std::mutex> lock(engine_mutex_);
        RtLockProbe::end(rt_probe_.hud_lock_wait, probe_lock_start_ns);
        const RtLockProbe::Scope probe_hold(rt_probe_.hud_lock_hold);
        if (!engine_) {
            return snapshot;
        }

        snapshot.active = true;
        snapshot.finished = snapshot.finished || engine_->is_finished();
        snapshot.game_over = engine_->is_game_over();
        snapshot.rate = config_.speed.rate;
        snapshot.hispeed = config_.speed.hi_speed;
        snapshot.judgement_line_position = config_.skin.judgement_line_position;
        snapshot.visual_offset_ms = config_.visual_offset_ms;
        snapshot.lane_count = std::max(1, engine_->lane_count());
        snapshot.scratch_lane_count = std::min(
            chart_.scratch_lanes.size(), snapshot.scratch_lanes.size());
        std::copy_n(chart_.scratch_lanes.begin(),
                    snapshot.scratch_lane_count,
                    snapshot.scratch_lanes.begin());
        snapshot.duration_samples = engine_->duration_samples();
        snapshot.riff_map = riff_map_;
        snapshot.riff_map_count = riff_map_count_;
        snapshot.riff_map_revision = riff_map_revision_;

        if (snapshot.current_sample < last_visual_cue_sample_) {
            // Playback can be rebased independently of the monotonic mixer/write
            // cursor. Notes retired by an earlier visual horizon must be scanned
            // again, just like BGA cues, for both player and ghost fields.
            hud_scan_start_ = 0;
            ghost_hud_scan_start_ = 0;
            hud_past_hold_indices_.clear();
            ghost_hud_past_hold_indices_.clear();
            hud_scan_cutoff_sample_ = (std::numeric_limits<int64_t>::min)();
            next_visual_cue_index_ = 0;
            current_background_base_path_.clear();
            current_background_overlay_path_.clear();
            current_background_base_start_sample_ = 0;
            current_background_overlay_start_sample_ = 0;
        }
        while (next_visual_cue_index_ < chart_.visual_cues.size() &&
               chart_.visual_cues[next_visual_cue_index_].start_sample <= snapshot.current_sample) {
            const auto& cue = chart_.visual_cues[next_visual_cue_index_++];
            if (const std::string* path = chart_.visual_asset_path(cue.asset_id)) {
                if (cue.layer == gameplay::VisualLayer::Overlay) {
                    current_background_overlay_path_ = *path;
                    current_background_overlay_start_sample_ = cue.start_sample;
                } else {
                    current_background_base_path_ = *path;
                    current_background_base_start_sample_ = cue.start_sample;
                }
            }
        }
        last_visual_cue_sample_ = snapshot.current_sample;
        snapshot.background_base_path = current_background_base_path_;
        snapshot.background_overlay_path = current_background_overlay_path_;
        snapshot.background_base_start_sample = current_background_base_start_sample_;
        snapshot.background_overlay_start_sample = current_background_overlay_start_sample_;

        const auto& stats = engine_->stats();
        snapshot.combo = stats.combo;
        snapshot.max_combo = stats.max_combo;
        const bool partial_chart = practice_start_seconds_ > 0.0 && !instant_restart_start_;
        const bool assisted = autoplay_enabled_ || auto_scratch_enabled_ || practice_no_fail_enabled_ ||
            (replay_playback_enabled_ &&
                (replay_source_.mode.autoplay_enabled || replay_source_.mode.practice_no_fail_enabled));
        snapshot.full_combo = gameplay_session_full_combo(engine_->is_finished(), snapshot.game_over,
            snapshot.user_aborted || (replay_playback_enabled_ && replay_source_.aborted),
            assisted || partial_chart, stats);
        snapshot.counts = stats.counts;
        snapshot.score = gameplay::scale_native_score(stats.raw_score, score_multiplier_);
        snapshot.auto_scratch_enabled = auto_scratch_enabled_;
        snapshot.auto_scratch_hide_lanes = auto_scratch_enabled_ && config_.mode.auto_scratch_hide_lanes;
        snapshot.pacemaker_mode = pacemaker_mode_;
        snapshot.pacemaker_target = pacemaker_mode_ == "accuracy"
            ? config_.mode.pacemaker_target_accuracy : static_cast<double>(config_.mode.pacemaker_target_score);
        snapshot.pacemaker_delta = pacemaker_live_delta(pacemaker_mode_, stats, score_multiplier_,
            config_.mode.pacemaker_target_accuracy, config_.mode.pacemaker_target_score);
        const gameplay::ResultStats* best_score_stats = score_reference_engine_ ? &score_reference_engine_->stats()
            : ghost_engine_ && score_comparison_compatible_ ? &ghost_engine_->stats() : nullptr;
        const double best_score_multiplier = score_reference_engine_
            ? score_reference_source_.score_multiplier : ghost_replay_source_.score_multiplier;
        // Local BEST permits pauses. Player and reference both freeze on the
        // logical chart clock, so pausing must not permanently hide this HUD.
        snapshot.score_comparison = live_score_comparison(stats, score_multiplier_,
            config_.skin.score_comparison_enabled && !assisted && !partial_chart && !replay_playback_enabled_ &&
                !snapshot.spectating_peer,
            best_score_stats, best_score_multiplier);
        snapshot.accuracy = stats.accuracy_percent();
        snapshot.detailed_accuracy = stats.detailed_accuracy_percent();
        snapshot.osu_od8_score_available = stats.osu_od8.available;
        snapshot.osu_od8_score = stats.osu_od8.score;

        const auto& gauge_state = engine_->gauge_state();
        snapshot.gauge = gauge_state.value;
        snapshot.gauge_type = gauge_state.type;

        const auto& feedback = engine_->live_feedback();
        const auto feedback_visible = [&](const gameplay::LiveJudgementFeedback& event) {
            const double duration_ms = is_successful_hit(event.judgement)
                ? kGameplayHitMotionMs : kHudFeedbackDisplayMs;
            return event.has_value && snapshot.current_sample - event.sample <=
                ms_to_samples(duration_ms, sample_rate_);
        };
        snapshot.has_feedback = feedback_visible(feedback);
        snapshot.feedback_judgement = feedback.judgement;
        snapshot.feedback_delta_ms = snapshot.has_feedback ? feedback.delta_ms : 0.0;
        const auto& timing = engine_->timing_text_feedback();
        snapshot.has_timing_feedback = gameplay::timing_text_feedback_visible(
            timing, snapshot.current_sample, sample_rate_);
        snapshot.has_retained_timing_feedback = snapshot.has_timing_feedback;
        snapshot.timing_feedback_delta_ms = timing.has_value ? timing.delta_ms : 0.0;
        snapshot.has_non_pg_feedback = gameplay::timing_feedback_visible(
            engine_->non_pg_feedback(), snapshot.current_sample, sample_rate_);
        snapshot.timing_maximum_window_ms = engine_->maximum_judgement_window_ms();
        engine_->collect_recent_timing_deltas(snapshot.timing_history_delta_ms, &snapshot.timing_history_count,
            snapshot.current_sample - ms_to_samples(700.0, sample_rate_), snapshot.current_sample, &snapshot.timing_history_maximums);
        engine_->collect_active_holds(active_holds_buffer_);
        if (!owns_foreground) {
            std::fill(lane_pressed_.begin(), lane_pressed_.end(), 0);
            std::fill(ghost_lane_pressed_.begin(), ghost_lane_pressed_.end(), 0);
        }
        snapshot.lane_activity.fill(0.0f);
        snapshot.lane_activity_count = std::min<std::size_t>(lane_activity_.size(), kGameplayHudMaxLanes);
        std::copy_n(lane_activity_.begin(), snapshot.lane_activity_count, snapshot.lane_activity.begin());
        snapshot.lane_hit_time_ns = lane_hit_time_ns_;
        snapshot.lane_pressed.fill(0);
        snapshot.lane_pressed_count = std::min<std::size_t>(lane_pressed_.size(), kGameplayHudMaxLanes);
        std::copy_n(lane_pressed_.begin(), snapshot.lane_pressed_count, snapshot.lane_pressed.begin());

        if (ghost_engine_) {
            snapshot.ghost_visible = !score_comparison_only_;
            snapshot.ghost_finished = ghost_engine_->is_finished();
            snapshot.ghost_game_over = ghost_engine_->is_game_over();

            const auto& ghost_stats = ghost_engine_->stats();
            snapshot.ghost_combo = ghost_stats.combo;
            snapshot.ghost_max_combo = ghost_stats.max_combo;
            snapshot.ghost_full_combo = gameplay_session_full_combo(snapshot.ghost_finished,
                snapshot.ghost_game_over, snapshot.user_aborted || ghost_replay_source_.aborted,
                partial_chart || auto_scratch_enabled_ || ghost_replay_source_.mode.autoplay_enabled ||
                    ghost_replay_source_.mode.practice_no_fail_enabled,
                ghost_stats);
            snapshot.ghost_counts = ghost_stats.counts;
            snapshot.ghost_score = ghost_stats.raw_score;
            snapshot.ghost_accuracy = ghost_stats.accuracy_percent();
            snapshot.ghost_detailed_accuracy = ghost_stats.detailed_accuracy_percent();
            snapshot.ghost_osu_od8_score_available = ghost_stats.osu_od8.available;
            snapshot.ghost_osu_od8_score = ghost_stats.osu_od8.score;

            const auto& ghost_gauge_state = ghost_engine_->gauge_state();
            snapshot.ghost_gauge = ghost_gauge_state.value;
            snapshot.ghost_gauge_type = ghost_gauge_state.type;

            const auto& ghost_feedback = ghost_engine_->live_feedback();
            snapshot.ghost_has_feedback = feedback_visible(ghost_feedback);
            snapshot.ghost_feedback_judgement = ghost_feedback.judgement;
            snapshot.ghost_feedback_delta_ms =
                snapshot.ghost_has_feedback ? ghost_feedback.delta_ms : 0.0;
            const auto& ghost_timing = ghost_engine_->timing_text_feedback();
            snapshot.ghost_has_timing_feedback = gameplay::timing_text_feedback_visible(
                ghost_timing, snapshot.current_sample, sample_rate_);
            snapshot.ghost_has_retained_timing_feedback = snapshot.ghost_has_timing_feedback;
            snapshot.ghost_timing_feedback_delta_ms = ghost_timing.has_value ? ghost_timing.delta_ms : 0.0;
            snapshot.ghost_has_non_pg_feedback = gameplay::timing_feedback_visible(
                ghost_engine_->non_pg_feedback(), snapshot.current_sample, sample_rate_);
            snapshot.ghost_timing_maximum_window_ms = ghost_engine_->maximum_judgement_window_ms();
            ghost_engine_->collect_recent_timing_deltas(snapshot.ghost_timing_history_delta_ms,
                                                        &snapshot.ghost_timing_history_count,
                snapshot.current_sample - ms_to_samples(700.0, sample_rate_), snapshot.current_sample, &snapshot.ghost_timing_history_maximums);
            ghost_engine_->collect_active_holds(ghost_active_holds_buffer_);
            snapshot.ghost_lane_activity.fill(0.0f);
            snapshot.ghost_lane_activity_count =
                std::min<std::size_t>(ghost_lane_activity_.size(), kGameplayHudMaxLanes);
            std::copy_n(ghost_lane_activity_.begin(),
                        snapshot.ghost_lane_activity_count,
                        snapshot.ghost_lane_activity.begin());
            snapshot.ghost_lane_hit_time_ns = ghost_lane_hit_time_ns_;
            snapshot.ghost_lane_pressed.fill(0);
            snapshot.ghost_lane_pressed_count =
                std::min<std::size_t>(ghost_lane_pressed_.size(), kGameplayHudMaxLanes);
            std::copy_n(ghost_lane_pressed_.begin(),
                        snapshot.ghost_lane_pressed_count,
                        snapshot.ghost_lane_pressed.begin());
        }

    const double scroll_scale =
        game::SpeedManager::visualScrollScale(snapshot.rate, snapshot.hispeed).value_or(3.0);
    const double normalized_scale = std::max(0.1, scroll_scale / 3.0);
    const double dynamic_lookahead_ms = std::clamp(
        static_cast<double>(kHudLookaheadMs) / normalized_scale,
        350.0,
        6000.0);

    const int64_t past_samples = ms_to_samples(static_cast<double>(kHudPastMs), sample_rate_);
    const int64_t reference_lookahead_samples = ms_to_samples(dynamic_lookahead_ms, sample_rate_);
    const double future_visual_span = gameplay_reference_visual_span(
        chart_base_bpm_, snapshot.rate, sample_rate_, reference_lookahead_samples);
    int64_t lookahead_samples = reference_lookahead_samples;
    if (!chart_.scroll_segments.empty()) {
        const int64_t visual_horizon_sample =
            chart_.sample_after_visual_travel(snapshot.current_sample, future_visual_span);
        lookahead_samples = std::max<int64_t>(0, visual_horizon_sample - snapshot.current_sample);
    }
    const GameplayHudWindow expanded_window = expand_gameplay_hud_window(
        sample_rate_,
        past_samples,
        lookahead_samples,
        config_.visual_offset_ms,
        kHudRenderSlackMs);
    snapshot.past_samples = past_samples;
    snapshot.lookahead_samples = lookahead_samples;
    snapshot.current_visual_position = chart_.visual_position_at(snapshot.current_sample);
    snapshot.visual_velocity = chart_.visual_velocity_at(snapshot.current_sample);
    snapshot.visual_scroll_segments = visual_scroll_segments_;
    // The same expanded time window and scroll map as notes keeps bars aligned
    // through rate changes, STOP, SCROLL and visual latency without new clocks.
    const auto measure_begin = std::lower_bound(chart_.measure_samples.begin(), chart_.measure_samples.end(),
        snapshot.current_sample - expanded_window.past_samples);
    const int64_t measure_end = snapshot.current_sample + expanded_window.lookahead_samples;
    for (auto it = measure_begin; it != chart_.measure_samples.end() && *it <= measure_end &&
         snapshot.measure_line_count < kGameplayHudMaxMeasureLines; ++it) {
        snapshot.measure_line_positions[snapshot.measure_line_count++] = chart_.visual_position_at(*it);
    }
    // Keep the field scale anchored to the starting BPM. Follow actual visual
    // travel through BPM, #SCROLL, STOP, and reverse segments when choosing the
    // HUD horizon so slow sections still enter from the same top edge without
    // normalizing their on-screen velocity.
    snapshot.future_visual_span = future_visual_span;
    snapshot.past_visual_span = gameplay_reference_visual_span(
        chart_base_bpm_, snapshot.rate, sample_rate_, past_samples);
    snapshot.lane_activity_count = std::max<std::size_t>(
        snapshot.lane_activity_count,
        std::min<std::size_t>(static_cast<std::size_t>(snapshot.lane_count), kGameplayHudMaxLanes));

    const int64_t cutoff_sample = snapshot.current_sample - expanded_window.past_samples;
    if (cutoff_sample < hud_scan_cutoff_sample_) {
        // A device rebase or a larger negative visual offset can bring retired
        // heads/tails back into view. Rebuild both fields from the same epoch.
        hud_scan_start_ = 0;
        ghost_hud_scan_start_ = 0;
        hud_past_hold_indices_.clear();
        ghost_hud_past_hold_indices_.clear();
    }
    hud_scan_cutoff_sample_ = cutoff_sample;
    const auto advance_head_scan = [&](std::size_t& cursor, std::vector<std::size_t>& past_holds) {
        past_holds.erase(std::remove_if(past_holds.begin(), past_holds.end(), [&](std::size_t index) {
            return note_is_expired_for_hud(chart_.notes[index], snapshot.current_sample,
                                           expanded_window.past_samples);
        }), past_holds.end());
        cursor = std::min(cursor, chart_.notes.size());
        while (cursor < chart_.notes.size() && chart_.notes[cursor].start_sample < cutoff_sample) {
            const auto& note = chart_.notes[cursor];
            if (note.end_sample.has_value() && *note.end_sample >= cutoff_sample)
                past_holds.push_back(cursor);
            ++cursor;
        }
    };
    advance_head_scan(hud_scan_start_, hud_past_hold_indices_);

    // Hidden-note flags and active holds must come from the same engine epoch.
    // Active holds are appended first so dense lookahead never crowds them out.
    snapshot.note_count = 0;
    for (const auto& hold : active_holds_buffer_) {
        if (snapshot.note_count >= kGameplayHudMaxNotes) {
            break;
        }
        if (!is_successful_hit(hold.head_judgement) || hold.lane <= 0 || hold.lane > snapshot.lane_count) {
            continue;
        }
        if (hold.end_sample < snapshot.current_sample - expanded_window.past_samples) {
            continue;
        }

        HudNote hud_note;
        hud_note.lane = hold.lane;
        hud_note.start_sample = snapshot.current_sample;
        hud_note.tail_sample = std::max(hold.end_sample, snapshot.current_sample);
        hud_note.hold = true;
        hud_note.visual_position = chart_.visual_position_at(hud_note.start_sample);
        hud_note.tail_visual_position = chart_.visual_position_at(hud_note.tail_sample);
        hud_note.head_visible = false;
        hud_note.pending = false;
        snapshot.notes[snapshot.note_count++] = hud_note;
    }

    const auto append_player_chart_note = [&](const gameplay::NoteEvent& note) {
        const bool hit = note.note_id < hidden_hit_note_ids_.size() && hidden_hit_note_ids_[note.note_id] != 0;
        // A judged release removes the engine hold immediately. Keep only its
        // visual body until the original tail passes, without restoring a head,
        // pending judgement, or the active-hold highlight.
        const bool released = hit && note.end_sample.has_value() &&
            std::none_of(active_holds_buffer_.begin(), active_holds_buffer_.end(), [&](const auto& hold) {
                return is_successful_hit(hold.head_judgement) &&
                       hold.lane == note.lane && hold.end_sample == *note.end_sample;
            });
        if (hit && !released) {
            return;
        }
        if (note.lane <= 0 || note.lane > snapshot.lane_count) {
            return;
        }

        HudNote hud_note;
        hud_note.lane = note.lane;
        hud_note.start_sample = note.start_sample;
        hud_note.tail_sample = note_visible_end_sample(note);
        hud_note.hold = note.end_sample.has_value();
        hud_note.visual_position = chart_.visual_position_at(hud_note.start_sample);
        hud_note.tail_visual_position = chart_.visual_position_at(hud_note.tail_sample);
        hud_note.head_visible = true;
        hud_note.pending = engine_->is_note_pending(note.lane, note.note_id);
        hud_note.released = released;
        snapshot.notes[snapshot.note_count++] = hud_note;
    };
    for (const auto index : hud_past_hold_indices_) {
        if (snapshot.note_count >= kGameplayHudMaxNotes) break;
        append_player_chart_note(chart_.notes[index]);
    }
    for (std::size_t i = hud_scan_start_;
         i < chart_.notes.size() && snapshot.note_count < kGameplayHudMaxNotes; ++i) {
        const auto& note = chart_.notes[i];
        if (note.start_sample > snapshot.current_sample + expanded_window.lookahead_samples) break;
        append_player_chart_note(note);
    }
    for (const auto& mine : chart_.mines) {
        if (snapshot.note_count >= kGameplayHudMaxNotes) {
            break;
        }
        if (mine.sample < snapshot.current_sample - expanded_window.past_samples) {
            continue;
        }
        if (mine.sample > snapshot.current_sample + expanded_window.lookahead_samples) {
            break;
        }
        if (mine.lane <= 0 || mine.lane > snapshot.lane_count ||
            !engine_->is_mine_pending(mine.lane, mine.mine_id)) {
            continue;
        }
        HudNote hud_note;
        hud_note.lane = mine.lane;
        hud_note.start_sample = mine.sample;
        hud_note.tail_sample = mine.sample;
        hud_note.mine = true;
        hud_note.visual_position = chart_.visual_position_at(mine.sample);
        hud_note.tail_visual_position = hud_note.visual_position;
        hud_note.pending = true;
        snapshot.notes[snapshot.note_count++] = hud_note;
    }

    if (snapshot.ghost_visible) {
        advance_head_scan(ghost_hud_scan_start_, ghost_hud_past_hold_indices_);

        snapshot.ghost_note_count = 0;
        for (const auto& hold : ghost_active_holds_buffer_) {
            if (snapshot.ghost_note_count >= kGameplayHudMaxNotes) {
                break;
            }
            if (!is_successful_hit(hold.head_judgement) || hold.lane <= 0 || hold.lane > snapshot.lane_count) {
                continue;
            }
            if (hold.end_sample < snapshot.current_sample - expanded_window.past_samples) {
                continue;
            }

            HudNote hud_note;
            hud_note.lane = hold.lane;
            hud_note.start_sample = snapshot.current_sample;
            hud_note.tail_sample = std::max(hold.end_sample, snapshot.current_sample);
            hud_note.hold = true;
            hud_note.visual_position = chart_.visual_position_at(hud_note.start_sample);
            hud_note.tail_visual_position = chart_.visual_position_at(hud_note.tail_sample);
            hud_note.head_visible = false;
            hud_note.pending = false;
            snapshot.ghost_notes[snapshot.ghost_note_count++] = hud_note;
        }

        const auto append_ghost_chart_note = [&](const gameplay::NoteEvent& note) {
            const bool hit = note.note_id < ghost_hidden_hit_note_ids_.size() && ghost_hidden_hit_note_ids_[note.note_id] != 0;
            const bool released = hit && note.end_sample.has_value() &&
                std::none_of(ghost_active_holds_buffer_.begin(), ghost_active_holds_buffer_.end(), [&](const auto& hold) {
                    return is_successful_hit(hold.head_judgement) &&
                           hold.lane == note.lane && hold.end_sample == *note.end_sample;
                });
            if (hit && !released) {
                return;
            }
            if (note.lane <= 0 || note.lane > snapshot.lane_count) {
                return;
            }

            HudNote hud_note;
            hud_note.lane = note.lane;
            hud_note.start_sample = note.start_sample;
            hud_note.tail_sample = note_visible_end_sample(note);
            hud_note.hold = note.end_sample.has_value();
            hud_note.visual_position = chart_.visual_position_at(hud_note.start_sample);
            hud_note.tail_visual_position = chart_.visual_position_at(hud_note.tail_sample);
            hud_note.head_visible = true;
            hud_note.pending = ghost_engine_->is_note_pending(note.lane, note.note_id);
            hud_note.released = released;
            snapshot.ghost_notes[snapshot.ghost_note_count++] = hud_note;
        };
        for (const auto index : ghost_hud_past_hold_indices_) {
            if (snapshot.ghost_note_count >= kGameplayHudMaxNotes) break;
            append_ghost_chart_note(chart_.notes[index]);
        }
        for (std::size_t i = ghost_hud_scan_start_;
             i < chart_.notes.size() && snapshot.ghost_note_count < kGameplayHudMaxNotes; ++i) {
            const auto& note = chart_.notes[i];
            if (note.start_sample > snapshot.current_sample + expanded_window.lookahead_samples) break;
            append_ghost_chart_note(note);
        }
        for (const auto& mine : chart_.mines) {
            if (snapshot.ghost_note_count >= kGameplayHudMaxNotes) {
                break;
            }
            if (mine.sample < snapshot.current_sample - expanded_window.past_samples) {
                continue;
            }
            if (mine.sample > snapshot.current_sample + expanded_window.lookahead_samples) {
                break;
            }
            if (mine.lane <= 0 || mine.lane > snapshot.lane_count ||
                !ghost_engine_->is_mine_pending(mine.lane, mine.mine_id)) {
                continue;
            }
            HudNote hud_note;
            hud_note.lane = mine.lane;
            hud_note.start_sample = mine.sample;
            hud_note.tail_sample = mine.sample;
            hud_note.mine = true;
            hud_note.visual_position = chart_.visual_position_at(mine.sample);
            hud_note.tail_visual_position = hud_note.visual_position;
            hud_note.pending = true;
            snapshot.ghost_notes[snapshot.ghost_note_count++] = hud_note;
        }

    }
    }

    std::sort(snapshot.notes.begin(),
              snapshot.notes.begin() + static_cast<std::ptrdiff_t>(snapshot.note_count),
              [](const HudNote& lhs, const HudNote& rhs) {
                  if (lhs.start_sample != rhs.start_sample) {
                      return lhs.start_sample < rhs.start_sample;
                  }
                  if (lhs.tail_sample != rhs.tail_sample) {
                      return lhs.tail_sample < rhs.tail_sample;
                  }
                  return lhs.lane < rhs.lane;
              });

    if (snapshot.ghost_visible) {
        std::sort(snapshot.ghost_notes.begin(),
                  snapshot.ghost_notes.begin() + static_cast<std::ptrdiff_t>(snapshot.ghost_note_count),
                  [](const HudNote& lhs, const HudNote& rhs) {
                      if (lhs.start_sample != rhs.start_sample) {
                          return lhs.start_sample < rhs.start_sample;
                      }
                      if (lhs.tail_sample != rhs.tail_sample) {
                          return lhs.tail_sample < rhs.tail_sample;
                      }
                      return lhs.lane < rhs.lane;
                  });
    }

    snapshot.key_overlay = key_overlay_.snapshot(snapshot.hud_publish_time_ns,
        !snapshot.paused && !snapshot.countdown_active && !snapshot.finished &&
        !snapshot.spectating_peer && !replay_playback_enabled_ && !autoplay_enabled_ && owns_foreground);
    return snapshot;
}

