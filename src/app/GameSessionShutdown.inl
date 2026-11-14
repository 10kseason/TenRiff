void GameSession::shutdown() {
    stop_requested_.store(true, std::memory_order_release);
    // A final callback can finish the chart and fail in the same buffer. Wait
    // for driver callbacks before latching the error, and before destroying the
    // backend or deciding whether this session may export a score.
    audio_thread_.stop();
    if (audio_thread_.has_runtime_error()) {
        audio_error_message_ = audio_thread_.error_message();
        if (audio_error_message_.empty()) audio_error_message_ = "The audio driver stopped unexpectedly.";
    }
    flush_audio_timing_diagnostics();
    if (RtLockProbe::enabled() && gameplay_started_) {
        std::cerr << "[probe] rt_lock ";
        rt_probe_.write(std::cerr);
        std::cerr << std::endl;
    }
    audio_thread_.shutdown();
    if (input_thread_.is_running() &&
        input_backend_state_.configured_backend == input::InputBackend::RawInput &&
        input_thread_.current_backend() == input::InputBackend::Polling &&
        !input_backend_state_.auto_fallback) {
        input_backend_state_.auto_fallback = true;
        input_backend_state_.fallback_origin = InputFallbackOrigin::Gameplay;
        input_backend_state_.fallback_reason =
            "RawInput became unhealthy during gameplay; Polling kept note input active.";
        input_backend_state_.fallback_timestamp_utc = utc_timestamp_compact();
        input_backend_state_.effective_backend = input::InputBackend::Polling;
    }
    input_thread_.shutdown();
    stop_chart_audio_workers();
    key_overlay_.stop();
    if (engine_ && gameplay_started_ && audio_error_message_.empty() &&
        !restart_requested_.load(std::memory_order_acquire) &&
        !exit_requested_.load(std::memory_order_acquire)) {
        bool engine_game_over = false;
        bool engine_finished = false;
        game::GaugeType final_gauge = game::GaugeType::Normal;
        double final_gauge_value = 0.0;
        {
            std::lock_guard<std::mutex> lock(engine_mutex_);
            result_.stats = engine_->stats();
            engine_game_over = engine_->is_game_over();
            engine_finished = engine_->is_finished();
            final_gauge = engine_->gauge_state().type;
            final_gauge_value = engine_->gauge_state().value;
        }
        result_.finished = engine_finished;
        result_.final_gauge = gauge_type_token(final_gauge);
        result_.final_gauge_value = final_gauge_value;
        result_.mods = active_mods_;
        result_.key_conversion_version = key_conversion_version_;
        result_.bms_timing_version = bms_timing_version_;
        result_.bms_gauge_version = bms_gauge_version_;
        result_.bms_control_flow_version = bms_control_flow_version_;
        // Playback must retain an old file's absent field instead of fabricating new evidence.
        result_.bms_random_seed = replay_playback_enabled_ ? replay_source_.mode.bms_random_seed :
            (chart_.bms_has_random ? std::optional<uint32_t>(bms_random_seed_) : std::nullopt);
        result_.long_note_mix_version = mode_mod_long_note_mix(active_mods_) ? long_note_mix_version_ : 1;
        result_.player_name = config::normalize_profile_nickname(config_.ui.profile_nickname);
        if (result_.player_name.empty()) {
            result_.player_name = options_.profile;
        }
        result_.rate_multiplier = rate_multiplier_;
        result_.score_multiplier = score_multiplier_;
        result_.final_score = gameplay::scale_native_score(result_.stats.raw_score, result_.score_multiplier);
        result_.ex_score = gameplay::ex_score_from_counts(result_.stats.counts);
        const bool user_aborted = user_aborted_.load(std::memory_order_acquire);
        // Pacemaker is a non-scoring live overlay: gauge failure and clear
        // eligibility are exactly the ordinary run's policy.
        result_.clear_status = gameplay_session_clear_status(
            engine_finished, engine_game_over, user_aborted, final_gauge,
            autoplay_enabled_, practice_no_fail_enabled_, one_miss_fail_enabled_, gauge_shift_enabled_);
        result_.game_over = !gameplay_session_cleared(
            engine_finished, engine_game_over, user_aborted, autoplay_enabled_);
        result_.all_combo_badge = gameplay_session_full_combo(engine_finished, engine_game_over,
            user_aborted || (replay_playback_enabled_ && replay_source_.aborted),
            autoplay_enabled_ || auto_scratch_enabled_ || practice_no_fail_enabled_ ||
                (practice_start_seconds_ > 0.0 && !instant_restart_start_) ||
                (replay_playback_enabled_ &&
                    (replay_source_.mode.autoplay_enabled || replay_source_.mode.practice_no_fail_enabled)),
            result_.stats);
        if (auto_scratch_enabled_) result_.clear_status = "ASSIST AUTO SCRATCH " + result_.clear_status;
        result_.pause_used = replay_playback_enabled_
                                 ? replay_source_.pause_used
                                 : pause_used_.load(std::memory_order_acquire);
        result_.has_value = true;

        const std::string format_token = chart_format_token(chart_format_);
        if (replay_playback_enabled_) {
            result_.replay_path = replay_source_path_;
        } else {
            const std::string created_utc = utc_timestamp_compact();
            // Profile paths are stored as UTF-8, including on Windows. Decoding
            // them through the active ANSI code page breaks replay/result export.
            const auto profile_dir = std::filesystem::u8path(profile_dir_);
            std::filesystem::path replay_path = profile_dir / "replays" / ("replay_" + created_utc + ".json");
            std::filesystem::path result_path = profile_dir / "results" / ("result_" + created_utc + ".json");

            gameplay::ReplayFile replay;
            std::string chart_hash_error;
            const ChartFileHashes chart_hashes = hash_chart_file_utf8(chart_path_, &chart_hash_error);
            if (!chart_hashes.valid()) {
                // Keep the replay playable as legacy evidence, but never claim a
                // verifiable v3 binding when the exact chart bytes were unavailable.
                replay.replay_format_version = 0;
                result_.export_warnings.push_back(
                    "Replay verification evidence unavailable: " +
                    (chart_hash_error.empty() ? std::string("chart hashing failed") : chart_hash_error));
            }
            replay.chart_path = chart_path_;
            replay.chart_format = format_token;
            replay.bms_rank = chart_.bms_rank;
            replay.chart_sha256 = chart_hashes.sha256;
            replay.ruleset_id = replay_ruleset_id_for_runtime(config_.judge,
                                                              config_.gauge,
                                                              gauge_shift_enabled_,
                                                              course_gauge_enabled_,
                                                              "off", result_.mods);
            if (auto_scratch_enabled_ || stale_input_catchup_used_) {
                replay.ruleset_id = mode_mod_judge_ex(result_.mods) ? "custom-ruleset-14" : "custom-ruleset-13";
            }
            if (stale_input_catchup_used_) {
                const std::string warning = "Official scoring disabled: stale input state catch-up is not represented in the replay trace.";
                result_.export_warnings.push_back(warning);
                std::cerr << "[warn] " << warning << std::endl;
            }
            replay.created_utc = created_utc;
            // The copied session options cannot be promoted by a later F10
            // mode change. This origin also survives exported-file retries.
            const bool online_origin = options_.online_recording_enabled &&
                (!options_.online_recording_permission ||
                 options_.online_recording_permission->load(std::memory_order_acquire));
            replay.record_play_mode = online_origin ? "online" : "guest";
            replay.sample_rate = sample_rate_;
            replay.rate = config_.speed.rate;
            replay.input_offset_ms = config_.input_offset_ms;
            replay.mods = result_.mods;
            replay.rate_multiplier = result_.rate_multiplier;
            replay.score_multiplier = result_.score_multiplier;
            replay.final_score = result_.final_score;
            replay.pause_used = result_.pause_used;
            replay.aborted = user_aborted;
            replay.mode.key_mode = config_.mode.key_mode;
            replay.mode.long_note_mix_version = result_.long_note_mix_version;
            replay.mode.key_conversion_version = result_.key_conversion_version;
            replay.mode.bms_timing_version = result_.bms_timing_version;
            replay.mode.bms_gauge_version = result_.bms_gauge_version;
            replay.mode.bms_control_flow_version = result_.bms_control_flow_version;
            replay.mode.bms_random_seed = result_.bms_random_seed;
            replay.mode.key_conversion_algorithm = config_.mode.key_conversion_algorithm;
            replay.mode.key_conversion_nk2_preset = config_.mode.key_conversion_nk2_preset;
            replay.mode.random = config_.mode.random;
            replay.mode.random_seed = config_.mode.random_seed;
            replay.mode.gauge = config_.mode.gauge;
            if (course_gauge_enabled_ && !peer_battle_mode_) {
                replay.mode.course_gauge = gameplay::kLr2CourseGaugeId;
                replay.mode.course_gauge_initial_value =
                    std::clamp(course_gauge_initial_value_, 0.0, 100.0);
            }
            replay.server_challenge_id = options_.ranked_challenge_id;
            replay.server_challenge_nonce = options_.ranked_challenge_nonce;
            replay.mode.autoplay_enabled = autoplay_enabled_;
            replay.mode.practice_no_fail_enabled = practice_no_fail_enabled_;
            replay.mode.one_miss_fail_enabled = one_miss_fail_enabled_;
            replay.trace = engine_->replay();
            replay.stats = result_.stats;

            auto replay_export = gameplay::save_replay_json(replay_path.u8string(), replay);
            if (!replay_export.success()) {
                if (!replay_export.error.empty()) {
                    result_.export_warnings.push_back("Replay export failed: " + replay_export.error);
                }
                result_.export_warnings.insert(result_.export_warnings.end(),
                                               replay_export.warnings.begin(),
                                               replay_export.warnings.end());
            } else {
                result_.replay_path = replay_path.u8string();
                std::string replay_hash_error;
                const ChartFileHashes replay_hashes = hash_chart_file(replay_path, &replay_hash_error);
                if (replay_hashes.valid()) {
                    result_.replay_sha256 = replay_hashes.sha256;
                } else {
                    result_.export_warnings.push_back(
                        "Replay result binding unavailable: " +
                        (replay_hash_error.empty() ? std::string("replay hashing failed")
                                                   : replay_hash_error));
                }
                result_.export_warnings.insert(result_.export_warnings.end(),
                                               replay_export.warnings.begin(),
                                               replay_export.warnings.end());
            }

            gameplay::ResultFile exported_result;
            exported_result.replay_format_version = replay.replay_format_version;
            exported_result.chart_path = chart_path_;
            exported_result.chart_format = format_token;
            exported_result.bms_rank = replay.bms_rank;
            exported_result.chart_sha256 = replay.chart_sha256;
            exported_result.ruleset_id = replay.ruleset_id;
            exported_result.created_utc = created_utc;
            exported_result.record_play_mode = replay.record_play_mode;
            exported_result.player_name = result_.player_name;
            exported_result.replay_path = result_.replay_path;
            exported_result.replay_sha256 = result_.replay_sha256;
            exported_result.clear_status = result_.clear_status;
            exported_result.final_gauge = gauge_type_token(final_gauge);
            exported_result.sample_rate = sample_rate_;
            exported_result.rate = replay.rate;
            exported_result.game_over = result_.game_over;
            exported_result.mods = result_.mods;
            exported_result.long_note_mix_version = result_.long_note_mix_version;
            exported_result.key_conversion_version = result_.key_conversion_version;
            exported_result.bms_timing_version = result_.bms_timing_version;
            exported_result.bms_gauge_version = result_.bms_gauge_version;
            exported_result.bms_control_flow_version = result_.bms_control_flow_version;
            exported_result.bms_random_seed = result_.bms_random_seed;
            exported_result.rate_multiplier = result_.rate_multiplier;
            exported_result.score_multiplier = result_.score_multiplier;
            exported_result.final_score = result_.final_score;
            exported_result.pause_used = result_.pause_used;
            exported_result.aborted = user_aborted;
            exported_result.autoplay_enabled = autoplay_enabled_;
            exported_result.practice_no_fail_enabled = practice_no_fail_enabled_;
            exported_result.one_miss_fail_enabled = one_miss_fail_enabled_;
            exported_result.stats = result_.stats;

            auto result_export = gameplay::save_result_json(result_path.u8string(), exported_result);
            if (!result_export.success()) {
                if (!result_export.error.empty()) {
                    result_.export_warnings.push_back("Result export failed: " + result_export.error);
                }
                result_.export_warnings.insert(result_.export_warnings.end(),
                                               result_export.warnings.begin(),
                                               result_export.warnings.end());
            } else {
                result_.result_path = result_path.u8string();
                result_.export_warnings.insert(result_.export_warnings.end(),
                                               result_export.warnings.begin(),
                                               result_export.warnings.end());
            }
        }
    }
    engine_.reset();
    ghost_engine_.reset();
    reset_score_reference();
    clock_sync_.reset();
    shared_input_clock_.reset();
    audio_input_timing_anchor_ = {};
    judgement_horizon_active_ = false;
    visual_scroll_segments_.reset();
    scheduled_input_audio_.clear();
    scheduled_input_audio_index_ = 0;
    scheduled_input_audio_prepared_ = false;
    current_playback_sample_ = 0;
    pause_sample_offset_ = 0;
    pause_physical_start_sample_ = 0;
    paused_chart_sample_ = 0;
    paused_playback_sample_ = 0;
    pause_anchor_valid_ = false;
    paused_.store(false, std::memory_order_release);
    pause_resume_requested_.store(false, std::memory_order_release);
    resume_countdown_value_.store(0, std::memory_order_release);
    pause_resume_end_sample_ = 0;
    startup_input_timing_anchor_ = {};
    audio_timing_diagnostics_logged_ = false;
    countdown_active_ = false;
    countdown_value_ = 0;
    countdown_started_ns_ = 0;
    reset_control_repeats();
    result_transition_sample_ = 0;
    result_transition_pending_ = false;
    gameplay_started_ = false;
    tone_voices_.clear();
    settings_click_.reset();
    settings_click_requested_.store(false, std::memory_order_release);
    chart_audio_assets_.clear();
    chart_audio_events_.clear();
    chart_audio_voices_.clear();
    chart_audio_load_queue_.clear();
    chart_audio_pcm_registry_.clear();
    chart_audio_deduplicator_.clear();
    next_chart_audio_event_ = 0;
    startup_preload_budget_bytes_ = 0;
    chart_audio_decoded_bytes_ = 0;
    chart_audio_deferred_count_ = 0;
    last_chart_audio_service_sample_ = (std::numeric_limits<int64_t>::min)();
    chart_audio_startup_logged_ = false;
    chart_audio_steady_state_logged_ = false;
    synthetic_tones_enabled_.store(true, std::memory_order_release);
    chart_ = {};
    riff_map_.fill(0);
    riff_map_count_ = 0;
    chart_base_bpm_ = 0.0;
    next_visual_cue_index_ = 0;
    last_visual_cue_sample_ = -1;
    current_background_base_path_.clear();
    current_background_overlay_path_.clear();
    current_background_base_start_sample_ = 0;
    current_background_overlay_start_sample_ = 0;
    active_mods_.clear();
    rate_multiplier_ = 1.0;
    score_multiplier_ = 1.0;
    pause_click_action_.store(-1);
    for (auto& adjustment : pause_click_adjust_) adjustment.store(0);
    autoplay_events_.clear();
    autoplay_enabled_ = false;
    auto_scratch_enabled_ = false;
    autoplay_event_index_ = 0;
    practice_no_fail_enabled_ = false;
    one_miss_fail_enabled_ = false;
    pacemaker_mode_ = "off";
    gauge_shift_enabled_ = false;
    lane_activity_.clear();
    lane_hit_time_ns_.fill(0);
    lane_pressed_.clear();
    hidden_hit_note_ids_.clear();
    active_holds_buffer_.clear();
    ghost_lane_activity_.clear();
    ghost_lane_hit_time_ns_.fill(0);
    ghost_lane_pressed_.clear();
    ghost_hidden_hit_note_ids_.clear();
    ghost_active_holds_buffer_.clear();
    polled_gameplay_keys_.clear();
    hud_scan_start_ = 0;
    ghost_hud_scan_start_ = 0;
    hud_past_hold_indices_.clear();
    ghost_hud_past_hold_indices_.clear();
    hud_scan_cutoff_sample_ = (std::numeric_limits<int64_t>::min)();
    hud_callback_ = nullptr;
    screenshot_callback_ = nullptr;
    // The stopped-session transition is complete. Menus may now drain queued trims.
    memory_deferral_.end();
}

