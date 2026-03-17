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
#include <vector>

namespace tenriff::render {

struct MenuWindowConfig {
    std::string title = "TenRiff";
    std::string display_mode = "borderless";
    bool vsync = false;
    int refresh_hz = 1050;
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
    std::string title;
    std::string artist;
    std::string detail;
    int level = 0;
    double rating = 0.0;
    int song_index = -1;
    bool selected = false;
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
    std::string selected_source_name;
    std::string selected_source_path;
    int selected_source_song_count = -1;
    bool selected_source_active = false;
    std::string selected_song_title;
    std::string selected_song_artist;
    std::string selected_song_detail;
    std::string selected_song_background_path;
    std::string browser_summary;
    std::string sort_summary;
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
    int miss = 0;
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
    std::string profile;
    std::string track;
    std::string title;
    std::string artist;

    std::string rank = "--";
    std::string status = "NO DATA";
    std::string gauge_label = "NORMAL";

    int64_t score = 0;
    double accuracy = 0.0;
    double gauge_value = 0.0;
    int max_combo = 0;
    int total_notes = 0;
    int judged_notes = 0;

    int perfect = 0;
    int great = 0;
    int good = 0;
    int bad = 0;
    int miss = 0;

    double mean_delta_ms = 0.0;
    double stddev_delta_ms = 0.0;
    int shift_count = 0;
    int export_warning_count = 0;

    std::string replay_file;
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
    double note_width_scale = 1.0;
    double note_height_scale = 1.8;
    double hold_body_width_scale = 0.60;
    bool note_border_enabled = true;
    std::string note_shape = "rect";
    std::string skin_source = "native";
    std::string osu_skin_root;
    std::string osu_skin_name;
    double visual_offset_ms = 0.0;

    double bpm = 0.0;
    double rate = 1.0;
    double hispeed = 3.0;
    double scroll_speed = 0.0;

    int64_t score = 0;
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

    bool finished = false;
    bool game_over = false;

    std::size_t lane_activity_count = 0;
    std::array<float, kGameplayHudMaxLanes> lane_activity{};
    std::size_t lane_color_count = 0;
    std::array<uint32_t, kGameplayHudMaxLanes> lane_colors{};
    std::size_t note_count = 0;
    std::array<GameplayNoteData, kGameplayHudMaxNotes> notes{};
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
    double judgement_line_position = 0.82;
    double combo_position = 0.24;
    double note_width_scale = 1.0;
    double note_height_scale = 1.8;
    double hold_body_width_scale = 0.60;
    bool note_border_enabled = true;
    std::string note_shape = "rect";
    std::array<uint32_t, kGameplayHudMaxLanes> lane_colors{};
};

struct GenericMenuData {
    std::string heading;
    std::vector<MenuRowData> rows;
    std::vector<std::string> notes;
    SkinPreviewData skin_preview;
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

    std::string screen_title;
    std::vector<std::string> lines;
    GenericMenuData generic;

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
    void invalidate_gameplay_static_cache();
    [[nodiscard]] bool ensure_gameplay_static_cache(const GameplayHudData& data);
    [[nodiscard]] bool recreate_targets();
    [[nodiscard]] bool is_input_foreground() const;
    void update_cursor_visibility(bool hidden);
    void resize_swap_chain(unsigned int width, unsigned int height);
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
        std::wstring judge_stats_text{};
        std::wstring gauge_label_text{};
        std::wstring gauge_value_text{};
        std::wstring feedback_text{};
    };

    struct GameplayStaticCache {
        int lane_count = 0;
        double judgement_line_position = 0.82;
        double note_width_scale = 1.0;
        double note_height_scale = 1.8;
    };

    struct GameplayNoteSpriteCache {
        int lane_count = 0;
        bool note_border_enabled = true;
        std::string note_shape = "rect";
        std::string skin_source = "native";
        std::string osu_skin_root;
        std::string osu_skin_name;
        bool using_osu_skin_assets = false;
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
    SongScrollbarState song_scrollbar_state_{};
    bool song_scroll_drag_active_ = false;
    float song_scroll_drag_offset_y_ = 0.0f;
    int song_scroll_drag_selected_offset_ = 0;
    int song_scroll_drag_last_index_ = -1;
};

}  // namespace tenriff::render

#endif  // _WIN32
