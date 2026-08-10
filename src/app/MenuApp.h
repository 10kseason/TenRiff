#pragma once

#include "app/MultiplayerMenuState.h"
#include "network/PeerSession.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "app/CommandLine.h"
#include "app/GameplayHudRevisions.h"
#include "app/InputBackendStatus.h"
#include "app/Lr2Course.h"
#include "app/MenuMusicController.h"
#include "app/MultiplayerChartSearch.h"
#include "app/SessionMix.h"
#include "app/SongSelectState.h"
#include "GameplayHudLimits.h"
#include "app/SongIndex.h"
#include "app/SongIndexerThread.h"
#include "app/TenRiffSkin.h"
#include "audio/AudioThread.h"
#include "config/Config.h"
#include "config/Keymap.h"
#include "gameplay/ResultStats.h"
#include "input/InputThread.h"
#include "input/RawInputHealthProbe.h"
#include "render/MenuWindow.h"
#include "render/RenderThread.h"

namespace tenriff::app {

[[nodiscard]] std::string resolve_keymap_edit_mode_for_menu(std::optional<int> selected_chart_key_count,
                                                            std::string_view runtime_key_mode);

[[nodiscard]] inline render::NoteImageAspect render_note_image_aspect(std::string_view token) {
    const std::string normalized = config::normalize_skin_note_image_aspect_token(token);
    if (normalized == "width") {
        return render::NoteImageAspect::Width;
    }
    if (normalized == "contain") {
        return render::NoteImageAspect::Contain;
    }
    return render::NoteImageAspect::Stretch;
}

class MenuApp {
public:
    MenuApp();
    ~MenuApp();

    MenuApp(const MenuApp&) = delete;
    MenuApp& operator=(const MenuApp&) = delete;

    [[nodiscard]] bool initialize(const CommandLineOptions& options);
    void run();
    void shutdown();
    [[nodiscard]] int exit_code() const { return exit_code_; }

private:
    static constexpr int kKeymapButtonReset = 0;
    static constexpr int kKeymapButtonNkroTest = 1;
    static constexpr int kKeymapButtonBack = 2;

    struct BestResultRecord;
    struct LocalPlayRecord;
    struct ReplaySummary;

    enum class Screen {
        QuickSetup,
        Title,
        OptionsHub,
        Multiplayer,
        SongSelect,
        SessionMix,
        SongBrowser,
        Gameplay,
        SettingsAudio,
        SettingsGraphics,
        SettingsSkins,
        SettingsInput,
        SettingsCalibration,
        ModeSelect,
        ModeMods,
        Keymap,
        KeymapConfirm,
        OnnxUpscalerConfirm,
        KeymapTest,
        Result,
    };

    enum class SongSelectFocus {
        SongList,
        LeftNav,
    };

public:
    enum class SongSortMode {
        DifficultyAsc,
        DifficultyDesc,
        TitleAsc,
        TitleDesc,
        ArtistAsc,
        ArtistDesc,
    };

    enum class SongGroupMode {
        None,
        Artist,
        Level,
        Folder,
    };

private:
    enum class SongSelectView {
        Songs,
        Sources,
        Records,
    };

    struct MenuSnapshot {
        render::MenuRenderData render;
    };

    struct GameplayHudState {
        struct Note {
            int lane = 1;
            int64_t start_sample = 0;
            int64_t tail_sample = 0;
            bool hold = false;
            bool head_visible = true;
            bool pending = false;
        bool mine = false;
        double visual_position = 0.0;
        double tail_visual_position = 0.0;
        };

        bool active = false;
        bool finished = false;
        bool game_over = false;
        bool spectating_peer = false;
        bool user_aborted = false;
        bool paused = false;
        int pause_menu_cursor = 0;
        bool loading = false;
        bool countdown_active = false;
        int countdown_value = 0;
        int loading_percent = 0;
        std::string loading_stage;

        int lane_count = 10;
        int64_t current_sample = 0;
        int64_t duration_samples = 0;
        int sample_rate = 48000;
        int64_t audio_sample_time_ns = 0;
        int64_t hud_publish_time_ns = 0;
        uint32_t audio_buffer_frames = 0;
        int64_t lookahead_samples = 0;
        int64_t past_samples = 0;
        double current_visual_position = 0.0;
        double visual_velocity = 1.0;
        double future_visual_span = 1.0;
        double past_visual_span = 1.0;
        std::string background_base_path;
        std::string background_overlay_path;
        int64_t background_base_start_sample = 0;
        int64_t background_overlay_start_sample = 0;

        int combo = 0;
        int max_combo = 0;
        gameplay::JudgementCounts counts;
        int64_t score = 0;
        double accuracy = 0.0;
        double detailed_accuracy = 0.0;
        bool osu_od8_score_available = false;
        int64_t osu_od8_score = 0;

        double gauge = 0.0;
        game::GaugeType gauge_type = game::GaugeType::Normal;

        double rate = 1.0;
        double hispeed = 3.0;
        // Mirrors of the in-play tuning keys. Only read while `active`, so the
        // menu keeps rendering the stored config outside a song.
        double judgement_line_position = config::kJudgementLinePositionDefault;
        double visual_offset_ms = 0.0;

        bool has_feedback = false;
        game::Judgement feedback = game::Judgement::BD;
        double feedback_delta_ms = 0.0;
        uint64_t peer_revision = 0;
        std::size_t timing_history_count = 0;
        std::array<double, kGameplayTimingHistoryMaxEntries> timing_history_delta_ms{};
        uint64_t motion_revision = 0;
        uint64_t text_revision = 0;

        std::size_t lane_activity_count = 0;
        std::array<float, kGameplayHudMaxLanes> lane_activity{};
        std::size_t lane_pressed_count = 0;
        std::array<uint8_t, kGameplayHudMaxLanes> lane_pressed{};
        std::size_t note_count = 0;
        std::array<Note, kGameplayHudMaxNotes> notes{};

        bool ghost_visible = false;
        int64_t ghost_score = 0;
        double ghost_accuracy = 0.0;
        double ghost_detailed_accuracy = 0.0;
        bool ghost_osu_od8_score_available = false;
        int64_t ghost_osu_od8_score = 0;
        int ghost_combo = 0;
        int ghost_max_combo = 0;
        gameplay::JudgementCounts ghost_counts;
        double ghost_gauge = 0.0;
        game::GaugeType ghost_gauge_type = game::GaugeType::Normal;
        bool ghost_has_feedback = false;
        game::Judgement ghost_feedback = game::Judgement::BD;
        double ghost_feedback_delta_ms = 0.0;
        std::size_t ghost_timing_history_count = 0;
        std::array<double, kGameplayTimingHistoryMaxEntries> ghost_timing_history_delta_ms{};
        bool ghost_finished = false;
        bool ghost_game_over = false;
        std::size_t ghost_lane_activity_count = 0;
        std::array<float, kGameplayHudMaxLanes> ghost_lane_activity{};
        std::size_t ghost_lane_pressed_count = 0;
        std::array<uint8_t, kGameplayHudMaxLanes> ghost_lane_pressed{};
        std::size_t ghost_note_count = 0;
        std::array<Note, kGameplayHudMaxNotes> ghost_notes{};
    };

    void start_menu_threads();
    void stop_menu_threads();
    void restart_input_thread(bool retry_configured_backend = false);
    void restart_audio_thread();
    void restart_render_thread();
    void apply_runtime_graphics_config();
    [[nodiscard]] int effective_refresh_hz() const;
    [[nodiscard]] int effective_present_refresh_hz() const;
    [[nodiscard]] int effective_render_fps_limit() const;
    [[nodiscard]] render::RenderConfig current_render_config() const;
    [[nodiscard]] render::MenuWindowConfig current_window_config() const;
    [[nodiscard]] static GameplayHudRevisionInput gameplay_hud_revision_input(const GameplayHudState& state);
    static void advance_gameplay_hud_revisions(GameplayHudState& state, bool motion_changed, bool text_changed);
    static void reset_gameplay_hud_state(GameplayHudState& state, bool preserve_loading = false);

    void handle_input_event(const input::InputEvent& event);
    void handle_menu_click(const render::MenuClickEvent& event);
    void handle_text_input(std::string_view text);
    void handle_quick_setup_input(uint32_t keycode);
    void handle_title_input(uint32_t keycode);
    void handle_options_hub_input(uint32_t keycode);
    void handle_multiplayer_input(uint32_t keycode);
    void handle_song_select_input(uint32_t keycode);
    void handle_session_mix_input(uint32_t keycode);
    void handle_song_browser_input(uint32_t keycode);
    void handle_audio_settings_input(uint32_t keycode);
    void handle_graphics_settings_input(uint32_t keycode);
    void handle_skins_settings_input(uint32_t keycode);
    void handle_input_settings_input(uint32_t keycode);
    void handle_calibration_settings_input(uint32_t keycode);
    void handle_mode_settings_input(uint32_t keycode);
    void handle_mode_mods_input(uint32_t keycode);
    void handle_keymap_input(uint32_t keycode);
    void handle_keymap_confirm_input(uint32_t keycode);
    void handle_onnx_upscaler_confirm_input(uint32_t keycode);
    void handle_keymap_test_input(uint32_t keycode);
    void handle_result_input(uint32_t keycode);
    [[nodiscard]] bool result_presentation_ready() const;

    void publish_snapshot();
    [[nodiscard]] std::string current_track_label() const;
    void populate_quick_setup_render_data(render::MenuRenderData& render);
    void populate_multiplayer_render_data(render::MenuRenderData& render);
    void populate_title_render_data(render::MenuRenderData& render,
                                    const std::string& current_track,
                                    const BestResultRecord& current_best);
    void populate_song_select_render_data(render::MenuRenderData& render,
                                          const std::string& current_track,
                                          const BestResultRecord& current_best,
                                          const LocalPlayRecord* selected_record);
    void populate_song_browser_render_data(render::MenuRenderData& render);
    void populate_result_render_data(render::MenuRenderData& render, const std::string& current_track);
    void populate_audio_settings_render_data(render::MenuRenderData& render);
    void populate_graphics_settings_render_data(render::MenuRenderData& render);
    void populate_input_settings_render_data(render::MenuRenderData& render);
    void populate_calibration_settings_render_data(render::MenuRenderData& render);
    void populate_mode_settings_render_data(render::MenuRenderData& render);
    void populate_mode_mods_render_data(render::MenuRenderData& render);
    void populate_skin_settings_render_data(render::MenuRenderData& render);
    void populate_keymap_render_data(render::MenuRenderData& render);
    void populate_keymap_confirm_render_data(render::MenuRenderData& render);
    void populate_onnx_upscaler_confirm_render_data(render::MenuRenderData& render);
    void populate_keymap_test_render_data(render::MenuRenderData& render);
    void populate_generic_screen_render_data(render::MenuRenderData& render);
    void render_tick();
    void render_snapshot(const MenuSnapshot& snapshot);
    void update_keymap_capture_timeout();
    void update_pressed_keys(const input::InputEvent& event);
    void service_input_backend_health();
    void reset_input_backend_probe();
    void update_song_select_repeat();
    void reset_song_select_repeat();
    [[nodiscard]] bool is_song_select_repeat_key(uint32_t keycode) const;
    [[nodiscard]] std::vector<uint32_t> current_menu_probe_keycodes() const;
    void refresh_menu_input_polling_scope();
    void rebuild_pressed_keys_from_polling_snapshot();
    [[nodiscard]] bool fallback_menu_input_to_polling(std::string_view reason);
    void remember_input_backend_fallback(const InputBackendRuntimeState& state);
    void note_runtime_input_event_source(const input::InputEvent& event);
    [[nodiscard]] std::string current_input_backend_status_label() const;
    [[nodiscard]] std::string current_input_backend_status_detail() const;

    void launch_gameplay(const std::string& chart_path,
                         const std::string& replay_path = {},
                         GameplayLaunchKind launch_kind = GameplayLaunchKind::SinglePlayer);
    void launch_selected_song();
    void start_session_mix();
    bool add_selected_song_to_session_mix_draft();
    void remove_last_session_mix_draft_song();
    bool save_session_mix_draft(std::string_view path);
    bool load_session_mix_lr2_course_file(std::string_view path, bool remember_path = true);
    [[nodiscard]] const Lr2CourseDefinition* selected_session_mix_lr2_course() const;
    void launch_current_session_mix_song();
    void advance_session_mix_from_result();
    void stop_session_mix(bool completed);
    void record_current_session_mix_result();
    [[nodiscard]] std::string session_mix_phase_label(SessionMixPhase phase) const;
    void select_multiplayer_chart();
    void service_multiplayer();
    void reset_multiplayer_for_single_player();
    void leave_multiplayer();
    void open_multiplayer_options();
    void reset_multiplayer_chart_match_search();
    void service_multiplayer_chart_match(const network::PeerSessionSnapshot& peer);
    [[nodiscard]] bool coordinate_multiplayer_start();
    [[nodiscard]] bool wait_for_multiplayer_result();
    [[nodiscard]] bool launch_replay_from_path(const std::string& replay_path,
                                               const std::string& fallback_chart_path = {});
    [[nodiscard]] bool launch_last_result_replay();
    [[nodiscard]] bool launch_selected_record_replay();
    void start_keymap_capture();
    void apply_keymap_capture(uint32_t keycode);
    void apply_keymap_reset();
    void apply_keymap_save();
    void clear_keymap_pending_state();
    void apply_song_sort(SongSortMode mode);
    bool remember_song_source(const std::string& source_path);
    void persist_runtime_config();
    void refresh_song_source(bool force_reindex);
    void switch_song_source(const std::string& new_songs_path, bool force_reindex);
    void exit_keymap_screen();
    void reload_chart_best_results();
    void sort_song_list_preserving_selection();
    void rebuild_visible_song_list(const std::string* selected_path = nullptr);
    void rebuild_current_song_record_indices();
    void refresh_song_collection_membership_cache();
    void sync_song_select_state();
    void populate_gameplay_render_data(render::GameplayHudData& target,
                                       uint64_t* out_motion_revision = nullptr,
                                       uint64_t* out_text_revision = nullptr);
    void update_gameplay_loading_state(int percent, std::string_view stage);
    void refresh_keymap_lane_list();
    void refresh_available_lr2_skins();
    void refresh_available_tenriff_skins();
    [[nodiscard]] bool import_lr2_skin_path(std::string_view source_path);
    [[nodiscard]] bool import_tenriff_skin_path(std::string_view source_path);
    [[nodiscard]] bool import_skin_path_auto(std::string_view source_path);
    [[nodiscard]] std::string active_external_skin_root() const;
    [[nodiscard]] std::string active_external_skin_name() const;
    [[nodiscard]] const struct LocalPlayRecord* current_selected_record() const;
    [[nodiscard]] bool open_result_record(const std::string& result_path,
                                          const std::string& replay_path);
    [[nodiscard]] bool open_selected_record_result();
    [[nodiscard]] bool open_current_song_best_result();
    [[nodiscard]] const ReplaySummary* replay_summary_for_path(const std::string& path);
    [[nodiscard]] std::string best_replay_path_for_selected_song() const;
    [[nodiscard]] bool move_song_select_selection(int delta);
    [[nodiscard]] bool handle_settings_shortcut(uint32_t keycode, Screen return_screen);
    [[nodiscard]] std::string song_absolute_path(const SongEntry& entry) const;
    void update_last_chart_metadata(const std::string& chart_path,
                                    const SongEntry* preferred_entry = nullptr);
    [[nodiscard]] std::string profile_display_name() const;
    [[nodiscard]] std::string song_background_preview_path_for_entry(const SongEntry& entry);
    [[nodiscard]] std::string selected_song_absolute_path() const;
    [[nodiscard]] std::string selected_song_storage_key() const;
    [[nodiscard]] bool selected_song_is_favorite() const;
    [[nodiscard]] bool selected_song_is_in_collection(std::string_view name) const;
    [[nodiscard]] bool song_entry_matches_collection_filter(const SongEntry& entry) const;
    [[nodiscard]] BestResultRecord best_result_for_song_entry(const SongEntry& entry) const;
    [[nodiscard]] std::string current_named_song_collection() const;
    [[nodiscard]] std::string song_collection_filter_label() const;
    void cycle_song_collection_filter(int direction);
    void create_next_song_collection();
    [[nodiscard]] bool toggle_selected_song_favorite();
    [[nodiscard]] bool toggle_selected_song_in_collection(std::string_view name);
    [[nodiscard]] std::string selected_song_background_preview_path();
    void sync_menu_music();
    void service_song_preview();
    void open_keymap_screen(Screen return_screen);
    void clear_keymap_status_message();
    void show_keymap_status_message(std::string message);
    void populate_help_overlay(render::HelpOverlayData& target) const;

    [[nodiscard]] bool ui_uses_korean() const;
    [[nodiscard]] std::string ui_text(std::string_view english, std::string_view korean) const;
    [[nodiscard]] std::string ui_on_off(bool enabled) const;
    [[nodiscard]] std::string ui_language_label(std::string_view token) const;
    [[nodiscard]] std::string ui_display_mode_label(std::string_view token) const;
    [[nodiscard]] std::string ui_resolution_label(std::string_view token) const;
    [[nodiscard]] std::string ui_preset_label(std::string_view token) const;
    [[nodiscard]] std::string ui_keysound_policy_label(std::string_view token) const;
    [[nodiscard]] std::string ui_song_index_profile_label(std::string_view token) const;
    [[nodiscard]] std::string ui_key_mode_label(std::string_view token) const;
    [[nodiscard]] std::string ui_key_conversion_algorithm_label(std::string_view token) const;
    [[nodiscard]] std::string ui_key_conversion_nk2_preset_label(std::string_view token) const;
    [[nodiscard]] std::string ui_gauge_label(std::string_view token) const;
    [[nodiscard]] std::string ui_random_label(std::string_view token) const;
    [[nodiscard]] std::string ui_skin_source_label(std::string_view token) const;
    [[nodiscard]] std::string ui_skin_note_shape_label(std::string_view token) const;
    [[nodiscard]] std::string ui_skin_note_image_aspect_label(std::string_view token) const;
    void apply_difficulty_table_url(std::string_view url);

    [[nodiscard]] std::string screen_title() const;
    [[nodiscard]] const SongEntry* visible_song_entry(std::size_t visible_index) const;
    [[nodiscard]] std::size_t visible_song_count() const { return visible_song_indices_.size(); }
    [[nodiscard]] std::string selected_song_path() const;
    struct BestResultRecord {
        bool has_value = false;
        std::string rank = "--";
        int64_t best_score = 0;
        int64_t detail_score = 0;
        int total_notes = 0;
        double accuracy = 0.0;
        double detailed_accuracy = 0.0;
        std::string clear_status = "FAILED";
        std::string final_gauge = "normal";
        bool game_over = true;
        int max_combo = 0;
        int perfect = 0;
        int great = 0;
        int good = 0;
        int bad = 0;
        int poor = 0;
        std::string created_utc;
        std::string result_path;
        std::string replay_path;
    };

    struct LocalPlayRecord {
        std::string chart_path;
        std::string chart_format;
        std::string created_utc;
        std::string player_name;
        std::string result_path;
        std::string replay_path;
        std::string rank = "--";
        std::string clear_status = "FAILED";
        std::string final_gauge = "normal";
        bool game_over = true;
        std::vector<std::string> mods;
        double rate_multiplier = 1.0;
        double score_multiplier = 1.0;
        bool pause_used = false;
        bool autoplay_enabled = false;
        bool practice_no_fail_enabled = false;
        int64_t raw_score = 0;
        int64_t score = 0;
        int64_t detail_score = 0;
        double accuracy = 0.0;
        double detailed_accuracy = 0.0;
        int max_combo = 0;
        int total_notes = 0;
        int judged_notes = 0;
        int perfect = 0;
        int great = 0;
        int good = 0;
        int bad = 0;
        int poor = 0;
        double mean_delta_ms = 0.0;
        double stddev_delta_ms = 0.0;
    };

    struct ReplaySummary {
        bool loaded = false;
        bool exists = false;
        int sample_rate = 0;
        int lane_count = 0;
        int event_count = 0;
        int64_t duration_samples = 0;
        double rate = 1.0;
        double input_offset_ms = 0.0;
        std::vector<std::string> mods;
        double rate_multiplier = 1.0;
        double score_multiplier = 1.0;
        bool pause_used = false;
        int64_t final_score = 0;
        std::string error;
    };

    [[nodiscard]] BestResultRecord current_song_best_result() const;

    [[nodiscard]] std::string format_song_line(std::size_t index) const;
    void update_song_list(SongIndex index);

    CommandLineOptions options_{};
    config::RuntimeConfig config_{};
    config::Keymap keymap_{};
    config::Keymap working_keymap_{};

    std::string profile_dir_;
    std::string songs_path_;
    std::string cache_path_;
    std::string last_chart_path_;
    std::string last_chart_title_;
    std::string last_chart_artist_;
    SongEntry last_chart_entry_{};
    bool last_chart_entry_valid_ = false;
    std::string last_result_player_name_;
    std::string last_replay_path_;
    std::string last_result_path_;
    std::string last_clear_status_;
    std::string last_final_gauge_ = "normal";
    std::vector<std::string> last_export_warnings_;
    std::vector<std::string> last_result_mods_{};
    double last_result_rate_multiplier_ = 1.0;
    double last_result_score_multiplier_ = 1.0;
    int64_t last_result_final_score_ = 0;
    bool last_pause_used_ = false;
    bool last_session_replay_playback_ = false;
    double last_chart_bpm_ = 0.0;
    GameplayHudState gameplay_hud_{};

    network::PeerSession peer_session_{};
    MultiplayerMenuState multiplayer_menu_{};
    std::string multiplayer_chart_path_{};
    std::string multiplayer_chart_title_{};
    network::ChartFingerprint multiplayer_chart_fingerprint_{};
    std::string multiplayer_status_message_{};
    uint64_t multiplayer_last_revision_ = 0;
    uint64_t multiplayer_local_library_index_revision_ = 0;
    uint64_t multiplayer_remote_library_revision_ = 0;
    bool multiplayer_remote_library_ready_ = false;
    std::size_t multiplayer_common_chart_count_ = 0;
    network::ChartFingerprint multiplayer_chart_match_target_{};
    uint64_t multiplayer_chart_match_index_revision_ = 0;
    std::vector<std::string> multiplayer_chart_match_source_inputs_{};
    std::vector<std::string> multiplayer_chart_match_sources_{};
    std::size_t multiplayer_chart_match_source_cursor_ = 0;
    std::vector<MultiplayerChartSearchCandidate> multiplayer_chart_match_candidates_{};
    std::size_t multiplayer_chart_match_cursor_ = 0;
    bool multiplayer_chart_match_active_ = false;
    bool multiplayer_selecting_chart_ = false;
    std::atomic<bool> multiplayer_match_active_{false};
    bool last_game_was_multiplayer_ = false;
    bool multiplayer_waiting_for_result_exit_ = false;

    SongIndexerThread song_indexer_{};
    std::vector<SongEntry> indexed_songs_{};
    uint64_t song_index_revision_ = 0;
    std::vector<std::size_t> visible_song_indices_{};
    std::unordered_map<std::string, BestResultRecord> chart_best_results_{};
    std::unordered_set<std::string> favorite_song_keys_{};
    std::unordered_map<std::string, std::unordered_set<std::string>> song_collection_membership_{};
    int indexed_favorite_count_ = 0;

    int session_mix_minutes_ = 30;
    int session_mix_source_index_ = 0;
    std::vector<SessionMixDraftEntry> session_mix_draft_{};
    std::vector<Lr2CourseDefinition> session_mix_lr2_courses_{};
    std::string session_mix_active_course_title_{};
    SessionMixPlan session_mix_plan_{};
    std::size_t session_mix_cursor_ = 0;
    int session_mix_completed_ = 0;
    int session_mix_cleared_ = 0;
    int64_t session_mix_total_score_ = 0;
    double session_mix_gauge_value_ = 100.0;
    bool session_mix_active_ = false;
    bool session_mix_current_result_recorded_ = false;
    std::string session_mix_status_message_{};

    Screen screen_ = Screen::Title;
    Screen submenu_return_screen_ = Screen::Title;
    int title_cursor_ = 0;
    int selected_song_ = 0;
    int selected_source_ = 0;
    int selected_record_ = 0;
    int settings_cursor_ = 0;
    int options_cursor_ = 0;
    int calibration_step_ms_ = 1;
    SongSelectFocus song_select_focus_ = SongSelectFocus::SongList;
    SongSortMode song_sort_mode_ = SongSortMode::DifficultyAsc;
    SongGroupMode song_group_mode_ = SongGroupMode::None;
    SongSelectView song_select_view_ = SongSelectView::Songs;
    int song_select_nav_cursor_ = 0;
    int keymap_cursor_ = 0;
    int onnx_upscaler_confirm_cursor_ = 1;
    int skin_edit_lane_ = 0;
    int skin_edit_gap_ = 0;
    bool keymap_dirty_ = false;
    bool keymap_capture_active_ = false;
    int64_t keymap_capture_deadline_ns_ = 0;
    std::string keymap_edit_mode_ = "10k";
    std::string skin_edit_mode_ = "10k";
    std::string keymap_pending_lane_;
    std::string keymap_pending_key_;
    std::string keymap_duplicate_lane_;
    std::string keymap_status_message_;
    int64_t keymap_status_deadline_ns_ = 0;
    bool first_run_profile_ = false;
    bool profile_nickname_edit_active_ = false;
    std::string profile_nickname_before_edit_;
    bool help_overlay_visible_ = false;
    std::string available_lr2_skin_root_;
    std::vector<std::string> available_lr2_skin_names_{};
    std::unordered_map<std::string, std::string> available_lr2_skin_roots_by_name_{};
    std::string available_tenriff_skin_root_;
    std::vector<std::string> available_tenriff_skin_names_{};
    TenRiffSkinDefinition active_tenriff_skin_{};

    bool has_result_ = false;
    bool last_game_over_ = false;
    gameplay::ResultStats last_result_{};
    int64_t result_presentation_start_ns_ = 0;
    bool result_presentation_skipped_ = false;

    bool input_dirty_ = false;
    bool input_backend_dirty_ = false;
    bool audio_dirty_ = false;
    bool graphics_dirty_ = false;
    bool skin_dirty_ = false;
    bool mode_dirty_ = false;
    bool mode_library_dirty_ = false;
    int64_t last_indexer_snapshot_ns_ = 0;
    int64_t last_song_select_slow_snapshot_log_ns_ = 0;

    std::atomic<bool> exit_requested_{false};
    int exit_code_ = 0;

    input::InputThread input_thread_{};
    audio::AudioThread audio_thread_{};
    MenuMusicController menu_music_{};
    std::string menu_music_scene_key_{};
    std::string menu_music_scene_path_{};
    std::unordered_map<std::string, std::size_t> menu_music_variant_cursors_{};
    std::string song_preview_selection_key_{};
    std::string song_preview_active_path_{};
    int64_t song_preview_due_ns_ = 0;
    bool song_preview_pending_ = false;
    render::RenderThread render_thread_{};
    render::MenuWindow menu_window_{};

    std::mutex snapshot_mutex_{};
    std::mutex gameplay_hud_mutex_{};
    MenuSnapshot snapshot_{};
    uint64_t snapshot_version_ = 0;
    uint64_t rendered_snapshot_version_ = 0;
    uint64_t rendered_gameplay_motion_version_ = 0;
    uint64_t rendered_gameplay_text_version_ = 0;
    bool gameplay_present_performance_active_ = false;
    bool render_cache_ready_ = false;
    render::MenuRenderData render_cache_{};

    std::unordered_set<uint32_t> pressed_keys_{};
    std::vector<std::string> keymap_lanes_{};
    std::unordered_map<std::string, int> source_song_counts_{};
    std::vector<LocalPlayRecord> local_play_records_{};
    std::unordered_map<std::string, std::vector<std::size_t>> chart_play_record_indices_{};
    std::vector<std::size_t> current_song_record_indices_{};
    std::unordered_map<std::string, ReplaySummary> replay_summary_cache_{};

    std::string song_search_query_{};
    bool song_select_search_active_ = false;
    // Typed difficulty-table URL. Editing keeps its own buffer so cancelling
    // leaves the table that is already loaded alone.
    bool difficulty_table_url_editing_ = false;
    std::string difficulty_table_url_input_{};
    int song_key_filter_ = 0;
    int song_level_min_filter_ = 0;
    int song_level_max_filter_ = 0;
    uint32_t song_select_repeat_key_ = 0;
    int64_t song_select_repeat_next_ns_ = 0;

    uint32_t key_up_ = 0;
    uint32_t key_down_ = 0;
    uint32_t key_left_ = 0;
    uint32_t key_right_ = 0;
    uint32_t key_page_up_ = 0;
    uint32_t key_page_down_ = 0;
    uint32_t key_enter_ = 0;
    uint32_t key_space_ = 0;
    uint32_t key_escape_ = 0;
    uint32_t key_backspace_ = 0;
    uint32_t key_delete_ = 0;
    uint32_t key_c_ = 0;
    uint32_t key_a_ = 0;
    uint32_t key_g_ = 0;
    uint32_t key_i_ = 0;
    uint32_t key_m_ = 0;
    uint32_t key_k_ = 0;
    uint32_t key_r_ = 0;
    uint32_t key_f1_ = 0;
    uint32_t key_f2_ = 0;
    uint32_t key_f5_ = 0;
    uint32_t key_f9_ = 0;
    uint32_t key_minus_ = 0;
    uint32_t key_plus_ = 0;
    input::RawInputHealthProbe input_backend_probe_{};
    std::unordered_map<uint32_t, bool> input_probe_polled_states_{};
    InputBackendFallbackPolicy input_backend_fallback_policy_{};
    InputBackendRuntimeState input_backend_state_{};
    InputBackendRuntimeState last_gameplay_input_backend_state_{};
};

}  // namespace tenriff::app
