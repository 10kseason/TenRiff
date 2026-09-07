#pragma once

#include <algorithm>

#include "network/PeerSession.h"
#include "render/MultiplayerPresentation.h"

namespace tenriff::app {

inline bool multiplayer_opponents_terminal(const network::PeerSessionSnapshot& room) {
    bool has_opponent = false;
    for (const auto& player : room.participants) {
        if (player.local || player.player_id == room.local_player_id) continue;
        has_opponent = true;
        if (!player.has_score || !(player.latest_score.finished || player.latest_score.game_over ||
                                   player.latest_score.aborted)) return false;
    }
    return has_opponent;
}

inline render::MultiplayerPlayerData multiplayer_player_data(
    const network::PeerParticipantSnapshot& participant) {
    render::MultiplayerPlayerData player;
    player.player_id = participant.player_id;
    player.name = participant.name;
    player.local = participant.local;
    player.has_score = participant.has_score;
    if (participant.has_score) {
        const auto& score = participant.latest_score;
        player.score = score.score;
        player.combo = score.combo;
        player.max_combo = score.max_combo;
        player.perfect = score.perfect;
        player.great = score.great;
        player.good = score.good;
        player.bad = score.bad;
        player.poor = score.poor;
        player.gauge = std::clamp(score.gauge_milli / 1000.0, 0.0, 100.0);
        player.finished = score.finished;
        player.game_over = score.game_over;
        player.aborted = score.aborted;
    }
    return player;
}

// The local HUD/result is newer than the network's throttled local score copy.
// Preserve every remote row, including players whose first score has not arrived.
inline std::vector<render::MultiplayerPlayerData> multiplayer_standings(
    const network::PeerSessionSnapshot& room, render::MultiplayerPlayerData local) {
    std::vector<render::MultiplayerPlayerData> players;
    local.local = true;
    local.player_id = room.local_player_id;
    for (const auto& participant : room.participants) {
        if (participant.local || participant.player_id == room.local_player_id) {
            if (local.name.empty()) local.name = participant.name;
        } else {
            players.push_back(multiplayer_player_data(participant));
        }
    }
    players.push_back(std::move(local));
    std::stable_sort(players.begin(), players.end(), [](const auto& a, const auto& b) {
        if (a.has_score != b.has_score) return a.has_score;
        if (a.has_score && a.score != b.score) return a.score > b.score;
        return a.player_id < b.player_id;
    });
    for (std::size_t i = 0; i < players.size(); ++i) {
        if (!players[i].has_score) continue;
        players[i].rank = i > 0 && players[i - 1].has_score && players[i - 1].score == players[i].score
            ? players[i - 1].rank : static_cast<int>(i + 1);
    }
    return players;
}

} // namespace tenriff::app
