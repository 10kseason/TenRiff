#include "doctest/doctest.h"

#include "network/PeerProtocol.h"

#include <filesystem>
#include <fstream>

namespace {

using tenriff::network::PeerDecodeStatus;
using tenriff::network::PeerMessage;
using tenriff::network::PeerMessageType;

PeerMessage round_trip(const PeerMessage& input) {
    std::string error;
    const std::vector<uint8_t> frame = tenriff::network::encode_peer_message(input, &error);
    REQUIRE(error.empty());
    REQUIRE_FALSE(frame.empty());

    PeerMessage output;
    std::size_t consumed = 0;
    CHECK(tenriff::network::decode_peer_message(frame, output, consumed, error) == PeerDecodeStatus::Complete);
    CHECK(error.empty());
    CHECK(consumed == frame.size());
    return output;
}

}  // namespace

TEST_CASE("peer protocol round-trips lobby messages") {
    PeerMessage hello;
    hello.type = PeerMessageType::Hello;
    hello.text = "Player One";
    CHECK(round_trip(hello).text == "Player One");

    PeerMessage chart;
    chart.type = PeerMessageType::Chart;
    chart.chart_hash = 0x123456789abcdef0ull;
    chart.chart_size = 4567;
    chart.text = "Test Chart";
    const PeerMessage chart_result = round_trip(chart);
    CHECK(chart_result.chart_hash == chart.chart_hash);
    CHECK(chart_result.chart_size == chart.chart_size);
    CHECK(chart_result.text == chart.text);

    PeerMessage ready;
    ready.type = PeerMessageType::Ready;
    ready.ready = true;
    CHECK(round_trip(ready).ready);

    PeerMessage begin;
    begin.type = PeerMessageType::Begin;
    begin.nonce = 42;
    begin.delay_ms = 1500;
    const PeerMessage begin_result = round_trip(begin);
    CHECK(begin_result.nonce == 42);
    CHECK(begin_result.delay_ms == 1500);

    PeerMessage launch;
    launch.type = PeerMessageType::Launch;
    launch.chart_hash = chart.chart_hash;
    launch.nonce = 42;
    const PeerMessage launch_result = round_trip(launch);
    CHECK(launch_result.chart_hash == chart.chart_hash);
    CHECK(launch_result.nonce == 42);

    PeerMessage loaded;
    loaded.type = PeerMessageType::Loaded;
    loaded.nonce = 42;
    CHECK(round_trip(loaded).nonce == 42);

    PeerMessage round_reset;
    round_reset.type = PeerMessageType::RoundReset;
    round_reset.nonce = 77;
    CHECK(round_trip(round_reset).nonce == 77);

    PeerMessage round_cancel;
    round_cancel.type = PeerMessageType::RoundCancel;
    round_cancel.nonce = 78;
    CHECK(round_trip(round_cancel).nonce == 78);

    PeerMessage round_cancel_ack;
    round_cancel_ack.type = PeerMessageType::RoundCancelAck;
    round_cancel_ack.nonce = 78;
    CHECK(round_trip(round_cancel_ack).nonce == 78);
}

TEST_CASE("peer protocol round-trips live score state") {
    PeerMessage score;
    score.type = PeerMessageType::FinalScore;
    score.nonce = 99;
    score.score.score = 987654;
    score.score.current_sample = 1234567;
    score.score.combo = 321;
    score.score.max_combo = 654;
    score.score.perfect = 600;
    score.score.great = 40;
    score.score.good = 10;
    score.score.bad = 3;
    score.score.poor = 1;
    score.score.gauge_milli = 76543;
    score.score.game_over = false;
    score.score.aborted = true;

    const PeerMessage result = round_trip(score);
    CHECK(result.nonce == 99);
    CHECK(result.score.score == 987654);
    CHECK(result.score.current_sample == 1234567);
    CHECK(result.score.combo == 321);
    CHECK(result.score.max_combo == 654);
    CHECK(result.score.perfect == 600);
    CHECK(result.score.great == 40);
    CHECK(result.score.good == 10);
    CHECK(result.score.bad == 3);
    CHECK(result.score.poor == 1);
    CHECK(result.score.gauge_milli == 76543);
    CHECK(result.score.finished);
    CHECK(result.score.aborted);
}

TEST_CASE("peer protocol waits for a complete frame and rejects a bad header") {
    PeerMessage ping;
    ping.type = PeerMessageType::Ping;
    ping.nonce = 42;
    std::vector<uint8_t> frame = tenriff::network::encode_peer_message(ping);
    REQUIRE(frame.size() > tenriff::network::kPeerFrameHeaderSize);

    std::vector<uint8_t> partial(frame.begin(), frame.end() - 1);
    PeerMessage output;
    std::size_t consumed = 99;
    std::string error;
    CHECK(tenriff::network::decode_peer_message(partial, output, consumed, error) == PeerDecodeStatus::Incomplete);
    CHECK(consumed == 0);
    CHECK(error.empty());

    frame[0] ^= 0xffu;
    CHECK(tenriff::network::decode_peer_message(frame, output, consumed, error) == PeerDecodeStatus::Error);
    CHECK_FALSE(error.empty());
}

TEST_CASE("chart fingerprint uses exact file bytes") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "tenriff_peer_protocol_test";
    std::error_code ec;
    fs::create_directories(root, ec);
    REQUIRE_FALSE(ec);
    const fs::path chart = root / "chart.bms";

    {
        std::ofstream out(chart, std::ios::binary | std::ios::trunc);
        out << "#TITLE test\n#00111:0100\n";
    }
    std::string error;
    const auto first = tenriff::network::fingerprint_chart_file(chart.u8string(), &error);
    REQUIRE(error.empty());
    REQUIRE(first.valid());
    CHECK(first.size > 0);
    CHECK(tenriff::network::fingerprint_chart_file(chart.u8string()).hash == first.hash);

    {
        std::ofstream out(chart, std::ios::binary | std::ios::app);
        out << "#00211:0001\n";
    }
    const auto changed = tenriff::network::fingerprint_chart_file(chart.u8string(), &error);
    CHECK(error.empty());
    CHECK(changed.hash != first.hash);
    CHECK(changed.size > first.size);

    fs::remove_all(root, ec);
}
