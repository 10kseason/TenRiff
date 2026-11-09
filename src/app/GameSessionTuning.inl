void GameSession::adjust_hispeed(double delta) {
    if (!std::isfinite(delta) || std::abs(delta) < 1e-9) {
        return;
    }
    if (!engine_) {
        return;
    }

    double next = std::clamp(config_.speed.hi_speed + delta, kHispeedMin, kHispeedMax);
    next = std::round(next * 100.0) / 100.0;
    if (config_.speed.hi_speed == next) return;
    config_.speed.hi_speed = next;
    if (countdown_active_) {
        // The gameplay audio device starts after the countdown. Give tuning
        // feedback now instead of leaving a stale click queued for chart start.
        play_settings_adjustment_click(focused_audio_master_gain(
            config_.audio_ui.master_volume, config_.audio_ui.mute_when_inactive,
            process_owns_foreground_window()));
    } else {
        settings_click_requested_.store(true, std::memory_order_release);
    }
}

void GameSession::update_hispeed_repeat_state(uint32_t keycode, input::InputState state, int64_t event_time_ns) {
    const int64_t safe_event_time_ns = (event_time_ns > 0) ? event_time_ns : timing::HighResClock::now_ns();
    auto update_repeat = [&](bool* held, int64_t* next_repeat_ns, int64_t* started_ns, double delta) {
        if (!held || !next_repeat_ns) {
            return;
        }
        if (state == input::InputState::Pressed) {
            if (*held) return; // Ignore platform typematic; our timer owns repeats.
            adjust_hispeed(delta);
            *held = true;
            *started_ns = safe_event_time_ns;
            *next_repeat_ns = safe_event_time_ns + ms_to_ns(kHispeedRepeatInitialDelayMs);
        } else {
            *held = false;
            *next_repeat_ns = 0;
            *started_ns = 0;
        }
    };

    if (f3_keycode_ != 0 && keycode == f3_keycode_) {
        update_repeat(&hispeed_decrease_held_, &hispeed_decrease_next_repeat_ns_, &hispeed_decrease_started_ns_, -kHispeedStep);
        return;
    }
    if (f4_keycode_ != 0 && keycode == f4_keycode_) {
        update_repeat(&hispeed_increase_held_, &hispeed_increase_next_repeat_ns_, &hispeed_increase_started_ns_, kHispeedStep);
    }
}

void GameSession::adjust_judgement_line_position(double delta) {
    if (!std::isfinite(delta) || std::abs(delta) < 1e-9) {
        return;
    }
    double next = std::clamp(config_.skin.judgement_line_position + delta,
                             config::kJudgementLinePositionMin,
                             config::kJudgementLinePositionMax);
    next = std::round(next * 1000.0) / 1000.0;
    config_.skin.judgement_line_position = next;
}

void GameSession::adjust_visual_offset(double delta) {
    if (!std::isfinite(delta) || std::abs(delta) < 1e-9) {
        return;
    }
    double next = std::clamp(config_.visual_offset_ms + delta, kVisualOffsetMin, kVisualOffsetMax);
    next = std::round(next * 100.0) / 100.0;
    config_.visual_offset_ms = next;
}

bool GameSession::update_tuning_repeat_state(uint32_t keycode,
                                             input::InputState state,
                                             int64_t event_time_ns) {
    const int64_t safe_event_time_ns = (event_time_ns > 0) ? event_time_ns : timing::HighResClock::now_ns();
    auto update_repeat = [&](TuningRepeat& repeat, double delta, bool judgement_line) {
        if (state == input::InputState::Pressed) {
            if (repeat.held) return;
            if (judgement_line) {
                adjust_judgement_line_position(delta);
            } else {
                adjust_visual_offset(delta);
            }
            repeat.held = true;
            repeat.started_ns = safe_event_time_ns;
            repeat.next_repeat_ns = safe_event_time_ns + ms_to_ns(kHispeedRepeatInitialDelayMs);
        } else {
            repeat = {};
        }
    };

    if (f1_keycode_ != 0 && keycode == f1_keycode_) {
        update_repeat(judgement_line_decrease_repeat_, -kJudgementLinePositionStep, true);
        return true;
    }
    if (f2_keycode_ != 0 && keycode == f2_keycode_) {
        update_repeat(judgement_line_increase_repeat_, kJudgementLinePositionStep, true);
        return true;
    }
    if (f7_keycode_ != 0 && keycode == f7_keycode_) {
        update_repeat(visual_offset_decrease_repeat_, -kInPlayVisualOffsetStep, false);
        return true;
    }
    if (f8_keycode_ != 0 && keycode == f8_keycode_) {
        update_repeat(visual_offset_increase_repeat_, kInPlayVisualOffsetStep, false);
        return true;
    }
    return false;
}

void GameSession::service_tuning_repeats(int64_t now_ns) {
    const int64_t safe_now_ns = (now_ns > 0) ? now_ns : timing::HighResClock::now_ns();
    auto service = [&](TuningRepeat& repeat, double delta, bool judgement_line) {
        if (repeat.held && repeat.next_repeat_ns > 0 && safe_now_ns >= repeat.next_repeat_ns) {
            if (judgement_line) {
                adjust_judgement_line_position(delta);
            } else {
                adjust_visual_offset(delta);
            }
            // Do not replay a backlog after a stalled frame/audio callback.
            repeat.next_repeat_ns = safe_now_ns + tuning_repeat_interval_ns(safe_now_ns - repeat.started_ns);
        }
    };
    service(judgement_line_decrease_repeat_, -kJudgementLinePositionStep, true);
    service(judgement_line_increase_repeat_, kJudgementLinePositionStep, true);
    service(visual_offset_decrease_repeat_, -kInPlayVisualOffsetStep, false);
    service(visual_offset_increase_repeat_, kInPlayVisualOffsetStep, false);
}

void GameSession::reset_tuning_repeats() {
    judgement_line_decrease_repeat_ = {};
    judgement_line_increase_repeat_ = {};
    visual_offset_decrease_repeat_ = {};
    visual_offset_increase_repeat_ = {};
}

void GameSession::reset_control_repeats() {
    hispeed_decrease_held_ = false;
    hispeed_increase_held_ = false;
    hispeed_decrease_next_repeat_ns_ = 0;
    hispeed_increase_next_repeat_ns_ = 0;
    hispeed_decrease_started_ns_ = 0;
    hispeed_increase_started_ns_ = 0;
    reset_tuning_repeats();
}

void GameSession::service_control_repeats(int64_t now_ns, bool foreground) {
    // Background input may drop release edges. Never carry acceleration across
    // focus loss, a pause-resume countdown, or the end of a session.
    if (!foreground || finished_.load(std::memory_order_acquire) ||
        stop_requested_.load(std::memory_order_acquire) ||
        pause_resume_requested_.load(std::memory_order_acquire) ||
        resume_countdown_value_.load(std::memory_order_acquire) > 0) {
        reset_control_repeats();
        return;
    }
    service_hispeed_repeat(now_ns);
    service_tuning_repeats(now_ns);
}

void GameSession::service_hispeed_repeat(int64_t now_ns) {
    const int64_t safe_now_ns = (now_ns > 0) ? now_ns : timing::HighResClock::now_ns();
    if (hispeed_decrease_held_ &&
           hispeed_decrease_next_repeat_ns_ > 0 &&
           safe_now_ns >= hispeed_decrease_next_repeat_ns_) {
        adjust_hispeed(-kHispeedStep);
        hispeed_decrease_next_repeat_ns_ = safe_now_ns + tuning_repeat_interval_ns(safe_now_ns - hispeed_decrease_started_ns_);
    }
    if (hispeed_increase_held_ &&
           hispeed_increase_next_repeat_ns_ > 0 &&
           safe_now_ns >= hispeed_increase_next_repeat_ns_) {
        adjust_hispeed(kHispeedStep);
        hispeed_increase_next_repeat_ns_ = safe_now_ns + tuning_repeat_interval_ns(safe_now_ns - hispeed_increase_started_ns_);
    }
}

