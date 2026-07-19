#pragma once

#include <cstdint>
#include <limits>

namespace tenriff::app {

inline constexpr std::int64_t kPeerBattleScoreLeadEndpoint = 10'000;

struct PeerBattleScoreLead {
    // Saturated local - peer score difference. The renderer can use this for
    // labels without risking signed overflow at the integer limits.
    std::int64_t difference = 0;

    // Local-perspective position on the LOSS -> WIN track.
    // 0.0 = LOSS endpoint, 0.5 = tied, 1.0 = WIN endpoint.
    double position = 0.5;
};

[[nodiscard]] inline constexpr std::int64_t peer_battle_saturated_score_difference(
    std::int64_t local_score,
    std::int64_t peer_score) noexcept {
    if (peer_score > 0 &&
        local_score < (std::numeric_limits<std::int64_t>::min)() + peer_score) {
        return (std::numeric_limits<std::int64_t>::min)();
    }
    if (peer_score < 0 &&
        local_score > (std::numeric_limits<std::int64_t>::max)() + peer_score) {
        return (std::numeric_limits<std::int64_t>::max)();
    }
    return local_score - peer_score;
}

[[nodiscard]] inline constexpr PeerBattleScoreLead peer_battle_score_lead(
    std::int64_t local_score,
    std::int64_t peer_score) noexcept {
    const std::int64_t difference =
        peer_battle_saturated_score_difference(local_score, peer_score);

    if (difference <= -kPeerBattleScoreLeadEndpoint) {
        return PeerBattleScoreLead{difference, 0.0};
    }
    if (difference >= kPeerBattleScoreLeadEndpoint) {
        return PeerBattleScoreLead{difference, 1.0};
    }

    const double signed_ratio =
        static_cast<double>(difference) /
        static_cast<double>(kPeerBattleScoreLeadEndpoint);
    return PeerBattleScoreLead{difference, 0.5 + signed_ratio * 0.5};
}

enum class PeerBattleSpectatorDecision : std::uint8_t {
    ContinueSpectating,
    FinishSession,
};

struct PeerBattleSpectatorState {
    bool local_game_over = false;
    bool peer_connected = false;
    bool round_active = false;
    bool remote_finished = false;
    bool remote_game_over = false;
};

// Only a locally failed peer-battle session is held open for spectating. A
// terminal remote state, disconnect, or canceled round releases the session.
// Treating remote GAME OVER as terminal also prevents simultaneous-failure
// deadlocks before both sides publish their authoritative FinalScore packets.
[[nodiscard]] inline constexpr PeerBattleSpectatorDecision peer_battle_spectator_decision(
    const PeerBattleSpectatorState& state) noexcept {
    if (!state.local_game_over || !state.peer_connected || !state.round_active ||
        state.remote_finished || state.remote_game_over) {
        return PeerBattleSpectatorDecision::FinishSession;
    }
    return PeerBattleSpectatorDecision::ContinueSpectating;
}

}  // namespace tenriff::app
