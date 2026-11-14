#include "app/MenuApp.h"
#include "app/GameSession.h"

#include <tuple>

#include "app/MenuAppSongSelectUtils.h"
#include "app/MenuRecordUtils.h"
#include "app/MenuSongUtils.h"
#include "app/ModeManager.h"

namespace tenriff::app {

void MenuApp::prepare_score_comparison_replay(GameSession& session, CommandLineOptions& options) const {
    const std::string score_path = config_.skin.score_comparison_enabled
        ? score_comparison_replay_path_for_selected_song() : std::string{};
    // Turning on a HUD must never pick another visible opponent or alter the
    // existing historical Ghost Battle conversion/playback policy.
    options.ghost_replay_path = config_.mode.ghost_battle_enabled
        ? best_replay_path_for_selected_song() : score_path;
    session.set_score_comparison_replay_path(score_path);
    session.set_score_comparison_only(!config_.mode.ghost_battle_enabled);
}

std::string MenuApp::score_comparison_replay_path_for_selected_song() const {
    const auto* entry = selected_song_ >= 0 ? visible_song_entry(static_cast<std::size_t>(selected_song_)) : nullptr;
    if (!entry || showing_table_level_folders()) return {};
    const BestResultRecord* best = nullptr;
    for (const auto& key : menu_songs::build_chart_path_keys(song_absolute_path(*entry), songs_path_)) {
        const auto found = chart_score_high_water_.find(key);
        if (found == chart_score_high_water_.end() || !found->second.best.has_value || !found->second.best.ex_score) continue;
        const auto& candidate = found->second.best;
        if (!best || std::tie(candidate.judgement_generation, candidate.bms_gauge_version,
                              candidate.bms_control_flow_version, candidate.bms_timing_version, candidate.ex_score) >
                     std::tie(best->judgement_generation, best->bms_gauge_version,
                              best->bms_control_flow_version, best->bms_timing_version, best->ex_score)) best = &candidate;
    }
    if (!best || !best->replay_verified || best->replay_path.empty()) return {};
    for (const auto& record : local_play_records_) {
        if (record.replay_path == best->replay_path &&
            equivalent_mode_mod_tokens(record.mods, config_.mode.mods) &&
            (!mode_mod_long_note_mix(record.mods) || record.long_note_mix_version == 3) &&
            menu_records::default_ghost_replay_allowed(record.autoplay_enabled,
                                                      record.practice_no_fail_enabled, record.clear_status))
            return best->replay_path;
    }
    // An incompatible chart-wide high score is unavailable, not a fabricated
    // comparison with another mod/rate/chart. Session setup checks full metadata.
    return {};
}

} // namespace tenriff::app
