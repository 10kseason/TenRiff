#include "doctest/doctest.h"

#include <string>
#include <set>

#include "app/OnlineRecordsClient.h"
#include "config/SimpleJson.h"

namespace {
std::string sites_timing_board_fixture(const tenriff::config::JsonValue* profile = nullptr) {
    using namespace tenriff::config;
    JsonObject conditions;
    conditions.emplace("ruleset", JsonValue{"tenriff-native-score-v2-ruleset-3"});
    conditions.emplace("gauge", JsonValue{"normal"});
    conditions.emplace("random", JsonValue{"off"});
    conditions.emplace("mods", JsonValue{JsonArray{JsonValue{"judge_easy"}}});
    if (profile) conditions.emplace("timing_profile", *profile);
    JsonObject board;
    board.emplace("id", JsonValue{std::string(64, 'a')});
    board.emplace("title", JsonValue{"Fixture"});
    board.emplace("chart_sha256", JsonValue{std::string(64, 'b')});
    board.emplace("key_mode", JsonValue{"7K"});
    board.emplace("rate_milli", JsonValue{1000.0});
    board.emplace("conditions", JsonValue{json_stringify(JsonValue{conditions})});
    JsonObject response;
    response.emplace("boards", JsonValue{JsonArray{JsonValue{board}}});
    return json_stringify(JsonValue{response});
}
}  // namespace

TEST_CASE("Sites board labels distinguish known timing profiles without claiming modified windows") {
    using namespace tenriff;
    const char* profiles[] = {"bms-easy", "bms-normal", "bms-hard", "bms-veryhard", "osu-fixed"};
    const char* labels[] = {"BMS EASY", "BMS NORMAL", "BMS HARD", "BMS VERY HARD", "osu!mania"};
    std::set<std::string> distinct;
    for (int i = 0; i < 5; ++i) {
        const config::JsonValue profile{profiles[i]};
        std::vector<app::SitesRecordBoard> boards;
        std::string error;
        REQUIRE(app::parse_sites_record_boards(sites_timing_board_fixture(&profile), "Fixture", boards, error));
        REQUIRE(boards.size() == 1);
        CHECK(boards[0].conditions_label.find(std::string("RULESET 3 / ") + labels[i]) != std::string::npos);
        CHECK(boards[0].conditions_label.find("judge_easy") != std::string::npos);
        CHECK(boards[0].conditions_label.find("ms") == std::string::npos);
        distinct.insert(boards[0].conditions_label);
    }
    CHECK(distinct.size() == 5);
}

TEST_CASE("Sites board timing metadata allows old and future profiles but rejects malformed values") {
    using namespace tenriff;
    std::vector<app::SitesRecordBoard> boards;
    std::string error;
    REQUIRE(app::parse_sites_record_boards(sites_timing_board_fixture(), "Fixture", boards, error));
    REQUIRE(boards.size() == 1);
    CHECK(boards[0].conditions_label.find("TIMING UNKNOWN") != std::string::npos);
    const config::JsonValue future{"future-timing-v2"};
    REQUIRE(app::parse_sites_record_boards(sites_timing_board_fixture(&future), "Fixture", boards, error));
    REQUIRE(boards.size() == 1);
    CHECK(boards[0].conditions_label.find("TIMING UNKNOWN") != std::string::npos);
    for (const auto& invalid : {config::JsonValue{}, config::JsonValue{3.0}, config::JsonValue{true},
                               config::JsonValue{""}, config::JsonValue{"bms-easy\n"},
                               config::JsonValue{"bms-easy / forged"}, config::JsonValue{std::string(65, 'a')}}) {
        CHECK_FALSE(app::parse_sites_record_boards(sites_timing_board_fixture(&invalid), "Fixture", boards, error));
        CHECK(boards.empty());
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("Sites boards preserve conditions and reject substring chart matches") {
    using namespace tenriff::app;
    std::vector<SitesRecordBoard> boards;
    std::string error;
    const auto json = std::string("{\"boards\":[{\"id\":\"") + std::string(64, 'a') +
        "\",\"title\":\"Fixture\",\"chart_sha256\":\"" + std::string(64, 'b') +
        "\",\"key_mode\":\"5K\",\"rate_milli\":1000,\"conditions\":\"{\\\"ruleset\\\":\\\"tenriff-native-v2\\\",\\\"gauge\\\":\\\"normal\\\",\\\"random\\\":\\\"off\\\",\\\"mods\\\":[]}\"}]}";
    REQUIRE(parse_sites_record_boards(json, "Fixture", boards, error));
    REQUIRE(boards.size() == 1);
    CHECK(boards[0].conditions_label.find("5K / 1.00x") != std::string::npos);
    CHECK(boards[0].ruleset_id == "tenriff-native-v2");
    REQUIRE(parse_sites_record_boards(json, "Fixt", boards, error));
    CHECK(boards.empty());
    CHECK_FALSE(parse_sites_record_boards("{\"boards\":[{}]}", "Fixture", boards, error));
    CHECK(boards.empty());
}

TEST_CASE("Sites rankings accept ties and nullable detail without claiming replay verification") {
    using namespace tenriff::app;
    SitesRecordBoard board;
    board.id = std::string(64, 'a');
    board.ruleset_id = "tenriff-native-v2";
    std::string error;
    std::vector<OnlineRecordEntry> records;
    const std::string row = "{\"rank\":1,\"nickname\":\"Fixture\",\"score\":9000,\"detail_score\":null,\"detailed_accuracy\":null,\"accuracy\":99.5,\"max_combo\":200,\"clear_status\":\"CLEAR\",\"played_at\":\"2026-10-02T00:00:00Z\"}";
    const auto json = "{\"board\":{\"id\":\"" + board.id + "\"},\"rankings\":[" + row + "," + row + "]}";
    REQUIRE(parse_sites_record_rankings(json, board, records, error));
    REQUIRE(records.size() == 2);
    CHECK(records[1].rank == 1);
    CHECK(records[0].verification_status == "sites_community");
    CHECK(records[0].detailed_accuracy == doctest::Approx(99.5));
    board.id[0] = 'b';
    CHECK_FALSE(parse_sites_record_rankings(json, board, records, error));
    CHECK(records.empty());
    board.id[0] = 'a';
    auto malformed = json;
    malformed.replace(malformed.find("99.5"), 4, "101");
    CHECK_FALSE(parse_sites_record_rankings(malformed, board, records, error));
    CHECK(records.empty());
}

TEST_CASE("online records parser accepts verified schema-v1 records") {
    const std::string hash(64, 'a');
    const std::string json =
        "{\"schema_version\":1,\"chart_sha256\":\"" + hash +
        "\",\"records\":[{\"rank\":1,\"player_name\":\"ryui\","
        "\"score\":987654,\"accuracy\":98.7654,\"max_combo\":1234,"
        "\"clear_status\":\"FULLCOMBO\",\"ruleset_id\":\"ranked-v1\","
        "\"verification_status\":\"online_verified\","
        "\"verified_at_utc\":\"2026-08-26T00:00:00Z\"}]}";
    tenriff::app::OnlineRecordsResponse response;
    std::string error;
    REQUIRE(tenriff::app::parse_online_records_response(
        json, std::string(64, 'A'), response, error));
    CHECK(error.empty());
    REQUIRE(response.records.size() == 1);
    CHECK(response.records[0].rank == 1);
    CHECK(response.records[0].player_name == "ryui");
    CHECK(response.records[0].score == 987654);
    CHECK(response.records[0].accuracy == doctest::Approx(98.7654));
    CHECK(response.records[0].verification_status == "online_verified");
}

TEST_CASE("online records parser rejects mismatched charts and client claims") {
    const std::string hash(64, 'b');
    tenriff::app::OnlineRecordsResponse response;
    std::string error;
    CHECK_FALSE(tenriff::app::parse_online_records_response(
        "{\"schema_version\":1,\"chart_sha256\":\"" +
            std::string(64, 'c') + "\",\"records\":[]}",
        hash, response, error));
    CHECK_FALSE(error.empty());

    const std::string claim =
        "{\"schema_version\":1,\"chart_sha256\":\"" + hash +
        "\",\"records\":[{\"rank\":1,\"player_name\":\"claim\","
        "\"score\":1,\"accuracy\":1.0,\"max_combo\":1,"
        "\"clear_status\":\"CLEAR\",\"ruleset_id\":\"ranked-v1\","
        "\"verification_status\":\"client_claim\","
        "\"verified_at_utc\":\"2026-08-26T00:00:00Z\"}]}";
    CHECK_FALSE(tenriff::app::parse_online_records_response(
        claim, hash, response, error));
    CHECK(error.find("unverified") != std::string::npos);
}
