#include "app/SitesLeaderboardConnection.h"
#include "config/SimpleJson.h"
#include <algorithm>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dpapi.h>
#endif

namespace tenriff::app {
namespace {
constexpr std::size_t kMaximumConnectionBytes = 4096;
bool is_key(std::string_view value) {
    return value.size() == 67 && value.substr(0, 3) == "tr_" &&
        std::all_of(value.begin() + 3, value.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
}
bool is_site_origin(std::string_view url) {
    constexpr std::string_view prefix = "https://", suffix = ".chatgpt.site";
    if (url.substr(0, prefix.size()) != prefix || url.size() > 253) return false;
    const auto host = url.substr(prefix.size());
    if (host.size() <= suffix.size() || host.substr(host.size() - suffix.size()) != suffix ||
        host.front() == '.' || host.front() == '-' || host.find("..") != std::string_view::npos) return false;
    return std::all_of(host.begin(), host.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-';
    });
}
void erase_secret(std::string& value) {
#ifdef _WIN32
    if (!value.empty()) SecureZeroMemory(value.data(), value.size());
#else
    std::fill(value.begin(), value.end(), '\0');
#endif
    value.clear();
}
bool read_connection_file(const std::filesystem::path& path, std::string& contents, std::string& error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size == 0 || size > kMaximumConnectionBytes) {
        error = "Connection file is missing, empty, or larger than 4 KiB."; return false;
    }
    std::ifstream input(path, std::ios::binary);
    contents.resize(static_cast<std::size_t>(size));
    if (!input.read(contents.data(), static_cast<std::streamsize>(size))) {
        error = "Could not read the connection file."; return false;
    }
    return true;
}
#ifdef _WIN32
DATA_BLOB connection_entropy() {
    static char label[] = "TenRiff Sites connection v1";
    return {sizeof(label) - 1, reinterpret_cast<BYTE*>(label)};
}
#endif
}  // namespace

bool parse_sites_leaderboard_connection(std::string_view json,
    SitesLeaderboardConnection& connection, std::string& error) {
    connection = {}; error.clear();
    if (json.size() > kMaximumConnectionBytes) { error = "Connection information exceeds 4 KiB."; return false; }
    if (json.substr(0, 3) == "\xef\xbb\xbf") json.remove_prefix(3);
    const auto parsed = config::parse_json(json);
    if (!parsed.success() || !parsed.root->is_object()) {
        error = "Sites connection information must be a JSON object."; return false;
    }
    const auto& object = *parsed.root->as_object();
    const auto version = object.find("schema_version"), url = object.find("site_url"), token = object.find("upload_token");
    if (version == object.end() || !version->second.is_number() || version->second.as_number() != 1 ||
        url == object.end() || !url->second.is_string() || token == object.end() || !token->second.is_string()) {
        error = "Sites connection information is incomplete."; return false;
    }
    auto site_url = url->second.as_string();
    if (!site_url.empty() && site_url.back() == '/') site_url.pop_back();
    const auto upload_token = token->second.as_string();
    if (!is_site_origin(site_url) || !is_key(upload_token)) {
        error = "Use a ChatGPT Site HTTPS origin and a valid upload key."; return false;
    }
    connection = {std::move(site_url), upload_token};
    return true;
}

bool load_sites_leaderboard_connection(const std::filesystem::path& directory,
    SitesLeaderboardConnection& connection, bool& missing, std::string& error) {
    connection = {}; missing = false; error.clear();
    std::error_code ec;
    const auto protected_path = directory / "sites-leaderboard.dpapi";
    const bool encrypted_exists = std::filesystem::exists(protected_path, ec);
    if (ec) { error = "Could not inspect the saved Sites connection."; return false; }
    std::string bytes;
    if (encrypted_exists) {
        if (!read_connection_file(protected_path, bytes, error)) return false;
#ifdef _WIN32
        DATA_BLOB input{static_cast<DWORD>(bytes.size()), reinterpret_cast<BYTE*>(bytes.data())};
        DATA_BLOB plaintext{}, entropy = connection_entropy();
        if (!CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &plaintext)) {
            error = "This Windows user cannot unlock the saved connection. Import it again."; return false;
        }
        std::string json(reinterpret_cast<char*>(plaintext.pbData), plaintext.cbData);
        SecureZeroMemory(plaintext.pbData, plaintext.cbData);
        LocalFree(plaintext.pbData);
        const bool valid = parse_sites_leaderboard_connection(json, connection, error);
        erase_secret(json);
        return valid;
#else
        error = "Windows-protected connections are unavailable on this platform."; return false;
#endif
    }
    // Read old manually installed configurations until the user imports them through the UI.
    const auto legacy_path = directory / "sites-leaderboard.json";
    if (!std::filesystem::exists(legacy_path, ec)) {
        if (ec) { error = "Could not inspect the legacy connection."; return false; }
        missing = true; return true;
    }
    if (!read_connection_file(legacy_path, bytes, error)) return false;
    const bool valid = parse_sites_leaderboard_connection(bytes, connection, error);
    erase_secret(bytes);
    return valid;
}

bool install_sites_leaderboard_connection(const std::filesystem::path& directory,
    std::string_view contents, std::string& error) {
    error.clear();
    while (!contents.empty() && (contents.front() == ' ' || contents.front() == '\r' || contents.front() == '\n' || contents.front() == '\t')) contents.remove_prefix(1);
    while (!contents.empty() && (contents.back() == ' ' || contents.back() == '\r' || contents.back() == '\n' || contents.back() == '\t')) contents.remove_suffix(1);
    SitesLeaderboardConnection connection;
    if (is_key(contents)) connection = {kSitesLeaderboardUrl, std::string(contents)};
    else if (!parse_sites_leaderboard_connection(contents, connection, error)) return false;
#ifdef _WIN32
    // Parsed fields contain only safe ASCII, so the canonical stored form needs no extra escaping.
    std::string json = "{\"schema_version\":1,\"site_url\":\"" + connection.site_url +
        "\",\"upload_token\":\"" + connection.upload_token + "\"}";
    erase_secret(connection.upload_token);
    DATA_BLOB input{static_cast<DWORD>(json.size()), reinterpret_cast<BYTE*>(json.data())};
    DATA_BLOB encrypted{}, entropy = connection_entropy();
    const bool protected_ok = CryptProtectData(&input, L"TenRiff Sites connection", &entropy,
        nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted) != FALSE;
    erase_secret(json);
    if (!protected_ok) { error = "Could not protect the Sites connection."; return false; }
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) { LocalFree(encrypted.pbData); error = "Could not create the connection directory."; return false; }
    const auto destination = directory / "sites-leaderboard.dpapi";
    auto temporary = destination; temporary += ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(encrypted.pbData), encrypted.cbData);
    LocalFree(encrypted.pbData); output.close();
    if (!output || !MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary, ec);
        error = "Could not save the protected connection; the previous connection was kept."; return false;
    }
    std::filesystem::remove(directory / "sites-leaderboard.json", ec);
    if (ec) { error = "Connection saved, but the old plaintext configuration could not be removed."; return false; }
    return true;
#else
    erase_secret(connection.upload_token);
    error = "In-game protected connection storage is available on Windows."; return false;
#endif
}
bool clear_sites_leaderboard_connection(const std::filesystem::path& directory, std::string& error) {
    error.clear();
    // Remove the fallback first so deleting the protected file cannot reactivate an older key.
    for (const auto* filename : {"sites-leaderboard.json", "sites-leaderboard.dpapi"}) {
        std::error_code ec;
        std::filesystem::remove(directory / filename, ec);
        if (ec) { error = "Could not remove the saved connection."; return false; }
    }
    return true;
}
}  // namespace tenriff::app
