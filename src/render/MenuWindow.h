#pragma once

#ifdef _WIN32

#include "GameplayHudLimits.h"
#include "render/RenderThread.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

struct ID2D1Bitmap;

namespace tenriff::render {

struct MenuWindowConfig {
    std::string title = "TenRiff";
    std::string display_mode = "borderless";
    bool vsync = false;
    int refresh_hz = 300;
    int width = 1280;
    int height = 720;
};

enum class MenuScreenKind {
    GenericList,
    TitleMenu,
    SongSelect,
    ResultScreen,
    GameplayHud,
};

enum class MenuHitTargetKind {
    None,
    MouseWheel,
    FileDrop,
    SongScrollbar,
    TitleButton,
    SongNavButton,
    SongCard,
    OptionsItem,
    SettingsRow,
    KeymapButton,
};

enum class MenuHitPart {
    Activate,
    Increment,
    Decrement,
};

struct MenuClickEvent {
    MenuHitTargetKind kind = MenuHitTargetKind::None;
    int index = -1;
    MenuHitPart part = MenuHitPart::Activate;
    bool double_click = false;
    int wheel_steps = 0;
    std::string path;
};

struct MenuButtonData {
    std::string label;
    std::string icon;
    bool selected = false;
    std::string detail;
};

struct TitleMenuData {
    std::string profile;
    std::string track;
    int64_t high_score = 0;
    std::vector<MenuButtonData> buttons;
    std::vector<std::string> guides;
};

struct SongCardData {
    std::string group_label;
    std::string title;
    std::string artist;
    std::string detail;
    std::string background_path;
    std::string lamp;
    int level = 0;
    double rating = 0.0;
    int song_index = -1;
    bool selected = false;
    bool favorite = false;
};

struct SongSelectData {
    std::string profile;
    std::string track;
    int64_t high_score = 0;
    int song_count = 0;
    int source_count = 0;
    int record_count = 0;
    bool showing_sources = false;
    bool showing_records = false;
    std::string current_source_name;
    std::string current_source_path;
    std::string index_profile_label;
    std::string selected_source_name;
    std::string selected_source_path;
    int selected_source_song_count = -1;
    bool selected_source_active = false;
    std::string selected_song_title;
    std::string selected_song_artist;
    std::string selected_song_detail;
    std::string selected_song_background_path;
    std::string selected_song_lamp;
    bool selected_song_favorite = false;
    std::string selected_song_collection_filter;
    std::string selected_song_ghost_status;
    std::string group_summary;
    std::string browser_summary;
    std::string sort_summary;
    std::string primary_hint;
    std::string secondary_hint;
    std::string empty_title;
    std::string empty_message;
    int list_total_count = 0;
    int list_visible_count = 0;
    int list_window_start = 0;
    int list_selected_index = -1;

    bool indexing = false;
    int indexing_percent = -1;
    int indexing_processed = 0;
    int indexing_total = -1;
    std::string indexing_stage;
    std::string indexing_eta;

    std::vector<MenuButtonData> left_nav;
    std::vector<SongCardData> songs;

    std::string rank;
    int64_t best_score = 0;
    int max_combo = 0;
    int perfect = 0;
    int great = 0;
    int good = 0;
    int bad = 0;
    int poor = 0;
    double accuracy = 0.0;
    std::string selected_record_created_utc;
    std::string selected_record_status;
    std::string selected_record_replay_file;
    std::string selected_record_replay_detail;
    int selected_record_replay_lane_count = 0;
    int selected_record_replay_event_count = 0;
};

struct ResultGaugePoint {
    float position = 0.0f;
    float value = 0.0f;
};

struct ResultShiftMarker {
    float position = 0.0f;
    std::string label;
};

struct ResultScreenData {
    bool peer_battle = false;
    bool peer_result_available = false;
    std::string profile;
    std::string peer_name;
    std::string peer_status = "WAITING";
    std::string peer_outcome;
    std::string track;
    std::string title;
    std::string artist;

    std::string rank = "--";
    std::string status = "NO DATA";
    std::string gauge_label = "NORMAL";

    int64_t score = 0;
    bool osu_od8_score_available = false;
    int64_t osu_od8_score = 0;
    double accuracy = 0.0;
    double gauge_value = 0.0;
    int max_combo = 0;
    int total_notes = 0;
    int judged_notes = 0;

    int perfect = 0;
    int great = 0;
    int good = 0;
    int bad = 0;
    int poor = 0;

    int64_t peer_score = 0;
    int64_t peer_score_difference = 0;
    double peer_gauge_value = 0.0;
    int peer_max_combo = 0;
    int peer_perfect = 0;
    int peer_great = 0;
    int peer_good = 0;
    int peer_bad = 0;
    int peer_poor = 0;

    double mean_delta_ms = 0.0;
    double stddev_delta_ms = 0.0;
    int shift_count = 0;
    int export_warning_count = 0;
    bool timing_guidance_visible = false;
    int timing_guidance_direction = 0;
    std::string timing_guidance_title;
    std::string timing_guidance_message;
    std::string timing_guidance_detail;

    std::string replay_file;
    bool replay_available = false;
    std::string result_file;
    std::vector<std::string> notes;
    std::vector<ResultGaugePoint> gauge_points;
    std::vector<ResultShiftMarker> gauge_shifts;
};

struct GameplayNoteData {
    int lane = 1;
    int64_t start_sample = 0;
    int64_t tail_sample = 0;
    bool hold = false;
    bool head_visible = true;
};

struct GameplayHudData {
    std::string title;
    std::string artist;
    uint64_t motion_revision = 0;
    uint64_t text_revision = 0;
    bool active = false;
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
    uint32_t audio_buffer_frames = 0;
    int64_t lookahead_samples = 0;
    int64_t past_samples = 0;
    double judgement_line_position = 0.82;
    double combo_position = 0.24;
    std::size_t lane_width_scale_count = 0;
    std::array<double, kGameplayHudMaxLanes> lane_width_scales{};
    double note_width_scale = 1.0;
    std::size_t lane_spacing_scale_count = 0;
    std::array<double, kGameplayHudMaxLanes> lane_spacing_scales{};
    double note_height_scale = 1.8;
    double lane_divider_width_scale = 1.0;
    double lane_center_gap_scale = 0.0;
    double hold_body_width_scale = 0.60;
    bool show_lane_dividers = true;
    bool show_judgement_line = true;
    bool show_gear_boundary_line = false;
    bool hold_tail_taper_enabled = false;
    bool judgement_line_glow_enabled = true;
    bool key_pulse_enabled = true;
    std::string key_label_position = "bottom";
    bool note_border_enabled = true;
    std::string note_shape = "rect";
    bool preserve_note_image_aspect_ratio = false;
    std::string skin_source = "native";
    std::string external_skin_root;
    std::string external_skin_name;
    std::string lr2_resolution_override = "auto";
    double lane_background_opacity = 0.18;
    double visual_opacity = 0.96;
    double note_outline_opacity = 0.78;
    double hold_body_opacity = 0.24;
    double visual_offset_ms = 0.0;

    double bpm = 0.0;
    double rate = 1.0;
    double hispeed = 3.0;
    double scroll_speed = 0.0;

    int64_t score = 0;
    bool osu_od8_score_available = false;
    int64_t osu_od8_score = 0;
    int combo = 0;
    int max_combo = 0;
    int total_notes = 0;
    double accuracy = 0.0;

    int pg = 0;
    int gr = 0;
    int gd = 0;
    int bd = 0;
    int pr = 0;

    double gauge = 0.0;
    std::string gauge_label;

    bool has_feedback = false;
    std::string feedback;
    double feedback_delta_ms = 0.0;
    std::size_t timing_history_count = 0;
    std::array<double, kGameplayTimingHistoryMaxEntries> timing_history_delta_ms{};

    bool finished = false;
    bool game_over = false;
    bool spectating_peer = false;

    bool peer_visible = false;
    bool peer_score_available = false;
    std::string peer_name;
    std::string peer_status;
    int64_t peer_current_sample = 0;
    int64_t peer_score = 0;
    int peer_combo = 0;
    int peer_max_combo = 0;
    int peer_pg = 0;
    int peer_gr = 0;
    int peer_gd = 0;
    int peer_bd = 0;
    int peer_pr = 0;
    double peer_gauge = 0.0;
    bool peer_finished = false;
    bool peer_game_over = false;
    bool peer_aborted = false;
    bool peer_disconnected = false;
    int64_t versus_score_difference = 0;
    double versus_score_position = 0.5;

    std::size_t lane_activity_count = 0;
    std::array<float, kGameplayHudMaxLanes> lane_activity{};
    std::size_t lane_color_count = 0;
    std::array<uint32_t, kGameplayHudMaxLanes> lane_colors{};
    std::size_t key_label_count = 0;
    std::array<std::string, kGameplayHudMaxLanes> key_labels{};
    std::size_t note_count = 0;
    std::array<GameplayNoteData, kGameplayHudMaxNotes> notes{};

    bool ghost_visible = false;
    int64_t ghost_score = 0;
    bool ghost_osu_od8_score_available = false;
    int64_t ghost_osu_od8_score = 0;
    int ghost_combo = 0;
    int ghost_max_combo = 0;
    double ghost_accuracy = 0.0;
    int ghost_pg = 0;
    int ghost_gr = 0;
    int ghost_gd = 0;
    int ghost_bd = 0;
    int ghost_pr = 0;
    double ghost_gauge = 0.0;
    std::string ghost_gauge_label;
    bool ghost_has_feedback = false;
    std::string ghost_feedback;
    double ghost_feedback_delta_ms = 0.0;
    std::size_t ghost_timing_history_count = 0;
    std::array<double, kGameplayTimingHistoryMaxEntries> ghost_timing_history_delta_ms{};
    bool ghost_finished = false;
    bool ghost_game_over = false;
    std::size_t ghost_lane_activity_count = 0;
    std::array<float, kGameplayHudMaxLanes> ghost_lane_activity{};
    std::size_t ghost_note_count = 0;
    std::array<GameplayNoteData, kGameplayHudMaxNotes> ghost_notes{};
};

struct MenuRowData {
    std::string label;
    std::string value;
    bool selected = false;
    bool activatable = false;
    bool adjustable = false;
    bool increment_enabled = false;
    bool decrement_enabled = false;
    MenuHitTargetKind target_kind = MenuHitTargetKind::None;
    int row_index = -1;
};

struct SkinPreviewData {
    bool visible = false;
    std::string mode_label;
    std::string selected_color_label;
    int lane_count = 0;
    int selected_lane = -1;
    int selected_gap = -1;
    double judgement_line_position = 0.82;
    double combo_position = 0.24;
    std::size_t lane_width_scale_count = 0;
    std::array<double, kGameplayHudMaxLanes> lane_width_scales{};
    double note_width_scale = 1.0;
    std::size_t lane_spacing_scale_count = 0;
    std::array<double, kGameplayHudMaxLanes> lane_spacing_scales{};
    double note_height_scale = 1.8;
    double lane_divider_width_scale = 1.0;
    double lane_center_gap_scale = 0.0;
    double hold_body_width_scale = 0.60;
    bool show_lane_dividers = true;
    bool show_judgement_line = true;
    bool show_gear_boundary_line = false;
    bool hold_tail_taper_enabled = false;
    bool judgement_line_glow_enabled = true;
    bool key_pulse_enabled = true;
    std::string key_label_position = "bottom";
    bool note_border_enabled = true;
    std::string note_shape = "rect";
    bool preserve_note_image_aspect_ratio = false;
    std::string skin_source = "native";
    std::string external_skin_root;
    std::string external_skin_name;
    std::string lr2_resolution_override = "auto";
    double lane_background_opacity = 0.18;
    double visual_opacity = 0.96;
    double note_outline_opacity = 0.78;
    double hold_body_opacity = 0.24;
    std::array<uint32_t, kGameplayHudMaxLanes> lane_colors{};
};

struct GenericMenuData {
    std::string heading;
    std::vector<MenuRowData> rows;
    std::vector<std::string> notes;
    std::vector<std::string> footer_notes;
    int footer_reserved_lines = 0;
    SkinPreviewData skin_preview;
};

struct HelpOverlayData {
    bool visible = false;
    std::string title;
    std::vector<std::string> lines;
    std::string footer;
};

struct PerformanceOverlayData {
    bool visible = false;
    bool valid = false;
    std::size_t sample_count = 0;
    std::size_t graph_sample_count = 0;
    uint64_t graph_revision = 0;
    uint64_t metrics_revision = 0;
    bool gameplay_metrics_visible = false;
    uint64_t gameplay_metrics_revision = 0;
    double gameplay_audio_age_ms = 0.0;
    double gameplay_hud_delta_ms = 0.0;
    double gameplay_extrapolated_ms = 0.0;
    double gameplay_buffer_ms = 0.0;
    double average_frame_ms = 0.0;
    double average_fps = 0.0;
    double max_fps = 0.0;
    double fps_0_1_low = 0.0;
    double fps_0_01_low = 0.0;
    std::array<float, kPerformanceGraphSamples> frame_times_ms{};
};

struct MenuRenderData {
    MenuScreenKind kind = MenuScreenKind::GenericList;
    bool ui_korean = false;

    std::string screen_title;
    std::vector<std::string> lines;
    GenericMenuData generic;
    HelpOverlayData help_overlay;

    TitleMenuData title;
    SongSelectData song_select;
    ResultScreenData result;
    GameplayHudData gameplay;
    PerformanceOverlayData performance;
};

class MenuWindow {
public:
    MenuWindow();
    ~MenuWindow();

    MenuWindow(const MenuWindow&) = delete;
    MenuWindow& operator=(const MenuWindow&) = delete;

    void set_config(const MenuWindowConfig& config);
    void render(const MenuRenderData& data);
    void shutdown();
    void request_close();
    void request_screenshot();
    void queue_resize(unsigned int width, unsigned int height);
    void on_mouse_button_down(int window_x, int window_y);
    void on_mouse_click(int window_x, int window_y, bool double_click);
    void on_mouse_secondary_click(int window_x, int window_y);
    void on_mouse_move(int window_x, int window_y);
    void on_mouse_wheel(int wheel_delta);
    void on_file_drop(std::string path);

    [[nodiscard]] std::optional<MenuClickEvent> poll_click_event();
    [[nodiscard]] bool cursor_hidden() const { return cursor_hidden_; }

    [[nodiscard]] bool should_close() const { return should_close_.load(std::memory_order_acquire); }
    [[nodiscard]] bool init_done() const { return init_done_.load(std::memory_order_acquire); }
    [[nodiscard]] bool init_success() const { return init_success_.load(std::memory_order_acquire); }
    [[nodiscard]] bool had_fatal_error() const { return fatal_error_.load(std::memory_order_acquire); }

private:
    bool initialize(const MenuWindowConfig& config);
    bool fail_fatal(std::string_view message);
    void destroy_window();
    void pump_messages();
    void draw(const MenuRenderData& data);
    void apply_pending_config();
    void update_layout();
    void update_brushes();
    void invalidate_menu_scene_target();
    [[nodiscard]] bool ensure_menu_scene_resources();
    [[nodiscard]] bool render_menu_scene(MenuScreenKind kind, int64_t now_ns);
    void invalidate_gameplay_note_sprite_cache();
    [[nodiscard]] bool ensure_gameplay_note_sprites(const GameplayHudData& data);
    void invalidate_song_select_preview_cache();
    [[nodiscard]] bool ensure_song_select_preview_bitmap(const SongSelectData& data);
    void clear_song_card_preview_cache();
    void update_song_select_preview_loading_state(const SongSelectData& data, int64_t now_ns);
    void pump_song_select_preview_loads(const SongSelectData& data, int64_t now_ns);
    [[nodiscard]] bool song_select_preview_loading_deferred(int64_t now_ns) const;
    [[nodiscard]] ID2D1Bitmap* find_song_card_preview_bitmap(std::string_view path);
    [[nodiscard]] bool load_song_card_preview_bitmap(std::string_view path);
    [[nodiscard]] bool load_selected_song_preview_bitmap(const SongSelectData& data, int64_t now_ns);
    void touch_song_card_preview_lru(std::string_view path);
    void trim_song_card_preview_cache();
    void invalidate_gameplay_static_cache();
    [[nodiscard]] bool ensure_gameplay_static_cache(const GameplayHudData& data);
    [[nodiscard]] bool recreate_targets();
    [[nodiscard]] bool save_screenshot_to_png();
    [[nodiscard]] bool is_input_foreground() const;
    void update_cursor_visibility(bool hidden);
    [[nodiscard]] bool resize_swap_chain(unsigned int width, unsigned int height);
    [[nodiscard]] bool enter_fullscreen_mode(unsigned int width, unsigned int height, const char* log_context);
    void push_click_event(MenuClickEvent event);
    void clear_song_scrollbar_state();
    [[nodiscard]] bool translate_window_point(int window_x, int window_y, float* out_x, float* out_y) const;
    [[nodiscard]] int song_scrollbar_target_index(float y, float drag_offset, int selected_offset) const;

    std::mutex config_mutex_;
    MenuWindowConfig config_{};
    MenuWindowConfig pending_config_{};
    bool config_dirty_ = false;

    std::atomic<bool> should_close_{false};
    std::atomic<bool> initialized_{false};
    std::atomic<bool> init_done_{false};
    std::atomic<bool> init_success_{false};
    std::atomic<bool> fatal_error_{false};
    std::atomic<bool> screenshot_requested_{false};
    bool fullscreen_ = false;
    bool fullscreen_restore_pending_ = false;
    bool com_initialized_ = false;
    bool resize_pending_ = false;
    unsigned int pending_width_ = 0;
    unsigned int pending_height_ = 0;
    unsigned int swap_chain_flags_ = 0;
    bool suppress_next_left_button_up_ = false;
    bool cursor_hidden_ = false;

    void* hwnd_ = nullptr;
    unsigned int width_ = 0;
    unsigned int height_ = 0;
    float scale_ = 1.0f;
    float offset_x_ = 0.0f;
    float offset_y_ = 0.0f;

    struct HitRegion {
        MenuHitTargetKind kind = MenuHitTargetKind::None;
        int index = -1;
        MenuHitPart part = MenuHitPart::Activate;
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
    };

    std::vector<HitRegion> hit_regions_{};
    std::mutex click_events_mutex_{};
    std::deque<MenuClickEvent> click_events_{};

    struct PerformanceOverlayCache {
        uint64_t graph_revision = 0;
        uint64_t metrics_revision = 0;
        uint64_t gameplay_metrics_revision = 0;
        bool compact_layout = false;
        bool gameplay_metrics_visible = false;
        float graph_ceiling_ms = 16.67f;
        float avg_line_ratio = 0.0f;
        std::wstring sample_text{};
        std::wstring top_label_text{};
        std::wstring avg_label_text{};
        std::array<std::wstring, 5> value_texts{};
        std::array<std::wstring, 4> gameplay_value_texts{};
    };

    struct GameplayHudCache {
        uint64_t text_revision = 0;
        std::wstring title_text{};
        std::wstring artist_text{};
        std::wstring speed_text{};
        std::wstring score_text{};
        std::wstring combo_text{};
        std::wstring combo_value_text{};
        std::wstring combo_label_text{};
        std::wstring judge_stats_text{};
        std::wstring gauge_label_text{};
        std::wstring gauge_value_text{};
        std::wstring feedback_text{};
        std::wstring peer_name_text{};
        std::wstring peer_status_text{};
        std::wstring peer_score_text{};
        std::wstring peer_combo_text{};
        std::wstring peer_judge_stats_text{};
        std::wstring peer_gauge_text{};
        std::wstring versus_score_difference_text{};
        std::wstring spectating_text{};
        std::wstring ghost_score_text{};
        std::wstring ghost_combo_text{};
        std::wstring ghost_combo_value_text{};
        std::wstring ghost_judge_stats_text{};
        std::wstring ghost_gauge_label_text{};
        std::wstring ghost_gauge_value_text{};
        std::wstring ghost_feedback_text{};
    };

    struct GameplayStaticCache {
        int lane_count = 0;
        double judgement_line_position = 0.82;
        double note_width_scale = 1.0;
        double note_height_scale = 1.8;
        std::size_t lane_width_scale_count = 0;
        std::array<double, kGameplayHudMaxLanes> lane_width_scales{};
        std::size_t lane_spacing_scale_count = 0;
        std::array<double, kGameplayHudMaxLanes> lane_spacing_scales{};
        double lane_divider_width_scale = 1.0;
        double lane_center_gap_scale = 0.0;
        bool show_lane_dividers = true;
        bool show_judgement_line = true;
        bool show_gear_boundary_line = false;
        bool judgement_line_glow_enabled = true;
        double lane_background_opacity = 0.18;
        double visual_opacity = 0.96;
        bool ghost_visible = false;
        std::size_t lane_color_count = 0;
        std::array<uint32_t, kGameplayHudMaxLanes> lane_colors{};
        std::size_t lane_divider_width_count = 0;
        std::array<float, kGameplayHudMaxLanes> lane_divider_widths{};
    };

    struct GameplayNoteSpriteCache {
        int lane_count = 0;
        bool note_border_enabled = true;
        std::string note_shape = "rect";
        bool preserve_note_image_aspect_ratio = false;
        std::string skin_source = "native";
        std::string external_skin_root;
        std::string external_skin_name;
        std::string lr2_resolution_override = "auto";
        bool use_full_lane_receptor_layout = false;
        std::size_t lane_divider_width_count = 0;
        std::array<float, kGameplayHudMaxLanes> lane_divider_widths{};
        std::size_t imported_lane_width_scale_count = 0;
        std::array<double, kGameplayHudMaxLanes> imported_lane_width_scales{};
        std::size_t imported_lane_spacing_scale_count = 0;
        std::array<double, kGameplayHudMaxLanes> imported_lane_spacing_scales{};
        bool has_imported_judgement_line_position = false;
        double imported_judgement_line_position = 0.82;
        float imported_note_width_ratio = 1.0f;
        float imported_note_height_ratio = 1.0f;
        std::array<uint32_t, kGameplayHudMaxLanes> lane_colors{};
    };

    struct SongSelectPreviewCache {
        std::string path{};
        bool attempted = false;
    };

    struct SongScrollbarState {
        bool visible = false;
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
        float thumb_top = 0.0f;
        float thumb_bottom = 0.0f;
        int total_count = 0;
        int visible_count = 0;
        int window_start = 0;
        int selected_index = -1;
    };

    struct D2DResources;
    std::unique_ptr<D2DResources> d2d_;
    PerformanceOverlayCache performance_overlay_cache_{};
    GameplayHudCache gameplay_hud_cache_{};
    GameplayStaticCache gameplay_static_cache_{};
    GameplayNoteSpriteCache gameplay_note_sprite_cache_{};
    SongSelectPreviewCache song_select_preview_cache_{};
    std::string song_select_preview_signature_{};
    int64_t song_select_preview_load_hold_until_ns_ = 0;
    std::unordered_set<std::string> song_select_preview_warned_decode_failures_{};
    std::unordered_set<std::string> song_select_preview_warned_slow_paths_{};
    SongScrollbarState song_scrollbar_state_{};
    bool song_scroll_drag_active_ = false;
    float song_scroll_drag_offset_y_ = 0.0f;
    int song_scroll_drag_selected_offset_ = 0;
    int song_scroll_drag_last_index_ = -1;
};

}  // namespace tenriff::render

#endif  // _WIN32
