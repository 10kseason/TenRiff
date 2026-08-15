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

    PeerMessage chat;
    chat.type = PeerMessageType::Chat;
    chat.player_id = 3;
    chat.text = "안녕하세요, room!";
    const PeerMessage chat_result = round_trip(chat);
    CHECK(chat_result.player_id == 3);
    CHECK(chat_result.text == chat.text);

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

TEST_CASE("peer protocol rejects empty and oversized chat frames") {
    std::string error;
    PeerMessage chat;
    chat.type = PeerMessageType::Chat;
    chat.text = "";
    CHECK(tenriff::network::encode_peer_message(chat, &error).empty());
    CHECK_FALSE(error.empty());

    chat.text.assign(tenriff::network::kPeerChatMaxBytes + 1, 'x');
    CHECK(tenriff::network::encode_peer_message(chat, &error).empty());
    CHECK_FALSE(error.empty());
}
TEST_CASE("peer protocol round-trips eight-player room metadata") {
    PeerMessage welcome;
    welcome.type = PeerMessageType::RoomWelcome;
    welcome.player_id = 7;
    welcome.leader_id = 3;
    const PeerMessage welcome_result = round_trip(welcome);
    CHECK(welcome_result.player_id == 7);
    CHECK(welcome_result.leader_id == 3);

    PeerMessage roster;
    roster.type = PeerMessageType::RoomRoster;
    roster.leader_id = 2;
    roster.round_active = true;
    roster.nonce = 1234;
    tenriff::network::PeerParticipantWire leader;
    leader.player_id = 2;
    leader.name = "Leader";
    leader.ready = true;
    leader.loaded = true;
    leader.chart_hash = 0x99887766u;
    leader.chart_size = 4096;
    leader.chart_name = "Shared BMS";
    tenriff::network::PeerParticipantWire waiting;
    waiting.player_id = 5;
    waiting.name = "Waiting";
    waiting.round_reset = true;
    roster.participants = {leader, waiting};

    const PeerMessage result = round_trip(roster);
    CHECK(result.leader_id == 2);
    CHECK(result.round_active);
    CHECK(result.nonce == 1234);
    REQUIRE(result.participants.size() == 2u);
    CHECK(result.participants[0].player_id == 2);
    CHECK(result.participants[0].ready);
    CHECK(result.participants[0].loaded);
    CHECK(result.participants[0].chart_hash == leader.chart_hash);
    CHECK(result.participants[0].chart_name == "Shared BMS");
    CHECK(result.participants[1].player_id == 5);
    CHECK(result.participants[1].round_reset);

    PeerMessage attributed_score;
    attributed_score.type = PeerMessageType::Score;
    attributed_score.player_id = 8;
    attributed_score.nonce = 55;
    attributed_score.score.score = 7654;
    const PeerMessage score_result = round_trip(attributed_score);
    CHECK(score_result.player_id == 8);
    CHECK(score_result.score.score == 7654);
}
TEST_CASE("peer protocol round-trips bounded chart-library messages") {
    PeerMessage begin;
    begin.type = PeerMessageType::LibraryBegin;
    begin.library_count = 2;
    CHECK(round_trip(begin).library_count == 2u);

    PeerMessage chunk;
    chunk.type = PeerMessageType::LibraryChunk;
    chunk.chart_sha256 = {
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789",
    };
    const PeerMessage chunk_result = round_trip(chunk);
    REQUIRE(chunk_result.chart_sha256.size() == 2u);
    CHECK(chunk_result.chart_sha256[0] == chunk.chart_sha256[0]);
    CHECK(chunk_result.chart_sha256[1] ==
          "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");

    PeerMessage end;
    end.type = PeerMessageType::LibraryEnd;
    CHECK(round_trip(end).type == PeerMessageType::LibraryEnd);

    std::string error;
    chunk.chart_sha256 = {"not-a-sha256"};
    CHECK(tenriff::network::encode_peer_message(chunk, &error).empty());
    CHECK_FALSE(error.empty());
}
TEST_CASE("peer protocol round-trips live score state") {
    PeerMessage score;
    score.type = PeerMessageType::FinalScore;
    score.nonce = 99;
    score.score.score = 9876;
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
    CHECK(result.score.score == 9876);
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

TEST_CASE("peer protocol rejects impossible score claims before display") {
    PeerMessage score;
    score.type = PeerMessageType::FinalScore;
    score.nonce = 99;
    score.score.score = tenriff::network::kPeerMaximumClaimedScore + 1;
    std::string error;
    CHECK(tenriff::network::encode_peer_message(score, &error).empty());
    CHECK_FALSE(error.empty());

    score.score.score = 9000;
    score.score.max_combo = 10;
    score.score.combo = 11;
    CHECK(tenriff::network::encode_peer_message(score, &error).empty());
    CHECK_FALSE(error.empty());

    score.score.combo = 10;
    score.score.gauge_milli = 100001;
    CHECK(tenriff::network::encode_peer_message(score, &error).empty());
    CHECK_FALSE(error.empty());
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
