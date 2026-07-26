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
    input_config.gate_policy = input::InputGatePolicy::AlwaysAllow;
    // Gameplay always keeps a bound-key polling shadow inside InputThread.
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
        append_unique_key(f3_keycode_);
        append_unique_key(f4_keycode_);
        append_unique_key(f5_keycode_);
        append_unique_key(f6_keycode_);
        append_unique_key(f9_keycode_);
    }
}

void GameSession::note_runtime_input_event_source(const input::InputEvent& event) {
    sync_runtime_input_backend_state(input_backend_state_, event, InputFallbackOrigin::Gameplay);
}

void GameSession::rebuild_polled_gameplay_keys() {
    polled_gameplay_keys_.clear();
    polled_gameplay_keys_.reserve(key_to_lane_.size() + 6);

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
    append_unique_key(f3_keycode_);
    append_unique_key(f4_keycode_);
    append_unique_key(f5_keycode_);
    append_unique_key(f6_keycode_);
    append_unique_key(f9_keycode_);
}

GameSession::GameSession() = default;

GameSession::~GameSession() {
    shutdown();
}

bool GameSession::initialize(const CommandLineOptions& options) {
    options_ = options;
    result_ = {};
    stop_requested_.store(false, std::memory_order_release);
    finished_.store(false, std::memory_order_release);
    spectating_peer_.store(false, std::memory_order_release);
    user_aborted_.store(false, std::memory_order_release);
    last_audio_sample_.store(0, std::memory_order_release);
    audio_timing_sequence_.store(0, std::memory_order_release);
    last_audio_timing_ = {};
    startup_input_timing_anchor_ = {};
    clock_sync_.reset();
    countdown_active_ = false;
    countdown_value_ = 0;
    countdown_started_ns_ = 0;
    hispeed_decrease_held_ = false;
    hispeed_increase_held_ = false;
    hispeed_decrease_next_repeat_ns_ = 0;
    hispeed_increase_next_repeat_ns_ = 0;
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
    autoplay_events_.clear();
    autoplay_enabled_ = false;
    autoplay_event_index_ = 0;
    practice_no_fail_enabled_ = false;
    one_miss_fail_enabled_ = false;
    ghost_replay_source_ = {};
    ghost_replay_enabled_ = false;
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
    chart_audio_active_until_samples_.reset();
    pending_input_events_.clear();
    hidden_hit_note_ids_.clear();
    ghost_hidden_hit_note_ids_.clear();
    active_holds_buffer_.clear();
    ghost_active_holds_buffer_.clear();
    next_chart_audio_event_ = 0;
    startup_preload_budget_bytes_ = 0;
    runtime_chart_audio_budget_bytes_ = 0;
    chart_audio_decoded_bytes_ = 0;
    chart_audio_deferred_count_ = 0;
    chart_audio_eviction_count_ = 0;
    last_chart_audio_service_sample_ = (std::numeric_limits<int64_t>::min)();
    chart_audio_startup_logged_ = false;
    chart_audio_steady_state_logged_ = false;
    synthetic_tones_enabled_.store(true, std::memory_order_release);
    next_guide_note_index_ = 0;
    hud_scan_start_ = 0;
    ghost_hud_scan_start_ = 0;
    chart_ = {};
    lane_activity_.clear();
    ghost_lane_activity_.clear();
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
    const bool profile_rawinput = config_.input.rawinput;
    const bool migrated_config = config_result.migrated;
    const bool stripped_session_only_mods = strip_session_only_mode_mods(config_);

    if (config_result.used_defaults || migrated_config || stripped_session_only_mods) {
        const config::RuntimeConfig persisted = build_persisted_runtime_config(config_);
        config_loader.save_profile(profile_dir_, persisted);
    }
    if (peer_battle_mode_) {
        apply_peer_battle_rules(config_);
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

    if (options.has_rate) {
        config_.speed.rate = options.rate;
    }
    if (options.has_hispeed) {
        config_.speed.hi_speed = options.hispeed;
    }
    escape_keycode_ = config::KeycodeMap::to_keycode("Esc").value_or(0);
    f3_keycode_ = config::KeycodeMap::to_keycode("F3").value_or(0);
    f4_keycode_ = config::KeycodeMap::to_keycode("F4").value_or(0);
    f5_keycode_ = config::KeycodeMap::to_keycode("F5").value_or(0);
    f6_keycode_ = config::KeycodeMap::to_keycode("F6").value_or(0);
    f9_keycode_ = config::KeycodeMap::to_keycode("F9").value_or(0);

    if (!options.replay_path.empty()) {
        auto replay_load = gameplay::load_replay_json(options.replay_path);
        if (!replay_load.success()) {
            std::cerr << "[warn] Failed to load replay " << options.replay_path
                      << ": " << replay_load.error << std::endl;
            return false;
        }
        replay_source_ = std::move(replay_load.replay.value());
        replay_source_path_ = options.replay_path;
        replay_playback_enabled_ = true;
        replay_event_index_ = 0;
        for (const auto& warning : replay_load.warnings) {
            std::cerr << "[warn] " << warning << std::endl;
        }
        if (!options.has_rate && replay_source_.rate > 0.0) {
            config_.speed.rate = replay_source_.rate;
        }
        if (!replay_source_.mods.empty()) {
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
        practice_no_fail_enabled_ = replay_source_.mode.practice_no_fail_enabled;
        one_miss_fail_enabled_ = replay_source_.mode.one_miss_fail_enabled;
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
    report_loading_progress(42, "Opening audio device");
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
            return false;
        }
        sample_rate_ = static_cast<int>(audio_thread_.sample_rate());
        if (sample_rate_ <= 0) {
            sample_rate_ = static_cast<int>(requested_rate);
        }
        input_offset_samples_ = ms_to_samples(config_.input_offset_ms, sample_rate_);
        return true;
    };

    auto load_chart_for_session_rate = [this](ChartLoadResult& out_result) -> bool {
        ChartLoader loader;
        out_result = loader.load(chart_path_, sample_rate_, config_.speed.rate,
                                 config_.audio_ui.bms_keysound_policy,
                                 config_.mode.enable_osu_charts);
        for (const auto& message : out_result.messages) {
            std::cerr << "[warn] " << message << std::endl;
        }
        return out_result.success();
    };

    const uint32_t initial_requested_rate = config_.audio.sample_rate;
    if (!initialize_audio_session(initial_requested_rate)) {
        return false;
    }
    if (loading_cancel_requested()) {
        return false;
    }

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
    const auto preferred_rate = detect_chart_preferred_sample_rate(chart_result.format,
                                                                   chart_result.chart,
                                                                   &preferred_rate_diagnostic);
    if (preferred_rate.has_value() && *preferred_rate != sample_rate_) {
        const int previous_actual_rate = sample_rate_;
        std::cerr << "[info] Detected chart audio sample rate " << *preferred_rate
                  << " Hz. Reinitializing gameplay audio for this chart." << std::endl;
        if (!initialize_audio_session(static_cast<uint32_t>(*preferred_rate))) {
            std::cerr << "[warn] Failed to switch gameplay audio to " << *preferred_rate
                      << " Hz. Falling back to " << previous_actual_rate << " Hz." << std::endl;
            if (!initialize_audio_session(initial_requested_rate)) {
                return false;
            }
        } else {
            const uint32_t device_mix_rate = audio_thread_.device_mix_sample_rate();
            if (!audio_thread_.is_exclusive() && device_mix_rate > 0 &&
                device_mix_rate != static_cast<uint32_t>(sample_rate_)) {
                std::cerr << "[info] Gameplay audio stream running at " << sample_rate_
                          << " Hz (shared-mode device mix " << device_mix_rate << " Hz)." << std::endl;
            } else {
                std::cerr << "[info] Gameplay audio stream running at " << sample_rate_ << " Hz." << std::endl;
            }
            if (sample_rate_ != previous_actual_rate && !load_chart_for_session_rate(chart_result)) {
                return false;
            }
        }
    } else if (!preferred_rate.has_value() && !preferred_rate_diagnostic.empty()) {
        std::cerr << "[warn] " << preferred_rate_diagnostic << std::endl;
    }
    if (loading_cancel_requested()) {
        return false;
    }

    chart_format_ = chart_result.format;
    report_loading_progress(78, "Applying gameplay mode");
    if (loading_cancel_requested()) {
        return false;
    }

    const ModeManagerResult mode_result =
        manage_modes(chart_result.chart,
                     chart_result.format,
                     config_.mode,
                     config_.judge,
                     config_.speed.rate,
                     chart_result.base_bpm,
                     sample_rate_);
    for (const auto& warning : mode_result.warnings) {
        std::cerr << "[warn] " << warning << std::endl;
    }

    gameplay::GameplayConfig gameplay_config;
    gameplay_config.sample_rate = sample_rate_;
    gameplay_config.rate = config_.speed.rate;
    gameplay_config.judge = mode_result.judge;
    gameplay_config.gauge = config_.gauge;
    gameplay_config.gauge_policy.normal_to_easy_shift = peer_battle_mode_;
    gameplay_config.input_offset_ms = config_.input_offset_ms;
    gameplay_config.practice_no_fail_enabled = practice_no_fail_enabled_;
    gameplay_config.one_miss_fail_enabled = one_miss_fail_enabled_;
    switch (mode_result.settings.gauge) {
        case gameplay::GaugeMode::ExHard:
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
        auto gauge = parse_gauge_type(options.gauge);
        if (gauge.has_value()) {
            gameplay_config.initial_gauge = gauge.value();
        }
    }

    active_mods_ = mode_result.active_mods;
    rate_multiplier_ = mode_result.rate_multiplier;
    score_multiplier_ = mode_result.final_multiplier;

    chart_ = mode_result.chart;
    next_visual_cue_index_ = 0;
    last_visual_cue_sample_ = -1;
    current_background_base_path_.clear();
    current_background_overlay_path_.clear();
    if (replay_playback_enabled_ &&
        replay_source_.trace.lane_count > 0 &&
        chart_.lane_count != replay_source_.trace.lane_count) {
        std::cerr << "[warn] Replay lane-count mismatch after mode application. expected="
                  << replay_source_.trace.lane_count << " actual=" << chart_.lane_count << std::endl;
        return false;
    }
    if (ghost_replay_enabled_) {
        bool ghost_compatible = true;
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
        if (ghost_compatible && !ghost_replay_source_.mode.random.empty() &&
            ghost_replay_source_.mode.random != config_.mode.random) {
            ghost_compatible = false;
        }
        if (ghost_compatible && ghost_replay_source_.mode.random_seed.has_value() &&
            ghost_replay_source_.mode.random_seed.value() != static_cast<int>(config_.mode.random_seed)) {
            ghost_compatible = false;
        }
        if (ghost_compatible &&
            !equivalent_mode_mod_tokens(ghost_replay_source_.mods, active_mods_)) {
            ghost_compatible = false;
        }
        if (!ghost_compatible) {
            std::cerr << "[warn] Ghost replay metadata does not match the current gameplay settings. "
                      << "Ghost comparison will be disabled for this run." << std::endl;
            ghost_replay_enabled_ = false;
            ghost_replay_source_ = {};
            ghost_replay_event_index_ = 0;
        }
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
        if (!lane_index.has_value()) {
            continue;
        }
        auto keycode = config::KeycodeMap::to_keycode(key);
        if (!keycode.has_value()) {
            continue;
        }
        key_to_lane_[keycode.value()] = lane_index.value();
    }
    std::cerr << "[info] Gameplay input configured="
              << input_backend_name(input_backend_state_.configured_backend)
              << " polling_shadow=" << (config_.input.rawinput ? "bound-keys" : "primary")
              << " key_mode=" << active_key_mode
              << " bound_lanes=" << key_to_lane_.size() << "/" << std::max(1, chart_.lane_count)
              << " keymap_normalized=" << keymap_result.normalized_binding_count
              << " keymap_repaired=" << keymap_result.repaired_binding_count
              << std::endl;
    if (key_to_lane_.empty() || key_to_lane_.size() < static_cast<std::size_t>(std::max(1, chart_.lane_count))) {
        std::cerr << "[warn] Gameplay lane binding coverage incomplete: mapped=" << key_to_lane_.size()
                  << " expected=" << std::max(1, chart_.lane_count)
                  << ". Invalid bindings or duplicate lane keys may prevent judgement/input interaction."
                  << std::endl;
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
              << " gate=always polling_keys=" << input_config.polling_keys.size()
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
    tone_voices_.reserve(std::max<std::size_t>(64, chart_.notes.size() / 8));
    chart_audio_voices_.reserve(std::max<std::size_t>(128, chart_.notes.size() / 16));
    lane_activity_.assign(static_cast<std::size_t>(std::max(1, chart_.lane_count)), 0.0f);
    ghost_lane_activity_.assign(static_cast<std::size_t>(std::max(1, chart_.lane_count)), 0.0f);

    engine_ = std::make_unique<gameplay::GameplayEngine>(chart_, gameplay_config);
    if (ghost_replay_enabled_) {
        gameplay::GameplayConfig ghost_config = gameplay_config;
        if (!ghost_replay_source_.mode.gauge.empty()) {
            if (auto gauge = parse_gauge_type(ghost_replay_source_.mode.gauge)) {
                ghost_config.initial_gauge = gauge.value();
            }
        }
        ghost_engine_ = std::make_unique<gameplay::GameplayEngine>(chart_, ghost_config);
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
              << " gate=always polling_keys=" << input_config.polling_keys.size()
              << " polling_shadow="
              << (input_config.backend == input::InputBackend::Polling
                      ? "primary"
                      : (input_config.rawinput_polling_shadow ? "bound-keys" : "off"));
    if (input_backend_state_.auto_fallback && !input_backend_state_.fallback_reason.empty()) {
        std::cerr << " fallback_reason=\"" << input_backend_state_.fallback_reason << "\"";
    }
    std::cerr << std::endl;

    countdown_active_ = true;
    countdown_value_ = static_cast<int>(kGameplayStartCountdownSeconds);
    report_loading_progress(100, "Ready");
    return true;
}

void GameSession::run() {
    auto next_hud_tick = std::chrono::steady_clock::now();

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
            service_hispeed_repeat(timing::HighResClock::now_ns());

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

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    if (!stop_requested_.load(std::memory_order_acquire) && !finished_.load(std::memory_order_acquire)) {
        rebaseline_gameplay_start_input_state(0);
        current_playback_sample_ = 0;
        startup_input_timing_anchor_ = {};
        audio_timing_diagnostics_logged_ = false;
        if (audio_thread_.start() != audio::AudioResult::Success) {
            std::cerr << "[error] Failed to start gameplay audio." << std::endl;
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

        if (spectating_peer_.load(std::memory_order_acquire) &&
            peer_spectator_done_callback_ && peer_spectator_done_callback_()) {
            finished_.store(true, std::memory_order_release);
            break;
        }

        service_chart_audio_streaming(last_audio_sample_.load(std::memory_order_acquire));

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

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
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
    snapshot.sample_rate = sample_rate_;
    snapshot.hud_publish_time_ns = timing::HighResClock::now_ns();
    snapshot.countdown_active = countdown_active_;
    snapshot.countdown_value = countdown_value_;

    for (;;) {
        const uint64_t begin = audio_timing_sequence_.load(std::memory_order_acquire);
        if ((begin & 1u) != 0u) {
            continue;
        }

        const AudioTimingState timing = last_audio_timing_;
        const uint64_t end = audio_timing_sequence_.load(std::memory_order_acquire);
        if (begin != end) {
            continue;
        }

        snapshot.current_sample = timing.sample;
        snapshot.audio_sample_time_ns = timing.time_ns;
        snapshot.audio_buffer_frames = timing.buffer_frames;
        break;
    }

    {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        if (!engine_) {
            return snapshot;
        }

        snapshot.active = true;
        snapshot.finished = snapshot.finished || engine_->is_finished();
        snapshot.game_over = engine_->is_game_over();
        snapshot.rate = config_.speed.rate;
        snapshot.hispeed = config_.speed.hi_speed;
        snapshot.lane_count = std::max(1, engine_->lane_count());
        snapshot.duration_samples = engine_->duration_samples();

        if (snapshot.current_sample < last_visual_cue_sample_) {
            next_visual_cue_index_ = 0;
            current_background_base_path_.clear();
            current_background_overlay_path_.clear();
        }
        while (next_visual_cue_index_ < chart_.visual_cues.size() &&
               chart_.visual_cues[next_visual_cue_index_].start_sample <= snapshot.current_sample) {
            const auto& cue = chart_.visual_cues[next_visual_cue_index_++];
            if (const std::string* path = chart_.visual_asset_path(cue.asset_id)) {
                if (cue.layer == gameplay::VisualLayer::Overlay) {
                    current_background_overlay_path_ = *path;
                } else {
                    current_background_base_path_ = *path;
                }
            }
        }
        last_visual_cue_sample_ = snapshot.current_sample;
        snapshot.background_base_path = current_background_base_path_;
        snapshot.background_overlay_path = current_background_overlay_path_;

        const auto& stats = engine_->stats();
        snapshot.combo = stats.combo;
        snapshot.max_combo = stats.max_combo;
        snapshot.counts = stats.counts;
        snapshot.score = std::max<int64_t>(
            0,
            static_cast<int64_t>(std::llround(static_cast<double>(stats.raw_score) * score_multiplier_)));
        snapshot.osu_od8_score_available = stats.osu_od8.available;
        snapshot.osu_od8_score = stats.osu_od8.score;

        const auto& gauge_state = engine_->gauge_state();
        snapshot.gauge = gauge_state.value;
        snapshot.gauge_type = gauge_state.type;

        const auto& feedback = engine_->live_feedback();
        const int64_t feedback_display_samples = ms_to_samples(kHudFeedbackDisplayMs, sample_rate_);
        const int64_t feedback_age_samples =
            feedback.has_value ? std::max<int64_t>(0, snapshot.current_sample - feedback.sample) : 0;
        snapshot.has_feedback = feedback.has_value && feedback_age_samples <= feedback_display_samples;
        snapshot.feedback_judgement = feedback.judgement;
        snapshot.feedback_delta_ms = snapshot.has_feedback ? feedback.delta_ms : 0.0;
        engine_->collect_recent_timing_deltas(snapshot.timing_history_delta_ms, &snapshot.timing_history_count);
        engine_->collect_active_holds(active_holds_buffer_);
        snapshot.lane_activity.fill(0.0f);
        snapshot.lane_activity_count = std::min<std::size_t>(lane_activity_.size(), kGameplayHudMaxLanes);
        std::copy_n(lane_activity_.begin(), snapshot.lane_activity_count, snapshot.lane_activity.begin());

        if (ghost_engine_) {
            snapshot.ghost_visible = true;
            snapshot.ghost_finished = ghost_engine_->is_finished();
            snapshot.ghost_game_over = ghost_engine_->is_game_over();

            const auto& ghost_stats = ghost_engine_->stats();
            snapshot.ghost_combo = ghost_stats.combo;
            snapshot.ghost_max_combo = ghost_stats.max_combo;
            snapshot.ghost_counts = ghost_stats.counts;
            snapshot.ghost_score = ghost_stats.raw_score;
            snapshot.ghost_osu_od8_score_available = ghost_stats.osu_od8.available;
            snapshot.ghost_osu_od8_score = ghost_stats.osu_od8.score;

            const auto& ghost_gauge_state = ghost_engine_->gauge_state();
            snapshot.ghost_gauge = ghost_gauge_state.value;
            snapshot.ghost_gauge_type = ghost_gauge_state.type;

            const auto& ghost_feedback = ghost_engine_->live_feedback();
            const int64_t ghost_feedback_age_samples =
                ghost_feedback.has_value ? std::max<int64_t>(0, snapshot.current_sample - ghost_feedback.sample) : 0;
            snapshot.ghost_has_feedback =
                ghost_feedback.has_value && ghost_feedback_age_samples <= feedback_display_samples;
            snapshot.ghost_feedback_judgement = ghost_feedback.judgement;
            snapshot.ghost_feedback_delta_ms =
                snapshot.ghost_has_feedback ? ghost_feedback.delta_ms : 0.0;
            ghost_engine_->collect_recent_timing_deltas(snapshot.ghost_timing_history_delta_ms,
                                                        &snapshot.ghost_timing_history_count);
            ghost_engine_->collect_active_holds(ghost_active_holds_buffer_);
            snapshot.ghost_lane_activity.fill(0.0f);
            snapshot.ghost_lane_activity_count =
                std::min<std::size_t>(ghost_lane_activity_.size(), kGameplayHudMaxLanes);
            std::copy_n(ghost_lane_activity_.begin(),
                        snapshot.ghost_lane_activity_count,
                        snapshot.ghost_lane_activity.begin());
        }

    const double scroll_scale =
        game::SpeedManager::visualScrollScale(snapshot.rate, snapshot.hispeed).value_or(3.0);
    const double normalized_scale = std::max(0.1, scroll_scale / 3.0);
    const double dynamic_lookahead_ms = std::clamp(
        static_cast<double>(kHudLookaheadMs) / normalized_scale,
        350.0,
        6000.0);

    const int64_t past_samples = ms_to_samples(static_cast<double>(kHudPastMs), sample_rate_);
    const int64_t lookahead_samples = ms_to_samples(dynamic_lookahead_ms, sample_rate_);
    const GameplayHudWindow expanded_window = expand_gameplay_hud_window(
        sample_rate_,
        past_samples,
        lookahead_samples,
        config_.visual_offset_ms,
        kHudRenderSlackMs);
    snapshot.past_samples = past_samples;
    snapshot.lookahead_samples = lookahead_samples;
    snapshot.lane_activity_count = std::max<std::size_t>(
        snapshot.lane_activity_count,
        std::min<std::size_t>(static_cast<std::size_t>(snapshot.lane_count), kGameplayHudMaxLanes));

    if (hud_scan_start_ >= chart_.notes.size()) {
        hud_scan_start_ = chart_.notes.size();
    }
    while (hud_scan_start_ < chart_.notes.size() &&
           note_is_expired_for_hud(chart_.notes[hud_scan_start_], snapshot.current_sample,
                                   expanded_window.past_samples)) {
        ++hud_scan_start_;
    }

    // Hidden-note flags and active holds must come from the same engine epoch.
    // Active holds are appended first so dense lookahead never crowds them out.
    snapshot.note_count = 0;
    for (const auto& hold : active_holds_buffer_) {
        if (snapshot.note_count >= kGameplayHudMaxNotes) {
            break;
        }
        if (hold.lane <= 0 || hold.lane > snapshot.lane_count) {
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
        hud_note.head_visible = false;
        snapshot.notes[snapshot.note_count++] = hud_note;
    }

    for (std::size_t i = hud_scan_start_; i < chart_.notes.size(); ++i) {
        const auto& note = chart_.notes[i];
        if (note.note_id < hidden_hit_note_ids_.size() && hidden_hit_note_ids_[note.note_id] != 0) {
            continue;
        }
        if (note.start_sample > snapshot.current_sample + expanded_window.lookahead_samples) {
            break;
        }
        if (note.lane <= 0 || note.lane > snapshot.lane_count) {
            continue;
        }

        HudNote hud_note;
        hud_note.lane = note.lane;
        hud_note.start_sample = note.start_sample;
        hud_note.tail_sample = note_visible_end_sample(note);
        hud_note.hold = note.end_sample.has_value();
        hud_note.head_visible = true;
        snapshot.notes[snapshot.note_count++] = hud_note;
        if (snapshot.note_count >= kGameplayHudMaxNotes) {
            break;
        }
    }

    if (snapshot.ghost_visible) {
        if (ghost_hud_scan_start_ >= chart_.notes.size()) {
            ghost_hud_scan_start_ = chart_.notes.size();
        }
        while (ghost_hud_scan_start_ < chart_.notes.size() &&
               note_is_expired_for_hud(chart_.notes[ghost_hud_scan_start_], snapshot.current_sample,
                                       expanded_window.past_samples)) {
            ++ghost_hud_scan_start_;
        }

        snapshot.ghost_note_count = 0;
        for (const auto& hold : ghost_active_holds_buffer_) {
            if (snapshot.ghost_note_count >= kGameplayHudMaxNotes) {
                break;
            }
            if (hold.lane <= 0 || hold.lane > snapshot.lane_count) {
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
            hud_note.head_visible = false;
            snapshot.ghost_notes[snapshot.ghost_note_count++] = hud_note;
        }

        for (std::size_t i = ghost_hud_scan_start_; i < chart_.notes.size(); ++i) {
            const auto& note = chart_.notes[i];
            if (note.note_id < ghost_hidden_hit_note_ids_.size() && ghost_hidden_hit_note_ids_[note.note_id] != 0) {
                continue;
            }
            if (note.start_sample > snapshot.current_sample + expanded_window.lookahead_samples) {
                break;
            }
            if (note.lane <= 0 || note.lane > snapshot.lane_count) {
                continue;
            }

            HudNote hud_note;
            hud_note.lane = note.lane;
            hud_note.start_sample = note.start_sample;
            hud_note.tail_sample = note_visible_end_sample(note);
            hud_note.hold = note.end_sample.has_value();
            hud_note.head_visible = true;
            snapshot.ghost_notes[snapshot.ghost_note_count++] = hud_note;
            if (snapshot.ghost_note_count >= kGameplayHudMaxNotes) {
                break;
            }
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

    return snapshot;
}

void GameSession::adjust_hispeed(double delta) {
    if (!std::isfinite(delta) || std::abs(delta) < 1e-9) {
        return;
    }
    if (!engine_) {
        return;
    }

    double next = std::clamp(config_.speed.hi_speed + delta, kHispeedMin, kHispeedMax);
    next = std::round(next * 100.0) / 100.0;
    config_.speed.hi_speed = next;
}

void GameSession::update_hispeed_repeat_state(uint32_t keycode, input::InputState state, int64_t event_time_ns) {
    const int64_t safe_event_time_ns = (event_time_ns > 0) ? event_time_ns : timing::HighResClock::now_ns();
    auto update_repeat = [&](bool* held, int64_t* next_repeat_ns, double delta) {
        if (!held || !next_repeat_ns) {
            return;
        }
        if (state == input::InputState::Pressed) {
            adjust_hispeed(delta);
            *held = true;
            *next_repeat_ns = safe_event_time_ns + ms_to_ns(kHispeedRepeatInitialDelayMs);
        } else {
            *held = false;
            *next_repeat_ns = 0;
        }
    };

    if (f3_keycode_ != 0 && keycode == f3_keycode_) {
        update_repeat(&hispeed_decrease_held_, &hispeed_decrease_next_repeat_ns_, -kHispeedStep);
        return;
    }
    if (f4_keycode_ != 0 && keycode == f4_keycode_) {
        update_repeat(&hispeed_increase_held_, &hispeed_increase_next_repeat_ns_, kHispeedStep);
    }
}

void GameSession::service_hispeed_repeat(int64_t now_ns) {
    const int64_t safe_now_ns = (now_ns > 0) ? now_ns : timing::HighResClock::now_ns();
    while (hispeed_decrease_held_ &&
           hispeed_decrease_next_repeat_ns_ > 0 &&
           safe_now_ns >= hispeed_decrease_next_repeat_ns_) {
        adjust_hispeed(-kHispeedStep);
        hispeed_decrease_next_repeat_ns_ += ms_to_ns(kHispeedRepeatIntervalMs);
    }
    while (hispeed_increase_held_ &&
           hispeed_increase_next_repeat_ns_ > 0 &&
           safe_now_ns >= hispeed_increase_next_repeat_ns_) {
        adjust_hispeed(kHispeedStep);
        hispeed_increase_next_repeat_ns_ += ms_to_ns(kHispeedRepeatIntervalMs);
    }
}

bool GameSession::prepare_chart_audio() {
    stop_chart_audio_workers();
    chart_audio_assets_.clear();
    chart_audio_events_.clear();
    chart_audio_voices_.clear();
    chart_audio_load_queue_.clear();
    chart_audio_active_until_samples_.reset();
    next_chart_audio_event_ = 0;
    startup_preload_budget_bytes_ = 0;
    runtime_chart_audio_budget_bytes_ = 0;
    chart_audio_decoded_bytes_ = 0;
    chart_audio_deferred_count_ = 0;
    chart_audio_eviction_count_ = 0;
    last_chart_audio_service_sample_ = (std::numeric_limits<int64_t>::min)();
    chart_audio_startup_logged_ = false;
    chart_audio_steady_state_logged_ = false;
    synthetic_tones_enabled_.store(true, std::memory_order_release);

    if (loading_cancel_requested()) {
        return false;
    }

    const std::size_t asset_count = chart_.audio_assets.size();
    if (asset_count == 0) {
        return true;
    }

    chart_audio_assets_.resize(asset_count);
    chart_audio_active_until_samples_ = std::make_unique<std::atomic<int64_t>[]>(asset_count);
    for (std::size_t i = 0; i < asset_count; ++i) {
        chart_audio_assets_[i].path = chart_.audio_assets[i].path;
        chart_audio_assets_[i].estimated_decoded_bytes = estimate_audio_decoded_bytes(chart_.audio_assets[i].path,
                                                                                      sample_rate_);
        chart_audio_active_until_samples_[i].store(0, std::memory_order_release);
    }

    int64_t max_sample = chart_.duration_samples;
    chart_audio_events_.reserve(chart_.audio_cues.size());
    for (const auto& cue : chart_.audio_cues) {
        if (cue.asset_id >= chart_audio_assets_.size()) {
            continue;
        }
        auto& asset = chart_audio_assets_[cue.asset_id];
        const int64_t sample = std::max<int64_t>(0, cue.start_sample);
        asset.has_bgm = true;
        asset.use_samples.push_back(sample);
        asset.first_use_sample = (std::min)(asset.first_use_sample, sample);
        asset.last_use_sample = (std::max)(asset.last_use_sample, sample);
        ++asset.use_count;
        chart_audio_events_.push_back(ChartAudioEvent{sample, cue.asset_id, ChartAudioEvent::Kind::Bgm});
    }
    for (const auto& note : chart_.notes) {
        const int64_t sample = std::max<int64_t>(0, note.start_sample);
        for_each_note_audio_asset_id(note, [&](std::size_t asset_id) {
            if (asset_id >= chart_audio_assets_.size()) {
                return;
            }
            auto& asset = chart_audio_assets_[asset_id];
            asset.has_keysound = true;
            asset.use_samples.push_back(sample);
            asset.first_use_sample = (std::min)(asset.first_use_sample, sample);
            asset.last_use_sample = (std::max)(asset.last_use_sample, sample);
            ++asset.use_count;
        });
    }

    for (auto& asset : chart_audio_assets_) {
        std::stable_sort(asset.use_samples.begin(), asset.use_samples.end());
    }
    std::stable_sort(chart_audio_events_.begin(), chart_audio_events_.end(),
                     [](const ChartAudioEvent& lhs, const ChartAudioEvent& rhs) {
                         if (lhs.start_sample != rhs.start_sample) {
                             return lhs.start_sample < rhs.start_sample;
                         }
                         return lhs.asset_id < rhs.asset_id;
                     });

    const auto budgets = choose_chart_audio_budgets(query_system_memory_snapshot());
    startup_preload_budget_bytes_ = budgets.startup_preload_bytes;
    runtime_chart_audio_budget_bytes_ = budgets.runtime_cache_bytes;

    const std::size_t worker_count = (std::min)(static_cast<std::size_t>(2u), chart_audio_assets_.size());
    start_chart_audio_workers(worker_count);

    const int64_t startup_window_samples = ms_to_samples(static_cast<double>(kChartAudioStartupWindowMs), sample_rate_);
    std::vector<ChartAudioStartupCandidate> startup_candidates(asset_count);
    for (std::size_t asset_id = 0; asset_id < asset_count; ++asset_id) {
        const auto& asset = chart_audio_assets_[asset_id];
        startup_candidates[asset_id].first_use_sample = asset.first_use_sample;
        startup_candidates[asset_id].has_bgm = asset.has_bgm;
        startup_candidates[asset_id].estimated_decoded_bytes = asset.estimated_decoded_bytes;
        startup_candidates[asset_id].use_count = asset.use_count;
    }
    const auto startup_plan =
        build_chart_audio_startup_plan(startup_candidates, startup_window_samples, startup_preload_budget_bytes_);
    chart_audio_deferred_count_ = startup_plan.deferred_count;
    {
        std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
        for (std::size_t asset_id = 0; asset_id < startup_plan.queued_assets.size(); ++asset_id) {
            if (startup_plan.queued_assets[asset_id] == 0) {
                continue;
            }
            auto& asset = chart_audio_assets_[asset_id];
            if (asset.state == ChartAudioAssetState::Unloaded) {
                asset.state = ChartAudioAssetState::Queued;
                chart_audio_load_queue_.push_back(asset_id);
            }
        }
    }
    chart_audio_stream_cv_.notify_all();

    if (!wait_for_chart_audio_startup(startup_plan.required_assets)) {
        stop_chart_audio_workers();
        chart_audio_assets_.clear();
        chart_audio_events_.clear();
        chart_audio_voices_.clear();
        chart_audio_active_until_samples_.reset();
        return false;
    }

    for (const auto& event : chart_audio_events_) {
        if (event.asset_id >= chart_audio_assets_.size()) {
            continue;
        }
        const auto& asset = chart_audio_assets_[event.asset_id];
        auto samples = std::atomic_load_explicit(&asset.clip.samples, std::memory_order_acquire);
        const int64_t source_frames = (samples && !samples->empty())
                                          ? static_cast<int64_t>(samples->size() / 2u)
                                          : static_cast<int64_t>(asset.estimated_decoded_bytes /
                                                                 (2u * sizeof(float)));
        const int64_t playback_frames =
            chart_audio_playback_duration_frames(source_frames, config_.speed.rate);
        if (playback_frames > 0) {
            max_sample = std::max(max_sample, event.start_sample + playback_frames);
        }
    }

    chart_.duration_samples = std::max(chart_.duration_samples, max_sample);
    synthetic_tones_enabled_.store(true, std::memory_order_release);
    for (const auto& asset : chart_audio_assets_) {
        auto samples = std::atomic_load_explicit(&asset.clip.samples, std::memory_order_acquire);
        if (samples && !samples->empty()) {
            synthetic_tones_enabled_.store(false, std::memory_order_release);
            break;
        }
    }

    log_chart_audio_memory("startup-preload");
    chart_audio_startup_logged_ = true;
    return true;
}

void GameSession::start_chart_audio_workers(std::size_t worker_count) {
    stop_chart_audio_workers();
    if (worker_count == 0) {
        return;
    }

    chart_audio_loader_stop_ = false;
    chart_audio_loader_threads_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        chart_audio_loader_threads_.emplace_back(&GameSession::chart_audio_loader_thread_main, this);
    }
}

void GameSession::stop_chart_audio_workers() {
    {
        std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
        chart_audio_loader_stop_ = true;
        chart_audio_load_queue_.clear();
    }
    chart_audio_stream_cv_.notify_all();
    for (auto& thread : chart_audio_loader_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    chart_audio_loader_threads_.clear();
    chart_audio_loader_stop_ = false;
}

void GameSession::chart_audio_loader_thread_main() {
    for (;;) {
        std::size_t asset_id = 0;
        std::string path;
        {
            std::unique_lock<std::mutex> lock(chart_audio_stream_mutex_);
            chart_audio_stream_cv_.wait(lock, [this]() {
                return chart_audio_loader_stop_ || !chart_audio_load_queue_.empty();
            });
            if (chart_audio_loader_stop_) {
                return;
            }

            asset_id = chart_audio_load_queue_.front();
            chart_audio_load_queue_.pop_front();
            if (asset_id >= chart_audio_assets_.size()) {
                continue;
            }
            auto& asset = chart_audio_assets_[asset_id];
            if (asset.state != ChartAudioAssetState::Queued) {
                continue;
            }
            asset.state = ChartAudioAssetState::Loading;
            path = asset.path;
        }

        std::vector<float> decoded;
        std::string error;
        const bool success = decode_audio_stereo_resampled(path, sample_rate_, decoded, &error);
        const std::uint64_t decoded_bytes = static_cast<std::uint64_t>(decoded.size()) * sizeof(float);
        auto clip_samples = std::make_shared<const std::vector<float>>(std::move(decoded));

        {
            std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
            if (asset_id >= chart_audio_assets_.size()) {
                continue;
            }
            auto& asset = chart_audio_assets_[asset_id];
            if (asset.state != ChartAudioAssetState::Loading) {
                continue;
            }

            if (!success || !clip_samples || clip_samples->empty()) {
                asset.state = ChartAudioAssetState::Failed;
                std::atomic_store_explicit(&asset.clip.samples,
                                           std::shared_ptr<const std::vector<float>>{},
                                           std::memory_order_release);
                asset.decoded_bytes = 0;
                if (!success) {
                    std::cerr << "[warn] Failed to load audio cue '" << path << "': " << error << std::endl;
                }
            } else {
                chart_audio_decoded_bytes_ += decoded_bytes;
                asset.decoded_bytes = decoded_bytes;
                std::atomic_store_explicit(&asset.clip.samples, clip_samples, std::memory_order_release);
                asset.state = ChartAudioAssetState::Ready;
                synthetic_tones_enabled_.store(false, std::memory_order_release);
            }
        }
        chart_audio_stream_cv_.notify_all();
    }
}

bool GameSession::wait_for_chart_audio_startup(const std::vector<uint8_t>& required_assets) {
    if (required_assets.empty()) {
        return true;
    }

    std::unique_lock<std::mutex> lock(chart_audio_stream_mutex_);
    for (;;) {
        if (loading_cancel_requested()) {
            return false;
        }

        bool pending_required = false;
        for (std::size_t asset_id = 0; asset_id < required_assets.size(); ++asset_id) {
            if (required_assets[asset_id] == 0 || asset_id >= chart_audio_assets_.size()) {
                continue;
            }
            const auto state = chart_audio_assets_[asset_id].state;
            if (state == ChartAudioAssetState::Queued || state == ChartAudioAssetState::Loading) {
                pending_required = true;
                break;
            }
        }
        if (!pending_required) {
            return true;
        }

        chart_audio_stream_cv_.wait_for(lock, std::chrono::milliseconds(10));
    }
}

void GameSession::log_chart_audio_memory(std::string_view phase) {
    std::size_t asset_count = 0;
    std::uint64_t decoded_bytes = 0;
    std::size_t deferred_count = 0;
    std::size_t eviction_count = 0;
    {
        std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
        asset_count = chart_audio_assets_.size();
        decoded_bytes = chart_audio_decoded_bytes_;
        deferred_count = chart_audio_deferred_count_;
        eviction_count = chart_audio_eviction_count_;
    }
    log_memory_phase("GameSession",
                     phase,
                     query_process_memory_snapshot(),
                     "asset_count=" + std::to_string(asset_count) +
                         " decoded=" + format_memory_bytes(decoded_bytes) +
                         " deferred=" + std::to_string(deferred_count) +
                         " evictions=" + std::to_string(eviction_count));
}

void GameSession::service_chart_audio_streaming(int64_t current_sample) {
    if (chart_audio_assets_.empty() || sample_rate_ <= 0) {
        return;
    }
    const int64_t min_step = ms_to_samples(static_cast<double>(kChartAudioServiceIntervalMs), sample_rate_);
    if (last_chart_audio_service_sample_ != (std::numeric_limits<int64_t>::min)() &&
        current_sample < last_chart_audio_service_sample_ + min_step) {
        return;
    }

    last_chart_audio_service_sample_ = current_sample;
    queue_chart_audio_prefetch(current_sample);
    trim_chart_audio_cache(current_sample);

    if (!chart_audio_steady_state_logged_ &&
        current_sample >= ms_to_samples(static_cast<double>(kChartAudioPrefetchWindowMs), sample_rate_)) {
        log_chart_audio_memory("steady-state-cache");
        chart_audio_steady_state_logged_ = true;
    }
}

void GameSession::queue_chart_audio_prefetch(int64_t current_sample) {
    const int64_t horizon_sample =
        current_sample + ms_to_samples(static_cast<double>(kChartAudioPrefetchWindowMs), sample_rate_);
    bool queued_any = false;

    std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
    for (std::size_t asset_id = 0; asset_id < chart_audio_assets_.size(); ++asset_id) {
        auto& asset = chart_audio_assets_[asset_id];
        while (asset.next_use_index < asset.use_samples.size() &&
               asset.use_samples[asset.next_use_index] < current_sample) {
            ++asset.next_use_index;
        }
        if (asset.next_use_index >= asset.use_samples.size()) {
            continue;
        }
        if (asset.use_samples[asset.next_use_index] > horizon_sample) {
            continue;
        }
        if (asset.state == ChartAudioAssetState::Unloaded) {
            asset.state = ChartAudioAssetState::Queued;
            chart_audio_load_queue_.push_back(asset_id);
            queued_any = true;
        }
    }

    if (queued_any) {
        chart_audio_stream_cv_.notify_all();
    }
}

void GameSession::trim_chart_audio_cache(int64_t current_sample) {
    if (chart_audio_decoded_bytes_ <= runtime_chart_audio_budget_bytes_) {
        return;
    }

    struct EvictCandidate {
        std::size_t asset_id = 0;
        int64_t next_use_sample = (std::numeric_limits<int64_t>::max)();
    };

    const int64_t horizon_sample =
        current_sample + ms_to_samples(static_cast<double>(kChartAudioPrefetchWindowMs), sample_rate_);

    std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
    auto evict_until = [&](bool include_prefetch_window) {
        std::vector<EvictCandidate> candidates;
        for (std::size_t asset_id = 0; asset_id < chart_audio_assets_.size(); ++asset_id) {
            auto& asset = chart_audio_assets_[asset_id];
            if (asset.state != ChartAudioAssetState::Ready || asset.decoded_bytes == 0) {
                continue;
            }

            while (asset.next_use_index < asset.use_samples.size() &&
                   asset.use_samples[asset.next_use_index] < current_sample) {
                ++asset.next_use_index;
            }

            const int64_t active_until = chart_audio_active_until_samples_
                                             ? chart_audio_active_until_samples_[asset_id].load(std::memory_order_acquire)
                                             : 0;
            if (active_until > current_sample) {
                continue;
            }

            const int64_t next_use =
                (asset.next_use_index < asset.use_samples.size()) ? asset.use_samples[asset.next_use_index]
                                                                  : (std::numeric_limits<int64_t>::max)();
            if (!include_prefetch_window && next_use <= horizon_sample) {
                continue;
            }
            candidates.push_back(EvictCandidate{asset_id, next_use});
        }

        std::stable_sort(candidates.begin(), candidates.end(), [](const EvictCandidate& lhs,
                                                                  const EvictCandidate& rhs) {
            if (lhs.next_use_sample != rhs.next_use_sample) {
                return lhs.next_use_sample > rhs.next_use_sample;
            }
            return lhs.asset_id < rhs.asset_id;
        });

        for (const auto& candidate : candidates) {
            if (chart_audio_decoded_bytes_ <= runtime_chart_audio_budget_bytes_) {
                break;
            }
            auto& asset = chart_audio_assets_[candidate.asset_id];
            auto samples = std::atomic_load_explicit(&asset.clip.samples, std::memory_order_acquire);
            if (!samples) {
                continue;
            }
            std::atomic_store_explicit(&asset.clip.samples,
                                       std::shared_ptr<const std::vector<float>>{},
                                       std::memory_order_release);
            chart_audio_decoded_bytes_ -= asset.decoded_bytes;
            asset.decoded_bytes = 0;
            asset.state = ChartAudioAssetState::Unloaded;
            ++chart_audio_eviction_count_;
        }
    };

    evict_until(false);
    if (chart_audio_decoded_bytes_ > runtime_chart_audio_budget_bytes_) {
        evict_until(true);
    }
}

void GameSession::schedule_note_keysound(const gameplay::NoteEvent& note, int64_t sample) {
    const int64_t start_sample = std::max<int64_t>(0, sample);
    const float gain = std::clamp(note.audio_gain, 0.0f, 1.0f);
    for_each_note_audio_asset_id(note, [&](std::size_t asset_id) {
        if (asset_id >= chart_audio_assets_.size()) {
            return;
        }
        chart_audio_voices_.push_back(
            ChartAudioVoice{start_sample, asset_id, ChartAudioEvent::Kind::Keysound, gain});
    });
}

void GameSession::schedule_chart_audio(int64_t buffer_end_samples) {
    while (next_chart_audio_event_ < chart_audio_events_.size()) {
        const auto& evt = chart_audio_events_[next_chart_audio_event_];
        if (evt.start_sample >= buffer_end_samples) {
            break;
        }
        chart_audio_voices_.push_back(ChartAudioVoice{evt.start_sample, evt.asset_id, evt.kind, 1.0f});
        ++next_chart_audio_event_;
    }
}

void GameSession::mix_chart_audio(float* output, uint32_t frames, int64_t buffer_start_samples) {
    if (!output || frames == 0 || chart_audio_voices_.empty()) {
        return;
    }

    const float bgm_gain = config_.audio_ui.background_sound_enabled
                               ? static_cast<float>(std::clamp(config_.audio_ui.bgm_volume, 0.0, 2.0))
                               : 0.0f;
    const float keysound_gain = static_cast<float>(std::clamp(config_.audio_ui.keysound_volume, 0.0, 2.0));
    const int64_t buffer_end_samples = buffer_start_samples + static_cast<int64_t>(frames);
    std::size_t write_index = 0;
    for (std::size_t i = 0; i < chart_audio_voices_.size(); ++i) {
        const auto voice = chart_audio_voices_[i];
        if (voice.asset_id >= chart_audio_assets_.size()) {
            continue;
        }

        const auto& asset = chart_audio_assets_[voice.asset_id];
        auto samples = std::atomic_load_explicit(&asset.clip.samples, std::memory_order_acquire);
        if (!samples || samples->empty()) {
            const int64_t estimated_source_frames =
                static_cast<int64_t>(asset.estimated_decoded_bytes / (2u * sizeof(float)));
            const int64_t estimated_playback_frames =
                chart_audio_playback_duration_frames(estimated_source_frames, config_.speed.rate);
            if (estimated_playback_frames > 0 &&
                voice.start_sample + estimated_playback_frames <= buffer_start_samples) {
                continue;
            }
            chart_audio_voices_[write_index++] = voice;
            continue;
        }

        const int64_t source_frames = static_cast<int64_t>(samples->size() / 2u);
        const int64_t playback_frames =
            chart_audio_playback_duration_frames(source_frames, config_.speed.rate);
        const int64_t active_until = voice.start_sample + playback_frames;
        if (chart_audio_active_until_samples_) {
            auto& slot = chart_audio_active_until_samples_[voice.asset_id];
            int64_t observed = slot.load(std::memory_order_acquire);
            while (observed < active_until &&
                   !slot.compare_exchange_weak(observed, active_until, std::memory_order_acq_rel)) {
            }
        }

        const int64_t clip_end = voice.start_sample + playback_frames;
        if (clip_end <= buffer_start_samples) {
            continue;
        }

        const float gain =
            ((voice.kind == ChartAudioEvent::Kind::Keysound) ? keysound_gain : bgm_gain) *
            std::clamp(voice.gain, 0.0f, 1.0f);
        (void)mix_chart_audio_clip_linear(*samples,
                                          voice.start_sample,
                                          config_.speed.rate,
                                          gain,
                                          output,
                                          frames,
                                          buffer_start_samples);

        if (clip_end <= buffer_end_samples) {
            continue;
        }
        chart_audio_voices_[write_index++] = voice;
    }
    chart_audio_voices_.resize(write_index);
}

void GameSession::clamp_output(float* output, uint32_t frames, float master_gain) {
    if (!output || frames == 0) {
        return;
    }
    const std::size_t sample_count = static_cast<std::size_t>(frames) * 2;
    for (std::size_t i = 0; i < sample_count; ++i) {
        output[i] = soft_limit_sample(output[i] * master_gain);
    }
}

void GameSession::shutdown() {
    stop_requested_.store(true, std::memory_order_release);
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
    if (engine_ && gameplay_started_) {
        bool engine_game_over = false;
        bool engine_finished = false;
        game::GaugeType final_gauge = game::GaugeType::Normal;
        {
            std::lock_guard<std::mutex> lock(engine_mutex_);
            result_.stats = engine_->stats();
            engine_game_over = engine_->is_game_over();
            engine_finished = engine_->is_finished();
            final_gauge = engine_->gauge_state().type;
        }
        result_.finished = engine_finished;
        result_.clear_status = gameplay_session_clear_status(
            engine_finished,
            engine_game_over,
            user_aborted_.load(std::memory_order_acquire),
            final_gauge,
            autoplay_enabled_,
            practice_no_fail_enabled_,
            one_miss_fail_enabled_);
        result_.game_over = !gameplay_session_cleared(
            engine_finished,
            engine_game_over,
            user_aborted_.load(std::memory_order_acquire));
        result_.mods = active_mods_;
        result_.rate_multiplier = rate_multiplier_;
        result_.score_multiplier = score_multiplier_;
        result_.final_score = std::max<int64_t>(
            0,
            static_cast<int64_t>(std::llround(static_cast<double>(result_.stats.raw_score) *
                                              result_.score_multiplier)));
        result_.has_value = true;

        const std::string format_token = chart_format_token(chart_format_);
        if (replay_playback_enabled_) {
            result_.replay_path = replay_source_path_;
        } else {
            const std::string created_utc = utc_timestamp_compact();
            std::filesystem::path profile_dir(profile_dir_);
            std::filesystem::path replay_path = profile_dir / "replays" / ("replay_" + created_utc + ".json");
            std::filesystem::path result_path = profile_dir / "results" / ("result_" + created_utc + ".json");

            gameplay::ReplayFile replay;
            replay.chart_path = chart_path_;
            replay.chart_format = format_token;
            replay.created_utc = created_utc;
            replay.sample_rate = sample_rate_;
            replay.rate = config_.speed.rate;
            replay.input_offset_ms = config_.input_offset_ms;
            replay.mods = result_.mods;
            replay.rate_multiplier = result_.rate_multiplier;
            replay.score_multiplier = result_.score_multiplier;
            replay.final_score = result_.final_score;
            replay.mode.key_mode = config_.mode.key_mode;
            replay.mode.random = config_.mode.random;
            replay.mode.random_seed = config_.mode.random_seed;
            replay.mode.gauge = config_.mode.gauge;
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
                result_.export_warnings.insert(result_.export_warnings.end(),
                                               replay_export.warnings.begin(),
                                               replay_export.warnings.end());
            }

            gameplay::ResultFile exported_result;
            exported_result.chart_path = chart_path_;
            exported_result.chart_format = format_token;
            exported_result.created_utc = created_utc;
            exported_result.replay_path = result_.replay_path;
            exported_result.clear_status = result_.clear_status;
            exported_result.final_gauge = gauge_type_token(final_gauge);
            exported_result.sample_rate = sample_rate_;
            exported_result.rate = replay.rate;
            exported_result.game_over = result_.game_over;
            exported_result.mods = result_.mods;
            exported_result.rate_multiplier = result_.rate_multiplier;
            exported_result.score_multiplier = result_.score_multiplier;
            exported_result.final_score = result_.final_score;
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
    clock_sync_.reset();
    current_playback_sample_ = 0;
    startup_input_timing_anchor_ = {};
    audio_timing_diagnostics_logged_ = false;
    countdown_active_ = false;
    countdown_value_ = 0;
    countdown_started_ns_ = 0;
    hispeed_decrease_held_ = false;
    hispeed_increase_held_ = false;
    hispeed_decrease_next_repeat_ns_ = 0;
    hispeed_increase_next_repeat_ns_ = 0;
    result_transition_sample_ = 0;
    result_transition_pending_ = false;
    gameplay_started_ = false;
    tone_voices_.clear();
    chart_audio_assets_.clear();
    chart_audio_events_.clear();
    chart_audio_voices_.clear();
    chart_audio_load_queue_.clear();
    chart_audio_active_until_samples_.reset();
    next_chart_audio_event_ = 0;
    startup_preload_budget_bytes_ = 0;
    runtime_chart_audio_budget_bytes_ = 0;
    chart_audio_decoded_bytes_ = 0;
    chart_audio_deferred_count_ = 0;
    chart_audio_eviction_count_ = 0;
    last_chart_audio_service_sample_ = (std::numeric_limits<int64_t>::min)();
    chart_audio_startup_logged_ = false;
    chart_audio_steady_state_logged_ = false;
    synthetic_tones_enabled_.store(true, std::memory_order_release);
    chart_ = {};
    next_visual_cue_index_ = 0;
    last_visual_cue_sample_ = -1;
    current_background_base_path_.clear();
    current_background_overlay_path_.clear();
    active_mods_.clear();
    rate_multiplier_ = 1.0;
    score_multiplier_ = 1.0;
    autoplay_events_.clear();
    autoplay_enabled_ = false;
    autoplay_event_index_ = 0;
    practice_no_fail_enabled_ = false;
    one_miss_fail_enabled_ = false;
    lane_activity_.clear();
    hidden_hit_note_ids_.clear();
    active_holds_buffer_.clear();
    ghost_lane_activity_.clear();
    ghost_hidden_hit_note_ids_.clear();
    ghost_active_holds_buffer_.clear();
    polled_gameplay_keys_.clear();
    hud_scan_start_ = 0;
    ghost_hud_scan_start_ = 0;
    hud_callback_ = nullptr;
    screenshot_callback_ = nullptr;
}

void GameSession::audio_callback(float* output,
                                 uint32_t frames,
                                 int64_t buffer_start_samples,
                                 int64_t playback_sample) {
    if (output && frames > 0) {
        std::fill(output, output + frames * 2, 0.0f);
    }

    bool engine_active = false;
    {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        if (!engine_) {
            return;
        }
        engine_active = true;

        const int64_t buffer_end_samples = buffer_start_samples + static_cast<int64_t>(frames);
        const int64_t lookahead_samples = ms_to_samples(static_cast<double>(kLookaheadMs), sample_rate_);

        const int64_t now_ns = timing::HighResClock::now_ns();
        // Input timestamps must learn against the device head, not the future write cursor.
        current_playback_sample_ = playback_sample;
        startup_input_timing_anchor_ = StartupInputTimingAnchor{playback_sample, now_ns, true};
        clock_sync_.add_sample(now_ns, playback_sample);
        if (!audio_timing_diagnostics_logged_) {
            const int64_t write_ahead_frames = (std::max)(int64_t{0}, buffer_start_samples - playback_sample);
            const double write_ahead_ms =
                (sample_rate_ > 0)
                    ? (static_cast<double>(write_ahead_frames) * 1000.0 / static_cast<double>(sample_rate_))
                    : 0.0;
            std::cerr << "[info] Gameplay audio timing mode="
                      << (audio_thread_.is_exclusive() ? "exclusive" : "shared")
                      << " sample_rate=" << sample_rate_
                      << " buffer_frames=" << audio_thread_.buffer_frames()
                      << " callback_frames=" << frames
                      << " write_ahead_frames=" << write_ahead_frames
                      << " write_ahead_ms=" << write_ahead_ms
                      << std::endl;
            audio_timing_diagnostics_logged_ = true;
        }
        schedule_note_guides(buffer_start_samples, buffer_end_samples);
        process_future_events(buffer_end_samples, lookahead_samples);
        process_input_queue(buffer_start_samples, buffer_end_samples, lookahead_samples);
        service_hispeed_repeat(now_ns);
        process_autoplay_queue(buffer_end_samples, lookahead_samples);
        process_ghost_replay_queue(buffer_end_samples, lookahead_samples);
        engine_->advance(buffer_end_samples);
        if (ghost_engine_) {
            ghost_engine_->advance(buffer_end_samples);
        }

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
                result_transition_sample_ = buffer_end_samples + tail_samples;
                result_transition_pending_ = true;
            }
            if (buffer_end_samples >= result_transition_sample_) {
                finished_.store(true, std::memory_order_release);
            }
        } else {
            result_transition_pending_ = false;
            result_transition_sample_ = 0;
        }
    }

    if (engine_active) {
        const int64_t buffer_end_samples = buffer_start_samples + static_cast<int64_t>(frames);
        schedule_chart_audio(buffer_end_samples);
        mix_chart_audio(output, frames, buffer_start_samples);
        mix_tones(output, frames, buffer_start_samples);
        const float master_gain = static_cast<float>(std::clamp(config_.audio_ui.master_volume, 0.0, 1.0));
        clamp_output(output, frames, master_gain);
    }

    const int64_t committed_sample = buffer_start_samples + static_cast<int64_t>(frames);
    const int64_t committed_time_ns = timing::HighResClock::now_ns();
    audio_timing_sequence_.fetch_add(1, std::memory_order_acq_rel);
    last_audio_timing_.sample = committed_sample;
    last_audio_timing_.buffer_start_sample = buffer_start_samples;
    last_audio_timing_.playback_sample = playback_sample;
    last_audio_timing_.time_ns = committed_time_ns;
    last_audio_timing_.buffer_frames = frames;
    audio_timing_sequence_.fetch_add(1, std::memory_order_release);
    last_audio_sample_.store(committed_sample, std::memory_order_release);
}

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
        process_future_events(tick_end_samples, lookahead_samples);
        process_input_queue(tick_cursor, tick_end_samples, lookahead_samples);
        process_autoplay_queue(tick_end_samples, lookahead_samples);
        process_ghost_replay_queue(tick_end_samples, lookahead_samples);
        engine_->advance(tick_end_samples);
        if (ghost_engine_) {
            ghost_engine_->advance(tick_end_samples);
        }
        tick_cursor = tick_end_samples;
    }
}

void GameSession::process_countdown_input_queue() {
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
}

void GameSession::rebaseline_gameplay_start_input_state(int64_t sample) {
    while (input_thread_.queue().pop().has_value()) {
    }
    while (future_events_.pop().has_value()) {
    }
    pending_input_events_.clear();

    hispeed_decrease_held_ = false;
    hispeed_increase_held_ = false;
    hispeed_decrease_next_repeat_ns_ = 0;
    hispeed_increase_next_repeat_ns_ = 0;

    const int64_t baseline_time_ns = timing::HighResClock::now_ns();
    for (auto& tracked : polled_gameplay_keys_) {
        bool pressed = false;
        if (const auto poll_vk = config::KeycodeMap::polling_vk_for_keycode(tracked.keycode); poll_vk.has_value()) {
            pressed = (GetAsyncKeyState(static_cast<int>(*poll_vk)) & 0x8000) != 0;
        }
        if (!pressed) {
            continue;
        }

        input::InputEvent baseline_event{};
        baseline_event.keycode = tracked.keycode;
        baseline_event.state = input::InputState::Pressed;
        baseline_event.input_time_ns = baseline_time_ns;

        if (tracked.keycode == f3_keycode_) {
            hispeed_decrease_held_ = true;
            hispeed_decrease_next_repeat_ns_ = baseline_time_ns + ms_to_ns(kHispeedRepeatInitialDelayMs);
        } else if (tracked.keycode == f4_keycode_) {
            hispeed_increase_held_ = true;
            hispeed_increase_next_repeat_ns_ = baseline_time_ns + ms_to_ns(kHispeedRepeatInitialDelayMs);
        }

        if (autoplay_enabled_ || replay_playback_enabled_) {
            continue;
        }
        if (auto lane = lane_from_keycode(baseline_event.keycode)) {
            catch_up_lane_input(lane.value(), baseline_event.state, sample);
        }
    }

    std::fill(lane_activity_.begin(), lane_activity_.end(), 0.0f);
}

bool GameSession::handle_control_input(const input::InputEvent& event) {
    if ((f3_keycode_ != 0 && event.keycode == f3_keycode_) ||
        (f4_keycode_ != 0 && event.keycode == f4_keycode_)) {
        update_hispeed_repeat_state(event.keycode, event.state, event.input_time_ns);
        return true;
    }
    if (event.state == input::InputState::Pressed) {
        if (f5_keycode_ != 0 && event.keycode == f5_keycode_) {
            adjust_hispeed(-kHispeedStepCoarse);
            return true;
        }
        if (f6_keycode_ != 0 && event.keycode == f6_keycode_) {
            adjust_hispeed(kHispeedStepCoarse);
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
        user_aborted_.store(true, std::memory_order_release);
        stop_requested_.store(true, std::memory_order_release);
        finished_.store(true, std::memory_order_release);
        return true;
    }
    return false;
}

void GameSession::update_lane_feedback(int lane, input::InputState state) {
    const int lane_index = lane - 1;
    if (lane_index < 0 || lane_index >= static_cast<int>(lane_activity_.size())) {
        return;
    }

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

void GameSession::dispatch_lane_input(int lane, input::InputState state, int64_t sample) {
    update_lane_feedback(lane, state);
    if (state == input::InputState::Pressed) {
        schedule_tone(lane, sample, false);
    }

    auto hit_note = engine_->handle_input(lane, state, sample);
    if (state == input::InputState::Pressed && hit_note.has_value()) {
        trigger_lane_hit_effect(lane);
        if (hit_note->note_id < hidden_hit_note_ids_.size()) {
            hidden_hit_note_ids_[hit_note->note_id] = 1;
        }
        schedule_note_keysound(hit_note.value(), sample);
    }
}

void GameSession::catch_up_lane_input(int lane, input::InputState state, int64_t sample) {
    update_lane_feedback(lane, state);
    engine_->sync_input_state(lane, state, sample);
}

void GameSession::process_future_events(int64_t buffer_end_samples, int64_t lookahead_samples) {
    while (true) {
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
        if (next->sample > buffer_end_samples + lookahead_samples) {
            next->sample = buffer_end_samples;
        }
        if (autoplay_enabled_) {
            continue;
        }
        if (auto lane = lane_from_keycode(next->event.keycode)) {
            dispatch_lane_input(lane.value(), next->event.state, next->sample);
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
        auto mapped = clock_sync_.input_to_audio_samples(event.input_time_ns);
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
        int64_t sample = resolved_sample + input_offset_samples_;
        auto lane = lane_from_keycode(event.keycode);
        if (!lane.has_value()) {
            continue;
        }
        if (autoplay_enabled_) {
            continue;
        }
        if (sample > buffer_end_samples + lookahead_samples) {
            sample = buffer_end_samples;
        }

        const int lane_index = lane.value() - 1;
        if (lane_index < 0 || lane_index >= static_cast<int>(kGameplayHudMaxLanes)) {
            continue;
        }
        if (event_is_stale) {
            stale_lane_present[static_cast<std::size_t>(lane_index)] = 1;
            stale_lane_inputs[static_cast<std::size_t>(lane_index)] = BufferedLaneInput{
                lane.value(), event.state, sample};
            continue;
        }
        pending_input_events_.push_back(BufferedLaneInput{lane.value(), event.state, sample});
    }

    for (std::size_t lane_index = 0; lane_index < stale_lane_present.size(); ++lane_index) {
        if (stale_lane_present[lane_index] == 0) {
            continue;
        }
        const auto& buffered = stale_lane_inputs[lane_index];
        catch_up_lane_input(buffered.lane, buffered.state, buffered.sample);
    }

    for (const auto& buffered : pending_input_events_) {
        dispatch_lane_input(buffered.lane, buffered.state, buffered.sample);
    }
}

void GameSession::build_autoplay_events() {
    autoplay_events_.clear();
    autoplay_event_index_ = 0;
    if (!autoplay_enabled_ || chart_.notes.empty()) {
        return;
    }

    autoplay_events_.reserve(chart_.notes.size() * 2);
    for (const auto& note : chart_.notes) {
        if (note.lane <= 0) {
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
                             return lhs.state == input::InputState::Pressed &&
                                    rhs.state == input::InputState::Released;
                         }
                         return lhs.lane < rhs.lane;
                     });
}

void GameSession::process_autoplay_queue(int64_t buffer_end_samples, int64_t lookahead_samples) {
    if (!autoplay_enabled_ || !engine_) {
        return;
    }

    while (autoplay_event_index_ < autoplay_events_.size()) {
        const auto& autoplay_event = autoplay_events_[autoplay_event_index_];
        if (autoplay_event.sample > buffer_end_samples + lookahead_samples) {
            break;
        }
        dispatch_lane_input(autoplay_event.lane, autoplay_event.state, autoplay_event.sample);
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
    static_cast<void>(buffer_start_samples);

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
        if (playback_sample > buffer_end_samples + lookahead_samples) {
            break;
        }
        dispatch_lane_input(replay_event.lane, replay_event.state, playback_sample);
        ++replay_event_index_;
    }
}

void GameSession::dispatch_ghost_lane_input(int lane, input::InputState state, int64_t sample) {
    if (!ghost_engine_) {
        return;
    }

    if (state == input::InputState::Pressed) {
        const int lane_index = lane - 1;
        if (lane_index >= 0 && lane_index < static_cast<int>(ghost_lane_activity_.size())) {
            ghost_lane_activity_[static_cast<std::size_t>(lane_index)] = 1.0f;
        }
    }

    auto hit_note = ghost_engine_->handle_input(lane, state, sample);
    if (state == input::InputState::Pressed && hit_note.has_value() &&
        hit_note->note_id < ghost_hidden_hit_note_ids_.size()) {
        ghost_hidden_hit_note_ids_[hit_note->note_id] = 1;
    }
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

    const std::vector<std::string> extensions = {".bms", ".bme", ".bml"};

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
