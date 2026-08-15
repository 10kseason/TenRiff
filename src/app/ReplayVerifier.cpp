#include "app/ReplayVerifier.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <system_error>
#include <utility>

#include "app/ChartFileHash.h"
#include "app/ModeManager.h"
#include "app/SessionResultStatus.h"
#include "gameplay/GameplayEngine.h"
#include "gameplay/ModeSettings.h"

namespace tenriff::app {

namespace {

bool close_enough(double lhs, double rhs, double epsilon = 1e-9) {
    return std::isfinite(lhs) && std::isfinite(rhs) && std::abs(lhs - rhs) <= epsilon;
}

bool equal_delta_table(const game::GaugeDeltaTable& lhs, const game::GaugeDeltaTable& rhs) {
    return close_enough(lhs.pg, rhs.pg) && close_enough(lhs.gr, rhs.gr) &&
           close_enough(lhs.gd, rhs.gd) && close_enough(lhs.bd, rhs.bd) &&
           close_enough(lhs.pr, rhs.pr);
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string gauge_token(game::GaugeType type) {
    switch (type) {
        case game::GaugeType::ExHard: return "ex_hard";
        case game::GaugeType::Hard: return "hard";
        case game::GaugeType::Easy: return "easy";
        case game::GaugeType::Normal:
        default: return "normal";
    }
}

game::GaugeType initial_gauge_for(gameplay::GaugeMode mode) {
    switch (mode) {
        case gameplay::GaugeMode::ExHard:
        case gameplay::GaugeMode::Shift:
            return game::GaugeType::ExHard;
        case gameplay::GaugeMode::Hard:
            return game::GaugeType::Hard;
        case gameplay::GaugeMode::Easy:
            return game::GaugeType::Easy;
        case gameplay::GaugeMode::Normal:
        default:
            return game::GaugeType::Normal;
    }
}

bool key_claims_match(const gameplay::ReplayFile& replay,
                      const gameplay::ResultStats& recomputed,
                      int64_t final_score) {
    const auto& stored = replay.stats;
    return replay.final_score == final_score &&
           stored.raw_score == recomputed.raw_score &&
           stored.detail_score == recomputed.detail_score &&
           stored.total_notes == recomputed.total_notes &&
           stored.total_combo_steps == recomputed.total_combo_steps &&
           stored.max_combo == recomputed.max_combo &&
           stored.counts.pg == recomputed.counts.pg &&
           stored.counts.gr == recomputed.counts.gr &&
           stored.counts.gd == recomputed.counts.gd &&
           stored.counts.bd == recomputed.counts.bd &&
           stored.counts.pr == recomputed.counts.pr;
}

ReplayVerificationResult invalid_result(std::string detail,
                                        ReplayVerificationStatus status = ReplayVerificationStatus::Invalid) {
    ReplayVerificationResult result;
    result.status = status;
    result.detail = std::move(detail);
    return result;
}

}  // namespace

std::string_view replay_verification_status_token(ReplayVerificationStatus status) {
    switch (status) {
        case ReplayVerificationStatus::Verified: return "verified";
        case ReplayVerificationStatus::LegacyUnverified: return "legacy-unverified";
        case ReplayVerificationStatus::CustomRuleset: return "custom-ruleset";
        case ReplayVerificationStatus::MissingEvidence: return "missing-evidence";
        case ReplayVerificationStatus::Invalid:
        default: return "invalid";
    }
}

bool is_canonical_score_ruleset(const config::JudgeConfig& judge,
                                const game::GaugeConfig& gauge) {
    const config::JudgeConfig expected_judge{};
    const game::GaugeConfig expected_gauge{};
    return close_enough(judge.pg_ms, expected_judge.pg_ms) &&
           close_enough(judge.gr_ms, expected_judge.gr_ms) &&
           close_enough(judge.gd_ms, expected_judge.gd_ms) &&
           close_enough(judge.bd_ms, expected_judge.bd_ms) &&
           close_enough(judge.indirect_miss_ms, expected_judge.indirect_miss_ms) &&
           judge.indirect_miss_enabled == expected_judge.indirect_miss_enabled &&
           close_enough(judge.hold_grace_ms, expected_judge.hold_grace_ms) &&
           close_enough(judge.hold_break_ms, expected_judge.hold_break_ms) &&
           close_enough(judge.mask_ms, expected_judge.mask_ms) &&
           equal_delta_table(gauge.ex_hard, expected_gauge.ex_hard) &&
           equal_delta_table(gauge.hard, expected_gauge.hard) &&
           equal_delta_table(gauge.normal, expected_gauge.normal) &&
           equal_delta_table(gauge.easy, expected_gauge.easy);
}

std::string replay_ruleset_id_for_runtime(const config::JudgeConfig& judge,
                                          const game::GaugeConfig& gauge,
                                          bool standard_gauge_shift,
                                          bool course_gauge,
                                          std::string_view pacemaker_mode) {
    if (!standard_gauge_shift || course_gauge || pacemaker_mode_active(pacemaker_mode) ||
        !is_canonical_score_ruleset(judge, gauge)) {
        return "custom";
    }
    return std::string(kCanonicalReplayRulesetId);
}

ReplayVerificationResult verify_replay_against_chart(
    const gameplay::ReplayFile& replay,
    const gameplay::GameplayChart& source_chart,
    ChartFormat chart_format,
    double base_bpm) {
    if (replay.replay_format_version < gameplay::kReplayFormatVersion) {
        return invalid_result("Legacy replay has no verifiable chart/ruleset evidence.",
                              ReplayVerificationStatus::LegacyUnverified);
    }
    const gameplay::ReplayValidationResult validation = gameplay::validate_replay_evidence(replay);
    if (!validation.success()) {
        return invalid_result(validation.error);
    }
    if (replay.ruleset_id != kCanonicalReplayRulesetId) {
        return invalid_result("Replay used a non-canonical score ruleset.",
                              ReplayVerificationStatus::CustomRuleset);
    }
    if (replay.mode.key_conversion_note_add_mode.size() > 0 &&
        replay.mode.key_conversion_note_add_mode != "default") {
        return invalid_result("Replay depends on a removed key-conversion mode.");
    }

    config::ModeConfig mode;
    mode.key_mode = replay.mode.key_mode.empty() ? "auto" : replay.mode.key_mode;
    mode.key_conversion_algorithm = replay.mode.key_conversion_algorithm.empty()
                                        ? "krrcream"
                                        : replay.mode.key_conversion_algorithm;
    mode.key_conversion_nk2_preset = replay.mode.key_conversion_nk2_preset.empty()
                                         ? "native"
                                         : replay.mode.key_conversion_nk2_preset;
    mode.gauge = replay.mode.gauge.empty() ? "normal" : replay.mode.gauge;
    mode.random = replay.mode.random.empty() ? "off" : replay.mode.random;
    mode.random_seed = static_cast<uint32_t>(replay.mode.random_seed.value_or(0));
    mode.mods = replay.mods;
    mode.autoplay_enabled = false;
    mode.practice_no_fail_enabled = replay.mode.practice_no_fail_enabled;
    mode.one_miss_fail_enabled = replay.mode.one_miss_fail_enabled;

    const config::JudgeConfig canonical_judge{};
    const game::GaugeConfig canonical_gauge{};
    const ModeManagerResult managed = manage_modes(source_chart,
                                                   chart_format,
                                                   mode,
                                                   canonical_judge,
                                                   replay.rate,
                                                   base_bpm,
                                                   replay.sample_rate);
    if (!equivalent_mode_mod_tokens(managed.active_mods, replay.mods)) {
        return invalid_result("Replay mods do not normalize to the recorded mode set.");
    }
    if (!close_enough(managed.rate_multiplier, replay.rate_multiplier) ||
        !close_enough(managed.final_multiplier, replay.score_multiplier)) {
        return invalid_result("Replay score multipliers do not match the canonical ruleset.");
    }

    gameplay::GameplayChart chart = managed.chart;
    const int64_t lead_in_samples = static_cast<int64_t>(std::llround(
        static_cast<double>(kReplayVerificationLeadInMs) *
        static_cast<double>(replay.sample_rate) / 1000.0));
    gameplay::offset_gameplay_chart_samples(chart, lead_in_samples);
    if (chart.lane_count != replay.trace.lane_count ||
        chart.duration_samples != replay.trace.duration_samples) {
        return invalid_result("Replay trace shape does not match the reconstructed chart.");
    }

    gameplay::GameplayConfig engine_config;
    engine_config.sample_rate = replay.sample_rate;
    engine_config.rate = replay.rate;
    engine_config.judge = managed.judge;
    engine_config.gauge = canonical_gauge;
    engine_config.initial_gauge = initial_gauge_for(managed.settings.gauge);
    engine_config.gauge_shift_enabled = true;
    engine_config.input_offset_ms = replay.input_offset_ms;
    engine_config.practice_no_fail_enabled = replay.mode.practice_no_fail_enabled;
    engine_config.one_miss_fail_enabled = replay.mode.one_miss_fail_enabled;

    gameplay::GameplayEngine engine(chart, engine_config);
    std::size_t index = 0;
    while (index < replay.trace.events.size()) {
        const int64_t sample = replay.trace.events[index].sample;
        if (sample > 0) {
            engine.advance(sample - 1);
        }
        while (index < replay.trace.events.size() && replay.trace.events[index].sample == sample) {
            const auto& event = replay.trace.events[index];
            (void)engine.handle_input(event.lane, event.state, event.sample);
            ++index;
        }
    }
    const int64_t finish_slack = static_cast<int64_t>(replay.sample_rate) * 5;
    engine.advance(chart.duration_samples + finish_slack);

    ReplayVerificationResult result;
    result.status = ReplayVerificationStatus::Verified;
    result.stats = engine.stats();
    result.rate_multiplier = managed.rate_multiplier;
    result.score_multiplier = managed.final_multiplier;
    result.final_score = gameplay::scale_native_score(result.stats.raw_score,
                                                       result.score_multiplier);
    result.final_gauge = gauge_token(engine.gauge_state().type);
    result.final_gauge_value = engine.gauge_state().value;
    result.clear_status = gameplay_session_clear_status(
        engine.is_finished(),
        engine.is_game_over(),
        replay.aborted,
        engine.gauge_state().type,
        replay.mode.autoplay_enabled,
        replay.mode.practice_no_fail_enabled,
        replay.mode.one_miss_fail_enabled,
        true);
    result.game_over = !gameplay_session_cleared(
        engine.is_finished(), engine.is_game_over(), replay.aborted, replay.mode.autoplay_enabled);
    result.claims_match = key_claims_match(replay, result.stats, result.final_score);
    result.official_eligible = (engine.is_finished() || engine.is_game_over()) &&
                               !replay.aborted &&
                               !replay.mode.autoplay_enabled &&
                               !replay.mode.practice_no_fail_enabled &&
                               !mode_mod_adds_notes(replay.mods);
    result.detail = result.claims_match
                        ? "Replay outcome reproduced under the canonical ruleset."
                        : "Replay outcome reproduced; stored score/stat claims were ignored because they differ.";
    return result;
}

ReplayVerificationResult verify_replay_file(
    const std::filesystem::path& replay_path,
    const std::filesystem::path& expected_chart_path,
    std::string_view expected_replay_sha256) {
    if (replay_path.empty()) {
        return invalid_result("Result does not reference a replay.",
                              ReplayVerificationStatus::MissingEvidence);
    }
    std::string hash_error;
    const ChartFileHashes replay_hashes = hash_chart_file(replay_path, &hash_error);
    if (!replay_hashes.valid()) {
        return invalid_result(hash_error.empty() ? "Replay file is missing or unreadable." : hash_error,
                              ReplayVerificationStatus::MissingEvidence);
    }
    if (!expected_replay_sha256.empty() &&
        lower_ascii(replay_hashes.sha256) != lower_ascii(std::string(expected_replay_sha256))) {
        return invalid_result("Replay file SHA-256 does not match the result binding.");
    }

    const gameplay::ReplayLoadResult loaded = gameplay::load_replay_json(replay_path.u8string());
    if (!loaded.success()) {
        return invalid_result(loaded.error.empty() ? "Replay could not be parsed." : loaded.error);
    }
    const gameplay::ReplayFile& replay = *loaded.replay;
    if (replay.replay_format_version < gameplay::kReplayFormatVersion) {
        return invalid_result("Legacy replay has no chart/ruleset binding.",
                              ReplayVerificationStatus::LegacyUnverified);
    }

    std::filesystem::path chart_path = expected_chart_path;
    if (chart_path.empty()) {
        try {
            chart_path = std::filesystem::u8path(replay.chart_path);
        } catch (...) {
            return invalid_result("Replay chart path is not valid UTF-8.");
        }
    }
    const ChartFileHashes chart_hashes = hash_chart_file(chart_path, &hash_error);
    if (!chart_hashes.valid()) {
        return invalid_result(hash_error.empty() ? "Bound chart is missing or unreadable." : hash_error,
                              ReplayVerificationStatus::MissingEvidence);
    }
    if (lower_ascii(chart_hashes.sha256) != lower_ascii(replay.chart_sha256)) {
        return invalid_result("Replay chart SHA-256 does not match the selected chart bytes.");
    }

    ChartLoader loader;
    const uint32_t bms_random_seed = static_cast<uint32_t>(replay.mode.random_seed.value_or(0));
    const ChartLoadResult loaded_chart = loader.load(chart_path.u8string(),
                                                     replay.sample_rate,
                                                     replay.rate,
                                                     "ignore",
                                                     bms_random_seed);
    if (!loaded_chart.success()) {
        return invalid_result(loaded_chart.error.empty()
                                  ? "Bound chart could not be reconstructed."
                                  : loaded_chart.error);
    }
    ReplayVerificationResult result = verify_replay_against_chart(
        replay, loaded_chart.chart, loaded_chart.format, loaded_chart.base_bpm);
    result.chart_sha256 = chart_hashes.sha256;
    result.replay_sha256 = replay_hashes.sha256;
    return result;
}

}  // namespace tenriff::app
