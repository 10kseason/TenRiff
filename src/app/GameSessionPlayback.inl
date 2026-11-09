void GameSession::build_autoplay_events() {
    autoplay_events_.clear();
    autoplay_event_index_ = 0;
    if ((!autoplay_enabled_ && !auto_scratch_enabled_) || replay_playback_enabled_ || chart_.notes.empty()) {
        return;
    }

    autoplay_events_.reserve(chart_.notes.size() * 2);
    for (const auto& note : chart_.notes) {
        if (note.lane <= 0 || (!autoplay_enabled_ && !auto_scratch_lane(note.lane))) {
            continue;
        }

        autoplay_events_.push_back(gameplay::ReplayEvent{
            note.lane,
            input::InputState::Pressed,
            note.start_sample,
        });
        autoplay_events_.push_back(gameplay::ReplayEvent{
            note.lane,
            input::InputState::Released,
            note.end_sample.value_or(note.start_sample + 1),
        });
    }

    std::stable_sort(autoplay_events_.begin(),
                     autoplay_events_.end(),
                     [](const gameplay::ReplayEvent& lhs, const gameplay::ReplayEvent& rhs) {
                         if (lhs.sample != rhs.sample) {
                             return lhs.sample < rhs.sample;
                         }
                         if (lhs.state != rhs.state) {
                             return lhs.state == input::InputState::Released &&
                                    rhs.state == input::InputState::Pressed;
                         }
                         return lhs.lane < rhs.lane;
                     });
}

void GameSession::prepare_scheduled_input_audio(const gameplay::GameplayConfig& config) {
    scheduled_input_audio_.clear();
    scheduled_input_audio_index_ = 0;
    scheduled_input_audio_prepared_ = false;
    if (!chronological_judgement_ ||
        (!replay_playback_enabled_ && !autoplay_enabled_ && !auto_scratch_enabled_)) return;

    // Scratch playback is known, but other live lanes determine game-over.
    // Never simulate an incomplete live score to decide future scratch audio.
    if (auto_scratch_enabled_ && !autoplay_enabled_ && !replay_playback_enabled_) {
        for (const auto& note : chart_.notes) {
            if (auto_scratch_lane(note.lane))
                scheduled_input_audio_.push_back({note.start_sample, note.lane, note, std::nullopt});
        }
    } else {
        // Only loading pays for this temporary engine. It uses exactly the
        // playback policy/config to resolve replay misses, LN tails and mines;
        // the callback retains just audio events, never a second scoring engine.
        gameplay::GameplayEngine audio_plan_engine(chart_, config);
        const auto& events = replay_playback_enabled_ ? replay_source_.trace.events : autoplay_events_;
        std::vector<gameplay::MineTrigger> mines;
        const auto append_mines = [&]() {
            mines.clear();
            audio_plan_engine.drain_mine_triggers(mines);
            for (const auto& mine : mines)
                scheduled_input_audio_.push_back({mine.sample, mine.lane, std::nullopt, mine});
        };
        for (const auto& event : events) {
            const int64_t sample = replay_playback_enabled_
                ? playback_sample_for_replay_event(replay_source_, event.sample) : event.sample;
            auto note = audio_plan_engine.handle_input(event.lane, event.state, sample);
            if (event.state == input::InputState::Pressed)
                scheduled_input_audio_.push_back({sample, event.lane, std::move(note), std::nullopt});
            append_mines();
        }
        audio_plan_engine.advance(chart_.duration_samples);
        append_mines();
    }
    std::stable_sort(scheduled_input_audio_.begin(), scheduled_input_audio_.end(),
        [](const ScheduledInputAudio& lhs, const ScheduledInputAudio& rhs) { return lhs.sample < rhs.sample; });
    scheduled_input_audio_prepared_ = true;
}

void GameSession::schedule_input_audio(int64_t buffer_start_samples, int64_t buffer_end_samples) {
    if (!scheduled_input_audio_prepared_) return;
    while (scheduled_input_audio_index_ < scheduled_input_audio_.size()) {
        const auto& event = scheduled_input_audio_[scheduled_input_audio_index_];
        if (event.sample > buffer_end_samples) break;
        const int64_t audible = pin_realtime_audio_start_sample(event.sample, buffer_start_samples);
        if (event.mine) {
            schedule_mine_keysound(*event.mine, audible);
        } else {
            schedule_tone(event.lane, audible, false);
            if (event.note && (!auto_scratch_enabled_ || autoplay_enabled_ || replay_playback_enabled_ ||
                               !engine_->is_game_over()))
                schedule_note_keysound(*event.note, audible);
        }
        ++scheduled_input_audio_index_;
    }
}

void GameSession::process_autoplay_queue(int64_t buffer_start_samples,
                                         int64_t buffer_end_samples,
                                         int64_t lookahead_samples) {
    if ((!autoplay_enabled_ && !auto_scratch_enabled_) || replay_playback_enabled_ || !engine_) {
        return;
    }

    while (autoplay_event_index_ < autoplay_events_.size()) {
        const auto& autoplay_event = autoplay_events_[autoplay_event_index_];
        if (autoplay_event.sample > buffer_end_samples + lookahead_samples) {
            break;
        }
        dispatch_lane_input(autoplay_event.lane,
                            autoplay_event.state,
                            autoplay_event.sample,
                            buffer_start_samples, std::nullopt, !scheduled_input_audio_prepared_);
        ++autoplay_event_index_;
    }
}

int64_t GameSession::playback_sample_for_replay_event(const gameplay::ReplayFile& replay,
                                                      int64_t replay_sample) const {
    const int replay_sample_rate = replay.trace.sample_rate > 0
                                       ? replay.trace.sample_rate
                                       : replay.sample_rate;
    if (replay_sample_rate <= 0 || replay_sample_rate == sample_rate_) {
        return replay_sample;
    }
    return static_cast<int64_t>(std::llround(static_cast<long double>(replay_sample) *
                                             static_cast<long double>(sample_rate_) /
                                             static_cast<long double>(replay_sample_rate)));
}

void GameSession::process_replay_input_queue(int64_t buffer_start_samples,
                                             int64_t buffer_end_samples,
                                             int64_t lookahead_samples) {

    while (true) {
        auto maybe_event = input_thread_.queue().pop();
        if (!maybe_event.has_value()) {
            break;
        }
        note_runtime_input_event_source(*maybe_event);
        if (handle_control_input(*maybe_event) &&
            finished_.load(std::memory_order_acquire)) {
            return;
        }
    }

    while (replay_event_index_ < replay_source_.trace.events.size()) {
        const auto& replay_event = replay_source_.trace.events[replay_event_index_];
        const int64_t playback_sample = playback_sample_for_replay_event(replay_source_, replay_event.sample);
        if (playback_sample > (judgement_horizon_active_
                ? judgement_horizon_sample_ : buffer_end_samples + lookahead_samples)) {
            break;
        }
        dispatch_lane_input(replay_event.lane,
                            replay_event.state,
                            playback_sample,
                            buffer_start_samples, std::nullopt, !scheduled_input_audio_prepared_);
        ++replay_event_index_;
    }
}

void GameSession::dispatch_ghost_lane_input(int lane, input::InputState state, int64_t sample) {
    if (!ghost_engine_) {
        return;
    }

    const int pressed_lane_index = lane - 1;
    if (pressed_lane_index >= 0 && pressed_lane_index < static_cast<int>(ghost_lane_pressed_.size())) {
        ghost_lane_pressed_[static_cast<std::size_t>(pressed_lane_index)] =
            state == input::InputState::Pressed ? 1 : 0;
        if (state == input::InputState::Pressed) {
            ghost_lane_activity_[static_cast<std::size_t>(pressed_lane_index)] = 1.0f;
        }
    }

    auto hit_note = ghost_engine_->handle_input(lane, state, sample);
    if (state == input::InputState::Pressed && hit_note.has_value() &&
        is_successful_hit(ghost_engine_->live_feedback().judgement)) {
        if (pressed_lane_index >= 0 && pressed_lane_index < static_cast<int>(ghost_lane_hit_time_ns_.size())) {
            ghost_lane_hit_time_ns_[static_cast<std::size_t>(pressed_lane_index)] = hit_effect_clock_ns_;
        }
        if (hit_note->note_id < ghost_hidden_hit_note_ids_.size()) {
            ghost_hidden_hit_note_ids_[hit_note->note_id] = 1;
        }
    }
}

bool GameSession::score_reference_matches(const gameplay::ReplayFile& replay, bool seed_independent) const {
    if (!live_score_replay_policy_matches(replay.ruleset_id,
        replay_ruleset_id_for_runtime(config_.judge, config_.gauge, gauge_shift_enabled_, course_gauge_enabled_, "off", active_mods_),
        replay.mode.key_conversion_version, key_conversion_version_, replay.mode.bms_timing_version, bms_timing_version_,
        replay.mode.bms_gauge_version, bms_gauge_version_, replay.mode.bms_control_flow_version, bms_control_flow_version_,
        replay.mode.autoplay_enabled || replay.mode.practice_no_fail_enabled, replay.aborted)) return false;
    if (replay.version != gameplay::kNativeScoreVersion || replay.trace.lane_count != chart_.lane_count ||
        !(replay.trace.sample_rate > 0 || replay.sample_rate > 0) || !std::isfinite(replay.rate) || replay.rate <= 0 ||
        std::abs(replay.rate - config_.speed.rate) > 0.001 || !std::isfinite(replay.score_multiplier) ||
        std::abs(replay.score_multiplier - score_multiplier_) > 0.000001 || !replay.mode.course_gauge.empty() ||
        replay.mode.one_miss_fail_enabled != one_miss_fail_enabled_) return false;
    if (normalize_runtime_key_mode_local(replay.mode.key_mode) != normalize_runtime_key_mode_local(config_.mode.key_mode) ||
        replay.mode.key_conversion_algorithm != config_.mode.key_conversion_algorithm ||
        ((config_.mode.key_conversion_algorithm == "nk2" || config_.mode.key_conversion_algorithm == "nk3") &&
         normalize_nk2_preset_local(replay.mode.key_conversion_nk2_preset) != normalize_nk2_preset_local(config_.mode.key_conversion_nk2_preset)) ||
        (!replay.mode.key_conversion_note_add_mode.empty() && replay.mode.key_conversion_note_add_mode != "default") ||
        replay.mode.random != config_.mode.random || !equivalent_mode_mod_tokens(replay.mods, active_mods_)) return false;
    if ((!seed_independent && (!replay.mode.random_seed.has_value() ||
         replay.mode.random_seed.value() != static_cast<int>(config_.mode.random_seed))) ||
        (mode_mod_long_note_mix(active_mods_) && replay.mode.long_note_mix_version != long_note_mix_version_) ||
        (chart_.bms_has_random && gameplay::replay_bms_random_seed(replay.mode) != bms_random_seed_)) return false;
    const auto hashes = hash_chart_file_utf8(chart_path_);
    return hashes.valid() && !replay.chart_sha256.empty() && hashes.sha256 == replay.chart_sha256;
}

void GameSession::initialize_score_reference(const gameplay::GameplayConfig& config, bool seed_independent) {
    reset_score_reference();
    if (!config_.skin.score_comparison_enabled || score_comparison_replay_path_.empty() ||
        replay_playback_enabled_ || autoplay_enabled_ || practice_no_fail_enabled_ || auto_scratch_enabled_ ||
        (practice_start_seconds_ > 0.0 && !instant_restart_start_)) return;
    auto loaded = gameplay::load_replay_json(score_comparison_replay_path_);
    if (!loaded.success() || !score_reference_matches(*loaded.replay, seed_independent)) {
        std::cerr << "[warn] Score comparison replay is unavailable or incompatible; visible ghost is unchanged." << std::endl;
        return;
    }
    score_reference_source_ = std::move(*loaded.replay);
    auto reference_config = config;
    reference_config.judge = replay_judge_config_for_playback(score_reference_source_, config_.judge, chart_.bms_rank);
    reference_config.strict_standard_hold_release = replay_uses_strict_standard_hold_release(score_reference_source_);
    reference_config.great_standard_hold_release = replay_uses_great_standard_hold_release(score_reference_source_);
    reference_config.legacy_hold_release = replay_uses_legacy_hold_release(score_reference_source_);
    reference_config.chronological_judgement = replay_uses_chronological_judgement(score_reference_source_);
    reference_config.empty_poor_affects_accuracy = replay_empty_poor_affects_accuracy(score_reference_source_);
    reference_config.initial_gauge_value.reset();
    if (is_gauge_shift_token(score_reference_source_.mode.gauge)) reference_config.initial_gauge = game::GaugeType::ExHard;
    else if (auto gauge = parse_gauge_type(score_reference_source_.mode.gauge)) reference_config.initial_gauge = *gauge;
    reference_config.practice_no_fail_enabled = false;
    reference_config.one_miss_fail_enabled = score_reference_source_.mode.one_miss_fail_enabled;
    score_reference_engine_ = std::make_unique<gameplay::GameplayEngine>(chart_, reference_config);
    score_reference_mine_triggers_.reserve(chart_.mines.size());
}

void GameSession::advance_score_reference(int64_t sample) {
    if (!score_reference_engine_ || paused_.load(std::memory_order_relaxed) || sample < score_reference_sample_) return;
    // Only recorded edges due at this exact chart time are fed to this engine.
    // No audio scheduling, physical key state, visible-ghost or player writes.
    while (score_reference_event_index_ < score_reference_source_.trace.events.size()) {
        const auto& event = score_reference_source_.trace.events[score_reference_event_index_];
        const auto due = playback_sample_for_replay_event(score_reference_source_, event.sample);
        if (due > sample) break;
        (void)score_reference_engine_->handle_input(event.lane, event.state, due);
        ++score_reference_event_index_;
    }
    score_reference_engine_->advance(sample);
    score_reference_sample_ = sample;
    score_reference_mine_triggers_.clear();
    score_reference_engine_->drain_mine_triggers(score_reference_mine_triggers_);
}

void GameSession::reset_score_reference() {
    score_reference_engine_.reset();
    score_reference_source_ = {};
    score_reference_event_index_ = 0;
    score_reference_sample_ = (std::numeric_limits<int64_t>::min)();
    score_reference_mine_triggers_.clear();
}

void GameSession::process_ghost_replay_queue(int64_t buffer_end_samples, int64_t lookahead_samples) {
    if (!ghost_engine_ || !ghost_replay_enabled_) {
        return;
    }

    while (ghost_replay_event_index_ < ghost_replay_source_.trace.events.size()) {
        const auto& replay_event = ghost_replay_source_.trace.events[ghost_replay_event_index_];
        const int64_t playback_sample =
            playback_sample_for_replay_event(ghost_replay_source_, replay_event.sample);
        if (playback_sample > buffer_end_samples + lookahead_samples) {
            break;
        }
        dispatch_ghost_lane_input(replay_event.lane, replay_event.state, playback_sample);
        ++ghost_replay_event_index_;
    }
}

void GameSession::schedule_note_guides(int64_t buffer_start_samples, int64_t buffer_end_samples) {
    if (!synthetic_tones_enabled_.load(std::memory_order_acquire)) {
        return;
    }
    while (next_guide_note_index_ < chart_.notes.size()) {
        const auto& note = chart_.notes[next_guide_note_index_];
        if (note.start_sample > buffer_end_samples) {
            break;
        }
        if (note.start_sample >= buffer_start_samples) {
            schedule_tone(note.lane, note.start_sample, true);
        }
        ++next_guide_note_index_;
    }
}

void GameSession::schedule_tone(int lane, int64_t sample, bool guide) {
    if (!synthetic_tones_enabled_.load(std::memory_order_acquire)) {
        return;
    }
    if (sample_rate_ <= 0 || lane <= 0) {
        return;
    }
    if (tone_voices_.size() >= kMaxToneVoices) {
        tone_voices_.erase(tone_voices_.begin());
    }

    const int64_t duration_samples = std::max<int64_t>(
        8, ms_to_samples(guide ? kGuideToneMs : kHitToneMs, sample_rate_));
    if (duration_samples <= 0) {
        return;
    }

    const int lane_count = std::max(1, chart_.lane_count);
    const double normalized = (lane_count <= 1)
                                  ? 0.0
                                  : (-1.0 + 2.0 * static_cast<double>(lane - 1) /
                                                static_cast<double>(lane_count - 1));
    const double pan = std::clamp(normalized, -0.90, 0.90);
    const double gain = guide ? kGuideToneGain : kHitToneGain;
    const double frequency = lane_frequency_hz(lane) * (guide ? 0.5 : 1.0);

    ToneVoice voice;
    voice.start_sample = sample;
    voice.end_sample = sample + duration_samples;
    voice.phase = 0.0;
    voice.phase_step = kTwoPi * frequency / static_cast<double>(sample_rate_);
    voice.gain_l = static_cast<float>(gain * (1.0 - pan) * 0.5);
    voice.gain_r = static_cast<float>(gain * (1.0 + pan) * 0.5);
    tone_voices_.push_back(voice);
}

void GameSession::mix_tones(float* output, uint32_t frames, int64_t buffer_start_samples) {
    if (!output || frames == 0 || tone_voices_.empty()) {
        return;
    }

    const int64_t buffer_end_samples = buffer_start_samples + static_cast<int64_t>(frames);
    std::size_t write_index = 0;
    for (std::size_t i = 0; i < tone_voices_.size(); ++i) {
        auto voice = tone_voices_[i];
        if (voice.end_sample <= buffer_start_samples) {
            continue;
        }

        const int64_t start_offset = std::max<int64_t>(0, voice.start_sample - buffer_start_samples);
        const int64_t end_offset =
            std::min<int64_t>(static_cast<int64_t>(frames), voice.end_sample - buffer_start_samples);
        const int64_t duration = std::max<int64_t>(1, voice.end_sample - voice.start_sample);

        for (int64_t frame = start_offset; frame < end_offset; ++frame) {
            const int64_t abs_sample = buffer_start_samples + frame;
            const double progress =
                static_cast<double>(abs_sample - voice.start_sample) / static_cast<double>(duration);
            const double envelope = std::clamp(1.0 - progress, 0.0, 1.0);
            const float value = static_cast<float>(std::sin(voice.phase) * envelope);

            const std::size_t index = static_cast<std::size_t>(frame) * 2;
            output[index] += value * voice.gain_l;
            output[index + 1] += value * voice.gain_r;

            voice.phase += voice.phase_step;
            if (voice.phase >= kTwoPi) {
                voice.phase = std::fmod(voice.phase, kTwoPi);
            }
        }

        if (voice.end_sample <= buffer_end_samples) {
            continue;
        }
        tone_voices_[write_index++] = voice;
    }
    tone_voices_.resize(write_index);

    const std::size_t sample_count = static_cast<std::size_t>(frames) * 2;
    for (std::size_t i = 0; i < sample_count; ++i) {
        output[i] = std::clamp(output[i], -1.0f, 1.0f);
    }
}

std::optional<int> GameSession::lane_from_keycode(uint32_t keycode) const {
    auto it = key_to_lane_.find(keycode);
    if (it == key_to_lane_.end()) {
        return std::nullopt;
    }
    return it->second;
}

double GameSession::lane_frequency_hz(int lane) const {
    const int clamped_lane = std::max(1, lane);
    const int semitone = clamped_lane - 1;
    return 220.0 * std::pow(2.0, static_cast<double>(semitone) / 12.0);
}

std::string GameSession::find_first_chart(const std::string& root_path) const {
    namespace fs = std::filesystem;
    std::error_code ec;
#ifdef _WIN32
    fs::path root_dir = fs::u8path(root_path);
#else
    fs::path root_dir(root_path);
#endif
    if (!fs::exists(root_dir, ec) || !fs::is_directory(root_dir, ec)) {
        return {};
    }

    const std::vector<std::string> extensions = {".bms", ".bme", ".bml", ".pms"};

    fs::directory_options options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(root_dir, options, ec);
    fs::recursive_directory_iterator end;
    while (it != end) {
        if (ec) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        const fs::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        ec.clear();

        auto ext = entry.path().extension().u8string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
            return entry.path().u8string();
        }
        it.increment(ec);
    }

    return {};
}

}  // namespace tenriff::app
