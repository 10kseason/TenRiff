#include "doctest/doctest.h"
#include <chrono>
#include <fstream>
#include <iterator>
#include <limits>
#include "app/SitesLeaderboardClient.h"
#include "app/ReplayVerifier.h"
#include "config/SimpleJson.h"
using namespace tenriff;
namespace {
gameplay::ReplayFile score_fixture() {
    gameplay::ReplayFile replay;
    replay.ruleset_id = std::string(app::kCanonicalReplayRulesetId);
    replay.chart_sha256 = std::string(64, 'b');
    replay.created_utc = "20260913_000000Z";
    replay.trace.lane_count = 10;
    replay.final_score = 9900;
    replay.stats.total_notes = 100;
    replay.stats.counts.pg = 98;
    replay.stats.counts.gr = 2;
    replay.stats.max_combo = 100;
    replay.stats.detail_score = 496;
    replay.stats.detailed_accuracy_points = 98.7654321;
    replay.stats.detailed_accuracy_weight = 100.0;
    return replay;
}
}  // namespace

TEST_CASE("Sites payload preserves all four native metrics without display rounding") {
    const auto replay = score_fixture();
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", payload, error));
    const auto json = config::parse_json(payload);
    REQUIRE(json.success());
    const auto* object = json.root->as_object();
    REQUIRE(object != nullptr);
    CHECK(object->at("schema_version").as_number() == 1);
    CHECK(object->at("score").as_number() == replay.final_score);
    CHECK(object->at("detail_score").as_number() == replay.stats.detail_score);
    CHECK(object->at("accuracy").as_number() == replay.stats.accuracy_percent());
    CHECK(object->at("detailed_accuracy").as_number() == replay.stats.detailed_accuracy_percent());
    CHECK(object->at("accuracy").as_number() != object->at("detailed_accuracy").as_number());
    CHECK(payload.find("\"detail_score\":496,") != std::string::npos);
}

TEST_CASE("Sites payload rejects malformed detailed metrics instead of uploading a fallback percentage") {
    const double invalid_numbers[] = {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -1.0
    };
    for (const auto bad : invalid_numbers) {
        for (int field = 0; field < 4; ++field) {
            auto replay = score_fixture();
            double* values[] = {&replay.stats.accuracy_points, &replay.stats.accuracy_weight,
                               &replay.stats.detailed_accuracy_points, &replay.stats.detailed_accuracy_weight};
            *values[field] = bad;
            std::string payload = "stale", error;
            CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
            CHECK(payload.empty());
            CHECK_FALSE(error.empty());
        }
    }
    for (const int64_t bad : {-1LL, 1'000'000'001LL}) {
        auto replay = score_fixture();
        replay.stats.detail_score = bad;
        std::string payload, error;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    }
    auto replay = score_fixture();
    replay.stats.counts.pg = std::numeric_limits<int>::max();
    replay.stats.counts.gr = std::numeric_limits<int>::max();
    std::string payload, error;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    replay = score_fixture();
    replay.stats.counts.pg = -1;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    replay = score_fixture();
    replay.stats.detailed_accuracy_points = 101.0;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    replay = score_fixture();
    replay.stats.accuracy_points = 1.0;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
}

TEST_CASE("Sites connection accepts only an HTTPS Site origin and header-safe upload key") {
    app::SitesLeaderboardConnection connection;
    std::string error;
    const std::string token = "tr_" + std::string(64, 'a');
    const auto config = [&](const std::string& origin) {
        return "{\"schema_version\":1,\"site_url\":\"" + origin + "\",\"upload_token\":\"" + token + "\"}";
    };
    CHECK(app::parse_sites_leaderboard_connection(config("https://tenriff.test.chatgpt.site"), connection, error));
    CHECK_FALSE(app::parse_sites_leaderboard_connection(config("http://tenriff.test.chatgpt.site"), connection, error));
    CHECK_FALSE(app::parse_sites_leaderboard_connection(config("https://tenriff.test.chatgpt.site.evil.test"), connection, error));
    CHECK_FALSE(app::parse_sites_leaderboard_connection(config("https://tenriff.test.chatgpt.site/path"), connection, error));
    CHECK_FALSE(app::parse_sites_leaderboard_connection(config("https://user@tenriff.test.chatgpt.site"), connection, error));
    CHECK_FALSE(app::parse_sites_leaderboard_connection("{}", connection, error));
}
TEST_CASE("Sites score payload omits private evidence and rejects assisted or noncanonical play") {
    gameplay::ReplayFile replay;
    replay.ruleset_id = std::string(app::kCanonicalReplayRulesetId);
    replay.chart_sha256 = std::string(64, 'b');
    replay.chart_path = "C:/private/music/song.bms";
    replay.created_utc = "20260913_000000Z";
    replay.trace.lane_count = 10;
    replay.final_score = 950000;
    replay.stats.total_notes = 100;
    replay.stats.counts.pg = 100;
    replay.stats.max_combo = 100;
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Song \"Title\"", "FULLCOMBO", payload, error));
    REQUIRE(config::parse_json(payload).success());
    CHECK(payload.find("2026-09-13T00:00:00Z") != std::string::npos);
    CHECK(payload.find("private") == std::string::npos);
    CHECK(payload.find("online_verified") == std::string::npos);
    CHECK(payload.find("\"key_mode\":\"10K\"") != std::string::npos);
    replay.mode.autoplay_enabled = true;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Song", "CLEAR", payload, error));
    replay.mode.autoplay_enabled = false;
    replay.pause_used = true;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Song", "CLEAR", payload, error));
    replay.pause_used = false;
    replay.ruleset_id = "custom";
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Song", "CLEAR", payload, error));
}

#ifdef _WIN32
TEST_CASE("Sites in-game import protects keys and keeps a valid connection after rejected input") {
    const auto directory = std::filesystem::temp_directory_path() /
        ("tenriff_sites_import_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            for (const auto* file : {"sites-leaderboard.json", "sites-leaderboard.dpapi", "sites-leaderboard.dpapi.tmp"})
                std::filesystem::remove(path / file, ec);
            std::filesystem::remove(path, ec);
        }
    } cleanup{directory};
    std::filesystem::create_directories(directory);
    const std::string first_key = "tr_" + std::string(64, 'a');
    const std::string second_key = "tr_" + std::string(64, 'b');
    const std::string json = "{\"schema_version\":1,\"site_url\":\"https://tenriff.test.chatgpt.site\",\"upload_token\":\"" + first_key + "\"}";
    std::ofstream(directory / "sites-leaderboard.json") << json;
    app::SitesLeaderboardConnection connection;
    std::string error;
    bool missing = false;
    REQUIRE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK_FALSE(missing);
    CHECK(connection.upload_token == first_key);
    REQUIRE(app::install_sites_leaderboard_connection(directory, json, error));
    CHECK_FALSE(std::filesystem::exists(directory / "sites-leaderboard.json"));
    std::ifstream stored(directory / "sites-leaderboard.dpapi", std::ios::binary);
    const std::string encrypted((std::istreambuf_iterator<char>(stored)), {});
    stored.close();
    CHECK(encrypted.find(first_key) == std::string::npos);
    REQUIRE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK(connection.upload_token == first_key);
    CHECK_FALSE(app::install_sites_leaderboard_connection(directory, "{invalid}", error));
    REQUIRE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK(connection.upload_token == first_key);
    REQUIRE(app::install_sites_leaderboard_connection(directory, second_key, error));
    REQUIRE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK(connection.upload_token == second_key);
    CHECK(connection.site_url == app::kSitesLeaderboardUrl);
    REQUIRE(app::clear_sites_leaderboard_connection(directory, error));
    REQUIRE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK(missing);
    CHECK(connection.upload_token.empty());
}
TEST_CASE("Sites damaged protected connection never falls back to an older plaintext key") {
    const auto directory = std::filesystem::temp_directory_path() /
        ("tenriff_sites_invalid_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    std::ofstream(directory / "sites-leaderboard.dpapi", std::ios::binary) << "damaged";
    std::ofstream(directory / "sites-leaderboard.json") <<
        "{\"schema_version\":1,\"site_url\":\"https://test.chatgpt.site\",\"upload_token\":\"tr_" + std::string(64, 'c') + "\"}";
    app::SitesLeaderboardConnection connection;
    std::string error;
    bool missing = false;
    CHECK_FALSE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK_FALSE(error.empty());
    CHECK(connection.upload_token.empty());
    CHECK(app::clear_sites_leaderboard_connection(directory, error));
    std::error_code ec;
    std::filesystem::remove(directory, ec);
}
#endif
