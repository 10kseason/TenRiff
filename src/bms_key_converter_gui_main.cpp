#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

#include "app/BmsKeyConverter.h"
#include "util/Utf8Compat.h"

namespace {

constexpr wchar_t kWindowClassName[] = L"TenRiffBmsKeyConverterWindow";
constexpr wchar_t kWindowTitle[] = L"TenRiff BMS Key Converter";
constexpr wchar_t kFileFilter[] =
    L"BMS Files (*.bms;*.bme;*.bml;*.pms)\0*.bms;*.bme;*.bml;*.pms\0All Files (*.*)\0*.*\0";
constexpr int kClientWidth = 860;
constexpr int kClientHeight = 552;

enum ControlId : int {
    kIdInputEdit = 1001,
    kIdInputBrowse = 1002,
    kIdOutputEdit = 1003,
    kIdOutputBrowse = 1004,
    kIdPresetCombo = 1005,
    kIdTargetCombo = 1006,
    kIdMaxKeysEdit = 1007,
    kIdMinKeysEdit = 1008,
    kIdSpeedSlotEdit = 1009,
    kIdSeedEdit = 1010,
    kIdSampleRateEdit = 1011,
    kIdConvertButton = 1012,
    kIdAlgorithmCombo = 1013,
};

struct AppState {
    HFONT font = nullptr;
    HWND input_label = nullptr;
    HWND input_edit = nullptr;
    HWND input_browse_button = nullptr;
    HWND output_label = nullptr;
    HWND output_edit = nullptr;
    HWND output_browse_button = nullptr;
    HWND preset_label = nullptr;
    HWND preset_combo = nullptr;
    HWND algorithm_label = nullptr;
    HWND algorithm_combo = nullptr;
    HWND target_label = nullptr;
    HWND target_combo = nullptr;
    HWND max_keys_label = nullptr;
    HWND max_keys_edit = nullptr;
    HWND min_keys_label = nullptr;
    HWND min_keys_edit = nullptr;
    HWND speed_slot_label = nullptr;
    HWND speed_slot_edit = nullptr;
    HWND seed_label = nullptr;
    HWND seed_edit = nullptr;
    HWND sample_rate_label = nullptr;
    HWND sample_rate_edit = nullptr;
    HWND hint_label = nullptr;
    HWND convert_button = nullptr;
    HWND log_label = nullptr;
    HWND log_edit = nullptr;
    std::wstring last_suggested_output;
    std::wstring log_buffer;
    bool applying_preset = false;
};

void maybe_apply_output_suggestion(AppState& state);

std::wstring get_window_text_copy(HWND window) {
    const int length = GetWindowTextLengthW(window);
    std::wstring value(static_cast<std::size_t>(length) + 1u, L'\0');
    if (length > 0) {
        GetWindowTextW(window, value.data(), length + 1);
    }
    value.resize(static_cast<std::size_t>(length));
    return value;
}

void set_window_text_copy(HWND window, std::wstring_view value) {
    SetWindowTextW(window, std::wstring(value).c_str());
}

void set_control_font(HWND window, HFONT font) {
    SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void append_log(AppState& state, std::wstring_view line) {
    if (!state.log_buffer.empty()) {
        state.log_buffer.append(L"\r\n");
    }
    state.log_buffer.append(line);

    constexpr std::size_t kMaxLogChars = 24 * 1024;
    if (state.log_buffer.size() > kMaxLogChars) {
        state.log_buffer.erase(0, state.log_buffer.size() - kMaxLogChars);
        const std::size_t first_newline = state.log_buffer.find(L"\r\n");
        if (first_newline != std::wstring::npos) {
            state.log_buffer.erase(0, first_newline + 2);
        }
    }

    SetWindowTextW(state.log_edit, state.log_buffer.c_str());
    const auto end = static_cast<WPARAM>(state.log_buffer.size());
    SendMessageW(state.log_edit, EM_SETSEL, end, end);
    SendMessageW(state.log_edit, EM_SCROLLCARET, 0, 0);
}

bool is_blank(std::wstring_view value) {
    return std::all_of(value.begin(), value.end(), [](wchar_t ch) {
        return std::iswspace(static_cast<wint_t>(ch)) != 0;
    });
}

bool parse_int_value(std::wstring_view text, int& value) {
    if (text.empty()) {
        return false;
    }
    const std::wstring temp(text);
    wchar_t* end = nullptr;
    const long parsed = std::wcstol(temp.c_str(), &end, 10);
    if (end == temp.c_str()) {
        return false;
    }
    while (end && *end != L'\0' && std::iswspace(static_cast<wint_t>(*end)) != 0) {
        ++end;
    }
    if (!end || *end != L'\0') {
        return false;
    }
    if (parsed < static_cast<long>((std::numeric_limits<int>::min)()) ||
        parsed > static_cast<long>((std::numeric_limits<int>::max)())) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool parse_u32_value(std::wstring_view text, uint32_t& value) {
    if (text.empty()) {
        return false;
    }
    const std::wstring temp(text);
    wchar_t* end = nullptr;
    const unsigned long parsed = std::wcstoul(temp.c_str(), &end, 10);
    if (end == temp.c_str()) {
        return false;
    }
    while (end && *end != L'\0' && std::iswspace(static_cast<wint_t>(*end)) != 0) {
        ++end;
    }
    if (!end || *end != L'\0') {
        return false;
    }
    if (parsed > (std::numeric_limits<uint32_t>::max)()) {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool parse_optional_int(HWND edit, int default_value, int& value) {
    const std::wstring text = get_window_text_copy(edit);
    if (is_blank(text)) {
        value = default_value;
        return true;
    }
    return parse_int_value(text, value);
}

bool parse_optional_u32(HWND edit, uint32_t default_value, uint32_t& value) {
    const std::wstring text = get_window_text_copy(edit);
    if (is_blank(text)) {
        value = default_value;
        return true;
    }
    return parse_u32_value(text, value);
}

bool parse_sample_rate_edit(HWND edit, int& value) {
    std::wstring text = get_window_text_copy(edit);
    if (is_blank(text)) {
        value = 0;
        return true;
    }
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t ch) {
        if (ch >= L'A' && ch <= L'Z') {
            return static_cast<wchar_t>(ch - (L'A' - L'a'));
        }
        return ch;
    });
    if (text == L"auto" || text == L"detect" || text == L"0") {
        value = 0;
        return true;
    }
    return parse_int_value(text, value) && value > 0;
}

std::wstring lowercase_ascii(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        if (ch >= L'A' && ch <= L'Z') {
            return static_cast<wchar_t>(ch - (L'A' - L'a'));
        }
        return ch;
    });
    return value;
}

bool is_bms_family_file(const std::filesystem::path& path) {
    const std::wstring extension = lowercase_ascii(path.extension().wstring());
    return extension == L".bms" || extension == L".bme" || extension == L".bml" || extension == L".pms";
}

int selected_target_keys(const AppState& state) {
    const int selection = static_cast<int>(SendMessageW(state.target_combo, CB_GETCURSEL, 0, 0));
    if (selection == CB_ERR) {
        return 0;
    }
    const LRESULT item_data = SendMessageW(state.target_combo, CB_GETITEMDATA, selection, 0);
    if (item_data == CB_ERR) {
        return 0;
    }
    return static_cast<int>(item_data);
}

std::string selected_algorithm(const AppState& state) {
    const int selection = static_cast<int>(SendMessageW(state.algorithm_combo, CB_GETCURSEL, 0, 0));
    if (selection == CB_ERR) {
        return "krrcream";
    }
    const LRESULT item_data = SendMessageW(state.algorithm_combo, CB_GETITEMDATA, selection, 0);
    if (item_data == 2) {
        return "nk3";
    }
    return item_data == 1 ? "nk2" : "krrcream";
}

void update_algorithm_controls(AppState& state) {
    const std::string algorithm = selected_algorithm(state);
    // Shipped Krrcream tuning is intentionally read-only in the GUI. nK2 ignores
    // these fields, so neither algorithm exposes misleading mutable controls.
    EnableWindow(state.max_keys_edit, FALSE);
    EnableWindow(state.min_keys_edit, FALSE);
    EnableWindow(state.speed_slot_edit, FALSE);
    EnableWindow(state.seed_edit, FALSE);
    set_window_text_copy(state.hint_label,
                         algorithm == "nk3"
                             ? L"NK3: P64 ONNX + host beam; strict GPU by default, CPU by environment."
                             : algorithm == "nk2"
                                   ? L"nK2: native profile; Krrcream tuning locked."
                                   : L"Krrcream: shipped preset tuning locked.");
}

bool select_target_keys(AppState& state, int target_keys) {
    const int count = static_cast<int>(SendMessageW(state.target_combo, CB_GETCOUNT, 0, 0));
    for (int index = 0; index < count; ++index) {
        const LRESULT item_data = SendMessageW(state.target_combo, CB_GETITEMDATA, static_cast<WPARAM>(index), 0);
        if (item_data == static_cast<LRESULT>(target_keys)) {
            SendMessageW(state.target_combo, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
            return true;
        }
    }
    return false;
}

void apply_selected_preset(AppState& state) {
    const int selection = static_cast<int>(SendMessageW(state.preset_combo, CB_GETCURSEL, 0, 0));
    if (selection == CB_ERR) {
        return;
    }

    const LRESULT preset_index = SendMessageW(state.preset_combo, CB_GETITEMDATA, static_cast<WPARAM>(selection), 0);
    if (preset_index < 0) {
        return;
    }

    const auto& presets = tenriff::app::bms_key_converter_presets();
    if (static_cast<std::size_t>(preset_index) >= presets.size()) {
        return;
    }

    const auto& preset = presets[static_cast<std::size_t>(preset_index)];
    state.applying_preset = true;
    select_target_keys(state, preset.target_lane_count);
    set_window_text_copy(state.max_keys_edit, std::to_wstring(preset.max_keys));
    set_window_text_copy(state.min_keys_edit, std::to_wstring(preset.min_keys));
    set_window_text_copy(state.speed_slot_edit, std::to_wstring(preset.transform_speed_slot));
    if (preset.fixed_seed.has_value()) {
        set_window_text_copy(state.seed_edit, std::to_wstring(preset.fixed_seed.value()));
    }
    state.applying_preset = false;
    maybe_apply_output_suggestion(state);
}

std::wstring suggest_output_path(std::wstring_view input_path, int target_keys) {
    if (input_path.empty() || target_keys <= 0) {
        return {};
    }

    const std::filesystem::path source_path(input_path);
    if (source_path.empty()) {
        return {};
    }

    const std::filesystem::path parent = source_path.parent_path();
    const std::wstring stem = source_path.stem().wstring();
    const std::wstring extension = source_path.has_extension() ? source_path.extension().wstring() : L".bms";
    if (stem.empty()) {
        return {};
    }

    const std::wstring output_name = stem + L"_" + std::to_wstring(target_keys) + L"k" + extension;
    const std::filesystem::path suggested = parent / output_name;
    const std::filesystem::path normalized = suggested.lexically_normal();
    return normalized.wstring();
}

void maybe_apply_output_suggestion(AppState& state) {
    const std::wstring current_output = get_window_text_copy(state.output_edit);
    if (!current_output.empty() && current_output != state.last_suggested_output) {
        return;
    }

    const std::wstring suggestion = suggest_output_path(get_window_text_copy(state.input_edit),
                                                        selected_target_keys(state));
    if (suggestion.empty()) {
        if (current_output == state.last_suggested_output) {
            set_window_text_copy(state.output_edit, L"");
        }
        state.last_suggested_output.clear();
        return;
    }

    state.last_suggested_output = suggestion;
    set_window_text_copy(state.output_edit, suggestion);
}

bool browse_for_open_file(HWND owner, std::wstring& path) {
    std::array<wchar_t, 32768> buffer{};
    if (!path.empty()) {
        wcsncpy_s(buffer.data(), buffer.size(), path.c_str(), _TRUNCATE);
    }

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = kFileFilter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    dialog.lpstrDefExt = L"bms";

    if (!GetOpenFileNameW(&dialog)) {
        return false;
    }

    path.assign(buffer.data());
    return true;
}

bool browse_for_save_file(HWND owner, std::wstring& path) {
    std::array<wchar_t, 32768> buffer{};
    if (!path.empty()) {
        wcsncpy_s(buffer.data(), buffer.size(), path.c_str(), _TRUNCATE);
    }

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = kFileFilter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    dialog.lpstrDefExt = L"bms";

    if (!GetSaveFileNameW(&dialog)) {
        return false;
    }

    path.assign(buffer.data());
    return true;
}

bool collect_options(const AppState& state, tenriff::app::BmsKeyConverterOptions& options, std::wstring& error) {
    options = tenriff::app::BmsKeyConverterOptions{};

    const std::wstring input_path = get_window_text_copy(state.input_edit);
    const std::wstring output_path = get_window_text_copy(state.output_edit);
    if (is_blank(input_path)) {
        error = L"Input BMS file is required.";
        return false;
    }
    if (is_blank(output_path)) {
        error = L"Output BMS file is required.";
        return false;
    }

    options.input_path = tenriff::util::utf8_from_wide_lossy(input_path);
    options.output_path = tenriff::util::utf8_from_wide_lossy(output_path);
    options.target_lane_count = selected_target_keys(state);
    options.conversion_algorithm = selected_algorithm(state);
    if (options.input_path.empty()) {
        error = L"Failed to encode the input path as UTF-8.";
        return false;
    }
    if (options.output_path.empty()) {
        error = L"Failed to encode the output path as UTF-8.";
        return false;
    }
    if (options.target_lane_count <= 0) {
        error = L"Select a target key count.";
        return false;
    }
    if (!parse_optional_int(state.max_keys_edit, 0, options.max_keys)) {
        error = L"Max Keys must be a valid integer.";
        return false;
    }
    if (!parse_optional_int(state.min_keys_edit, 0, options.min_keys)) {
        error = L"Min Keys must be a valid integer.";
        return false;
    }
    if (!parse_optional_int(state.speed_slot_edit, 4, options.transform_speed_slot)) {
        error = L"Speed Slot must be a valid integer.";
        return false;
    }
    if (!parse_optional_u32(state.seed_edit, 0, options.seed)) {
        error = L"Seed must be a valid unsigned integer.";
        return false;
    }
    if (!parse_sample_rate_edit(state.sample_rate_edit, options.sample_rate)) {
        error = L"Sample Rate must be Auto, 0, or a positive integer.";
        return false;
    }

    return true;
}

void layout_controls(HWND window, AppState& state) {
    RECT client{};
    GetClientRect(window, &client);
    const int client_width = client.right - client.left;
    const int client_height = client.bottom - client.top;

    const int margin = 12;
    const int row_height = 24;
    const int row_gap = 8;
    const int label_width = 82;
    const int button_width = 92;
    const int edit_width = client_width - (margin * 2) - label_width - button_width - (row_gap * 2);

    int y = margin;
    MoveWindow(state.input_label, margin, y + 4, label_width, row_height, TRUE);
    MoveWindow(state.input_edit, margin + label_width + row_gap, y, edit_width, row_height, TRUE);
    MoveWindow(state.input_browse_button, client_width - margin - button_width, y, button_width, row_height, TRUE);

    y += row_height + row_gap;
    MoveWindow(state.output_label, margin, y + 4, label_width, row_height, TRUE);
    MoveWindow(state.output_edit, margin + label_width + row_gap, y, edit_width, row_height, TRUE);
    MoveWindow(state.output_browse_button, client_width - margin - button_width, y, button_width, row_height, TRUE);

    y += row_height + row_gap + 4;
    MoveWindow(state.preset_label, margin, y + 4, 76, row_height, TRUE);
    MoveWindow(state.preset_combo, margin + 78, y, 200, 240, TRUE);
    MoveWindow(state.algorithm_label, margin + 296, y + 4, 76, row_height, TRUE);
    MoveWindow(state.algorithm_combo, margin + 374, y, 190, 160, TRUE);

    y += row_height + row_gap;
    MoveWindow(state.target_label, margin, y + 4, 76, row_height, TRUE);
    MoveWindow(state.target_combo, margin + 78, y, 80, 200, TRUE);
    MoveWindow(state.max_keys_label, margin + 170, y + 4, 72, row_height, TRUE);
    MoveWindow(state.max_keys_edit, margin + 244, y, 58, row_height, TRUE);
    MoveWindow(state.min_keys_label, margin + 316, y + 4, 68, row_height, TRUE);
    MoveWindow(state.min_keys_edit, margin + 388, y, 58, row_height, TRUE);
    MoveWindow(state.speed_slot_label, margin + 460, y + 4, 78, row_height, TRUE);
    MoveWindow(state.speed_slot_edit, margin + 542, y, 58, row_height, TRUE);

    y += row_height + row_gap;
    MoveWindow(state.seed_label, margin, y + 4, 76, row_height, TRUE);
    MoveWindow(state.seed_edit, margin + 78, y, 120, row_height, TRUE);
    MoveWindow(state.sample_rate_label, margin + 214, y + 4, 92, row_height, TRUE);
    MoveWindow(state.sample_rate_edit, margin + 308, y, 90, row_height, TRUE);
    MoveWindow(state.hint_label, margin + 410, y + 4, client_width - (margin + 410) - 150 - row_gap, row_height, TRUE);
    MoveWindow(state.convert_button, client_width - margin - 150, y - 1, 150, row_height + 2, TRUE);

    y += row_height + row_gap + 4;
    MoveWindow(state.log_label, margin, y + 2, 64, row_height, TRUE);
    y += row_height;
    MoveWindow(state.log_edit, margin, y, client_width - (margin * 2), client_height - y - margin, TRUE);
}

void initialize_target_combo(AppState& state) {
    constexpr int kTargets[] = {4, 5, 6, 8, 9, 10, 16};
    for (int target : kTargets) {
        const std::wstring label = std::to_wstring(target) + L"K";
        const LRESULT index = SendMessageW(state.target_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (index != CB_ERR && index != CB_ERRSPACE) {
            SendMessageW(state.target_combo, CB_SETITEMDATA, static_cast<WPARAM>(index), static_cast<LPARAM>(target));
        }
    }
    SendMessageW(state.target_combo, CB_SETCURSEL, 0, 0);
}

void initialize_algorithm_combo(AppState& state) {
    const LRESULT krrcream_index =
        SendMessageW(state.algorithm_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Krrcream"));
    if (krrcream_index != CB_ERR && krrcream_index != CB_ERRSPACE) {
        SendMessageW(state.algorithm_combo,
                     CB_SETITEMDATA,
                     static_cast<WPARAM>(krrcream_index),
                     static_cast<LPARAM>(0));
    }

    const LRESULT nk2_index =
        SendMessageW(state.algorithm_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"nK2 (Native 50/50)"));
    if (nk2_index != CB_ERR && nk2_index != CB_ERRSPACE) {
        SendMessageW(state.algorithm_combo,
                     CB_SETITEMDATA,
                     static_cast<WPARAM>(nk2_index),
                     static_cast<LPARAM>(1));
    }

    const LRESULT nk3_index =
        SendMessageW(state.algorithm_combo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"NK3 (P64 ONNX, NPU)"));
    if (nk3_index != CB_ERR && nk3_index != CB_ERRSPACE) {
        SendMessageW(state.algorithm_combo,
                     CB_SETITEMDATA,
                     static_cast<WPARAM>(nk3_index),
                     static_cast<LPARAM>(2));
    }

    SendMessageW(state.algorithm_combo, CB_SETCURSEL, 0, 0);
    update_algorithm_controls(state);
}

void initialize_preset_combo(AppState& state) {
    const LRESULT manual_index =
        SendMessageW(state.preset_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Manual"));
    if (manual_index != CB_ERR && manual_index != CB_ERRSPACE) {
        SendMessageW(state.preset_combo, CB_SETITEMDATA, static_cast<WPARAM>(manual_index), static_cast<LPARAM>(-1));
    }

    const auto& presets = tenriff::app::bms_key_converter_presets();
    for (std::size_t index = 0; index < presets.size(); ++index) {
        const auto& preset = presets[index];
        if (!preset.supported_output) {
            continue;
        }

        const std::wstring label = tenriff::util::wide_from_utf8_lossy(preset.display_name);
        const LRESULT combo_index =
            SendMessageW(state.preset_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (combo_index != CB_ERR && combo_index != CB_ERRSPACE) {
            SendMessageW(state.preset_combo,
                         CB_SETITEMDATA,
                         static_cast<WPARAM>(combo_index),
                         static_cast<LPARAM>(index));
        }
    }

    SendMessageW(state.preset_combo, CB_SETCURSEL, 0, 0);
}

void create_controls(HWND window, AppState& state) {
    state.font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    auto create = [&](DWORD ex_style,
                      const wchar_t* class_name,
                      const wchar_t* text,
                      DWORD style,
                      int id) -> HWND {
        HWND control = CreateWindowExW(ex_style,
                                       class_name,
                                       text,
                                       style,
                                       0,
                                       0,
                                       0,
                                       0,
                                       window,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                       GetModuleHandleW(nullptr),
                                       nullptr);
        if (control) {
            set_control_font(control, state.font);
        }
        return control;
    };

    state.input_label = create(0, L"STATIC", L"Input", WS_CHILD | WS_VISIBLE, 0);
    state.input_edit = create(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, kIdInputEdit);
    state.input_browse_button =
        create(0, L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, kIdInputBrowse);

    state.output_label = create(0, L"STATIC", L"Output", WS_CHILD | WS_VISIBLE, 0);
    state.output_edit =
        create(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, kIdOutputEdit);
    state.output_browse_button =
        create(0, L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, kIdOutputBrowse);

    state.preset_label = create(0, L"STATIC", L"Preset", WS_CHILD | WS_VISIBLE, 0);
    state.preset_combo = create(0,
                                L"COMBOBOX",
                                L"",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                kIdPresetCombo);
    state.algorithm_label = create(0, L"STATIC", L"Algorithm", WS_CHILD | WS_VISIBLE, 0);
    state.algorithm_combo = create(0,
                                   L"COMBOBOX",
                                   L"",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   kIdAlgorithmCombo);
    state.target_label = create(0, L"STATIC", L"Target", WS_CHILD | WS_VISIBLE, 0);
    state.target_combo = create(0,
                                L"COMBOBOX",
                                L"",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                kIdTargetCombo);
    state.max_keys_label = create(0, L"STATIC", L"Max Keys", WS_CHILD | WS_VISIBLE, 0);
    state.max_keys_edit =
        create(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, kIdMaxKeysEdit);
    state.min_keys_label = create(0, L"STATIC", L"Min Keys", WS_CHILD | WS_VISIBLE, 0);
    state.min_keys_edit =
        create(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, kIdMinKeysEdit);
    state.speed_slot_label = create(0, L"STATIC", L"Speed Slot", WS_CHILD | WS_VISIBLE, 0);
    state.speed_slot_edit = create(WS_EX_CLIENTEDGE,
                                   L"EDIT",
                                   L"4",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                   kIdSpeedSlotEdit);

    state.seed_label = create(0, L"STATIC", L"Seed", WS_CHILD | WS_VISIBLE, 0);
    state.seed_edit =
        create(WS_EX_CLIENTEDGE, L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, kIdSeedEdit);
    state.sample_rate_label = create(0, L"STATIC", L"Sample Rate", WS_CHILD | WS_VISIBLE, 0);
    state.sample_rate_edit = create(WS_EX_CLIENTEDGE,
                                    L"EDIT",
                                    L"Auto",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                    kIdSampleRateEdit);
    state.hint_label =
        create(0, L"STATIC", L"Krrcream: preset tuning active.", WS_CHILD | WS_VISIBLE, 0);
    state.convert_button = create(0,
                                  L"BUTTON",
                                  L"Convert",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                  kIdConvertButton);

    state.log_label = create(0, L"STATIC", L"Log", WS_CHILD | WS_VISIBLE, 0);
    state.log_edit = create(WS_EX_CLIENTEDGE,
                            L"EDIT",
                            L"",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY |
                                WS_VSCROLL,
                            0);

    initialize_preset_combo(state);
    initialize_algorithm_combo(state);
    initialize_target_combo(state);
    SendMessageW(state.log_edit, EM_LIMITTEXT, static_cast<WPARAM>(0x7ffffffe), 0);
    DragAcceptFiles(window, TRUE);
    layout_controls(window, state);

    append_log(state, L"Select a BMS-family chart or drag one into this window.");
    append_log(state, L"Supported targets: 4K, 5K, 6K, 8K, 9K, 10K, 16K.");
    append_log(state, L"Algorithm: Krrcream or nK2 native; Krrcream tuning is locked.");
    append_log(state, L"nK2 ignores Krrcream Max/Min/Speed/Seed tuning fields.");
}

void log_conversion_result(AppState& state,
                           const tenriff::app::BmsKeyConverterOptions& options,
                           const tenriff::app::BmsKeyConverterResult& result,
                           ULONGLONG elapsed_ms) {
    for (const auto& warning : result.warnings) {
        append_log(state, L"[warning] " + tenriff::util::wide_from_utf8_lossy(warning));
    }

    if (!result.success) {
        append_log(state, L"[error] " + tenriff::util::wide_from_utf8_lossy(result.error));
        return;
    }

    std::wstringstream summary;
    summary << L"[ok] Converted " << result.source_lane_count << L"K -> " << result.target_lane_count
            << L"K, notes=" << result.note_count << L", holds=" << result.hold_count
            << L", sample_rate=" << result.sample_rate << (result.sample_rate_auto ? L" auto" : L"")
            << L", elapsed=" << elapsed_ms
            << L" ms";
    append_log(state, summary.str());
    append_log(state, L"Output: " + tenriff::util::wide_from_utf8_lossy(options.output_path));
}

void run_conversion(HWND window, AppState& state) {
    tenriff::app::BmsKeyConverterOptions options;
    std::wstring error;
    if (!collect_options(state, options, error)) {
        append_log(state, L"[error] " + error);
        MessageBeep(MB_ICONERROR);
        return;
    }

    append_log(state, L"Converting: " + tenriff::util::wide_from_utf8_lossy(options.input_path));
    EnableWindow(state.convert_button, FALSE);
    UpdateWindow(window);

    const ULONGLONG start = GetTickCount64();
    tenriff::app::BmsKeyConverterResult result;
    try {
        result = tenriff::app::convert_bms_chart_file(options);
    } catch (const std::exception& exception) {
        result.success = false;
        result.error = std::string("Unexpected exception: ") + exception.what();
    } catch (...) {
        result.success = false;
        result.error = "Unexpected unknown exception.";
    }
    const ULONGLONG elapsed_ms = GetTickCount64() - start;

    EnableWindow(state.convert_button, TRUE);
    log_conversion_result(state, options, result, elapsed_ms);
    if (!result.success) {
        MessageBeep(MB_ICONERROR);
    }
}

void handle_input_drop(AppState& state, HDROP drop_handle) {
    std::array<wchar_t, 32768> buffer{};
    if (DragQueryFileW(drop_handle, 0, buffer.data(), static_cast<UINT>(buffer.size())) == 0) {
        append_log(state, L"[error] Failed to read the dropped file path.");
        DragFinish(drop_handle);
        return;
    }

    const std::filesystem::path dropped_path(buffer.data());
    if (!is_bms_family_file(dropped_path)) {
        append_log(state, L"[error] Drop a .bms, .bme, .bml, or .pms file.");
        DragFinish(drop_handle);
        return;
    }

    set_window_text_copy(state.input_edit, dropped_path.wstring());
    maybe_apply_output_suggestion(state);
    append_log(state, L"Loaded input from drop: " + dropped_path.wstring());
    DragFinish(drop_handle);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_NCCREATE: {
        auto* created_state = new AppState();
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created_state));
        return TRUE;
    }
    case WM_CREATE:
        if (state) {
            create_controls(window, *state);
        }
        return 0;
    case WM_SIZE:
        if (state) {
            layout_controls(window, *state);
        }
        return 0;
    case WM_COMMAND:
        if (!state) {
            return 0;
        }
        switch (LOWORD(w_param)) {
        case kIdInputBrowse:
            if (HIWORD(w_param) == BN_CLICKED) {
                std::wstring path = get_window_text_copy(state->input_edit);
                if (browse_for_open_file(window, path)) {
                    set_window_text_copy(state->input_edit, path);
                    maybe_apply_output_suggestion(*state);
                }
            }
            return 0;
        case kIdOutputBrowse:
            if (HIWORD(w_param) == BN_CLICKED) {
                std::wstring path = get_window_text_copy(state->output_edit);
                if (path.empty()) {
                    path = suggest_output_path(get_window_text_copy(state->input_edit), selected_target_keys(*state));
                }
                if (browse_for_save_file(window, path)) {
                    set_window_text_copy(state->output_edit, path);
                }
            }
            return 0;
        case kIdPresetCombo:
            if (HIWORD(w_param) == CBN_SELCHANGE) {
                apply_selected_preset(*state);
            }
            return 0;
        case kIdAlgorithmCombo:
            if (HIWORD(w_param) == CBN_SELCHANGE) {
                update_algorithm_controls(*state);
            }
            return 0;
        case kIdTargetCombo:
            if (HIWORD(w_param) == CBN_SELCHANGE) {
                if (!state->applying_preset) {
                    SendMessageW(state->preset_combo, CB_SETCURSEL, 0, 0);
                }
                maybe_apply_output_suggestion(*state);
            }
            return 0;
        case kIdMaxKeysEdit:
        case kIdMinKeysEdit:
        case kIdSpeedSlotEdit:
        case kIdSeedEdit:
            if (HIWORD(w_param) == EN_CHANGE && !state->applying_preset) {
                SendMessageW(state->preset_combo, CB_SETCURSEL, 0, 0);
            }
            return 0;
        case kIdConvertButton:
            if (HIWORD(w_param) == BN_CLICKED) {
                run_conversion(window, *state);
            }
            return 0;
        default:
            return 0;
        }
    case WM_DROPFILES:
        if (state) {
            handle_input_drop(*state, reinterpret_cast<HDROP>(w_param));
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_NCDESTROY:
        delete state;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        return DefWindowProcW(window, message, w_param, l_param);
    default:
        return DefWindowProcW(window, message, w_param, l_param);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&window_class) == 0) {
        MessageBoxW(nullptr, L"Failed to register the converter window class.", kWindowTitle, MB_ICONERROR | MB_OK);
        return 1;
    }

    RECT desired_client{0, 0, kClientWidth, kClientHeight};
    AdjustWindowRectEx(&desired_client,
                       WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                       FALSE,
                       0);

    const HWND window = CreateWindowExW(0,
                                        kWindowClassName,
                                        kWindowTitle,
                                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                        CW_USEDEFAULT,
                                        CW_USEDEFAULT,
                                        desired_client.right - desired_client.left,
                                        desired_client.bottom - desired_client.top,
                                        nullptr,
                                        nullptr,
                                        instance,
                                        nullptr);
    if (!window) {
        MessageBoxW(nullptr, L"Failed to create the converter window.", kWindowTitle, MB_ICONERROR | MB_OK);
        return 1;
    }

    ShowWindow(window, show_command == 0 ? SW_SHOWNORMAL : show_command);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

#endif
