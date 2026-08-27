#include "app/MenuApp.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
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
