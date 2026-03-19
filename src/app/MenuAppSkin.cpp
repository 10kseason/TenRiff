#include "app/MenuApp.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>
#endif

#include "app/Lr2Skin.h"
#include "app/MenuAppSkinUtils.h"
#include "app/MenuSongUtils.h"
#include "app/OsuSkin.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

namespace fs = std::filesystem;

constexpr double kJudgementLinePositionStep = 0.01;
constexpr double kComboPositionStep = 0.02;
constexpr double kNoteSizeScaleStep = 0.05;
constexpr double kLaneDividerScaleStep = 0.05;

fs::path path_from_utf8(std::string_view value) {
    try {
        return util::path_from_utf8_lossy(value);
    } catch (...) {
        return {};
    }
}

std::string normalize_filesystem_display_name(std::string value) {
    value = util::sanitize_ui_text(value);
    if (value.empty()) {
        return "Imported Skin";
    }
    for (char& ch : value) {
        switch (ch) {
            case '<':
            case '>':
            case ':':
            case '"':
            case '/':
            case '\\':
            case '|':
            case '?':
            case '*':
                ch = '_';
                break;
            default:
                break;
        }
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '.')) {
        value.pop_back();
    }
    return value.empty() ? "Imported Skin" : value;
}

fs::path osu_skin_import_root_path(std::string_view profile_dir) {
    return path_from_utf8(profile_dir) / "skins" / "osu";
}

fs::path lr2_skin_import_root_path(std::string_view profile_dir) {
    return path_from_utf8(profile_dir) / "skins" / "lr2";
}

#ifdef _WIN32
std::optional<std::string> pick_folder_dialog_utf8() {
    const HRESULT init_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool should_uninitialize = SUCCEEDED(init_hr);
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    const HRESULT create_hr = CoCreateInstance(CLSID_FileOpenDialog,
                                               nullptr,
                                               CLSCTX_INPROC_SERVER,
                                               IID_PPV_ARGS(&dialog));
    if (FAILED(create_hr) || !dialog) {
        if (should_uninitialize) {
            CoUninitialize();
        }
        return std::nullopt;
    }

    DWORD options = 0;
    if (FAILED(dialog->GetOptions(&options))) {
        if (should_uninitialize) {
            CoUninitialize();
        }
        return std::nullopt;
    }
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

    if (FAILED(dialog->Show(nullptr))) {
        if (should_uninitialize) {
            CoUninitialize();
        }
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item)) || !item) {
        if (should_uninitialize) {
            CoUninitialize();
        }
        return std::nullopt;
    }

    PWSTR raw_path = nullptr;
    std::optional<std::string> result;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path)) && raw_path) {
        const std::filesystem::path path(raw_path);
        result = path.u8string();
        CoTaskMemFree(raw_path);
    }

    if (should_uninitialize) {
        CoUninitialize();
    }
    return result;
}
#endif

}  // namespace

void MenuApp::refresh_available_osu_skins() {
    available_osu_skin_root_.clear();
    available_osu_skin_names_.clear();
    available_osu_skin_roots_by_name_.clear();

    auto add_skin_root = [&](const std::string& root, bool prefer_existing) {
        if (root.empty()) {
            return;
        }
        const auto names = list_osu_skin_names(root);
        for (const auto& name : names) {
            const auto it = available_osu_skin_roots_by_name_.find(name);
            if (it == available_osu_skin_roots_by_name_.end()) {
                available_osu_skin_names_.push_back(name);
                available_osu_skin_roots_by_name_[name] = root;
            } else if (!prefer_existing) {
                it->second = root;
            }
        }
    };

    add_skin_root(osu_skin_import_root_path(profile_dir_).u8string(), false);
    add_skin_root(find_default_osu_skin_test_root(), true);
    std::sort(available_osu_skin_names_.begin(), available_osu_skin_names_.end());

    if (available_osu_skin_names_.empty()) {
        config_.skin.osu_skin_name.clear();
        return;
    }
    if (config_.skin.osu_skin_name.empty() ||
        std::find(available_osu_skin_names_.begin(),
                  available_osu_skin_names_.end(),
                  config_.skin.osu_skin_name) == available_osu_skin_names_.end()) {
        config_.skin.osu_skin_name = available_osu_skin_names_.front();
    }
    const auto root_it = available_osu_skin_roots_by_name_.find(config_.skin.osu_skin_name);
    if (root_it != available_osu_skin_roots_by_name_.end()) {
        available_osu_skin_root_ = root_it->second;
    } else {
        available_osu_skin_root_.clear();
    }
}

void MenuApp::refresh_available_lr2_skins() {
    available_lr2_skin_root_.clear();
    available_lr2_skin_names_.clear();
    available_lr2_skin_roots_by_name_.clear();

    auto add_skin_root = [&](const std::string& root, bool prefer_existing) {
        if (root.empty()) {
            return;
        }
        const auto names = list_lr2_skin_names(root);
        for (const auto& name : names) {
            const auto it = available_lr2_skin_roots_by_name_.find(name);
            if (it == available_lr2_skin_roots_by_name_.end()) {
                available_lr2_skin_names_.push_back(name);
                available_lr2_skin_roots_by_name_[name] = root;
            } else if (!prefer_existing) {
                it->second = root;
            }
        }
    };

    add_skin_root(lr2_skin_import_root_path(profile_dir_).u8string(), false);
    add_skin_root(find_default_lr2_skin_test_root(), true);
    std::sort(available_lr2_skin_names_.begin(), available_lr2_skin_names_.end());

    if (available_lr2_skin_names_.empty()) {
        config_.skin.lr2_skin_name.clear();
        return;
    }
    if (config_.skin.lr2_skin_name.empty() ||
        std::find(available_lr2_skin_names_.begin(),
                  available_lr2_skin_names_.end(),
                  config_.skin.lr2_skin_name) == available_lr2_skin_names_.end()) {
        config_.skin.lr2_skin_name = available_lr2_skin_names_.front();
    }
    const auto root_it = available_lr2_skin_roots_by_name_.find(config_.skin.lr2_skin_name);
    if (root_it != available_lr2_skin_roots_by_name_.end()) {
        available_lr2_skin_root_ = root_it->second;
    } else {
        available_lr2_skin_root_.clear();
    }
}

bool MenuApp::import_osu_skin_path(std::string_view source_path) {
    const std::string normalized_source = menu_songs::normalize_song_source_path(std::string(source_path));
    if (normalized_source.empty()) {
        return false;
    }

    const fs::path source = path_from_utf8(normalized_source);
    std::error_code ec;
    if (!fs::is_directory(source, ec)) {
        return false;
    }

    const fs::path import_root = osu_skin_import_root_path(profile_dir_);
    fs::create_directories(import_root, ec);
    if (ec) {
        std::cerr << "[warn] Failed to create osu skin import root: " << import_root.u8string() << std::endl;
        return false;
    }

    int imported_count = 0;
    auto import_single_skin = [&](const fs::path& skin_dir) -> bool {
        const std::string source_skin_path = menu_songs::normalize_song_source_path(skin_dir.u8string());
        if (source_skin_path.empty() || !is_osu_skin_directory(source_skin_path)) {
            return false;
        }

        const std::string base_name = normalize_filesystem_display_name(skin_dir.filename().u8string());
        fs::path destination = import_root / path_from_utf8(base_name);
        std::error_code path_ec;
        const fs::path source_canonical = fs::weakly_canonical(skin_dir, path_ec);
        path_ec.clear();
        const fs::path import_root_canonical = fs::weakly_canonical(import_root, path_ec);
        path_ec.clear();

        if (!source_canonical.empty() &&
            !import_root_canonical.empty() &&
            source_canonical.parent_path() == import_root_canonical) {
            config_.skin.osu_skin_name = source_canonical.filename().u8string();
            ++imported_count;
            return true;
        }

        int suffix = 2;
        while (fs::exists(destination, ec)) {
            if (!ec) {
                destination = import_root / path_from_utf8(base_name + " (" + std::to_string(suffix) + ")");
                ++suffix;
                continue;
            }
            ec.clear();
            return false;
        }

        fs::copy(skin_dir, destination, fs::copy_options::recursive, ec);
        if (ec) {
            std::cerr << "[warn] Failed to import osu skin: " << skin_dir.u8string()
                      << " -> " << destination.u8string() << std::endl;
            return false;
        }

        config_.skin.osu_skin_name = destination.filename().u8string();
        ++imported_count;
        return true;
    };

    bool imported = import_single_skin(source);
    if (!imported) {
        for (const auto& skin_name : list_osu_skin_names(normalized_source)) {
            imported |= import_single_skin(source / path_from_utf8(skin_name));
        }
    }
    if (!imported) {
        return false;
    }

    refresh_available_osu_skins();
    skin_dirty_ = true;
    std::cerr << "[info] Imported " << imported_count << " osu skin(s)." << std::endl;
    return true;
}

bool MenuApp::import_lr2_skin_path(std::string_view source_path) {
    const std::string normalized_source = menu_songs::normalize_song_source_path(std::string(source_path));
    if (normalized_source.empty()) {
        return false;
    }

    const fs::path source = path_from_utf8(normalized_source);
    std::error_code ec;
    if (!fs::is_directory(source, ec)) {
        return false;
    }

    const fs::path import_root = lr2_skin_import_root_path(profile_dir_);
    fs::create_directories(import_root, ec);
    if (ec) {
        std::cerr << "[warn] Failed to create LR2 skin import root: " << import_root.u8string() << std::endl;
        return false;
    }

    int imported_count = 0;
    auto import_single_skin = [&](const fs::path& skin_dir) -> bool {
        const std::string source_skin_path = menu_songs::normalize_song_source_path(skin_dir.u8string());
        if (source_skin_path.empty() || !is_lr2_skin_directory(source_skin_path)) {
            return false;
        }

        const std::string base_name = normalize_filesystem_display_name(skin_dir.filename().u8string());
        fs::path destination = import_root / path_from_utf8(base_name);
        std::error_code path_ec;
        const fs::path source_canonical = fs::weakly_canonical(skin_dir, path_ec);
        path_ec.clear();
        const fs::path import_root_canonical = fs::weakly_canonical(import_root, path_ec);
        path_ec.clear();

        if (!source_canonical.empty() &&
            !import_root_canonical.empty() &&
            source_canonical.parent_path() == import_root_canonical) {
            config_.skin.lr2_skin_name = source_canonical.filename().u8string();
            ++imported_count;
            return true;
        }

        int suffix = 2;
        while (fs::exists(destination, ec)) {
            if (!ec) {
                destination = import_root / path_from_utf8(base_name + " (" + std::to_string(suffix) + ")");
                ++suffix;
                continue;
            }
            ec.clear();
            return false;
        }

        fs::copy(skin_dir, destination, fs::copy_options::recursive, ec);
        if (ec) {
            std::cerr << "[warn] Failed to import LR2 skin: " << skin_dir.u8string()
                      << " -> " << destination.u8string() << std::endl;
            return false;
        }

        config_.skin.lr2_skin_name = destination.filename().u8string();
        ++imported_count;
        return true;
    };

    bool imported = import_single_skin(source);
    if (!imported) {
        std::error_code scan_ec;
        for (fs::directory_iterator it(source, scan_ec), end; !scan_ec && it != end; it.increment(scan_ec)) {
            if (!it->is_directory(scan_ec)) {
                continue;
            }
            imported |= import_single_skin(it->path());
        }
    }
    if (!imported) {
        return false;
    }

    refresh_available_lr2_skins();
    skin_dirty_ = true;
    std::cerr << "[info] Imported " << imported_count << " LR2 skin(s)." << std::endl;
    return true;
}

bool MenuApp::import_skin_path_auto(std::string_view source_path) {
    const std::string active_source = config::normalize_skin_source_token(config_.skin.source);
    if (active_source == "osu") {
        if (import_osu_skin_path(source_path)) {
            return true;
        }
        if (import_lr2_skin_path(source_path)) {
            config_.skin.source = "lr2";
            return true;
        }
        return false;
    }
    if (active_source == "lr2") {
        if (import_lr2_skin_path(source_path)) {
            return true;
        }
        if (import_osu_skin_path(source_path)) {
            config_.skin.source = "osu";
            return true;
        }
        return false;
    }
    if (import_osu_skin_path(source_path)) {
        config_.skin.source = "osu";
        return true;
    }
    if (import_lr2_skin_path(source_path)) {
        config_.skin.source = "lr2";
        return true;
    }
    return false;
}

std::string MenuApp::active_external_skin_root() const {
    const std::string source = config::normalize_skin_source_token(config_.skin.source);
    if (source == "osu") {
        return available_osu_skin_root_;
    }
    if (source == "lr2") {
        return available_lr2_skin_root_;
    }
    return {};
}

std::string MenuApp::active_external_skin_name() const {
    const std::string source = config::normalize_skin_source_token(config_.skin.source);
    if (source == "osu") {
        return config_.skin.osu_skin_name;
    }
    if (source == "lr2") {
        return config_.skin.lr2_skin_name;
    }
    return {};
}

void MenuApp::handle_skins_settings_input(uint32_t keycode) {
    const int item_count = 20;
    if (keycode == key_up_) {
        settings_cursor_ = clamp_int(settings_cursor_ - 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        settings_cursor_ = clamp_int(settings_cursor_ + 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 0 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.source = cycle_skin_source(config_.skin.source, direction);
        refresh_available_osu_skins();
        refresh_available_lr2_skins();
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 1 && (keycode == key_left_ || keycode == key_right_)) {
        const std::string active_source = config::normalize_skin_source_token(config_.skin.source);
        auto* names = &available_osu_skin_names_;
        auto* selected_name = &config_.skin.osu_skin_name;
        if (active_source == "lr2") {
            names = &available_lr2_skin_names_;
            selected_name = &config_.skin.lr2_skin_name;
        }
        if (active_source != "native" && !names->empty()) {
            int current_index = 0;
            for (int i = 0; i < static_cast<int>(names->size()); ++i) {
                if ((*names)[static_cast<std::size_t>(i)] == *selected_name) {
                    current_index = i;
                    break;
                }
            }
            current_index += (keycode == key_left_) ? -1 : 1;
            if (current_index < 0) {
                current_index = static_cast<int>(names->size()) - 1;
            } else if (current_index >= static_cast<int>(names->size())) {
                current_index = 0;
            }
            *selected_name = (*names)[static_cast<std::size_t>(current_index)];
            refresh_available_osu_skins();
            refresh_available_lr2_skins();
            skin_dirty_ = true;
        }
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 2 && keycode == key_enter_) {
#ifdef _WIN32
        if (auto picked_folder = pick_folder_dialog_utf8(); picked_folder.has_value()) {
            if (!import_skin_path_auto(*picked_folder)) {
                std::cerr << "[warn] Selected folder is not an osu!mania or LR2 skin folder: "
                          << *picked_folder << std::endl;
            }
            publish_snapshot();
        }
#endif
        return;
    }
    if (settings_cursor_ == 3 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        skin_edit_mode_ = cycle_skin_edit_mode(skin_edit_mode_, direction);
        skin_edit_lane_ = clamp_int(skin_edit_lane_, 0, lane_count_for_skin_mode(skin_edit_mode_) - 1);
        editable_skin_lane_colors(config_.skin, skin_edit_mode_);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 4 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        const int lane_count = lane_count_for_skin_mode(skin_edit_mode_);
        int next_lane = skin_edit_lane_ + direction;
        if (next_lane < 0) {
            next_lane = lane_count - 1;
        } else if (next_lane >= lane_count) {
            next_lane = 0;
        }
        skin_edit_lane_ = next_lane;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 5 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        auto& lane_colors = editable_skin_lane_colors(config_.skin, skin_edit_mode_);
        const auto palette = config::supported_skin_color_tokens();
        const std::string current = config::normalize_skin_color_token(
            lane_colors[static_cast<std::size_t>(skin_edit_lane_)]);
        int index = 0;
        for (int i = 0; i < static_cast<int>(palette.size()); ++i) {
            if (palette[static_cast<std::size_t>(i)] == current) {
                index = i;
                break;
            }
        }
        index += direction;
        if (index < 0) {
            index = static_cast<int>(palette.size()) - 1;
        } else if (index >= static_cast<int>(palette.size())) {
            index = 0;
        }
        lane_colors[static_cast<std::size_t>(skin_edit_lane_)] = palette[static_cast<std::size_t>(index)];
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 6 && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.note_shape =
            (config::normalize_skin_note_shape_token(config_.skin.note_shape) == "circle") ? "rect" : "circle";
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 7 && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.note_border_enabled = !config_.skin.note_border_enabled;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 8 && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.preserve_note_image_aspect_ratio = !config_.skin.preserve_note_image_aspect_ratio;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 9 && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.show_lane_dividers = !config_.skin.show_lane_dividers;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 10 && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.show_judgement_line = !config_.skin.show_judgement_line;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 11 && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.show_gear_boundary_line = !config_.skin.show_gear_boundary_line;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 12 && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.hold_tail_taper_enabled = !config_.skin.hold_tail_taper_enabled;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 13 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.judgement_line_position = clamp_step_value(
            config_.skin.judgement_line_position + static_cast<double>(direction) * kJudgementLinePositionStep,
            config::kJudgementLinePositionMin, config::kJudgementLinePositionMax, kJudgementLinePositionStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 14 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        auto& note_width_scale = editable_skin_note_width_scale(config_.skin, skin_edit_mode_);
        note_width_scale = clamp_step_value(
            note_width_scale + static_cast<double>(direction) * kNoteSizeScaleStep,
            config::kNoteWidthScaleMin, config::kNoteWidthScaleMax, kNoteSizeScaleStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 15 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        auto& lane_divider_width_scale =
            editable_skin_lane_divider_width_scale(config_.skin, skin_edit_mode_);
        lane_divider_width_scale = clamp_step_value(
            lane_divider_width_scale + static_cast<double>(direction) * kLaneDividerScaleStep,
            config::kLaneDividerWidthScaleMin, config::kLaneDividerWidthScaleMax, kLaneDividerScaleStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 16 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.hold_body_width_scale = clamp_step_value(
            config_.skin.hold_body_width_scale + static_cast<double>(direction) * kNoteSizeScaleStep,
            config::kHoldBodyWidthScaleMin, config::kHoldBodyWidthScaleMax, kNoteSizeScaleStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 17 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        auto& note_height_scale = editable_skin_note_height_scale(config_.skin, skin_edit_mode_);
        note_height_scale = clamp_step_value(
            note_height_scale + static_cast<double>(direction) * kNoteSizeScaleStep,
            config::kNoteHeightScaleMin, config::kNoteHeightScaleMax, kNoteSizeScaleStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 18 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.combo_position = clamp_step_value(
            config_.skin.combo_position + static_cast<double>(direction) * kComboPositionStep,
            config::kComboPositionMin, config::kComboPositionMax, kComboPositionStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }

    if ((keycode == key_enter_ && settings_cursor_ == item_count - 1) ||
        keycode == key_escape_ ||
        keycode == key_backspace_) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        if (skin_dirty_) {
            persist_runtime_config();
            skin_dirty_ = false;
        }
        publish_snapshot();
    }
}

void MenuApp::populate_skin_settings_render_data(render::MenuRenderData& render) {
    skin_edit_mode_ = normalize_skin_edit_mode(skin_edit_mode_);
    const int lane_count = lane_count_for_skin_mode(skin_edit_mode_);
    skin_edit_lane_ = clamp_int(skin_edit_lane_, 0, lane_count - 1);
    const auto preview_lane_colors = config::resolved_skin_lane_colors(config_.skin, skin_edit_mode_);
    const double preview_note_width_scale = config::resolved_skin_note_width_scale(config_.skin, skin_edit_mode_);
    const double preview_note_height_scale = config::resolved_skin_note_height_scale(config_.skin, skin_edit_mode_);
    const double preview_lane_divider_width_scale =
        config::resolved_skin_lane_divider_width_scale(config_.skin, skin_edit_mode_);
    const std::string active_skin_source = config::normalize_skin_source_token(config_.skin.source);
    const std::string imported_skin_row_label =
        (active_skin_source == "osu") ? "OSU Skin"
                                      : ((active_skin_source == "lr2") ? "LR2 Skin" : "Imported Skin");
    std::string imported_skin_value = "N/A";
    if (active_skin_source == "osu") {
        imported_skin_value = available_osu_skin_names_.empty()
                                  ? std::string("Not Found")
                                  : (config_.skin.osu_skin_name.empty() ? available_osu_skin_names_.front()
                                                                        : config_.skin.osu_skin_name);
    } else if (active_skin_source == "lr2") {
        imported_skin_value = available_lr2_skin_names_.empty()
                                  ? std::string("Not Found")
                                  : (config_.skin.lr2_skin_name.empty() ? available_lr2_skin_names_.front()
                                                                        : config_.skin.lr2_skin_name);
    }

    append_menu_row(render.generic, "Skin Source", skin_source_label(active_skin_source), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, imported_skin_row_label, imported_skin_value, settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow, 1, false, true);
    append_menu_row(render.generic, "Import Skin", "Open Folder", settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow, 2, true, false);
    append_menu_row(render.generic, "Key Mode", key_mode_label(skin_edit_mode_), settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow, 3, false, true);
    append_menu_row(render.generic, "Target Lane",
                    lane_display_label(skin_edit_lane_) + " / " + std::to_string(lane_count),
                    settings_cursor_ == 4, render::MenuHitTargetKind::SettingsRow, 4, false, true);
    append_menu_row(render.generic, "Lane Color",
                    config::skin_color_label(preview_lane_colors[static_cast<std::size_t>(skin_edit_lane_)]),
                    settings_cursor_ == 5, render::MenuHitTargetKind::SettingsRow, 5, false, true);
    append_menu_row(render.generic, "Note Shape", config::skin_note_shape_label(config_.skin.note_shape),
                    settings_cursor_ == 6, render::MenuHitTargetKind::SettingsRow, 6, false, true);
    append_menu_row(render.generic, "Note Border", on_off(config_.skin.note_border_enabled), settings_cursor_ == 7,
                    render::MenuHitTargetKind::SettingsRow, 7, false, true);
    append_menu_row(render.generic, "Image Aspect", on_off(config_.skin.preserve_note_image_aspect_ratio),
                    settings_cursor_ == 8, render::MenuHitTargetKind::SettingsRow, 8, false, true);
    append_menu_row(render.generic, "Lane Dividers", on_off(config_.skin.show_lane_dividers), settings_cursor_ == 9,
                    render::MenuHitTargetKind::SettingsRow, 9, false, true);
    append_menu_row(render.generic, "Judgement Line", on_off(config_.skin.show_judgement_line),
                    settings_cursor_ == 10, render::MenuHitTargetKind::SettingsRow, 10, false, true);
    append_menu_row(render.generic, "Gear Boundary", on_off(config_.skin.show_gear_boundary_line),
                    settings_cursor_ == 11, render::MenuHitTargetKind::SettingsRow, 11, false, true);
    append_menu_row(render.generic, "LN Tail Taper", on_off(config_.skin.hold_tail_taper_enabled),
                    settings_cursor_ == 12, render::MenuHitTargetKind::SettingsRow, 12, false, true);
    append_menu_row(render.generic, "Judge Line", format_percent(config_.skin.judgement_line_position),
                    settings_cursor_ == 13, render::MenuHitTargetKind::SettingsRow, 13, false, true);
    append_menu_row(render.generic, "Note Width", format_percent(preview_note_width_scale), settings_cursor_ == 14,
                    render::MenuHitTargetKind::SettingsRow, 14, false, true);
    append_menu_row(render.generic, "Divider Width", format_percent(preview_lane_divider_width_scale),
                    settings_cursor_ == 15, render::MenuHitTargetKind::SettingsRow, 15, false, true);
    append_menu_row(render.generic, "LN Body Width", format_percent(config_.skin.hold_body_width_scale),
                    settings_cursor_ == 16, render::MenuHitTargetKind::SettingsRow, 16, false, true);
    append_menu_row(render.generic, "Note Height", format_percent(preview_note_height_scale), settings_cursor_ == 17,
                    render::MenuHitTargetKind::SettingsRow, 17, false, true);
    append_menu_row(render.generic, "Combo Y", format_percent(config_.skin.combo_position), settings_cursor_ == 18,
                    render::MenuHitTargetKind::SettingsRow, 18, false, true);
    append_menu_row(render.generic, "Back", "", settings_cursor_ == 19, render::MenuHitTargetKind::SettingsRow, 19,
                    true, false);

    render.generic.skin_preview.visible = true;
    render.generic.skin_preview.mode_label = key_mode_label(skin_edit_mode_);
    render.generic.skin_preview.selected_color_label =
        config::skin_color_label(preview_lane_colors[static_cast<std::size_t>(skin_edit_lane_)]);
    render.generic.skin_preview.lane_count = lane_count;
    render.generic.skin_preview.selected_lane = skin_edit_lane_ + 1;
    render.generic.skin_preview.judgement_line_position = config_.skin.judgement_line_position;
    render.generic.skin_preview.combo_position = config_.skin.combo_position;
    render.generic.skin_preview.note_width_scale = preview_note_width_scale;
    render.generic.skin_preview.note_height_scale = preview_note_height_scale;
    render.generic.skin_preview.lane_divider_width_scale = preview_lane_divider_width_scale;
    render.generic.skin_preview.hold_body_width_scale = config_.skin.hold_body_width_scale;
    render.generic.skin_preview.show_lane_dividers = config_.skin.show_lane_dividers;
    render.generic.skin_preview.show_judgement_line = config_.skin.show_judgement_line;
    render.generic.skin_preview.show_gear_boundary_line = config_.skin.show_gear_boundary_line;
    render.generic.skin_preview.hold_tail_taper_enabled = config_.skin.hold_tail_taper_enabled;
    render.generic.skin_preview.note_border_enabled = config_.skin.note_border_enabled;
    render.generic.skin_preview.note_shape = config_.skin.note_shape;
    render.generic.skin_preview.preserve_note_image_aspect_ratio =
        config_.skin.preserve_note_image_aspect_ratio;
    render.generic.skin_preview.skin_source = active_skin_source;
    render.generic.skin_preview.external_skin_root = active_external_skin_root();
    render.generic.skin_preview.external_skin_name = active_external_skin_name();
    render.generic.skin_preview.lane_colors.fill(0);
    for (int lane = 0; lane < lane_count && lane < static_cast<int>(kGameplayHudMaxLanes); ++lane) {
        render.generic.skin_preview.lane_colors[static_cast<std::size_t>(lane)] =
            config::skin_color_rgb(preview_lane_colors[static_cast<std::size_t>(lane)]);
    }

    render.generic.notes.push_back("Skin Source switches between the native vector skin, imported osu!mania PNG skins, and imported LR2 playskins.");
    render.generic.notes.push_back("Imported OSU skins scan profile skins first, then build/Release/test-skins-osu as a fallback test root.");
    render.generic.notes.push_back("Imported LR2 skins scan profile skins first, then build/Release/test-skins-lr2 as a fallback test root.");
    render.generic.notes.push_back("Import Skin opens a folder picker. You can also drag and drop a skin folder onto this screen.");
    render.generic.notes.push_back("LR2 porting imports note, LN, lane-gap, and destination-size data from default active branches in the playskin.");
    render.generic.notes.push_back("Image Aspect keeps imported head and tail art from stretching to the gameplay note box.");
    render.generic.notes.push_back("Lane Dividers, Judgement Line, and Gear Boundary can be toggled independently.");
    render.generic.notes.push_back("LN Tail Taper only changes visuals: the hold body narrows toward the tail without changing timing or hitboxes.");
    render.generic.notes.push_back("Divider Width scales native lane separators and multiplies imported divider widths when the external skin provides them.");
    render.generic.notes.push_back("Key Mode and Target Lane still edit the native fallback palette and sizing per layout.");
}

}  // namespace tenriff::app
