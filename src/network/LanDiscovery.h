#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tenriff::network {

inline constexpr uint16_t kLanDiscoveryPort = 27301;
inline constexpr std::size_t kLanDiscoveryHostNameMaxBytes = 48;

struct LanRoomAdvertisement {
    std::string host_name;
    uint16_t tcp_port = 0;
    uint8_t player_count = 0;
    uint8_t max_players = 0;
    bool accepting_players = false;
};

struct LanDiscoveredRoom : LanRoomAdvertisement {
    std::string address;
};

struct LanDiscoverySnapshot {
    uint64_t revision = 0;
    uint16_t discovery_port = 0;
    bool browsing = false;
    bool advertising = false;
    std::vector<LanDiscoveredRoom> rooms;
    std::string status_detail;
};

[[nodiscard]] std::vector<uint8_t> encode_lan_discovery_query();
[[nodiscard]] bool is_lan_discovery_query(const std::vector<uint8_t>& packet);
[[nodiscard]] std::vector<uint8_t> encode_lan_room_advertisement(
    const LanRoomAdvertisement& advertisement);
[[nodiscard]] std::optional<LanRoomAdvertisement> decode_lan_room_advertisement(
    const std::vector<uint8_t>& packet,
    std::string* error = nullptr);

/// Server-free LAN room discovery. Browsers broadcast a small UDP query and
/// hosts answer directly to the sender; the selected room still joins through
/// the existing PeerSession TCP path. No internet relay or NAT traversal is
/// attempted here.
class LanDiscoveryService {
public:
    explicit LanDiscoveryService(uint16_t discovery_port = kLanDiscoveryPort);
    ~LanDiscoveryService();

    LanDiscoveryService(const LanDiscoveryService&) = delete;
    LanDiscoveryService& operator=(const LanDiscoveryService&) = delete;

    void start_browsing();
    void advertise(LanRoomAdvertisement advertisement);
    void stop();

    [[nodiscard]] LanDiscoverySnapshot snapshot() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tenriff::network
