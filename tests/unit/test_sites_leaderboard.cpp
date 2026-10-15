#include "doctest/doctest.h"
#include <chrono>
#include <fstream>
#include <iterator>
#include <iostream>
#include <limits>
#include "app/SitesLeaderboardClient.h"
#include "app/SitesUploadResponse.h"
#include "app/ReplayVerifier.h"
#include "config/SimpleJson.h"
using namespace tenriff;
TEST_CASE("Sites upload failures distinguish compatibility authentication and retry without reflecting remote bodies") {
    const std::string untrusted = R"({"error":"private-response-marker","accepted":true})";
    struct Case { unsigned status; bool retry; const char* reason; };
    for (const auto test : {Case{400, false, "ruleset"}, Case{401, false, "connection key"},
                           Case{403, false, "Access"}, Case{404, false, "endpoint"},
                           Case{429, true, "rate limited"}, Case{503, true, "temporarily unavailable"},
                           Case{302, false, "did not accept"}}) {
        bool retryable = !test.retry;
        std::string error = "previous error";
        CHECK_FALSE(app::parse_sites_upload_response(test.status, untrusted, retryable, error));
        CHECK(retryable == test.retry);
        CHECK(error.find(std::to_string(test.status)) != std::string::npos);
        CHECK(error.find(test.reason) != std::string::npos);
        CHECK(error.find("private-response-marker") == std::string::npos);
        if (test.status == 400) CHECK(error.find("connection key") == std::string::npos);
    }
}

TEST_CASE("Sites accepts only acknowledged 200 and 201 responses and clears stale retry state") {
    for (const unsigned status : {200u, 201u}) {
        bool retryable = true;
        std::string error = "previous error";
        CHECK(app::parse_sites_upload_response(status, R"({"accepted":true,"duplicate":true})", retryable, error));
        CHECK_FALSE(retryable);
        CHECK(error.empty());
        for (const auto body : {"invalid", "{}", "[]", R"({"accepted":false})", R"({"accepted":"true"})"}) {
            CHECK_FALSE(app::parse_sites_upload_response(status, body, retryable, error));
            CHECK_FALSE(retryable);
            CHECK(error == "Sites did not acknowledge the record.");
        }
    }
}

TEST_CASE("Sites future clock errors use only the allowlisted code and never reflect server text") {
    bool retryable = true;
    std::string error;
    REQUIRE_FALSE(app::parse_sites_upload_response(400,
        R"({"error_code":"future_play_time","error":"private-response-marker"})", retryable, error));
    CHECK_FALSE(retryable);
    CHECK(error.find("PC clock") != std::string::npos);
    CHECK(error.find("private-response-marker") == std::string::npos);
    REQUIRE_FALSE(app::parse_sites_upload_response(400,
        R"({"error_code":"private-response-marker","error":"future_play_time"})", retryable, error));
    CHECK(error.find("PC clock") == std::string::npos);
    CHECK(error.find("private-response-marker") == std::string::npos);
}

TEST_CASE("Sites late asynchronous messages cannot replace a different result status") {
    const app::SitesUploadMessage completed{std::string(64, 'a'), "Sites community record uploaded."};
    CHECK(completed.applies_to_result(std::string(64, 'a')));
    CHECK_FALSE(completed.applies_to_result(std::string(64, 'b')));
    CHECK_FALSE(completed.applies_to_result(""));
    CHECK_FALSE(app::SitesUploadMessage{}.applies_to_result(""));
    CHECK(app::sites_upload_status_text(completed.text).korean == "웹 기록 등록 완료");
    for (const unsigned status : {400u, 401u, 403u, 404u, 429u}) {
        bool retryable = false;
        std::string error;
        REQUIRE_FALSE(app::parse_sites_upload_response(status, R"({"error":"private-response-marker"})", retryable, error));
        const auto summary = app::sites_upload_status_text(error);
        CHECK(summary.english.find(std::to_string(status)) != std::string_view::npos);
        CHECK(summary.korean.find(std::to_string(status)) != std::string_view::npos);
        CHECK(summary.english.find("private-response-marker") == std::string_view::npos);
    }
}

namespace {
bool json_values_equal(const config::JsonValue& lhs, const config::JsonValue& rhs) {
    if (lhs.value.index() != rhs.value.index()) return false;
    if (lhs.is_object()) {
        const auto& left = *lhs.as_object();
        const auto& right = *rhs.as_object();
        if (left.size() != right.size()) return false;
        for (const auto& field : left) {
            const auto found = right.find(field.first);
            if (found == right.end() || !json_values_equal(field.second, found->second)) return false;
        }
        return true;
    }
    if (lhs.is_array()) {
        const auto& left = *lhs.as_array();
        const auto& right = *rhs.as_array();
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i)
            if (!json_values_equal(left[i], right[i])) return false;
        return true;
    }
    if (lhs.is_string()) return lhs.as_string() == rhs.as_string();
    if (lhs.is_number()) return lhs.as_number() == rhs.as_number();
    if (lhs.is_bool()) return lhs.as_bool() == rhs.as_bool();
    return lhs.is_null();
}

gameplay::ReplayFile score_fixture() {
    gameplay::ReplayFile replay;
    replay.ruleset_id = std::string(app::kCanonicalReplayRulesetId);
    replay.chart_format = "bms";
    replay.bms_rank = 3;
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
    CHECK(object->at("ruleset_id").as_string() == "tenriff-native-score-v2-ruleset-5");
    CHECK(object->at("score").as_number() == replay.final_score);
    CHECK(object->at("detail_score").as_number() == replay.stats.detail_score);
    CHECK(object->at("accuracy").as_number() == replay.stats.accuracy_percent());
    CHECK(object->at("detailed_accuracy").as_number() == replay.stats.detailed_accuracy_percent());
    CHECK(object->at("accuracy").as_number() != object->at("detailed_accuracy").as_number());
    CHECK(payload.find("\"detail_score\":496,") != std::string::npos);
    CHECK(object->at("timing_profile").as_string() == "bms-easy");
}

TEST_CASE("1.8.9.4 Sites optional chart author adds metadata without changing replay or score identity") {
    const auto replay = score_fixture();
    std::string old_payload, payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", old_payload, error));
    const std::string author = u8"譜面作者 \"패터너\"";
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", payload, error, author));
    // Public synthetic fixture for native/web payload-contract checks.
    std::cout << "[chart-author-payload] " << payload << '\n';
    const auto old_json = config::parse_json(old_payload);
    const auto new_json = config::parse_json(payload);
    REQUIRE(old_json.success());
    REQUIRE(new_json.success());
    const auto& old_fields = *old_json.root->as_object();
    auto new_fields = *new_json.root->as_object();
    REQUIRE(new_fields.count("chart_author") == 1);
    CHECK(new_fields.at("chart_author").as_string() == author);
    CHECK(old_fields.count("chart_author") == 0);
    new_fields.erase("chart_author");
    // Object iteration order is not a wire contract; every existing field's
    // type and exact value (including binary64 metrics and array order) is.
    CHECK(json_values_equal(config::JsonValue{new_fields}, config::JsonValue{old_fields}));
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", payload, error, ""));
    CHECK(payload == old_payload);
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", payload, error, std::string(480, 'a')));
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", payload, error, std::string(481, 'a')));
    CHECK(payload.empty());
    CHECK_FALSE(error.empty());
}

TEST_CASE("Sites payload preserves active judge modifiers without relabeling normal records") {
    for (const std::string mod : {"", "judge_easy", "judge_hard"}) {
        auto replay = score_fixture();
        if (!mod.empty()) replay.mods = {mod};
        std::string payload, error;
        REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
        const auto json = config::parse_json(payload);
        REQUIRE(json.success());
        const auto* root = json.root->as_object();
        REQUIRE(root);
        const auto* mods = root->at("mods").as_array();
        REQUIRE(mods);
        CHECK(mods->size() == (mod.empty() ? 0u : 1u));
        if (!mod.empty()) CHECK(mods->front().as_string() == mod);
        CHECK(root->at("ruleset_id").as_string() == app::kCanonicalReplayRulesetId);
        CHECK(root->at("timing_profile").as_string() == "bms-easy");
    }
}

TEST_CASE("Sites keeps musical LN Mix v2 out of unversioned legacy boards") {
    auto replay = score_fixture();
    replay.mods = {"ln_mix_30"};
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    for (int version : {0, 2, 3}) {
        replay.mode.long_note_mix_version = version;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
        CHECK(payload.empty());
        CHECK(error.find("separate leaderboard condition") != std::string::npos);
    }
    replay.mods.clear();
    replay.mode.long_note_mix_version = 2;
    CHECK(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
}

TEST_CASE("Sites keeps corrected BMS timing out of historical timing boards") {
    auto replay = score_fixture();
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    replay.mode.bms_timing_version = 2;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    CHECK(payload.empty());
    CHECK(error.find("BMS timing v2") != std::string::npos);
}

TEST_CASE("Sites keeps TOTAL and authored branch policies out of legacy boards") {
    for (int policy = 0; policy < 3; ++policy) {
        auto replay = score_fixture();
        if (policy == 0) replay.mode.bms_gauge_version = 2;
        if (policy == 1) replay.mode.bms_control_flow_version = 2;
        if (policy == 2) replay.mode.bms_random_seed = 0u;
        std::string payload, error;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
        CHECK(payload.empty());
        CHECK(error.find("separate leaderboard condition") != std::string::npos);
    }
}

TEST_CASE("Sites EX payload requires its own R6 policy and excludes mixed judge modes") {
    auto replay = score_fixture();
    replay.ruleset_id = std::string(app::kExReplayRulesetId);
    replay.mods = {"judge_ex"};
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "EX Fixture", "CLEAR", payload, error));
    const auto json = config::parse_json(payload);
    REQUIRE(json.success());
    const auto* root = json.root->as_object();
    REQUIRE(root);
    CHECK(root->at("ruleset_id").as_string() == app::kExReplayRulesetId);
    CHECK(root->at("mods").as_array()->front().as_string() == "judge_ex");
    CHECK(root->at("timing_profile").as_string() == "bms-easy");
    // Synthetic public fixture: exercise the native serializer in web schema QA.
    std::cout << "[judge-ex-payload] " << payload << '\n';
    replay.ruleset_id = std::string(app::kCanonicalReplayRulesetId);
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "EX Fixture", "CLEAR", payload, error));
    replay.ruleset_id = std::string(app::kExReplayRulesetId);
    for (const std::vector<std::string> mods : {std::vector<std::string>{},
            std::vector<std::string>{"judge_hard"}, std::vector<std::string>{"judge_ex", "judge_hard"},
            std::vector<std::string>{"judge_ex", "judge_easy"}}) {
        replay.mods = mods;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "EX Fixture", "CLEAR", payload, error));
        CHECK(payload.empty());
    }
}

TEST_CASE("Sites ruleset 4 payload separates every BMS rank and fixed osu timing") {
    const char* profiles[] = {"bms-veryhard", "bms-hard", "bms-normal", "bms-easy"};
    for (int rank = 0; rank <= 3; ++rank) {
        auto replay = score_fixture();
        replay.bms_rank = rank;
        std::string payload, error;
        REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
        const auto json = config::parse_json(payload);
        REQUIRE(json.success());
        REQUIRE(json.root->as_object());
        CHECK(json.root->as_object()->at("timing_profile").as_string() == profiles[rank]);
    }
    auto replay = score_fixture();
    replay.chart_format = "osu";
    replay.bms_rank.reset();
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    const auto json = config::parse_json(payload);
    REQUIRE(json.success());
    CHECK(json.root->as_object()->at("timing_profile").as_string() == "osu-fixed");
}

TEST_CASE("Sites ruleset 4 payload fails closed on missing or invalid timing metadata") {
    const auto rejects = [](const gameplay::ReplayFile& replay) {
        std::string payload = "stale payload", error;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
        CHECK(payload.empty());
        CHECK_FALSE(error.empty());
    };
    auto replay = score_fixture();
    replay.bms_rank.reset();
    rejects(replay);
    for (const int rank : {-1, 4, (std::numeric_limits<int>::max)()}) {
        replay = score_fixture();
        replay.bms_rank = rank;
        rejects(replay);
    }
    for (const std::string format : {"", "unknown", "BMS", "bme"}) {
        replay = score_fixture();
        replay.chart_format = format;
        rejects(replay);
    }
    replay = score_fixture();
    replay.chart_format = "osu";
    replay.bms_rank = -1;
    rejects(replay);
    for (const auto ruleset : {app::kLegacyReplayRulesetId, app::kPreviousReplayRulesetId, app::kRuleset3ReplayRulesetId, app::kRuleset4ReplayRulesetId}) {
        replay = score_fixture();
        replay.ruleset_id = std::string(ruleset);
        rejects(replay);
    }
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
    replay.chart_format = "bms";
    replay.bms_rank = 3;
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

TEST_CASE("Sites rejects Auto Scratch even if canonical ruleset metadata is forged") {
    auto replay = score_fixture();
    replay.mods = {"auto_scratch"};
    std::string payload, error;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    CHECK(payload.empty());
}
