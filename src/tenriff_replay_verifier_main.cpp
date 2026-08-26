#include "app/ReplayVerifier.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string escape_json(const std::string& value) {
    std::ostringstream output;
    for (const unsigned char byte : value) {
        switch (byte) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (byte < 0x20) {
                    output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<unsigned>(byte) << std::dec;
                } else {
                    output << static_cast<char>(byte);
                }
        }
    }
    return output.str();
}

void usage() {
    std::cerr << "Usage: tenriff-replay-verifier --replay <file> --chart <bms-file> "
                 "[--challenge-id <id> --challenge-nonce <nonce>]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path replay;
    std::filesystem::path chart;
    std::string challenge_id;
    std::string challenge_nonce;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if ((argument == "--replay" || argument == "--chart" ||
             argument == "--challenge-id" || argument == "--challenge-nonce") &&
            index + 1 >= argc) {
            usage();
            return 2;
        }
        if (argument == "--replay") replay = std::filesystem::u8path(argv[++index]);
        else if (argument == "--chart") chart = std::filesystem::u8path(argv[++index]);
        else if (argument == "--challenge-id") challenge_id = argv[++index];
        else if (argument == "--challenge-nonce") challenge_nonce = argv[++index];
        else if (argument == "--help" || argument == "-h") {
            usage();
            return 0;
        } else {
            usage();
            return 2;
        }
    }
    if (replay.empty() || chart.empty()) {
        usage();
        return 2;
    }

    if (!challenge_id.empty() || !challenge_nonce.empty()) {
        const auto loaded = tenriff::gameplay::load_replay_json(replay.u8string());
        if (!loaded.success() || loaded.replay->server_challenge_id != challenge_id ||
            loaded.replay->server_challenge_nonce != challenge_nonce) {
            std::cout << "{\"status\":\"invalid\",\"detail\":\"Replay is not bound to the issued server challenge.\"}\n";
            return 1;
        }
    }

    const tenriff::app::ReplayVerificationResult result =
        tenriff::app::verify_replay_file(replay, chart);
    std::cout << "{\"status\":\""
              << tenriff::app::replay_verification_status_token(result.status)
              << "\",\"detail\":\"" << escape_json(result.detail)
              << "\",\"chart_sha256\":\"" << result.chart_sha256
              << "\",\"replay_sha256\":\"" << result.replay_sha256
              << "\",\"official_eligible\":" << (result.official_eligible ? "true" : "false")
              << ",\"claims_match\":" << (result.claims_match ? "true" : "false")
              << ",\"score\":" << result.final_score
              << ",\"accuracy\":" << std::setprecision(17) << result.stats.accuracy_percent()
              << ",\"max_combo\":" << result.stats.max_combo
              << ",\"clear_status\":\"" << escape_json(result.clear_status)
              << "\",\"ruleset_id\":\"" << tenriff::app::kCanonicalReplayRulesetId
              << "\"}\n";
    return result.verified() && result.official_eligible ? 0 : 1;
}
