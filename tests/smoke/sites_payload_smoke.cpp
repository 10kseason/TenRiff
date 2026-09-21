#include <iostream>
#include "app/MenuRecordUtils.h"
#include "app/ChartFileHash.h"
#include "app/RankedRecordsClient.h"
#include "app/SitesLeaderboardClient.h"
#include "app/ReplayVerifier.h"
int main(int argc, char** argv) {
    if (argc == 4 && (std::string(argv[1]) == "--inspect" || std::string(argv[1]) == "--retry")) {
        std::string error, payload;
        const auto result = tenriff::app::menu_records::parse_result_file(std::filesystem::u8path(argv[2]), &error);
        if (!result) { std::cerr << "Result parse failed: " << error; return 1; }
        const auto path = std::filesystem::u8path(result->replay_path);
        const auto replay = tenriff::gameplay::load_replay_json(path.u8string());
        if (!replay.success()) { std::cerr << "Replay parse failed: " << replay.error; return 1; }
        const auto hashes = tenriff::app::hash_chart_file(path, &error);
        if (!hashes.valid() || hashes.sha256 != result->replay_sha256) { std::cerr << "Replay hash mismatch"; return 1; }
        if (!tenriff::app::build_sites_score_json(*replay.replay, hashes.sha256, argv[3], result->clear_status, payload, error)) {
            std::cerr << error; return 1;
        }
        std::cout << "Native payload validated; score=" << replay.replay->final_score << "\n";
        tenriff::app::SitesLeaderboardConnection connection;
        bool missing = false;
        if (!tenriff::app::load_sites_leaderboard_connection("config", connection, missing, error) || missing) {
            std::cerr << "Connection load failed: " << error; return 1;
        }
        std::cout << "Protected connection loaded for " << connection.site_url << "\n";
        if (std::string(argv[1]) == "--inspect") return 0;
        bool retryable = false;
        const bool accepted = tenriff::app::submit_sites_leaderboard_score(connection.site_url, connection.upload_token, payload, retryable, error);
        std::fill(connection.upload_token.begin(), connection.upload_token.end(), '\0');
        if (!accepted) { std::cerr << "Upload failed: " << error << " retryable=" << retryable; return 1; }
        std::cout << "Upload accepted\n";
        return 0;
    }
    tenriff::gameplay::ReplayFile replay;
    replay.ruleset_id = std::string(tenriff::app::kCanonicalReplayRulesetId);
    replay.chart_sha256 = std::string(64, 'a');
    replay.created_utc = "20260901_000000Z";
    replay.trace.lane_count = 10;
    replay.stats.total_notes = 100;
    replay.stats.counts.pg = 98;
    replay.stats.counts.gr = 2;
    replay.stats.max_combo = 100;
    replay.stats.detail_score = 496;
    replay.stats.detailed_accuracy_points = 98.7654321;
    replay.stats.detailed_accuracy_weight = 100.0;
    replay.final_score = 9900;
    replay.mode.gauge = "normal";
    replay.mode.random = "off";
    std::string payload, error;
    if (!tenriff::app::build_sites_score_json(replay, std::string(64, 'b'),
         "QA Fixture - TenRiff", "FULLCOMBO", payload, error)) {
        std::cerr << error; return 1;
    }
    std::cout << payload;
}
