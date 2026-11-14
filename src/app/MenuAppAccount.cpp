#include "app/NativeFileDialog.h"
#include "app/MenuApp.h"

#include <algorithm>
#include <filesystem>
#include <utility>
#include <fstream>

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

#include "app/ChatInteraction.h"
#include "app/ClipboardText.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {
namespace {

void clear_secret(std::string& value) {
#ifdef _WIN32
    if (!value.empty()) SecureZeroMemory(value.data(), value.size());
#else
    std::fill(value.begin(), value.end(), '\0');
#endif
    value.clear();
}

#ifdef _WIN32
std::wstring account_utf8_to_wide(std::string_view value) {
    if (value.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          value.data(), static_cast<int>(value.size()),
                                          nullptr, 0);
    if (count <= 0) return {};
    std::wstring output(static_cast<std::size_t>(count), L'\0');
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                               value.data(), static_cast<int>(value.size()),
                               output.data(), count) == count
               ? output
               : std::wstring{};
}
#endif


#ifdef _WIN32
NativeFileChoice pick_sites_connection_file() {
    return run_native_file_dialog_sta([] {
        NativeFileChoice result;
        Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (SUCCEEDED(hr)) {
            DWORD options = 0;
            hr = dialog->GetOptions(&options);
            if (SUCCEEDED(hr)) hr = dialog->SetOptions(options | FOS_FORCEFILESYSTEM |
                FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
            const COMDLG_FILTERSPEC filter[] = {{L"TenRiff connection (*.json)", L"*.json"}};
            if (SUCCEEDED(hr)) hr = dialog->SetFileTypes(1, filter);
            if (SUCCEEDED(hr)) hr = dialog->SetTitle(L"Select TenRiff leaderboard connection");
            if (SUCCEEDED(hr)) hr = show_native_file_dialog(dialog.Get());
            if (SUCCEEDED(hr)) {
                Microsoft::WRL::ComPtr<IShellItem> item;
                hr = dialog->GetResult(&item);
                struct ShellPath { PWSTR value = nullptr; ~ShellPath() { CoTaskMemFree(value); } } raw;
                if (SUCCEEDED(hr) && item) hr = item->GetDisplayName(SIGDN_FILESYSPATH, &raw.value);
                if (SUCCEEDED(hr) && raw.value) result.path = util::utf8_from_wide_lossy(raw.value);
            }
        }
        result.cancelled = hr == HRESULT_FROM_WIN32(ERROR_CANCELLED);
        if (result.path.empty() && !result.cancelled) result.error = "Could not read the selected connection file.";
        return result;
    });
}
#endif

}  // namespace

void MenuApp::choose_play_mode(bool online) {
    // Once guest mode touches a play, returning online must never promote that record.
    if (!online && active_recording_permission_)
        active_recording_permission_->store(false, std::memory_order_release);
    if (online != online_play_mode_ || !online) {
        peer_session_.disable_hosted();
        multiplayer_account_checked_ = false;
    }
    sites_leaderboard_service_.set_online_enabled(online);
    online_play_mode_ = online;
    account_mode_choice_online_ = online;
    account_mode_selection_ = false;
    if (online) {
        refresh_sites_leaderboard_connection();
    } else {
        set_ranked_account_overlay(false);
    }
    publish_snapshot();
}

void MenuApp::toggle_ranked_account_overlay() {
    set_ranked_account_overlay(!ranked_account_overlay_visible_);
    publish_snapshot();
}

void MenuApp::set_ranked_account_overlay(bool visible) {
    ranked_account_overlay_visible_ = visible;
    if (visible) {
        account_mode_selection_ = true;
        account_mode_choice_online_ = online_play_mode_;
        set_multiplayer_chat_overlay(false);
        dismiss_chat_url_warning();
        refresh_sites_leaderboard_connection();
    }
    gameplay_overlay_capture_active_.store(
        visible || chat_overlay_visible_, std::memory_order_release);
}

bool MenuApp::handle_ranked_account_overlay_input(uint32_t keycode) {
    if (!ranked_account_overlay_visible_) return false;
    if (keycode == key_escape_) {
        set_ranked_account_overlay(false);
        publish_snapshot();
        return true;
    }
    if (account_mode_selection_) {
        if (keycode == key_left_) account_mode_choice_online_ = false;
        else if (keycode == key_right_) account_mode_choice_online_ = true;
        else if (keycode == key_tab_) account_mode_choice_online_ = !account_mode_choice_online_;
        else if (keycode == key_enter_) { choose_play_mode(account_mode_choice_online_); return true; }
        publish_snapshot();
        return true;
    }
    if (keycode == key_v_ && control_modifier_pressed()) {
        handle_sites_leaderboard_action(render::SitesLeaderboardAction::PasteConnection);
    } else if (keycode == key_enter_) {
        if (sites_connection_saved_) {
            set_ranked_account_overlay(false);
            publish_snapshot();
        } else {
            handle_sites_leaderboard_action(render::SitesLeaderboardAction::ImportFile);
        }
    }
    return true;
}

void MenuApp::populate_ranked_account_overlay(render::RankedAccountOverlayData& target) const {
    target.visible = ranked_account_overlay_visible_;
    target.mode_selection = account_mode_selection_;
    target.online_mode = online_play_mode_;
    target.mode_choice_online = account_mode_choice_online_;
    target.sites_connected = sites_connection_saved_;
    target.sites_url = kSitesLeaderboardUrl;
    target.sites_status = sites_connection_status_;
}

void MenuApp::refresh_sites_leaderboard_connection() {
    SitesLeaderboardConnection connection;
    bool missing = false;
    std::string error;
    const bool loaded = load_sites_leaderboard_connection("config", connection, missing, error);
    sites_connection_saved_ = loaded && !missing;
    clear_secret(connection.upload_token);
    sites_connection_status_ = !online_play_mode_
        ? ui_text("Guest mode: records stay local and are never uploaded later.", "게스트 모드: 기록은 로컬 전용이며 나중에도 업로드되지 않습니다.")
        : !loaded ? error : sites_connection_saved_
        ? ui_text("Connection saved. Eligible records upload automatically.",
                  "연결 정보가 저장되어 있습니다. 플레이 후 자동 업로드합니다.")
        : ui_text("Open the website, sign in, and copy your connection information.",
                  "웹사이트에서 로그인한 뒤 연결 정보를 복사해 주세요.");
    if (online_play_mode_ && sites_connection_saved_ && !sites_last_upload_status_.empty())
        sites_connection_status_ = sites_last_upload_status_;
}

void MenuApp::handle_sites_leaderboard_action(render::SitesLeaderboardAction action) {
    std::string error;
    if (action == render::SitesLeaderboardAction::OpenWebsite) {
#ifdef _WIN32
        const auto url = account_utf8_to_wide(kSitesLeaderboardUrl);
        const auto opened = ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(opened) <= 32) {
            sites_connection_status_ = ui_text("Could not open the website.", "웹사이트를 열지 못했습니다.");
        }
#else
        sites_connection_status_ = ui_text("Open the leaderboard in your browser.", "브라우저에서 리더보드를 열어주세요.");
#endif
    } else if (action == render::SitesLeaderboardAction::Disconnect) {
        if (clear_sites_leaderboard_connection("config", error)) {
            peer_session_.disable_hosted();
            multiplayer_account_checked_ = false;
            sites_leaderboard_service_.cancel_pending();
            sites_last_upload_status_.clear();
            sites_result_replay_sha256_.clear();
            sites_result_upload_status_ = ui_text("Web connection changed; record saved locally.",
                                                 "웹 연결 변경 · 기록은 로컬에 저장");
            refresh_sites_leaderboard_connection();
            sites_connection_status_ = ui_text("Automatic uploads disabled on this PC.",
                                               "이 PC의 자동 업로드 연결을 해제했습니다.");
        } else sites_connection_status_ = error;
    } else {
        std::string contents;
        if (action == render::SitesLeaderboardAction::PasteConnection) {
            auto clipboard = clipboard_text_utf8(false);
            if (clipboard) contents = std::move(*clipboard);
            else error = ui_text("Clipboard is empty.", "클립보드가 비어 있습니다.");
        } else if (action == render::SitesLeaderboardAction::ImportFile) {
#ifdef _WIN32
            const auto choice = pick_sites_connection_file();
            if (choice.path.empty()) {
                if (!choice.error.empty()) sites_connection_status_ = choice.error;
                publish_snapshot();
                return;
            }
            const auto picked = std::filesystem::u8path(choice.path);
            std::error_code ec;
            const auto size = std::filesystem::file_size(picked, ec);
            if (ec || size == 0 || size > 4096) error = ui_text(
                "Select a connection JSON file smaller than 4 KiB.", "4 KiB 이하의 연결 JSON 파일을 선택해 주세요.");
            else {
                contents.resize(static_cast<std::size_t>(size));
                std::ifstream input(picked, std::ios::binary);
                if (!input.read(contents.data(), static_cast<std::streamsize>(size)))
                    error = ui_text("Could not read the connection file.", "연결 파일을 읽지 못했습니다.");
            }
#else
            error = "The file picker is available on Windows.";
#endif
        }
        if (error.empty() && install_sites_leaderboard_connection("config", contents, error)) {
            peer_session_.disable_hosted();
            multiplayer_account_checked_ = false;
            sites_leaderboard_service_.cancel_pending();
            sites_last_upload_status_.clear();
            sites_result_replay_sha256_.clear();
            sites_result_upload_status_ = ui_text("Web connection changed; record saved locally.",
                                                 "웹 연결 변경 · 기록은 로컬에 저장");
            refresh_sites_leaderboard_connection();
            sites_connection_status_ = ui_text("Connection imported and protected. You can play now.",
                                               "연결 정보를 암호화해 저장했습니다. 이제 플레이하면 됩니다.");
        } else if (!error.empty()) sites_connection_status_ = error;
        clear_secret(contents);
    }
    publish_snapshot();
}

void MenuApp::show_chat_url_warning(std::string url) {
    const auto parsed = first_chat_web_url(url);
    if (!parsed.has_value() || *parsed != url) return;
    chat_url_warning_target_ = std::move(url);
    chat_url_warning_visible_ = true;
    gameplay_overlay_capture_active_.store(true, std::memory_order_release);
    publish_snapshot();
}

void MenuApp::dismiss_chat_url_warning() {
    chat_url_warning_visible_ = false;
    chat_url_warning_target_.clear();
    gameplay_overlay_capture_active_.store(
        chat_overlay_visible_ || ranked_account_overlay_visible_,
        std::memory_order_release);
}

bool MenuApp::handle_chat_url_warning_input(uint32_t keycode) {
    if (!chat_url_warning_visible_) return false;
    if (keycode == key_escape_) {
        dismiss_chat_url_warning();
        publish_snapshot();
    } else if (keycode == key_enter_) {
        open_warned_chat_url();
    }
    return true;
}

void MenuApp::open_warned_chat_url() {
    const std::string url = chat_url_warning_target_;
    const auto parsed = first_chat_web_url(url);
    if (!parsed.has_value() || *parsed != url) {
        dismiss_chat_url_warning();
        publish_snapshot();
        return;
    }
#ifdef _WIN32
    const std::wstring wide = account_utf8_to_wide(url);
    const HINSTANCE opened = wide.empty()
                                 ? nullptr
                                 : ShellExecuteW(nullptr, L"open", wide.c_str(),
                                                 nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(opened) <= 32) {
        multiplayer_status_message_ = ui_text(
            "Windows could not open the selected link.",
            "Windows에서 선택한 링크를 열지 못했습니다.");
    }
#endif
    dismiss_chat_url_warning();
    publish_snapshot();
}

}  // namespace tenriff::app
