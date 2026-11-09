bool GameSession::FutureQueue::push(const FutureEvent& evt) {
    std::size_t next = (head + 1) % kCapacity;
    if (next == tail) {
        return false;
    }
    data[head] = evt;
    head = next;
    return true;
}

std::optional<GameSession::FutureEvent> GameSession::FutureQueue::pop() {
    if (head == tail) {
        return std::nullopt;
    }
    auto value = data[tail];
    tail = (tail + 1) % kCapacity;
    return value;
}

std::optional<GameSession::FutureEvent> GameSession::FutureQueue::peek() const {
    if (head == tail) {
        return std::nullopt;
    }
    return data[tail];
}

void GameSession::FutureQueue::consume() {
    if (head != tail) {
        tail = (tail + 1) % kCapacity;
    }
}

void GameSession::rebuild_input_thread_config(input::InputThreadConfig& input_config) const {
    input_config.backend = config_.input.rawinput ? input::InputBackend::RawInput
                                                  : input::InputBackend::Polling;
    input_config.gate_policy = gameplay_input_gate_policy();
    // Gameplay keeps a bound-key polling shadow inside InputThread.
    // RawInput remains the low-latency primary path, while the same single
    // KeyStateTracker deduplicates polling edges if WM_INPUT silently stops.
    input_config.rawinput_polling_shadow = config_.input.rawinput;
    input_config.raw_input.register_keyboard = config_.input.rawinput;
    input_config.raw_input.input_sink = true;
    input_config.raw_input.no_legacy = false;
    input_config.polling_hz = config_.input.polling_hz;
    input_config.key_state.debounce_window_ns =
        std::max<int64_t>(0, static_cast<int64_t>(std::llround(config_.input.debounce_ms * 1'000'000.0)));

    auto append_unique_key = [&input_config](uint32_t keycode) {
        if (keycode == 0) {
            return;
        }
        if (std::find(input_config.polling_keys.begin(), input_config.polling_keys.end(), keycode) !=
            input_config.polling_keys.end()) {
            return;
        }
        input_config.polling_keys.push_back(keycode);
    };

    for (const auto& tracked : polled_gameplay_keys_) {
        append_unique_key(tracked.keycode);
    }
    if (input_config.polling_keys.empty()) {
        for (const auto& [keycode, lane] : key_to_lane_) {
            static_cast<void>(lane);
            append_unique_key(keycode);
        }
        append_unique_key(escape_keycode_);
        append_unique_key(up_keycode_);
        append_unique_key(down_keycode_);
        append_unique_key(enter_keycode_);
        append_unique_key(f1_keycode_);
        append_unique_key(f2_keycode_);
        append_unique_key(f3_keycode_);
        append_unique_key(f4_keycode_);
        append_unique_key(f5_keycode_);
        append_unique_key(f6_keycode_);
        append_unique_key(f7_keycode_);
        append_unique_key(f8_keycode_);
        append_unique_key(f9_keycode_);
        append_unique_key(f10_keycode_);
        append_unique_key(f11_keycode_);
        append_unique_key(instant_restart_keycode_);
    }
}

void GameSession::note_runtime_input_event_source(const input::InputEvent& event) {
    sync_runtime_input_backend_state(input_backend_state_, event, InputFallbackOrigin::Gameplay);
}

void GameSession::rebuild_polled_gameplay_keys() {
    polled_gameplay_keys_.clear();
    polled_gameplay_keys_.reserve(key_to_lane_.size() + 15);

    auto append_unique_key = [this](uint32_t keycode) {
        if (keycode == 0) {
            return;
        }
        for (const auto& tracked : polled_gameplay_keys_) {
            if (tracked.keycode == keycode) {
                return;
            }
        }
        polled_gameplay_keys_.push_back(PolledGameplayKey{keycode});
    };

    for (const auto& [keycode, lane] : key_to_lane_) {
        static_cast<void>(lane);
        append_unique_key(keycode);
    }
    append_unique_key(escape_keycode_);
    append_unique_key(up_keycode_);
    append_unique_key(down_keycode_);
    append_unique_key(enter_keycode_);
    append_unique_key(f1_keycode_);
    append_unique_key(f2_keycode_);
    append_unique_key(f3_keycode_);
    append_unique_key(f4_keycode_);
    append_unique_key(f5_keycode_);
    append_unique_key(f6_keycode_);
    append_unique_key(f7_keycode_);
    append_unique_key(f8_keycode_);
    append_unique_key(f9_keycode_);
    append_unique_key(f10_keycode_);
    append_unique_key(f11_keycode_);
    append_unique_key(instant_restart_keycode_);
}

GameSession::GameSession() = default;

void GameSession::prepare_riff_map() {
    const auto map = gameplay::build_riff_map(chart_);
    riff_map_ = map.values;
    riff_map_count_ = map.count;
    // A new GameSession is created for each song. A process-wide generation
    // prevents the renderer from reusing another song's static density map.
    static std::atomic<uint64_t> next_revision{0};
    riff_map_revision_ = next_revision.fetch_add(1, std::memory_order_relaxed) + 1;
}

GameSession::~GameSession() {
    shutdown();
}

bool GameSession::initialize(const CommandLineOptions& options) {
    memory_deferral_.begin();
    options_ = options;
    result_ = {};
    stop_requested_.store(false, std::memory_order_release);
    finished_.store(false, std::memory_order_release);
    spectating_peer_.store(false, std::memory_order_release);
    user_aborted_.store(false, std::memory_order_release);
    audio_error_message_.clear();
    paused_.store(false, std::memory_order_release);
    pause_used_.store(false, std::memory_order_release);
    pause_resume_requested_.store(false, std::memory_order_release);
    resume_countdown_value_.store(0, std::memory_order_release);
    pause_resume_end_sample_ = 0;
    restart_requested_.store(false, std::memory_order_release);
    instant_restart_requested_.store(false, std::memory_order_release);
    instant_restart_key_armed_ = true;
    instant_restart_keycode_ = 0;
    exit_requested_.store(false, std::memory_order_release);
    pause_menu_cursor_.store(0, std::memory_order_release);
    pause_sample_offset_ = 0;
    pause_physical_start_sample_ = 0;
    paused_chart_sample_ = 0;
    paused_playback_sample_ = 0;
    pause_anchor_valid_ = false;
    last_audio_sample_.store(0, std::memory_order_release);
    audio_timing_sequence_.store(0, std::memory_order_release);
    last_audio_timing_.store({});
    startup_input_timing_anchor_ = {};
    clock_sync_.reset();
    shared_input_clock_.reset();
    audio_input_timing_anchor_ = {};
    stale_input_catchup_used_ = false;
    judgement_horizon_sample_ = 0;
    judgement_horizon_active_ = false;
    chronological_judgement_ = true;
    ghost_chronological_judgement_ = false;
    visual_scroll_segments_.reset();
    scheduled_input_audio_.clear();
    scheduled_input_audio_index_ = 0;
    scheduled_input_audio_prepared_ = false;
    countdown_active_ = false;
    countdown_value_ = 0;
    countdown_started_ns_ = 0;
    reset_control_repeats();
    result_transition_sample_ = 0;
    result_transition_pending_ = false;
    gameplay_started_ = false;
    active_mods_.clear();
    rate_multiplier_ = 1.0;
    score_multiplier_ = 1.0;
    replay_source_ = {};
    replay_source_path_.clear();
    replay_playback_enabled_ = false;
    replay_event_index_ = 0;
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
    ghost_replay_source_ = {};
    ghost_replay_enabled_ = false;
    score_comparison_compatible_ = false;
    reset_score_reference();
    ghost_replay_event_index_ = 0;
    judgement_loop_plan_ = {};
    judgement_loop_step_carry_ = 0;

    future_events_ = {};
    input_backend_state_ = {};
    polled_gameplay_keys_.clear();
    tone_voices_.clear();
    stop_chart_audio_workers();
    chart_audio_assets_.clear();
    chart_audio_events_.clear();
    chart_audio_voices_.clear();
    chart_audio_load_queue_.clear();
    chart_audio_pcm_registry_.clear();
    chart_audio_deduplicator_.clear();
    pending_input_events_.clear();
    hidden_hit_note_ids_.clear();
    ghost_hidden_hit_note_ids_.clear();
    active_holds_buffer_.clear();
    ghost_active_holds_buffer_.clear();
    next_chart_audio_event_ = 0;
    startup_preload_budget_bytes_ = 0;
    chart_audio_decoded_bytes_ = 0;
    chart_audio_deferred_count_ = 0;
    last_chart_audio_service_sample_ = (std::numeric_limits<int64_t>::min)();
    chart_audio_startup_logged_ = false;
    chart_audio_steady_state_logged_ = false;
    synthetic_tones_enabled_.store(true, std::memory_order_release);
    next_guide_note_index_ = 0;
    hud_scan_start_ = 0;
    ghost_hud_scan_start_ = 0;
    hud_past_hold_indices_.clear();
    ghost_hud_past_hold_indices_.clear();
    hud_scan_cutoff_sample_ = (std::numeric_limits<int64_t>::min)();
    chart_ = {};
    riff_map_.fill(0);
    riff_map_count_ = 0;
    chart_base_bpm_ = 0.0;
    lane_activity_.clear();
    ghost_lane_activity_.clear();
    lane_hit_time_ns_.fill(0);
    ghost_lane_hit_time_ns_.fill(0);
    lane_pressed_.clear();
    ghost_lane_pressed_.clear();
    pending_input_events_.reserve(64);
    last_loading_percent_ = -1;
    last_loading_stage_.clear();
    report_loading_progress(0, "Loading profile");
    if (loading_cancel_requested()) {
        return false;
    }

    const std::filesystem::path profile_dir =
#ifdef _WIN32
        std::filesystem::u8path("profiles") / std::filesystem::u8path(options.profile);
#else
        std::filesystem::path("profiles") / std::filesystem::path(options.profile);
#endif
    profile_dir_ = profile_dir.u8string();

    config::ConfigLoader config_loader;
    auto config_result = config_loader.load_profile(profile_dir_);
    if (!config_result.success()) {
        return false;
    }
    config_ = config_result.config;
    if (practice_no_fail_override_) {
        config_.mode.practice_no_fail_enabled = true;
        config_.mode.one_miss_fail_enabled = false;
        config_.mode.autoplay_enabled = false;
    }
    const bool profile_rawinput = config_.input.rawinput;
    const bool migrated_config = config_result.migrated;
    const bool stripped_session_only_mods = strip_session_only_mode_mods(config_);

    if (config_result.used_defaults || migrated_config || stripped_session_only_mods) {
        const config::RuntimeConfig persisted = build_persisted_runtime_config(config_);
        config_loader.save_profile(profile_dir_, persisted);
    }
    // Judge selection intentionally does not persist. Carry the menu's current
    // selection across the profile reload, but never into replay or peer rules.
    if (!peer_battle_mode_ && options.replay_path.empty()) {
        apply_session_only_mode_mods(config_, session_mode_mods_);
    }
    if (peer_battle_mode_ && !apply_peer_battle_rules(config_, peer_battle_rate_milli_)) {
        std::cerr << "[error] The negotiated multiplayer Rate is invalid." << std::endl;
        return false;
    }
    if (force_polling_input_ && profile_rawinput) {
        // This is a process-lifetime recovery override. Keep the saved profile
        // on RawInput so an explicit retry or app restart can select it again.
        config_.input.rawinput = false;
        config_.input.backend = "polling";
    }
    input_backend_state_.configured_backend = profile_rawinput ? input::InputBackend::RawInput
                                                               : input::InputBackend::Polling;
    input_backend_state_.effective_backend = config_.input.rawinput ? input::InputBackend::RawInput
                                                                    : input::InputBackend::Polling;
    if (force_polling_input_ && profile_rawinput) {
        input_backend_state_.auto_fallback = true;
        input_backend_state_.fallback_origin =
            forced_polling_input_state_.fallback_origin == InputFallbackOrigin::None
                ? InputFallbackOrigin::Menu
                : forced_polling_input_state_.fallback_origin;
        input_backend_state_.fallback_reason = forced_polling_input_state_.fallback_reason.empty()
                                                   ? "A prior RawInput failure latched Polling for the rest of this app run."
                                                   : forced_polling_input_state_.fallback_reason;
        input_backend_state_.fallback_timestamp_utc =
            forced_polling_input_state_.fallback_timestamp_utc.empty()
                ? utc_timestamp_compact()
                : forced_polling_input_state_.fallback_timestamp_utc;
    }
    autoplay_enabled_ = config_.mode.autoplay_enabled;
    practice_no_fail_enabled_ = config_.mode.practice_no_fail_enabled;
    one_miss_fail_enabled_ = config_.mode.one_miss_fail_enabled;
    pacemaker_mode_ = config::normalize_pacemaker_mode_token(config_.mode.pacemaker_mode);
    report_loading_progress(12, "Loading keymap");
    if (loading_cancel_requested()) {
        return false;
    }

    config::KeymapManager keymap_manager;
    auto keymap_result = keymap_manager.load_profile(profile_dir_);
    if (!keymap_result.success()) {
        return false;
    }
    keymap_ = keymap_result.keymap;
    for (const auto& warning : keymap_result.warnings) {
        std::cerr << "[warn] " << warning << std::endl;
    }

    if (keymap_result.used_defaults || keymap_result.rewritten()) {
        keymap_manager.save_profile(profile_dir_, keymap_);
    }
    report_loading_progress(22, "Resolving chart");
    if (loading_cancel_requested()) {
        return false;
    }

    // The launched room rate is authoritative over this player's --rate. It is
    // session-only and never saved to the profile.
    if (options.has_rate && !peer_battle_mode_) {
        config_.speed.rate = options.rate;
    }
    if (options.has_hispeed) {
        config_.speed.hi_speed = options.hispeed;
    }
    escape_keycode_ = config::KeycodeMap::to_keycode("Esc").value_or(0);
    up_keycode_ = config::KeycodeMap::to_keycode("Up").value_or(0);
    down_keycode_ = config::KeycodeMap::to_keycode("Down").value_or(0);
    enter_keycode_ = config::KeycodeMap::to_keycode("Enter").value_or(0);
    f1_keycode_ = config::KeycodeMap::to_keycode(config::function_key(config_.ui, 1)).value_or(0);
    f2_keycode_ = config::KeycodeMap::to_keycode(config::function_key(config_.ui, 2)).value_or(0);
    f3_keycode_ = config::KeycodeMap::to_keycode(config::function_key(config_.ui, 3)).value_or(0);
    f4_keycode_ = config::KeycodeMap::to_keycode(config::function_key(config_.ui, 4)).value_or(0);
    f5_keycode_ = 0;
    f6_keycode_ = 0;
    hispeed_coarse_decrease_held_ = false;
    hispeed_coarse_increase_held_ = false;
    f7_keycode_ = config::KeycodeMap::to_keycode(config::function_key(config_.ui, 7)).value_or(0);
    f8_keycode_ = config::KeycodeMap::to_keycode(config::function_key(config_.ui, 8)).value_or(0);
    f9_keycode_ = config::KeycodeMap::to_keycode(config::function_key(config_.ui, 9)).value_or(0);
    f10_keycode_ = config::KeycodeMap::to_keycode(config::function_key(config_.ui, 10)).value_or(0);
    f11_keycode_ = config::KeycodeMap::to_keycode(config::function_key(config_.ui, 11)).value_or(0);

    if (!options.replay_path.empty()) {
        auto replay_load = gameplay::load_replay_json(options.replay_path);
        if (!replay_load.success()) {
            std::cerr << "[warn] Failed to load replay " << options.replay_path
                      << ": " << replay_load.error << std::endl;
            return false;
        }
        replay_source_ = std::move(replay_load.replay.value());
        pause_used_.store(replay_source_.pause_used, std::memory_order_release);
        replay_source_path_ = options.replay_path;
        replay_playback_enabled_ = true;
        replay_event_index_ = 0;
        for (const auto& warning : replay_load.warnings) {
            std::cerr << "[warn] " << warning << std::endl;
        }
        if (!options.has_rate && replay_source_.rate > 0.0) {
            config_.speed.rate = replay_source_.rate;
        }
        if (!replay_source_.mods.empty() ||
            replay_source_.replay_format_version >= gameplay::kReplayFormatVersion) {
            config_.mode.mods = replay_source_.mods;
        }
        if (!replay_source_.mode.key_mode.empty()) {
            config_.mode.key_mode = replay_source_.mode.key_mode;
        } else {
            const std::string inferred_key_mode = replay_key_mode_token(replay_source_.trace.lane_count);
            if (!inferred_key_mode.empty()) {
                config_.mode.key_mode = inferred_key_mode;
            }
        }
        if (!replay_source_.mode.key_conversion_algorithm.empty()) {
            config_.mode.key_conversion_algorithm = replay_source_.mode.key_conversion_algorithm;
        }
        if (!replay_source_.mode.key_conversion_nk2_preset.empty()) {
            config_.mode.key_conversion_nk2_preset =
                normalize_nk2_preset_local(replay_source_.mode.key_conversion_nk2_preset);
        } else if (config_.mode.key_conversion_algorithm == "nk2" ||
                   config_.mode.key_conversion_algorithm == "nk3") {
            // Replays written before the preset field existed used Native 12%.
            config_.mode.key_conversion_nk2_preset = "native";
        }
        if (!replay_source_.mode.key_conversion_note_add_mode.empty() &&
            replay_source_.mode.key_conversion_note_add_mode != "default") {
            std::cerr << "[warn] Replay uses the removed Conversion Note Add option and cannot be reproduced."
                      << std::endl;
            return false;
        }
        if (!replay_source_.mode.random.empty()) {
            config_.mode.random = replay_source_.mode.random;
        } else if (config_.mode.random != "off") {
            std::cerr << "[warn] Replay file does not contain random-mode metadata; current random setting will be used."
                      << std::endl;
        }
        if (replay_source_.mode.random_seed.has_value()) {
            config_.mode.random_seed = replay_source_.mode.random_seed.value();
        }
        if (!replay_source_.mode.gauge.empty()) {
            config_.mode.gauge = replay_source_.mode.gauge;
        }
        autoplay_enabled_ = false;
        auto_scratch_enabled_ = false;
        practice_no_fail_enabled_ = replay_source_.mode.practice_no_fail_enabled;
        one_miss_fail_enabled_ = replay_source_.mode.one_miss_fail_enabled;
        pacemaker_mode_ = "off";
    }
    // Every new solo play (including Retry) gets a fresh seed. Replays retain
    // their recorded seed; peer rooms keep their shared, random-OFF contract.
    if (!replay_playback_enabled_ && !peer_battle_mode_) {
        config_.mode.random_seed = next_random_session_seed();
    }
    if (!replay_playback_enabled_ && !options.ghost_replay_path.empty()) {
        auto ghost_load = gameplay::load_replay_json(options.ghost_replay_path);
        if (!ghost_load.success()) {
            std::cerr << "[warn] Failed to load ghost replay " << options.ghost_replay_path
                      << ": " << ghost_load.error << std::endl;
        } else {
            ghost_replay_source_ = std::move(ghost_load.replay.value());
            ghost_replay_enabled_ = true;
            ghost_replay_event_index_ = 0;
            for (const auto& warning : ghost_load.warnings) {
                std::cerr << "[warn] " << warning << std::endl;
            }
        }
    }

    std::string chart_path = options.chart_path;
    if (chart_path.empty() && replay_playback_enabled_) {
        chart_path = replay_source_.chart_path;
    }
    if (chart_path.empty()) {
        chart_path = find_first_chart(options.songs_path);
    }
    if (chart_path.empty()) {
        return false;
    }
    chart_path_ = chart_path;
    report_loading_progress(42, "Resolving chart");
    if (loading_cancel_requested()) {
        return false;
    }

    auto initialize_audio_session = [this](uint32_t requested_rate) -> bool {
        audio_thread_.shutdown();
        config_.audio.sample_rate = requested_rate;
        auto audio_result = audio_thread_.initialize(config_.audio, [this](float* output,
                                                                           uint32_t frames,
                                                                           int64_t buffer_start,
                                                                           int64_t playback_sample) {
            audio_callback(output, frames, buffer_start, playback_sample);
        });
        if (audio_result != audio::AudioResult::Success) {
            audio_error_message_ = audio_thread_.error_message();
            if (audio_error_message_.empty()) {
                audio_error_message_ = "Audio output is unavailable. Select a connected device in Settings > Audio > Sound Output Device, then retry.";
            }
            std::cerr << "[error] " << audio_error_message_ << std::endl;
            return false;
        }
        sample_rate_ = static_cast<int>(audio_thread_.sample_rate());
        if (sample_rate_ <= 0) {
            sample_rate_ = static_cast<int>(requested_rate);
        }
        mix_normalizer_.reset(sample_rate_);
        audio_error_message_.clear();
        input_offset_samples_ = ms_to_samples(config_.input_offset_ms, sample_rate_);
        return true;
    };

    bms_timing_version_ = replay_playback_enabled_ ? replay_source_.mode.bms_timing_version : 2;
    bms_control_flow_version_ = replay_playback_enabled_ ? replay_source_.mode.bms_control_flow_version :
        (peer_battle_mode_ ? 1 : 2);
    // Keep branch choices stable across device-rate reloads and independent of SR/conversion seeds.
    bms_random_seed_ = replay_playback_enabled_ ? gameplay::replay_bms_random_seed(replay_source_.mode)
        : (peer_battle_mode_ ? config_.mode.random_seed : next_random_session_seed());
    auto load_chart_for_session_rate = [this](ChartLoadResult& out_result) -> bool {
        ChartLoader loader;
        out_result = loader.load(chart_path_, sample_rate_, config_.speed.rate,
                                  config_.audio_ui.bms_keysound_policy,
                                  bms_random_seed_, bms_timing_version_, bms_control_flow_version_,
                                  preserve_legacy_empty_sp_scratch(config_.mode.key_mode,
                                      replay_playback_enabled_ ? replay_source_.trace.lane_count : 0,
                                      peer_battle_mode_));
        for (const auto& message : out_result.messages) {
            std::cerr << "[warn] " << message << std::endl;
        }
        return out_result.success();
    };

    // Discover assets and the chart's preferred rate before claiming a device.
    // Opening at the profile rate first held an idle (possibly exclusive)
    // stream throughout parsing, then reopened it for many 48 kHz charts.
    const uint32_t initial_requested_rate = config_.audio.sample_rate;
    sample_rate_ = static_cast<int>(initial_requested_rate);
    report_loading_progress(56, "Parsing chart");
    if (loading_cancel_requested()) {
        return false;
    }
    ChartLoadResult chart_result;
    if (!load_chart_for_session_rate(chart_result)) {
        return false;
    }
    log_memory_phase("GameSession",
                     "chart-parse",
                     query_process_memory_snapshot(),
                     "asset_count=" + std::to_string(chart_result.chart.audio_assets.size()) +
                         " note_count=" + std::to_string(chart_result.chart.notes.size()));

    report_loading_progress(68, "Matching chart sample rate");
    if (loading_cancel_requested()) {
        return false;
    }
    std::string preferred_rate_diagnostic;
    const auto preferred_rate = detect_chart_preferred_sample_rate(chart_result.chart,
                                                                   &preferred_rate_diagnostic);
    // An ASIO device stays at the user's chosen rate; chart audio is resampled
    // by the existing decoder instead of reconfiguring the driver for each song.
    uint32_t requested_rate = initial_requested_rate;
    if (config_.audio.backend != audio::AudioBackend::ASIO && preferred_rate.has_value()) {
        requested_rate = static_cast<uint32_t>(*preferred_rate);
    } else if (!preferred_rate.has_value() && !preferred_rate_diagnostic.empty()) {
        std::cerr << "[warn] " << preferred_rate_diagnostic << std::endl;
    }
    if (requested_rate != static_cast<uint32_t>(sample_rate_)) {
        sample_rate_ = static_cast<int>(requested_rate);
        // Rebuild with the original sample-rounding rules, still without an
        // open device. Scaling already-rounded timestamps would alter replays.
        if (!load_chart_for_session_rate(chart_result)) {
            return false;
        }
    }
    const int chart_sample_rate = sample_rate_;
    report_loading_progress(72, "Opening audio device");
    if (loading_cancel_requested()) {
        return false;
    }
    if (!initialize_audio_session(requested_rate)) {
        if (requested_rate == initial_requested_rate) {
            return false;
        }
        std::cerr << "[warn] Could not open chart-rate audio at " << requested_rate
                  << " Hz. Falling back to the configured " << initial_requested_rate
                  << " Hz." << std::endl;
        requested_rate = initial_requested_rate;
        if (!initialize_audio_session(requested_rate)) {
            return false;
        }
    }
    std::cerr << "[info] Gameplay audio opened requested=" << requested_rate
              << " actual=" << sample_rate_
              << " mode=" << (config_.audio.backend == audio::AudioBackend::ASIO ? "asio" :
                              audio_thread_.is_exclusive() ? "exclusive" : "shared")
              << " device_mix=" << audio_thread_.device_mix_sample_rate() << std::endl;
    // Driver negotiation/fallback can choose a different rate. Always build
    // the final chart in the actual device clock's sample domain.
    if (sample_rate_ != chart_sample_rate && !load_chart_for_session_rate(chart_result)) {
        return false;
    }
    if (loading_cancel_requested()) {
        return false;
    }

    // Equivalent timelines keep the historical policy identity, preserving
    // existing boards/ghosts. Only corrected charts export the new policy.
    if (!replay_playback_enabled_ && !chart_result.has_bpm_stop_timing_difference)
        bms_timing_version_ = 1;
    chart_format_ = chart_result.format;
    if (!replay_playback_enabled_ && !chart_result.chart.bms_has_random)
        bms_control_flow_version_ = 1;
    chart_base_bpm_ = chart_result.reference_bpm > 0.0 ? chart_result.reference_bpm : chart_result.base_bpm;
    report_loading_progress(78, "Applying gameplay mode");
    if (loading_cancel_requested()) {
        return false;
    }

    const auto requested_nk2_preset = gameplay::parse_nk2_preset(config_.mode.key_conversion_nk2_preset);
    if (config_.mode.key_conversion_algorithm != "nk2" &&
        requested_nk2_preset && gameplay::is_source_ln_preset(*requested_nk2_preset)) {
        // E/E+ are NK2-only; do not split unchanged Krrcream/NK3 records or ghosts.
        config_.mode.key_conversion_nk2_preset = "native";
    }
    key_conversion_version_ = replay_playback_enabled_ ? replay_source_.mode.key_conversion_version : 3;
    long_note_mix_version_ = replay_playback_enabled_ ? replay_source_.mode.long_note_mix_version : 3;
    ModeManagerResult mode_result =
        manage_modes(chart_result.chart,
                     chart_result.format,
                     config_.mode,
                     config_.judge,
                     config_.speed.rate,
                     chart_result.base_bpm,
                     sample_rate_,
                     static_cast<LongNoteMixVersion>(long_note_mix_version_), key_conversion_version_);
    if (!replay_playback_enabled_ && config_.mode.key_conversion_algorithm == "nk2" &&
        requested_nk2_preset == gameplay::Nk2Preset::FullFieldTapsEPlusV2) {
        // Keep compatible records on v1 and record the actual preset used.
        config_.mode.key_conversion_nk2_preset =
            gameplay::to_string(mode_result.settings.key_conversion_nk2_preset);
    }
    if (replay_playback_enabled_) {
        const auto base_judge = is_supported_canonical_replay_ruleset(replay_source_.ruleset_id)
                                    ? config::JudgeConfig{} : config_.judge;
        mode_result.judge = replay_judge_config_for_playback(replay_source_, base_judge, chart_result.chart.bms_rank);
    }
    for (const auto& warning : mode_result.warnings) {
        std::cerr << "[warn] " << warning << std::endl;
    }

    gameplay::GameplayConfig gameplay_config;
    gameplay_config.sample_rate = sample_rate_;
    gameplay_config.rate = config_.speed.rate;
    gameplay_config.judge = mode_result.judge;
    gameplay_config.strict_standard_hold_release = !replay_playback_enabled_ ||
        replay_uses_strict_standard_hold_release(replay_source_);
    gameplay_config.great_standard_hold_release = !replay_playback_enabled_ ||
        replay_uses_great_standard_hold_release(replay_source_);
    gameplay_config.legacy_hold_release = replay_playback_enabled_ &&
        replay_uses_legacy_hold_release(replay_source_);
    gameplay_config.chronological_judgement = !replay_playback_enabled_ ||
        replay_uses_chronological_judgement(replay_source_);
    gameplay_config.empty_poor_affects_accuracy = replay_playback_enabled_ &&
        replay_empty_poor_affects_accuracy(replay_source_);
    chronological_judgement_ = gameplay_config.chronological_judgement;
    gameplay_config.gauge = config_.gauge;
    gameplay_config.input_offset_ms = config_.input_offset_ms;
    gameplay_config.practice_no_fail_enabled =
        practice_no_fail_enabled_;
    gameplay_config.one_miss_fail_enabled = one_miss_fail_enabled_;
    // Gauge Shift is the standard runtime. The selected gauge is its starting
    // tier; course gauges keep their dedicated carry-over policy below.
    gauge_shift_enabled_ = true;
    switch (mode_result.settings.gauge) {
        case gameplay::GaugeMode::ExHard:
            gameplay_config.initial_gauge = game::GaugeType::ExHard;
            break;
        case gameplay::GaugeMode::Shift:
            gameplay_config.initial_gauge = game::GaugeType::ExHard;
            break;
        case gameplay::GaugeMode::Hard:
            gameplay_config.initial_gauge = game::GaugeType::Hard;
            break;
        case gameplay::GaugeMode::Easy:
            gameplay_config.initial_gauge = game::GaugeType::Easy;
            break;
        case gameplay::GaugeMode::Normal:
        default:
            gameplay_config.initial_gauge = game::GaugeType::Normal;
            break;
    }
    if (options.has_gauge) {
        if (is_gauge_shift_token(options.gauge)) {
            gauge_shift_enabled_ = true;
            gameplay_config.initial_gauge = game::GaugeType::ExHard;
        } else if (auto gauge = parse_gauge_type(options.gauge)) {
            gauge_shift_enabled_ = true;
            gameplay_config.initial_gauge = gauge.value();
        }
    }
    if (peer_battle_mode_) {
        // Multiplayer uses the same four-way parallel Gauge Shift rules as
        // single-player, regardless of the local profile or launch options.
        gauge_shift_enabled_ = true;
        gameplay_config.initial_gauge = game::GaugeType::ExHard;
    }
    if (course_gauge_enabled_ && !peer_battle_mode_ && !replay_playback_enabled_) {
        gauge_shift_enabled_ = false;
        gameplay_config.initial_gauge = game::GaugeType::Normal;
        gameplay_config.initial_gauge_value = std::clamp(course_gauge_initial_value_, 0.0, 100.0);
        gameplay_config.gauge_policy.course_lr2_deltas = true;
        gameplay_config.judge.indirect_miss_enabled = true;
    }
    gameplay_config.gauge_shift_enabled = gauge_shift_enabled_;
    if (!course_gauge_enabled_ || peer_battle_mode_ || replay_playback_enabled_) {
        gameplay_config.gauge_policy = {};
    }
    if (replay_playback_enabled_ && replay_source_.mode.course_gauge == gameplay::kLr2CourseGaugeId) {
        // Optional evidence restores new course runs without reinterpreting
        // older replays that never stored their course policy or carried HP.
        gauge_shift_enabled_ = false;
        gameplay_config.gauge_shift_enabled = false;
        gameplay_config.initial_gauge = game::GaugeType::Normal;
        gameplay_config.initial_gauge_value = replay_source_.mode.course_gauge_initial_value;
        gameplay_config.gauge_policy.course_lr2_deltas = true;
        gameplay_config.judge.indirect_miss_enabled = true;
    }

    bms_gauge_version_ = replay_playback_enabled_ ? replay_source_.mode.bms_gauge_version :
        (chart_format_ == ChartFormat::Bms && !peer_battle_mode_ && !gameplay_config.gauge_policy.course_lr2_deltas ? 2 : 1);
    gameplay_config.gauge_policy.bms_lr2_deltas = chart_format_ == ChartFormat::Bms && bms_gauge_version_ == 2;
    gameplay_config.gauge_policy.bms_total = chart_result.chart.bms_total.value_or(0.0);

    active_mods_ = mode_result.active_mods;
    auto_scratch_enabled_ = chart_format_ == ChartFormat::Bms && mode_mod_auto_scratch(active_mods_);
    rate_multiplier_ = mode_result.rate_multiplier;
    score_multiplier_ = mode_result.final_multiplier;

    chart_ = mode_result.chart;
    next_visual_cue_index_ = 0;
    last_visual_cue_sample_ = -1;
    current_background_base_path_.clear();
    current_background_overlay_path_.clear();
    current_background_base_start_sample_ = 0;
    current_background_overlay_start_sample_ = 0;
    if (replay_playback_enabled_ &&
        replay_source_.trace.lane_count > 0 &&
        chart_.lane_count != replay_source_.trace.lane_count) {
        std::cerr << "[warn] Replay lane-count mismatch after mode application. expected="
                  << replay_source_.trace.lane_count << " actual=" << chart_.lane_count << std::endl;
        return false;
    }
    if (ghost_replay_enabled_) {
        bool ghost_compatible = true;
        score_comparison_compatible_ = !score_comparison_replay_path_.empty() &&
            score_comparison_replay_path_ == options.ghost_replay_path && live_score_replay_policy_matches(
            ghost_replay_source_.ruleset_id,
            replay_ruleset_id_for_runtime(config_.judge, config_.gauge, gauge_shift_enabled_,
                                         course_gauge_enabled_, "off", active_mods_),
            ghost_replay_source_.mode.key_conversion_version, key_conversion_version_,
            ghost_replay_source_.mode.bms_timing_version, bms_timing_version_,
            ghost_replay_source_.mode.bms_gauge_version, bms_gauge_version_,
            ghost_replay_source_.mode.bms_control_flow_version, bms_control_flow_version_,
            ghost_replay_source_.mode.autoplay_enabled || ghost_replay_source_.mode.practice_no_fail_enabled,
            ghost_replay_source_.aborted);
        if (config_.skin.score_comparison_enabled && score_comparison_compatible_) {
            // A file edited in place after indexing cannot reuse the old chart's
            // score trace, even when its path, lanes and mode still match.
            const auto current_hashes = hash_chart_file_utf8(chart_path_);
            score_comparison_compatible_ = current_hashes.valid() &&
                !ghost_replay_source_.chart_sha256.empty() &&
                current_hashes.sha256 == ghost_replay_source_.chart_sha256;
        }
        if (score_comparison_only_ && !score_comparison_compatible_)
            ghost_compatible = false;
        if (ghost_replay_source_.trace.lane_count > 0 &&
            chart_.lane_count != ghost_replay_source_.trace.lane_count) {
            ghost_compatible = false;
        }
        if (ghost_compatible && ghost_replay_source_.rate > 0.0 &&
            std::abs(ghost_replay_source_.rate - config_.speed.rate) > 0.001) {
            ghost_compatible = false;
        }
        if (ghost_compatible && !ghost_replay_source_.mode.key_mode.empty() &&
            normalize_runtime_key_mode_local(ghost_replay_source_.mode.key_mode) !=
                normalize_runtime_key_mode_local(config_.mode.key_mode)) {
            ghost_compatible = false;
        }
        if (ghost_compatible && !ghost_replay_source_.mode.key_conversion_algorithm.empty() &&
            ghost_replay_source_.mode.key_conversion_algorithm !=
                config_.mode.key_conversion_algorithm) {
            ghost_compatible = false;
        }
        if (ghost_compatible &&
            (config_.mode.key_conversion_algorithm == "nk2" ||
             config_.mode.key_conversion_algorithm == "nk3") &&
            normalize_nk2_preset_local(ghost_replay_source_.mode.key_conversion_nk2_preset) !=
                normalize_nk2_preset_local(config_.mode.key_conversion_nk2_preset)) {
            ghost_compatible = false;
        }
        if (ghost_compatible &&
            !ghost_replay_source_.mode.key_conversion_note_add_mode.empty() &&
            ghost_replay_source_.mode.key_conversion_note_add_mode != "default") {
            ghost_compatible = false;
        }
        if (ghost_compatible && !ghost_replay_source_.mode.random.empty() &&
            ghost_replay_source_.mode.random != config_.mode.random) {
            ghost_compatible = false;
        }
        // Original and NK2 E/E+ consume no mode seed. A new per-play seed must
        // not disable their OFF/Mirror ghosts. Seed-dependent conversion/mods
        // still require matching seeds because both fields share one chart.
        int source_key_lanes = chart_result.chart.lane_count;
        for (int lane = 1; lane <= chart_result.chart.lane_count; ++lane)
            if (std::find(chart_result.chart.scratch_lanes.begin(), chart_result.chart.scratch_lanes.end(), lane) !=
                chart_result.chart.scratch_lanes.end()) --source_key_lanes;
        const bool seedless_conversion = mode_result.settings.key_mode == gameplay::KeyMode::Auto ||
            (mode_result.settings.key_conversion_algorithm == gameplay::KeyModeConversionAlgorithm::NK2 &&
             gameplay::is_source_ln_preset(mode_result.settings.key_conversion_nk2_preset)) ||
            (mode_result.settings.key_conversion_algorithm != gameplay::KeyModeConversionAlgorithm::NK3 &&
             gameplay::to_string(mode_result.settings.key_mode) == std::to_string(source_key_lanes) + "K");
        const bool seed_independent_ghost = seedless_conversion &&
            (mode_result.settings.random == gameplay::RandomMode::Off ||
             mode_result.settings.random == gameplay::RandomMode::Mirror) &&
            !mode_mod_adds_notes(active_mods_) && !mode_mod_long_note_mix(active_mods_);
        if (ghost_compatible && !seed_independent_ghost && ghost_replay_source_.mode.random_seed.has_value() &&
            ghost_replay_source_.mode.random_seed.value() != static_cast<int>(config_.mode.random_seed)) {
            ghost_compatible = false;
        }
        if (ghost_compatible &&
            !equivalent_mode_mod_tokens(ghost_replay_source_.mods, active_mods_)) {
            ghost_compatible = false;
        }
        if (ghost_compatible && mode_mod_long_note_mix(active_mods_) &&
            ghost_replay_source_.mode.long_note_mix_version != long_note_mix_version_) {
            // Both fields share one chart. Historical replays remain playable
            // alone, but a ghost must not undo the new run's mine avoidance.
            ghost_compatible = false;
            std::cerr << "[warn] Ghost disabled: LN Mix conversion differs from this play." << std::endl;
        }
        if (ghost_compatible && chart_result.has_bpm_stop_timing_difference &&
            ghost_replay_source_.mode.bms_timing_version != bms_timing_version_) {
            // Shared fields cannot mix two different note/mine schedules.
            // Keep old replays playable on their own; never revert a new run.
            ghost_compatible = false;
            std::cerr << "[warn] Legacy ghost disabled: this chart has corrected BPM/STOP timing." << std::endl;
        }
        if (ghost_compatible && chart_result.chart.bms_has_random &&
            (ghost_replay_source_.mode.bms_control_flow_version != bms_control_flow_version_ ||
             gameplay::replay_bms_random_seed(ghost_replay_source_.mode) != bms_random_seed_)) {
            ghost_compatible = false;
            std::cerr << "[warn] Ghost disabled: BMS branch seed/policy differs from this play." << std::endl;
        }
        if (ghost_compatible && key_conversion_version_ == 3 && ghost_replay_source_.mode.key_conversion_version < 3) {
            // A legacy ghost must not undo the new player's duplicate-head fix.
            // Both fields currently share one chart, so exclude only affected charts.
            auto corrected_source = chart_result.chart;
            if (gameplay::coalesce_bms_long_note_heads(corrected_source) > 0) {
                ghost_compatible = false;
                std::cerr << "[warn] Legacy ghost disabled: this BMS needs duplicate long-note head correction." << std::endl;
            }
        }
        if (!ghost_compatible) {
            score_comparison_compatible_ = false;
            std::cerr << "[warn] Ghost replay metadata does not match the current gameplay settings. "
                      << "Ghost comparison will be disabled for this run." << std::endl;
            ghost_replay_enabled_ = false;
            ghost_replay_source_ = {};
            ghost_replay_event_index_ = 0;
        } else if (ghost_replay_source_.mode.key_conversion_version != key_conversion_version_ ||
                   (mode_mod_long_note_mix(active_mods_) &&
                    ghost_replay_source_.mode.long_note_mix_version != long_note_mix_version_)) {
            // Both fields share one chart. Adopt the validated ghost's original
            // conversion only for this run; rejected ghosts cannot change a new
            // session. Export this same version so the new score stays replayable.
            key_conversion_version_ = ghost_replay_source_.mode.key_conversion_version;
            long_note_mix_version_ = ghost_replay_source_.mode.long_note_mix_version;
            chart_ = manage_modes(chart_result.chart, chart_result.format, config_.mode,
                                  config_.judge, config_.speed.rate, chart_result.base_bpm,
                                  sample_rate_, static_cast<LongNoteMixVersion>(long_note_mix_version_), key_conversion_version_).chart;
            std::cerr << "[info] Matching the ghost's recorded key-conversion and LN Mix versions." << std::endl;
        }
    }
    // The immediate shortcut means the beginning of the song even when this
    // attempt came from an editor practice marker. Ordinary Retry keeps it.
    if (practice_start_seconds_ > 0.0 && !instant_restart_start_) {
        const double rate = std::max(0.0001, rate_multiplier_);
        const auto start_sample = static_cast<int64_t>(std::llround(
            practice_start_seconds_ * static_cast<double>(sample_rate_) / rate));
        gameplay::trim_gameplay_chart_before_sample(chart_, start_sample);
        std::cerr << "[info] Practice playback starts at " << practice_start_seconds_
                  << "s (sample " << start_sample << ")." << std::endl;
    }
    offset_gameplay_chart_samples(chart_, ms_to_samples(static_cast<double>(kGameplayStartLeadInMs), sample_rate_));
    for (std::size_t i = 0; i < chart_.notes.size(); ++i) {
        chart_.notes[i].note_id = i;
    }
    hidden_hit_note_ids_.assign(chart_.notes.size(), 0);
    ghost_hidden_hit_note_ids_.assign(chart_.notes.size(), 0);
    active_holds_buffer_.clear();
    ghost_active_holds_buffer_.clear();
    build_autoplay_events();
    key_to_lane_.clear();
    config::KeymapManager keymap_manager_runtime;
    const std::string active_key_mode =
        keymap_manager_runtime.normalize_mode_token(std::to_string(std::max(1, chart_.lane_count)) + "k");
    for (const auto& [lane, key] : keymap_manager_runtime.bindings_for_mode(keymap_, active_key_mode)) {
        auto lane_index = parse_lane_index(lane);
        if (!lane_index.has_value() || *lane_index > chart_.lane_count) {
            continue;
        }
        auto keycode = config::KeycodeMap::to_keycode(key);
        if (!keycode.has_value()) {
            continue;
        }
        key_to_lane_[keycode.value()] = lane_index.value();
    }
    for (const auto& [lane, key] : keymap_manager_runtime.secondary_bindings_for_mode(keymap_, active_key_mode)) {
        const auto lane_index = parse_lane_index(lane);
        const auto keycode = config::KeycodeMap::to_keycode(key);
        if (lane_index && *lane_index <= chart_.lane_count && keycode) {
            // Preserve an existing primary assignment if a secondary conflicts.
            key_to_lane_.emplace(*keycode, *lane_index);
        }
    }
    lane_binding_state_.configure(key_to_lane_);
    std::unordered_set<int> bound_lanes;
    for (const auto& [key, lane] : key_to_lane_) bound_lanes.insert(lane);
    std::cerr << "[info] Gameplay input configured="
              << input_backend_name(input_backend_state_.configured_backend)
              << " polling_shadow=" << (config_.input.rawinput ? "bound-keys" : "primary")
              << " key_mode=" << active_key_mode
              << " bound_lanes=" << bound_lanes.size() << "/" << std::max(1, chart_.lane_count)
              << " bound_keys=" << key_to_lane_.size()
              << " keymap_normalized=" << keymap_result.normalized_binding_count
              << " keymap_repaired=" << keymap_result.repaired_binding_count
              << std::endl;
    if (bound_lanes.size() < static_cast<std::size_t>(std::max(1, chart_.lane_count))) {
        std::cerr << "[warn] Gameplay lane binding coverage incomplete: mapped=" << bound_lanes.size()
                  << " expected=" << std::max(1, chart_.lane_count)
                  << ". Invalid bindings or duplicate lane keys may prevent judgement/input interaction."
                  << std::endl;
    }
    instant_restart_keycode_ = resolve_instant_restart_key(config_.ui.instant_restart_key, key_to_lane_);
    const auto resolved_functions = resolve_function_keys(config_.ui, key_to_lane_);
    lane_conflicting_overlay_keys_.fill(0);
    for (int number = 10; number <= 11; ++number) {
        const auto key = config::KeycodeMap::to_keycode(config::function_key(config_.ui, number)).value_or(0);
        if (key_to_lane_.count(key)) lane_conflicting_overlay_keys_[number - 10] = key;
    }
    f1_keycode_ = resolved_functions[0];
    f2_keycode_ = resolved_functions[1];
    f3_keycode_ = resolved_functions[2];
    f4_keycode_ = resolved_functions[3];
    f5_keycode_ = resolved_functions[4];
    f6_keycode_ = resolved_functions[5];
    f7_keycode_ = resolved_functions[6];
    f8_keycode_ = resolved_functions[7];
    f9_keycode_ = resolved_functions[8];
    f10_keycode_ = resolved_functions[9];
    f11_keycode_ = resolved_functions[10];
    if (!config_.ui.instant_restart_key.empty() && instant_restart_keycode_ == 0) {
        std::cerr << "[warn] Instant Restart Key is invalid, reserved or bound to a lane; disabled for this chart." << std::endl;
    }
    rebuild_polled_gameplay_keys();
    report_loading_progress(88, "Preparing chart audio");
    if (loading_cancel_requested()) {
        return false;
    }
    if (!prepare_chart_audio()) {
        return false;
    }
    report_loading_progress(92, "Initializing input");
    if (loading_cancel_requested()) {
        return false;
    }

    input::InputThreadConfig input_config;
    rebuild_input_thread_config(input_config);
    std::cerr << "[info] Gameplay input startup requested="
              << input_backend_name(input_config.backend)
              << " gate=foreground-process polling_keys=" << input_config.polling_keys.size()
              << " polling_shadow="
              << (input_config.backend == input::InputBackend::Polling
                      ? "primary"
                      : (input_config.rawinput_polling_shadow ? "bound-keys" : "off"))
              << std::endl;
    if (!input_thread_.initialize(input_config)) {
        if (input_config.backend != input::InputBackend::RawInput) {
            return false;
        }
        std::cerr << "[warn] Gameplay RawInput initialize failed; falling back to Polling so input stays playable."
                  << std::endl;
        input_thread_.shutdown();
        input_config.backend = input::InputBackend::Polling;
        input_config.raw_input.register_keyboard = false;
        input_backend_state_.auto_fallback = true;
        input_backend_state_.fallback_origin = InputFallbackOrigin::Gameplay;
        input_backend_state_.fallback_reason = "RawInput initialize failed; Polling fallback kept gameplay input active.";
        input_backend_state_.fallback_timestamp_utc = utc_timestamp_compact();
        input_backend_state_.effective_backend = input::InputBackend::Polling;
        if (!input_thread_.initialize(input_config)) {
            return false;
        }
    }
    next_guide_note_index_ = 0;
    hud_scan_start_ = 0;
    ghost_hud_scan_start_ = 0;
    hud_past_hold_indices_.clear();
    ghost_hud_past_hold_indices_.clear();
    hud_scan_cutoff_sample_ = (std::numeric_limits<int64_t>::min)();
    tone_voices_.reserve(std::max<std::size_t>(64, chart_.notes.size() / 8));
    chart_audio_voices_.reserve(std::max<std::size_t>(128, chart_.notes.size() / 16));
    lane_activity_.assign(static_cast<std::size_t>(std::max(1, chart_.lane_count)), 0.0f);
    ghost_lane_activity_.assign(static_cast<std::size_t>(std::max(1, chart_.lane_count)), 0.0f);
    key_overlay_.configure(config_.skin.key_overlay_enabled, chart_.lane_count);
    if (!key_overlay_.start()) std::cerr << "[warn] Optional key overlay worker could not start." << std::endl;
    lane_pressed_.assign(static_cast<std::size_t>(std::max(1, chart_.lane_count)), 0);
    ghost_lane_pressed_.assign(static_cast<std::size_t>(std::max(1, chart_.lane_count)), 0);

    engine_ = std::make_unique<gameplay::GameplayEngine>(chart_, gameplay_config);
    visual_scroll_segments_ =
        std::make_shared<const std::vector<gameplay::ScrollSegment>>(chart_.scroll_segments);
    prepare_scheduled_input_audio(gameplay_config);
    prepare_riff_map();
    if (ghost_replay_enabled_) {
        gameplay::GameplayConfig ghost_config = gameplay_config;
        const auto ghost_base_judge = is_supported_canonical_replay_ruleset(ghost_replay_source_.ruleset_id)
                                          ? config::JudgeConfig{} : config_.judge;
        ghost_config.judge = replay_judge_config_for_playback(ghost_replay_source_, ghost_base_judge, chart_.bms_rank);
        ghost_config.strict_standard_hold_release = replay_uses_strict_standard_hold_release(ghost_replay_source_);
        ghost_config.great_standard_hold_release = replay_uses_great_standard_hold_release(ghost_replay_source_);
        ghost_config.legacy_hold_release = replay_uses_legacy_hold_release(ghost_replay_source_);
        ghost_config.chronological_judgement = replay_uses_chronological_judgement(ghost_replay_source_);
        ghost_config.empty_poor_affects_accuracy = replay_empty_poor_affects_accuracy(ghost_replay_source_);
        ghost_chronological_judgement_ = ghost_config.chronological_judgement;
        ghost_config.gauge_shift_enabled = true;
        ghost_config.gauge_policy = {};
        ghost_config.gauge_policy.bms_lr2_deltas = chart_format_ == ChartFormat::Bms && ghost_replay_source_.mode.bms_gauge_version == 2;
        ghost_config.gauge_policy.bms_total = chart_.bms_total.value_or(0.0);
        if (!ghost_replay_source_.mode.gauge.empty()) {
            const bool ghost_gauge_shift = is_gauge_shift_token(ghost_replay_source_.mode.gauge);
            if (ghost_gauge_shift) {
                ghost_config.initial_gauge = game::GaugeType::ExHard;
            } else if (auto gauge = parse_gauge_type(ghost_replay_source_.mode.gauge)) {
                ghost_config.initial_gauge = gauge.value();
            }
        }
        if (ghost_replay_source_.mode.course_gauge == gameplay::kLr2CourseGaugeId) {
            ghost_config.gauge_shift_enabled = false;
            ghost_config.initial_gauge = game::GaugeType::Normal;
            ghost_config.initial_gauge_value = ghost_replay_source_.mode.course_gauge_initial_value;
            ghost_config.gauge_policy.course_lr2_deltas = true;
            ghost_config.judge.indirect_miss_enabled = true;
        }
        ghost_engine_ = std::make_unique<gameplay::GameplayEngine>(chart_, ghost_config);
    }
    if (config_.skin.score_comparison_enabled && !score_comparison_replay_path_.empty() &&
        (!ghost_engine_ || score_comparison_replay_path_ != options.ghost_replay_path)) {
        // A visible clear-first ghost and the score high-water replay can be
        // different performances. This third engine only reads the latter;
        // it never supplies input, sound, gauge, or conversion to either field.
        int source_key_lanes = chart_result.chart.lane_count;
        for (const int lane : chart_result.chart.scratch_lanes)
            if (lane >= 1 && lane <= chart_result.chart.lane_count) --source_key_lanes;
        const bool seedless_conversion = mode_result.settings.key_mode == gameplay::KeyMode::Auto ||
            (mode_result.settings.key_conversion_algorithm == gameplay::KeyModeConversionAlgorithm::NK2 &&
             gameplay::is_source_ln_preset(mode_result.settings.key_conversion_nk2_preset)) ||
            (mode_result.settings.key_conversion_algorithm != gameplay::KeyModeConversionAlgorithm::NK3 &&
             gameplay::to_string(mode_result.settings.key_mode) == std::to_string(source_key_lanes) + "K");
        const bool seed_independent = seedless_conversion &&
            (mode_result.settings.random == gameplay::RandomMode::Off || mode_result.settings.random == gameplay::RandomMode::Mirror) &&
            !mode_mod_adds_notes(active_mods_) && !mode_mod_long_note_mix(active_mods_);
        initialize_score_reference(gameplay_config, seed_independent);
    }
    report_loading_progress(96, "Starting gameplay");
    if (loading_cancel_requested()) {
        return false;
    }

    if (!input_thread_.start()) {
        if (input_config.backend != input::InputBackend::RawInput) {
            return false;
        }
        std::cerr << "[warn] Gameplay RawInput start failed; falling back to Polling so input stays playable."
                  << std::endl;
        input_thread_.shutdown();
        input_config.backend = input::InputBackend::Polling;
        input_config.raw_input.register_keyboard = false;
        input_backend_state_.auto_fallback = true;
        input_backend_state_.fallback_origin = InputFallbackOrigin::Gameplay;
        input_backend_state_.fallback_reason = "RawInput start failed; Polling fallback kept gameplay input active.";
        input_backend_state_.fallback_timestamp_utc = utc_timestamp_compact();
        input_backend_state_.effective_backend = input::InputBackend::Polling;
        if (!input_thread_.initialize(input_config)) {
            return false;
        }
        if (!input_thread_.start()) {
            return false;
        }
    }
    input_backend_state_.effective_backend = input_thread_.current_backend();
    std::cerr << "[info] Gameplay input active configured="
              << input_backend_name(input_backend_state_.configured_backend)
              << " effective=" << input_backend_name(input_backend_state_.effective_backend)
              << " gate=foreground-process polling_keys=" << input_config.polling_keys.size()
              << " polling_shadow="
              << (input_config.backend == input::InputBackend::Polling
                      ? "primary"
                      : (input_config.rawinput_polling_shadow ? "bound-keys" : "off"));
    if (input_backend_state_.auto_fallback && !input_backend_state_.fallback_reason.empty()) {
        std::cerr << " fallback_reason=\"" << input_backend_state_.fallback_reason << "\"";
    }
    std::cerr << std::endl;

    // Loading and transition run concurrently. Fast charts wait here, before
    // countdown/audio start; slow charts incur no additional delay. Cancellation
    // remains available throughout this short presentation barrier.
    while (timing::HighResClock::now_ns() < loading_transition_deadline_ns_) {
        if (loading_cancel_requested()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    countdown_value_ = gameplay_start_countdown_seconds(config_.ui.skip_start_countdown,
                                                       peer_battle_mode_, instant_restart_start_);
    countdown_active_ = countdown_value_ > 0;
    report_loading_progress(100, "Ready");
    return true;
}

