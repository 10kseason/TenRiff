#include "app/MenuApp.h"

#include <filesystem>
#include <iostream>
#include <unordered_set>

#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSongSelectUtils.h"
#include "app/MenuRecordUtils.h"
#include "app/MenuSongUtils.h"
#include "app/ModeManager.h"
#include "app/ReplayVerifier.h"
#include "gameplay/Replay.h"
#include "timing/HighResClock.h"

namespace tenriff::app {

namespace {

std::filesystem::path utf8_path_or_empty(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    try {
        return std::filesystem::u8path(value.begin(), value.end());
    } catch (...) {
        return {};
    }
}

std::filesystem::path resolve_record_replay_path(const std::filesystem::path& result_path,
                                                 const std::filesystem::path& profile_dir,
                                                 std::string_view stored_path) {
    const std::filesystem::path direct = utf8_path_or_empty(stored_path);
    std::error_code ec;
    if (!direct.empty() && std::filesystem::is_regular_file(direct, ec)) {
        return direct;
    }
    ec.clear();
    const std::filesystem::path filename = direct.filename();
    if (filename.empty()) {
        return direct;
    }
    const std::filesystem::path beside_results = result_path.parent_path().parent_path() /
                                                  "replays" / filename;
    if (std::filesystem::is_regular_file(beside_results, ec)) {
        return beside_results;
    }
    ec.clear();
    const std::filesystem::path profile_replay = profile_dir / "replays" / filename;
    if (std::filesystem::is_regular_file(profile_replay, ec)) {
        return profile_replay;
    }
    return direct;
}

}  // namespace

void MenuApp::reload_chart_best_results() {
    using namespace menu_song_select;

    chart_best_results_.clear();
    local_play_records_.clear();
    chart_play_record_indices_.clear();
    current_song_record_indices_.clear();
    replay_summary_cache_.clear();

    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path results_dir = path_from_utf8(profile_dir_) / "results";
    if (!fs::exists(results_dir, ec) || !fs::is_directory(results_dir, ec)) {
        return;
    }

    fs::directory_options options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(results_dir, options, ec);
    fs::recursive_directory_iterator end;
    while (it != end) {
        if (ec) {
            ec.clear();
            it.increment(ec);
            continue;
        }

        const fs::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        ec.clear();

        const std::string ext = to_lower_ascii(entry.path().extension().u8string());
        if (ext != ".json") {
            it.increment(ec);
            continue;
        }

        std::string parse_error;
        auto parsed = menu_records::parse_result_file(entry.path(), &parse_error);
        if (!parsed.has_value()) {
            if (!parse_error.empty()) {
                std::cerr << "[warn] Failed to parse result file " << entry.path().u8string()
                          << ": " << parse_error << std::endl;
            }
            it.increment(ec);
            continue;
        }

        const fs::path replay_path = resolve_record_replay_path(
            entry.path(), path_from_utf8(profile_dir_), parsed->replay_path);
        ReplayVerificationResult verification;
        if (parsed->replay_format_version < gameplay::kReplayFormatVersion) {
            verification.status = ReplayVerificationStatus::LegacyUnverified;
            verification.detail = "Legacy result has no replay/chart integrity binding.";
        } else if (parsed->replay_sha256.empty() || parsed->chart_sha256.empty() ||
                   parsed->ruleset_id.empty()) {
            verification.status = ReplayVerificationStatus::MissingEvidence;
            verification.detail = "Result is missing replay, chart, or ruleset evidence.";
        } else {
            verification = verify_replay_file(replay_path,
                                              utf8_path_or_empty(parsed->chart_path),
                                              parsed->replay_sha256);
            if (verification.verified() &&
                (verification.chart_sha256 != parsed->chart_sha256 ||
                 parsed->ruleset_id != kCanonicalReplayRulesetId)) {
                verification.status = ReplayVerificationStatus::Invalid;
                verification.detail = "Result metadata does not match the verified replay evidence.";
                verification.official_eligible = false;
            }
        }

        const bool replay_verified = verification.verified();
        const gameplay::ResultStats& effective_stats =
            replay_verified ? verification.stats : parsed->stats;
        const int64_t effective_score =
            replay_verified ? verification.final_score : parsed->final_score;
        const bool effective_game_over =
            replay_verified ? verification.game_over : parsed->game_over;
        const std::string& effective_clear_status =
            replay_verified ? verification.clear_status : parsed->clear_status;
        const std::string& effective_final_gauge =
            replay_verified ? verification.final_gauge : parsed->final_gauge;

        BestResultRecord candidate;
        candidate.has_value = true;
        candidate.replay_verified = replay_verified;
        candidate.rank = menu_records::calculate_rank(effective_stats, effective_game_over);
        candidate.best_score = effective_score;
        candidate.detail_score = menu_records::calculate_detail_score(effective_stats);
        candidate.total_notes = effective_stats.total_notes;
        candidate.accuracy = menu_records::calculate_accuracy(effective_stats);
        candidate.detailed_accuracy = menu_records::calculate_detailed_accuracy(effective_stats);
        candidate.clear_status = effective_clear_status;
        candidate.final_gauge = effective_final_gauge;
        candidate.game_over = effective_game_over;
        candidate.max_combo = effective_stats.max_combo;
        candidate.perfect = effective_stats.counts.pg;
        candidate.great = effective_stats.counts.gr;
        candidate.good = effective_stats.counts.gd;
        candidate.bad = effective_stats.counts.bd;
        candidate.poor = effective_stats.counts.pr;
        candidate.created_utc = parsed->created_utc;
        candidate.result_path = entry.path().u8string();
        candidate.replay_path = replay_path.empty() ? parsed->replay_path : replay_path.u8string();
        const int candidate_judged = menu_records::judged_total(effective_stats.counts);
        const int candidate_clear_priority =
            menu_records::clear_status_priority(effective_clear_status,
                                                effective_game_over,
                                                effective_final_gauge);

        LocalPlayRecord record;
        record.chart_path = parsed->chart_path;
        record.chart_format = parsed->chart_format;
        record.created_utc = parsed->created_utc;
        record.player_name =
            parsed->player_name.empty() ? profile_display_name() : parsed->player_name;
        record.result_path = entry.path().u8string();
        record.replay_path = candidate.replay_path;
        record.rank = candidate.rank;
        record.clear_status = effective_clear_status;
        record.final_gauge = effective_final_gauge;
        record.game_over = effective_game_over;
        record.mods = parsed->mods;
        record.rate_multiplier = parsed->rate_multiplier;
        record.score_multiplier = parsed->score_multiplier;
        record.pause_used = parsed->pause_used;
        record.autoplay_enabled = parsed->autoplay_enabled;
        record.practice_no_fail_enabled = parsed->practice_no_fail_enabled;
        record.verification_status = std::string(replay_verification_status_token(verification.status));
        record.verification_detail = verification.detail;
        record.replay_claims_match = replay_verified && verification.claims_match;
        record.raw_score = effective_stats.raw_score;
        record.score = candidate.best_score;
        record.detail_score = candidate.detail_score;
        record.accuracy = menu_records::calculate_accuracy(effective_stats);
        record.detailed_accuracy = menu_records::calculate_detailed_accuracy(effective_stats);
        record.max_combo = effective_stats.max_combo;
        record.total_notes = effective_stats.total_notes;
        record.judged_notes = candidate_judged;
        record.perfect = effective_stats.counts.pg;
        record.great = effective_stats.counts.gr;
        record.good = effective_stats.counts.gd;
        record.bad = effective_stats.counts.bd;
        record.poor = effective_stats.counts.pr;
        record.mean_delta_ms = effective_stats.mean_delta_ms;
        record.stddev_delta_ms = effective_stats.stddev_delta_ms();
        const std::size_t record_index = local_play_records_.size();
        local_play_records_.push_back(record);
        const bool note_count_modified =
            mode_mod_adds_notes(parsed->mods) ||
            parsed->key_conversion_note_add_mode == "add_25_plus";

        // Fan results out across all normalized path keys so old exports still match
        // after source-root changes or relative/absolute-path differences.
        for (const auto& key : menu_songs::build_chart_path_keys(parsed->chart_path, songs_path_)) {
            chart_play_record_indices_[key].push_back(record_index);
            // Keep autoplay runs in local history/replay browsing, but never let
            // them become the chart's official best score or clear lamp.
            if (note_count_modified || parsed->autoplay_enabled ||
                parsed->practice_no_fail_enabled || !verification.official_eligible) {
                continue;
            }
            auto existing = chart_best_results_.find(key);
            if (existing == chart_best_results_.end()) {
                chart_best_results_.emplace(key, candidate);
                continue;
            }

            const int existing_judged =
                existing->second.perfect + existing->second.great + existing->second.good + existing->second.bad;
            const int existing_clear_priority =
                menu_records::clear_status_priority(existing->second.clear_status,
                                                    existing->second.game_over,
                                                    existing->second.final_gauge);
            if (menu_records::is_better_record(candidate.best_score,
                                               candidate_clear_priority,
                                               candidate.detail_score,
                                               candidate.detailed_accuracy,
                                               candidate.max_combo,
                                               candidate_judged,
                                               candidate.created_utc,
                                               existing->second.best_score,
                                               existing_clear_priority,
                                               existing->second.detail_score,
                                               existing->second.detailed_accuracy,
                                               existing->second.max_combo,
                                               existing_judged,
                                               existing->second.created_utc)) {
                existing->second = candidate;
            }
        }

        it.increment(ec);
    }

    rebuild_current_song_record_indices();
}

void MenuApp::rebuild_current_song_record_indices() {
    current_song_record_indices_.clear();

    const SongEntry* entry = (selected_song_ >= 0) ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                                   : nullptr;
    if (!entry) {
        selected_record_ = 0;
        return;
    }

    std::unordered_set<std::size_t> seen;
    for (const auto& key : menu_songs::build_chart_path_keys(entry->path, songs_path_)) {
        auto found = chart_play_record_indices_.find(key);
        if (found == chart_play_record_indices_.end()) {
            continue;
        }
        for (std::size_t index : found->second) {
            if (index < local_play_records_.size() && seen.insert(index).second) {
                current_song_record_indices_.push_back(index);
            }
        }
    }

    // Records are shown by clear strength first, then score/combo recency.
    std::stable_sort(current_song_record_indices_.begin(), current_song_record_indices_.end(),
                     [this](std::size_t lhs_index, std::size_t rhs_index) {
                         const auto& lhs = local_play_records_[lhs_index];
                         const auto& rhs = local_play_records_[rhs_index];
                         const int lhs_clear =
                             menu_records::clear_status_priority(lhs.clear_status, lhs.game_over, lhs.final_gauge);
                         const int rhs_clear =
                             menu_records::clear_status_priority(rhs.clear_status, rhs.game_over, rhs.final_gauge);
                         if (lhs_clear != rhs_clear) {
                             return lhs_clear > rhs_clear;
                         }
                         if (lhs.score != rhs.score) {
                             return lhs.score > rhs.score;
                         }
                         if (lhs.max_combo != rhs.max_combo) {
                             return lhs.max_combo > rhs.max_combo;
                         }
                         return lhs.created_utc > rhs.created_utc;
                     });

    if (current_song_record_indices_.empty()) {
        selected_record_ = 0;
    } else {
        selected_record_ = clamp_int(selected_record_, 0, static_cast<int>(current_song_record_indices_.size() - 1));
    }
}

const MenuApp::LocalPlayRecord* MenuApp::current_selected_record() const {
    if (current_song_record_indices_.empty()) {
        return nullptr;
    }
    if (selected_record_ < 0 || selected_record_ >= static_cast<int>(current_song_record_indices_.size())) {
        return nullptr;
    }
    const std::size_t record_index = current_song_record_indices_[static_cast<std::size_t>(selected_record_)];
    if (record_index >= local_play_records_.size()) {
        return nullptr;
    }
    return &local_play_records_[record_index];
}

const MenuApp::ReplaySummary* MenuApp::replay_summary_for_path(const std::string& path) {
    using namespace menu_song_select;

    if (path.empty()) {
        return nullptr;
    }

    // The Song Select detail panel can ask for the same replay summary every snapshot,
    // so cache the parse result once per replay path.
    auto found = replay_summary_cache_.find(path);
    if (found != replay_summary_cache_.end()) {
        return &found->second;
    }

    ReplaySummary summary;
    summary.loaded = true;
    std::error_code ec;
    summary.exists = std::filesystem::exists(path_from_utf8(path), ec) && !ec;
    if (summary.exists) {
        std::string parse_error;
        auto parsed = menu_records::parse_replay_file(path_from_utf8(path), &parse_error);
        if (parsed.has_value()) {
            summary.sample_rate = parsed->sample_rate;
            summary.lane_count = parsed->lane_count;
            summary.event_count = parsed->event_count;
            summary.duration_samples = parsed->duration_samples;
            summary.rate = parsed->rate;
            summary.input_offset_ms = parsed->input_offset_ms;
            summary.mods = parsed->mods;
            summary.rate_multiplier = parsed->rate_multiplier;
            summary.score_multiplier = parsed->score_multiplier;
            summary.pause_used = parsed->pause_used;
            summary.final_score = parsed->final_score;
        } else {
            summary.error = parse_error;
        }
    } else if (ec) {
        summary.error = ec.message();
    } else {
        summary.error = "Replay file not found.";
    }

    auto [it, inserted] = replay_summary_cache_.emplace(path, std::move(summary));
    (void)inserted;
    return &it->second;
}

MenuApp::BestResultRecord MenuApp::current_song_best_result() const {
    const SongEntry* entry = (selected_song_ >= 0) ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                                   : nullptr;
    return entry ? best_result_for_song_entry(*entry) : BestResultRecord{};
}

bool MenuApp::open_result_record(const std::string& result_path,
                                 const std::string& replay_path) {
    using namespace menu_song_select;

    if (result_path.empty()) {
        return false;
    }

    std::string parse_error;
    auto parsed = menu_records::parse_result_file(path_from_utf8(result_path), &parse_error);
    if (!parsed.has_value()) {
        if (!parse_error.empty()) {
            std::cerr << "[warn] Failed to open saved result " << result_path
                      << ": " << parse_error << std::endl;
        }
        return false;
    }

    last_result_ = parsed->stats;
    last_game_over_ = parsed->game_over;
    last_clear_status_ = parsed->clear_status;
    last_final_gauge_ = parsed->final_gauge;
    has_result_ = true;
    last_result_mods_ = parsed->mods;
    last_result_rate_multiplier_ = parsed->rate_multiplier;
    last_result_score_multiplier_ = parsed->score_multiplier;
    last_result_final_score_ = parsed->final_score;
    last_pause_used_ = parsed->pause_used;
    last_result_player_name_ =
        parsed->player_name.empty() ? profile_display_name() : parsed->player_name;
    last_replay_path_ = replay_path.empty() ? parsed->replay_path : replay_path;
    last_result_path_ = result_path;
    last_export_warnings_.clear();
    last_session_replay_playback_ = false;
    const SongEntry* entry = (selected_song_ >= 0)
                                 ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                 : nullptr;
    update_last_chart_metadata(parsed->chart_path, entry);
    result_presentation_start_ns_ = timing::HighResClock::now_ns();
    result_presentation_skipped_ = false;
    screen_ = Screen::Result;
    return true;
}

bool MenuApp::open_selected_record_result() {
    rebuild_current_song_record_indices();
    const LocalPlayRecord* record = current_selected_record();
    return record && open_result_record(record->result_path, record->replay_path);
}

bool MenuApp::open_current_song_best_result() {
    const BestResultRecord best = current_song_best_result();
    return best.has_value && open_result_record(best.result_path, best.replay_path);
}

bool MenuApp::launch_replay_from_path(const std::string& replay_path, const std::string& fallback_chart_path) {
    if (replay_path.empty()) {
        return false;
    }

    auto replay_load = gameplay::load_replay_json(replay_path);
    if (!replay_load.success()) {
        std::cerr << "[warn] Failed to load replay for playback " << replay_path
                  << ": " << replay_load.error << std::endl;
        return false;
    }
    for (const auto& warning : replay_load.warnings) {
        std::cerr << "[warn] " << warning << std::endl;
    }

    std::string chart_path = replay_load.replay->chart_path;
    if (chart_path.empty()) {
        chart_path = fallback_chart_path;
    }
    if (chart_path.empty()) {
        std::cerr << "[warn] Replay file does not contain a chart path: " << replay_path << std::endl;
        return false;
    }

    launch_gameplay(chart_path, replay_path);
    return true;
}

bool MenuApp::launch_last_result_replay() {
    return launch_replay_from_path(last_replay_path_);
}

std::string MenuApp::best_replay_path_for_selected_song() const {
    for (std::size_t record_index : current_song_record_indices_) {
        if (record_index >= local_play_records_.size()) {
            continue;
        }
        const auto& record = local_play_records_[record_index];
        if (!record.replay_path.empty() &&
            equivalent_mode_mod_tokens(record.mods, config_.mode.mods) &&
            menu_records::default_ghost_replay_allowed(record.autoplay_enabled,
                                                       record.practice_no_fail_enabled,
                                                       record.clear_status)) {
            return record.replay_path;
        }
    }
    return {};
}

bool MenuApp::launch_selected_record_replay() {
    rebuild_current_song_record_indices();
    const LocalPlayRecord* record = current_selected_record();
    if (!record) {
        return false;
    }
    return launch_replay_from_path(record->replay_path, record->chart_path);
}

std::string MenuApp::selected_song_absolute_path() const {
    if (selected_song_ < 0) {
        return {};
    }
    const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(selected_song_));
    if (!entry) {
        return {};
    }

    return song_absolute_path(*entry);
}

std::string MenuApp::song_absolute_path(const SongEntry& entry) const {
    using namespace menu_song_select;

    namespace fs = std::filesystem;
    fs::path candidate = path_from_utf8(entry.path);
    if (!candidate.is_absolute()) {
        fs::path rooted = path_from_utf8(songs_path_) / candidate;
        std::error_code ec;
        if (fs::exists(rooted, ec) && !ec) {
            candidate = std::move(rooted);
        }
    }

    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(candidate, ec);
    if (!ec && !canonical.empty()) {
        candidate = canonical;
    } else {
        ec.clear();
        candidate = candidate.lexically_normal();
    }
    return candidate.u8string();
}

void MenuApp::update_last_chart_metadata(const std::string& chart_path,
                                         const SongEntry* preferred_entry) {
    last_chart_path_ = chart_path;
    last_chart_entry_ = {};
    last_chart_entry_valid_ = false;

    const auto target_keys = menu_songs::build_chart_path_keys(chart_path, songs_path_);
    const std::unordered_set<std::string> target_key_set(target_keys.begin(), target_keys.end());
    const auto matches_chart = [&](const SongEntry& entry) {
        for (const auto& key : menu_songs::build_chart_path_keys(song_absolute_path(entry), songs_path_)) {
            if (target_key_set.find(key) != target_key_set.end()) {
                return true;
            }
        }
        return false;
    };
    const auto assign_entry = [&](const SongEntry& entry) {
        last_chart_entry_ = entry;
        last_chart_entry_valid_ = true;
        last_chart_title_ = entry.title.empty()
                                ? menu_song_select::filename_only(chart_path)
                                : entry.title;
        last_chart_artist_ = entry.artist;
        last_chart_bpm_ = entry.bpm;
    };

    if (preferred_entry && matches_chart(*preferred_entry)) {
        assign_entry(*preferred_entry);
        return;
    }
    for (const auto& entry : indexed_songs_) {
        if (matches_chart(entry)) {
            assign_entry(entry);
            return;
        }
    }

    last_chart_title_ = menu_song_select::filename_only(chart_path);
    last_chart_artist_.clear();
    last_chart_bpm_ = 0.0;
}


std::string MenuApp::song_background_preview_path_for_entry(const SongEntry& entry) {
    return entry.background_preview_path;
}

std::string MenuApp::selected_song_background_preview_path() {
    const SongEntry* entry = (selected_song_ >= 0) ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                                   : nullptr;
    if (!entry) {
        return {};
    }
    return song_background_preview_path_for_entry(*entry);
}

void MenuApp::launch_selected_song() {
    if (multiplayer_selecting_chart_) {
        select_multiplayer_chart();
        return;
    }
    const std::string chart_path = selected_song_absolute_path();
    if (chart_path.empty()) {
        return;
    }
    launch_gameplay(chart_path);
}

} // namespace tenriff::app
