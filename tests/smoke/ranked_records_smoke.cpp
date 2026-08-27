#include "app/RankedRecordsClient.h"
#include "config/SimpleJson.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc != 5 && argc != 6) {
        std::cerr << "Usage: ranked_records_smoke <base-url> <profile-dir> "
                     "<username-prefix> <chart-sha256> [replay-path]\n";
        return 2;
    }
    const auto profile_directory = std::filesystem::u8path(argv[2]);
    tenriff::app::RankedPlayAuthorization authorization;
    std::string error;
    if (!tenriff::app::prepare_ranked_play(
            argv[1], profile_directory, argv[3], argv[4],
            authorization, error)) {
        std::cerr << error << '\n';
        return 1;
    }

    if (argc == 6) {
        std::ifstream input(std::filesystem::u8path(argv[5]), std::ios::binary);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        if (!input) {
            std::cerr << "Could not read replay fixture.\n";
            return 1;
        }
        auto parsed = tenriff::config::parse_json(buffer.str());
        auto* root = parsed.root
                         ? std::get_if<tenriff::config::JsonObject>(&parsed.root->value)
                         : nullptr;
        if (!root) {
            std::cerr << "Could not parse replay fixture: " << parsed.error << '\n';
            return 1;
        }
        root->insert_or_assign(
            "server_challenge_id", tenriff::config::JsonValue(authorization.challenge_id));
        root->insert_or_assign(
            "server_challenge_nonce", tenriff::config::JsonValue(authorization.challenge_nonce));

        std::error_code filesystem_error;
        std::filesystem::create_directories(profile_directory, filesystem_error);
        if (filesystem_error) {
            std::cerr << "Could not create smoke profile directory.\n";
            return 1;
        }
        const auto replay_copy = profile_directory / "challenge-bound-replay.json";
        std::ofstream output(replay_copy, std::ios::binary | std::ios::trunc);
        output << tenriff::config::json_stringify(*parsed.root, 2) << '\n';
        if (!output) {
            std::cerr << "Could not write replay fixture copy.\n";
            return 1;
        }
        output.close();

        std::string receipt;
        if (!tenriff::app::submit_ranked_replay(
                argv[1], authorization, replay_copy, receipt, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        std::cout << "username=" << authorization.username
                  << " replay_verified=yes receipt_bytes=" << receipt.size() << '\n';
        return 0;
    }

    std::cout << "username=" << authorization.username
              << " challenge=" << authorization.challenge_id << '\n';
    return 0;
}
