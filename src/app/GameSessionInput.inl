void GameSession::refresh_judgement_loop_timing() {
    judgement_loop_plan_ = build_judgement_loop_timing_plan(sample_rate_, config_.input.judgement_hz);
    judgement_loop_step_carry_ = 0;
}

int64_t GameSession::next_judgement_loop_step_samples() {
    return ::tenriff::app::next_judgement_loop_step_samples(judgement_loop_plan_, judgement_loop_step_carry_);
}

void GameSession::run_judgement_loop(int64_t buffer_start_samples,
                                     int64_t buffer_end_samples,
                                     int64_t lookahead_samples) {
    int64_t tick_cursor = buffer_start_samples;
    while (tick_cursor < buffer_end_samples) {
        const int64_t step_samples = next_judgement_loop_step_samples();
        const int64_t tick_end_samples = std::min(buffer_end_samples, tick_cursor + step_samples);
        process_future_events(tick_cursor, tick_end_samples, lookahead_samples);
        process_input_queue(tick_cursor, tick_end_samples, lookahead_samples);
        process_autoplay_queue(tick_cursor, tick_end_samples, lookahead_samples);
        process_ghost_replay_queue(tick_end_samples, lookahead_samples);
        engine_->advance(tick_end_samples);
        mine_trigger_buffer_.clear();
        engine_->drain_mine_triggers(mine_trigger_buffer_);
        for (const auto& trigger : mine_trigger_buffer_) {
            trigger_lane_hit_effect(trigger.lane);
            const int64_t audible_sample =
                pin_realtime_audio_start_sample(trigger.sample, tick_cursor);
            schedule_mine_keysound(trigger, audible_sample);
        }
        if (ghost_engine_) {
            ghost_engine_->advance(tick_end_samples);
            ghost_mine_trigger_buffer_.clear();
            ghost_engine_->drain_mine_triggers(ghost_mine_trigger_buffer_);
        }
        advance_score_reference(tick_end_samples);
        tick_cursor = tick_end_samples;
    }
}

void GameSession::process_countdown_input_queue() {
    key_overlay_.invalidate();
    while (true) {
        auto maybe_event = input_thread_.queue().pop();
        if (!maybe_event.has_value()) {
            break;
        }
        note_runtime_input_event_source(*maybe_event);
        // InputThread already publishes one logical edge per key. Re-filtering
        // by device here can leave a countdown-baselined lane stuck down.
        if (handle_control_input(*maybe_event) &&
            finished_.load(std::memory_order_acquire)) {
            return;
        }
    }

    std::fill(lane_activity_.begin(), lane_activity_.end(), 0.0f);
    std::fill(lane_pressed_.begin(), lane_pressed_.end(), 0);
}

void GameSession::process_paused_input_queue() {
    key_overlay_.invalidate();
    const int clicked_action = pause_click_action_.exchange(-1);
    const bool menu_ready = paused_.load() && resume_countdown_value_.load() == 0 && !pause_resume_requested_.load();
    for (std::size_t index = 0; index < pause_click_adjust_.size(); ++index) {
        const int steps = pause_click_adjust_[index].exchange(0);
        if (!menu_ready || steps == 0) continue;
        if (index == 0) adjust_judgement_line_position(steps * kJudgementLinePositionStep);
        if (index == 1) adjust_hispeed(steps * kHispeedStep);
        if (index == 2) adjust_visual_offset(steps * kInPlayVisualOffsetStep);
    }
    if (menu_ready && clicked_action >= 0 && clicked_action < 3) {
        pause_menu_cursor_.store(clicked_action);
        input::InputEvent click{};
        click.keycode = enter_keycode_;
        click.state = input::InputState::Pressed;
        (void)handle_control_input(click);
    }
    while (true) {
        auto maybe_event = input_thread_.queue().pop();
        if (!maybe_event.has_value()) {
            break;
        }
        note_runtime_input_event_source(*maybe_event);
        (void)handle_control_input(*maybe_event);
    }
    while (future_events_.pop().has_value()) {
    }
    pending_input_events_.clear();
    const int64_t paused_now_ns = timing::HighResClock::now_ns();
    service_control_repeats(paused_now_ns, foreground_for_realtime(paused_now_ns));
    std::fill(lane_activity_.begin(), lane_activity_.end(), 0.0f);
    std::fill(ghost_lane_activity_.begin(), ghost_lane_activity_.end(), 0.0f);
    lane_hit_time_ns_.fill(0);
    ghost_lane_hit_time_ns_.fill(0);
    std::fill(lane_pressed_.begin(), lane_pressed_.end(), 0);
    std::fill(ghost_lane_pressed_.begin(), ghost_lane_pressed_.end(), 0);
}

void GameSession::rebaseline_gameplay_start_input_state(int64_t sample) {
    key_overlay_.invalidate();
    while (input_thread_.queue().pop().has_value()) {
    }
    while (future_events_.pop().has_value()) {
    }
    pending_input_events_.clear();
    const auto physical_key_pressed = [this](uint32_t keycode) {
        if (physical_key_state_probe_) return physical_key_state_probe_(keycode);
        if (const auto vk = config::KeycodeMap::polling_vk_for_keycode(keycode))
            return (GetAsyncKeyState(static_cast<int>(*vk)) & 0x8000) != 0;
        return input_thread_.gamepad_key_pressed(keycode);
    };

    // A key held through restart/loading/resume must be released before it can
    // start another attempt. It never becomes a lane press or replay event.
    instant_restart_key_armed_ = true;
    if (instant_restart_keycode_ != 0) {
        instant_restart_key_armed_ = !physical_key_pressed(instant_restart_keycode_);
    }

    reset_control_repeats();

    std::fill(lane_pressed_.begin(), lane_pressed_.end(), 0);
    hispeed_coarse_decrease_held_ = false;
    hispeed_coarse_increase_held_ = false;
    std::vector<uint32_t> held_lane_keys;
    held_lane_keys.reserve(polled_gameplay_keys_.size());
    const int64_t baseline_time_ns = timing::HighResClock::now_ns();
    for (auto& tracked : polled_gameplay_keys_) {
        if (!physical_key_pressed(tracked.keycode)) {
            continue;
        }

        if (tracked.keycode == f5_keycode_) hispeed_coarse_decrease_held_ = true;
        if (tracked.keycode == f6_keycode_) hispeed_coarse_increase_held_ = true;
        if (tracked.keycode == f3_keycode_) {
            hispeed_decrease_held_ = true;
            hispeed_decrease_started_ns_ = baseline_time_ns;
            hispeed_decrease_next_repeat_ns_ = baseline_time_ns + ms_to_ns(kHispeedRepeatInitialDelayMs);
        } else if (tracked.keycode == f4_keycode_) {
            hispeed_increase_held_ = true;
            hispeed_increase_started_ns_ = baseline_time_ns;
            hispeed_increase_next_repeat_ns_ = baseline_time_ns + ms_to_ns(kHispeedRepeatInitialDelayMs);
        }

        if (autoplay_enabled_ || replay_playback_enabled_) {
            continue;
        }
        held_lane_keys.push_back(tracked.keycode);
    }

    // Snapshot all physical keys first. Resume retains the actual owner rather
    // than selecting whichever held binding happens to occur last in the list.
    if (!autoplay_enabled_ && !replay_playback_enabled_) {
        lane_binding_state_.rebaseline(held_lane_keys, gameplay_started_.load(std::memory_order_acquire));
        for (int lane = 1; lane <= chart_.lane_count; ++lane) {
            if (auto_scratch_lane(lane)) continue;
            catch_up_lane_input(lane, lane_binding_state_.pressed(lane)
                ? input::InputState::Pressed : input::InputState::Released, sample);
        }
    }

    std::fill(lane_activity_.begin(), lane_activity_.end(), 0.0f);
}

bool GameSession::handle_control_input(const input::InputEvent& incoming_event) {
    auto event = incoming_event;
    if (const auto index = input::gamepad::keycode_index(event.keycode)) {
        const bool assigned = key_to_lane_.count(event.keycode) != 0 ||
            event.keycode == instant_restart_keycode_ || event.keycode == f5_keycode_ || event.keycode == f6_keycode_;
        if (!assigned && *index == 4) event.keycode = escape_keycode_;
        else if (paused_.load(std::memory_order_acquire) && !assigned) {
            if (*index == 0) event.keycode = up_keycode_;
            if (*index == 1) event.keycode = down_keycode_;
            if (*index == 10) event.keycode = enter_keycode_;
            if (*index == 11 || *index == 5) event.keycode = escape_keycode_;
        }
    }
    const bool decrease = f5_keycode_ != 0 && event.keycode == f5_keycode_;
    const bool increase = f6_keycode_ != 0 && event.keycode == f6_keycode_;
    bool coarse_press = false;
    if (decrease || increase) {
        auto& held = decrease ? hispeed_coarse_decrease_held_ : hispeed_coarse_increase_held_;
        coarse_press = event.state == input::InputState::Pressed && !held;
        held = event.state == input::InputState::Pressed;
    }
    const bool restart_key = instant_restart_keycode_ != 0 && event.keycode == instant_restart_keycode_;
    if (restart_key && event.state == input::InputState::Released) instant_restart_key_armed_ = true;
    // A later lane-key edit can invalidate a profile overlay shortcut. Do not
    // let the menu callback consume that lane even though it retains the label.
    const bool invalid_overlay_key = event.keycode != 0 &&
        (event.keycode == lane_conflicting_overlay_keys_[0] || event.keycode == lane_conflicting_overlay_keys_[1]);
    if (!invalid_overlay_key && control_input_callback_ && control_input_callback_(event)) {
        if (restart_key && event.state == input::InputState::Pressed) instant_restart_key_armed_ = false;
        reset_control_repeats();
        return true;
    }
    if (restart_key) {
        if (event.state == input::InputState::Pressed) {
            const bool allowed = instant_restart_key_armed_ && gameplay_started_.load(std::memory_order_acquire) &&
                !peer_battle_mode_ && !course_gauge_enabled_ && !countdown_active_ &&
                !paused_.load(std::memory_order_acquire) && !finished_.load(std::memory_order_acquire);
            instant_restart_key_armed_ = false;
            if (allowed) {
                instant_restart_requested_.store(true, std::memory_order_release);
                restart_requested_.store(true, std::memory_order_release);
                user_aborted_.store(true, std::memory_order_release);
                stop_requested_.store(true, std::memory_order_release);
                finished_.store(true, std::memory_order_release);
            }
        }
        return true;
    }
    // A deliberate Enter press can finish the audio tail after every note/LN.
    // Lane presses and releases still reach judgement normally; they never exit.
    if (event.state == input::InputState::Pressed && enter_keycode_ != 0 &&
        event.keycode == enter_keycode_ && !peer_battle_mode_ && !course_gauge_enabled_ &&
        gameplay_started_ && !paused_.load(std::memory_order_acquire) && engine_ && !engine_->is_game_over() &&
        should_skip_post_note_audio(engine_->is_finished(), result_transition_pending_, true)) {
        finished_.store(true, std::memory_order_release);
        return true;
    }
    // Course runs cannot be paused or aborted with Esc, including their opening
    // countdown. Overlay controls may still consume Esc to close their own UI.
    if (course_gauge_enabled_ && escape_keycode_ != 0 && event.keycode == escape_keycode_) {
        return true;
    }
    if (paused_.load(std::memory_order_acquire)) {
        if (event.state != input::InputState::Pressed) {
            update_hispeed_repeat_state(event.keycode, event.state, event.input_time_ns);
            (void)update_tuning_repeat_state(event.keycode, event.state, event.input_time_ns);
            return true;
        }
        if (f9_keycode_ != 0 && event.keycode == f9_keycode_) {
            if (screenshot_callback_) {
                screenshot_callback_();
            }
            return true;
        }
        if (resume_countdown_value_.load(std::memory_order_acquire) > 0 ||
            pause_resume_requested_.load(std::memory_order_acquire)) {
            // Esc cancels a countdown back to the pause menu; other queued input
            // cannot restart it or become a scored lane press on resumption.
            if (escape_keycode_ != 0 && event.keycode == escape_keycode_) {
                pause_resume_end_sample_ = 0;
                pause_resume_requested_.store(false, std::memory_order_release);
                resume_countdown_value_.store(0, std::memory_order_release);
            }
            return true;
        }
        if (escape_keycode_ != 0 && event.keycode == escape_keycode_) {
            pause_resume_requested_.store(true, std::memory_order_release);
            return true;
        }
        // Pause uses the same precise first step and accelerated hold as play.
        if ((f3_keycode_ != 0 && event.keycode == f3_keycode_) ||
            (f4_keycode_ != 0 && event.keycode == f4_keycode_)) {
            update_hispeed_repeat_state(event.keycode, event.state, event.input_time_ns);
            return true;
        }
        if (update_tuning_repeat_state(event.keycode, event.state, event.input_time_ns)) return true;
        if (f5_keycode_ != 0 && event.keycode == f5_keycode_) {
            if (coarse_press) adjust_hispeed(-config_.speed.hi_speed * 0.5);
            return true;
        }
        if (f6_keycode_ != 0 && event.keycode == f6_keycode_) {
            if (coarse_press) adjust_hispeed(config_.speed.hi_speed);
            return true;
        }
        if (up_keycode_ != 0 && event.keycode == up_keycode_) {
            const int cursor = pause_menu_cursor_.load(std::memory_order_acquire);
            pause_menu_cursor_.store(wrap_gameplay_pause_cursor(cursor, -1), std::memory_order_release);
            return true;
        }
        if (down_keycode_ != 0 && event.keycode == down_keycode_) {
            const int cursor = pause_menu_cursor_.load(std::memory_order_acquire);
            pause_menu_cursor_.store(wrap_gameplay_pause_cursor(cursor, 1), std::memory_order_release);
            return true;
        }
        if (enter_keycode_ != 0 && event.keycode == enter_keycode_) {
            switch (gameplay_pause_action_for_cursor(pause_menu_cursor_.load(std::memory_order_acquire))) {
                case GameplayPauseAction::Continue:
                    pause_resume_requested_.store(true, std::memory_order_release);
                    break;
                case GameplayPauseAction::Restart:
                    if (peer_battle_mode_ || course_gauge_enabled_) break;
                    restart_requested_.store(true, std::memory_order_release);
                    user_aborted_.store(true, std::memory_order_release);
                    stop_requested_.store(true, std::memory_order_release);
                    finished_.store(true, std::memory_order_release);
                    break;
                case GameplayPauseAction::Exit:
                    exit_requested_.store(true, std::memory_order_release);
                    user_aborted_.store(true, std::memory_order_release);
                    stop_requested_.store(true, std::memory_order_release);
                    finished_.store(true, std::memory_order_release);
                    break;
            }
        }
        return true;
    }
    if ((f3_keycode_ != 0 && event.keycode == f3_keycode_) ||
        (f4_keycode_ != 0 && event.keycode == f4_keycode_)) {
        update_hispeed_repeat_state(event.keycode, event.state, event.input_time_ns);
        return true;
    }
    if (update_tuning_repeat_state(event.keycode, event.state, event.input_time_ns)) {
        return true;
    }
    if (event.state == input::InputState::Pressed) {
        if (f5_keycode_ != 0 && event.keycode == f5_keycode_) {
            if (coarse_press) adjust_hispeed(-config_.speed.hi_speed * 0.5);
            return true;
        }
        if (f6_keycode_ != 0 && event.keycode == f6_keycode_) {
            if (coarse_press) adjust_hispeed(config_.speed.hi_speed);
            return true;
        }
        if (f9_keycode_ != 0 && event.keycode == f9_keycode_) {
            if (screenshot_callback_) {
                screenshot_callback_();
            }
            return true;
        }
    }
    if (escape_keycode_ != 0 && event.state == input::InputState::Pressed &&
        event.keycode == escape_keycode_) {
        // Completed audio tails accept Enter only; Esc must not pause/abort a clear.
        if (!peer_battle_mode_ && gameplay_started_ && engine_ && engine_->is_finished() && result_transition_pending_)
            return true;
        if (peer_battle_mode_ || !gameplay_started_) {
            user_aborted_.store(true, std::memory_order_release);
            stop_requested_.store(true, std::memory_order_release);
            finished_.store(true, std::memory_order_release);
        } else {
            pause_menu_cursor_.store(0, std::memory_order_release);
            pause_resume_requested_.store(false, std::memory_order_release);
            pause_resume_end_sample_ = 0;
            resume_countdown_value_.store(0, std::memory_order_release);
            pause_used_.store(true, std::memory_order_release);
            paused_.store(true, std::memory_order_release);
            reset_control_repeats();
        }
        return true;
    }
    return false;
}

void GameSession::update_lane_feedback(int lane, input::InputState state) {
    const int lane_index = lane - 1;
    if (lane_index < 0 || lane_index >= static_cast<int>(lane_activity_.size())) {
        return;
    }

    lane_pressed_[static_cast<std::size_t>(lane_index)] =
        state == input::InputState::Pressed ? 1 : 0;
    if (state == input::InputState::Pressed) {
        lane_activity_[static_cast<std::size_t>(lane_index)] = 1.0f;
    }
}

void GameSession::trigger_lane_hit_effect(int lane) {
    const int lane_index = lane - 1;
    if (lane_index < 0 || lane_index >= static_cast<int>(lane_activity_.size())) {
        return;
    }

    lane_activity_[static_cast<std::size_t>(lane_index)] = 1.0f;
}

void GameSession::dispatch_lane_input(int lane,
                                      input::InputState state,
                                      int64_t sample,
                                      int64_t audio_buffer_start_sample,
                                      std::optional<int64_t> physical_input_sample,
                                      bool emit_audio) {
    update_lane_feedback(lane, state);
    // Live calibration changes judgement, not when a physical press sounds.
    // Replay/autoplay callers retain their recorded/scheduled audio timestamp.
    const int64_t audible_sample = pin_realtime_audio_start_sample(
        physical_input_sample.value_or(sample), audio_buffer_start_sample);
    if (emit_audio && state == input::InputState::Pressed) {
        schedule_tone(lane, audible_sample, false);
    }

    auto hit_note = engine_->handle_input(lane, state, sample);
    if (state == input::InputState::Pressed && hit_note.has_value()) {
        trigger_lane_hit_effect(lane);
        if (is_successful_hit(engine_->live_feedback().judgement)) {
            if (lane > 0 && lane <= static_cast<int>(lane_hit_time_ns_.size())) {
                lane_hit_time_ns_[static_cast<std::size_t>(lane - 1)] = hit_effect_clock_ns_;
            }
            if (hit_note->note_id < hidden_hit_note_ids_.size()) {
                hidden_hit_note_ids_[hit_note->note_id] = 1;
            }
        }
        if (emit_audio) schedule_note_keysound(hit_note.value(), audible_sample);
    }
}

void GameSession::catch_up_lane_input(int lane, input::InputState state, int64_t sample) {
    update_lane_feedback(lane, state);
    engine_->sync_input_state(lane, state, sample);
}

void GameSession::process_future_events(int64_t buffer_start_samples,
                                        int64_t buffer_end_samples,
                                        int64_t lookahead_samples) {
    while (true) {
        if (judgement_horizon_active_) {
            const auto next = future_events_.peek();
            if (next && next->sample > judgement_horizon_sample_) return;
        }
        auto next = future_events_.pop();
        if (!next.has_value()) {
            return;
        }
        if (handle_control_input(next->event)) {
            if (finished_.load(std::memory_order_acquire)) {
                return;
            }
            continue;
        }
        if (!judgement_horizon_active_ && next->sample > buffer_end_samples + lookahead_samples) {
            next->sample = buffer_end_samples;
        }
        if (autoplay_enabled_) {
            continue;
        }
        if (auto lane = lane_from_keycode(next->event.keycode)) {
            if (auto_scratch_lane(*lane)) continue;
            if (auto_scratch_enabled_) process_autoplay_queue(buffer_start_samples, next->sample, 0);
            const auto logical_state = lane_binding_state_.apply(next->event.keycode, next->event.state);
            if (!logical_state) continue;
            key_overlay_.record(*lane - 1, *logical_state == input::InputState::Pressed, next->event.input_time_ns);
            dispatch_lane_input(lane.value(),
                                *logical_state,
                                next->sample,
                                buffer_start_samples);
        }
    }
}

void GameSession::process_input_queue(int64_t buffer_start_samples, int64_t buffer_end_samples,
                                      int64_t lookahead_samples) {
    if (replay_playback_enabled_) {
        process_replay_input_queue(buffer_start_samples, buffer_end_samples, lookahead_samples);
        return;
    }

    // Backlog age is measured in the same QPC clock as the input event. The
    // mapped sample can temporarily drift while the audio clock rebases, and
    // must not turn a fresh physical press into a non-scoring catch-up event.
    const double stale_window_ms = gameplay_input_backlog_stale_window_ms(config_.judge.bd_ms);
    std::array<BufferedLaneInput, kGameplayHudMaxLanes> stale_lane_inputs{};
    std::array<uint8_t, kGameplayHudMaxLanes> stale_lane_present{};
    pending_input_events_.clear();

    while (true) {
        auto maybe_event = input_thread_.queue().pop();
        if (!maybe_event.has_value()) {
            break;
        }

        const auto event = *maybe_event;
        note_runtime_input_event_source(event);
        if (handle_control_input(event)) {
            if (finished_.load(std::memory_order_acquire)) {
                return;
            }
            continue;
        }

        const bool event_is_stale = gameplay_input_event_is_stale(
            event.input_time_ns,
            startup_input_timing_anchor_.callback_time_ns,
            stale_window_ms);
        auto mapped = judgement_horizon_active_
            ? shared_input_clock_.sample_at(event.input_time_ns)
            : clock_sync_.input_to_audio_samples(event.input_time_ns);
        int64_t resolved_sample = resolve_startup_gameplay_input_sample(
            mapped,
            event.input_time_ns,
            startup_input_timing_anchor_,
            sample_rate_,
            current_playback_sample_);
        const auto anchor_sample = estimate_input_sample_from_startup_anchor(
            event.input_time_ns,
            startup_input_timing_anchor_,
            sample_rate_);
        resolved_sample = reconcile_fresh_gameplay_input_sample(
            resolved_sample,
            anchor_sample,
            event_is_stale,
            stale_window_ms,
            sample_rate_);
        // Guard an implausible clock mapping before adding intentional input
        // calibration. Clamping the calibrated sample to this buffer quantizes
        // positive offsets according to the callback phase (per-note jitter).
        const int64_t mapping_end = judgement_horizon_active_
            ? current_playback_sample_ + (buffer_end_samples - buffer_start_samples) : buffer_end_samples;
        if (resolved_sample > mapping_end + lookahead_samples) {
            resolved_sample = mapping_end;
        }
        const int64_t sample = resolved_sample + input_offset_samples_;
        auto lane = lane_from_keycode(event.keycode);
        if (!lane.has_value()) {
            continue;
        }
        if (autoplay_enabled_ || auto_scratch_lane(*lane)) {
            continue;
        }
        const int lane_index = lane.value() - 1;
        if (lane_index < 0 || lane_index >= static_cast<int>(kGameplayHudMaxLanes)) {
            continue;
        }
        const auto logical_state = lane_binding_state_.apply(event.keycode, event.state);
        if (!logical_state) continue;
        if (event_is_stale) {
            key_overlay_.invalidate();
            stale_lane_present[static_cast<std::size_t>(lane_index)] = 1;
            stale_lane_inputs[static_cast<std::size_t>(lane_index)] = BufferedLaneInput{
                lane.value(), *logical_state, sample};
            continue;
        }
        key_overlay_.record(lane_index, *logical_state == input::InputState::Pressed, event.input_time_ns);
        pending_input_events_.push_back(
            BufferedLaneInput{lane.value(), *logical_state, sample,
                judgement_horizon_active_ ? estimate_input_sample_from_startup_anchor(
                    event.input_time_ns, audio_input_timing_anchor_, sample_rate_).value_or(resolved_sample)
                    : resolved_sample});
    }

    for (std::size_t lane_index = 0; lane_index < stale_lane_present.size(); ++lane_index) {
        if (stale_lane_present[lane_index] == 0) {
            continue;
        }
        const auto& buffered = stale_lane_inputs[lane_index];
        // Startup/resume rebaselines also use catch_up_lane_input; only an
        // actual discarded stale edge makes this replay trace incomplete.
        stale_input_catchup_used_ = true;
        catch_up_lane_input(buffered.lane, buffered.state, buffered.sample);
    }

    for (const auto& buffered : pending_input_events_) {
        // Input may have arrived after the paired device observation while
        // GetBuffer/callback delivery was delayed. It is already observed, so
        // its calibrated timestamp can safely extend this callback's frontier.
        if (judgement_horizon_active_)
            judgement_horizon_sample_ = (std::max)(judgement_horizon_sample_, buffered.sample);
        // Merge synthetic scratch edges before each physical key edge, so the
        // engine and replay preserve exact sample order even with large buffers.
        if (auto_scratch_enabled_) process_autoplay_queue(buffer_start_samples, buffered.sample, 0);
        dispatch_lane_input(buffered.lane,
                            buffered.state,
                            buffered.sample,
                            buffer_start_samples,
                            buffered.audio_sample);
    }
}

