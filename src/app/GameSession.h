#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "app/ChartLoader.h"
#include "app/CommandLine.h"
#include "app/InputBackendStatus.h"
#include "app/GameSessionInputTiming.h"
#include "audio/AudioThread.h"
#include "config/Config.h"
#include "config/Keymap.h"
#include "config/KeycodeMap.h"
#include "GameplayHudLimits.h"
#include "app/JudgementLoopTiming.h"
#include "gameplay/GameplayEngine.h"
#include "input/InputThread.h"
#include "timing/ClockSync.h"

namespace tenriff::app {

class GameSession {
public:
    struct HudNote {
        int lane = 1;
        int64_t start_sample = 0;
        int64_t tail_sample = 0;
        bool hold = false;
        bool head_visible = true;
    };

    struct HudSnapshot {
        bool active = false;
        bool finished = false;
        bool game_over = false;
        bool spectating_peer = false;
        bool user_aborted = false;
        bool countdown_active = false;
        int countdown_value = 0;

        int lane_count = 10;
        int64_t current_sample = 0;
        int64_t duration_samples = 0;
        int sample_rate = 48000;
        int64_t audio_sample_time_ns = 0;
        int64_t hud_publish_time_ns = 0;
        uint32_t audio_buffer_frames = 0;
        int64_t lookahead_samples = 0;
        int64_t past_samples = 0;

        int combo = 0;
        int max_combo = 0;
        gameplay::JudgementCounts counts;
        int64_t score = 0;
        bool osu_od8_score_available = false;
        int64_t osu_od8_score = 0;

        double gauge = 0.0;
        game::GaugeType gauge_type = game::GaugeType::Normal;

        double rate = 1.0;
        double hispeed = 3.0;

        bool has_feedback = false;
        game::Judgement feedback_judgement = game::Judgement::BD;
        double feedback_delta_ms = 0.0;
        std::size_t timing_history_count = 0;
        std::array<double, kGameplayTimingHistoryMaxEntries> timing_history_delta_ms{};

        std::size_t lane_activity_count = 0;
        std::array<float, kGameplayHudMaxLanes> lane_activity{};
        std::size_t note_count = 0;
        std::array<HudNote, kGameplayHudMaxNotes> notes{};

        bool ghost_visible = false;
        int64_t ghost_score = 0;
        bool ghost_osu_od8_score_available = false;
        int64_t ghost_osu_od8_score = 0;
        int ghost_combo = 0;
        int ghost_max_combo = 0;
        gameplay::JudgementCounts ghost_counts;
        double ghost_gauge = 0.0;
        game::GaugeType ghost_gauge_type = game::GaugeType::Normal;
        bool ghost_has_feedback = false;
        game::Judgement ghost_feedback_judgement = game::Judgement::BD;
        double ghost_feedback_delta_ms = 0.0;
        std::size_t ghost_timing_history_count = 0;
        std::array<double, kGameplayTimingHistoryMaxEntries> ghost_timing_history_delta_ms{};
        bool ghost_finished = false;
        bool ghost_game_over = false;
        std::size_t ghost_lane_activity_count = 0;
        std::array<float, kGameplayHudMaxLanes> ghost_lane_activity{};
        std::size_t ghost_note_count = 0;
        std::array<HudNote, kGameplayHudMaxNotes> ghost_notes{};
    };

    using HudCallback = std::function<void(const HudSnapshot&)>;

    struct LoadingProgress {
        int percent = 0;
        std::string stage;
    };

    using LoadingProgressCallback = std::function<void(const LoadingProgress&)>;
    using LoadingCancelCallback = std::function<bool()>;
    using ScreenshotCallback = std::function<void()>;
    using PeerSpectatorDoneCallback = std::function<bool()>;

    struct GameSessionResult {
        bool has_value = false;
        bool game_over = false;
        bool finished = false;
        std::string clear_status = "FAILED";
        gameplay::ResultStats stats;
        std::vector<std::string> mods;
        double rate_multiplier = 1.0;
        double score_multiplier = 1.0;
        int64_t final_score = 0;
        std::string replay_path;
        std::string result_path;
        std::vector<std::string> export_warnings;
    };

    GameSession();
    ~GameSession();

    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    [[nodiscard]] bool initialize(const CommandLineOptions& options);
    void run();
    void shutdown();
    void set_hud_callback(HudCallback callback);
    void set_loading_progress_callback(LoadingProgressCallback callback);
    void set_loading_cancel_callback(LoadingCancelCallback callback);
    void set_screenshot_callback(ScreenshotCallback callback);
    void set_peer_spectator_done_callback(PeerSpectatorDoneCallback callback);
    void set_peer_battle_mode(bool enabled) { peer_battle_mode_ = enabled; }
    void set_input_backend_fallback_override(const InputBackendRuntimeState& state) {
        force_polling_input_ = state.auto_fallback &&
                               state.effective_backend == input::InputBackend::Polling;
        forced_polling_input_state_ = state;
    }
    [[nodiscard]] HudSnapshot hud_snapshot();
    [[nodiscard]] bool was_user_aborted() const { return user_aborted_.load(std::memory_order_acquire); }
    [[nodiscard]] const GameSessionResult& result() const { return result_; }
    [[nodiscard]] double final_hispeed() const { return config_.speed.hi_speed; }
    [[nodiscard]] bool final_rawinput_enabled() const { return config_.input.rawinput; }
    [[nodiscard]] const InputBackendRuntimeState& input_backend_state() const { return input_backend_state_; }

private:
    struct FutureEvent {
        input::InputEvent event;
        int64_t sample = 0;
    };

    struct FutureQueue {
        static constexpr std::size_t kCapacity = 256;
        FutureEvent data[kCapacity];
        std::size_t head = 0;
        std::size_t tail = 0;

        bool push(const FutureEvent& evt);
        std::optional<FutureEvent> pop();
        [[nodiscard]] std::optional<FutureEvent> peek() const;
        void consume();
        [[nodiscard]] bool empty() const { return head == tail; }
    };

    struct ToneVoice {
        int64_t start_sample = 0;
        int64_t end_sample = 0;
        double phase = 0.0;
        double phase_step = 0.0;
        float gain_l = 0.0f;
        float gain_r = 0.0f;
    };

    struct ChartAudioClip {
        std::shared_ptr<const std::vector<float>> samples;
    };

    enum class ChartAudioAssetState {
        Unloaded,
        Queued,
        Loading,
        Ready,
        Failed,
    };

    struct ChartAudioAsset {
        std::string path;
        std::vector<int64_t> use_samples;
        int64_t first_use_sample = (std::numeric_limits<int64_t>::max)();
        int64_t last_use_sample = 0;
        std::size_t next_use_index = 0;
        int use_count = 0;
        bool has_bgm = false;
        bool has_keysound = false;
        std::uint64_t estimated_decoded_bytes = 0;
        std::uint64_t decoded_bytes = 0;
        ChartAudioClip clip;
        ChartAudioAssetState state = ChartAudioAssetState::Unloaded;
    };

    struct ChartAudioEvent {
        enum class Kind {
            Bgm,
            Keysound,
        };

        int64_t start_sample = 0;
        std::size_t asset_id = 0;
        Kind kind = Kind::Bgm;
    };

    struct ChartAudioVoice {
        int64_t start_sample = 0;
        std::size_t asset_id = 0;
        ChartAudioEvent::Kind kind = ChartAudioEvent::Kind::Bgm;
        float gain = 1.0f;
    };

    struct BufferedLaneInput {
        int lane = 1;
        input::InputState state = input::InputState::Released;
        int64_t sample = 0;
    };

    struct PolledGameplayKey {
        uint32_t keycode = 0;
    };

    struct AudioTimingState {
        int64_t sample = 0;
        int64_t buffer_start_sample = 0;
        int64_t playback_sample = 0;
        int64_t time_ns = 0;
        uint32_t buffer_frames = 0;
    };

    void audio_callback(float* output,
                        uint32_t frames,
                        int64_t buffer_start_samples,
                        int64_t playback_sample);
    void refresh_judgement_loop_timing();
    [[nodiscard]] int64_t next_judgement_loop_step_samples();
    void run_judgement_loop(int64_t buffer_start_samples, int64_t buffer_end_samples, int64_t lookahead_samples);
    void process_countdown_input_queue();
    void rebaseline_gameplay_start_input_state(int64_t sample);
    void process_future_events(int64_t buffer_end_samples, int64_t lookahead_samples);
    void process_input_queue(int64_t buffer_start_samples, int64_t buffer_end_samples, int64_t lookahead_samples);
    [[nodiscard]] bool handle_control_input(const input::InputEvent& event);
    void rebuild_input_thread_config(input::InputThreadConfig& config) const;
    void note_runtime_input_event_source(const input::InputEvent& event);
    void rebuild_polled_gameplay_keys();
    void update_lane_feedback(int lane, input::InputState state);
    void trigger_lane_hit_effect(int lane);
    void dispatch_lane_input(int lane, input::InputState state, int64_t sample);
    void catch_up_lane_input(int lane, input::InputState state, int64_t sample);
    void schedule_note_guides(int64_t buffer_start_samples, int64_t buffer_end_samples);
    void schedule_note_keysound(const gameplay::NoteEvent& note, int64_t sample);
    void schedule_tone(int lane, int64_t sample, bool guide);
    void adjust_hispeed(double delta);
    void update_hispeed_repeat_state(uint32_t keycode, input::InputState state, int64_t event_time_ns);
    void service_hispeed_repeat(int64_t now_ns);
    [[nodiscard]] bool prepare_chart_audio();
    void start_chart_audio_workers(std::size_t worker_count);
    void stop_chart_audio_workers();
    void chart_audio_loader_thread_main();
    void service_chart_audio_streaming(int64_t current_sample);
    void queue_chart_audio_prefetch(int64_t current_sample);
    void trim_chart_audio_cache(int64_t current_sample);
    [[nodiscard]] bool wait_for_chart_audio_startup(const std::vector<uint8_t>& required_assets);
    void log_chart_audio_memory(std::string_view phase);
    void schedule_chart_audio(int64_t buffer_end_samples);
    void mix_chart_audio(float* output, uint32_t frames, int64_t buffer_start_samples);
    void mix_tones(float* output, uint32_t frames, int64_t buffer_start_samples);
    static void clamp_output(float* output, uint32_t frames, float master_gain);
    void report_loading_progress(int percent, std::string_view stage);
    [[nodiscard]] bool loading_cancel_requested();
    [[nodiscard]] int64_t playback_sample_for_replay_event(const gameplay::ReplayFile& replay,
                                                           int64_t replay_sample) const;
    void build_autoplay_events();
    void process_replay_input_queue(int64_t buffer_start_samples, int64_t buffer_end_samples, int64_t lookahead_samples);
    void process_autoplay_queue(int64_t buffer_end_samples, int64_t lookahead_samples);
    void process_ghost_replay_queue(int64_t buffer_end_samples, int64_t lookahead_samples);
    void dispatch_ghost_lane_input(int lane, input::InputState state, int64_t sample);

    [[nodiscard]] std::optional<int> lane_from_keycode(uint32_t keycode) const;
    [[nodiscard]] double lane_frequency_hz(int lane) const;
    [[nodiscard]] std::string find_first_chart(const std::string& root_path) const;

    config::RuntimeConfig config_;
    config::Keymap keymap_;
    CommandLineOptions options_;
    std::string profile_dir_;
    std::string chart_path_;
    std::string replay_source_path_;
    ChartFormat chart_format_ = ChartFormat::Unknown;
    gameplay::GameplayChart chart_;
    std::vector<std::string> active_mods_{};
    double rate_multiplier_ = 1.0;
    double score_multiplier_ = 1.0;
    gameplay::ReplayFile replay_source_{};
    bool replay_playback_enabled_ = false;
    std::size_t replay_event_index_ = 0;
    std::vector<gameplay::ReplayEvent> autoplay_events_{};
    bool autoplay_enabled_ = false;
    std::size_t autoplay_event_index_ = 0;
    bool practice_no_fail_enabled_ = false;
    bool one_miss_fail_enabled_ = false;
    gameplay::ReplayFile ghost_replay_source_{};
    bool ghost_replay_enabled_ = false;
    std::size_t ghost_replay_event_index_ = 0;

    std::unique_ptr<gameplay::GameplayEngine> engine_;
    std::unique_ptr<gameplay::GameplayEngine> ghost_engine_;
    std::mutex engine_mutex_;

    input::InputThread input_thread_;
    audio::AudioThread audio_thread_;
    timing::ClockSync clock_sync_;

    std::unordered_map<uint32_t, int> key_to_lane_;

    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> finished_{false};
    std::atomic<bool> spectating_peer_{false};
    std::atomic<bool> user_aborted_{false};
    std::atomic<int64_t> last_audio_sample_{0};
    std::atomic<uint64_t> audio_timing_sequence_{0};
    AudioTimingState last_audio_timing_{};
    StartupInputTimingAnchor startup_input_timing_anchor_{};
    bool countdown_active_ = false;
    int countdown_value_ = 0;
    bool hispeed_decrease_held_ = false;
    bool hispeed_increase_held_ = false;
    uint32_t escape_keycode_ = 0;
    uint32_t f3_keycode_ = 0;
    uint32_t f4_keycode_ = 0;
    uint32_t f5_keycode_ = 0;
    uint32_t f6_keycode_ = 0;
    uint32_t f9_keycode_ = 0;

    int64_t input_offset_samples_ = 0;
    int64_t current_playback_sample_ = 0;
    int sample_rate_ = 48000;
    JudgementLoopTimingPlan judgement_loop_plan_{};
    int64_t judgement_loop_step_carry_ = 0;
    std::size_t next_guide_note_index_ = 0;
    std::size_t hud_scan_start_ = 0;
    int64_t countdown_started_ns_ = 0;
    int64_t hispeed_decrease_next_repeat_ns_ = 0;
    int64_t hispeed_increase_next_repeat_ns_ = 0;
    int64_t result_transition_sample_ = 0;
    bool gameplay_started_ = false;
    bool result_transition_pending_ = false;
    bool peer_battle_mode_ = false;
    bool force_polling_input_ = false;

    FutureQueue future_events_{};
    InputBackendRuntimeState forced_polling_input_state_{};
    InputBackendRuntimeState input_backend_state_{};
    std::vector<PolledGameplayKey> polled_gameplay_keys_;
    std::vector<ToneVoice> tone_voices_;
    std::vector<ChartAudioAsset> chart_audio_assets_;
    std::vector<ChartAudioEvent> chart_audio_events_;
    std::vector<ChartAudioVoice> chart_audio_voices_;
    std::mutex chart_audio_stream_mutex_;
    std::condition_variable chart_audio_stream_cv_;
    std::deque<std::size_t> chart_audio_load_queue_;
    std::vector<std::thread> chart_audio_loader_threads_;
    std::unique_ptr<std::atomic<int64_t>[]> chart_audio_active_until_samples_;
    std::vector<BufferedLaneInput> pending_input_events_;
    std::vector<uint8_t> hidden_hit_note_ids_;
    std::vector<uint8_t> ghost_hidden_hit_note_ids_;
    std::vector<gameplay::ActiveHoldView> active_holds_buffer_;
    std::vector<gameplay::ActiveHoldView> ghost_active_holds_buffer_;
    std::size_t next_chart_audio_event_ = 0;
    std::uint64_t startup_preload_budget_bytes_ = 0;
    std::uint64_t runtime_chart_audio_budget_bytes_ = 0;
    std::uint64_t chart_audio_decoded_bytes_ = 0;
    std::size_t chart_audio_deferred_count_ = 0;
    std::size_t chart_audio_eviction_count_ = 0;
    int64_t last_chart_audio_service_sample_ = (std::numeric_limits<int64_t>::min)();
    bool chart_audio_loader_stop_ = false;
    bool chart_audio_startup_logged_ = false;
    bool chart_audio_steady_state_logged_ = false;
    std::atomic<bool> synthetic_tones_enabled_{true};
    bool audio_timing_diagnostics_logged_ = false;
    std::vector<float> lane_activity_;
    std::vector<float> ghost_lane_activity_;
    HudCallback hud_callback_;
    LoadingProgressCallback loading_progress_callback_;
    LoadingCancelCallback loading_cancel_callback_;
    ScreenshotCallback screenshot_callback_;
    PeerSpectatorDoneCallback peer_spectator_done_callback_;
    int last_loading_percent_ = -1;
    std::string last_loading_stage_;

    GameSessionResult result_{};
    std::size_t ghost_hud_scan_start_ = 0;
};

}  // namespace tenriff::app
