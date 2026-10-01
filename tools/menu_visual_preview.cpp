#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "render/MenuWindow.h"
#include "timing/HighResClock.h"
#include "app/PeerBattleRuntimeRules.h"
#include "app/TenRiffSkin.h"
#include "app/MenuApp.h"
#include "config/Config.h"
#include "config/GraphicsResolution.h"
#include "render/SkinGameplayPreview.h"
#include "app/menu/settings/AudioSettingsView.h"
#include "app/menu/settings/KeymapSettingsView.h"
#include "config/KeycodeMap.h"
#include "config/SimpleJson.h"
#include "app/SettingsHelpTips.h"

#include <windows.h>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <sstream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <set>
#include <unordered_set>
#include <optional>
#include <memory>

namespace tenriff::app {
// Seed only in-memory state, then use the production Skin Settings builder.
// MenuApp is never initialized: no profiles, indexing, services or input start.
struct MenuAppVisualTestAccess {
    static void populate_skin_rows(render::MenuRenderData& source,
                                   const TenRiffSkinDefinition* imported,
                                   int selected_row, bool backdrop_override) {
        auto app = std::make_unique<MenuApp>();
        const auto& preview = source.generic.skin_preview;
        const std::string mode = std::to_string(preview.lane_count) + "k";
        app->config_.ui.language = ui::language_token(source.ui_language);
        auto& skin = app->config_.skin;
        // Keep the deterministic scene's values reflected in its actual rows.
        skin.note_height_scale = preview.note_height_scale;
        skin.note_height_scales[mode] = preview.note_height_scale;
        skin.note_width_scale = preview.note_width_scale;
        skin.note_width_scales[mode] = preview.note_width_scale;
        skin.lane_divider_width_scale = preview.lane_divider_width_scale;
        skin.lane_divider_width_scales[mode] = preview.lane_divider_width_scale;
        skin.lane_center_gap_scale = preview.lane_center_gap_scale;
        skin.lane_center_gap_scales[mode] = preview.lane_center_gap_scale;
        skin.hold_body_width_scale = preview.hold_body_width_scale;
        skin.judgement_line_position = preview.judgement_line_position;
        skin.gameplay_field_offset_x = preview.gameplay_field_offset_x;
        skin.combo_position = preview.combo_position;
        skin.judgement_position = preview.judgement_position;
        skin.combo_offset_x = preview.combo_offset_x;
        skin.combo_font_scale = preview.combo_font_scale;
        skin.judgement_font_scale = preview.judgement_font_scale;
        skin.judgement_offset_x = preview.judgement_offset_x;
        skin.show_lane_dividers = preview.show_lane_dividers;
        skin.note_divider_gap_px = preview.note_divider_gap_px;
        skin.show_judgement_line = preview.show_judgement_line;
        skin.show_gear_boundary_line = preview.show_gear_boundary_line;
        skin.show_hold_tail = preview.show_hold_tail;
        skin.hold_tail_taper_enabled = preview.hold_tail_taper_enabled;
        skin.judgement_line_glow_enabled = preview.judgement_line_glow_enabled;
        skin.key_pulse_enabled = preview.key_pulse_enabled;
        skin.key_pulse_brightness = preview.key_pulse_brightness;
        skin.key_backdrop_override = backdrop_override;
        skin.key_backdrop_enabled = preview.key_backdrop_enabled;
        skin.key_backdrop_opacity = preview.key_backdrop_opacity;
        skin.key_backdrop_brightness = preview.key_backdrop_brightness;
        skin.key_backdrop_height = preview.key_backdrop_height;
        skin.hit_burst_style = preview.hit_burst_style;
        skin.key_label_position = preview.key_label_position;
        skin.note_border_enabled = preview.note_border_enabled;
        skin.note_shape = preview.note_shape;
        skin.lane_background_opacity = preview.lane_background_opacity;
        skin.black_playfield_enabled = preview.black_playfield_enabled;
        skin.visual_opacity = preview.visual_opacity;
        skin.note_outline_opacity = preview.note_outline_opacity;
        skin.hold_body_opacity = preview.hold_body_opacity;
        if (imported) {
            skin.source = "tenriff";
            skin.tenriff_skin_name = imported->folder_name.empty() ? imported->name : imported->folder_name;
            app->available_tenriff_skin_names_ = {skin.tenriff_skin_name};
            app->available_tenriff_skin_root_ = imported->root_path;
            app->active_tenriff_skin_ = *imported;
            const auto index = static_cast<std::size_t>(preview.lane_count - 1);
            app->active_tenriff_skin_modes_[index] = *imported;
            app->active_tenriff_gameplay_modes_[index] = preview.resolved_tenriff_skin;
        }
        config::KeymapManager keymap_manager;
        app->keymap_ = keymap_manager.default_keymap();
        app->skin_settings_controller_.reset(mode);
        if (selected_row >= 0) {
            if (const auto id = menu::settings::skin_setting_id_at(static_cast<std::size_t>(selected_row), false))
                static_cast<void>(app->skin_settings_controller_.select(*id, false));
        }
        render::MenuRenderData actual;
        actual.ui_language = source.ui_language;
        app->populate_skin_settings_render_data(actual);
        source.generic.rows = std::move(actual.generic.rows);
        source.generic.notes = std::move(actual.generic.notes);
        const auto tips = settings_help_tips(source.ui_language);
        source.generic.notes.insert(source.generic.notes.begin(), tips.begin(), tips.end());
    }
};
}  // namespace tenriff::app

namespace tenriff::render {
// Only this standalone fixture inspects private hit regions and the production
// geometry resolver. Native dispatch is checked only while its own window has
// foreground focus; no desktop pointer or keyboard is synthesized.
struct MenuWindowVisualTestAccess {
    static bool verify_and_write(MenuWindow& window, const MenuRenderData& source,
                                 const std::string& hitmap_path, bool verify) {
        using namespace config;
        JsonArray captured_regions;
        JsonArray failures;
        int checked = 0;
        const bool foreground = window.is_input_foreground();
        bool native_dispatch_verified = verify && foreground;
        const bool skin_fixture = source.kind == MenuScreenKind::GenericList &&
            source.generic.heading == ui::text(source.ui_language, "Skin Settings", "스킨 설정");
        const bool song_fixture = source.kind == MenuScreenKind::SongSelect;
        JsonArray limitations;
        if (verify && !foreground)
            limitations.emplace_back("Own preview window is not foreground in this execution environment. Geometry resolver checks run; native input dispatch is unverified.");
        auto fixture = source;
        const std::size_t visits = verify && !fixture.generic.rows.empty() ? fixture.generic.rows.size() : 1;
        std::set<std::pair<int, int>> verified_targets;
        for (std::size_t visit = 0; visit < visits; ++visit) {
            if (verify) {
                for (std::size_t row = 0; row < fixture.generic.rows.size(); ++row)
                    fixture.generic.rows[row].selected = row == visit;
                window.render(fixture);
            }
            const auto regions = window.hit_regions_;
            for (const auto& region : regions) {
                const bool song_control = song_fixture &&
                    (region.kind == MenuHitTargetKind::SongCard || region.kind == MenuHitTargetKind::SongNavButton ||
                     region.kind == MenuHitTargetKind::SongDifficultyTable);
                if (!song_control && region.kind != MenuHitTargetKind::SettingsRow && region.kind != MenuHitTargetKind::KeymapButton)
                    continue;
                const bool control = song_control || region.part == MenuHitPart::Decrement ||
                    region.part == MenuHitPart::Increment || region.part == MenuHitPart::SetValue ||
                    (skin_fixture && region.part == MenuHitPart::Activate) ||
                    fixture.generic.keymap_keyboard;
                if (verify && (!control || (visits > 1 &&
                    (region.kind != fixture.generic.rows[visit].target_kind ||
                     region.index != fixture.generic.rows[visit].row_index)))) continue;
                const float x = (region.left + region.right) * 0.5f;
                const float y = (region.top + region.bottom) * 0.5f;
                const int window_x = static_cast<int>(std::lround(window.offset_x_ + x * window.scale_));
                const int window_y = static_cast<int>(std::lround(window.offset_y_ + y * window.scale_));
                JsonObject item{
                    {"kind", JsonValue(static_cast<double>(region.kind))},
                    {"id", JsonValue(static_cast<double>(region.index))},
                    {"part", JsonValue(static_cast<double>(region.part))},
                    {"logical_rect", JsonValue(JsonArray{JsonValue(static_cast<double>(region.left)), JsonValue(static_cast<double>(region.top)), JsonValue(static_cast<double>(region.right)), JsonValue(static_cast<double>(region.bottom))})},
                    {"client_point", JsonValue(JsonArray{JsonValue(static_cast<double>(window_x)), JsonValue(static_cast<double>(window_y))})}
                };
                if (verify) {
                    // Production on_mouse_click calls this same resolver after its
                    // foreground gate. Direct use here checks geometry without
                    // synthesizing input or weakening the real focus boundary.
                    const auto resolved = window.resolve_menu_click(window_x, window_y, false, false);
                    const bool geometry_pass = resolved && resolved->kind == region.kind &&
                        resolved->index == region.index && resolved->part == region.part;
                    item.emplace("geometry_resolver_verified", JsonValue(geometry_pass));
                    item.emplace("native_dispatch_verified", JsonValue(false));
                    if (resolved) {
                        item.emplace("received_id", JsonValue(static_cast<double>(resolved->index)));
                        item.emplace("received_part", JsonValue(static_cast<double>(resolved->part)));
                        if (region.part == MenuHitPart::SetValue && std::abs(resolved->value - 0.5) > 0.02)
                            failures.emplace_back("slider center did not produce ratio 0.5");
                    }
                    if (geometry_pass) verified_targets.emplace(region.index, static_cast<int>(region.part));
                    else failures.emplace_back("geometry hit mismatch: id=" + std::to_string(region.index) + " part=" + std::to_string(static_cast<int>(region.part)));
                    if (foreground) {
                        while (window.poll_click_event()) {}
                        window.on_mouse_click(window_x, window_y, false);
                        const auto dispatched = window.poll_click_event();
                        const bool native_pass = dispatched && dispatched->kind == region.kind &&
                            dispatched->index == region.index && dispatched->part == region.part;
                        item["native_dispatch_verified"] = JsonValue(native_pass);
                        native_dispatch_verified = native_dispatch_verified && native_pass;
                        if (!native_pass)
                            failures.emplace_back("native dispatch mismatch: id=" + std::to_string(region.index));
                    }
                    ++checked;
                }
                captured_regions.emplace_back(std::move(item));
            }
        }
        if (verify && song_fixture) {
            const auto search = std::find_if(window.hit_regions_.begin(), window.hit_regions_.end(), [](const auto& hit) {
                return hit.kind == MenuHitTargetKind::SongNavButton && hit.index == 2;
            });
            if (search == window.hit_regions_.end()) failures.emplace_back("song search field missing");
            else {
                std::size_t cards = 0;
                for (const auto& hit : window.hit_regions_) {
                    if (hit.kind != MenuHitTargetKind::SongCard) continue;
                    ++cards;
                    if (hit.bottom > search->top || std::abs(hit.left - search->left) > 1.0f ||
                        std::abs(hit.right - search->right) > 1.0f)
                        failures.emplace_back("song search must align below every visible card");
                }
                if (cards != source.song_select.songs.size()) failures.emplace_back("visible song card lost to search field");
                if (window.song_scrollbar_state_.visible && window.song_scrollbar_state_.bottom > search->top)
                    failures.emplace_back("song scrollbar overlaps search field");
            }
        }
        if (verify && source.generic.heading == ui::text(source.ui_language, "Audio Settings", "오디오 설정")) {
            using namespace app::menu::settings;
            for (const auto id : {AudioSettingId::Normalize, AudioSettingId::Backend,
                                 AudioSettingId::AsioDriver, AudioSettingId::SampleRate, AudioSettingId::BufferFrames}) {
                if (!verified_targets.count({static_cast<int>(id), static_cast<int>(MenuHitPart::Decrement)}))
                    failures.emplace_back("required audio minus hit missing: id=" + std::to_string(static_cast<int>(id)));
            }
        }
        if (verify && skin_fixture) {
            // Every enabled real row must be reachable after scrolling to it.
            // Disabled 7+1/16K/source-specific rows deliberately have no action.
            for (const auto& row : source.generic.rows) {
                const auto require_part = [&](MenuHitPart part) {
                    if (!verified_targets.count({row.row_index, static_cast<int>(part)}))
                        failures.emplace_back("required skin hit missing: id=" + std::to_string(row.row_index) +
                                              " part=" + std::to_string(static_cast<int>(part)));
                };
                if (row.slider) require_part(MenuHitPart::SetValue);
                else if (row.adjustable) {
                    require_part(MenuHitPart::Activate);
                    if (row.decrement_enabled) require_part(MenuHitPart::Decrement);
                    if (row.increment_enabled) require_part(MenuHitPart::Increment);
                } else if (row.activatable) require_part(MenuHitPart::Activate);
            }
        }
        window.render(source);
        JsonArray rows;
        for (const auto& row : source.generic.rows) rows.emplace_back(JsonObject{
            {"id", JsonValue(static_cast<double>(row.row_index))}, {"label", JsonValue(row.label)},
            {"value", JsonValue(row.value)}, {"category", JsonValue(row.category)},
            {"hit_eligible", JsonValue(row.activatable || row.adjustable)}, {"slider", JsonValue(row.slider)}});
        JsonArray notes;
        for (const auto& note : source.generic.notes) notes.emplace_back(note);
        JsonObject report{
            {"notes", JsonValue(std::move(notes))},
            {"width", JsonValue(static_cast<double>(window.width_))}, {"height", JsonValue(static_cast<double>(window.height_))},
            {"scale", JsonValue(static_cast<double>(window.scale_))}, {"foreground", JsonValue(foreground)},
            {"geometry_resolver_verified", JsonValue(verify && checked > 0 && failures.empty())},
            {"native_dispatch_verified", JsonValue(native_dispatch_verified && checked > 0)},
            {"limitations", JsonValue(std::move(limitations))},
            {"verified_regions", JsonValue(static_cast<double>(checked))},
            {"rows", JsonValue(std::move(rows))}, {"regions", JsonValue(std::move(captured_regions))},
            {"failures", JsonValue(failures)}
        };
        if (!hitmap_path.empty()) {
            const auto path = std::filesystem::u8path(hitmap_path);
            if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
            std::ofstream out(path, std::ios::binary);
            if (!out.good()) return false;
            out << json_stringify(JsonValue(std::move(report)), 2) << '\n';
        }
        std::cout << "geometry hit checks=" << checked << " failures=" << failures.size()
                  << " native_dispatch_verified=" << (native_dispatch_verified && checked > 0)
                  << " foreground=" << foreground << '\n';
        return failures.empty();
    }
};
}  // namespace tenriff::render

// Synthetic scene data; real settings builders. No MenuApp initialization or record writes.
int main(int argc, char** argv) {
    using namespace tenriff::render;
    MenuWindowConfig config;
    config.title = "TenRiff UI Preview - synthetic data";
    config.display_mode = "windowed";
    config.vsync = true;
    bool result = false;
    bool options_grid = false;
    bool table_editor = false;
    bool gameplay = false;
    bool ghost = false;
    bool ghost_paused = false;
    bool no_feedback = false;
    bool search_active = false;
    std::string search_query;
    bool skin_scene = false;
    double field_offset = 0;
    int players = 0;
    int preview_fps = 144;
    int preview_keys = 10;
    int menu_keys = 7;
    double preview_note_height = 1.8;
    double preview_note_width = 1.0;
    double combo_font_scale = 1.0;
    double judgement_font_scale = 1.0;
    std::string bga_path, bga_next_path, bga_overlay_path;
    bool opaque_field = false;
    double fixture_seconds = -1.0;
    bool fixture_idle = false;
    int fixed_grade = -1;
    bool moved_labels = false;
    bool empty = false;
    bool failed = false;
    bool reveal = false;
    bool settings = false;
    bool skin_settings = false;
    bool keymap_settings = false;
    bool keymap_test = false;
    bool keys_explicit = false;
    bool mute_inactive = false;
    std::string title_music_mode = "default";
    bool backdrop_off = false;
    double backdrop_opacity = -1.0;
    double backdrop_brightness = -1.0;
    double backdrop_height = -1.0;
    int pressed_lane = 1;
    int selected_row = -1;
    int selected_setting = -1;
    bool verify_hits = false;
    bool hitmap_written = false;
    std::string hitmap_path;
    bool profile_settings = false;
    bool title = false;
    bool focus_options = false;
    bool sites_account = false;
    bool sites_connected = false;
    bool help_overlay = false;
    bool chat_overlay = false;
    bool url_warning = false;
    bool bms_editor = false;
    bool cycle_font_scale = false;
    bool capture_once = false;
    bool capture_requested = false;
    int rendered_frames = 0;
    int frame_limit = 0;
    int avatar_refresh_frame = 0;
    bool cycle_selection = false;
    std::vector<int> capture_frames;
    std::string skin_folder;
    std::optional<tenriff::app::TenRiffSkinDefinition> fixture_skin;
    std::string avatar_path;
    MenuRenderData data;
    data.ui_language = tenriff::ui::Language::Korean;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--verify-hits") verify_hits = true;
        else if (arg == "--hitmap" && i + 1 < argc) hitmap_path = argv[++i];
        else if (arg == "--select-row" && i + 1 < argc) selected_row = std::stoi(argv[++i]);
        else if (arg == "--select-setting" && i + 1 < argc) selected_setting = std::stoi(argv[++i]);
        else if (arg == "--combo-font-scale" && i + 1 < argc) combo_font_scale = std::clamp(std::stod(argv[++i]), 0.5, 2.0);
        else if (arg == "--judgement-font-scale" && i + 1 < argc) judgement_font_scale = std::clamp(std::stod(argv[++i]), 0.5, 2.0);
        else if (arg == "--bga" && i + 1 < argc) bga_path = argv[++i];
        else if (arg == "--bga-next" && i + 1 < argc) bga_next_path = argv[++i];
        else if (arg == "--bga-overlay" && i + 1 < argc) bga_overlay_path = argv[++i];
        else if (arg == "--opaque-field") opaque_field = true;
        else if (arg == "--keymap") keymap_settings = true;
        else if (arg == "--keymap-test") keymap_settings = keymap_test = true;
        else if (arg == "--mute-inactive") mute_inactive = true;
        else if (arg == "--title-music" && i + 1 < argc) title_music_mode = argv[++i];
        else if (arg == "--key-backdrop-off") backdrop_off = true;
        else if (arg == "--key-backdrop-brightness" && i + 1 < argc) {
            backdrop_brightness = std::stod(argv[++i]);
            if (!std::isfinite(backdrop_brightness) || backdrop_brightness < 0 || backdrop_brightness > 2) return 2;
        }
        else if (arg == "--key-backdrop-height" && i + 1 < argc) {
            backdrop_height = std::stod(argv[++i]);
            if (!std::isfinite(backdrop_height) || backdrop_height < 0 || backdrop_height > 1) return 2;
        }
        else if (arg == "--key-backdrop-opacity" && i + 1 < argc) {
            backdrop_opacity = std::stod(argv[++i]);
            if (!std::isfinite(backdrop_opacity) || backdrop_opacity < 0 || backdrop_opacity > 1) return 2;
        }
        else if (arg == "--pressed-lane" && i + 1 < argc) pressed_lane = std::stoi(argv[++i]);
        else if (arg == "--sites-account") sites_account = true;
        else if (arg == "--help") help_overlay = true;
        else if (arg == "--chat") chat_overlay = true;
        else if (arg == "--url-warning") url_warning = true;
        else if (arg == "--bms-editor") bms_editor = true;
        else if (arg == "--cycle-font-scale") cycle_font_scale = true;
        else if (arg == "--japanese") data.ui_language = tenriff::ui::Language::Japanese;
        else if (arg == "--reduced-motion") config.reduce_menu_motion = true;
        else if (arg == "--cycle-selection") cycle_selection = true;
        else if (arg == "--frames" && i + 1 < argc) frame_limit = std::stoi(argv[++i]);
        else if (arg == "--skin" && i + 1 < argc) skin_folder = argv[++i];
        else if (arg == "--font-size" && i + 1 < argc)
            data.ui_text_scale = tenriff::config::menu_text_scale(argv[++i]);
        else if (arg == "--avatar" && i + 1 < argc) avatar_path = argv[++i];
        else if (arg == "--note-height" && i + 1 < argc)
            preview_note_height = std::clamp(std::stod(argv[++i]),
                tenriff::config::kNoteHeightScaleMin, tenriff::config::kNoteHeightScaleMax);
        else if (arg == "--avatar-refresh-frame" && i + 1 < argc) avatar_refresh_frame = std::stoi(argv[++i]);
        else if (arg == "--profile") profile_settings = true;
        else if (arg == "--capture-frames" && i + 1 < argc) {
            std::istringstream stream(argv[++i]);
            std::string value;
            while (std::getline(stream, value, ',')) capture_frames.push_back(std::stoi(value));
        }
        else if (arg == "--sites-connected") { sites_account = true; sites_connected = true; }
        else if (arg == "--capture") capture_once = true;
        else if (arg == "--table-editor") table_editor = true;
        else if (arg == "--options") options_grid = true;
        else if (arg == "--gameplay") gameplay = true;
        else if (arg == "--ghost") ghost = gameplay = true;
        else if (arg == "--ghost-paused") ghost_paused = ghost = gameplay = true;
        else if (arg == "--no-feedback") no_feedback = true;
        else if (arg == "--search-active") search_active = true;
        else if (arg == "--search-query" && i + 1 < argc) search_query = argv[++i];
        else if (arg == "--note-width" && i + 1 < argc) preview_note_width = std::clamp(std::stod(argv[++i]), 0.5, 2.0);
        else if (arg == "--keys" && i + 1 < argc) {
            keys_explicit = true;
            preview_keys = std::stoi(argv[++i]);
            if (preview_keys < 4 || preview_keys > 16) return 2;
            menu_keys = preview_keys;
        }
        else if (arg == "--fixture-time" && i + 1 < argc) fixture_seconds = std::stod(argv[++i]);
        else if (arg == "--idle-keys") fixture_idle = true;
        else if (arg == "--three-players") players = 3;
        else if (arg == "--eight-players") players = 8;
        else if (arg == "--60fps") preview_fps = 60;
        else if (arg == "--perfect") fixed_grade = 0;
        else if (arg == "--great") fixed_grade = 1;
        else if (arg == "--good") fixed_grade = 2;
        else if (arg == "--moved-labels") moved_labels = true;
        else if (arg == "--small") { config.width = 960; config.height = 540; }
        else if (arg == "--result") result = true;
        else if (arg == "--title") title = true;
        else if (arg == "--focus-options") focus_options = true;
        else if (arg == "--settings") settings = true;
        else if (arg == "--skin-scene") { settings = skin_settings = skin_scene = true; }
        else if (arg == "--field-offset" && i + 1 < argc) field_offset = std::stod(argv[++i]);
        else if (arg == "--skin-settings") { settings = true; skin_settings = true; }
        else if (arg == "--empty") empty = true;
        else if (arg == "--failed") failed = true;
        else if (arg == "--reveal") reveal = true;
        else if (arg == "--english") data.ui_language = tenriff::ui::Language::English;
        else if (arg == "--performance") data.performance.visible = true;
        else if (arg == "--resolution" && i + 1 < argc) {
            const auto [width, height] = tenriff::config::graphics_resolution_dimensions(argv[++i]);
            if (width == 0 || height == 0) return 2;
            config.width = width; config.height = height;
        }
        else if (arg == "--borderless") config.display_mode = "borderless";
        else if (arg == "--1080p") { config.width = 1920; config.height = 1080; }
        else if (arg == "--sources") data.song_select.showing_sources = true;
        else if (arg == "--records") data.song_select.showing_records = true;
        else {
            std::cerr << "Unknown preview argument: " << arg << '\n';
            return 2;
        }
    }
    if (!keys_explicit && (keymap_settings || skin_settings)) preview_keys = 4;
    if (pressed_lane < 0 || pressed_lane > preview_keys) return 2;
    // Match the client localization path. Keep chart metadata, player names,
    // asset paths and renderer status tokens unchanged: those are fixture data.
    const auto loc = [&](std::string_view english, std::string_view korean) {
        return tenriff::ui::text(data.ui_language, english, korean);
    };
    const std::string preview_backend = loc("Input backend: ", "입력 백엔드: ") + "Preview";
    data.account_overlay.visible = sites_account;
    data.account_overlay.sites_mode = sites_account;
    data.account_overlay.sites_connected = sites_connected;
    data.account_overlay.sites_url = "https://tenriff-leaderboard.lastestarcorp.chatgpt.site";
    data.account_overlay.sites_status = sites_connected
        ? loc("Connection imported and protected. You can play now.",
              "연결 정보를 암호화해 저장했습니다. 이제 플레이하면 됩니다.")
        : loc("Open the website, sign in, and copy your connection information.",
              "웹사이트에서 로그인한 뒤 연결 정보를 복사해 주세요.");
    data.kind = result ? MenuScreenKind::ResultScreen : MenuScreenKind::SongSelect;
    if (settings) {
        data.kind = MenuScreenKind::GenericList;
        data.generic.heading = skin_settings ? loc("Skin Settings", "스킨 설정")
                                             : loc("Mode Settings", "모드 설정");
        data.generic.footer_notes = {preview_backend,
            loc("VIEW ONLY", "보기 전용") + " / " + loc("OFFLINE", "오프라인")};
        auto& preview = data.generic.skin_preview;
        preview.visible = skin_settings;
        preview.note_height_scale = preview_note_height;
        preview.lane_count = preview_keys;
        preview.mode_label = std::to_string(preview_keys) + "K";
        preview.gameplay_field_offset_x = field_offset;
        if (moved_labels) {
            preview.judgement_position = 0.4; preview.judgement_offset_x = -120;
            preview.combo_position = 0.5; preview.combo_offset_x = 120;
        }
        preview.selected_lane = fixture_idle ? 0 : pressed_lane;
        preview.selected_color_label = "#EDF2F7";
        for (int i = 0; i < preview_keys; ++i) {
            preview.lane_colors[i] = i % 2 ? 0x4B76EF : 0xEDF2F7;
            preview.key_labels[i] = std::to_string(i + 1);
        }
    }
    if (settings && !skin_settings) {
        using namespace tenriff::app::menu::settings;
        tenriff::config::RuntimeConfig runtime;
        runtime.audio.backend = tenriff::audio::AudioBackend::ASIO;
        runtime.audio.asio_driver = "fixture-a";
        runtime.audio.sample_rate = 44100;
        runtime.audio.frames_per_buffer = 320;
        runtime.audio_ui.mute_when_inactive = mute_inactive;
        runtime.audio_ui.title_music = tenriff::config::normalize_title_music_token(title_music_mode);
        AudioSettingsController controller;
        controller.set_asio_drivers({{"fixture-a", "Preview ASIO Device A"}, {"fixture-b", "Preview ASIO Device B"}});
        controller.reset(AudioSettingId::KeysoundMode);
        auto view = AudioSettingsView::build(controller, runtime, data.ui_language);
        data.generic.heading = loc("Audio Settings", "오디오 설정");
        data.generic.rows.clear();
        data.generic.notes = std::move(view.notes);
        for (auto& source : view.rows) {
            MenuRowData row;
            row.category = std::move(source.category); row.label = std::move(source.label); row.value = std::move(source.value);
            row.selected = source.selected; row.activatable = source.activatable; row.adjustable = source.adjustable;
            row.increment_enabled = row.decrement_enabled = source.adjustable;
            row.slider = source.slider_ratio.has_value(); row.slider_ratio = source.slider_ratio.value_or(0.0);
            row.target_kind = MenuHitTargetKind::SettingsRow; row.row_index = static_cast<int>(source.id);
            data.generic.rows.push_back(std::move(row));
        }
    }
    if (keymap_settings) {
        using namespace tenriff::app::menu::settings;
        tenriff::config::KeymapManager manager;
        auto working = manager.default_keymap();
        KeymapSettingsController controller;
        controller.reset(preview_keys, std::to_string(preview_keys) + "k");
        // A secondary assignment is synthetic; the view and input labels are real.
        if (!controller.lane_ids().empty())
            working.secondary_mode_bindings[std::string(controller.edit_mode())][controller.lane_ids().front()] = "Space";
        std::unordered_set<std::uint32_t> pressed;
        if (keymap_test) {
            const auto bindings = manager.bindings_for_mode(working, controller.edit_mode());
            if (pressed_lane > 0 && pressed_lane <= static_cast<int>(controller.lane_ids().size())) {
                const auto found = bindings.find(controller.lane_ids()[pressed_lane - 1]);
                if (found != bindings.end()) if (const auto code = tenriff::config::KeycodeMap::to_keycode(found->second)) pressed.insert(*code);
            }
        }
        auto view = keymap_test
            ? KeymapSettingsView::build_nkro_test(controller, working, pressed, preview_backend, data.ui_language)
            : KeymapSettingsView::build(controller, working, preview_keys, std::to_string(preview_keys) + "k",
                                        preview_backend, tenriff::timing::HighResClock::now_ns(), data.ui_language);
        data.kind = MenuScreenKind::GenericList;
        data.generic.heading = keymap_test ? "NKRO Test" : loc("Keymap", "키 설정");
        data.generic.rows.clear(); data.generic.notes.clear();
        data.generic.keymap_keyboard = true; data.generic.keymap_test = keymap_test;
        data.generic.footer_reserved_lines = view.footer_reserved_lines;
        data.generic.footer_notes = std::move(view.footer_notes);
        int index = 0;
        for (auto& source : view.rows) {
            MenuRowData row;
            row.label = std::move(source.label); row.value = std::move(source.value); row.secondary_value = std::move(source.secondary_value);
            row.selected = source.selected; row.activatable = !keymap_test || source.action == KeymapActionId::Back;
            row.target_kind = source.action ? MenuHitTargetKind::KeymapButton : (keymap_test ? MenuHitTargetKind::None : MenuHitTargetKind::SettingsRow);
            row.row_index = source.action ? static_cast<int>(*source.action) : index;
            if (keymap_test && source.action) row.target_kind = MenuHitTargetKind::SettingsRow;
            if (!keymap_test && index == 0) row.adjustable = row.increment_enabled = row.decrement_enabled = true;
            data.generic.rows.push_back(std::move(row)); ++index;
        }

    }
    if (profile_settings) {
        data.kind = MenuScreenKind::GenericList;
        data.generic.heading = loc("Profile Setup", "프로필 설정");
        data.generic.profile_preview_visible = true;
        data.generic.profile_avatar_path = avatar_path;
        const std::array<std::string, 12> labels{
            loc("Language", "언어"), loc("Menu Font Size", "메뉴 글자 크기"),
            loc("Songs Folder", "곡 폴더"), loc("Gauge", "게이지"), "Rate",
            loc("Visual Latency", "비주얼 레이턴시"), loc("Keysound", "키음"),
            loc("Input Backend", "입력 방식"), loc("Nickname", "닉네임"),
            loc("Avatar Image", "프로필 사진"), loc("Clear Avatar", "사진 지우기"),
            loc("Done", "완료")};
        const std::array<std::string, 12> values{
            data.ui_language == tenriff::ui::Language::Japanese ? "日本語" : loc("English", "한국어"),
            data.ui_text_scale > 1.2f ? loc("Extra Large", "더 크게") :
                data.ui_text_scale > 1.0f ? loc("Large", "크게") : loc("Normal", "보통"),
            "ALL SONG", "NORMAL", "1.00x", "0 ms", "Follow", "RawInput", "PLAYER",
            avatar_path.empty() ? loc("Not set", "설정 안 됨") : "preview-avatar.png",
            loc("Clear", "지우기"), loc("Back", "돌아가기")};
        for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
            MenuRowData row;
            row.label = labels[i]; row.value = values[i]; row.row_index = i;
            row.target_kind = MenuHitTargetKind::SettingsRow;
            row.selected = i == 1; row.adjustable = i < 8;
            row.increment_enabled = row.decrement_enabled = true;
            data.generic.rows.push_back(std::move(row));
        }
        data.generic.notes = {
            loc("Language and menu font size apply immediately and are saved to this profile.",
                "언어와 메뉴 글자 크기는 즉시 적용되고 이 프로필에 저장됩니다."),
            loc("Nickname is shown in saved records and multiplayer. Avatar Image accepts local PNG/JPG files.",
                "닉네임은 저장 기록과 멀티플레이에 표시됩니다. 프로필 사진은 로컬 PNG/JPG 파일을 사용합니다.")};
    }
    if (title) {
        data.kind = MenuScreenKind::TitleMenu;
        data.title.profile = "PLAYER";
        data.title.profile_avatar_path = avatar_path;
        data.title.track = empty ? "" : "Luminous Horizon [Another]";
        data.title.buttons = {
            {empty ? loc("ADD SONGS FOLDER", "곡 폴더 추가") : loc("PLAY", "플레이"), "+", !focus_options,
             empty ? loc("Choose a folder containing your BMS charts.", "BMS 차트가 있는 폴더를 선택합니다.")
                   : loc("Open your library and choose a track.", "라이브러리에서 플레이할 곡을 고릅니다.")},
            {loc("MULTIPLAYER", "멀티플레이"), "P2P", false,
             loc("Find a room or host a session with friends.", "방을 찾거나 친구들과 새 세션을 엽니다.")},
            {loc("OPTIONS", "옵션"), "\xE2\x9A\x99", focus_options,
             loc("Adjust audio, input, graphics and skins.", "오디오, 입력, 그래픽과 스킨을 설정합니다.")},
            {loc("EXIT", "종료"), "\xE2\x8F\xBB", false, loc("Close TenRiff.", "TenRiff를 종료합니다.")},
        };
        data.title.guides = {
            loc("UP / DOWN or mouse to move", "위아래 키 또는 마우스로 이동"),
            loc("ENTER or double-click to open", "ENTER 또는 더블클릭으로 열기"),
            empty ? loc("PLAY becomes Add Songs Folder until a library is indexed", "라이브러리가 없으면 곡 폴더 추가를 표시합니다")
                  : loc("F2 selects a songs folder; -/+ adjusts Rate", "F2 곡 폴더 선택 / -/+ 배속 조절"),
            loc("F5 refreshes the current song source", "F5 현재 곡 소스 새로고침"),
            loc("F1 opens the control help overlay", "F1 조작 도움말 열기"),
            loc("ESC exits from the title menu", "ESC 타이틀에서 종료"), preview_backend};
    }
    auto& songs = data.song_select;
    songs.profile = "PLAYER";
    songs.profile_avatar_path = avatar_path;
    songs.selected_song_title = "Luminous Horizon / A very long chart title [Another]";
    songs.selected_song_artist = "TenRiff UI Preview";
    songs.selected_song_key_count = menu_keys;
    songs.selected_song_layout = std::to_string(menu_keys) + " KEYS";
    songs.selected_song_difficulty = "12";
    songs.selected_song_bpm = 180;
    songs.selected_song_note_count = 1824;
    songs.selected_song_nps_min = 3;
    songs.selected_song_nps_median = 10;
    songs.selected_song_nps_max = 24;
    songs.selected_song_chart_name = "ANOTHER";
    songs.current_gauge = loc("Normal", "노말");
    songs.current_hi_speed = "3.50";
    songs.current_visual_latency = "+0 ms";
    songs.current_random = loc("Off", "꺼짐");
    songs.difficulty_table_name = empty ? loc("Native LV", "기본 LV") : "発狂BMS難易度表";
    songs.difficulty_table_active = !empty;
    songs.difficulty_table_editing = table_editor;
    songs.difficulty_table_url_input = "https://example.com/table/header.json";
    songs.sort_summary = loc("LV ASC", "LV 오름");
    songs.group_summary = loc("NONE", "없음");
    songs.primary_hint = loc("UP/DOWN  MOVE     ENTER / dbl-click  PLAY     I  EDIT BMS     C  ADD COURSE",
                             "위아래 이동 / ENTER·더블클릭 플레이 / I BMS 편집 / C 코스 추가");
    songs.secondary_hint = loc("LEFT/RIGHT NAV     BACKSPACE BACK     F2 FOLDER     -/+ RATE     F5 REINDEX     F1 HELP",
                               "좌우 이동 / BACKSPACE 뒤로 / F2 폴더 / -/+ 배속 / F5 재검색 / F1 도움말");
    songs.empty_title = loc("NO CHARTS MATCH", "일치하는 차트 없음");
    songs.empty_message = loc("Clear the current search/filter or switch the active source to see more charts.",
                              "검색·필터를 지우거나 활성 소스를 바꿔 더 많은 차트를 확인하세요.");
    songs.result_available = !empty;
    songs.search_active = search_active;
    songs.search_query = search_query;
    songs.rank = "AAA";
    songs.best_score = 9452;
    songs.detail_score = 8901;
    songs.max_detail_score = 9120;
    songs.max_combo = 742;
    songs.accuracy = 97.82;
    songs.detailed_accuracy = 97.65;
    songs.selected_record_status = loc("CLEAR", "클리어");
    songs.selected_record_created_utc = "2026-09-05 09:00";
    songs.selected_source_name = loc("SONG LIBRARY", "곡 라이브러리");
    songs.selected_source_path = "songs / preview";
    const std::array<std::string, 7> navigation{
        loc("SONGS", "곡 목록"), loc("SOURCES", "소스"), loc("SEARCH", "검색"),
        loc("FILTER", "필터"), loc("RECORDS", "기록"), loc("SESSION MIX", "세션 믹스"),
        loc("OPTIONS", "옵션")};
    for (const auto& label : navigation) {
        MenuButtonData button;
        button.label = label;
        songs.left_nav.push_back(button);
    }
    const char* titles[] = {"Luminous Horizon / A very long chart title [Another]", "Blue Hour", "Afterglow",
                            "Orbit", "Midnight Transit", "Parallel Lines", "First Light"};
    if (!empty) {
        for (int i = 0; i < 7; ++i) {
            SongCardData card;
            card.title = titles[i];
            card.artist = "TenRiff UI Preview";
            card.detail = "7K  /  ANOTHER  /  180 BPM";
            card.level = 12 + i;
            card.level_label = "⑤" + loc("LEVEL", "레벨") + " " + std::to_string(12 + i);
            card.song_index = i;
            card.selected = i == 0;
            card.favorite = i == 1;
            card.lamp = "CLEAR";
            songs.songs.push_back(card);
        }
        songs.song_count = songs.list_total_count = 42;
        songs.record_count = songs.source_count = 42;
        songs.list_visible_count = 7;
    }
    if (songs.showing_sources && !empty) {
        songs.selected_source_all = true;
        songs.selected_source_name = "ALL SONG";
        songs.selected_source_path = loc("All registered song folders", "등록한 모든 곡 폴더");
        songs.selected_source_song_count = 42;
        songs.source_count = 6;
        songs.songs.front().title = "ALL SONG";
        songs.songs.front().artist = songs.selected_source_path;
        songs.songs.front().detail = loc("Combined library · duplicates removed", "통합 라이브러리 · 중복 제외");
        songs.songs.front().level = 42;
    }
    auto& score = data.result;
    score.title = songs.selected_song_title;
    score.artist = songs.selected_song_artist;
    score.profile = "PLAYER";
    score.key_count = menu_keys;
    score.level = 12;
    score.bpm = 180;
    score.rank = failed ? "C" : "AAA";
    score.status = failed ? "FAILED" : "CLEAR";
    score.cleared = !failed;
    score.score = failed ? 4230 : 9452;
    score.detail_score = 8901;
    score.max_detail_score = 9120;
    score.accuracy = songs.accuracy;
    score.detailed_accuracy = songs.detailed_accuracy;
    score.max_combo = 742;
    score.total_notes = score.judged_notes = 1824;
    score.perfect = 1620;
    score.great = 164;
    score.good = 32;
    score.bad = 8;
    score.poor = 0;
    score.mean_delta_ms = 2.4;
    score.stddev_delta_ms = 14.6;
    score.fast_count = 824;
    score.slow_count = 992;
    score.replay_available = !failed;
    score.notes = {loc("VIEW ONLY", "보기 전용") + " / " + loc("OFFLINE", "오프라인"),
                   preview_backend};
    for (int i = 0; i <= 80; ++i) {
        const float position = static_cast<float>(i) / 80.0f;
        score.gauge_points.push_back({position, failed ? 0.7f * (1.0f - position)
            : 0.65f + 0.17f * std::sin(position * 18.0f)});
    }
    if (options_grid) {
        data.kind = MenuScreenKind::GenericList;
        data.generic = {};
        data.generic.heading = loc("Options", "옵션");
        data.generic.card_grid = true;
        const std::array<std::string, 10> labels{
            loc("KEY MODE", "키 모드"), loc("KEYMAP", "키 설정"), loc("SKINS", "스킨"),
            loc("GRAPHICS", "그래픽"), loc("AUDIO", "오디오"), loc("INPUT", "입력"),
            loc("CALIBRATION", "레이턴시"), loc("PROFILE", "프로필"),
            loc("MODS", "모드 설정"), loc("KEY TEST", "키 입력 테스트")};
        const std::array<std::string, 10> values{
            "4K", loc("Configure", "설정"), "LR2", loc("Borderless", "테두리 없음"),
            loc("High", "고성능"), "RawInput", "-43.0 ms", "default",
            loc("Configure", "설정"), loc("Test", "테스트")};
        const std::array<std::string, 10> descriptions{
            loc("Choose the play key mode. The current mode is shown prominently on this first card.",
                "플레이 키 모드를 선택합니다. 현재 모드는 첫 카드에 크게 표시됩니다."),
            loc("Assign gameplay keys and test the current mapping.", "게임 키를 지정하고 현재 키 배치를 테스트합니다."),
            loc("Import and tune TenRiff or LR2 skins, notes, LN colour, and hit bursts.",
                "TenRiff·LR2 스킨과 노트, 롱노트 색, 키 폭발을 설정합니다."),
            loc("Set display mode, resolution, frame timing, and background upscaling.",
                "화면 모드, 해상도, 프레임 타이밍, 배경 업스케일을 설정합니다."),
            loc("Set keysound policy, volumes, and audio preset.", "키음 정책, 음량, 오디오 프리셋을 설정합니다."),
            loc("Set input backend, polling, judgement rate, and debounce.",
                "입력 백엔드, 폴링, 판정 주기, 디바운스를 설정합니다."),
            loc("Calibrate audio and visual timing. Visual latency changes in 1 ms steps.",
                "오디오·비주얼 타이밍을 보정합니다. 비주얼 레이턴시는 1ms씩 조절됩니다."),
            loc("Change profile name, avatar, and device setup.", "프로필 이름, 아바타, 장치 설정을 변경합니다."),
            loc("Choose gameplay modifiers and review the score multiplier.", "플레이 모드와 점수 배율을 확인합니다."),
            loc("Check simultaneous key presses with the current key mapping.", "현재 키 배치로 동시 입력을 확인합니다.")};
        for (int i = 0; i < 10; ++i) {
            MenuRowData row;
            row.label = labels[i]; row.value = values[i]; row.row_index = i;
            row.selected = i == 0; row.activatable = true; row.target_kind = MenuHitTargetKind::OptionsItem;
            data.generic.rows.push_back(row);
            data.generic.card_descriptions.push_back(descriptions[static_cast<std::size_t>(i)]);
        }
    }
    if (options_grid || settings || keymap_settings || profile_settings) {
        const auto tips = tenriff::app::settings_help_tips(data.ui_language);
        data.generic.notes.insert(data.generic.notes.begin(), tips.begin(), tips.end());
    }
    for (int i = 0; i < players; ++i) {
        MultiplayerPlayerData player;
        player.player_id = static_cast<uint8_t>(i + 1); player.rank = i + 1;
        player.name = i == 1 ? "GOMazk / 긴 플레이어 이름" : "PLAYER " + std::to_string(i + 1);
        player.local = i == 0; player.has_score = true; player.finished = result;
        player.score = 9500 - i * 640; player.combo = 123 + i; player.max_combo = 456;
        player.perfect = 620; player.great = 80; player.good = 12; player.bad = 3;
        player.gauge = 90 - i * 10;
        data.result.multiplayer_players.push_back(player);
        data.gameplay.multiplayer_players.push_back(player);
    }
    if (players > 0 && result) data.result.peer_battle = true;
    if (gameplay) {
        data.kind = MenuScreenKind::GameplayHud;
        auto& hud = data.gameplay;
        hud.note_height_scale = preview_note_height;
        hud.gameplay_field_offset_x = field_offset;
        hud.active = true; hud.title = loc("Gameplay", "게임플레이") + " / " + loc("LIVE PREVIEW", "미리보기");
        hud.artist = loc("VIEW ONLY", "보기 전용") + " / " + loc("OFFLINE", "오프라인");
        hud.visual_velocity = 1.0 / 48000.0;
        if (moved_labels) {
            hud.judgement_position = 0.4; hud.judgement_offset_x = -120;
            hud.combo_position = 0.5; hud.combo_offset_x = 120;
        }
        hud.lane_count = preview_keys; hud.black_playfield_enabled = true; hud.gauge = 75; hud.gauge_label = "NORMAL";
        hud.lookahead_samples = 48000; hud.past_samples = 4800; hud.duration_samples = 48000 * 600;
        hud.lane_activity_count = hud.lane_pressed_count = static_cast<std::size_t>(preview_keys);
        hud.lane_color_count = hud.key_label_count = static_cast<std::size_t>(preview_keys);
        const auto palette = tenriff::config::default_skin_lane_colors(std::to_string(preview_keys) + "k");
        const std::array<const char*,16> labels{"S","D","F","G","H","J","K","L","Q","W","E","R","U","I","O","P"};
        for (int lane = 0; lane < preview_keys; ++lane) {
            hud.lane_colors[lane] = tenriff::config::skin_color_rgb(palette[lane]);
            hud.key_labels[lane] = labels[lane];
        }
        hud.artist = "LUMA KEYS / " + std::to_string(preview_keys) + "K / SYNTHETIC PREVIEW";
        hud.has_feedback = true; hud.feedback = "PG"; hud.combo = 123;
        hud.peer_visible = players > 0; hud.peer_score_available = players > 0;
        hud.peer_score = 8860; hud.score = 9500;
        const auto lead = tenriff::app::peer_battle_score_lead(hud.score, hud.peer_score);
        hud.versus_score_difference = lead.difference; hud.versus_score_position = lead.position;
        hud.timing_history_count = 3;
        hud.timing_history_delta_ms[0] = -12; hud.timing_history_delta_ms[1] = 9; hud.timing_history_delta_ms[2] = 28;
    }
    if (!skin_folder.empty()) {
        const auto skin = tenriff::app::load_tenriff_skin_folder(skin_folder, preview_keys);
        if (!skin.found || !skin.warnings.empty()) {
            for (const auto& warning : skin.warnings) std::cerr << warning << '\n';
            return 3;
        }
        fixture_skin = skin;
        const auto resolved=std::make_shared<const tenriff::app::ImportedGameplaySkinDefinition>(skin.gameplay);
        // Exercise the same resolved manifest passed by MenuApp to gameplay and Skin Settings.
        auto apply_gameplay_skin = [&](auto& hud) {
            hud.resolved_tenriff_skin=resolved;
            hud.skin_source=skin.native_gameplay_fallback?"native":"tenriff";
            hud.skin_revision=1;
            hud.external_skin_root=skin.root_path;
            hud.external_skin_name=skin.name;
            const auto& style=skin.gameplay_style;
            auto assign=[](auto& destination,const auto& value) { if(value) destination=*value; };
            assign(hud.show_lane_dividers,style.show_lane_dividers);
            assign(hud.show_judgement_line,style.show_judgement_line);
            assign(hud.show_gear_boundary_line,style.show_gear_boundary_line);
            assign(hud.show_hold_tail,style.show_hold_tail);
            assign(hud.hold_tail_taper_enabled,style.hold_tail_taper_enabled);
            assign(hud.judgement_line_glow_enabled,style.judgement_line_glow_enabled);
            assign(hud.key_pulse_enabled,style.key_pulse_enabled);
            assign(hud.note_border_enabled,style.note_border_enabled);
            assign(hud.black_playfield_enabled,style.black_playfield_enabled);
            assign(hud.key_pulse_brightness,style.key_pulse_brightness);
            assign(hud.key_backdrop_enabled,style.key_backdrop_enabled);
            assign(hud.key_backdrop_opacity,style.key_backdrop_opacity);
            assign(hud.key_backdrop_brightness,style.key_backdrop_brightness);
            assign(hud.key_backdrop_height,style.key_backdrop_height);
            assign(hud.lane_background_opacity,style.lane_background_opacity);
            assign(hud.visual_opacity,style.visual_opacity);
            assign(hud.note_outline_opacity,style.note_outline_opacity);
            assign(hud.hold_body_opacity,style.hold_body_opacity);
            assign(hud.hit_burst_style,style.hit_burst_style);
            assign(hud.key_label_position,style.key_label_position);
            assign(hud.note_shape,style.note_shape);
            if(skin.gameplay.native_renderer && skin.gameplay.has_hit_position)
                hud.judgement_line_position=skin.gameplay.hit_position/480.0;
            if(!style.lane_colors.empty()) {
                for(int lane=0;lane<preview_keys;++lane)
                    hud.lane_colors[lane]=style.lane_colors[std::min(static_cast<std::size_t>(lane),style.lane_colors.size()-1)];
            }
        };
        apply_gameplay_skin(data.gameplay);
        apply_gameplay_skin(data.generic.skin_preview);
        if(skin.gameplay_style.show_timing_feedback) data.gameplay.show_timing_feedback=*skin.gameplay_style.show_timing_feedback;
        data.gameplay.skin_background_path=skin.gameplay_background_path;
        data.gameplay.skin_background_opacity=skin.gameplay_background_opacity;
        data.lobby_skin.enabled = true;
        data.lobby_skin.revision = 1;
        data.lobby_skin.native_menu_renderer = skin.native_menu_renderer;
        data.lobby_skin.native_menu = skin.native_menu;
        data.lobby_skin.background_path = skin.lobby_background_path;
        data.lobby_skin.logo_path = skin.lobby_logo_path;
        data.lobby_skin.theme_colors = skin.theme_colors;
        data.lobby_skin.referenced_asset_paths = skin.referenced_asset_paths;
        for (const auto& item : skin.layout_rects) {
            const auto& r = item.second;
            data.lobby_skin.layout_rects[item.first] = {r.left, r.top, r.right, r.bottom};
        }
    }
    data.gameplay.combo_font_scale = data.generic.skin_preview.combo_font_scale = combo_font_scale;
    data.gameplay.note_width_scale = data.generic.skin_preview.note_width_scale = preview_note_width;
    data.gameplay.judgement_font_scale = data.generic.skin_preview.judgement_font_scale = judgement_font_scale;
    if (backdrop_off) data.gameplay.key_backdrop_enabled = false;
    if (backdrop_opacity >= 0.0) data.gameplay.key_backdrop_opacity = backdrop_opacity;
    if (backdrop_brightness >= 0.0) data.gameplay.key_backdrop_brightness = backdrop_brightness;
    if (backdrop_height >= 0.0) data.gameplay.key_backdrop_height = backdrop_height;
    if (backdrop_off) data.generic.skin_preview.key_backdrop_enabled = false;
    if (backdrop_opacity >= 0.0) data.generic.skin_preview.key_backdrop_opacity = backdrop_opacity;
    if (backdrop_brightness >= 0.0) data.generic.skin_preview.key_backdrop_brightness = backdrop_brightness;
    if (backdrop_height >= 0.0) data.generic.skin_preview.key_backdrop_height = backdrop_height;
    if (skin_settings) {
        tenriff::app::MenuAppVisualTestAccess::populate_skin_rows(
            data, fixture_skin ? &*fixture_skin : nullptr, selected_row,
            backdrop_off || backdrop_opacity >= 0.0 || backdrop_brightness >= 0.0 || backdrop_height >= 0.0);
    }
    if (selected_row >= 0) {
        for (std::size_t i = 0; i < data.generic.rows.size(); ++i)
            data.generic.rows[i].selected = static_cast<int>(i) == selected_row;
    }
    if (selected_setting >= 0) {
        for (auto& row : data.generic.rows) row.selected = row.row_index == selected_setting;
    }
    if (skin_scene) {
        data.gameplay = make_skin_gameplay_preview(data.generic.skin_preview);
        data.kind = MenuScreenKind::GameplayHud;
    }
    if (ghost) {
        data.gameplay.ghost_visible = true;
        data.gameplay.paused = ghost_paused;
        data.gameplay.max_combo = 12345;
        data.gameplay.accuracy = 99.98;
        data.gameplay.detailed_accuracy = 98.76;
    }
    if (bms_editor) {
        data.kind = MenuScreenKind::BmsEditor;
        auto& editor = data.bms_editor;
        editor.title = "READABILITY / SYNTHETIC CHART";
        editor.artist = "TenRiff UI Preview";
        editor.path = "examples/preview.bms";
        editor.status = loc("Preview only", "미리보기 전용");
        editor.lane_count = 4; editor.measure_count = 16; editor.base_bpm = 180;
        for (int lane = 1; lane <= 4; ++lane) {
            BmsEditorNoteData note;
            note.id = lane; note.lane = lane; note.measure = lane;
            note.token = "01"; editor.notes.push_back(note);
        }
        BmsEditorMarkerData marker;
        marker.kind = "BPM"; marker.value = 180; marker.label = "BPM 180";
        editor.markers.push_back(marker);
    }
    if (help_overlay) {
        data.help_overlay.visible = true;
        data.help_overlay.title = loc("Controls and settings", "조작과 설정");
        data.help_overlay.lines = {
            loc("ALL SONG joins registered folders; difficulty tables filter matching charts.",
                "ALL SONG은 등록 폴더를 합치며 난이도표에 해당하는 차트를 표시합니다."),
            loc("Skin Settings previews actual gameplay proportions.", "스킨 설정은 실제 인게임 비율을 미리 보여줍니다.")};
        data.help_overlay.footer = loc("ESC / F1  Close", "ESC / F1 닫기");
    }
    if (chat_overlay) {
        data.chat_overlay.visible = true; data.chat_overlay.connected = true;
        data.chat_overlay.title = loc("Chat preview", "채팅 미리보기");
        data.chat_overlay.messages = {"PLAYER: Readable UI text", "PLAYER 2: 밝은 배경에서도 확인합니다."};
        data.chat_overlay.input = loc("Synthetic message", "합성 메시지");
        data.chat_overlay.status = loc("Preview only", "미리보기 전용");
        data.chat_overlay.hint = loc("ENTER Send / ESC Close", "ENTER 보내기 / ESC 닫기");
    }
    if (url_warning) {
        data.url_warning_overlay.visible = true;
        data.url_warning_overlay.url = "https://example.com/preview";
    }
    data.gameplay.background_base_path = bga_path;
    data.gameplay.background_overlay_path = bga_overlay_path;
    if (opaque_field) data.gameplay.black_playfield_enabled = true;
    MenuWindow window;
    window.set_config(config);
    const auto start = std::chrono::steady_clock::now();
    while (!window.should_close()) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) window.request_close();
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (score.presentation_start_ns == 0 && reveal) {
            score.presentation_start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        if (gameplay) {
            const double seconds = fixture_seconds >= 0.0 ? fixture_seconds
                : std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            auto& hud = data.gameplay;
            const int hit = static_cast<int>(seconds * 2);
            const int grade = fixed_grade >= 0 ? fixed_grade : (hit / 4) % 3;
            hud.feedback = grade == 0 ? "PG" : grade == 1 ? "GR" : "G";
            hud.feedback_delta_ms = hit % 2 == 0 ? -28.0 : 28.0;
            hud.text_revision = static_cast<uint64_t>(hit + 1);
            hud.motion_revision++;
            hud.combo = 123 + hit; hud.pg = hit + 1;
            hud.current_sample = static_cast<int64_t>(seconds * 48000);
            hud.current_visual_position = seconds;
            hud.audio_sample_time_ns = hud.activity_publish_time_ns = tenriff::timing::HighResClock::now_ns();
            hud.lane_activity.fill(0.0f);
            hud.lane_pressed.fill(0);
            const int struck_lane = hit % preview_keys;
            hud.lane_activity[static_cast<std::size_t>(struck_lane)] = fixture_idle ? 0.0f
                : static_cast<float>(std::max(0.0, 1.0 - std::fmod(seconds, 0.5) * 5.0));
            hud.lane_pressed[static_cast<std::size_t>(struck_lane)] = !fixture_idle && std::fmod(seconds, 0.5) < 0.18;
            // A held key and a pending LN exercise the two distinct visual states.
            const int held_lane = (struck_lane + 2) % preview_keys;
            hud.lane_pressed[static_cast<std::size_t>(held_lane)] = !fixture_idle;
            hud.note_count = 0;
            for (int lane = 1; lane <= preview_keys; ++lane) {
                GameplayNoteData note;
                note.lane = lane;
                note.start_sample = hud.current_sample + static_cast<int64_t>((0.1 + std::fmod(lane * 0.13 + seconds * 0.5, 1.0)) * 40000);
                note.visual_position = note.start_sample / 48000.0;
                note.tail_visual_position = note.visual_position;
                if (lane % 3 == 0) {
                    note.hold = true;
                    note.tail_sample = note.start_sample + 14000;
                    note.tail_visual_position = note.tail_sample / 48000.0;
                }
                hud.notes[hud.note_count++] = note;
                if (lane == held_lane + 1 && !fixture_idle) {
                    note.hold = true; note.head_visible = false;
                    note.start_sample = hud.current_sample - 4800;
                    note.visual_position = note.start_sample / 48000.0;
                    note.tail_sample = hud.current_sample + 18000;
                    note.tail_visual_position = note.tail_sample / 48000.0;
                    hud.notes[hud.note_count++] = note;
                }
            }
            if (ghost) {
                // Identical note/key activity makes visual parity measurable;
                // independent score labels still exercise the split info cards.
                hud.ghost_score = hud.score + 123;
                hud.ghost_combo = hud.combo;
                hud.ghost_max_combo = hud.max_combo;
                hud.ghost_accuracy = hud.accuracy;
                hud.ghost_detailed_accuracy = hud.detailed_accuracy;
                hud.ghost_pg = hud.pg; hud.ghost_gr = 23456;
                hud.ghost_gd = 3456; hud.ghost_bd = 456; hud.ghost_pr = 56;
                hud.ghost_gauge = hud.gauge; hud.ghost_gauge_label = hud.gauge_label;
                hud.ghost_has_feedback = !no_feedback;
                hud.ghost_feedback = hud.feedback;
                hud.ghost_feedback_delta_ms = hud.feedback_delta_ms;
                hud.ghost_timing_history_count = hud.timing_history_count;
                hud.ghost_timing_history_delta_ms = hud.timing_history_delta_ms;
                hud.ghost_lane_activity_count = hud.lane_activity_count;
                hud.ghost_lane_activity = hud.lane_activity;
                hud.ghost_lane_pressed_count = hud.lane_pressed_count;
                hud.ghost_lane_pressed = hud.lane_pressed;
                hud.ghost_note_count = hud.note_count;
                hud.ghost_notes = hud.notes;
            }
            if (no_feedback) hud.has_feedback = false;
        }
        ++rendered_frames;
        if (cycle_font_scale && rendered_frames == 40) data.ui_text_scale = 1.4f;
        if (rendered_frames == 35 && !bga_next_path.empty())
            data.gameplay.background_base_path = bga_next_path;
        if (avatar_refresh_frame > 0 && rendered_frames == avatar_refresh_frame) ++data.profile_avatar_revision;
        if (cycle_selection) {
            const int selection = (rendered_frames / 45);
            for (std::size_t i = 0; i < data.title.buttons.size(); ++i)
                data.title.buttons[i].selected = static_cast<int>(i) == selection % data.title.buttons.size();
            for (std::size_t i = 0; i < data.generic.rows.size(); ++i)
                data.generic.rows[i].selected = static_cast<int>(i) == selection % data.generic.rows.size();
            for (std::size_t i = 0; i < songs.songs.size(); ++i)
                songs.songs[i].selected = static_cast<int>(i) == selection % songs.songs.size();
            if (!songs.songs.empty()) {
                songs.list_selected_index = selection % static_cast<int>(songs.songs.size());
                songs.selected_song_title = songs.songs[songs.list_selected_index].title;
            }
        }
        if (std::find(capture_frames.begin(), capture_frames.end(), rendered_frames) != capture_frames.end()) {
            window.request_screenshot();
            std::cout << "capture frame=" << rendered_frames << '\n';
        }
        if (capture_once && !capture_requested && rendered_frames >= 30) {
            window.request_screenshot();
            capture_requested = true;
        }
        window.render(data);
        if (!hitmap_written && rendered_frames >= 5 && (verify_hits || !hitmap_path.empty())) {
            if (!MenuWindowVisualTestAccess::verify_and_write(window, data, hitmap_path, verify_hits)) {
                window.shutdown(); return 4;
            }
            hitmap_written = true;
        }
        if (frame_limit > 0 && rendered_frames >= frame_limit) break;
        if (capture_once && rendered_frames >= 33) break;
        while (const auto click = window.poll_click_event()) {
            std::cout << "hit kind=" << static_cast<int>(click->kind)
                      << " index=" << click->index << std::endl;
            if (click->kind == MenuHitTargetKind::SongDifficultyTable) {
                const auto action = static_cast<SongDifficultyTableAction>(click->index);
                if (action == SongDifficultyTableAction::EditUrl) songs.difficulty_table_editing = true;
                if (action == SongDifficultyTableAction::Cancel) songs.difficulty_table_editing = false;
                if (action == SongDifficultyTableAction::Reset) { songs.difficulty_table_active = false; songs.difficulty_table_name = loc("Native LV", "기본 LV"); }
                if (action == SongDifficultyTableAction::LocalFile) songs.difficulty_table_status = loc("FILE", "파일") + " / " + loc("VIEW ONLY", "보기 전용");
                if (action == SongDifficultyTableAction::Apply) songs.difficulty_table_status = loc("APPLY", "적용") + " / " + loc("OFFLINE", "오프라인");
            }
            if (options_grid && click->kind == MenuHitTargetKind::OptionsItem) {
                for (auto& row : data.generic.rows) row.selected = row.row_index == click->index;
            }
        }
        if (window.had_fatal_error()) return 1;
        if (std::chrono::steady_clock::now() - start > std::chrono::minutes(10)) break;
        std::this_thread::sleep_until(start + std::chrono::nanoseconds(
            static_cast<int64_t>((std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() * preview_fps + 1.0)) * (1000000000 / preview_fps)));
    }
    window.shutdown();
}
