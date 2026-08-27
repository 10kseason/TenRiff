#include "app/RankedRecordsClient.h"

#include "config/SimpleJson.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <dpapi.h>
#include <winhttp.h>
#endif

namespace tenriff::app {
namespace {

constexpr std::size_t kMaximumResponseBytes = 1024 * 1024;
constexpr std::size_t kMaximumReplayBytes = 5 * 1024 * 1024;

struct Credentials {
    std::string username;
    std::string password;
};

struct HttpResponse {
    unsigned status = 0;
    std::string body;
};

void secure_clear(std::string& value) {
#ifdef _WIN32
    if (!value.empty()) SecureZeroMemory(value.data(), value.size());
#else
    std::fill(value.begin(), value.end(), '\0');
#endif
    value.clear();
}

bool valid_sha256(std::string_view value) {
    return value.size() == 64 &&
           std::all_of(value.begin(), value.end(), [](unsigned char byte) {
               return std::isxdigit(byte) != 0;
           });
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char byte) {
        return static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte);
    });
    return value;
}

std::string json_escape(std::string_view value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(value.size() + 8);
    for (const unsigned char byte : value) {
        switch (byte) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (byte < 0x20) {
                    output += "\\u00";
                    output.push_back(digits[byte >> 4U]);
                    output.push_back(digits[byte & 0x0fU]);
                } else {
                    output.push_back(static_cast<char>(byte));
                }
        }
    }
    return output;
}

std::string json_string_field(std::string_view body, const char* name) {
    const auto parsed = config::parse_json(body);
    if (!parsed.success() || !parsed.root->is_object()) return {};
    const auto* object = parsed.root->as_object();
    const auto found = object->find(name);
    return found != object->end() && found->second.is_string()
               ? found->second.as_string()
               : std::string{};
}

std::string response_error(const HttpResponse& response) {
    const std::string message = json_string_field(response.body, "error");
    return message.empty()
               ? "Ranked server returned HTTP " + std::to_string(response.status) + "."
               : message;
}

std::string base64_encode(const std::vector<unsigned char>& input) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve((input.size() + 2) / 3 * 4);
    for (std::size_t cursor = 0; cursor < input.size(); cursor += 3) {
        const unsigned first = input[cursor];
        const unsigned second = cursor + 1 < input.size() ? input[cursor + 1] : 0;
        const unsigned third = cursor + 2 < input.size() ? input[cursor + 2] : 0;
        const unsigned value = (first << 16U) | (second << 8U) | third;
        output.push_back(alphabet[(value >> 18U) & 0x3fU]);
        output.push_back(alphabet[(value >> 12U) & 0x3fU]);
        output.push_back(cursor + 1 < input.size() ? alphabet[(value >> 6U) & 0x3fU] : '=');
        output.push_back(cursor + 2 < input.size() ? alphabet[value & 0x3fU] : '=');
    }
    return output;
}

#ifdef _WIN32
class InternetHandle {
public:
    explicit InternetHandle(HINTERNET value = nullptr) : value_(value) {}
    ~InternetHandle() { if (value_) WinHttpCloseHandle(value_); }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    [[nodiscard]] HINTERNET get() const { return value_; }
    [[nodiscard]] explicit operator bool() const { return value_ != nullptr; }
private:
    HINTERNET value_ = nullptr;
};

std::wstring utf8_to_wide(std::string_view value) {
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

std::string wide_to_utf8(std::wstring_view value) {
    if (value.empty()) return {};
    const int count = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string output(static_cast<std::size_t>(count), '\0');
    return WideCharToMultiByte(
               CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
               output.data(), count, nullptr, nullptr) == count
               ? output
               : std::string{};
}

std::string winhttp_error(const char* operation) {
    return std::string(operation) + " failed (WinHTTP error " +
           std::to_string(GetLastError()) + ").";
}

bool request_json(const std::string& base_url,
                  const std::wstring& method,
                  const std::wstring& endpoint,
                  const std::string& body,
                  const std::string& bearer_token,
                  HttpResponse& response,
                  std::string& error) {
    response = {};
    const std::wstring url = utf8_to_wide(base_url);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (url.empty() || !WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()),
                                        0, &components) ||
        (components.nScheme != INTERNET_SCHEME_HTTP &&
         components.nScheme != INTERNET_SCHEME_HTTPS) ||
        components.dwHostNameLength == 0 || components.dwExtraInfoLength != 0) {
        error = "Ranked server URL is invalid.";
        return false;
    }
    const std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    while (!path.empty() && path.back() == L'/') path.pop_back();
    path += endpoint;

    InternetHandle session(WinHttpOpen(L"TenRiff/1.5.1 ranked-records",
                                       WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                       WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) { error = winhttp_error("WinHttpOpen"); return false; }
    WinHttpSetTimeouts(session.get(), 3000, 3000, 5000, 35000);
    InternetHandle connection(WinHttpConnect(session.get(), host.c_str(),
                                             components.nPort, 0));
    if (!connection) { error = winhttp_error("WinHttpConnect"); return false; }
    const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS
                            ? WINHTTP_FLAG_SECURE
                            : 0;
    InternetHandle request(WinHttpOpenRequest(
        connection.get(), method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request) { error = winhttp_error("WinHttpOpenRequest"); return false; }
    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY,
                          &redirect_policy, sizeof(redirect_policy))) {
        error = winhttp_error("WinHttpSetOption redirect policy");
        return false;
    }
    std::wstring headers = L"Accept: application/json\r\n";
    if (method == L"POST") headers += L"Content-Type: application/json\r\n";
    if (!bearer_token.empty()) {
        headers += L"Authorization: Bearer ";
        headers += utf8_to_wide(bearer_token);
        headers += L"\r\n";
    }
    if (!WinHttpSendRequest(request.get(), headers.c_str(),
                            static_cast<DWORD>(headers.size()),
                            body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
                            static_cast<DWORD>(body.size()),
                            static_cast<DWORD>(body.size()), 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
        error = winhttp_error("Ranked server request");
        return false;
    }
    DWORD status_size = sizeof(response.status);
    if (!WinHttpQueryHeaders(request.get(),
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &response.status,
                             &status_size, WINHTTP_NO_HEADER_INDEX)) {
        error = winhttp_error("WinHttpQueryHeaders");
        return false;
    }
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available)) {
            error = winhttp_error("WinHttpQueryDataAvailable");
            return false;
        }
        if (available == 0) break;
        if (available > kMaximumResponseBytes - response.body.size()) {
            error = "Ranked server response exceeds 1 MiB.";
            return false;
        }
        const auto old_size = response.body.size();
        response.body.resize(old_size + available);
        DWORD received = 0;
        if (!WinHttpReadData(request.get(), response.body.data() + old_size,
                             available, &received)) {
            error = winhttp_error("WinHttpReadData");
            return false;
        }
        response.body.resize(old_size + received);
    }
    return true;
}

bool post_json(const std::string& base_url,
               const std::wstring& endpoint,
               const std::string& body,
               const std::string& bearer_token,
               HttpResponse& response,
               std::string& error) {
    return request_json(base_url, L"POST", endpoint, body, bearer_token,
                        response, error);
}

bool get_json(const std::string& base_url,
              const std::wstring& endpoint,
              const std::string& bearer_token,
              HttpResponse& response,
              std::string& error) {
    return request_json(base_url, L"GET", endpoint, {}, bearer_token,
                        response, error);
}

bool save_credentials(const std::filesystem::path& path,
                      const Credentials& credentials,
                      std::string& error) {
    std::string plaintext = credentials.username + "\n" + credentials.password;
    DATA_BLOB input{static_cast<DWORD>(plaintext.size()),
                    reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()))};
    const std::string entropy_text = "TenRiff ranked account v1";
    DATA_BLOB entropy{static_cast<DWORD>(entropy_text.size()),
                      reinterpret_cast<BYTE*>(const_cast<char*>(entropy_text.data()))};
    DATA_BLOB encrypted{};
    const bool protected_ok = CryptProtectData(
        &input, L"TenRiff ranked account", &entropy, nullptr,
        nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted) != FALSE;
    secure_clear(plaintext);
    if (!protected_ok) {
        error = "Could not protect ranked account credentials.";
        return false;
    }
    std::error_code filesystem_error;
    std::filesystem::create_directories(path.parent_path(), filesystem_error);
    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(encrypted.pbData), encrypted.cbData);
    LocalFree(encrypted.pbData);
    output.close();
    if (!output) {
        std::filesystem::remove(temporary, filesystem_error);
        error = "Could not save protected ranked account credentials.";
        return false;
    }
    std::filesystem::remove(path, filesystem_error);
    filesystem_error.clear();
    std::filesystem::rename(temporary, path, filesystem_error);
    if (filesystem_error) {
        std::filesystem::remove(temporary, filesystem_error);
        error = "Could not install protected ranked account credentials.";
        return false;
    }
    return true;
}

bool load_credentials(const std::filesystem::path& path,
                      Credentials& credentials,
                      bool& missing,
                      std::string& error) {
    missing = false;
    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(path, filesystem_error)) {
        missing = true;
        return true;
    }
    std::ifstream input(path, std::ios::binary);
    std::vector<unsigned char> encrypted;
    encrypted.assign(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
    if (input.bad() || encrypted.empty() || encrypted.size() > 4096) {
        error = "Protected ranked account file is invalid.";
        return false;
    }
    DATA_BLOB encrypted_blob{static_cast<DWORD>(encrypted.size()), encrypted.data()};
    const std::string entropy_text = "TenRiff ranked account v1";
    DATA_BLOB entropy{static_cast<DWORD>(entropy_text.size()),
                      reinterpret_cast<BYTE*>(const_cast<char*>(entropy_text.data()))};
    DATA_BLOB plaintext{};
    if (!CryptUnprotectData(&encrypted_blob, nullptr, &entropy, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &plaintext)) {
        error = "Could not unlock ranked account credentials for this Windows user.";
        return false;
    }
    std::string value(reinterpret_cast<const char*>(plaintext.pbData),
                      plaintext.cbData);
    SecureZeroMemory(plaintext.pbData, plaintext.cbData);
    LocalFree(plaintext.pbData);
    const auto newline = value.find('\n');
    if (newline == std::string::npos || newline < 3 || newline > 32 ||
        value.size() - newline - 1 < 10 || value.size() - newline - 1 > 128) {
        error = "Protected ranked account contents are invalid.";
        return false;
    }
    credentials.username = value.substr(0, newline);
    credentials.password = value.substr(newline + 1);
    secure_clear(value);
    return true;
}

bool request_account(const std::string& base_url,
                     const Credentials& credentials,
                     bool create_account,
                     RankedAccountSession& session,
                     std::string& error) {
    std::string body =
        "{\"username\":\"" + json_escape(credentials.username) +
        "\",\"password\":\"" + json_escape(credentials.password) + "\"}";
    HttpResponse response;
    const std::wstring endpoint = create_account
                                      ? L"/v1/accounts/register"
                                      : L"/v1/accounts/login";
    const bool posted = post_json(base_url, endpoint, body, {}, response, error);
    secure_clear(body);
    if (!posted) return false;
    const unsigned expected = create_account ? 201u : 200u;
    if (response.status != expected) {
        error = response_error(response);
        return false;
    }
    session.username = json_string_field(response.body, "username");
    session.role = json_string_field(response.body, "role");
    session.bearer_token = json_string_field(response.body, "token");
    session.expires_at_utc = json_string_field(response.body, "expires_at_utc");
    if (!session.valid()) {
        session = {};
        error = "Ranked account response is incomplete.";
        return false;
    }
    if (session.role.empty()) session.role = "user";
    return true;
}

bool parse_global_chat_response(std::string_view body,
                                std::vector<RankedGlobalChatMessage>& messages,
                                std::string& error) {
    const auto parsed = config::parse_json(body);
    if (!parsed.success() || !parsed.root->is_object()) {
        error = "Global chat response is invalid JSON.";
        return false;
    }
    const auto* root = parsed.root->as_object();
    const auto found = root->find("messages");
    const auto* array = found == root->end() ? nullptr : found->second.as_array();
    if (!array) {
        error = "Global chat response has no messages array.";
        return false;
    }
    for (const auto& value : *array) {
        const auto* object = value.as_object();
        if (!object) continue;
        const auto id = object->find("id");
        const auto username = object->find("username");
        const auto role = object->find("role");
        const auto text = object->find("text");
        const auto created = object->find("created_at_utc");
        if (id == object->end() || username == object->end() ||
            text == object->end() || created == object->end() ||
            !id->second.is_number() || !username->second.is_string() ||
            !text->second.is_string() || !created->second.is_string()) {
            continue;
        }
        RankedGlobalChatMessage message;
        message.id = static_cast<std::int64_t>(id->second.as_number());
        message.username = username->second.as_string();
        message.role = role != object->end() && role->second.is_string()
                           ? role->second.as_string()
                           : "user";
        message.text = text->second.as_string();
        message.created_at_utc = created->second.as_string();
        if (message.id > 0 && !message.username.empty() && !message.text.empty()) {
            messages.push_back(std::move(message));
        }
    }
    return true;
}

std::string base_url_host(const std::string& base_url) {
    const std::wstring url = utf8_to_wide(base_url);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    if (url.empty() || !WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()),
                                        0, &components) ||
        components.dwHostNameLength == 0) {
        return {};
    }
    return wide_to_utf8(std::wstring_view(
        components.lpszHostName, components.dwHostNameLength));
}

bool parse_multiplayer_rooms_response(
    std::string_view body,
    std::string_view address,
    std::vector<RankedMultiplayerRoom>& rooms,
    std::string& error) {
    const auto parsed = config::parse_json(body);
    if (!parsed.success() || !parsed.root->is_object()) {
        error = "Multiplayer room response is invalid JSON.";
        return false;
    }
    const auto* root = parsed.root->as_object();
    const auto found = root->find("rooms");
    const auto* array = found == root->end() ? nullptr : found->second.as_array();
    if (!array) {
        error = "Multiplayer room response has no rooms array.";
        return false;
    }
    for (const auto& value : *array) {
        const auto* object = value.as_object();
        if (!object) continue;
        const auto id = object->find("id");
        const auto name = object->find("name");
        const auto port = object->find("tcp_port");
        const auto players = object->find("player_count");
        const auto maximum = object->find("max_players");
        const auto accepting = object->find("accepting_players");
        const auto active = object->find("round_active");
        if (id == object->end() || name == object->end() ||
            port == object->end() || players == object->end() ||
            maximum == object->end() || accepting == object->end() ||
            active == object->end() || !id->second.is_string() ||
            !name->second.is_string() || !port->second.is_number() ||
            !players->second.is_number() || !maximum->second.is_number() ||
            !accepting->second.is_bool() || !active->second.is_bool()) {
            continue;
        }
        const double port_value = port->second.as_number();
        const double player_value = players->second.as_number();
        const double maximum_value = maximum->second.as_number();
        if (port_value < 1 || port_value > 65535 ||
            player_value < 0 || player_value > 8 ||
            maximum_value < 1 || maximum_value > 8 ||
            player_value > maximum_value) {
            continue;
        }
        RankedMultiplayerRoom room;
        room.id = id->second.as_string();
        room.name = name->second.as_string();
        room.address = std::string(address);
        room.tcp_port = static_cast<std::uint16_t>(port_value);
        room.player_count = static_cast<std::uint8_t>(player_value);
        room.max_players = static_cast<std::uint8_t>(maximum_value);
        room.accepting_players = accepting->second.as_bool();
        room.round_active = active->second.as_bool();
        if (!room.id.empty() && !room.name.empty() && !room.address.empty()) {
            rooms.push_back(std::move(room));
        }
    }
    return true;
}
#endif

}  // namespace

bool authenticate_ranked_account(const std::string& base_url,
                                 const std::filesystem::path& profile_directory,
                                 const std::string& username,
                                 const std::string& password,
                                 bool create_account,
                                 RankedAccountSession& session,
                                 std::string& error) {
    session = {};
    error.clear();
#ifdef _WIN32
    Credentials credentials{username, password};
    if (!request_account(base_url, credentials, create_account, session, error)) {
        secure_clear(credentials.password);
        return false;
    }
    const bool saved = save_credentials(
        profile_directory / "ranked-account.dpapi", credentials, error);
    secure_clear(credentials.password);
    if (!saved) {
        session = {};
        return false;
    }
    return true;
#else
    static_cast<void>(base_url);
    static_cast<void>(profile_directory);
    static_cast<void>(username);
    static_cast<void>(password);
    static_cast<void>(create_account);
    error = "Ranked accounts are not available on this platform build.";
    return false;
#endif
}

bool saved_ranked_account_username(const std::filesystem::path& profile_directory,
                                   std::string& username,
                                   std::string& error) {
    username.clear();
    error.clear();
#ifdef _WIN32
    Credentials credentials;
    bool missing = false;
    if (!load_credentials(profile_directory / "ranked-account.dpapi",
                          credentials, missing, error)) {
        return false;
    }
    if (!missing) username = credentials.username;
    secure_clear(credentials.password);
    return true;
#else
    static_cast<void>(profile_directory);
    return true;
#endif
}

bool clear_saved_ranked_account(const std::filesystem::path& profile_directory,
                                std::string& error) {
    error.clear();
    std::error_code filesystem_error;
    std::filesystem::remove(profile_directory / "ranked-account.dpapi", filesystem_error);
    if (filesystem_error) {
        error = "Could not remove protected ranked account credentials.";
        return false;
    }
    return true;
}

bool fetch_ranked_global_chat(const std::string& base_url,
                              const std::string& bearer_token,
                              std::int64_t after_id,
                              std::vector<RankedGlobalChatMessage>& messages,
                              std::string& error) {
    messages.clear();
    error.clear();
#ifdef _WIN32
    HttpResponse response;
    const std::wstring endpoint = L"/v1/chat/messages?after_id=" +
                                  std::to_wstring(std::max<std::int64_t>(0, after_id)) +
                                  L"&limit=100";
    if (!get_json(base_url, endpoint, bearer_token, response, error)) return false;
    if (response.status != 200) {
        error = response_error(response);
        return false;
    }
    return parse_global_chat_response(response.body, messages, error);
#else
    static_cast<void>(base_url);
    static_cast<void>(bearer_token);
    static_cast<void>(after_id);
    error = "Global chat is not available on this platform build.";
    return false;
#endif
}

bool send_ranked_global_chat(const std::string& base_url,
                             const std::string& bearer_token,
                             const std::string& text,
                             std::string& error) {
    error.clear();
#ifdef _WIN32
    std::string body = "{\"text\":\"" + json_escape(text) + "\"}";
    HttpResponse response;
    const bool posted = post_json(base_url, L"/v1/chat/messages", body,
                                  bearer_token, response, error);
    secure_clear(body);
    if (!posted) return false;
    if (response.status != 201) {
        error = response_error(response);
        return false;
    }
    return true;
#else
    static_cast<void>(base_url);
    static_cast<void>(bearer_token);
    static_cast<void>(text);
    error = "Global chat is not available on this platform build.";
    return false;
#endif
}

bool fetch_ranked_multiplayer_rooms(
    const std::string& base_url,
    const std::string& bearer_token,
    std::vector<RankedMultiplayerRoom>& rooms,
    std::string& error) {
    rooms.clear();
    error.clear();
#ifdef _WIN32
    const std::string host = base_url_host(base_url);
    if (host.empty()) {
        error = "Multiplayer server URL has no valid host.";
        return false;
    }
    HttpResponse response;
    if (!get_json(base_url, L"/v1/multiplayer/rooms", bearer_token,
                  response, error)) {
        return false;
    }
    if (response.status != 200) {
        error = response_error(response);
        return false;
    }
    return parse_multiplayer_rooms_response(response.body, host, rooms, error);
#else
    static_cast<void>(base_url);
    static_cast<void>(bearer_token);
    error = "Multiplayer room search is not available on this platform build.";
    return false;
#endif
}

bool prepare_ranked_play(const std::string& base_url,
                         const std::filesystem::path& profile_directory,
                         const std::string& preferred_username,
                         const std::string& chart_sha256,
                         RankedPlayAuthorization& authorization,
                         std::string& error) {
    authorization = {};
    error.clear();
    static_cast<void>(preferred_username);
    if (!valid_sha256(chart_sha256)) {
        error = "Ranked play requires an indexed BMS SHA-256.";
        return false;
    }
#ifdef _WIN32
    const auto credentials_path = profile_directory / "ranked-account.dpapi";
    Credentials credentials;
    bool missing = false;
    if (!load_credentials(credentials_path, credentials, missing, error)) return false;
    if (missing) {
        error = "No ranked account is signed in. Press F10 to log in or register.";
        return false;
    }
    RankedAccountSession account;
    if (!request_account(base_url, credentials, false, account, error)) {
        secure_clear(credentials.password);
        return false;
    }
    secure_clear(credentials.password);
    authorization.bearer_token = account.bearer_token;
    authorization.username = account.username;

    const std::string challenge_body =
        "{\"chart_sha256\":\"" + lower_ascii(chart_sha256) + "\"}";
    HttpResponse challenge;
    if (!post_json(base_url, L"/v1/challenges", challenge_body,
                   authorization.bearer_token, challenge, error)) {
        authorization = {};
        return false;
    }
    if (challenge.status != 201) {
        error = response_error(challenge);
        authorization = {};
        return false;
    }
    authorization.challenge_id = json_string_field(challenge.body, "challenge_id");
    authorization.challenge_nonce = json_string_field(challenge.body, "nonce");
    if (!authorization.valid()) {
        error = "Ranked challenge response is incomplete.";
        authorization = {};
        return false;
    }
    return true;
#else
    static_cast<void>(base_url);
    static_cast<void>(profile_directory);
    static_cast<void>(preferred_username);
    error = "Ranked replay upload is not available on this platform build.";
    return false;
#endif
}

bool submit_ranked_replay(const std::string& base_url,
                          const RankedPlayAuthorization& authorization,
                          const std::filesystem::path& replay_path,
                          std::string& receipt,
                          std::string& error) {
    receipt.clear();
    error.clear();
    if (!authorization.valid()) {
        error = "Ranked replay has no active challenge.";
        return false;
    }
#ifdef _WIN32
    std::error_code filesystem_error;
    const auto replay_size = std::filesystem::file_size(replay_path, filesystem_error);
    if (filesystem_error || replay_size == 0 || replay_size > kMaximumReplayBytes) {
        error = "Ranked replay must be readable and at most 5 MiB.";
        return false;
    }
    std::ifstream input(replay_path, std::ios::binary);
    std::vector<unsigned char> bytes;
    bytes.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
    if (input.bad() || bytes.size() != replay_size) {
        error = "Could not read the ranked replay file.";
        return false;
    }
    const std::string body =
        "{\"challenge_id\":\"" + json_escape(authorization.challenge_id) +
        "\",\"challenge_nonce\":\"" + json_escape(authorization.challenge_nonce) +
        "\",\"replay_base64\":\"" + base64_encode(bytes) + "\"}";
    HttpResponse response;
    if (!post_json(base_url, L"/v1/replays", body,
                   authorization.bearer_token, response, error)) {
        return false;
    }
    if (response.status != 201) {
        error = response_error(response);
        return false;
    }
    receipt = json_string_field(response.body, "receipt");
    if (receipt.empty()) {
        error = "Ranked replay response is missing its verification receipt.";
        return false;
    }
    return true;
#else
    static_cast<void>(base_url);
    static_cast<void>(replay_path);
    error = "Ranked replay upload is not available on this platform build.";
    return false;
#endif
}

}  // namespace tenriff::app
