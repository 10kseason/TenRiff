#include "app/MenuApp.h"
#include "app/AudioMixPolicy.h"
#include "app/SongPreviewPlayback.h"
#include "timing/HighResClock.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#endif

namespace tenriff::app {
namespace {

bool is_bms_editor_extension(const std::filesystem::path& path) {
    const std::string extension = path.extension().u8string();
    return extension == ".bms" || extension == ".bme" || extension == ".bml" || extension == ".pms";
}

std::string next_editor_save_path(const std::string& source_path) {
    std::filesystem::path source;
    try {
        source = std::filesystem::u8path(source_path);
    } catch (...) {
        source = std::filesystem::path(source_path);
    }
    if (source.empty()) return {};
    const auto directory = source.parent_path();
    const auto stem = source.stem().u8string();
    for (int index = 1; index < 1000; ++index) {
        const std::string suffix = index == 1 ? ".edited" : ".edited." + std::to_string(index);
        const auto candidate = directory / std::filesystem::u8path(stem + suffix + ".bms");
        std::error_code ec;
        if (!std::filesystem::exists(candidate, ec) || ec) {
            return candidate.u8string();
        }
    }
    return {};
}

std::string force_bms_output_path(std::string path) {
    if (path.empty()) return path;
    try {
        auto output = std::filesystem::u8path(path);
        output.replace_extension(std::filesystem::u8path(".bms"));
        return output.u8string();
    } catch (...) {
        const auto slash = path.find_last_of("/\\");
        const auto dot = path.find_last_of('.');
        if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return path + ".bms";
        return path.substr(0, dot) + ".bms";
    }
}

#ifdef _WIN32
std::string browse_bms_save_file(const std::string& title, const std::string& suggested_name) {
    std::string result;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return result;
    IFileSaveDialog* dialog = nullptr;
    const HRESULT create_hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
                                               IID_IFileSaveDialog, reinterpret_cast<void**>(&dialog));
    if (SUCCEEDED(create_hr) && dialog) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT);
        const COMDLG_FILTERSPEC filters[] = {
            {L"BMS charts (*.bms;*.bme;*.bml;*.pms)", L"*.bms;*.bme;*.bml;*.pms"},
            {L"All files", L"*.*"},
        };
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
        dialog->SetFileTypeIndex(1);
        dialog->SetDefaultExtension(L"bms");
        const auto utf8_to_wide = [](const std::string& value) {
            std::wstring result;
            if (value.empty()) return result;
            const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
            if (length <= 0) return result;
            result.resize(static_cast<std::size_t>(length));
            MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), length);
            return result;
        };
        const std::wstring wide_name = utf8_to_wide(suggested_name);
        if (!wide_name.empty()) dialog->SetFileName(wide_name.c_str());
        const std::wstring wide_title = utf8_to_wide(title);
        if (!wide_title.empty()) dialog->SetTitle(wide_title.c_str());
        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                    const int length = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
                    if (length > 0) {
                        result.resize(static_cast<std::size_t>(length));
                        WideCharToMultiByte(CP_UTF8, 0, path, -1, result.data(), length, nullptr, nullptr);
                        if (!result.empty() && result.back() == '\0') result.pop_back();
                    }
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    CoUninitialize();
    return result;
}
#endif

}  // namespace

void MenuApp::open_bms_editor() {
    if (current_screen() != Screen::SongSelect || song_select_view_ != SongSelectView::Songs) {
        return;
    }
    const std::string chart_path = selected_song_absolute_path();
    if (chart_path.empty()) {
        return;
    }
    // Song Select owns the same AudioThread used by editor preview. Release
    // that session before entering the editor so O/Ctrl+O can claim it.
    stop_song_preview_audio();
    try {
        if (!is_bms_editor_extension(std::filesystem::u8path(chart_path))) {
            return;
        }
    } catch (...) {
        return;
    }

    std::string error;
    if (!bms_editor_.load_file(chart_path, &error)) {
        std::cerr << "[MenuApp] BMS editor load failed: " << error << std::endl;
        return;
    }
    bms_editor_view_measures_ = 8;
    bms_editor_snap_index_ = 2;
    bms_editor_x_axis_lock_ = false;
    bms_editor_repeat_key_ = 0;
    bms_editor_repeat_next_ns_ = 0;
    bms_editor_hover_note_id_ = 0;
    bms_editor_selected_note_ids_.clear();
    bms_editor_selected_bgm_ids_.clear();
    bms_editor_auto_align_bgm_ = true;
    bms_editor_tool_ = BmsEditorTool::PlaceSilent;
    bms_editor_drag_note_id_ = 0;
    bms_editor_drag_view_start_ = -1;
    push_screen(Screen::BmsEditor);
    publish_snapshot();
}

void MenuApp::save_bms_editor_as() {
    if (!bms_editor_.loaded()) return;
#ifdef _WIN32
    const std::string output_path = force_bms_output_path(browse_bms_save_file(
        "Save edited BMS chart", bms_editor_.title().empty() ? "edited-chart.bms" : bms_editor_.title() + ".bms"));
#else
    const std::string output_path = next_editor_save_path(bms_editor_.path());
#endif
    if (output_path.empty()) {
        return;
    }
    std::string error;
    if (!bms_editor_.save_as(output_path, &error)) {
        std::cerr << "[MenuApp] BMS editor save failed: " << error << std::endl;
    }
    publish_snapshot();
}

void MenuApp::cleanup_bms_editor_practice_file() {
    if (bms_editor_practice_path_.empty()) return;
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(bms_editor_practice_path_), ec);
    bms_editor_practice_path_.clear();
}

void MenuApp::launch_bms_editor_practice() {
    if (!bms_editor_.loaded()) return;
    std::string serialize_error;
    const std::string chart_text = bms_editor_.serialize_text(&serialize_error);
    const auto start_seconds = bms_editor_.cursor_seconds();
    if (chart_text.empty() || !start_seconds.has_value()) {
        std::cerr << "[MenuApp] BMS editor practice launch failed: "
                  << (serialize_error.empty() ? "Could not resolve the cursor time." : serialize_error)
                  << std::endl;
        return;
    }

    cleanup_bms_editor_practice_file();
    std::filesystem::path source;
    try { source = std::filesystem::u8path(bms_editor_.path()); }
    catch (...) { source = std::filesystem::current_path(); }
    const auto directory = source.parent_path().empty() ? std::filesystem::current_path() : source.parent_path();
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto practice_path = directory /
        std::filesystem::u8path(".tenriff-practice-" + std::to_string(ticks) + ".bms");
    std::ofstream output(practice_path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!output) {
        std::cerr << "[MenuApp] BMS editor practice launch failed: could not write temporary chart."
                  << std::endl;
        return;
    }
    output.write(chart_text.data(), static_cast<std::streamsize>(chart_text.size()));
    if (!output) {
        std::error_code ec;
        std::filesystem::remove(practice_path, ec);
        std::cerr << "[MenuApp] BMS editor practice launch failed: temporary chart write failed."
                  << std::endl;
        return;
    }
    bms_editor_practice_path_ = practice_path.u8string();
    bms_editor_practice_start_seconds_ = *start_seconds;
    std::cerr << "[MenuApp] BMS editor practice launch at " << *start_seconds
              << "s with no-fail enabled." << std::endl;
    launch_gameplay(bms_editor_practice_path_);
    bms_editor_practice_start_seconds_.reset();
}

int MenuApp::bms_editor_snap_division() const noexcept {
    static constexpr int kSnapDivisions[] = {4, 8, 12, 16, 24, 32, 48, 64, 96, 192};
    return kSnapDivisions[std::clamp(bms_editor_snap_index_, 0,
                                     static_cast<int>(std::size(kSnapDivisions)) - 1)];
}

void MenuApp::update_bms_editor_repeat() {
    if (current_screen() != Screen::BmsEditor || bms_editor_repeat_key_ == 0 ||
        bms_editor_repeat_next_ns_ == 0) return;
    const int64_t now = timing::HighResClock::now_ns();
    if (now < bms_editor_repeat_next_ns_) return;
    handle_bms_editor_input(bms_editor_repeat_key_);
    bms_editor_repeat_next_ns_ = now + 40'000'000LL;
}

void MenuApp::handle_bms_editor_input(uint32_t keycode) {
    if (!bms_editor_.loaded()) {
        if (keycode == key_escape_ || keycode == key_backspace_) {
            stop_bms_editor_preview();
            (void)pop_screen();
            publish_snapshot();
        }
        return;
    }
    if (keycode == key_escape_ || keycode == key_backspace_) {
        stop_bms_editor_preview();
        (void)pop_screen();
        publish_snapshot();
        return;
    }
    bool changed = false;
    bool seeked = false;
    const bool control = control_modifier_pressed();
    const auto move_time = [&](int delta) {
        const int grid = BmsEditorDocument::kGridDivision;
        const int current = bms_editor_.cursor_measure() * grid +
            static_cast<int>(std::llround(static_cast<double>(bms_editor_.cursor_slice()) * grid /
                                          std::max(1, bms_editor_.cursor_slice_count())));
        const int max_time = std::max(0, (bms_editor_.measure_count() - 1) * grid + grid - 1);
        const int next = std::clamp(current + delta, 0, max_time);
        bms_editor_.set_cursor_exact(bms_editor_.cursor_lane(), next / grid, next % grid, grid);
    };
    if (control && !bms_editor_selected_bgm_ids_.empty() && keycode == key_up_) {
        const std::vector<std::uint64_t> ids(bms_editor_selected_bgm_ids_.begin(),
                                             bms_editor_selected_bgm_ids_.end());
        changed = bms_editor_.move_bgm_selection(ids, 0, -bms_editor_snap_division());
    } else if (control && !bms_editor_selected_bgm_ids_.empty() && keycode == key_down_) {
        const std::vector<std::uint64_t> ids(bms_editor_selected_bgm_ids_.begin(),
                                             bms_editor_selected_bgm_ids_.end());
        changed = bms_editor_.move_bgm_selection(ids, 0, bms_editor_snap_division());
    } else if (keycode == key_left_) {
        bms_editor_.move_cursor(-1, 0, 0);
        changed = true;
    } else if (keycode == key_right_) {
        bms_editor_.move_cursor(1, 0, 0);
        changed = true;
    } else if (keycode == key_up_ && !bms_editor_x_axis_lock_) {
        move_time(-bms_editor_snap_division());
        changed = true;
    } else if (keycode == key_down_ && !bms_editor_x_axis_lock_) {
        move_time(bms_editor_snap_division());
        changed = true;
    } else if (keycode == key_page_up_ && !bms_editor_x_axis_lock_) {
        changed = bms_editor_.move_cursor_seconds(-5.0);
        seeked = changed;
    } else if (keycode == key_page_down_ && !bms_editor_x_axis_lock_) {
        changed = bms_editor_.move_cursor_seconds(5.0);
        seeked = changed;
    } else if (keycode == key_enter_ || keycode == key_space_) {
        changed = bms_editor_.toggle_cursor_note();
    } else if (keycode == key_delete_) {
        changed = bms_editor_.remove_cursor_note();
    } else if (key_h_ != 0 && keycode == key_h_) {
        const int start_slice = static_cast<int>(std::llround(
            static_cast<double>(bms_editor_.cursor_slice()) * BmsEditorDocument::kGridDivision /
            static_cast<double>(std::max(1, bms_editor_.cursor_slice_count()))));
        const int end_slice = start_slice + 48;
        const int end_measure = bms_editor_.cursor_measure() + end_slice / BmsEditorDocument::kGridDivision;
        changed = bms_editor_.add_cursor_hold(end_measure, end_slice % BmsEditorDocument::kGridDivision);
    } else if (keycode == key_minus_) {
        changed = bms_editor_.cycle_sample(-1);
    } else if (keycode == key_plus_) {
        changed = bms_editor_.cycle_sample(1);
    } else if (control && keycode == key_k_) {
        changed = bms_editor_.replace_silent_notes();
    } else if (keycode == key_k_) {
        changed = bms_editor_.toggle_silent_note_mode();
    } else if (keycode == key_m_) {
        bms_editor_x_axis_lock_ = !bms_editor_x_axis_lock_;
        changed = true;
    } else if (keycode == key_tab_) {
        bms_editor_snap_index_ = (bms_editor_snap_index_ + 1) % 10;
        changed = true;
    } else if (keycode == key_f6_) {
        bms_editor_view_measures_ = std::max(1, bms_editor_view_measures_ / 2);
        changed = true;
    } else if (keycode == key_f7_) {
        bms_editor_view_measures_ = std::min(32, std::max(1, bms_editor_view_measures_ * 2));
        changed = true;
    } else if (control && keycode == key_z_) {
        changed = bms_editor_.undo();
    } else if (control && keycode == key_y_) {
        bms_editor_auto_align_bgm_ = !bms_editor_auto_align_bgm_;
        changed = true;
    } else if (keycode == key_t_) {
        bms_editor_tool_ = bms_editor_tool_ == BmsEditorTool::PlaceSilent
            ? BmsEditorTool::Move
            : bms_editor_tool_ == BmsEditorTool::Move
                ? BmsEditorTool::Remove
                : BmsEditorTool::PlaceSilent;
        changed = true;
    } else if (control && keycode == key_r_) {
        changed = bms_editor_.redo();
    } else if (control && keycode == key_s_) {
        save_bms_editor_as();
        return;
    } else if (keycode == key_p_) {
        launch_bms_editor_practice();
        return;
    } else if (keycode == key_o_) {
        request_bms_editor_preview(control && keycode == key_o_);
        return;
    }
    if (seeked && (bms_editor_preview_active_ || bms_editor_preview_future_.valid())) {
        stop_bms_editor_preview();
        request_bms_editor_preview(false);
    }
    if (changed) publish_snapshot();
}

void MenuApp::request_bms_editor_preview(bool from_start, std::string keysound_token, bool keysound_only) {
    if (!bms_editor_.loaded()) return;
    // Hover auditions must not replace an explicit O/Ctrl+O playback.
    if (keysound_only && !bms_editor_preview_one_shot_ &&
        (bms_editor_preview_future_.valid() || (bms_editor_preview_active_ &&
         bms_editor_preview_finished_ && !bms_editor_preview_finished_->load()))) return;
    stop_song_preview_audio();
    if (bms_editor_preview_active_ || bms_editor_preview_future_.valid()) {
        stop_bms_editor_preview();
    }
    std::string serialize_error;
    const std::string chart_text = bms_editor_.serialize_text(&serialize_error);
    if (chart_text.empty()) {
        std::cerr << "[MenuApp] BMS editor preview serialization failed: " << serialize_error << std::endl;
        return;
    }
    BmsEditorPreviewRequest request;
    request.chart_text = chart_text;
    request.source_chart_path = bms_editor_.path();
    request.keysound_only = keysound_only;
    request.cursor_measure = (from_start || keysound_only) ? 0 : bms_editor_.cursor_measure();
    request.cursor_fraction = (from_start || keysound_only) ? 0.0 :
        static_cast<double>(bms_editor_.cursor_slice()) /
        static_cast<double>(std::max(1, bms_editor_.cursor_slice_count()));
    request.sample_rate = static_cast<int>(std::clamp<std::uint32_t>(config_.audio.sample_rate, 8'000u, 192'000u));
    request.max_duration_seconds = keysound_only ? 1 : 10;
    request.play_to_end = from_start && !keysound_only;
    if (keysound_only) request.keysound_id = keysound_token.empty()
        ? bms_editor_.selected_sample_token() : std::move(keysound_token);
    bms_editor_preview_one_shot_ = keysound_only;
    bms_editor_preview_cancel_ = std::make_shared<std::atomic<bool>>(false);
    const auto cancel = bms_editor_preview_cancel_;
    bms_editor_preview_future_ = std::async(
        std::launch::async, [request, cancel]() { return build_bms_editor_preview(request, cancel); });
    std::cerr << "[MenuApp] BMS editor preview requested mode="
              << (keysound_only ? "keysound" : from_start ? "start-to-end" : "cursor-10s")
              << " sample_rate=" << request.sample_rate << std::endl;
}

void MenuApp::service_bms_editor_preview() {
    if (!bms_editor_preview_future_.valid() && bms_editor_preview_active_ &&
        bms_editor_preview_finished_ &&
        bms_editor_preview_finished_->load(std::memory_order_acquire)) {
        stop_bms_editor_preview();
        return;
    }
    if (!bms_editor_preview_future_.valid() ||
        bms_editor_preview_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    BmsEditorPreviewResult result;
    try { result = bms_editor_preview_future_.get(); }
    catch (const std::exception& ex) {
        std::cerr << "[MenuApp] BMS editor preview failed: " << ex.what() << std::endl;
        bms_editor_preview_cancel_.reset();
        return;
    }
    bms_editor_preview_cancel_.reset();
    if (!result.success()) {
        std::cerr << "[MenuApp] BMS editor preview failed: " << result.error << std::endl;
        return;
    }
    const bool one_shot = bms_editor_preview_one_shot_;
    stop_bms_editor_preview();
    bms_editor_preview_one_shot_ = one_shot;
    if (!result.warning.empty()) std::cerr << "[MenuApp] BMS editor preview: " << result.warning << std::endl;
    audio::AudioConfig preview_config = config_.audio;
    preview_config.sample_rate = static_cast<std::uint32_t>(result.sample_rate);
    preview_config.exclusive_mode = false;
    const auto samples = result.stereo_samples;
    bms_editor_preview_finished_ = std::make_shared<std::atomic<bool>>(false);
    const auto finished = bms_editor_preview_finished_;
    const float gain = static_cast<float>(menu_background_gain(true, config_.audio_ui.master_volume,
        one_shot ? config_.audio_ui.keysound_volume : config_.audio_ui.bgm_volume));
    const auto initialized = audio_thread_.initialize(
        preview_config,
        [samples, finished, gain, frame_cursor = std::size_t{0}](float* output, std::uint32_t frames,
                                                              std::int64_t, std::int64_t) mutable {
            mix_once_song_preview(*samples, frame_cursor, gain, output, frames);
            if (frame_cursor >= samples->size() / 2u) finished->store(true, std::memory_order_release);
        });
    if (initialized != audio::AudioResult::Success ||
        audio_thread_.start() != audio::AudioResult::Success) {
        std::cerr << "[MenuApp] BMS editor preview audio failed: "
                  << audio_thread_.error_message() << std::endl;
        audio_thread_.shutdown();
        return;
    }
    bms_editor_preview_active_ = true;
}

void MenuApp::stop_bms_editor_preview() {
    if (bms_editor_preview_cancel_) bms_editor_preview_cancel_->store(true, std::memory_order_release);
    if (bms_editor_preview_future_.valid()) {
        bms_editor_preview_future_.wait();
        try { static_cast<void>(bms_editor_preview_future_.get()); } catch (...) {}
    }
    bms_editor_preview_cancel_.reset();
    if (bms_editor_preview_active_) {
        audio_thread_.shutdown();
        bms_editor_preview_active_ = false;
    }
    bms_editor_preview_one_shot_ = false;
    bms_editor_preview_finished_.reset();
}

void MenuApp::populate_bms_editor_render_data(render::MenuRenderData& render) {
    render.kind = render::MenuScreenKind::BmsEditor;
    render.bms_editor.path = bms_editor_.path();
    render.bms_editor.title = bms_editor_.title();
    render.bms_editor.artist = bms_editor_.artist();
    render.bms_editor.status = bms_editor_.status();
    render.bms_editor.lane_count = bms_editor_.lane_count();
    render.bms_editor.measure_count = bms_editor_.measure_count();
    render.bms_editor.cursor_lane = bms_editor_.cursor_lane();
    render.bms_editor.cursor_measure = bms_editor_.cursor_measure();
    render.bms_editor.cursor_slice = bms_editor_.cursor_slice();
    render.bms_editor.cursor_slice_count = bms_editor_.cursor_slice_count();
    render.bms_editor.base_bpm = bms_editor_.base_bpm();
    render.bms_editor.grid_division = BmsEditorDocument::kGridDivision;
    render.bms_editor.view_measure_count = bms_editor_view_measures_;
    render.bms_editor.view_start_measure = bms_editor_drag_view_start_;
    render.bms_editor.snap_division = bms_editor_snap_division();
    render.bms_editor.x_axis_lock = bms_editor_x_axis_lock_;
    render.bms_editor.auto_align_bgm = bms_editor_auto_align_bgm_;
    render.bms_editor.tool = bms_editor_tool_ == BmsEditorTool::PlaceSilent
        ? "place_silent"
        : bms_editor_tool_ == BmsEditorTool::Move ? "move" : "remove";
    render.bms_editor.silent_note_mode = bms_editor_.silent_note_mode();
    const std::string editor_skin_mode =
        config_.mode.key_mode.empty() || config_.mode.key_mode == "auto"
            ? std::to_string(bms_editor_.lane_count()) + "k"
            : config_.mode.key_mode;
    const auto editor_lane_widths = config::resolved_skin_lane_width_scales(config_.skin, editor_skin_mode);
    render.bms_editor.lane_width_scale_count = std::min(
        editor_lane_widths.size(), render.bms_editor.lane_width_scales.size());
    render.bms_editor.lane_width_scales.fill(config::kLaneWidthScaleDefault);
    for (std::size_t lane = 0; lane < render.bms_editor.lane_width_scale_count; ++lane) {
        render.bms_editor.lane_width_scales[lane] = editor_lane_widths[lane];
    }
    render.bms_editor.note_width_scale = config::resolved_skin_note_width_scale(
        config_.skin, editor_skin_mode);
    render.bms_editor.note_height_scale = config::resolved_skin_note_height_scale(
        config_.skin, editor_skin_mode);
    render.bms_editor.dirty = bms_editor_.dirty();
    render.bms_editor.notes.reserve(bms_editor_.notes().size());
    for (const auto& note : bms_editor_.notes()) {
        render.bms_editor.notes.push_back(render::BmsEditorNoteData{
            note.id, note.lane, note.measure, note.slice, note.slice_count,
            note.long_note, note.editable, note.end_measure, note.end_slice,
            note.end_slice_count, note.token,
            bms_editor_selected_note_ids_.count(note.id) != 0});
    }
    render.bms_editor.bgm.reserve(bms_editor_.bgm().size());
    for (const auto& bgm : bms_editor_.bgm()) {
        render.bms_editor.bgm.push_back(render::BmsEditorBgmData{
            bgm.id, bgm.measure, bgm.slice, bgm.slice_count, bgm.token,
            bms_editor_selected_bgm_ids_.count(bgm.id) != 0});
    }
    render.bms_editor.markers.reserve(bms_editor_.markers().size());
    for (const auto& marker : bms_editor_.markers()) {
        render.bms_editor.markers.push_back(render::BmsEditorMarkerData{
            marker.id, marker.measure, marker.slice, marker.slice_count,
            marker.kind == BmsEditorMarker::Kind::Bpm ? "BPM" : "STOP",
            marker.value, marker.label});
    }
    for (const auto& sample : bms_editor_.samples()) {
        render.bms_editor.samples.push_back(sample.token);
    }
    render.bms_editor.selected_sample_token = bms_editor_.selected_sample_token();
}

}  // namespace tenriff::app
