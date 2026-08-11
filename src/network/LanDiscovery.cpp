#include "network/LanDiscovery.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "network/PeerProtocol.h"
#include "util/Utf8Compat.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace tenriff::network {

namespace {

using SteadyClock = std::chrono::steady_clock;

constexpr std::array<uint8_t, 8> kDiscoveryMagic = {
    'T', 'R', 'L', 'A', 'N', '0', '0', '1'};
constexpr uint8_t kPacketQuery = 1;
constexpr uint8_t kPacketRoom = 2;
constexpr uint8_t kRoomAcceptingPlayers = 1u << 0u;
constexpr auto kQueryInterval = std::chrono::milliseconds(750);
constexpr auto kRoomExpiry = std::chrono::seconds(3);
constexpr auto kWorkerPollInterval = std::chrono::milliseconds(20);

void append_u16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
}

bool read_u16(const std::vector<uint8_t>& packet, std::size_t& cursor, uint16_t& out) {
    if (cursor + 2u > packet.size()) return false;
    out = static_cast<uint16_t>(static_cast<uint16_t>(packet[cursor]) << 8u) |
          static_cast<uint16_t>(packet[cursor + 1u]);
    cursor += 2u;
    return true;
}

bool has_header(const std::vector<uint8_t>& packet, uint8_t type) {
    return packet.size() >= kDiscoveryMagic.size() + 3u &&
           std::equal(kDiscoveryMagic.begin(), kDiscoveryMagic.end(), packet.begin()) &&
           packet[kDiscoveryMagic.size()] == type;
}

std::string bounded_host_name(std::string_view source) {
    std::string name = util::sanitize_ui_text(source);
    if (name.empty()) name = "TenRiff Host";
    if (name.size() <= kLanDiscoveryHostNameMaxBytes) return name;

    std::size_t end = kLanDiscoveryHostNameMaxBytes;
    while (end > 0u &&
           (static_cast<unsigned char>(name[end]) & 0xC0u) == 0x80u) {
        --end;
    }
    name.resize(end);
    return name.empty() ? "TenRiff Host" : name;
}

#ifdef _WIN32

std::string winsock_error(std::string_view operation, int error) {
    return std::string(operation) + " failed (Winsock " + std::to_string(error) + ").";
}

class WinsockRuntime {
public:
    bool start(std::string& error) {
        WSADATA data{};
        const int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) {
            error = winsock_error("WSAStartup", result);
            return false;
        }
        started_ = true;
        return true;
    }

    ~WinsockRuntime() {
        if (started_) WSACleanup();
    }

private:
    bool started_ = false;
};

class SocketHandle {
public:
    SocketHandle() = default;
    explicit SocketHandle(SOCKET value) : value_(value) {}
    ~SocketHandle() {
        if (value_ != INVALID_SOCKET) closesocket(value_);
    }
    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    [[nodiscard]] SOCKET get() const { return value_; }
    [[nodiscard]] bool valid() const { return value_ != INVALID_SOCKET; }

private:
    SOCKET value_ = INVALID_SOCKET;
};

bool set_nonblocking(SOCKET socket, std::string& error) {
    u_long enabled = 1;
    if (ioctlsocket(socket, FIONBIO, &enabled) == SOCKET_ERROR) {
        error = winsock_error("ioctlsocket(FIONBIO)", WSAGetLastError());
        return false;
    }
    return true;
}

uint16_t bound_udp_port(SOCKET socket) {
    sockaddr_in address{};
    int size = sizeof(address);
    if (getsockname(socket, reinterpret_cast<sockaddr*>(&address), &size) == SOCKET_ERROR) {
        return 0;
    }
    return ntohs(address.sin_port);
}

#endif

}  // namespace

std::vector<uint8_t> encode_lan_discovery_query() {
    std::vector<uint8_t> packet(kDiscoveryMagic.begin(), kDiscoveryMagic.end());
    packet.push_back(kPacketQuery);
    append_u16(packet, kPeerProtocolVersion);
    return packet;
}

bool is_lan_discovery_query(const std::vector<uint8_t>& packet) {
    if (!has_header(packet, kPacketQuery) || packet.size() != kDiscoveryMagic.size() + 3u) {
        return false;
    }
    std::size_t cursor = kDiscoveryMagic.size() + 1u;
    uint16_t protocol = 0;
    return read_u16(packet, cursor, protocol) && protocol == kPeerProtocolVersion;
}

std::vector<uint8_t> encode_lan_room_advertisement(
    const LanRoomAdvertisement& advertisement) {
    if (advertisement.tcp_port == 0 || advertisement.max_players == 0 ||
        advertisement.player_count > advertisement.max_players) {
        return {};
    }

    const std::string name = bounded_host_name(advertisement.host_name);
    std::vector<uint8_t> packet(kDiscoveryMagic.begin(), kDiscoveryMagic.end());
    packet.reserve(kDiscoveryMagic.size() + 10u + name.size());
    packet.push_back(kPacketRoom);
    append_u16(packet, kPeerProtocolVersion);
    append_u16(packet, advertisement.tcp_port);
    packet.push_back(advertisement.player_count);
    packet.push_back(advertisement.max_players);
    packet.push_back(advertisement.accepting_players ? kRoomAcceptingPlayers : 0u);
    packet.push_back(static_cast<uint8_t>(name.size()));
    packet.insert(packet.end(), name.begin(), name.end());
    return packet;
}

std::optional<LanRoomAdvertisement> decode_lan_room_advertisement(
    const std::vector<uint8_t>& packet,
    std::string* error) {
    const auto fail = [error](std::string message) -> std::optional<LanRoomAdvertisement> {
        if (error) *error = std::move(message);
        return std::nullopt;
    };
    if (!has_header(packet, kPacketRoom)) return fail("Invalid LAN room packet header.");

    std::size_t cursor = kDiscoveryMagic.size() + 1u;
    uint16_t protocol = 0;
    uint16_t tcp_port = 0;
    if (!read_u16(packet, cursor, protocol) || protocol != kPeerProtocolVersion) {
        return fail("LAN room uses an incompatible peer protocol.");
    }
    if (!read_u16(packet, cursor, tcp_port) || tcp_port == 0 || cursor + 4u > packet.size()) {
        return fail("LAN room packet is truncated or has an invalid TCP port.");
    }

    LanRoomAdvertisement advertisement;
    advertisement.tcp_port = tcp_port;
    advertisement.player_count = packet[cursor++];
    advertisement.max_players = packet[cursor++];
    const uint8_t flags = packet[cursor++];
    const std::size_t name_size = packet[cursor++];
    if (advertisement.max_players == 0 || advertisement.player_count > advertisement.max_players ||
        advertisement.max_players > kPeerMaxPlayers) {
        return fail("LAN room packet has an invalid player count.");
    }
    if (name_size == 0 || name_size > kLanDiscoveryHostNameMaxBytes ||
        cursor + name_size != packet.size()) {
        return fail("LAN room packet has an invalid host name length.");
    }
    advertisement.host_name = bounded_host_name(std::string_view(
        reinterpret_cast<const char*>(packet.data() + cursor), name_size));
    advertisement.accepting_players = (flags & kRoomAcceptingPlayers) != 0u;
    if (error) error->clear();
    return advertisement;
}

struct LanDiscoveryService::Impl {
    enum class Mode : uint8_t { Stopped, Browsing, Advertising };

    explicit Impl(uint16_t port) : requested_port(port) {
        current.discovery_port = port;
    }

    mutable std::mutex mutex;
    std::thread worker;
    std::atomic<bool> stop_requested{false};
    uint16_t requested_port = kLanDiscoveryPort;
    Mode mode = Mode::Stopped;
    LanRoomAdvertisement host_advertisement;
    LanDiscoverySnapshot current;

    void mutate_snapshot(const std::function<void(LanDiscoverySnapshot&)>& mutation) {
        std::lock_guard<std::mutex> lock(mutex);
        mutation(current);
        ++current.revision;
    }

    void run_browser() {
#ifdef _WIN32
        std::string error;
        WinsockRuntime winsock;
        if (!winsock.start(error)) {
            mutate_snapshot([&](LanDiscoverySnapshot& value) {
                value.browsing = false;
                value.status_detail = error;
            });
            return;
        }

        SocketHandle socket(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
        if (!socket.valid()) {
            error = winsock_error("socket(UDP)", WSAGetLastError());
            mutate_snapshot([&](LanDiscoverySnapshot& value) {
                value.browsing = false;
                value.status_detail = error;
            });
            return;
        }
        const BOOL enabled = TRUE;
        if (setsockopt(socket.get(), SOL_SOCKET, SO_BROADCAST,
                       reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR ||
            !set_nonblocking(socket.get(), error)) {
            if (error.empty()) error = winsock_error("setsockopt(SO_BROADCAST)", WSAGetLastError());
            mutate_snapshot([&](LanDiscoverySnapshot& value) {
                value.browsing = false;
                value.status_detail = error;
            });
            return;
        }

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = 0;
        if (bind(socket.get(), reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
            error = winsock_error("bind(LAN browser)", WSAGetLastError());
            mutate_snapshot([&](LanDiscoverySnapshot& value) {
                value.browsing = false;
                value.status_detail = error;
            });
            return;
        }

        struct SeenRoom {
            LanDiscoveredRoom room;
            SteadyClock::time_point last_seen;
        };
        std::unordered_map<std::string, SeenRoom> seen;
        auto next_query = SteadyClock::time_point::min();
        const std::vector<uint8_t> query = encode_lan_discovery_query();

        while (!stop_requested.load(std::memory_order_acquire)) {
            const auto now = SteadyClock::now();
            if (now >= next_query) {
                sockaddr_in targets[2]{};
                targets[0].sin_family = AF_INET;
                targets[0].sin_addr.s_addr = htonl(INADDR_BROADCAST);
                targets[0].sin_port = htons(requested_port);
                targets[1].sin_family = AF_INET;
                targets[1].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                targets[1].sin_port = htons(requested_port);
                for (const auto& target : targets) {
                    sendto(socket.get(), reinterpret_cast<const char*>(query.data()),
                           static_cast<int>(query.size()), 0,
                           reinterpret_cast<const sockaddr*>(&target), sizeof(target));
                }
                next_query = now + kQueryInterval;
            }

            bool changed = false;
            while (true) {
                std::array<uint8_t, 256> buffer{};
                sockaddr_in sender{};
                int sender_size = sizeof(sender);
                const int received = recvfrom(socket.get(), reinterpret_cast<char*>(buffer.data()),
                                              static_cast<int>(buffer.size()), 0,
                                              reinterpret_cast<sockaddr*>(&sender), &sender_size);
                if (received < 0) {
                    const int socket_error = WSAGetLastError();
                    if (socket_error != WSAEWOULDBLOCK) {
                        error = winsock_error("recvfrom(LAN browser)", socket_error);
                    }
                    break;
                }
                if (received == 0) break;
                std::vector<uint8_t> packet(buffer.begin(), buffer.begin() + received);
                const auto advertisement = decode_lan_room_advertisement(packet);
                if (!advertisement.has_value()) continue;

                std::array<char, INET_ADDRSTRLEN> address{};
                if (!inet_ntop(AF_INET, &sender.sin_addr, address.data(),
                               static_cast<DWORD>(address.size()))) {
                    continue;
                }
                LanDiscoveredRoom room;
                static_cast<LanRoomAdvertisement&>(room) = *advertisement;
                room.address = address.data();
                const std::string key = room.address + ":" + std::to_string(room.tcp_port);
                auto existing = seen.find(key);
                if (existing == seen.end() ||
                    existing->second.room.host_name != room.host_name ||
                    existing->second.room.player_count != room.player_count ||
                    existing->second.room.max_players != room.max_players ||
                    existing->second.room.accepting_players != room.accepting_players) {
                    changed = true;
                }
                seen[key] = SeenRoom{std::move(room), now};
            }

            for (auto it = seen.begin(); it != seen.end();) {
                if (now - it->second.last_seen > kRoomExpiry) {
                    it = seen.erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }

            if (changed) {
                std::vector<LanDiscoveredRoom> rooms;
                rooms.reserve(seen.size());
                for (const auto& entry : seen) rooms.push_back(entry.second.room);
                std::sort(rooms.begin(), rooms.end(), [](const auto& lhs, const auto& rhs) {
                    if (lhs.host_name != rhs.host_name) return lhs.host_name < rhs.host_name;
                    if (lhs.address != rhs.address) return lhs.address < rhs.address;
                    return lhs.tcp_port < rhs.tcp_port;
                });
                mutate_snapshot([&](LanDiscoverySnapshot& value) {
                    value.rooms = std::move(rooms);
                    value.status_detail = error;
                });
            }
            std::this_thread::sleep_for(kWorkerPollInterval);
        }
#else
        mutate_snapshot([](LanDiscoverySnapshot& value) {
            value.browsing = false;
            value.status_detail = "LAN discovery is available on Windows builds only.";
        });
#endif
    }

    void run_advertiser() {
#ifdef _WIN32
        std::string error;
        WinsockRuntime winsock;
        if (!winsock.start(error)) {
            mutate_snapshot([&](LanDiscoverySnapshot& value) {
                value.advertising = false;
                value.status_detail = error;
            });
            return;
        }
        SocketHandle socket(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
        if (!socket.valid()) {
            error = winsock_error("socket(UDP)", WSAGetLastError());
            mutate_snapshot([&](LanDiscoverySnapshot& value) {
                value.advertising = false;
                value.status_detail = error;
            });
            return;
        }
        const BOOL enabled = TRUE;
        setsockopt(socket.get(), SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&enabled), sizeof(enabled));
        if (!set_nonblocking(socket.get(), error)) {
            mutate_snapshot([&](LanDiscoverySnapshot& value) {
                value.advertising = false;
                value.status_detail = error;
            });
            return;
        }
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(requested_port);
        if (bind(socket.get(), reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
            error = winsock_error("bind(LAN discovery port)", WSAGetLastError());
            mutate_snapshot([&](LanDiscoverySnapshot& value) {
                value.advertising = false;
                value.status_detail = error;
            });
            return;
        }
        const uint16_t actual_port = bound_udp_port(socket.get());
        mutate_snapshot([&](LanDiscoverySnapshot& value) {
            value.discovery_port = actual_port;
            value.advertising = true;
            value.status_detail.clear();
        });

        while (!stop_requested.load(std::memory_order_acquire)) {
            std::array<uint8_t, 256> buffer{};
            sockaddr_in sender{};
            int sender_size = sizeof(sender);
            const int received = recvfrom(socket.get(), reinterpret_cast<char*>(buffer.data()),
                                          static_cast<int>(buffer.size()), 0,
                                          reinterpret_cast<sockaddr*>(&sender), &sender_size);
            if (received > 0) {
                const std::vector<uint8_t> packet(buffer.begin(), buffer.begin() + received);
                if (is_lan_discovery_query(packet)) {
                    LanRoomAdvertisement advertisement;
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        advertisement = host_advertisement;
                    }
                    const std::vector<uint8_t> response =
                        encode_lan_room_advertisement(advertisement);
                    if (!response.empty()) {
                        sendto(socket.get(), reinterpret_cast<const char*>(response.data()),
                               static_cast<int>(response.size()), 0,
                               reinterpret_cast<const sockaddr*>(&sender), sender_size);
                    }
                }
            } else if (received < 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
                error = winsock_error("recvfrom(LAN advertiser)", WSAGetLastError());
                mutate_snapshot([&](LanDiscoverySnapshot& value) {
                    value.status_detail = error;
                });
            }
            std::this_thread::sleep_for(kWorkerPollInterval);
        }
#else
        mutate_snapshot([](LanDiscoverySnapshot& value) {
            value.advertising = false;
            value.status_detail = "LAN discovery is available on Windows builds only.";
        });
#endif
    }
};

LanDiscoveryService::LanDiscoveryService(uint16_t discovery_port)
    : impl_(std::make_unique<Impl>(discovery_port)) {}

LanDiscoveryService::~LanDiscoveryService() {
    stop();
}

void LanDiscoveryService::start_browsing() {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->mode == Impl::Mode::Browsing) return;
    }
    stop();
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->mode = Impl::Mode::Browsing;
        impl_->current.browsing = true;
        impl_->current.advertising = false;
        impl_->current.rooms.clear();
        impl_->current.status_detail.clear();
        ++impl_->current.revision;
    }
    impl_->stop_requested.store(false, std::memory_order_release);
    impl_->worker = std::thread([impl = impl_.get()] { impl->run_browser(); });
}

void LanDiscoveryService::advertise(LanRoomAdvertisement advertisement) {
    advertisement.host_name = bounded_host_name(advertisement.host_name);
    bool already_advertising = false;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        already_advertising = impl_->mode == Impl::Mode::Advertising;
        impl_->host_advertisement = std::move(advertisement);
    }
    if (already_advertising) return;

    stop();
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->mode = Impl::Mode::Advertising;
        impl_->current.browsing = false;
        impl_->current.advertising = false;
        impl_->current.rooms.clear();
        impl_->current.status_detail.clear();
        ++impl_->current.revision;
    }
    impl_->stop_requested.store(false, std::memory_order_release);
    impl_->worker = std::thread([impl = impl_.get()] { impl->run_advertiser(); });
}

void LanDiscoveryService::stop() {
    impl_->stop_requested.store(true, std::memory_order_release);
    if (impl_->worker.joinable()) impl_->worker.join();
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->mode == Impl::Mode::Stopped && !impl_->current.browsing &&
        !impl_->current.advertising && impl_->current.rooms.empty()) {
        return;
    }
    impl_->mode = Impl::Mode::Stopped;
    impl_->current.browsing = false;
    impl_->current.advertising = false;
    impl_->current.rooms.clear();
    impl_->current.status_detail.clear();
    impl_->current.discovery_port = impl_->requested_port;
    ++impl_->current.revision;
}

LanDiscoverySnapshot LanDiscoveryService::snapshot() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->current;
}

}  // namespace tenriff::network
