#include "app/MenuApp.h"

#include <algorithm>
#include <array>
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
constexpr double kSkinOpacityStep = 0.05;

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

std::string lr2_resolution_mode_label(std::string_view value) {
    const std::string normalized = config::normalize_skin_lr2_resolution_mode_token(value);
    if (normalized == "sd") {
        return "SD";
    }
    if (normalized == "hd") {
        return "HD";
    }
    if (normalized == "fhd") {
        return "FHD";
    }
    return "Auto";
}

std::string cycle_lr2_resolution_mode(std::string_view value, int direction) {
    static constexpr std::array<std::string_view, 4> kModes = {"auto", "sd", "hd", "fhd"};
    const std::string normalized = config::normalize_skin_lr2_resolution_mode_token(value);
    int index = 0;
    for (int i = 0; i < static_cast<int>(kModes.size()); ++i) {
        if (kModes[static_cast<std::size_t>(i)] == normalized) {
            index = i;
            break;
        }
    }
    index += direction;
    if (index < 0) {
        index = static_cast<int>(kModes.size()) - 1;
    } else if (index >= static_cast<int>(kModes.size())) {
        index = 0;
    }
    return std::string(kModes[static_cast<std::size_t>(index)]);
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
    const std::string active_skin_source = config::normalize_skin_source_token(config_.skin.source);
    const bool lr2_source = active_skin_source == "lr2";
    const int lr2_shift = lr2_source ? 1 : 0;
    const int item_count = 32 + lr2_shift;
    const int imported_skin_row = 1;
    const int lr2_resolution_row = lr2_source ? 2 : -1;
    const int import_skin_row = 2 + lr2_shift;
    const int key_mode_row = 3 + lr2_shift;
    const int target_lane_row = 4 + lr2_shift;
    const int target_gap_row = 5 + lr2_shift;
    const int lane_color_row = 6 + lr2_shift;
    const int note_shape_row = 7 + lr2_shift;
    const int note_border_row = 8 + lr2_shift;
    const int image_aspect_row = 9 + lr2_shift;
    const int lane_dividers_row = 10 + lr2_shift;
    const int judgement_line_row = 11 + lr2_shift;
    const int gear_boundary_row = 12 + lr2_shift;
    const int ln_tail_taper_row = 13 + lr2_shift;
    const int visual_preset_row = 14 + lr2_shift;
    const int lane_background_opacity_row = 15 + lr2_shift;
    const int visual_opacity_row = 16 + lr2_shift;
    const int note_outline_opacity_row = 17 + lr2_shift;
    const int ln_body_opacity_row = 18 + lr2_shift;
    const int judgement_line_glow_row = 19 + lr2_shift;
    const int key_pulse_row = 20 + lr2_shift;
    const int key_label_position_row = 21 + lr2_shift;
    const int judge_line_row = 22 + lr2_shift;
    const int lane_width_row = 23 + lr2_shift;
    const int note_width_row = 24 + lr2_shift;
    const int lane_spacing_row = 25 + lr2_shift;
    const int divider_width_row = 26 + lr2_shift;
    const int center_gap_row = 27 + lr2_shift;
    const int ln_body_width_row = 28 + lr2_shift;
    const int note_height_row = 29 + lr2_shift;
    const int combo_y_row = 30 + lr2_shift;
    const int back_row = 31 + lr2_shift;

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
        const bool was_lr2 = active_skin_source == "lr2";
        config_.skin.source = cycle_skin_source(config_.skin.source, direction);
        const bool is_lr2 = config::normalize_skin_source_token(config_.skin.source) == "lr2";
        const int new_item_count = 32 + (is_lr2 ? 1 : 0);
        if (!was_lr2 && is_lr2 && settings_cursor_ >= imported_skin_row) {
            ++settings_cursor_;
        } else if (was_lr2 && !is_lr2 && settings_cursor_ >= import_skin_row) {
            --settings_cursor_;
        }
        settings_cursor_ = clamp_int(settings_cursor_, 0, new_item_count - 1);
        refresh_available_osu_skins();
        refresh_available_lr2_skins();
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == imported_skin_row && (keycode == key_left_ || keycode == key_right_)) {
        auto* names = &available_osu_skin_names_;
        auto* selected_name = &config_.skin.osu_skin_name;
        if (active_skin_source == "lr2") {
            names = &available_lr2_skin_names_;
            selected_name = &config_.skin.lr2_skin_name;
        }
        if (active_skin_source != "native" && !names->empty()) {
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
    if (settings_cursor_ == lr2_resolution_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.lr2_resolution_mode = cycle_lr2_resolution_mode(config_.skin.lr2_resolution_mode, direction);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == import_skin_row && keycode == key_enter_) {
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
    if (settings_cursor_ == key_mode_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        skin_edit_mode_ = cycle_skin_edit_mode(skin_edit_mode_, direction);
        const int lane_count = lane_count_for_skin_mode(skin_edit_mode_);
        skin_edit_lane_ = clamp_int(skin_edit_lane_, 0, lane_count - 1);
        skin_edit_gap_ = clamp_int(skin_edit_gap_, 0, std::max(0, lane_count - 2));
        editable_skin_lane_colors(config_.skin, skin_edit_mode_);
        editable_skin_lane_width_scales(config_.skin, skin_edit_mode_);
        editable_skin_lane_spacing_scales(config_.skin, skin_edit_mode_);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == target_lane_row && (keycode == key_left_ || keycode == key_right_)) {
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
    if (settings_cursor_ == target_gap_row && (keycode == key_left_ || keycode == key_right_)) {
        const int lane_count = lane_count_for_skin_mode(skin_edit_mode_);
        const int gap_count = std::max(0, lane_count - 1);
        if (gap_count > 0) {
            const int direction = (keycode == key_left_) ? -1 : 1;
            int next_gap = skin_edit_gap_ + direction;
            if (next_gap < 0) {
                next_gap = gap_count - 1;
            } else if (next_gap >= gap_count) {
                next_gap = 0;
            }
            skin_edit_gap_ = next_gap;
        }
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == lane_color_row && (keycode == key_left_ || keycode == key_right_)) {
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
    if (settings_cursor_ == note_shape_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.note_shape =
            (config::normalize_skin_note_shape_token(config_.skin.note_shape) == "circle") ? "rect" : "circle";
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == note_border_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.note_border_enabled = !config_.skin.note_border_enabled;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == image_aspect_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.preserve_note_image_aspect_ratio = !config_.skin.preserve_note_image_aspect_ratio;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == lane_dividers_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.show_lane_dividers = !config_.skin.show_lane_dividers;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == judgement_line_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.show_judgement_line = !config_.skin.show_judgement_line;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == gear_boundary_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.show_gear_boundary_line = !config_.skin.show_gear_boundary_line;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == ln_tail_taper_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.hold_tail_taper_enabled = !config_.skin.hold_tail_taper_enabled;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == visual_preset_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config::apply_skin_visual_preset(config_.skin, cycle_skin_visual_preset(config_.skin.visual_preset, direction));
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == lane_background_opacity_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.lane_background_opacity = clamp_step_value(
            config_.skin.lane_background_opacity + static_cast<double>(direction) * kSkinOpacityStep,
            config::kSkinLaneBackgroundOpacityMin,
            config::kSkinLaneBackgroundOpacityMax,
            kSkinOpacityStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == visual_opacity_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.visual_opacity = clamp_step_value(
            config_.skin.visual_opacity + static_cast<double>(direction) * kSkinOpacityStep,
            config::kSkinVisualOpacityMin,
            config::kSkinVisualOpacityMax,
            kSkinOpacityStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == note_outline_opacity_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.note_outline_opacity = clamp_step_value(
            config_.skin.note_outline_opacity + static_cast<double>(direction) * kSkinOpacityStep,
            config::kSkinNoteOutlineOpacityMin,
            config::kSkinNoteOutlineOpacityMax,
            kSkinOpacityStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == ln_body_opacity_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.hold_body_opacity = clamp_step_value(
            config_.skin.hold_body_opacity + static_cast<double>(direction) * kSkinOpacityStep,
            config::kSkinHoldBodyOpacityMin,
            config::kSkinHoldBodyOpacityMax,
            kSkinOpacityStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == judgement_line_glow_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.judgement_line_glow_enabled = !config_.skin.judgement_line_glow_enabled;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == key_pulse_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.key_pulse_enabled = !config_.skin.key_pulse_enabled;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == key_label_position_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.key_label_position = cycle_skin_key_label_position(config_.skin.key_label_position, direction);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == judge_line_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.judgement_line_position = clamp_step_value(
            config_.skin.judgement_line_position + static_cast<double>(direction) * kJudgementLinePositionStep,
            config::kJudgementLinePositionMin, config::kJudgementLinePositionMax, kJudgementLinePositionStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == lane_width_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        auto& lane_width_scales = editable_skin_lane_width_scales(config_.skin, skin_edit_mode_);
        if (static_cast<std::size_t>(skin_edit_lane_) < lane_width_scales.size()) {
            lane_width_scales[static_cast<std::size_t>(skin_edit_lane_)] = clamp_step_value(
                lane_width_scales[static_cast<std::size_t>(skin_edit_lane_)] +
                    static_cast<double>(direction) * kNoteSizeScaleStep,
                config::kLaneWidthScaleMin,
                config::kLaneWidthScaleMax,
                kNoteSizeScaleStep);
            skin_dirty_ = true;
        }
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == note_width_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        auto& note_width_scale = editable_skin_note_width_scale(config_.skin, skin_edit_mode_);
        note_width_scale = clamp_step_value(
            note_width_scale + static_cast<double>(direction) * kNoteSizeScaleStep,
            config::kNoteWidthScaleMin, config::kNoteWidthScaleMax, kNoteSizeScaleStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == lane_spacing_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        auto& lane_spacing_scales = editable_skin_lane_spacing_scales(config_.skin, skin_edit_mode_);
        if (static_cast<std::size_t>(skin_edit_gap_) < lane_spacing_scales.size()) {
            lane_spacing_scales[static_cast<std::size_t>(skin_edit_gap_)] = clamp_step_value(
                lane_spacing_scales[static_cast<std::size_t>(skin_edit_gap_)] +
                    static_cast<double>(direction) * kLaneDividerScaleStep,
                config::kLaneSpacingScaleMin,
                config::kLaneSpacingScaleMax,
                kLaneDividerScaleStep);
            skin_dirty_ = true;
        }
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == divider_width_row && (keycode == key_left_ || keycode == key_right_)) {
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
    if (settings_cursor_ == center_gap_row && (keycode == key_left_ || keycode == key_right_)) {
        if (config::normalize_skin_mode_token(skin_edit_mode_) == "16k") {
            const int direction = (keycode == key_left_) ? -1 : 1;
            auto& lane_center_gap_scale =
                editable_skin_lane_center_gap_scale(config_.skin, skin_edit_mode_);
            lane_center_gap_scale = clamp_step_value(
                lane_center_gap_scale + static_cast<double>(direction) * kLaneDividerScaleStep,
                config::kLaneCenterGapScaleMin, config::kLaneCenterGapScaleMax, kLaneDividerScaleStep);
            skin_dirty_ = true;
        }
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == ln_body_width_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.hold_body_width_scale = clamp_step_value(
            config_.skin.hold_body_width_scale + static_cast<double>(direction) * kNoteSizeScaleStep,
            config::kHoldBodyWidthScaleMin, config::kHoldBodyWidthScaleMax, kNoteSizeScaleStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == note_height_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        auto& note_height_scale = editable_skin_note_height_scale(config_.skin, skin_edit_mode_);
        note_height_scale = clamp_step_value(
            note_height_scale + static_cast<double>(direction) * kNoteSizeScaleStep,
            config::kNoteHeightScaleMin, config::kNoteHeightScaleMax, kNoteSizeScaleStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == combo_y_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.combo_position = clamp_step_value(
            config_.skin.combo_position + static_cast<double>(direction) * kComboPositionStep,
            config::kComboPositionMin, config::kComboPositionMax, kComboPositionStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }

    if ((keycode == key_enter_ && settings_cursor_ == back_row) ||
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
    const int gap_count = std::max(0, lane_count - 1);
    skin_edit_gap_ = clamp_int(skin_edit_gap_, 0, std::max(0, gap_count - 1));
    const auto preview_lane_colors = config::resolved_skin_lane_colors(config_.skin, skin_edit_mode_);
    const auto preview_lane_width_scales = config::resolved_skin_lane_width_scales(config_.skin, skin_edit_mode_);
    const double preview_note_width_scale = config::resolved_skin_note_width_scale(config_.skin, skin_edit_mode_);
    const auto preview_lane_spacing_scales = config::resolved_skin_lane_spacing_scales(config_.skin, skin_edit_mode_);
    const double preview_note_height_scale = config::resolved_skin_note_height_scale(config_.skin, skin_edit_mode_);
    const double preview_lane_divider_width_scale =
        config::resolved_skin_lane_divider_width_scale(config_.skin, skin_edit_mode_);
    const double preview_lane_center_gap_scale =
        config::resolved_skin_lane_center_gap_scale(config_.skin, skin_edit_mode_);
    const double preview_lane_width_scale =
        (static_cast<std::size_t>(skin_edit_lane_) < preview_lane_width_scales.size())
            ? preview_lane_width_scales[static_cast<std::size_t>(skin_edit_lane_)]
            : config::kLaneWidthScaleDefault;
    const double preview_lane_spacing_scale =
        (static_cast<std::size_t>(skin_edit_gap_) < preview_lane_spacing_scales.size())
            ? preview_lane_spacing_scales[static_cast<std::size_t>(skin_edit_gap_)]
            : config::kLaneSpacingScaleDefault;
    const bool center_gap_available = config::normalize_skin_mode_token(skin_edit_mode_) == "16k";
    const std::string active_skin_source = config::normalize_skin_source_token(config_.skin.source);
    const bool lr2_source = active_skin_source == "lr2";
    const int lr2_shift = lr2_source ? 1 : 0;
    const std::string imported_skin_row_label =
        (active_skin_source == "osu") ? ui_text("OSU Skin", "OSU 스킨")
                                      : ((active_skin_source == "lr2") ? ui_text("LR2 Skin", "LR2 스킨")
                                                                       : ui_text("Imported Skin", "가져온 스킨"));
    std::string imported_skin_value = ui_text("N/A", "없음");
    if (active_skin_source == "osu") {
        imported_skin_value = available_osu_skin_names_.empty()
                                  ? ui_text("Not Found", "없음")
                                  : (config_.skin.osu_skin_name.empty() ? available_osu_skin_names_.front()
                                                                        : config_.skin.osu_skin_name);
    } else if (active_skin_source == "lr2") {
        imported_skin_value = available_lr2_skin_names_.empty()
                                  ? ui_text("Not Found", "없음")
                                  : (config_.skin.lr2_skin_name.empty() ? available_lr2_skin_names_.front()
                                                                        : config_.skin.lr2_skin_name);
    }

    append_menu_row(render.generic, ui_text("Skin Source", "스킨 소스"), ui_skin_source_label(active_skin_source), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, imported_skin_row_label, imported_skin_value, settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow, 1, false, true);
    if (lr2_source) {
        append_menu_row(render.generic, ui_text("LR2 Resolution", "LR2 해상도"),
                        ui_uses_korean()
                            ? ((config::normalize_skin_lr2_resolution_mode_token(config_.skin.lr2_resolution_mode) == "auto")
                                   ? std::string("자동")
                                   : lr2_resolution_mode_label(config_.skin.lr2_resolution_mode))
                            : lr2_resolution_mode_label(config_.skin.lr2_resolution_mode),
                        settings_cursor_ == 2, render::MenuHitTargetKind::SettingsRow, 2, false, true);
    }
    append_menu_row(render.generic, ui_text("Import Skin", "스킨 가져오기"), ui_text("Open Folder", "폴더 열기"), settings_cursor_ == 2 + lr2_shift,
                    render::MenuHitTargetKind::SettingsRow, 2 + lr2_shift, true, false);
    append_menu_row(render.generic, ui_text("Key Mode", "키 모드"), ui_key_mode_label(skin_edit_mode_), settings_cursor_ == 3 + lr2_shift,
                    render::MenuHitTargetKind::SettingsRow, 3 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Target Lane", "대상 레인"),
                    ui_text("Lane ", "레인 ") + std::to_string(skin_edit_lane_ + 1) + " / " + std::to_string(lane_count),
                    settings_cursor_ == 4 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 4 + lr2_shift, false, true);
    append_menu_row(render.generic,
                    ui_text("Target Gap", "대상 간격"),
                    (gap_count > 0)
                        ? (std::to_string(skin_edit_gap_ + 1) + "-" + std::to_string(skin_edit_gap_ + 2) +
                           " / " + std::to_string(gap_count))
                        : ui_text("N/A", "없음"),
                    settings_cursor_ == 5 + lr2_shift,
                    render::MenuHitTargetKind::SettingsRow,
                    5 + lr2_shift,
                    false,
                    gap_count > 0);
    append_menu_row(render.generic, ui_text("Lane Color", "레인 색상"),
                    config::skin_color_label(preview_lane_colors[static_cast<std::size_t>(skin_edit_lane_)]),
                    settings_cursor_ == 6 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 6 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Note Shape", "노트 모양"), ui_skin_note_shape_label(config_.skin.note_shape),
                    settings_cursor_ == 7 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 7 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Note Border", "노트 테두리"), ui_on_off(config_.skin.note_border_enabled),
                    settings_cursor_ == 8 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 8 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Image Aspect", "이미지 비율"), ui_on_off(config_.skin.preserve_note_image_aspect_ratio),
                    settings_cursor_ == 9 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 9 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("White Dividers", "흰 레인 구분선"), ui_on_off(config_.skin.show_lane_dividers),
                    settings_cursor_ == 10 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 10 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Judgement Line", "판정선 표시"), ui_on_off(config_.skin.show_judgement_line),
                    settings_cursor_ == 11 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 11 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Gear Boundary", "기어 경계선"), ui_on_off(config_.skin.show_gear_boundary_line),
                    settings_cursor_ == 12 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 12 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("LN Tail Taper", "LN 꼬리 테이퍼"), ui_on_off(config_.skin.hold_tail_taper_enabled),
                    settings_cursor_ == 13 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 13 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Visual Preset", "비주얼 프리셋"),
                    config::skin_visual_preset_label(config_.skin.visual_preset),
                    settings_cursor_ == 14 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 14 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Lane BG Alpha", "레인 배경 투명도"),
                    format_percent(config_.skin.lane_background_opacity),
                    settings_cursor_ == 15 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 15 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Visual Opacity", "전체 투명도"),
                    format_percent(config_.skin.visual_opacity),
                    settings_cursor_ == 16 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 16 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Outline Alpha", "외곽선 투명도"),
                    format_percent(config_.skin.note_outline_opacity),
                    settings_cursor_ == 17 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 17 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("LN Body Alpha", "LN 몸통 투명도"),
                    format_percent(config_.skin.hold_body_opacity),
                    settings_cursor_ == 18 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 18 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Judge Glow", "판정선 글로우"), ui_on_off(config_.skin.judgement_line_glow_enabled),
                    settings_cursor_ == 19 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 19 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Key Pulse", "키 펄스"), ui_on_off(config_.skin.key_pulse_enabled),
                    settings_cursor_ == 20 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 20 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Key Labels", "키 이름"),
                    config::skin_key_label_position_label(config_.skin.key_label_position),
                    settings_cursor_ == 21 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 21 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Judge Line", "판정선 위치"), format_percent(config_.skin.judgement_line_position),
                    settings_cursor_ == 22 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 22 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Lane Width", "레인 너비"), format_percent(preview_lane_width_scale),
                    settings_cursor_ == 23 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 23 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Note Width", "노트 너비"), format_percent(preview_note_width_scale),
                    settings_cursor_ == 24 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 24 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Lane Spacing", "레인 간격"), format_percent(preview_lane_spacing_scale),
                    settings_cursor_ == 25 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 25 + lr2_shift, false, gap_count > 0);
    append_menu_row(render.generic, ui_text("Divider Width", "구분선 너비"), format_percent(preview_lane_divider_width_scale),
                    settings_cursor_ == 26 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 26 + lr2_shift, false, true);
    append_menu_row(render.generic,
                    ui_text("16K Center Gap", "16K 중앙 간격"),
                    center_gap_available ? format_percent(preview_lane_center_gap_scale) : ui_text("16K Only", "16K 전용"),
                    settings_cursor_ == 27 + lr2_shift,
                    render::MenuHitTargetKind::SettingsRow,
                    27 + lr2_shift,
                    false,
                    center_gap_available);
    append_menu_row(render.generic, ui_text("LN Body Width", "LN 몸통 너비"), format_percent(config_.skin.hold_body_width_scale),
                    settings_cursor_ == 28 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 28 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Note Height", "노트 높이"), format_percent(preview_note_height_scale),
                    settings_cursor_ == 29 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 29 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Combo Y", "콤보 Y"), format_percent(config_.skin.combo_position),
                    settings_cursor_ == 30 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 30 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Back", "뒤로"), "", settings_cursor_ == 31 + lr2_shift,
                    render::MenuHitTargetKind::SettingsRow, 31 + lr2_shift, true, false);

    render.generic.skin_preview.visible = true;
    render.generic.skin_preview.mode_label = ui_key_mode_label(skin_edit_mode_);
    render.generic.skin_preview.selected_color_label =
        config::skin_color_label(preview_lane_colors[static_cast<std::size_t>(skin_edit_lane_)]);
    render.generic.skin_preview.lane_count = lane_count;
    render.generic.skin_preview.selected_lane = clamp_int(skin_edit_lane_ + 1, 1, lane_count);
    render.generic.skin_preview.selected_gap = (gap_count > 0) ? clamp_int(skin_edit_gap_ + 1, 1, gap_count) : -1;
    render.generic.skin_preview.judgement_line_position = std::clamp(
        config_.skin.judgement_line_position,
        config::kJudgementLinePositionMin,
        config::kJudgementLinePositionMax);
    render.generic.skin_preview.combo_position = std::clamp(
        config_.skin.combo_position,
        config::kComboPositionMin,
        config::kComboPositionMax);
    render.generic.skin_preview.lane_width_scale_count =
        std::min(preview_lane_width_scales.size(), render.generic.skin_preview.lane_width_scales.size());
    render.generic.skin_preview.lane_width_scales.fill(config::kLaneWidthScaleDefault);
    for (std::size_t lane = 0; lane < render.generic.skin_preview.lane_width_scale_count; ++lane) {
        render.generic.skin_preview.lane_width_scales[lane] = preview_lane_width_scales[lane];
    }
    render.generic.skin_preview.note_width_scale = preview_note_width_scale;
    render.generic.skin_preview.lane_spacing_scale_count =
        std::min(preview_lane_spacing_scales.size(), render.generic.skin_preview.lane_spacing_scales.size());
    render.generic.skin_preview.lane_spacing_scales.fill(config::kLaneSpacingScaleDefault);
    for (std::size_t gap = 0; gap < render.generic.skin_preview.lane_spacing_scale_count; ++gap) {
        render.generic.skin_preview.lane_spacing_scales[gap] = preview_lane_spacing_scales[gap];
    }
    render.generic.skin_preview.note_height_scale = preview_note_height_scale;
    render.generic.skin_preview.lane_divider_width_scale = preview_lane_divider_width_scale;
    render.generic.skin_preview.lane_center_gap_scale = center_gap_available ? preview_lane_center_gap_scale : 0.0;
    render.generic.skin_preview.hold_body_width_scale = config_.skin.hold_body_width_scale;
    render.generic.skin_preview.show_lane_dividers = config_.skin.show_lane_dividers;
    render.generic.skin_preview.show_judgement_line = config_.skin.show_judgement_line;
    render.generic.skin_preview.show_gear_boundary_line = config_.skin.show_gear_boundary_line;
    render.generic.skin_preview.hold_tail_taper_enabled = config_.skin.hold_tail_taper_enabled;
    render.generic.skin_preview.judgement_line_glow_enabled = config_.skin.judgement_line_glow_enabled;
    render.generic.skin_preview.key_pulse_enabled = config_.skin.key_pulse_enabled;
    render.generic.skin_preview.key_label_position =
        config::normalize_skin_key_label_position_token(config_.skin.key_label_position);
    render.generic.skin_preview.note_border_enabled = config_.skin.note_border_enabled;
    render.generic.skin_preview.note_shape = config_.skin.note_shape;
    render.generic.skin_preview.preserve_note_image_aspect_ratio =
        config_.skin.preserve_note_image_aspect_ratio;
    render.generic.skin_preview.skin_source = active_skin_source;
    render.generic.skin_preview.external_skin_root = active_external_skin_root();
    render.generic.skin_preview.external_skin_name = active_external_skin_name();
    render.generic.skin_preview.lr2_resolution_override =
        config::normalize_skin_lr2_resolution_mode_token(config_.skin.lr2_resolution_mode);
    render.generic.skin_preview.lane_background_opacity = std::clamp(
        config_.skin.lane_background_opacity,
        config::kSkinLaneBackgroundOpacityMin,
        config::kSkinLaneBackgroundOpacityMax);
    render.generic.skin_preview.visual_opacity = std::clamp(
        config_.skin.visual_opacity,
        config::kSkinVisualOpacityMin,
        config::kSkinVisualOpacityMax);
    render.generic.skin_preview.note_outline_opacity = std::clamp(
        config_.skin.note_outline_opacity,
        config::kSkinNoteOutlineOpacityMin,
        config::kSkinNoteOutlineOpacityMax);
    render.generic.skin_preview.hold_body_opacity = std::clamp(
        config_.skin.hold_body_opacity,
        config::kSkinHoldBodyOpacityMin,
        config::kSkinHoldBodyOpacityMax);
    render.generic.skin_preview.lane_colors.fill(0);
    for (int lane = 0; lane < lane_count && lane < static_cast<int>(kGameplayHudMaxLanes); ++lane) {
        render.generic.skin_preview.lane_colors[static_cast<std::size_t>(lane)] =
            config::skin_color_rgb(preview_lane_colors[static_cast<std::size_t>(lane)]);
    }

    render.generic.notes.push_back(ui_text("Skin Source switches between the native vector skin, imported osu!mania PNG skins, and imported LR2 playskins.",
                                           "스킨 소스는 기본 벡터 스킨, 가져온 osu!mania PNG 스킨, 가져온 LR2 플레이스킨 사이를 전환합니다."));
    render.generic.notes.push_back(ui_text("Imported OSU skins scan profile skins first, then build/Release/test-skins-osu as a fallback test root.",
                                           "가져온 OSU 스킨은 먼저 프로필 스킨 폴더를 찾고, 없으면 build/Release/test-skins-osu를 테스트 루트로 사용합니다."));
    render.generic.notes.push_back(ui_text("Imported LR2 skins scan profile skins first, then build/Release/test-skins-lr2 as a fallback test root.",
                                           "가져온 LR2 스킨은 먼저 프로필 스킨 폴더를 찾고, 없으면 build/Release/test-skins-lr2를 테스트 루트로 사용합니다."));
    render.generic.notes.push_back(ui_text("LR2 Resolution overrides the imported LR2 family before the auto-detected layout is applied.",
                                           "LR2 해상도는 자동 감지 레이아웃을 적용하기 전에 가져온 LR2 계열 해상도를 덮어씁니다."));
    render.generic.notes.push_back(ui_text("Import Skin opens a folder picker. You can also drag and drop a skin folder onto this screen.",
                                           "스킨 가져오기는 폴더 선택 창을 엽니다. 이 화면에 스킨 폴더를 드래그 앤 드롭해도 됩니다."));
    render.generic.notes.push_back(ui_text("LR2 porting imports note, LN, lane-gap, and destination-size data from default active branches in the playskin.",
                                           "LR2 포팅은 플레이스킨의 기본 활성 브랜치에서 노트, LN, 레인 간격, 대상 크기 데이터를 가져옵니다."));
    render.generic.notes.push_back(ui_text("Image Aspect keeps imported head and tail art from stretching to the gameplay note box.",
                                           "이미지 비율은 가져온 헤드/테일 이미지를 게임 노트 박스에 맞출 때 늘어나지 않도록 유지합니다."));
    render.generic.notes.push_back(ui_text("White Dividers, Judgement Line, and Gear Boundary can be toggled independently.",
                                           "흰 레인 구분선, 판정선, 기어 경계선은 각각 독립적으로 켜고 끌 수 있습니다."));
    render.generic.notes.push_back(ui_text("LN Tail Taper only changes visuals: the hold body narrows toward the tail without changing timing or hitboxes.",
                                           "LN 꼬리 테이퍼는 시각 효과만 바꿉니다. 판정이나 히트박스는 그대로 두고 홀드 몸통만 꼬리 쪽으로 좁아집니다."));
    render.generic.notes.push_back(ui_text("Divider Width is shared across all key modes, and it scales the white lane separators plus any imported divider widths.",
                                           "구분선 너비는 모든 키 모드에 공용으로 적용되며, 흰 레인 구분선과 외부 스킨이 제공하는 구분선 폭에 함께 배율로 적용됩니다."));
    render.generic.notes.push_back(ui_text("Lane Width changes the selected lane's field width, while Note Width still scales the note body inside each lane.",
                                           "레인 너비는 선택한 레인의 필드 폭을 바꾸고, 노트 너비는 각 레인 안의 노트 폭만 따로 조절합니다."));
    render.generic.notes.push_back(ui_text("Lane Spacing adds blank space after the selected gap, so you can open individual separators without changing note timing.",
                                           "레인 간격은 선택한 구간 뒤에 빈 공간을 더해 개별 구분 간격을 벌리며, 노트 타이밍에는 영향을 주지 않습니다."));
    render.generic.notes.push_back(ui_text("16K Center Gap inserts a blank center lane-width gap between the left and right halves of the 16-key field.",
                                           "16K 중앙 간격은 16키 필드의 좌우 묶음 사이에 빈 중앙 간격을 추가합니다."));
    render.generic.notes.push_back(ui_text("Key Mode, Target Lane, and Target Gap edit per-layout fallback geometry before imported divider art is applied.",
                                           "키 모드, 대상 레인, 대상 간격은 외부 스킨 구분선 아트를 적용하기 전 레이아웃별 기본 지오메트리를 편집합니다."));
}

}  // namespace tenriff::app
