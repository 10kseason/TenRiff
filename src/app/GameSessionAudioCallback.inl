void GameSession::audio_callback(float* output,
                                 uint32_t frames,
                                 int64_t physical_buffer_start_samples,
                                 int64_t physical_playback_sample) {
    // WASAPI supplies the QPC captured with its padding-derived playback sample,
    // not the potentially delayed time at which this callback finally runs.
    // ASIO and direct/synthetic callbacks retain their existing QPC fallback.
    const int64_t paired_time_ns = audio_thread_.callback_playback_time_ns();
    const int64_t callback_time_ns = paired_time_ns > 0
        ? paired_time_ns : timing::HighResClock::now_ns();
    if (output && frames > 0) {
        std::fill(output, output + frames * 2, 0.0f);
    }

    bool engine_active = false;
    bool paused_callback = false;
    int64_t logical_buffer_start_samples =
        gameplay_logical_sample(physical_buffer_start_samples, pause_sample_offset_);
    int64_t logical_playback_sample =
        gameplay_logical_sample(physical_playback_sample, pause_sample_offset_);
    int64_t committed_sample = logical_buffer_start_samples + static_cast<int64_t>(frames);
    int64_t committed_buffer_start_sample = logical_buffer_start_samples;
    int64_t committed_playback_sample = logical_playback_sample;
    double committed_clock_rate = 1.0;

    const RtLockProbe::Scope probe_callback(rt_probe_.audio_callback);
    {
        const int64_t probe_lock_start_ns = RtLockProbe::begin();
        std::lock_guard<std::mutex> lock(engine_mutex_);
        RtLockProbe::end(rt_probe_.audio_lock_wait, probe_lock_start_ns);
        if (!engine_) {
            return;
        }
        engine_active = true;

        const int64_t now_ns = callback_time_ns;
        // Use the existing callback clock for visual events in this batch.
        hit_effect_clock_ns_ = now_ns;
        if (paused_.load(std::memory_order_acquire)) {
            if (!pause_anchor_valid_) {
                pause_physical_start_sample_ = physical_buffer_start_samples;
                paused_chart_sample_ = logical_buffer_start_samples;
                paused_playback_sample_ = logical_playback_sample;
                if (chronological_judgement_) {
                    paused_playback_sample_ = shared_input_clock_.sample_at(now_ns).value_or(logical_playback_sample);
                }
                pause_anchor_valid_ = true;
            }

            current_playback_sample_ = paused_playback_sample_;
            process_paused_input_queue();
            if (stop_requested_.load(std::memory_order_acquire)) {
                finished_.store(true, std::memory_order_release);
                pause_resume_end_sample_ = 0;
                resume_countdown_value_.store(0, std::memory_order_release);
            } else {
                if (pause_resume_requested_.exchange(false, std::memory_order_acq_rel) &&
                    pause_resume_end_sample_ == 0) {
                    pause_resume_end_sample_ = physical_buffer_start_samples +
                        static_cast<int64_t>(sample_rate_) * kGameplayResumeCountdownSeconds;
                }
                const int remaining = gameplay_resume_countdown_value(
                    pause_resume_end_sample_, physical_buffer_start_samples, sample_rate_);
                resume_countdown_value_.store(remaining, std::memory_order_release);
                if (pause_resume_end_sample_ > 0 && remaining == 0) {
                    // Include this silent callback in the offset so the next audible
                    // buffer resumes at the exact chart sample where pause began.
                    const int64_t physical_resume_sample =
                        physical_buffer_start_samples + static_cast<int64_t>(frames);
                    pause_sample_offset_ = gameplay_pause_resume_offset(
                        pause_sample_offset_, pause_physical_start_sample_, physical_resume_sample);
                    pause_anchor_valid_ = false;
                    pause_resume_end_sample_ = 0;
                    paused_.store(false, std::memory_order_release);
                    clock_sync_.reset();
                    startup_input_timing_anchor_ = {};
                    shared_input_clock_.reset(paused_playback_sample_);
                    audio_input_timing_anchor_ = {};
                    rebaseline_gameplay_start_input_state(chronological_judgement_
                        ? paused_playback_sample_ + (replay_playback_enabled_ || autoplay_enabled_ ? 0 : input_offset_samples_)
                        : paused_chart_sample_);
                }
            }

            committed_sample = paused_chart_sample_;
            committed_buffer_start_sample = paused_chart_sample_;
            committed_playback_sample = paused_playback_sample_;
            paused_callback = true;
        } else {
            const int64_t buffer_end_samples =
                logical_buffer_start_samples + static_cast<int64_t>(frames);
            const int64_t lookahead_samples =
                ms_to_samples(static_cast<double>(kLookaheadMs), sample_rate_);

            // Input timestamps must learn against the device head, not the future write cursor.
            current_playback_sample_ = logical_playback_sample;
            startup_input_timing_anchor_ = StartupInputTimingAnchor{logical_playback_sample, now_ns, true};
            audio_input_timing_anchor_ = startup_input_timing_anchor_;
            clock_sync_.add_sample(now_ns, logical_playback_sample);
            int64_t shared_clock_sample = logical_playback_sample;
            if (chronological_judgement_ || ghost_chronological_judgement_) {
                const int64_t fitted = clock_sync_.estimate_audio_samples(now_ns).value_or(logical_playback_sample);
                shared_clock_sample = reconcile_fresh_gameplay_input_sample(
                    fitted, logical_playback_sample, false, config_.judge.bd_ms, sample_rate_);
                if (shared_clock_sample > buffer_end_samples + lookahead_samples)
                    shared_clock_sample = buffer_end_samples;
                shared_clock_sample = shared_input_clock_.observe(now_ns, shared_clock_sample, sample_rate_);
            }
            if (chronological_judgement_) {
                // One fitted, non-rewinding anchor drives input and the HUD.
                // Reading it is pure: querying now must not clamp older queued inputs.
                current_playback_sample_ = shared_clock_sample;
                startup_input_timing_anchor_.playback_sample = shared_clock_sample;
                committed_playback_sample = shared_clock_sample;
                committed_clock_rate = shared_input_clock_.rate();
            }
            // Negative calibration means a newly arriving input can still name
            // a past chart sample. Automatic judgements must wait for that same
            // calibrated clock, while audio continues filling the write buffer.
            judgement_horizon_sample_ = chronological_judgement_
                ? shared_clock_sample + (replay_playback_enabled_ || autoplay_enabled_ ? 0 : input_offset_samples_)
                : buffer_end_samples;
            judgement_horizon_active_ = chronological_judgement_;
            if (!audio_timing_diagnostics_logged_) {
                // Formatting and console I/O happen on the session thread.
                audio_timing_diagnostics_frames_.store(frames, std::memory_order_release);
                audio_timing_diagnostics_write_ahead_frames_.store(
                    (std::max)(int64_t{0}, logical_buffer_start_samples - logical_playback_sample),
                    std::memory_order_release);
                audio_timing_diagnostics_pending_.store(true, std::memory_order_release);
                audio_timing_diagnostics_logged_ = true;
            }

            schedule_note_guides(logical_buffer_start_samples, buffer_end_samples);
            process_future_events(logical_buffer_start_samples,
                                  buffer_end_samples,
                                  lookahead_samples);
            process_input_queue(logical_buffer_start_samples, buffer_end_samples, lookahead_samples);

            if (paused_.load(std::memory_order_acquire)) {
                pause_physical_start_sample_ = physical_buffer_start_samples;
                paused_chart_sample_ = logical_buffer_start_samples;
                paused_playback_sample_ = current_playback_sample_;
                pause_anchor_valid_ = true;
                committed_sample = paused_chart_sample_;
                committed_buffer_start_sample = paused_chart_sample_;
                committed_playback_sample = paused_playback_sample_;
                paused_callback = true;
            } else {
                service_control_repeats(now_ns, foreground_for_realtime(now_ns));
                process_autoplay_queue(logical_buffer_start_samples,
                                       chronological_judgement_ ? judgement_horizon_sample_ : buffer_end_samples,
                                       chronological_judgement_ ? 0 : lookahead_samples);
                process_ghost_replay_queue(
                    ghost_chronological_judgement_ ? shared_clock_sample : buffer_end_samples,
                    ghost_chronological_judgement_ ? 0 : lookahead_samples);
                engine_->advance(judgement_horizon_sample_);
                if (chronological_judgement_) {
                    mine_trigger_buffer_.clear();
                    engine_->drain_mine_triggers(mine_trigger_buffer_);
                    for (const auto& trigger : mine_trigger_buffer_) {
                        trigger_lane_hit_effect(trigger.lane);
                        if (!scheduled_input_audio_prepared_ || (!replay_playback_enabled_ && !autoplay_enabled_))
                            schedule_mine_keysound(trigger, pin_realtime_audio_start_sample(trigger.sample, logical_buffer_start_samples));
                    }
                }
                if (ghost_engine_) {
                    ghost_engine_->advance(ghost_chronological_judgement_ ? shared_clock_sample : buffer_end_samples);
                }
                advance_score_reference(shared_clock_sample);
                // Live input can fail the run in this callback. Decide scratch
                // keysounds afterward, while retaining their future write time.
                schedule_input_audio(logical_buffer_start_samples, buffer_end_samples + lookahead_samples);

                if (!lane_activity_.empty() && sample_rate_ > 0) {
                    const float decay = static_cast<float>(static_cast<double>(frames) * 5.0 /
                                                           static_cast<double>(sample_rate_));
                    for (float& activity : lane_activity_) {
                        activity = std::max(0.0f, activity - decay);
                    }
                    for (float& activity : ghost_lane_activity_) {
                        activity = std::max(0.0f, activity - decay);
                    }
                }

                if (stop_requested_.load(std::memory_order_acquire)) {
                    finished_.store(true, std::memory_order_release);
                } else if (engine_->is_game_over()) {
                    if (peer_battle_mode_) {
                        // Keep the synchronized chart/audio timeline alive while the
                        // defeated player watches the remaining peer. FinalScore stays
                        // single-shot and is sent only after this spectator phase ends.
                        spectating_peer_.store(true, std::memory_order_release);
                        result_transition_pending_ = false;
                        result_transition_sample_ = 0;
                    } else {
                        finished_.store(true, std::memory_order_release);
                    }
                } else if (engine_->is_finished()) {
                    if (!result_transition_pending_) {
                        const int64_t tail_samples =
                            ms_to_samples(std::max(0.0, config_.ui.result_tail_ms), sample_rate_);
                        // Automatic results wait for missed notes to exit the
                        // field and for a released hold to reach its original tail.
                        const int64_t note_fall_samples = ms_to_samples(kHudPastMs, sample_rate_);
                        int64_t visible_note_end = 0;
                        for (const auto& note : chart_.notes) {
                            const bool successful = note.note_id < hidden_hit_note_ids_.size() &&
                                                    hidden_hit_note_ids_[note.note_id] != 0;
                            if (!successful || note.end_sample.has_value()) {
                                const auto end = note_visible_end_sample(note) + (successful ? 0 : note_fall_samples);
                                visible_note_end = std::max(visible_note_end, end);
                            }
                        }
                        if (visible_note_end > 0) {
                            visible_note_end += ms_to_samples(
                                std::max(0.0, -config_.visual_offset_ms), sample_rate_);
                        }
                        result_transition_sample_ = gameplay_result_transition_sample(
                            buffer_end_samples, visible_note_end, tail_samples);
                        result_transition_pending_ = true;
                    }
                } else {
                    result_transition_pending_ = false;
                    result_transition_sample_ = 0;
                }
            }
        }
        judgement_horizon_active_ = false;
    }

    if (engine_active && !paused_callback) {
        const int64_t buffer_end_samples =
            logical_buffer_start_samples + static_cast<int64_t>(frames);
        schedule_chart_audio(buffer_end_samples);
        if (config_.audio_ui.play_to_end && result_transition_pending_ && !chart_audio_voices_.empty()) {
            // The final mixed buffer is still queued at the device. Remember
            // its end and wait for the playback head before Stop/Reset can run.
            result_transition_sample_ = std::max(result_transition_sample_, buffer_end_samples);
        }
        mix_chart_audio(output, frames, logical_buffer_start_samples);
        mix_tones(output, frames, logical_buffer_start_samples);
        if (result_transition_pending_ &&
            (!config_.audio_ui.play_to_end ||
             (next_chart_audio_event_ >= chart_audio_events_.size() && chart_audio_voices_.empty())) &&
            logical_playback_sample >= result_transition_sample_) {
            finished_.store(true, std::memory_order_release);
        }
    }

    if (engine_active) {
        // Paused tuning still clicks immediately while chart audio and its
        // normalizer remain frozen until the resume countdown completes.
        finish_audio_output(output, frames,
            !config_.audio_ui.mute_when_inactive || foreground_for_realtime(callback_time_ns),
            !paused_callback);
    }
    audio_timing_sequence_.fetch_add(1, std::memory_order_acq_rel);
    // Pair with the reader's acquire fence: observing a newer payload field
    // must also expose this odd sequence value before the final reader check.
    std::atomic_thread_fence(std::memory_order_release);
    last_audio_timing_.store({committed_sample, committed_buffer_start_sample,
                             committed_playback_sample, callback_time_ns, frames, committed_clock_rate});
    audio_timing_sequence_.fetch_add(1, std::memory_order_release);
    last_audio_sample_.store(committed_sample, std::memory_order_release);
}
