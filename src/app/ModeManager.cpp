#include "app/ModeManager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <limits>
#include <unordered_map>

#include "app/ModeResolver.h"
#include "gameplay/ModeApplier.h"

namespace tenriff::app {

namespace {

using gameplay::KeyMode;

std::string normalize_ascii_token(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isalnum(ch) != 0) {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return out;
}

const std::vector<ModeModDescriptor>& registry_storage() {
    static const std::vector<ModeModDescriptor> kRegistry = {
        {"judge_easy", "Judge Easy", "Judge Easy", "judge_window", "Judge Window", 0.85},
        {"judge_hard", "Judge Hard", "Judge Hard", "judge_window", "Judge Window", 1.10},
        {"full_long_notes", "Full Long Notes", "Full LN", "note_structure", "Note Structure", 1.00},
        {"ln_mix_10", "LN Mix 10%", "LN 10%", "note_structure", "Note Structure", 1.00, 10},
        {"ln_mix_20", "LN Mix 20%", "LN 20%", "note_structure", "Note Structure", 1.00, 20},
        {"ln_mix_30", "LN Mix 30%", "LN 30%", "note_structure", "Note Structure", 1.00, 30},
        {"ln_mix_40", "LN Mix 40%", "LN 40%", "note_structure", "Note Structure", 1.00, 40},
        {"ln_mix_50", "LN Mix 50%", "LN 50%", "note_structure", "Note Structure", 1.00, 50},
        {"ln_mix_60", "LN Mix 60%", "LN 60%", "note_structure", "Note Structure", 1.00, 60},
        {"ln_mix_70", "LN Mix 70%", "LN 70%", "note_structure", "Note Structure", 1.00, 70},
        {"ln_mix_80", "LN Mix 80%", "LN 80%", "note_structure", "Note Structure", 1.00, 80},
        {"ln_mix_90", "LN Mix 90%", "LN 90%", "note_structure", "Note Structure", 1.00, 90},
        {"full_short_notes", "Full Short Notes", "Full Tap", "note_structure", "Note Structure", 0.50},
        {"no_ln_release", "No LN Release", "No LN Release", "hold_rule", "Hold Rule", 0.90},
    };
    return kRegistry;
}

const std::unordered_map<std::string, const ModeModDescriptor*>& registry_lookup() {
    static const std::unordered_map<std::string, const ModeModDescriptor*> kLookup = [] {
        std::unordered_map<std::string, const ModeModDescriptor*> map;
        for (const auto& descriptor : registry_storage()) {
            map.emplace(normalize_ascii_token(descriptor.token), &descriptor);
        }
        return map;
    }();
    return kLookup;
}

const std::vector<ModeModCategoryDescriptor>& category_storage() {
    static const std::vector<ModeModCategoryDescriptor> kCategories = [] {
        std::vector<ModeModCategoryDescriptor> categories;
        for (const auto& descriptor : registry_storage()) {
            auto it = std::find_if(categories.begin(), categories.end(), [&](const ModeModCategoryDescriptor& item) {
                return item.token == descriptor.category_token;
            });
            if (it == categories.end()) {
                ModeModCategoryDescriptor category;
                category.token = descriptor.category_token;
                category.label = descriptor.category_label;
                category.mods.push_back(&descriptor);
                categories.push_back(std::move(category));
            } else {
                it->mods.push_back(&descriptor);
            }
        }
        return categories;
    }();
    return kCategories;
}

bool is_excluded_mod_token(std::string_view token) {
    const std::string normalized = normalize_ascii_token(token);
    return normalized == "removespeedchanges" ||
           normalized == "nospeedchanges" ||
           normalized == "speedchangeoff" ||
           normalized == "speedremove";
}

bool has_mod_token(const std::vector<std::string>& tokens, std::string_view token) {
    return std::find(tokens.begin(), tokens.end(), std::string(token)) != tokens.end();
}

int target_lane_count(KeyMode mode) {
    switch (mode) {
        case KeyMode::Keys4: return 4;
        case KeyMode::Keys5: return 5;
        case KeyMode::Keys6: return 6;
        case KeyMode::Keys7: return 7;
        case KeyMode::Keys8: return 8;
        case KeyMode::Keys9: return 9;
        case KeyMode::Keys10: return 10;
        case KeyMode::Keys16: return 16;
        case KeyMode::Auto:
        default:
            return 0;
    }
}

KeyMode key_mode_for_lane_count(int lane_count) {
    switch (lane_count) {
        case 4: return KeyMode::Keys4;
        case 5: return KeyMode::Keys5;
        case 6: return KeyMode::Keys6;
        case 7: return KeyMode::Keys7;
        case 8: return KeyMode::Keys8;
        case 9: return KeyMode::Keys9;
        case 10: return KeyMode::Keys10;
        case 16: return KeyMode::Keys16;
        default: return KeyMode::Auto;
    }
}

void apply_full_short_notes(gameplay::GameplayChart& chart) {
    for (auto& note : chart.notes) {
        note.end_sample.reset();
        note.release_required = false;
    }
}

void apply_no_ln_release(gameplay::GameplayChart& chart) {
    for (auto& note : chart.notes) {
        if (note.end_sample.has_value()) {
            note.release_required = false;
        }
    }
}

void apply_full_long_notes(gameplay::GameplayChart& chart) {
    if (chart.notes.empty()) {
        return;
    }

    int lane_count = chart.lane_count;
    if (lane_count <= 0) {
        for (const auto& note : chart.notes) {
            lane_count = std::max(lane_count, note.lane);
        }
    }
    if (lane_count <= 0) {
        return;
    }

    std::vector<std::vector<std::size_t>> lanes(static_cast<std::size_t>(lane_count) + 1u);
    for (std::size_t index = 0; index < chart.notes.size(); ++index) {
        const auto& note = chart.notes[index];
        if (note.lane <= 0 || note.lane > lane_count) {
            continue;
        }
        lanes[static_cast<std::size_t>(note.lane)].push_back(index);
    }

    for (int lane = 1; lane <= lane_count; ++lane) {
        auto& indices = lanes[static_cast<std::size_t>(lane)];
        std::stable_sort(indices.begin(), indices.end(), [&](std::size_t lhs, std::size_t rhs) {
            return chart.notes[lhs].start_sample < chart.notes[rhs].start_sample;
        });
        for (std::size_t position = 0; position < indices.size(); ++position) {
            auto& note = chart.notes[indices[position]];
            if (note.end_sample.has_value()) {
                continue;
            }

            int64_t end_sample = std::max<int64_t>(note.start_sample + 1, chart.duration_samples);
            if (position + 1 < indices.size()) {
                const int64_t next_start = chart.notes[indices[position + 1]].start_sample;
                end_sample = std::max<int64_t>(note.start_sample + 1, next_start - 1);
            }
            note.end_sample = end_sample;
            note.release_required = false;
            chart.duration_samples = std::max(chart.duration_samples, end_sample);
        }
    }
}

uint64_t mix_long_note_hash(uint64_t value) {
    // SplitMix64 keeps the chosen taps stable across standard-library and
    // compiler versions, unlike std::hash or a distribution implementation.
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30u)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27u)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31u);
}

int selected_long_note_mix_percent(const std::vector<std::string>& active_mods) {
    for (const auto& token : active_mods) {
        const auto* descriptor = find_mode_mod_descriptor(token);
        if (descriptor && descriptor->long_note_mix_percent > 0) {
            return descriptor->long_note_mix_percent;
        }
    }
    return 0;
}

struct LongNoteMixDurationCounts {
    std::size_t sixteenth = 0;
    std::size_t eighth = 0;
    std::size_t dense = 0;
};

// Largest-remainder allocation preserves the 70/20/10 target as closely as
// integer note counts allow, with stable tie-breaking toward common 1/16 lengths.
LongNoteMixDurationCounts allocate_long_note_mix_durations(std::size_t total) {
    constexpr std::array<std::size_t, 3> kWeights = {70u, 20u, 10u};
    std::array<std::size_t, 3> counts{};
    std::array<std::size_t, 3> remainders{};
    const std::size_t hundreds = total / 100u;
    const std::size_t tail = total % 100u;

    std::size_t assigned = 0;
    for (std::size_t i = 0; i < kWeights.size(); ++i) {
        counts[i] = hundreds * kWeights[i] + (tail * kWeights[i]) / 100u;
        remainders[i] = (tail * kWeights[i]) % 100u;
        assigned += counts[i];
    }
    while (assigned < total) {
        std::size_t best = 0;
        for (std::size_t i = 1; i < remainders.size(); ++i) {
            if (remainders[i] > remainders[best]) {
                best = i;
            }
        }
        ++counts[best];
        remainders[best] = 0;
        ++assigned;
    }
    return {counts[0], counts[1], counts[2]};
}

int64_t long_note_subdivision_samples(int sample_rate, double base_bpm, int subdivision) {
    constexpr int kFallbackSampleRate = 44100;
    constexpr double kFallbackBpm = 180.0;
    const int effective_sample_rate = sample_rate > 0 ? sample_rate : kFallbackSampleRate;
    const double effective_bpm =
        std::isfinite(base_bpm) && base_bpm > 0.0 ? base_bpm : kFallbackBpm;
    const double duration = static_cast<double>(effective_sample_rate) * 60.0 /
                            effective_bpm * 4.0 / static_cast<double>(subdivision);
    if (!std::isfinite(duration) || duration <= 1.0) {
        return 1;
    }
    const double safe_max = static_cast<double>((std::numeric_limits<int64_t>::max)() / 4);
    return static_cast<int64_t>(std::llround(std::min(duration, safe_max)));
}

void apply_mixed_long_notes(gameplay::GameplayChart& chart,
                            int percent,
                            uint32_t random_seed,
                            double base_bpm,
                            int sample_rate) {
    percent = std::clamp(percent, 0, 100);
    if (percent <= 0 || chart.notes.empty()) {
        return;
    }

    int lane_count = chart.lane_count;
    if (lane_count <= 0) {
        for (const auto& note : chart.notes) {
            lane_count = std::max(lane_count, note.lane);
        }
    }
    if (lane_count <= 0) {
        return;
    }

    struct Candidate {
        std::size_t note_index = 0;
        int64_t end_sample = 0;
        uint64_t selection_key = 0;
        uint64_t duration_key = 0;
    };

    std::vector<std::vector<std::size_t>> lanes(static_cast<std::size_t>(lane_count) + 1u);
    for (std::size_t index = 0; index < chart.notes.size(); ++index) {
        const auto& note = chart.notes[index];
        if (note.lane > 0 && note.lane <= lane_count) {
            lanes[static_cast<std::size_t>(note.lane)].push_back(index);
        }
    }

    constexpr int kFallbackSampleRate = 44100;
    const int effective_sample_rate = sample_rate > 0 ? sample_rate : kFallbackSampleRate;
    const int64_t same_lane_clearance_samples =
        std::max<int64_t>(1, static_cast<int64_t>(std::llround(effective_sample_rate * 0.05)));
    const int64_t eighth_samples = long_note_subdivision_samples(sample_rate, base_bpm, 8);
    const int64_t sixteenth_samples = long_note_subdivision_samples(sample_rate, base_bpm, 16);
    const int64_t twenty_fourth_samples = long_note_subdivision_samples(sample_rate, base_bpm, 24);
    const int64_t thirty_second_samples = long_note_subdivision_samples(sample_rate, base_bpm, 32);
    std::vector<Candidate> candidates;
    candidates.reserve(chart.notes.size());

    for (int lane = 1; lane <= lane_count; ++lane) {
        auto& indices = lanes[static_cast<std::size_t>(lane)];
        std::stable_sort(indices.begin(), indices.end(), [&](std::size_t lhs, std::size_t rhs) {
            if (chart.notes[lhs].start_sample != chart.notes[rhs].start_sample) {
                return chart.notes[lhs].start_sample < chart.notes[rhs].start_sample;
            }
            return lhs < rhs;
        });

        bool has_prior_lane_span = false;
        int64_t prior_lane_end = 0;
        for (std::size_t position = 0; position < indices.size(); ++position) {
            const std::size_t note_index = indices[position];
            const auto& note = chart.notes[note_index];
            const int64_t original_end =
                std::max(note.start_sample, note.end_sample.value_or(note.start_sample));
            const bool overlaps_prior_lane_span =
                has_prior_lane_span && note.start_sample <= prior_lane_end;
            prior_lane_end = has_prior_lane_span ? std::max(prior_lane_end, original_end) : original_end;
            has_prior_lane_span = true;

            if (note.end_sample.has_value() || overlaps_prior_lane_span) {
                continue;
            }

            int64_t end_sample = chart.duration_samples;
            if (position + 1 < indices.size()) {
                // A one-sample tail-to-head gap still renders and plays like an
                // overlap. Keep a short release/repress lane between generated
                // holds and the next note while preserving taps on other lanes.
                end_sample =
                    chart.notes[indices[position + 1]].start_sample - same_lane_clearance_samples;
            }
            // Every candidate must fit the longest bucket so the requested ratio never
            // changes through tail clamping on dense same-lane patterns.
            if (end_sample <= note.start_sample || end_sample - note.start_sample < eighth_samples) {
                continue;
            }

            uint64_t material = static_cast<uint64_t>(random_seed);
            material ^= static_cast<uint64_t>(note.start_sample) + 0x9E3779B97F4A7C15ull;
            material ^= static_cast<uint64_t>(note.lane) * 0xBF58476D1CE4E5B9ull;
            material ^= static_cast<uint64_t>(note_index) * 0x94D049BB133111EBull;
            const uint64_t selection_key = mix_long_note_hash(material);
            const uint64_t duration_key =
                mix_long_note_hash(selection_key ^ 0xD1B54A32D192ED03ull);
            candidates.push_back(Candidate{note_index, end_sample, selection_key, duration_key});
        }
    }

    const std::size_t selected_count = std::min<std::size_t>(
        candidates.size(),
        static_cast<std::size_t>(std::llround(static_cast<double>(candidates.size()) *
                                              static_cast<double>(percent) / 100.0)));
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.selection_key != rhs.selection_key) {
            return lhs.selection_key < rhs.selection_key;
        }
        return lhs.note_index < rhs.note_index;
    });

    std::vector<Candidate> selected(candidates.begin(), candidates.begin() + selected_count);
    std::sort(selected.begin(), selected.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.duration_key != rhs.duration_key) {
            return lhs.duration_key < rhs.duration_key;
        }
        return lhs.note_index < rhs.note_index;
    });
    const LongNoteMixDurationCounts duration_counts =
        allocate_long_note_mix_durations(selected.size());

    for (std::size_t i = 0; i < selected.size(); ++i) {
        const Candidate& candidate = selected[i];
        auto& note = chart.notes[candidate.note_index];
        int64_t duration_samples = sixteenth_samples;
        if (i >= duration_counts.sixteenth &&
            i < duration_counts.sixteenth + duration_counts.eighth) {
            duration_samples = eighth_samples;
        } else if (i >= duration_counts.sixteenth + duration_counts.eighth) {
            // Alternate the dense bucket between musical 1/24 and 1/32 lengths.
            const std::size_t dense_index =
                i - duration_counts.sixteenth - duration_counts.eighth;
            duration_samples = dense_index % 2u == 0u
                                   ? twenty_fourth_samples
                                   : thirty_second_samples;
        }
        note.end_sample = std::min(candidate.end_sample, note.start_sample + duration_samples);
        note.release_required = false;
    }
}

void scale_judge_windows(config::JudgeConfig& judge, double scale) {
    if (!std::isfinite(scale) || scale <= 0.0 || std::abs(scale - 1.0) < 1e-9) {
        return;
    }
    judge.pg_ms *= scale;
    judge.gr_ms *= scale;
    judge.gd_ms *= scale;
    judge.bd_ms *= scale;
    judge.hold_grace_ms *= scale;
    judge.hold_break_ms *= scale;
    judge.hold_break_ms = std::max(judge.hold_break_ms, judge.hold_grace_ms);
}

std::string join_labels(const std::vector<std::string>& labels) {
    if (labels.empty()) {
        return "Off";
    }
    std::string joined;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (i > 0) {
            joined += " + ";
        }
        joined += labels[i];
    }
    return joined;
}

}  // namespace

const std::vector<ModeModDescriptor>& mode_mod_registry() {
    return registry_storage();
}

const std::vector<ModeModCategoryDescriptor>& mode_mod_categories() {
    return category_storage();
}

const ModeModDescriptor* find_mode_mod_descriptor(std::string_view token) {
    const auto& lookup = registry_lookup();
    const auto it = lookup.find(normalize_ascii_token(token));
    return (it == lookup.end()) ? nullptr : it->second;
}

std::vector<std::string> normalize_mode_mod_tokens(const std::vector<std::string>& tokens,
                                                   std::vector<std::string>* warnings) {
    std::unordered_map<std::string, std::string> selected_by_category;

    for (const auto& token : tokens) {
        const ModeModDescriptor* descriptor = find_mode_mod_descriptor(token);
        if (!descriptor) {
            if (is_excluded_mod_token(token)) {
                if (warnings) {
                    warnings->push_back("Mod \"" + token + "\" is excluded in the current build.");
                }
            } else if (warnings) {
                warnings->push_back("Mod \"" + token + "\" is not recognized and was ignored.");
            }
            continue;
        }

        auto existing = selected_by_category.find(std::string(descriptor->category_token));
        if (existing != selected_by_category.end() && existing->second == descriptor->token) {
            if (warnings) {
                warnings->push_back("Duplicate mod \"" + std::string(descriptor->token) + "\" was ignored.");
            }
            continue;
        }
        selected_by_category[std::string(descriptor->category_token)] = std::string(descriptor->token);
    }

    std::vector<std::string> active;
    active.reserve(selected_by_category.size());
    for (const auto& descriptor : registry_storage()) {
        auto selected = selected_by_category.find(std::string(descriptor.category_token));
        if (selected != selected_by_category.end() && selected->second == descriptor.token) {
            active.push_back(std::string(descriptor.token));
        }
    }

    const auto no_ln_release_it = std::find(active.begin(), active.end(), "no_ln_release");
    if (has_mod_token(active, "full_short_notes") && no_ln_release_it != active.end()) {
        if (warnings) {
            warnings->push_back("Mod \"no_ln_release\" was removed because \"full_short_notes\" has no hold tails.");
        }
        active.erase(no_ln_release_it);
    }

    return active;
}

bool equivalent_mode_mod_tokens(const std::vector<std::string>& lhs,
                                const std::vector<std::string>& rhs) {
    return normalize_mode_mod_tokens(lhs) == normalize_mode_mod_tokens(rhs);
}

std::string mode_mod_summary(const std::vector<std::string>& tokens) {
    const std::vector<std::string> normalized = normalize_mode_mod_tokens(tokens);
    std::vector<std::string> labels;
    labels.reserve(normalized.size());
    for (const auto& token : normalized) {
        if (const auto* descriptor = find_mode_mod_descriptor(token)) {
            labels.emplace_back(descriptor->short_label);
        }
    }
    return join_labels(labels);
}

std::string mode_mod_category_value(std::string_view category_token,
                                    const std::vector<std::string>& tokens) {
    const std::vector<std::string> normalized = normalize_mode_mod_tokens(tokens);
    for (const auto& token : normalized) {
        const auto* descriptor = find_mode_mod_descriptor(token);
        if (!descriptor || descriptor->category_token != category_token) {
            continue;
        }
        return std::string(descriptor->label);
    }
    return "Off";
}

double rate_score_multiplier(double rate) {
    if (!std::isfinite(rate)) {
        return 1.0;
    }
    if (std::abs(rate - 1.0) < 1e-9) {
        return 1.0;
    }
    if (rate < 1.0) {
        const int steps = static_cast<int>(std::llround((1.0 - rate) / 0.05));
        if (steps <= 0) {
            return 1.0;
        }
        const int clamped_steps = std::clamp(steps, 1, 10);
        return 0.75 - static_cast<double>(clamped_steps - 1) * (0.25 / 9.0);
    }

    const int steps = static_cast<int>(std::llround((rate - 1.0) / 0.05));
    if (steps <= 0) {
        return 1.0;
    }
    const int clamped_steps = std::clamp(steps, 1, 20);
    return 1.05 + static_cast<double>(clamped_steps - 1) * (0.10 / 19.0);
}

double mod_score_multiplier(const std::vector<std::string>& tokens) {
    const std::vector<std::string> normalized = normalize_mode_mod_tokens(tokens);
    double multiplier = std::numeric_limits<double>::infinity();
    for (const auto& token : normalized) {
        if (const auto* descriptor = find_mode_mod_descriptor(token)) {
            multiplier = std::min(multiplier, descriptor->score_multiplier);
        }
    }
    if (!std::isfinite(multiplier)) {
        return 1.0;
    }
    return multiplier;
}

double final_score_multiplier(const std::vector<std::string>& tokens, double rate) {
    return std::min(rate_score_multiplier(rate), mod_score_multiplier(tokens));
}

ModeManagerResult manage_modes(const gameplay::GameplayChart& chart,
                               ChartFormat chart_format,
                               const config::ModeConfig& config,
                               const config::JudgeConfig& judge,
                               double rate,
                               double base_bpm,
                               int sample_rate) {
    ModeManagerResult result;
    result.chart = chart;
    result.judge = judge;

    const ModeResolveResult resolved = resolve_mode_settings(config);
    result.settings = resolved.settings;
    result.warnings.insert(result.warnings.end(), resolved.warnings.begin(), resolved.warnings.end());

    const auto applied = gameplay::apply_mode_settings(result.chart, result.settings, {base_bpm, sample_rate});
    result.chart = applied.chart;
    result.warnings.insert(result.warnings.end(), applied.warnings.begin(), applied.warnings.end());

    result.active_mods = normalize_mode_mod_tokens(config.mods, &result.warnings);

    if (has_mod_token(result.active_mods, "full_short_notes")) {
        apply_full_short_notes(result.chart);
    } else {
        if (has_mod_token(result.active_mods, "full_long_notes")) {
            apply_full_long_notes(result.chart);
        } else if (const int mix_percent = selected_long_note_mix_percent(result.active_mods);
                   mix_percent > 0) {
            apply_mixed_long_notes(
                result.chart, mix_percent, config.random_seed, base_bpm, sample_rate);
        }
        if (has_mod_token(result.active_mods, "no_ln_release")) {
            apply_no_ln_release(result.chart);
        }
    }

    if (has_mod_token(result.active_mods, "judge_easy")) {
        result.judge_window_scale = 1.25;
    } else if (has_mod_token(result.active_mods, "judge_hard")) {
        result.judge_window_scale = 0.85;
    }
    scale_judge_windows(result.judge, result.judge_window_scale);

    result.rate_multiplier = rate_score_multiplier(rate);
    result.mod_multiplier = mod_score_multiplier(result.active_mods);
    result.final_multiplier = std::min(result.rate_multiplier, result.mod_multiplier);
    return result;
}

}  // namespace tenriff::app
