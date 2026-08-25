#include "app/MenuApp.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>
#endif

#include "app/Lr2Skin.h"
#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSkinUtils.h"
#include "app/MenuSongUtils.h"
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
std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
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

std::string cycle_hit_burst_style(std::string_view value, int direction) {
    static constexpr std::array<std::string_view, 3> kStyles = {"prism", "ring", "spark"};
    const std::string normalized = config::normalize_skin_hit_burst_style_token(value);
    int index = 0;
    for (int i = 0; i < static_cast<int>(kStyles.size()); ++i) {
        if (kStyles[static_cast<std::size_t>(i)] == normalized) {
            index = i;
            break;
        }
    }
    index = (index + direction + static_cast<int>(kStyles.size())) %
            static_cast<int>(kStyles.size());
    return std::string(kStyles[static_cast<std::size_t>(index)]);
}

fs::path lr2_skin_import_root_path(std::string_view profile_dir) {
    return path_from_utf8(profile_dir) / "skins" / "lr2";
}

fs::path tenriff_skin_import_root_path(std::string_view profile_dir) {
    return path_from_utf8(profile_dir) / "skins" / "tenriff";
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

bool open_folder_in_shell(std::string_view path_utf8) {
    const fs::path path = path_from_utf8(path_utf8);
    std::error_code ec;
    if (path.empty() || !fs::is_directory(path, ec) || ec) {
        return false;
    }
    const HINSTANCE opened = ShellExecuteW(nullptr,
                                           L"open",
                                           path.c_str(),
                                           nullptr,
                                           nullptr,
                                           SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(opened) > 32;
}


#endif

}  // namespace

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

void MenuApp::refresh_available_tenriff_skins() {
    available_tenriff_skin_root_ = tenriff_skin_import_root_path(profile_dir_).u8string();
    available_tenriff_skin_names_ = list_tenriff_skin_names(available_tenriff_skin_root_);
    active_tenriff_skin_ = {};
    active_tenriff_skin_modes_.fill(TenRiffSkinDefinition{});
    active_tenriff_gameplay_modes_.fill(nullptr);

    if (available_tenriff_skin_names_.empty()) {
        config_.skin.tenriff_skin_name.clear();
        skin_status_messages_.clear();
        return;
    }
    if (config_.skin.tenriff_skin_name.empty() ||
        std::find(available_tenriff_skin_names_.begin(),
                  available_tenriff_skin_names_.end(),
                  config_.skin.tenriff_skin_name) == available_tenriff_skin_names_.end()) {
        config_.skin.tenriff_skin_name = available_tenriff_skin_names_.front();
    }
    for (int keys = 1; keys <= 16; ++keys) {
        auto definition = resolve_tenriff_skin(
            available_tenriff_skin_root_, config_.skin.tenriff_skin_name, keys);
        if (definition.found) {
            active_tenriff_gameplay_modes_[static_cast<std::size_t>(keys - 1)] =
                std::make_shared<const ImportedGameplaySkinDefinition>(definition.gameplay);
        }
        active_tenriff_skin_modes_[static_cast<std::size_t>(keys - 1)] =
            std::move(definition);
    }
    if (const auto* selected = active_tenriff_skin_for_keys(
            lane_count_for_skin_mode(skin_edit_mode_))) {
        active_tenriff_skin_ = *selected;
    }
    for (const auto& mode_definition : active_tenriff_skin_modes_) {
        for (const auto& path : mode_definition.referenced_asset_paths) {
            if (std::find(active_tenriff_skin_.referenced_asset_paths.begin(),
                          active_tenriff_skin_.referenced_asset_paths.end(), path) ==
                active_tenriff_skin_.referenced_asset_paths.end()) {
                active_tenriff_skin_.referenced_asset_paths.push_back(path);
            }
        }
    }
    skin_status_messages_.clear();
    for (const auto& warning : active_tenriff_skin_.warnings) {
        std::cerr << "[warn] TenRiff skin: " << warning << std::endl;
        if (skin_status_messages_.size() < 4u) {
            skin_status_messages_.push_back(ui_text("Warning: ", "경고: ") + warning);
        }
    }
    if (active_tenriff_skin_.warnings.size() > skin_status_messages_.size()) {
        skin_status_messages_.push_back(
            ui_text("More warnings are available in the log.", "추가 경고는 로그에서 확인할 수 있습니다."));
    }
}

const TenRiffSkinDefinition* MenuApp::active_tenriff_skin_for_keys(int keys) const {
    const int clamped = clamp_int(keys, 1, 16);
    const auto& definition = active_tenriff_skin_modes_[static_cast<std::size_t>(clamped - 1)];
    return definition.found ? &definition : nullptr;
}

std::shared_ptr<const ImportedGameplaySkinDefinition>
MenuApp::active_tenriff_gameplay_for_keys(int keys) const {
    const int clamped = clamp_int(keys, 1, 16);
    return active_tenriff_gameplay_modes_[static_cast<std::size_t>(clamped - 1)];
}

bool MenuApp::import_lr2_skin_path(std::string_view source_path) {
    const std::string normalized_source = menu_songs::normalize_song_source_path(std::string(source_path));
    if (normalized_source.empty()) {
        return false;
    }

    const fs::path import_root = lr2_skin_import_root_path(profile_dir_);
    const Lr2SkinImportResult imported =
        import_lr2_skin_tree(normalized_source, import_root.u8string());
    for (const auto& warning : imported.warnings) {
        std::cerr << "[warn] " << warning << std::endl;
    }
    if (!imported.success()) {
        skin_status_messages_.clear();
        for (const auto& warning : imported.warnings) {
            if (skin_status_messages_.size() >= 4u) break;
            skin_status_messages_.push_back(ui_text("Import failed: ", "가져오기 실패: ") + warning);
        }
        return false;
    }

    // Candidate ordering is deterministic. A Theme bundle activates its first
    // installed playskin while leaving every imported sibling available in the
    // Imported Skin row.
    config_.skin.lr2_skin_name = imported.skin_names.front();
    refresh_available_lr2_skins();
    refresh_available_tenriff_skins();
    skin_status_messages_ = {
        ui_text("LR2 skin import completed.", "LR2 스킨 가져오기를 완료했습니다.")
    };
    skin_dirty_ = true;
    std::cerr << "[info] Imported " << imported.skin_names.size()
              << " LR2 skin(s), files=" << imported.copied_files
              << " bytes=" << imported.copied_bytes << "." << std::endl;
    return true;
}

bool MenuApp::import_tenriff_skin_path(std::string_view source_path) {
    const TenRiffSkinImportResult imported = import_tenriff_skin(
        source_path, tenriff_skin_import_root_path(profile_dir_).u8string());
    for (const auto& warning : imported.warnings) {
        std::cerr << "[warn] " << warning << std::endl;
    }
    if (!imported.success()) {
        skin_status_messages_.clear();
        for (const auto& warning : imported.warnings) {
            if (skin_status_messages_.size() >= 4u) break;
            skin_status_messages_.push_back(ui_text("Import failed: ", "가져오기 실패: ") + warning);
        }
        return false;
    }

    config_.skin.tenriff_skin_name = imported.skin_name;
    refresh_available_tenriff_skins();
    ++tenriff_skin_revision_;
    std::vector<std::string> status = {
        ui_text("Imported and activated: ", "가져오고 활성화했습니다: ") + imported.skin_name
    };
    for (const auto& warning : imported.warnings) {
        if (status.size() >= 4u) break;
        status.push_back(ui_text("Warning: ", "경고: ") + warning);
    }
    skin_status_messages_ = std::move(status);
    skin_dirty_ = true;
    std::cerr << "[info] Imported TenRiff skin " << imported.skin_name
              << ", files=" << imported.copied_files
              << " bytes=" << imported.copied_bytes << "." << std::endl;
    return true;
}

bool MenuApp::import_skin_path_auto(std::string_view source_path) {
    fs::path candidate = path_from_utf8(source_path);
    std::error_code ec;
    const bool tenriff_manifest_selected =
        lower_ascii(candidate.filename().u8string()) == "skin.json";
    if (!tenriff_manifest_selected && fs::is_directory(candidate, ec) && !ec) {
        candidate /= "skin.json";
    }
    if ((tenriff_manifest_selected || fs::is_regular_file(candidate, ec)) && !ec &&
        import_tenriff_skin_path(source_path)) {
        config_.skin.source = "tenriff";
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
    if (source == "lr2") {
        return available_lr2_skin_root_;
    }
    if (source == "tenriff") {
        return available_tenriff_skin_root_;
    }
    return {};
}

std::string MenuApp::active_external_skin_name() const {
    const std::string source = config::normalize_skin_source_token(config_.skin.source);
    if (source == "lr2") {
        return config_.skin.lr2_skin_name;
    }
    if (source == "tenriff") {
        return config_.skin.tenriff_skin_name;
    }
    return {};
}

void MenuApp::handle_skins_settings_input(uint32_t keycode) {
    const std::string active_skin_source = config::normalize_skin_source_token(config_.skin.source);
    const bool lr2_source = active_skin_source == "lr2";
    const SkinSettingsRows rows{lr2_source};
    const int item_count = rows.count();
    const int key_mode_row = rows.index_of(SkinSettingsRowId::KeyMode);
    const int skin_source_row = rows.index_of(SkinSettingsRowId::SkinSource);
    const int imported_skin_row = rows.index_of(SkinSettingsRowId::ImportedSkin);
    const int lr2_resolution_row = rows.index_of(SkinSettingsRowId::Lr2Resolution);
    const int import_skin_row = rows.index_of(SkinSettingsRowId::ImportSkin);
    const int create_skin_row = rows.index_of(SkinSettingsRowId::CreateSkin);
    const int open_skin_folder_row = rows.index_of(SkinSettingsRowId::OpenSkinFolder);
    const int reload_skin_row = rows.index_of(SkinSettingsRowId::ReloadSkin);
    const int target_lane_row = rows.index_of(SkinSettingsRowId::TargetLane);
    const int target_gap_row = rows.index_of(SkinSettingsRowId::TargetGap);
    const int lane_color_row = rows.index_of(SkinSettingsRowId::LaneColor);
    const int single_color_row = rows.index_of(SkinSettingsRowId::SingleColor);
    const int note_shape_row = rows.index_of(SkinSettingsRowId::NoteShape);
    const int note_border_row = rows.index_of(SkinSettingsRowId::NoteBorder);
    const int image_aspect_row = rows.index_of(SkinSettingsRowId::ImageAspect);
    const int lane_dividers_row = rows.index_of(SkinSettingsRowId::LaneDividers);
    const int judgement_line_row = rows.index_of(SkinSettingsRowId::JudgementLine);
    const int gear_boundary_row = rows.index_of(SkinSettingsRowId::GearBoundary);
    const int show_hold_tail_row = rows.index_of(SkinSettingsRowId::ShowHoldTail);
    const int ln_tail_taper_row = rows.index_of(SkinSettingsRowId::LnTailTaper);
    const int visual_preset_row = rows.index_of(SkinSettingsRowId::VisualPreset);
    const int lane_background_opacity_row = rows.index_of(SkinSettingsRowId::LaneBackgroundOpacity);
    const int visual_opacity_row = rows.index_of(SkinSettingsRowId::VisualOpacity);
    const int note_outline_opacity_row = rows.index_of(SkinSettingsRowId::NoteOutlineOpacity);
    const int ln_body_opacity_row = rows.index_of(SkinSettingsRowId::LnBodyOpacity);
    const int judgement_line_glow_row = rows.index_of(SkinSettingsRowId::JudgementLineGlow);
    const int hit_burst_style_row = rows.index_of(SkinSettingsRowId::HitBurstStyle);
    const int key_pulse_row = rows.index_of(SkinSettingsRowId::KeyPulse);
    const int key_label_position_row = rows.index_of(SkinSettingsRowId::KeyLabelPosition);
    const int judge_line_row = rows.index_of(SkinSettingsRowId::JudgeLinePosition);
    const int lane_width_row = rows.index_of(SkinSettingsRowId::LaneWidth);
    const int note_width_row = rows.index_of(SkinSettingsRowId::NoteWidth);
    const int lane_spacing_row = rows.index_of(SkinSettingsRowId::LaneSpacing);
    const int divider_width_row = rows.index_of(SkinSettingsRowId::DividerWidth);
    const int center_gap_row = rows.index_of(SkinSettingsRowId::CenterGap);
    const int ln_body_width_row = rows.index_of(SkinSettingsRowId::LnBodyWidth);
    const int note_height_row = rows.index_of(SkinSettingsRowId::NoteHeight);
    const int combo_y_row = rows.index_of(SkinSettingsRowId::ComboY);
    const int black_playfield_row = rows.index_of(SkinSettingsRowId::BlackPlayfield);
    const int ui_font_row = rows.index_of(SkinSettingsRowId::UiFont);
    const int visual_latency_row = rows.index_of(SkinSettingsRowId::VisualLatency);
    const int note_gap_row = rows.index_of(SkinSettingsRowId::NoteGap);
    const int gameplay_cursor_row = rows.index_of(SkinSettingsRowId::GameplayCursor);
    const int timing_feedback_row = rows.index_of(SkinSettingsRowId::TimingFeedback);
    const int back_row = rows.index_of(SkinSettingsRowId::Back);

    if (keycode == key_f5_) {
        refresh_available_tenriff_skins();
        ++tenriff_skin_revision_;
        skin_status_messages_.insert(
            skin_status_messages_.begin(),
            ui_text("Reloaded the current skin from disk.", "현재 스킨을 디스크에서 다시 불러왔습니다."));
        if (skin_status_messages_.size() > 5u) skin_status_messages_.resize(5u);
        publish_snapshot();
        return;
    }

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

    if (settings_cursor_ == skin_source_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        const bool was_lr2 = active_skin_source == "lr2";
        config_.skin.source = cycle_skin_source(config_.skin.source, direction);
        const bool is_lr2 = config::normalize_skin_source_token(config_.skin.source) == "lr2";
        const int new_item_count = skin_settings_row_count(is_lr2);
        if (!was_lr2 && is_lr2 && settings_cursor_ >= import_skin_row) {
            ++settings_cursor_;
        } else if (was_lr2 && !is_lr2 && settings_cursor_ >= lr2_resolution_row) {
            --settings_cursor_;
        }
        settings_cursor_ = clamp_int(settings_cursor_, 0, new_item_count - 1);
        refresh_available_lr2_skins();
        refresh_available_tenriff_skins();
        ++tenriff_skin_revision_;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == imported_skin_row && (keycode == key_left_ || keycode == key_right_)) {
        std::vector<std::string>* names = nullptr;
        std::string* selected_name = nullptr;
        if (active_skin_source == "lr2") {
            names = &available_lr2_skin_names_;
            selected_name = &config_.skin.lr2_skin_name;
        } else if (active_skin_source == "tenriff") {
            names = &available_tenriff_skin_names_;
            selected_name = &config_.skin.tenriff_skin_name;
        }
        if (names != nullptr && selected_name != nullptr && !names->empty()) {
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
            if (active_skin_source == "lr2") {
                refresh_available_lr2_skins();
            } else {
                refresh_available_tenriff_skins();
                ++tenriff_skin_revision_;
            }
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
        const auto picked_path = pick_folder_dialog_utf8();
        if (picked_path.has_value()) {
            if (!import_skin_path_auto(*picked_path)) {
                std::cerr << "[warn] Selected path is not a supported TenRiff or LR2 skin folder: "
                          << *picked_path << std::endl;
                skin_status_messages_ = {
                    ui_text("Import failed: unsupported or invalid skin folder.",
                            "가져오기 실패: 지원하지 않거나 잘못된 스킨 폴더입니다.")
                };
            }
            publish_snapshot();
        }
#endif
        return;
    }
    if (settings_cursor_ == create_skin_row && keycode == key_enter_) {
        const TenRiffSkinCreateResult created = create_tenriff_skin_template(
            tenriff_skin_import_root_path(profile_dir_).u8string());
        if (!created.success()) {
            skin_status_messages_.clear();
            for (const auto& warning : created.warnings) {
                if (skin_status_messages_.size() >= 4u) break;
                skin_status_messages_.push_back(ui_text("Create failed: ", "생성 실패: ") + warning);
            }
            if (skin_status_messages_.empty()) {
                skin_status_messages_.push_back(
                    ui_text("Could not create a new skin.", "새 스킨을 만들 수 없습니다."));
            }
            publish_snapshot();
            return;
        }
        config_.skin.source = "tenriff";
        config_.skin.tenriff_skin_name = created.skin_name;
        refresh_available_tenriff_skins();
        ++tenriff_skin_revision_;
        skin_dirty_ = true;
        skin_status_messages_ = {
            ui_text("Created an editable skin: ", "편집 가능한 스킨을 만들었습니다: ") + created.skin_name,
            ui_text("Add standard-named images, then press F5 to reload.",
                    "표준 파일명으로 이미지를 넣은 뒤 F5로 다시 불러오세요.")
        };
#ifdef _WIN32
        static_cast<void>(open_folder_in_shell(created.folder_path));
#endif
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == open_skin_folder_row && keycode == key_enter_) {
#ifdef _WIN32
        bool opened = false;
        if (active_skin_source == "tenriff" && !config_.skin.tenriff_skin_name.empty()) {
            opened = open_folder_in_shell(
                (path_from_utf8(available_tenriff_skin_root_) /
                 path_from_utf8(config_.skin.tenriff_skin_name)).u8string());
        }
        skin_status_messages_ = {
            opened ? ui_text("Opened the active skin folder.", "현재 스킨 폴더를 열었습니다.")
                   : ui_text("No editable TenRiff skin folder is active.",
                             "편집할 TenRiff 스킨 폴더가 활성화되어 있지 않습니다.")
        };
#endif
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == reload_skin_row && keycode == key_enter_) {
        refresh_available_tenriff_skins();
        ++tenriff_skin_revision_;
        skin_status_messages_.insert(
            skin_status_messages_.begin(),
            ui_text("Reloaded the current skin from disk.", "현재 스킨을 디스크에서 다시 불러왔습니다."));
        if (skin_status_messages_.size() > 5u) skin_status_messages_.resize(5u);
        publish_snapshot();
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
        if (active_skin_source == "tenriff") {
            refresh_available_tenriff_skins();
        }
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
        if (config::normalize_skin_single_color_token(config_.skin.single_color) != "off") {
            publish_snapshot();
            return;
        }
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
    if (settings_cursor_ == single_color_row && (keycode == key_left_ || keycode == key_right_)) {
        std::vector<std::string> options{"off"};
        const auto palette = config::supported_skin_color_tokens();
        options.insert(options.end(), palette.begin(), palette.end());
        const std::string current = config::normalize_skin_single_color_token(config_.skin.single_color);
        auto it = std::find(options.begin(), options.end(), current);
        int index = it == options.end() ? 0 : static_cast<int>(std::distance(options.begin(), it));
        index += keycode == key_left_ ? -1 : 1;
        if (index < 0) {
            index = static_cast<int>(options.size()) - 1;
        } else if (index >= static_cast<int>(options.size())) {
            index = 0;
        }
        config_.skin.single_color = options[static_cast<std::size_t>(index)];
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == note_shape_row && (keycode == key_left_ || keycode == key_right_)) {
        constexpr std::array<std::string_view, 8> shapes{
            "rect", "square", "circle", "diamond", "arrow", "triangle", "pentagon", "hexagon"};
        const std::string current = config::normalize_skin_note_shape_token(config_.skin.note_shape);
        auto it = std::find(shapes.begin(), shapes.end(), current);
        int index = (it == shapes.end()) ? 0 : static_cast<int>(std::distance(shapes.begin(), it));
        index += (keycode == key_left_) ? -1 : 1;
        if (index < 0) index = static_cast<int>(shapes.size()) - 1;
        if (index >= static_cast<int>(shapes.size())) index = 0;
        config_.skin.note_shape = std::string(shapes[static_cast<std::size_t>(index)]);
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
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.note_image_aspect =
            cycle_skin_note_image_aspect(config_.skin.note_image_aspect, direction);
        config_.skin.preserve_note_image_aspect_ratio = config_.skin.note_image_aspect != "stretch";
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
    if (settings_cursor_ == show_hold_tail_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.show_hold_tail = !config_.skin.show_hold_tail;
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
    if (settings_cursor_ == hit_burst_style_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.hit_burst_style = cycle_hit_burst_style(config_.skin.hit_burst_style, direction);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == key_pulse_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.key_pulse_brightness = clamp_step_value(
            config_.skin.key_pulse_brightness + static_cast<double>(direction) * kSkinOpacityStep,
            config::kSkinKeyPulseBrightnessMin,
            config::kSkinKeyPulseBrightnessMax,
            kSkinOpacityStep);
        config_.skin.key_pulse_enabled = config_.skin.key_pulse_brightness > 0.0;
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
    if (settings_cursor_ == black_playfield_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.black_playfield_enabled = !config_.skin.black_playfield_enabled;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == ui_font_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.ui_font = cycle_skin_ui_font(config_.skin.ui_font, direction);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == visual_latency_row &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.visual_offset_ms = clamp_step_value(
            config_.visual_offset_ms + static_cast<double>(direction) * kVisualOffsetStep,
            kVisualOffsetMin, kVisualOffsetMax, kVisualOffsetStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == note_gap_row && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.note_divider_gap_px = clamp_step_value(
            config_.skin.note_divider_gap_px +
                static_cast<double>(direction) * config::kNoteDividerGapPxStep,
            config::kNoteDividerGapPxMin,
            config::kNoteDividerGapPxMax,
            config::kNoteDividerGapPxStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == gameplay_cursor_row && (keycode == key_left_ || keycode == key_right_)) {
        config_.ui.show_cursor_in_gameplay = !config_.ui.show_cursor_in_gameplay;
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == timing_feedback_row &&
        (keycode == key_left_ || keycode == key_right_)) {
        config_.skin.show_timing_feedback = !config_.skin.show_timing_feedback;
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
    const auto preview_lane_palette = config::resolved_skin_lane_palette(config_.skin, skin_edit_mode_);
    const auto preview_lane_colors = config::resolved_skin_lane_colors(config_.skin, skin_edit_mode_);
    const bool single_color_enabled =
        config::normalize_skin_single_color_token(config_.skin.single_color) != "off";
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
    const SkinSettingsRows stable_rows{lr2_source};
    const TenRiffSkinGameplayStyle* manifest_style =
        active_skin_source == "tenriff" && active_tenriff_skin_.found
            ? &active_tenriff_skin_.gameplay_style
            : nullptr;
    auto style_bool = [&](const std::optional<bool>& value, bool fallback) {
        return manifest_style && value.has_value() ? *value : fallback;
    };
    auto style_number = [&](const std::optional<float>& value, double fallback) {
        return manifest_style && value.has_value() ? static_cast<double>(*value) : fallback;
    };
    const std::string imported_skin_row_label =
        active_skin_source == "tenriff" ? ui_text("TenRiff Skin", "TenRiff 스킨")
                                         : (active_skin_source == "lr2"
                                                ? ui_text("LR2 Skin", "LR2 스킨")
                                                : ui_text("Imported Skin", "가져온 스킨"));
    std::string imported_skin_value = ui_text("N/A", "없음");
    if (active_skin_source == "lr2") {
        imported_skin_value = available_lr2_skin_names_.empty()
                                  ? ui_text("Not Found", "없음")
                                  : (config_.skin.lr2_skin_name.empty() ? available_lr2_skin_names_.front()
                                                                        : config_.skin.lr2_skin_name);
    } else if (active_skin_source == "tenriff") {
        imported_skin_value = available_tenriff_skin_names_.empty()
                                  ? ui_text("Not Found", "없음")
                                  : (config_.skin.tenriff_skin_name.empty() ? available_tenriff_skin_names_.front()
                                                                            : config_.skin.tenriff_skin_name);
    }
    append_menu_row(render.generic, ui_text("Key Mode", "키 모드"), ui_key_mode_label(skin_edit_mode_),
                    settings_cursor_ == stable_rows.index_of(SkinSettingsRowId::KeyMode),
                    render::MenuHitTargetKind::SettingsRow,
                    stable_rows.index_of(SkinSettingsRowId::KeyMode), false, true);
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
    append_menu_row(render.generic, ui_text("Import Skin", "스킨 가져오기"),
                    ui_text("Open Folder", "폴더 열기"),
                    settings_cursor_ == 2 + lr2_shift,
                    render::MenuHitTargetKind::SettingsRow, 2 + lr2_shift, true, false);
    append_menu_row(render.generic, ui_text("Create New Skin", "새 스킨 만들기"),
                    ui_text("Create & Open", "생성 후 열기"),
                    false, render::MenuHitTargetKind::SettingsRow,
                    3 + lr2_shift, true, false);
    const bool editable_tenriff_skin = active_skin_source == "tenriff" &&
                                       active_tenriff_skin_.found &&
                                       !config_.skin.tenriff_skin_name.empty();
    append_menu_row(render.generic, ui_text("Open Skin Folder", "스킨 폴더 열기"),
                    editable_tenriff_skin ? ui_text("Open", "열기") : ui_text("TenRiff Only", "TenRiff 전용"),
                    false, render::MenuHitTargetKind::SettingsRow,
                    4 + lr2_shift, editable_tenriff_skin, false);
    append_menu_row(render.generic, ui_text("Reload Skin", "스킨 다시 불러오기"),
                    editable_tenriff_skin ? "F5" : ui_text("TenRiff Only", "TenRiff 전용"),
                    false, render::MenuHitTargetKind::SettingsRow,
                    5 + lr2_shift, editable_tenriff_skin, false);
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
                    single_color_enabled
                        ? ui_text("Single Color Active", "단일 색상 사용 중")
                        : config::skin_color_label(preview_lane_palette[static_cast<std::size_t>(skin_edit_lane_)]),
                    settings_cursor_ == 6 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 6 + lr2_shift,
                    false, !single_color_enabled);
    append_menu_row(render.generic,
                    ui_text("Single Color", "단일 색상"),
                    config::skin_single_color_label(config_.skin.single_color),
                    false,
                    render::MenuHitTargetKind::SettingsRow,
                    stable_rows.index_of(SkinSettingsRowId::SingleColor),
                    false,
                    true);
    append_menu_row(render.generic, ui_text("Note Shape", "노트 모양"), ui_skin_note_shape_label(config_.skin.note_shape),
                    settings_cursor_ == 7 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 7 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Note Border", "노트 테두리"), ui_on_off(config_.skin.note_border_enabled),
                    settings_cursor_ == 8 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 8 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Image Aspect", "이미지 비율"),
                    ui_skin_note_image_aspect_label(config_.skin.note_image_aspect),
                    settings_cursor_ == 9 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 9 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("White Dividers", "흰 레인 구분선"), ui_on_off(config_.skin.show_lane_dividers),
                    settings_cursor_ == 10 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 10 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Judgement Line", "판정선 표시"), ui_on_off(config_.skin.show_judgement_line),
                    settings_cursor_ == 11 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 11 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Gear Boundary", "기어 경계선"), ui_on_off(config_.skin.show_gear_boundary_line),
                    settings_cursor_ == 12 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 12 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("LN Tail Cap", "LN \uD14C\uC77C \uCEA1"), ui_on_off(config_.skin.show_hold_tail),
                    settings_cursor_ == 13 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 13 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("LN Tail Taper", "LN 꼬리 테이퍼"), ui_on_off(config_.skin.hold_tail_taper_enabled),
                    settings_cursor_ == 14 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 14 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Visual Preset", "비주얼 프리셋"),
                    config::skin_visual_preset_label(config_.skin.visual_preset),
                    settings_cursor_ == 15 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 15 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Lane Tint Opacity", "레인 색상 농도"),
                    format_percent(config_.skin.lane_background_opacity),
                    settings_cursor_ == 16 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 16 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Visual Opacity", "전체 투명도"),
                    format_percent(config_.skin.visual_opacity),
                    settings_cursor_ == 17 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 17 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Outline Alpha", "외곽선 투명도"),
                    format_percent(config_.skin.note_outline_opacity),
                    settings_cursor_ == 18 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 18 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("LN Body Alpha", "LN 몸통 투명도"),
                    format_percent(config_.skin.hold_body_opacity),
                    settings_cursor_ == 19 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 19 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Judge Glow", "판정선 글로우"), ui_on_off(config_.skin.judgement_line_glow_enabled),
                    settings_cursor_ == 20 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 20 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Hit Burst Style", "키 폭발 모양"),
                    config::skin_hit_burst_style_label(config_.skin.hit_burst_style),
                    settings_cursor_ == 21 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 21 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Hit Burst Brightness", "키 폭발 밝기"),
                    format_percent(config_.skin.key_pulse_brightness),
                    settings_cursor_ == 22 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 22 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Key Labels", "키 이름"),
                    config::skin_key_label_position_label(config_.skin.key_label_position),
                    settings_cursor_ == 23 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 23 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Judge Line", "판정선 위치"), format_percent(config_.skin.judgement_line_position),
                    settings_cursor_ == 24 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 24 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Lane Width", "레인 너비"), format_percent(preview_lane_width_scale),
                    settings_cursor_ == 25 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 25 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Note & Field Size", "노트·필드 크기"), format_percent(preview_note_width_scale),
                    settings_cursor_ == 26 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 26 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Lane Spacing", "레인 간격"), format_percent(preview_lane_spacing_scale),
                    settings_cursor_ == 27 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 27 + lr2_shift, false, gap_count > 0);
    append_menu_row(render.generic, ui_text("Divider Width", "구분선 너비"), format_percent(preview_lane_divider_width_scale),
                    settings_cursor_ == 28 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 28 + lr2_shift, false, true);
    append_menu_row(render.generic,
                    ui_text("16K Center Gap", "16K 중앙 간격"),
                    center_gap_available ? format_percent(preview_lane_center_gap_scale) : ui_text("16K Only", "16K 전용"),
                    settings_cursor_ == 29 + lr2_shift,
                    render::MenuHitTargetKind::SettingsRow,
                    29 + lr2_shift,
                    false,
                    center_gap_available);
    append_menu_row(render.generic, ui_text("LN Body Width", "LN 몸통 너비"), format_percent(config_.skin.hold_body_width_scale),
                    settings_cursor_ == 30 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 30 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Note Height", "노트 높이"), format_percent(preview_note_height_scale),
                    settings_cursor_ == 31 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 31 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Combo Y", "콤보 Y"), format_percent(config_.skin.combo_position),
                    settings_cursor_ == 32 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 32 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Opaque Playfield", "기어 뒤 BGA 가림"),
                    ui_on_off(config_.skin.black_playfield_enabled),
                    settings_cursor_ == 33 + lr2_shift, render::MenuHitTargetKind::SettingsRow, 33 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("UI Font", "UI 폰트"),
                    config::skin_ui_font_label(config_.skin.ui_font),
                    settings_cursor_ == 34 + lr2_shift, render::MenuHitTargetKind::SettingsRow,
                    34 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Visual Latency", "비주얼 레이턴시"),
                    format_signed_offset_ms(config_.visual_offset_ms),
                    settings_cursor_ == 35 + lr2_shift, render::MenuHitTargetKind::SettingsRow,
                    35 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Note Gap", "노트 여백"),
                    format_pixels(config_.skin.note_divider_gap_px),
                    settings_cursor_ == 36 + lr2_shift, render::MenuHitTargetKind::SettingsRow,
                    36 + lr2_shift, false, true);
    append_menu_row(render.generic, ui_text("Gameplay Cursor", "인게임 커서"),
                    ui_on_off(config_.ui.show_cursor_in_gameplay),
                    settings_cursor_ == 37 + lr2_shift, render::MenuHitTargetKind::SettingsRow,
                    37 + lr2_shift, false, true);
    append_menu_row(render.generic,
                    ui_text("FAST/SLOW Indicator", "FAST/SLOW 인디케이터"),
                    ui_on_off(config_.skin.show_timing_feedback),
                    false,
                    render::MenuHitTargetKind::SettingsRow,
                    stable_rows.index_of(SkinSettingsRowId::TimingFeedback),
                    false,
                    true);
    append_menu_row(render.generic, ui_text("Back", "뒤로"), "", false,
                    render::MenuHitTargetKind::SettingsRow,
                    stable_rows.index_of(SkinSettingsRowId::Back), true, false);
    for (std::size_t row = 0; row < render.generic.rows.size(); ++row) {
        render.generic.rows[row].row_index = static_cast<int>(row);
        render.generic.rows[row].selected = settings_cursor_ == static_cast<int>(row);
    }

    render.generic.skin_preview.visible = true;
    render.generic.skin_preview.mode_label = ui_key_mode_label(skin_edit_mode_);
    render.generic.skin_preview.selected_color_label =
        manifest_style && !manifest_style->lane_colors.empty()
            ? ui_text("Skin Manifest", "스킨 매니페스트")
            : config::skin_color_label(preview_lane_colors[static_cast<std::size_t>(skin_edit_lane_)]);
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
    render.generic.skin_preview.show_lane_dividers = style_bool(
        manifest_style ? manifest_style->show_lane_dividers : std::optional<bool>{},
        config_.skin.show_lane_dividers);
    render.generic.skin_preview.note_divider_gap_px = config_.skin.note_divider_gap_px;
    render.generic.skin_preview.show_judgement_line = style_bool(
        manifest_style ? manifest_style->show_judgement_line : std::optional<bool>{},
        config_.skin.show_judgement_line);
    render.generic.skin_preview.show_gear_boundary_line = style_bool(
        manifest_style ? manifest_style->show_gear_boundary_line : std::optional<bool>{},
        config_.skin.show_gear_boundary_line);
    render.generic.skin_preview.show_hold_tail = style_bool(
        manifest_style ? manifest_style->show_hold_tail : std::optional<bool>{},
        config_.skin.show_hold_tail);
    render.generic.skin_preview.hold_tail_taper_enabled = style_bool(
        manifest_style ? manifest_style->hold_tail_taper_enabled : std::optional<bool>{},
        config_.skin.hold_tail_taper_enabled);
    render.generic.skin_preview.judgement_line_glow_enabled = style_bool(
        manifest_style ? manifest_style->judgement_line_glow_enabled : std::optional<bool>{},
        config_.skin.judgement_line_glow_enabled);
    render.generic.skin_preview.key_pulse_enabled = style_bool(
        manifest_style ? manifest_style->key_pulse_enabled : std::optional<bool>{},
        config_.skin.key_pulse_enabled);
    render.generic.skin_preview.key_pulse_brightness = static_cast<float>(style_number(
        manifest_style ? manifest_style->key_pulse_brightness : std::optional<float>{},
        config_.skin.key_pulse_brightness));
    render.generic.skin_preview.hit_burst_style =
        manifest_style && manifest_style->hit_burst_style.has_value()
            ? *manifest_style->hit_burst_style
            : config::normalize_skin_hit_burst_style_token(config_.skin.hit_burst_style);
    render.generic.skin_preview.key_label_position =
        manifest_style && manifest_style->key_label_position.has_value()
            ? *manifest_style->key_label_position
            : config::normalize_skin_key_label_position_token(config_.skin.key_label_position);
    render.generic.skin_preview.note_border_enabled = style_bool(
        manifest_style ? manifest_style->note_border_enabled : std::optional<bool>{},
        config_.skin.note_border_enabled);
    render.generic.skin_preview.note_shape =
        manifest_style && manifest_style->note_shape.has_value()
            ? *manifest_style->note_shape
            : config_.skin.note_shape;
    render.generic.skin_preview.note_image_aspect =
        render_note_image_aspect(config_.skin.note_image_aspect);
    render.generic.skin_preview.skin_source = active_skin_source;
    render.generic.skin_preview.external_skin_root = active_external_skin_root();
    render.generic.skin_preview.external_skin_name = active_external_skin_name();
    render.generic.skin_preview.skin_revision = tenriff_skin_revision_;
    render.generic.skin_preview.resolved_tenriff_skin =
        active_skin_source == "tenriff"
            ? active_tenriff_gameplay_for_keys(lane_count)
            : nullptr;
    render.generic.skin_preview.lr2_resolution_override =
        config::normalize_skin_lr2_resolution_mode_token(config_.skin.lr2_resolution_mode);
    render.generic.skin_preview.lane_background_opacity = std::clamp(
        style_number(manifest_style ? manifest_style->lane_background_opacity : std::optional<float>{},
                     config_.skin.lane_background_opacity),
        config::kSkinLaneBackgroundOpacityMin,
        config::kSkinLaneBackgroundOpacityMax);
    render.generic.skin_preview.black_playfield_enabled = style_bool(
        manifest_style ? manifest_style->black_playfield_enabled : std::optional<bool>{},
        config_.skin.black_playfield_enabled);
    render.generic.skin_preview.visual_opacity = std::clamp(
        style_number(manifest_style ? manifest_style->visual_opacity : std::optional<float>{},
                     config_.skin.visual_opacity),
        config::kSkinVisualOpacityMin,
        config::kSkinVisualOpacityMax);
    render.generic.skin_preview.note_outline_opacity = std::clamp(
        style_number(manifest_style ? manifest_style->note_outline_opacity : std::optional<float>{},
                     config_.skin.note_outline_opacity),
        config::kSkinNoteOutlineOpacityMin,
        config::kSkinNoteOutlineOpacityMax);
    render.generic.skin_preview.hold_body_opacity = std::clamp(
        style_number(manifest_style ? manifest_style->hold_body_opacity : std::optional<float>{},
                     config_.skin.hold_body_opacity),
        config::kSkinHoldBodyOpacityMin,
        config::kSkinHoldBodyOpacityMax);
    render.generic.skin_preview.lane_colors.fill(0);
    for (int lane = 0; lane < lane_count && lane < static_cast<int>(kGameplayHudMaxLanes); ++lane) {
        const std::size_t lane_index = static_cast<std::size_t>(lane);
        render.generic.skin_preview.lane_colors[lane_index] =
            manifest_style && !manifest_style->lane_colors.empty()
                ? manifest_style->lane_colors[std::min(lane_index, manifest_style->lane_colors.size() - 1u)]
                : config::skin_color_rgb(preview_lane_colors[lane_index]);
    }

    for (const auto& status : skin_status_messages_) {
        render.generic.notes.push_back(status);
    }
    render.generic.notes.push_back(ui_text("Source: Native, TenRiff skin.json, or imported LR2.",
                                           "소스: Native, TenRiff skin.json, 가져온 LR2."));
    render.generic.notes.push_back(ui_text("Import or drop a folder. Create New Skin makes an editable template.",
                                           "폴더를 가져오거나 드롭하세요. 새 스킨 만들기는 편집용 틀을 만듭니다."));
    render.generic.notes.push_back(ui_text("Use Open Skin Folder, then press F5 to reload changes.",
                                           "스킨 폴더를 열어 편집한 뒤 F5로 다시 불러오세요."));
    render.generic.notes.push_back(ui_text("AI authoring guide: docs/skin-agent-guide.md",
                                           "AI 제작 안내: docs/skin-agent-guide.md"));
    if (manifest_style) {
        render.generic.notes.push_back(ui_text(
            "This skin's gameplay values override matching fallback rows.",
            "이 스킨의 게임플레이 값은 같은 기본 설정 행보다 우선합니다."));
    }
}

}  // namespace tenriff::app
