#pragma once

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

namespace tenriff::app {

struct RankedAccountSession {
    std::string username;
    std::string role;
    std::string bearer_token;
    std::string expires_at_utc;

    [[nodiscard]] bool valid() const noexcept {
        return !username.empty() && !bearer_token.empty();
    }
};

struct RankedGlobalChatMessage {
    std::int64_t id = 0;
    std::string username;
    std::string role;
    std::string text;
    std::string created_at_utc;
};

struct RankedMultiplayerRoom {
    std::string id;
    std::string name;
    std::string address;
    std::uint16_t tcp_port = 0;
    std::uint8_t player_count = 0;
    std::uint8_t max_players = 8;
    bool accepting_players = false;
    bool round_active = false;
};

[[nodiscard]] bool authenticate_ranked_account(
    const std::string& base_url,
    const std::filesystem::path& profile_directory,
    const std::string& username,
    const std::string& password,
    bool create_account,
    RankedAccountSession& session,
    std::string& error);

[[nodiscard]] bool saved_ranked_account_username(
    const std::filesystem::path& profile_directory,
    std::string& username,
    std::string& error);

[[nodiscard]] bool clear_saved_ranked_account(
    const std::filesystem::path& profile_directory,
    std::string& error);

[[nodiscard]] bool fetch_ranked_global_chat(
    const std::string& base_url,
    const std::string& bearer_token,
    std::int64_t after_id,
    std::vector<RankedGlobalChatMessage>& messages,
    std::string& error);

[[nodiscard]] bool send_ranked_global_chat(
    const std::string& base_url,
    const std::string& bearer_token,
    const std::string& text,
    std::string& error);

[[nodiscard]] bool fetch_ranked_multiplayer_rooms(
    const std::string& base_url,
    const std::string& bearer_token,
    std::vector<RankedMultiplayerRoom>& rooms,
    std::string& error);

struct RankedPlayAuthorization {
    std::string bearer_token;
    std::string challenge_id;
    std::string challenge_nonce;
    std::string username;

    [[nodiscard]] bool valid() const noexcept {
        return !bearer_token.empty() && !challenge_id.empty() &&
               !challenge_nonce.empty();
    }
};

[[nodiscard]] bool prepare_ranked_play(
    const std::string& base_url,
    const std::filesystem::path& profile_directory,
    const std::string& preferred_username,
    const std::string& chart_sha256,
    RankedPlayAuthorization& authorization,
    std::string& error);

[[nodiscard]] bool submit_ranked_replay(
    const std::string& base_url,
    const RankedPlayAuthorization& authorization,
    const std::filesystem::path& replay_path,
    std::string& receipt,
    std::string& error);

}  // namespace tenriff::app
