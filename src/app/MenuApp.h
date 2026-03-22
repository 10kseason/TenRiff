#pragma once

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
#include "app/MenuMusicController.h"
#include "app/SongSelectState.h"
#include "GameplayHudLimits.h"
#include "app/SongIndex.h"
#include "app/SongIndexerThread.h"
#include "audio/AudioThread.h"
#include "config/Config.h"
#include "config/Keymap.h"
#include "gameplay/ResultStats.h"
#include "input/InputThread.h"
#include "render/MenuWindow.h"
#include "render/RenderThread.h"

namespace tenriff::app {

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
    struct BestResultRecord;
    struct LocalPlayRecord;
    struct ReplaySummary;

    enum class Screen {
        QuickSetup,
        Title,
        OptionsHub,
        EditStub,
        SongSelect,
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
        };

        bool active = false;
        bool finished = false;
        bool game_over = false;
        bool user_aborted = false;
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

        int combo = 0;
        int max_combo = 0;
        gameplay::JudgementCounts counts;
        int64_t score = 0;

        double gauge = 0.0;
        game::GaugeType gauge_type = game::GaugeType::Normal;

        double rate = 1.0;
        double hispeed = 3.0;

        bool has_feedback = false;
        game::Judgement feedback = game::Judgement::BD;
        double feedback_delta_ms = 0.0;
        uint64_t motion_revision = 0;
        uint64_t text_revision = 0;

        std::size_t lane_activity_count = 0;
        std::array<float, kGameplayHudMaxLanes> lane_activity{};
        std::size_t note_count = 0;
        std::array<Note, kGameplayHudMaxNotes> notes{};

        bool ghost_visible = false;
        int64_t ghost_score = 0;
        int ghost_combo = 0;
        int ghost_max_combo = 0;
        gameplay::JudgementCounts ghost_counts;
        double ghost_gauge = 0.0;
        game::GaugeType ghost_gauge_type = game::GaugeType::Normal;
        bool ghost_has_feedback = false;
        game::Judgement ghost_feedback = game::Judgement::BD;
        double ghost_feedback_delta_ms = 0.0;
        bool ghost_finished = false;
        bool ghost_game_over = false;
        std::size_t ghost_lane_activity_count = 0;
        std::array<float, kGameplayHudMaxLanes> ghost_lane_activity{};
        std::size_t ghost_note_count = 0;
        std::array<Note, kGameplayHudMaxNotes> ghost_notes{};
    };

    void start_menu_threads();
    void stop_menu_threads();
    void restart_input_thread();
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
    void handle_quick_setup_input(uint32_t keycode);
    void handle_title_input(uint32_t keycode);
    void handle_options_hub_input(uint32_t keycode);
    void handle_edit_stub_input(uint32_t keycode);
    void handle_song_select_input(uint32_t keycode);
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
    void handle_keymap_test_input(uint32_t keycode);
    void handle_result_input(uint32_t keycode);

    void publish_snapshot();
    [[nodiscard]] std::string current_track_label() const;
    void populate_quick_setup_render_data(render::MenuRenderData& render);
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
    void populate_keymap_test_render_data(render::MenuRenderData& render);
    void populate_generic_screen_render_data(render::MenuRenderData& render);
    void render_tick();
    void render_snapshot(const MenuSnapshot& snapshot);
    void update_keymap_capture_timeout();
    void update_pressed_keys(const input::InputEvent& event);
    void update_song_select_repeat();
    void reset_song_select_repeat();
    [[nodiscard]] bool is_song_select_repeat_key(uint32_t keycode) const;

    void launch_gameplay(const std::string& chart_path, const std::string& replay_path = {});
    void launch_selected_song();
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
    void refresh_available_osu_skins();
    void refresh_available_lr2_skins();
    [[nodiscard]] bool import_osu_skin_path(std::string_view source_path);
    [[nodiscard]] bool import_lr2_skin_path(std::string_view source_path);
    [[nodiscard]] bool import_skin_path_auto(std::string_view source_path);
    [[nodiscard]] std::string active_external_skin_root() const;
    [[nodiscard]] std::string active_external_skin_name() const;
    [[nodiscard]] const struct LocalPlayRecord* current_selected_record() const;
    [[nodiscard]] bool open_selected_record_result();
    [[nodiscard]] const ReplaySummary* replay_summary_for_path(const std::string& path);
    [[nodiscard]] std::string best_replay_path_for_selected_song() const;
    [[nodiscard]] bool move_song_select_selection(int delta);
    [[nodiscard]] bool handle_settings_shortcut(uint32_t keycode, Screen return_screen);
    [[nodiscard]] std::string song_absolute_path(const SongEntry& entry) const;
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
    [[nodiscard]] std::string ui_chart_filter_label(std::string_view token) const;
    [[nodiscard]] std::string ui_key_mode_label(std::string_view token) const;
    [[nodiscard]] std::string ui_gauge_label(std::string_view token) const;
    [[nodiscard]] std::string ui_random_label(std::string_view token) const;
    [[nodiscard]] std::string ui_skin_source_label(std::string_view token) const;
    [[nodiscard]] std::string ui_skin_note_shape_label(std::string_view token) const;

    [[nodiscard]] std::string screen_title() const;
    [[nodiscard]] const SongEntry* visible_song_entry(std::size_t visible_index) const;
    [[nodiscard]] std::size_t visible_song_count() const { return visible_song_indices_.size(); }
    [[nodiscard]] std::string selected_song_path() const;
    struct BestResultRecord {
        bool has_value = false;
        std::string rank = "--";
        int64_t best_score = 0;
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
    };

    struct LocalPlayRecord {
        std::string chart_path;
        std::string chart_format;
        std::string created_utc;
        std::string result_path;
        std::string replay_path;
        std::string rank = "--";
        std::string clear_status = "FAILED";
        std::string final_gauge = "normal";
        bool game_over = true;
        std::vector<std::string> mods;
        double rate_multiplier = 1.0;
        double score_multiplier = 1.0;
        int64_t raw_score = 0;
        int64_t score = 0;
        double accuracy = 0.0;
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
    std::string last_replay_path_;
    std::string last_result_path_;
    std::string last_clear_status_;
    std::vector<std::string> last_export_warnings_;
    std::vector<std::string> last_result_mods_{};
    double last_result_rate_multiplier_ = 1.0;
    double last_result_score_multiplier_ = 1.0;
    int64_t last_result_final_score_ = 0;
    bool last_session_replay_playback_ = false;
    double last_chart_bpm_ = 0.0;
    GameplayHudState gameplay_hud_{};

    SongIndexerThread song_indexer_{};
    std::vector<SongEntry> indexed_songs_{};
    std::vector<std::size_t> visible_song_indices_{};
    std::unordered_map<std::string, BestResultRecord> chart_best_results_{};
    std::unordered_set<std::string> favorite_song_keys_{};
    std::unordered_map<std::string, std::unordered_set<std::string>> song_collection_membership_{};
    int indexed_favorite_count_ = 0;

    Screen screen_ = Screen::Title;
    Screen submenu_return_screen_ = Screen::Title;
    int title_cursor_ = 0;
    int selected_song_ = 0;
    int selected_source_ = 0;
    int selected_record_ = 0;
    int settings_cursor_ = 0;
    int options_cursor_ = 0;
    int calibration_step_ms_ = 5;
    SongSelectFocus song_select_focus_ = SongSelectFocus::SongList;
    SongSortMode song_sort_mode_ = SongSortMode::DifficultyAsc;
    SongSelectView song_select_view_ = SongSelectView::Songs;
    int song_select_nav_cursor_ = 0;
    int keymap_cursor_ = 0;
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
    bool help_overlay_visible_ = false;
    std::string available_osu_skin_root_;
    std::vector<std::string> available_osu_skin_names_{};
    std::unordered_map<std::string, std::string> available_osu_skin_roots_by_name_{};
    std::string available_lr2_skin_root_;
    std::vector<std::string> available_lr2_skin_names_{};
    std::unordered_map<std::string, std::string> available_lr2_skin_roots_by_name_{};

    bool has_result_ = false;
    bool last_game_over_ = false;
    gameplay::ResultStats last_result_{};

    bool input_dirty_ = false;
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
    render::RenderThread render_thread_{};
    render::MenuWindow menu_window_{};

    std::mutex snapshot_mutex_{};
    std::mutex gameplay_hud_mutex_{};
    MenuSnapshot snapshot_{};
    uint64_t snapshot_version_ = 0;
    uint64_t rendered_snapshot_version_ = 0;
    uint64_t rendered_gameplay_motion_version_ = 0;
    uint64_t rendered_gameplay_text_version_ = 0;
    uint64_t gameplay_performance_last_motion_revision_ = 0;
    bool gameplay_performance_active_ = false;
    bool render_cache_ready_ = false;
    render::MenuRenderData render_cache_{};
    render::PerformanceTracker gameplay_performance_tracker_{};

    std::unordered_set<uint32_t> pressed_keys_{};
    std::vector<std::string> keymap_lanes_{};
    std::unordered_map<std::string, int> source_song_counts_{};
    std::vector<LocalPlayRecord> local_play_records_{};
    std::unordered_map<std::string, std::vector<std::size_t>> chart_play_record_indices_{};
    std::vector<std::size_t> current_song_record_indices_{};
    std::unordered_map<std::string, ReplaySummary> replay_summary_cache_{};

    std::string song_search_query_{};
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
    uint32_t key_escape_ = 0;
    uint32_t key_backspace_ = 0;
    uint32_t key_delete_ = 0;
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
};

}  // namespace tenriff::app
