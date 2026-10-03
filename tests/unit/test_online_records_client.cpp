#include "doctest/doctest.h"

#include <string>

#include "app/OnlineRecordsClient.h"

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
