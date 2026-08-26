#include "doctest/doctest.h"

#include <string>

#include "app/OnlineRecordsClient.h"

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
