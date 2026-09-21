#include "app/MenuApp.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>
#include <fstream>
#include <optional>

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
#include "app/AccountInput.h"
#include "app/ClipboardText.h"
#include "app/MenuAppSongSelectUtils.h"
#include "app/RankedRecordsClient.h"

namespace tenriff::app {
namespace {

bool valid_account_username(std::string_view value) {
    return value.size() >= 3 && value.size() <= 32 &&
           std::all_of(value.begin(), value.end(), [](unsigned char byte) {
               return std::isalnum(byte) != 0 || byte == '_' || byte == '-' || byte == '.';
           });
}

void erase_last_utf8(std::string& value) {
    if (value.empty()) return;
    std::size_t offset = value.size() - 1;
    while (offset > 0 &&
           (static_cast<unsigned char>(value[offset]) & 0xc0u) == 0x80u) {
        --offset;
    }
    value.erase(offset);
}

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
std::optional<std::filesystem::path> pick_sites_connection_file(std::string& error) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(initialized)) { error = "Could not open the file picker."; return std::nullopt; }
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    std::optional<std::filesystem::path> result;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(hr)) {
        DWORD options = 0;
        hr = dialog->GetOptions(&options);
        if (SUCCEEDED(hr)) hr = dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
        const COMDLG_FILTERSPEC filter[] = {{L"TenRiff connection (*.json)", L"*.json"}};
        if (SUCCEEDED(hr)) hr = dialog->SetFileTypes(1, filter);
        if (SUCCEEDED(hr)) hr = dialog->SetTitle(L"Select TenRiff leaderboard connection");
        if (SUCCEEDED(hr)) hr = dialog->Show(nullptr);
        if (SUCCEEDED(hr)) {
            Microsoft::WRL::ComPtr<IShellItem> item;
            hr = dialog->GetResult(&item);
            PWSTR path = nullptr;
            if (SUCCEEDED(hr)) hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
            if (SUCCEEDED(hr) && path) result = std::filesystem::path(path);
            if (path) CoTaskMemFree(path);
        }
    }
    dialog.Reset();
    CoUninitialize();
    if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) error = "Could not read the selected connection file.";
    return result;
}
#endif

}  // namespace

void MenuApp::toggle_ranked_account_overlay() {
    set_ranked_account_overlay(!ranked_account_overlay_visible_);
    publish_snapshot();
}

void MenuApp::set_ranked_account_overlay(bool visible) {
    ranked_account_overlay_visible_ = visible;
    if (visible) {
        set_multiplayer_chat_overlay(false);
        dismiss_chat_url_warning();
        ranked_account_focused_field_ = ranked_account_use_private_server_ ? 0 : 1;
        ranked_account_status_.clear();
        refresh_sites_leaderboard_connection();
        if (ranked_account_username_.empty()) {
            ranked_account_username_ = ranked_account_signed_in_username_;
        }
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
    if (ranked_account_request_busy_.load(std::memory_order_acquire)) return true;
    if (keycode == key_tab_ && control_modifier_pressed()) {
        ranked_account_sites_mode_ = !ranked_account_sites_mode_;
        if (ranked_account_sites_mode_) refresh_sites_leaderboard_connection();
        publish_snapshot();
        return true;
    }
    if (ranked_account_sites_mode_) {
        if (keycode == key_v_ && control_modifier_pressed())
            handle_sites_leaderboard_action(render::SitesLeaderboardAction::PasteConnection);
        else if (keycode == key_enter_)
            handle_sites_leaderboard_action(render::SitesLeaderboardAction::ImportFile);
        return true;
    }
    if (!ranked_account_signed_in_username_.empty()) {
        if (keycode == key_enter_) {
            set_ranked_account_overlay(false);
            publish_snapshot();
        }
        return true;
    }
    if (keycode == key_left_ || keycode == key_right_) {
        ranked_account_register_mode_ = keycode == key_right_;
        ranked_account_status_.clear();
        publish_snapshot();
        return true;
    }
    if (keycode == key_tab_) {
        if (ranked_account_use_private_server_) {
            ranked_account_focused_field_ = (ranked_account_focused_field_ + 1) % 3;
        } else {
            ranked_account_focused_field_ = ranked_account_focused_field_ == 1 ? 2 : 1;
        }
        publish_snapshot();
        return true;
    }
    if (keycode == key_enter_) {
        begin_ranked_account_request();
        return true;
    }
    std::string& field = ranked_account_focused_field_ == 0
                             ? ranked_account_private_server_url_
                             : (ranked_account_focused_field_ == 1
                                    ? ranked_account_username_
                                    : ranked_account_password_);
    if (keycode == key_v_ && control_modifier_pressed()) {
        const auto clipboard = clipboard_text_utf8(false);
        if (ranked_account_focused_field_ == 2 && clipboard.has_value()) {
            std::string password = sanitize_pasted_account_password(*clipboard);
            if (!password.empty()) {
                clear_secret(field);
                field = std::move(password);
                ranked_account_status_ = ui_text(
                    "Password pasted securely (masked).",
                    "비밀번호를 안전하게 붙여넣었습니다(마스킹됨).");
            } else {
                ranked_account_status_ = ui_text(
                    "Clipboard does not contain a usable password.",
                    "클립보드에 사용할 수 있는 비밀번호가 없습니다.");
            }
        } else {
            ranked_account_status_ = ui_text(
                "Select the password field before pressing Ctrl+V.",
                "비밀번호 칸을 선택한 뒤 Ctrl+V를 누르세요.");
        }
        publish_snapshot();
        return true;
    }
    if (keycode == key_backspace_) {
        erase_last_utf8(field);
        publish_snapshot();
    } else if (keycode == key_delete_) {
        if (ranked_account_focused_field_ == 2) clear_secret(field);
        else field.clear();
        publish_snapshot();
    }
    return true;
}

void MenuApp::populate_ranked_account_overlay(
    render::RankedAccountOverlayData& target) const {
    target.visible = ranked_account_overlay_visible_;
    target.register_mode = ranked_account_register_mode_;
    target.busy = ranked_account_request_busy_.load(std::memory_order_acquire);
    target.signed_in = !ranked_account_signed_in_username_.empty();
    target.private_server = ranked_account_use_private_server_;
    target.sites_mode = ranked_account_sites_mode_;
    target.sites_connected = sites_connection_saved_;
    target.sites_url = sites_connection_url_.empty() ? kSitesLeaderboardUrl : sites_connection_url_;
    target.sites_status = sites_connection_status_;
    target.focused_field = ranked_account_focused_field_;
    target.username = ranked_account_username_;
    target.server_url = ranked_account_private_server_url_;
    target.main_server_url = ranked_account_main_server_url_;
    target.password_mask.assign(ranked_account_password_.size(), '*');
    target.signed_in_as = target.signed_in
                              ? ui_text("Signed in as ", "로그인됨: ") +
                                    ranked_account_signed_in_username_
                              : std::string{};
    target.role = ranked_account_role_;
    target.status = ranked_account_status_.empty()
                        ? ui_text("TAB changes field   Ctrl+V pastes password   F10 or ESC closes",
                                  "TAB 입력칸 변경   Ctrl+V 비밀번호 붙여넣기   F10 또는 ESC 닫기")
                        : ranked_account_status_;
}


void MenuApp::refresh_sites_leaderboard_connection() {
    SitesLeaderboardConnection connection;
    bool missing = false;
    std::string error;
    const bool loaded = load_sites_leaderboard_connection("config", connection, missing, error);
    sites_connection_saved_ = loaded && !missing;
    sites_connection_url_ = sites_connection_saved_ ? connection.site_url : kSitesLeaderboardUrl;
    clear_secret(connection.upload_token);
    sites_connection_status_ = !loaded ? error : sites_connection_saved_
        ? ui_text("Connection saved. Eligible records upload automatically.",
                  "연결 정보가 저장되어 있습니다. 플레이 후 자동 업로드합니다.")
        : ui_text("Open the website, sign in, and copy your connection information.",
                  "웹사이트에서 로그인한 뒤 연결 정보를 복사해 주세요.");
    if (sites_connection_saved_ && !sites_last_upload_status_.empty())
        sites_connection_status_ = sites_last_upload_status_;
}

void MenuApp::handle_sites_leaderboard_action(render::SitesLeaderboardAction action) {
    std::string error;
    if (action == render::SitesLeaderboardAction::OpenWebsite) {
#ifdef _WIN32
        const auto url = account_utf8_to_wide(sites_connection_url_.empty() ? kSitesLeaderboardUrl : sites_connection_url_);
        const auto opened = ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(opened) <= 32) {
            sites_connection_status_ = ui_text("Could not open the website.", "웹사이트를 열지 못했습니다.");
        }
#else
        sites_connection_status_ = ui_text("Open the leaderboard in your browser.", "브라우저에서 리더보드를 열어주세요.");
#endif
    } else if (action == render::SitesLeaderboardAction::Disconnect) {
        if (clear_sites_leaderboard_connection("config", error)) {
            sites_leaderboard_service_.cancel_pending();
            sites_last_upload_status_.clear();
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
            const auto picked = pick_sites_connection_file(error);
            if (!picked) {
                if (!error.empty()) sites_connection_status_ = error;
                publish_snapshot();
                return;
            }
            std::error_code ec;
            const auto size = std::filesystem::file_size(*picked, ec);
            if (ec || size == 0 || size > 4096) error = ui_text(
                "Select a connection JSON file smaller than 4 KiB.", "4 KiB 이하의 연결 JSON 파일을 선택해 주세요.");
            else {
                contents.resize(static_cast<std::size_t>(size));
                std::ifstream input(*picked, std::ios::binary);
                if (!input.read(contents.data(), static_cast<std::streamsize>(size)))
                    error = ui_text("Could not read the connection file.", "연결 파일을 읽지 못했습니다.");
            }
#else
            error = "The file picker is available on Windows.";
#endif
        }
        if (error.empty() && install_sites_leaderboard_connection("config", contents, error)) {
            sites_leaderboard_service_.cancel_pending();
            sites_last_upload_status_.clear();
            refresh_sites_leaderboard_connection();
            sites_connection_status_ = ui_text("Connection imported and protected. You can play now.",
                                               "연결 정보를 암호화해 저장했습니다. 이제 플레이하면 됩니다.");
        } else if (!error.empty()) sites_connection_status_ = error;
        clear_secret(contents);
    }
    publish_snapshot();
}

void MenuApp::begin_ranked_account_request() {
    if (ranked_account_request_busy_.load(std::memory_order_acquire)) return;
    const std::string selected_server = config::normalize_online_records_server_url(
        ranked_account_use_private_server_
            ? ranked_account_private_server_url_
            : ranked_account_main_server_url_);
    if (selected_server.empty()) {
        ranked_account_status_ = ui_text(
            "Enter a valid HTTPS API server URL. Localhost may use HTTP.",
            "올바른 HTTPS API 서버 주소를 입력하세요. localhost만 HTTP를 사용할 수 있습니다.");
        ranked_account_focused_field_ = ranked_account_use_private_server_ ? 0 : 1;
        publish_snapshot();
        return;
    }
    if (!valid_account_username(ranked_account_username_)) {
        ranked_account_status_ = ui_text(
            "Username: 3-32 ASCII letters, digits, '.', '_' or '-'.",
            "아이디는 영문/숫자/점/밑줄/하이픈 3-32자로 입력하세요.");
        publish_snapshot();
        return;
    }
    if (ranked_account_password_.size() < 10 || ranked_account_password_.size() > 128) {
        ranked_account_status_ = ui_text(
            "Password must contain 10-128 UTF-8 bytes.",
            "비밀번호는 UTF-8 기준 10-128바이트여야 합니다.");
        publish_snapshot();
        return;
    }
    if (ranked_account_thread_.joinable()) ranked_account_thread_.join();
    const std::string base_url = selected_server;
    const std::filesystem::path profile =
        menu_song_select::path_from_utf8(profile_dir_);
    const std::string username = ranked_account_username_;
    std::string password = ranked_account_password_;
    const bool create_account = ranked_account_register_mode_;
    clear_secret(ranked_account_password_);
    ranked_account_status_ = create_account
                                 ? ui_text("Creating account...", "계정을 만드는 중...")
                                 : ui_text("Signing in...", "로그인 중...");
    ranked_account_request_busy_.store(true, std::memory_order_release);
    publish_snapshot();

    ranked_account_thread_ = std::thread(
        [this, base_url, profile, username, password = std::move(password),
         create_account]() mutable {
            RankedAccountRequestResult result;
            result.available = true;
            result.success = authenticate_ranked_account(
                base_url, profile, username, password, create_account,
                result.session, result.error);
            clear_secret(password);
            std::lock_guard<std::mutex> lock(ranked_account_result_mutex_);
            ranked_account_result_ = std::move(result);
        });
}

void MenuApp::service_ranked_account_request() {
    RankedAccountRequestResult result;
    {
        std::lock_guard<std::mutex> lock(ranked_account_result_mutex_);
        if (!ranked_account_result_.available) return;
        result = std::move(ranked_account_result_);
        ranked_account_result_ = {};
    }
    if (ranked_account_thread_.joinable()) ranked_account_thread_.join();
    ranked_account_request_busy_.store(false, std::memory_order_release);
    if (result.success) {
        ranked_account_signed_in_username_ = result.session.username;
        ranked_account_username_ = result.session.username;
        ranked_account_role_ = result.session.role;
        ranked_account_active_server_url_ = ranked_account_use_private_server_
                                                ? config::normalize_online_records_server_url(
                                                      ranked_account_private_server_url_)
                                                : ranked_account_main_server_url_;
        config_.ui.account_server_mode = ranked_account_use_private_server_
                                             ? "private"
                                             : "main";
        config_.ui.tenriff_main_server_url = ranked_account_main_server_url_;
        config_.ui.private_server_url = ranked_account_private_server_url_;
        config_.ui.online_records_server_url = ranked_account_active_server_url_;
        persist_runtime_config();
        global_chat_service_.configure(ranked_account_active_server_url_,
                                       result.session.bearer_token);
        ranked_account_status_ = result.session.role == "admin"
                                     ? ui_text("Administrator signed in.", "관리자로 로그인했습니다.")
                                     : ui_text("Account ready for ranked play.",
                                               "랭킹 플레이 계정이 준비되었습니다.");
    } else {
        ranked_account_status_ = menu_song_select::safe_ui_text(
            result.error,
            ui_text("Account request failed.", "계정 요청에 실패했습니다."));
    }
    publish_snapshot();
}

void MenuApp::logout_ranked_account() {
    std::string error;
    const bool cleared = clear_saved_ranked_account(
        menu_song_select::path_from_utf8(profile_dir_), error);
    global_chat_service_.clear();
    ranked_account_signed_in_username_.clear();
    ranked_account_role_.clear();
    ranked_account_active_server_url_.clear();
    ranked_account_status_ = cleared
                                 ? ui_text("Logged out.", "로그아웃했습니다.")
                                 : menu_song_select::safe_ui_text(
                                       error, ui_text("Logout failed.", "로그아웃에 실패했습니다."));
    ranked_account_focused_field_ = ranked_account_use_private_server_ ? 0 : 1;
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
