#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace tenriff::app {
inline constexpr char kSitesLeaderboardUrl[] = "https://tenriff-leaderboard.lastestarcorp.chatgpt.site";
struct SitesLeaderboardConnection {
    std::string site_url;
    std::string upload_token;
};
[[nodiscard]] bool parse_sites_leaderboard_connection(std::string_view json,
    SitesLeaderboardConnection& connection, std::string& error);
// These operations never include key material in errors. A missing connection is not an error.
[[nodiscard]] bool load_sites_leaderboard_connection(const std::filesystem::path& config_directory,
    SitesLeaderboardConnection& connection, bool& missing, std::string& error);
[[nodiscard]] bool install_sites_leaderboard_connection(const std::filesystem::path& config_directory,
    std::string_view clipboard_or_file_contents, std::string& error);
[[nodiscard]] bool clear_sites_leaderboard_connection(const std::filesystem::path& config_directory,
    std::string& error);
}  // namespace tenriff::app
