#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "app/ImportedGameplaySkin.h"

namespace tenriff::app {

inline constexpr int kTenRiffSkinFormatVersion = 1;

// Menu rect override, in the 1920x1080 base coordinate space the renderer uses.
struct SkinLayoutRect {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

inline constexpr std::array<std::string_view, 20> kTenRiffSkinScreenIds = {
    "quick_setup", "title", "options", "multiplayer", "song_select",
    "session_mix", "song_browser", "settings", "settings_audio",
    "settings_graphics", "settings_skins", "settings_input",
    "settings_calibration", "mode_select", "mode_mods", "keymap",
    "keymap_confirm", "onnx_upscaler_confirm", "keymap_test", "result"
};

// Dedicated render rects plus generic content/preview fallbacks. Every screen ID
// above may also define its own content and preview slots.
inline constexpr std::array<std::string_view, 22> kTenRiffSkinLayoutSlots = {
    "title.spectrum",
    "title.logo",
    "title.buttons",
    "title.guide",
    "title.footer",
    "song_select.top_bar",
    "song_select.logo",
    "song_select.nav",
    "song_select.profile",
    "song_select.left_panel",
    "song_select.center_panel",
    "song_select.right_panel",
    "song_select.bottom_bar",
    "generic.content",
    "generic.preview",
    "result.profile",
    "result.song_panel",
    "result.analysis_panel",
    "result.stats_panel",
    "result.continue",
    "result.replay",
    "result.retry",
};

[[nodiscard]] bool is_tenriff_skin_layout_slot(std::string_view key);

// Optional manifest-level gameplay appearance. A missing field preserves the
// player's configured value; an explicitly supplied field belongs to the skin.
struct TenRiffSkinGameplayStyle {
    std::optional<bool> show_lane_dividers;
    std::optional<bool> show_judgement_line;
    std::optional<bool> show_timing_feedback;
    std::optional<bool> show_gear_boundary_line;
    std::optional<bool> show_hold_tail;
    std::optional<bool> hold_tail_taper_enabled;
    std::optional<bool> judgement_line_glow_enabled;
    std::optional<bool> key_pulse_enabled;
    std::optional<bool> note_border_enabled;
    std::optional<bool> black_playfield_enabled;
    std::optional<float> key_pulse_brightness;
    std::optional<float> lane_background_opacity;
    std::optional<float> visual_opacity;
    std::optional<float> note_outline_opacity;
    std::optional<float> hold_body_opacity;
    std::optional<std::string> hit_burst_style;
    std::optional<std::string> key_label_position;
    std::optional<std::string> note_shape;
    std::vector<std::uint32_t> lane_colors;
};

struct TenRiffSkinDefinition {
    bool found = false;
    int format_version = 0;
    std::string folder_name;
    std::string name;
    std::string author;
    std::string root_path;
    std::string lobby_background_path;
    std::string lobby_logo_path;
    float lobby_background_opacity = 0.72f;
    std::unordered_map<std::string, std::string> screen_background_paths;
    std::unordered_map<std::string, float> screen_background_opacities;
    // UI brush name -> normalized RGBA floats.
    std::unordered_map<std::string, std::array<float, 4>> theme_colors;
    std::string gameplay_background_path;
    float gameplay_background_opacity = 0.66f;
    std::unordered_map<std::string, SkinLayoutRect> layout_rects;
    ImportedGameplaySkinDefinition gameplay;
    TenRiffSkinGameplayStyle gameplay_style;
    std::vector<std::string> referenced_asset_paths;
    std::vector<std::string> warnings;
};

struct TenRiffSkinCreateResult {
    std::string skin_name;
    std::string folder_path;
    std::vector<std::string> warnings;

    [[nodiscard]] bool success() const { return !skin_name.empty(); }
};

struct TenRiffSkinImportResult {
    std::string skin_name;
    std::string install_root;
    std::size_t copied_files = 0;
    std::uintmax_t copied_bytes = 0;
    std::vector<std::string> warnings;

    [[nodiscard]] bool success() const { return !skin_name.empty(); }
};

[[nodiscard]] TenRiffSkinDefinition load_tenriff_skin_folder(std::string_view folder_utf8,
                                                              int keys);
[[nodiscard]] TenRiffSkinDefinition resolve_tenriff_skin(std::string_view root_utf8,
                                                         std::string_view skin_name,
                                                         int keys);
[[nodiscard]] std::vector<std::string> list_tenriff_skin_names(std::string_view root_utf8);
[[nodiscard]] TenRiffSkinImportResult import_tenriff_skin(std::string_view source_utf8,
                                                          std::string_view import_root_utf8);
[[nodiscard]] TenRiffSkinCreateResult create_tenriff_skin_template(
    std::string_view import_root_utf8);

}  // namespace tenriff::app
