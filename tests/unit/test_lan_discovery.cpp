#include "network/LanDiscovery.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include "doctest/doctest.h"
#include "network/PeerProtocol.h"

namespace {

template <typename Predicate>
bool wait_until(Predicate&& predicate,
                std::chrono::milliseconds timeout = std::chrono::milliseconds(3000)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return predicate();
}

}  // namespace

TEST_CASE("LAN discovery query is versioned and rejects malformed packets") {
    const std::vector<uint8_t> query = tenriff::network::encode_lan_discovery_query();
    CHECK(tenriff::network::is_lan_discovery_query(query));

    std::vector<uint8_t> truncated = query;
    truncated.pop_back();
    CHECK_FALSE(tenriff::network::is_lan_discovery_query(truncated));

    std::vector<uint8_t> incompatible = query;
    incompatible[incompatible.size() - 1u] =
        static_cast<uint8_t>(tenriff::network::kPeerProtocolVersion + 1u);
    CHECK_FALSE(tenriff::network::is_lan_discovery_query(incompatible));
}

TEST_CASE("LAN room advertisement round-trips bounded room metadata") {
    tenriff::network::LanRoomAdvertisement source;
    source.host_name = "루나 LAN 방";
    source.tcp_port = 27300;
    source.player_count = 3;
    source.max_players = tenriff::network::kPeerMaxPlayers;
    source.accepting_players = true;

    const std::vector<uint8_t> packet =
        tenriff::network::encode_lan_room_advertisement(source);
    REQUIRE_FALSE(packet.empty());

    std::string error;
    const auto decoded = tenriff::network::decode_lan_room_advertisement(packet, &error);
    REQUIRE(decoded.has_value());
    CHECK(error.empty());
    CHECK(decoded->host_name == source.host_name);
    CHECK(decoded->tcp_port == source.tcp_port);
    CHECK(decoded->player_count == source.player_count);
    CHECK(decoded->max_players == source.max_players);
    CHECK(decoded->accepting_players);
}

TEST_CASE("LAN room advertisement rejects invalid capacity and truncation") {
    tenriff::network::LanRoomAdvertisement invalid;
    invalid.host_name = "Invalid";
    invalid.tcp_port = 27300;
    invalid.player_count = 9;
    invalid.max_players = tenriff::network::kPeerMaxPlayers;
    CHECK(tenriff::network::encode_lan_room_advertisement(invalid).empty());

    invalid.player_count = 1;
    std::vector<uint8_t> packet = tenriff::network::encode_lan_room_advertisement(invalid);
    REQUIRE_FALSE(packet.empty());
    packet.pop_back();
    CHECK_FALSE(tenriff::network::decode_lan_room_advertisement(packet).has_value());
}

TEST_CASE("LAN discovery finds a loopback host without manual IP entry") {
#ifdef _WIN32
    tenriff::network::LanDiscoveryService host(0);
    host.advertise({"Loopback Host", 31415, 1, tenriff::network::kPeerMaxPlayers, true});
    REQUIRE(wait_until([&] {
        const auto snapshot = host.snapshot();
        return snapshot.advertising && snapshot.discovery_port != 0;
    }));

    const uint16_t discovery_port = host.snapshot().discovery_port;
    tenriff::network::LanDiscoveryService browser(discovery_port);
    browser.start_browsing();
    REQUIRE(wait_until([&] { return !browser.snapshot().rooms.empty(); }));

    const auto rooms = browser.snapshot().rooms;
    const auto room = std::find_if(rooms.begin(), rooms.end(), [](const auto& candidate) {
        return candidate.host_name == "Loopback Host" && candidate.tcp_port == 31415;
    });
    REQUIRE(room != rooms.end());
    CHECK_FALSE(room->address.empty());
    CHECK(room->player_count == 1);
    CHECK(room->accepting_players);
#else
    MESSAGE("LAN discovery runtime is Windows-only; packet validation remains cross-platform.");
#endif
}
