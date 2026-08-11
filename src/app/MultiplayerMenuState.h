#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace tenriff::app {

enum class MultiplayerRole : uint8_t {
    Host,
    Join,
};

enum class GameplayLaunchKind : uint8_t {
    SinglePlayer,
    PeerBattle,
};

enum class MultiplayerMenuRow : uint8_t {
    Address = 0,
    Port,
    LanRoom,
    Host,
    Join,
    Chart,
    Ready,
    Start,
    Chat,
    Options,
    Back,
};

enum class MultiplayerEditField : uint8_t {
    None,
    Address,
    Port,
    Chat,
};

inline constexpr int kMultiplayerMenuRowCount = 11;
inline constexpr std::size_t kMultiplayerAddressMaxLength = 255;
inline constexpr std::size_t kMultiplayerPortTextMaxLength = 5;
inline constexpr std::size_t kMultiplayerChatInputMaxBytes = 256;

struct MultiplayerMenuState {
    MultiplayerRole role = MultiplayerRole::Join;
    int cursor = 0;
    MultiplayerEditField edit_field = MultiplayerEditField::None;
    std::string address = "127.0.0.1";
    std::string port_text = "27300";
    std::string chat_input;

    bool connected = false;
    bool local_ready = false;
    bool peer_ready = false;  // All other connected players are ready.
    uint8_t local_player_id = 0;
    uint8_t leader_player_id = 0;
    uint8_t player_count = 0;
    bool local_is_leader = false;
    uint64_t local_chart_fingerprint = 0;
    uint64_t local_chart_size = 0;
    uint64_t peer_chart_fingerprint = 0;
    uint64_t peer_chart_size = 0;
};

[[nodiscard]] inline constexpr bool gameplay_launch_uses_peer_battle(
    GameplayLaunchKind kind,
    bool replay_playback) {
    return kind == GameplayLaunchKind::PeerBattle && !replay_playback;
}

inline void reset_multiplayer_menu_session(MultiplayerMenuState& state) {
    std::string address = std::move(state.address);
    std::string port_text = std::move(state.port_text);
    state = {};
    state.address = std::move(address);
    state.port_text = std::move(port_text);
}

[[nodiscard]] inline constexpr int clamp_multiplayer_menu_cursor(int cursor) {
    return cursor < 0 ? 0 : (cursor >= kMultiplayerMenuRowCount ? kMultiplayerMenuRowCount - 1 : cursor);
}

[[nodiscard]] inline constexpr int move_multiplayer_menu_cursor(int cursor, int delta) {
    const long long current = clamp_multiplayer_menu_cursor(cursor);
    const long long requested = current + static_cast<long long>(delta);
    return static_cast<int>(std::clamp(requested,
                                       0LL,
                                       static_cast<long long>(kMultiplayerMenuRowCount - 1)));
}

[[nodiscard]] inline constexpr bool is_multiplayer_address_edit_character(char ch) {
    const bool ascii_letter = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    const bool ascii_digit = ch >= '0' && ch <= '9';
    return ascii_letter || ascii_digit || ch == '.' || ch == '-';
}

[[nodiscard]] inline bool is_valid_multiplayer_address_text(std::string_view address) {
    if (address.empty() || address.size() > kMultiplayerAddressMaxLength) {
        return false;
    }
    return std::all_of(address.begin(), address.end(), is_multiplayer_address_edit_character);
}

inline bool try_append_multiplayer_address_character(std::string& address, char ch) {
    if (address.size() >= kMultiplayerAddressMaxLength || !is_multiplayer_address_edit_character(ch)) {
        return false;
    }
    address.push_back(ch);
    return true;
}

[[nodiscard]] inline constexpr bool is_multiplayer_port_edit_character(char ch) {
    return ch >= '0' && ch <= '9';
}

inline bool try_append_multiplayer_port_character(std::string& port_text, char ch) {
    if (port_text.size() >= kMultiplayerPortTextMaxLength || !is_multiplayer_port_edit_character(ch)) {
        return false;
    }
    port_text.push_back(ch);
    return true;
}

inline void erase_last_multiplayer_utf8_character(std::string& text) {
    if (text.empty()) return;
    std::size_t erase_from = text.size() - 1;
    while (erase_from > 0 &&
           (static_cast<unsigned char>(text[erase_from]) & 0xC0u) == 0x80u) {
        --erase_from;
    }
    text.erase(erase_from);
}

inline bool try_append_multiplayer_chat_text(std::string& target,
                                             std::string_view fragment) {
    if (fragment.empty() || target.size() >= kMultiplayerChatInputMaxBytes) {
        return false;
    }
    const std::size_t available = kMultiplayerChatInputMaxBytes - target.size();
    std::size_t cursor = 0;
    std::size_t accepted = 0;
    while (cursor < fragment.size() && cursor < available) {
        const unsigned char lead = static_cast<unsigned char>(fragment[cursor]);
        std::size_t length = 1;
        if ((lead & 0x80u) == 0u) {
            length = 1;
        } else if ((lead & 0xE0u) == 0xC0u) {
            length = 2;
        } else if ((lead & 0xF0u) == 0xE0u) {
            length = 3;
        } else if ((lead & 0xF8u) == 0xF0u) {
            length = 4;
        }
        if (cursor + length > fragment.size() ||
            cursor + length > available) {
            break;
        }
        accepted = cursor + length;
        cursor += length;
    }
    if (accepted == 0) return false;
    target.append(fragment.substr(0, accepted));
    return true;
}

[[nodiscard]] inline std::optional<uint16_t> parse_multiplayer_port(std::string_view port_text) {
    if (port_text.empty() || port_text.size() > kMultiplayerPortTextMaxLength) {
        return std::nullopt;
    }

    uint32_t value = 0;
    for (char ch : port_text) {
        if (!is_multiplayer_port_edit_character(ch)) {
            return std::nullopt;
        }
        value = value * 10u + static_cast<uint32_t>(ch - '0');
        if (value > 65535u) {
            return std::nullopt;
        }
    }

    if (value == 0u) {
        return std::nullopt;
    }
    return static_cast<uint16_t>(value);
}

[[nodiscard]] inline constexpr bool multiplayer_chart_fingerprints_match(
    const MultiplayerMenuState& state) {
    return state.local_chart_fingerprint != 0 &&
           state.local_chart_fingerprint == state.peer_chart_fingerprint &&
           state.local_chart_size == state.peer_chart_size;
}

[[nodiscard]] inline constexpr bool multiplayer_leader_can_choose_chart(
    const MultiplayerMenuState& state) {
    return state.local_is_leader;
}

[[nodiscard]] inline constexpr bool multiplayer_ready_gate_open(const MultiplayerMenuState& state) {
    return state.connected && multiplayer_chart_fingerprints_match(state);
}

[[nodiscard]] inline constexpr bool multiplayer_start_gate_open(const MultiplayerMenuState& state) {
    return multiplayer_ready_gate_open(state) && state.local_ready && state.peer_ready;
}

[[nodiscard]] inline constexpr bool multiplayer_leader_can_start(const MultiplayerMenuState& state) {
    return state.local_is_leader && multiplayer_start_gate_open(state);
}

/// The coordinator sends Begin from the leader request. Other players receive the
/// Begin frame later, so subtract half the measured RTT as a symmetric-path
/// estimate while preserving at least 500 ms of visible countdown.
[[nodiscard]] inline constexpr uint32_t multiplayer_compensated_peer_begin_delay_ms(
    uint32_t host_delay_ms,
    uint32_t estimated_rtt_ms) {
    const uint32_t estimated_one_way_ms = std::min<uint32_t>(estimated_rtt_ms / 2u, 500u);
    const uint32_t minimum_delay_ms = std::min<uint32_t>(host_delay_ms, 500u);
    return std::max<uint32_t>(minimum_delay_ms, host_delay_ms -
                                                   std::min(host_delay_ms, estimated_one_way_ms));
}

[[nodiscard]] inline constexpr bool multiplayer_ascii_iequals(std::string_view lhs,
                                                               std::string_view rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        const auto lower = [](char ch) constexpr {
            return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
        };
        if (lower(lhs[i]) != lower(rhs[i])) return false;
    }
    return true;
}

/// Title/path matches are only a search-order hint. The chart is accepted only
/// after its exact file fingerprint and size match the host announcement.
[[nodiscard]] inline constexpr bool multiplayer_chart_candidate_name_matches(
    std::string_view title,
    std::string_view path,
    std::string_view host_name) {
    return !host_name.empty() &&
           (multiplayer_ascii_iequals(title, host_name) ||
            multiplayer_ascii_iequals(path, host_name));
}

enum class MultiplayerScoreOutcome : int8_t {
    Loss = -1,
    Draw = 0,
    Win = 1,
};

struct MultiplayerScoreComparison {
    MultiplayerScoreOutcome outcome = MultiplayerScoreOutcome::Draw;
    int64_t difference = 0;
};

[[nodiscard]] inline constexpr MultiplayerScoreComparison compare_multiplayer_scores(
    int64_t local_score,
    int64_t peer_score) {
    MultiplayerScoreComparison comparison;
    comparison.outcome = local_score > peer_score
                             ? MultiplayerScoreOutcome::Win
                             : (local_score < peer_score ? MultiplayerScoreOutcome::Loss
                                                         : MultiplayerScoreOutcome::Draw);
    if (peer_score > 0 && local_score < (std::numeric_limits<int64_t>::min)() + peer_score) {
        comparison.difference = (std::numeric_limits<int64_t>::min)();
    } else if (peer_score < 0 && local_score > (std::numeric_limits<int64_t>::max)() + peer_score) {
        comparison.difference = (std::numeric_limits<int64_t>::max)();
    } else {
        comparison.difference = local_score - peer_score;
    }
    return comparison;
}

}  // namespace tenriff::app
