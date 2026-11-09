bool GameSession::prepare_chart_audio() {
    stop_chart_audio_workers();
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

    if (loading_cancel_requested()) {
        return false;
    }

    const std::size_t asset_count = chart_.audio_assets.size();
    if (asset_count == 0) {
        return true;
    }

    chart_audio_assets_.resize(asset_count);
    std::vector<ChartAudioDeduplicator::Occurrence> audio_occurrences;
    for (std::size_t i = 0; i < asset_count; ++i) {
        chart_audio_assets_[i].path = chart_.audio_assets[i].path;
        chart_audio_assets_[i].estimated_decoded_bytes = estimate_audio_decoded_bytes(chart_.audio_assets[i].path,
                                                                                      sample_rate_);
    }

    chart_audio_events_.reserve(chart_.audio_cues.size());
    for (const auto& cue : chart_.audio_cues) {
        if (cue.asset_id >= chart_audio_assets_.size()) {
            continue;
        }
        auto& asset = chart_audio_assets_[cue.asset_id];
        const int64_t sample = chart_audio_start_sample_with_offset(
            cue.start_sample, config_.sound_offset_ms, sample_rate_);
        asset.has_bgm = true;
        asset.use_samples.push_back(sample);
        asset.first_use_sample = (std::min)(asset.first_use_sample, sample);
        asset.last_use_sample = (std::max)(asset.last_use_sample, sample);
        ++asset.use_count;
        const float gain = std::clamp(cue.gain, 0.0f, 1.0f);
        chart_audio_events_.push_back(ChartAudioEvent{sample, cue.asset_id, ChartAudioEvent::Kind::Bgm, gain, cue.start_sample});
        audio_occurrences.push_back({cue.start_sample, cue.asset_id, false, gain});
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
            audio_occurrences.push_back({note.start_sample, asset_id, true, std::clamp(note.audio_gain, 0.0f, 1.0f)});
        });
    }
    for (const auto& mine : chart_.mines) {
        if (mine.audio_asset_id >= chart_audio_assets_.size()) {
            continue;
        }
        auto& asset = chart_audio_assets_[mine.audio_asset_id];
        const int64_t sample = std::max<int64_t>(0, mine.sample);
        asset.has_keysound = true;
        asset.use_samples.push_back(sample);
        asset.first_use_sample = (std::min)(asset.first_use_sample, sample);
        asset.last_use_sample = (std::max)(asset.last_use_sample, sample);
        ++asset.use_count;
        audio_occurrences.push_back({mine.sample, mine.audio_asset_id, true, 1.0f});
    }

    chart_audio_deduplicator_.prepare(std::move(audio_occurrences));

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

    const std::size_t worker_count = (std::min)(static_cast<std::size_t>(2u), chart_audio_assets_.size());
    start_chart_audio_workers(worker_count);

    const int64_t startup_window_samples = ms_to_samples(static_cast<double>(kChartAudioStartupWindowMs), sample_rate_);
    std::vector<ChartAudioStartupCandidate> startup_candidates(asset_count);
    for (std::size_t asset_id = 0; asset_id < asset_count; ++asset_id) {
        const auto& asset = chart_audio_assets_[asset_id];
        startup_candidates[asset_id].first_use_sample = asset.first_use_sample;
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
        chart_audio_pcm_registry_.clear();
        chart_audio_deduplicator_.clear();
        return false;
    }

    // Chart duration is part of replay verification. Memory-budget estimates
    // are not media durations; completion follows the actual mixer below.
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

        // Resolve PCM identity through the worker-only registry, before taking
        // streaming state. The session/HUD loop also needs that state;
        // a long PCM or slow representative reload must not block its updates.
        // Registry and asset resets happen only after every worker is joined.
        std::shared_ptr<const ChartAudioPcmIdentity> identity;
        if (success && !clip_samples->empty()) {
            const auto identity_id = chart_audio_pcm_registry_.intern(clip_samples, path,
                [&](const std::string& representative_path) -> ChartAudioPcmRegistry::Samples {
                    std::vector<float> representative;
                    if (!decode_audio_stereo_resampled(representative_path, sample_rate_, representative, nullptr)) return {};
                    return std::make_shared<const std::vector<float>>(std::move(representative));
                });
            identity = std::make_shared<ChartAudioPcmIdentity>(ChartAudioPcmIdentity{clip_samples, identity_id});
        }

        {
            std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
            // Decoding/identity work ran without this lock. Stop may have been
            // requested meanwhile; never publish into a cancelled load.
            if (chart_audio_loader_stop_) {
                return;
            }
            if (asset_id >= chart_audio_assets_.size()) {
                continue;
            }
            auto& asset = chart_audio_assets_[asset_id];
            if (asset.state != ChartAudioAssetState::Loading) {
                continue;
            }

            if (!success || !clip_samples || clip_samples->empty()) {
                asset.state = ChartAudioAssetState::Failed;
                // Empty PCM is a terminal decode failure; nullptr still means
                // unloaded/loading. Publish this distinction without taking a
                // streaming mutex in the realtime mixer or keeping failed tails.
                std::atomic_store_explicit(&asset.clip.samples,
                                           std::make_shared<const std::vector<float>>(),
                                           std::memory_order_release);
                std::atomic_store_explicit(&asset.clip.identity,
                                           std::shared_ptr<const ChartAudioPcmIdentity>{},
                                           std::memory_order_release);
                asset.decoded_bytes = 0;
                if (!success) {
                    std::cerr << "[warn] Failed to load audio cue '" << path << "': " << error << std::endl;
                }
            } else {
                chart_audio_decoded_bytes_ += decoded_bytes;
                asset.decoded_bytes = decoded_bytes;
                std::atomic_store_explicit(&asset.clip.identity, std::move(identity), std::memory_order_release);
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
    {
        std::lock_guard<std::mutex> lock(chart_audio_stream_mutex_);
        asset_count = chart_audio_assets_.size();
        decoded_bytes = chart_audio_decoded_bytes_;
        deferred_count = chart_audio_deferred_count_;
    }
    log_memory_phase("GameSession",
                     phase,
                     query_process_memory_snapshot(),
                     "asset_count=" + std::to_string(asset_count) +
                         " decoded=" + format_memory_bytes(decoded_bytes) +
                         " deferred=" + std::to_string(deferred_count) +
                         " retention=until-session-stop");
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
    // Keep decoded PCM pinned through the entire session, including pauses.
    // Only shutdown, after the mixer and decode workers stop, releases it.

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

void GameSession::schedule_note_keysound(const gameplay::NoteEvent& note, int64_t sample) {
    const int64_t start_sample = std::max<int64_t>(0, sample);
    const float gain = std::clamp(note.audio_gain, 0.0f, 1.0f);
    for_each_note_audio_asset_id(note, [&](std::size_t asset_id) {
        if (asset_id >= chart_audio_assets_.size()) {
            return;
        }
        chart_audio_voices_.push_back(
            ChartAudioVoice{start_sample, asset_id, ChartAudioEvent::Kind::Keysound, gain, note.start_sample});
    });
}

void GameSession::schedule_mine_keysound(const gameplay::MineTrigger& trigger, int64_t sample) {
    if (trigger.audio_asset_id >= chart_audio_assets_.size()) {
        return;
    }
    chart_audio_voices_.push_back(ChartAudioVoice{
        std::max<int64_t>(0, sample),
        trigger.audio_asset_id,
        ChartAudioEvent::Kind::Keysound,
        1.0f,
        trigger.sample,
    });
}

void GameSession::schedule_chart_audio(int64_t buffer_end_samples) {
    while (next_chart_audio_event_ < chart_audio_events_.size()) {
        const auto& evt = chart_audio_events_[next_chart_audio_event_];
        if (evt.start_sample >= buffer_end_samples) {
            break;
        }
        chart_audio_voices_.push_back(ChartAudioVoice{evt.start_sample, evt.asset_id, evt.kind, evt.gain, evt.chart_sample});
        ++next_chart_audio_event_;
    }
}

void GameSession::mix_chart_audio(float* output, uint32_t frames, int64_t buffer_start_samples) {
    if (!output || frames == 0 || chart_audio_voices_.empty()) {
        return;
    }

    // Keep authored per-event gains; the single BMS performance slider is
    // applied once to the complete mix in finish_audio_output. Legacy profile
    // BGM/keysound balances must not alter playback or duplicate selection.
    const int64_t buffer_end_samples = buffer_start_samples + static_cast<int64_t>(frames);
    // BGM is queued after input voices. Choose the earliest audible copy in this
    // buffer rather than letting queue order or buffer size change which wins.
    chart_audio_deduplicator_.begin_buffer();
    for (std::size_t i = 0; i < chart_audio_voices_.size(); ++i) {
        auto& voice = chart_audio_voices_[i];
        if (voice.dedup_claimed || voice.start_sample >= buffer_end_samples || voice.asset_id >= chart_audio_assets_.size()) continue;
        const auto& asset = chart_audio_assets_[voice.asset_id];
        const auto samples = std::atomic_load_explicit(&asset.clip.samples, std::memory_order_acquire);
        const auto identity = std::atomic_load_explicit(&asset.clip.identity, std::memory_order_acquire);
        if (!samples || samples->empty() || !identity || identity->samples != samples) continue;
        if (voice.start_sample + chart_audio_playback_duration_frames(static_cast<int64_t>(samples->size() / 2u),
                config_.speed.rate) <= buffer_start_samples) continue;
        // Pin a selected candidate until its voice retires. Concurrent cache
        // eviction cannot remove the winner after it suppressed another copy.
        voice.decoded_identity = identity;
        const float gain = std::clamp(voice.gain, 0.0f, 1.0f);
        chart_audio_deduplicator_.consider(voice.chart_sample, voice.asset_id,
            voice.kind == ChartAudioEvent::Kind::Keysound, voice.gain, identity->id, gain, voice.start_sample, i);
    }
    std::size_t write_index = 0;
    for (std::size_t i = 0; i < chart_audio_voices_.size(); ++i) {
        auto voice = chart_audio_voices_[i];
        if (voice.asset_id >= chart_audio_assets_.size()) {
            continue;
        }

        const auto& asset = chart_audio_assets_[voice.asset_id];
        auto samples = voice.decoded_identity ? voice.decoded_identity->samples
            : std::atomic_load_explicit(&asset.clip.samples, std::memory_order_acquire);
        if (samples && samples->empty()) {
            continue;
        }
        if (!samples) {
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
        const int64_t clip_end = voice.start_sample + playback_frames;
        if (clip_end <= buffer_start_samples) {
            continue;
        }

        const float gain = std::clamp(voice.gain, 0.0f, 1.0f);
        if (!voice.dedup_claimed && voice.start_sample < buffer_end_samples) {
            const auto identity = voice.decoded_identity ? voice.decoded_identity
                : std::atomic_load_explicit(&asset.clip.identity, std::memory_order_acquire);
            const auto identity_id = identity && identity->samples == samples ? identity->id : 0;
            if (!chart_audio_deduplicator_.claim(voice.chart_sample, voice.asset_id,
                    voice.kind == ChartAudioEvent::Kind::Keysound, voice.gain, identity_id, gain, voice.start_sample, i)) continue;
            voice.dedup_claimed = true;
        }
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

void GameSession::finish_audio_output(float* output, uint32_t frames, bool foreground, bool normalize_mix) {
    if (!output || frames == 0) {
        return;
    }
    const bool normalize_audio = config_.audio_ui.normalize_audio;
    // OFF bypasses both automatic gain and the soft limiter; even ordinary
    // full-scale PCM was previously compressed above 0.92 before master gain.
    if (normalize_mix && normalize_audio) mix_normalizer_.process(output, frames);
    if (settings_click_requested_.exchange(false, std::memory_order_acq_rel))
        settings_click_.trigger(sample_rate_);
    // Apply the chart-wide slider after normalization, before UI feedback. At
    // 100% the chart mix is byte-for-byte unchanged; menu music never enters here.
    const float chart_gain = gameplay_volume_gain(config_.audio_ui.gameplay_volume);
    if (chart_gain != 1.0f) {
        for (std::size_t i = 0; i < static_cast<std::size_t>(frames) * 2; ++i)
            output[i] *= chart_gain;
    }
    // Keep UI feedback out of automatic loudness tracking.
    settings_click_.mix(output, frames);
    const double master_gain = focused_audio_master_gain(
        config_.audio_ui.master_volume, config_.audio_ui.mute_when_inactive, foreground);
    const std::size_t sample_count = static_cast<std::size_t>(frames) * 2;
    for (std::size_t i = 0; i < sample_count; ++i) {
        output[i] = normalize_audio
            ? apply_master_volume_to_sample(output[i], master_gain)
            : apply_clean_master_volume_to_sample(output[i], master_gain);
    }
}

