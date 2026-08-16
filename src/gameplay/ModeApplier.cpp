#include "gameplay/ModeApplier.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <random>
#include <utility>

#include "gameplay/KeyModeConverter.h"

namespace tenriff::gameplay {

namespace {

void sort_notes_for_gameplay(GameplayChart& chart) {
    std::stable_sort(chart.notes.begin(), chart.notes.end(), [](const NoteEvent& lhs, const NoteEvent& rhs) {
        if (lhs.start_sample != rhs.start_sample) {
            return lhs.start_sample < rhs.start_sample;
        }
        if (lhs.lane != rhs.lane) {
            return lhs.lane < rhs.lane;
        }
        const int64_t lhs_end = lhs.end_sample.value_or(lhs.start_sample);
        const int64_t rhs_end = rhs.end_sample.value_or(rhs.start_sample);
        return lhs_end < rhs_end;
    });
}

void sort_audio_cues(GameplayChart& chart) {
    std::stable_sort(chart.audio_cues.begin(), chart.audio_cues.end(),
                     [](const AudioCueEvent& lhs, const AudioCueEvent& rhs) {
                         if (lhs.start_sample != rhs.start_sample) {
                             return lhs.start_sample < rhs.start_sample;
                         }
                         return lhs.asset_id < rhs.asset_id;
                     });
}

int resolve_lane_count(const GameplayChart& chart) {
    int lane_count = chart.lane_count;
    if (lane_count > 0) {
        return lane_count;
    }
    for (const auto& note : chart.notes) {
        lane_count = std::max(lane_count, note.lane);
    }
    return lane_count > 0 ? lane_count : 10;
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
        case KeyMode::Keys12: return 12;
        case KeyMode::Keys14: return 14;
        case KeyMode::Keys16: return 16;
        case KeyMode::Auto: default: return 0;
    }
}

uint32_t normalize_seed(uint32_t seed) {
    return seed;
}

bool is_scratch_lane(const GameplayChart& chart, int lane) {
    return std::find(chart.scratch_lanes.begin(), chart.scratch_lanes.end(), lane) !=
           chart.scratch_lanes.end();
}

int effective_group_size(const GameplayChart& chart) {
    if (chart.lane_group_size > 0 && chart.lane_count == chart.lane_group_size * 2) {
        return chart.lane_group_size;
    }
    // Preserve the established dual-player behavior for legacy 10K/16K charts
    // that predate explicit layout metadata.
    if ((chart.lane_count == 10 || chart.lane_count == 16) && chart.scratch_lanes.empty()) {
        return chart.lane_count / 2;
    }
    return 0;
}

std::vector<std::vector<int>> playable_lane_groups(const GameplayChart& chart) {
    const int group_size = effective_group_size(chart);
    const int group_count = group_size > 0 ? 2 : 1;
    std::vector<std::vector<int>> groups(static_cast<std::size_t>(group_count));

    for (int lane = 1; lane <= chart.lane_count; ++lane) {
        if (is_scratch_lane(chart, lane)) {
            continue;
        }
        const int group = group_size > 0 ? std::min(1, (lane - 1) / group_size) : 0;
        groups[static_cast<std::size_t>(group)].push_back(lane);
    }
    return groups;
}

void apply_lane_map(GameplayChart& chart, const std::vector<int>& lane_map) {
    for (auto& note : chart.notes) {
        if (note.lane > 0 && note.lane < static_cast<int>(lane_map.size())) {
            note.lane = lane_map[static_cast<std::size_t>(note.lane)];
        }
    }
    for (auto& mine : chart.mines) {
        if (mine.lane > 0 && mine.lane < static_cast<int>(lane_map.size())) {
            mine.lane = lane_map[static_cast<std::size_t>(mine.lane)];
        }
    }
    for (auto& scratch_lane : chart.scratch_lanes) {
        if (scratch_lane > 0 && scratch_lane < static_cast<int>(lane_map.size())) {
            scratch_lane = lane_map[static_cast<std::size_t>(scratch_lane)];
        }
    }
    std::sort(chart.scratch_lanes.begin(), chart.scratch_lanes.end());
    sort_notes_for_gameplay(chart);
}

struct ScratchExtraction {
    GameplayChart chart;
    std::size_t scratch_note_count = 0;
    std::size_t autoplay_cue_count = 0;
};

ScratchExtraction extract_key_part(const GameplayChart& source) {
    ScratchExtraction result;
    result.chart = source;

    std::vector<int> scratches = source.scratch_lanes;
    scratches.erase(std::remove_if(scratches.begin(), scratches.end(), [&](int lane) {
        return lane <= 0 || lane > source.lane_count;
    }), scratches.end());
    std::sort(scratches.begin(), scratches.end());
    scratches.erase(std::unique(scratches.begin(), scratches.end()), scratches.end());

    std::vector<int> lane_map(static_cast<std::size_t>(source.lane_count + 1), 0);
    int next_lane = 0;
    for (int lane = 1; lane <= source.lane_count; ++lane) {
        if (!std::binary_search(scratches.begin(), scratches.end(), lane)) {
            lane_map[static_cast<std::size_t>(lane)] = ++next_lane;
        }
    }

    std::vector<NoteEvent> key_notes;
    key_notes.reserve(source.notes.size());
    for (const auto& source_note : source.notes) {
        if (std::binary_search(scratches.begin(), scratches.end(), source_note.lane)) {
            ++result.scratch_note_count;
            const std::size_t asset_count = note_audio_asset_count(source_note);
            for (std::size_t asset_index = 0; asset_index < asset_count; ++asset_index) {
                const std::size_t asset_id = note_audio_asset_at(source_note, asset_index);
                if (asset_id == kInvalidAudioAssetId) {
                    continue;
                }
                result.chart.audio_cues.push_back(AudioCueEvent{source_note.start_sample, asset_id});
                ++result.autoplay_cue_count;
            }
            continue;
        }

        if (source_note.lane <= 0 || source_note.lane > source.lane_count) {
            continue;
        }
        NoteEvent key_note = source_note;
        key_note.lane = lane_map[static_cast<std::size_t>(source_note.lane)];
        key_notes.push_back(std::move(key_note));
    }

    std::vector<MineEvent> key_mines;
    key_mines.reserve(source.mines.size());
    for (const auto& source_mine : source.mines) {
        if (source_mine.lane <= 0 || source_mine.lane > source.lane_count ||
            std::binary_search(scratches.begin(), scratches.end(), source_mine.lane)) {
            continue;
        }
        MineEvent mine = source_mine;
        mine.lane = lane_map[static_cast<std::size_t>(source_mine.lane)];
        key_mines.push_back(std::move(mine));
    }
    result.chart.mines = std::move(key_mines);
    result.chart.notes = std::move(key_notes);
    result.chart.lane_count = next_lane;
    result.chart.scratch_lanes.clear();

    const int source_group_size = effective_group_size(source);
    if (source_group_size > 0) {
        const auto left_scratch_count = static_cast<int>(std::count_if(
            scratches.begin(), scratches.end(), [source_group_size](int lane) {
                return lane <= source_group_size;
            }));
        const int right_scratch_count = static_cast<int>(scratches.size()) - left_scratch_count;
        if (left_scratch_count == right_scratch_count &&
            source_group_size > left_scratch_count &&
            next_lane == (source_group_size - left_scratch_count) * 2) {
            result.chart.lane_group_size = source_group_size - left_scratch_count;
        } else {
            result.chart.lane_group_size = 0;
        }
    }

    sort_notes_for_gameplay(result.chart);
    sort_audio_cues(result.chart);
    return result;
}

KeyModeConverterOptions default_converter_options(int source_lane_count,
                                                  int target_lane_count,
                                                  uint32_t seed,
                                                  KeyModeConversionAlgorithm algorithm,
                                                  Nk2Preset nk2_preset,
                                                  const ModeApplyContext& context) {
    KeyModeConverterOptions options;
    options.target_lane_count = target_lane_count;
    options.seed = normalize_seed(seed);
    options.base_bpm = context.base_bpm;
    options.sample_rate = context.sample_rate;
    options.algorithm = algorithm;
    options.nk2_preset = nk2_preset;

    if (target_lane_count == 10) {
        options.max_keys = 10;
        options.min_keys = 1;
        options.transform_speed_slot = 5;
        options.seed = 0;
        return options;
    }

    if (target_lane_count >= 8 && target_lane_count > source_lane_count && source_lane_count <= 7) {
        options.max_keys = std::max(1, source_lane_count);
        options.min_keys = std::max(1, source_lane_count);
        options.transform_speed_slot = 2;
        return options;
    }

    if (target_lane_count <= 4) {
        options.max_keys = target_lane_count;
        options.min_keys = target_lane_count;
        options.transform_speed_slot = 2;
        return options;
    }

    if (target_lane_count <= 6) {
        options.max_keys = target_lane_count;
        options.min_keys = std::min(target_lane_count, 4);
        options.transform_speed_slot = 2;
        return options;
    }

    options.max_keys = std::min(target_lane_count, 8);
    options.min_keys = 2;
    options.transform_speed_slot = 4;
    return options;
}

bool convert_grouped_chart(const GameplayChart& source,
                           int target_count,
                           uint32_t seed,
                           KeyModeConversionAlgorithm algorithm,
                           Nk2Preset nk2_preset,
                           const ModeApplyContext& context,
                           GameplayChart& output,
                           std::vector<std::string>& warnings) {
    const int source_group_size = effective_group_size(source);
    if (source_group_size <= 0 || source.lane_count != source_group_size * 2 ||
        target_count <= 0 || target_count % 2 != 0) {
        return false;
    }

    const int target_group_size = target_count / 2;
    output = source;
    output.notes.clear();
    output.lane_count = target_count;
    output.lane_group_size = target_group_size;
    output.scratch_lanes.clear();

    for (int group = 0; group < 2; ++group) {
        GameplayChart half = source;
        half.notes.clear();
        half.audio_cues.clear();
        half.visual_cues.clear();
        half.lane_count = source_group_size;
        half.lane_group_size = 0;
        half.scratch_lanes.clear();

        const int source_start = group * source_group_size + 1;
        const int source_end = source_start + source_group_size - 1;
        for (const auto& note : source.notes) {
            if (note.lane < source_start || note.lane > source_end) {
                continue;
            }
            NoteEvent local_note = note;
            local_note.lane -= group * source_group_size;
            half.notes.push_back(std::move(local_note));
        }

        GameplayChart converted_half = half;
        if ((source_group_size != target_group_size ||
             algorithm == KeyModeConversionAlgorithm::NK3) &&
            !half.notes.empty()) {
            auto converted = convert_key_mode_chart(
                half,
                default_converter_options(source_group_size,
                                          target_group_size,
                                          seed ^ (0x9E3779B9u * static_cast<uint32_t>(group + 1)),
                                          algorithm,
                                          nk2_preset,
                                          context));
            for (auto& warning : converted.warnings) {
                warnings.push_back((group == 0 ? "1P: " : "2P: ") + warning);
            }
            if (!converted.converted) {
                return false;
            }
            converted_half = std::move(converted.chart);
        } else {
            converted_half.lane_count = target_group_size;
        }

        for (auto note : converted_half.notes) {
            note.lane += group * target_group_size;
            output.notes.push_back(std::move(note));
        }
    }

    sort_notes_for_gameplay(output);
    return true;
}

void apply_legacy_key_mode_fallback(GameplayChart& chart,
                                    int target_count,
                                    std::vector<std::string>& warnings) {
    if (target_count <= 0 || target_count == chart.lane_count) {
        return;
    }

    if (target_count < chart.lane_count) {
        auto& notes = chart.notes;
        notes.erase(std::remove_if(notes.begin(), notes.end(), [target_count](const NoteEvent& note) {
            return note.lane > target_count;
        }), notes.end());
        chart.lane_count = target_count;
        chart.lane_group_size = 0;
        chart.scratch_lanes.erase(
            std::remove_if(chart.scratch_lanes.begin(), chart.scratch_lanes.end(),
                           [target_count](int lane) { return lane > target_count; }),
            chart.scratch_lanes.end());
        warnings.push_back("Key mode fallback dropped notes outside the target lane range.");
        return;
    }

    warnings.push_back("Key mode fallback kept the original lane count.");
}

void remap_mines_proportionally(GameplayChart& chart, int source_lane_count) {
    if (source_lane_count <= 0 || chart.lane_count <= 0 ||
        source_lane_count == chart.lane_count) {
        return;
    }
    for (auto& mine : chart.mines) {
        if (mine.lane <= 0 || mine.lane > source_lane_count) {
            continue;
        }
        const double normalized =
            (static_cast<double>(mine.lane) - 0.5) / static_cast<double>(source_lane_count);
        mine.lane = std::clamp(
            static_cast<int>(std::floor(normalized * static_cast<double>(chart.lane_count))) + 1,
            1, chart.lane_count);
    }
}

void apply_dp_flip(GameplayChart& chart) {
    const int group_size = effective_group_size(chart);
    if (group_size <= 0 || chart.lane_count != group_size * 2) {
        return;
    }

    std::vector<int> lane_map(static_cast<std::size_t>(chart.lane_count + 1));
    std::iota(lane_map.begin(), lane_map.end(), 0);
    for (int lane = 1; lane <= chart.lane_count; ++lane) {
        lane_map[static_cast<std::size_t>(lane)] =
            lane <= group_size ? lane + group_size : lane - group_size;
    }
    apply_lane_map(chart, lane_map);
}

void apply_mirror(GameplayChart& chart) {
    if (chart.lane_count <= 0) {
        return;
    }

    std::vector<int> lane_map(static_cast<std::size_t>(chart.lane_count + 1));
    std::iota(lane_map.begin(), lane_map.end(), 0);
    for (const auto& group : playable_lane_groups(chart)) {
        for (std::size_t index = 0; index < group.size(); ++index) {
            lane_map[static_cast<std::size_t>(group[index])] = group[group.size() - 1 - index];
        }
    }
    apply_lane_map(chart, lane_map);
}

void apply_rotate_random(GameplayChart& chart, uint32_t seed) {
    if (chart.lane_count <= 0) {
        return;
    }

    std::vector<int> lane_map(static_cast<std::size_t>(chart.lane_count + 1));
    std::iota(lane_map.begin(), lane_map.end(), 0);
    std::mt19937 rng(normalize_seed(seed));
    for (const auto& group : playable_lane_groups(chart)) {
        if (group.size() <= 1) {
            continue;
        }
        std::uniform_int_distribution<std::size_t> offset_dist(1, group.size() - 1);
        const std::size_t offset = offset_dist(rng);
        for (std::size_t index = 0; index < group.size(); ++index) {
            lane_map[static_cast<std::size_t>(group[index])] = group[(index + offset) % group.size()];
        }
    }
    apply_lane_map(chart, lane_map);
}

void apply_full_random(GameplayChart& chart, uint32_t seed) {
    if (chart.lane_count <= 0) {
        return;
    }

    std::vector<int> lane_map(static_cast<std::size_t>(chart.lane_count + 1));
    std::iota(lane_map.begin(), lane_map.end(), 0);
    std::mt19937 rng(normalize_seed(seed));
    for (const auto& group : playable_lane_groups(chart)) {
        std::vector<int> shuffled = group;
        std::shuffle(shuffled.begin(), shuffled.end(), rng);
        for (std::size_t index = 0; index < group.size(); ++index) {
            lane_map[static_cast<std::size_t>(group[index])] = shuffled[index];
        }
    }
    apply_lane_map(chart, lane_map);
}

void apply_super_random(GameplayChart& chart, uint32_t seed, std::vector<std::string>& warnings) {
    const int lane_count = chart.lane_count;
    if (lane_count <= 0) {
        return;
    }

    struct NoteSpan {
        std::size_t index = 0;
        int lane = 0;
        int64_t start = 0;
        int64_t end = 0;
    };

    std::vector<NoteSpan> spans;
    spans.reserve(chart.notes.size());
    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        const auto& note = chart.notes[i];
        int64_t end = note.end_sample.value_or(note.start_sample);
        if (end < note.start_sample) {
            end = note.start_sample;
        }
        spans.push_back({i, note.lane, note.start_sample, end});
    }
    std::sort(spans.begin(), spans.end(), [](const NoteSpan& lhs, const NoteSpan& rhs) {
        if (lhs.start == rhs.start) {
            return lhs.end < rhs.end;
        }
        return lhs.start < rhs.start;
    });

    const auto groups = playable_lane_groups(chart);
    std::vector<int> group_by_lane(static_cast<std::size_t>(lane_count + 1), -1);
    for (std::size_t group_index = 0; group_index < groups.size(); ++group_index) {
        for (const int lane : groups[group_index]) {
            group_by_lane[static_cast<std::size_t>(lane)] = static_cast<int>(group_index);
        }
    }

    std::vector<int64_t> lane_end(static_cast<std::size_t>(lane_count),
                                  std::numeric_limits<int64_t>::min());
    std::vector<int> candidates;
    std::mt19937 rng(normalize_seed(seed));
    std::size_t fallback_count = 0;

    for (const auto& span : spans) {
        if (span.lane <= 0 || span.lane > lane_count || is_scratch_lane(chart, span.lane)) {
            if (span.lane > 0 && span.lane <= lane_count) {
                const auto lane_index = static_cast<std::size_t>(span.lane - 1);
                lane_end[lane_index] = std::max(lane_end[lane_index], span.end);
            }
            continue;
        }

        const int group_index = group_by_lane[static_cast<std::size_t>(span.lane)];
        if (group_index < 0 || group_index >= static_cast<int>(groups.size())) {
            continue;
        }

        candidates.clear();
        for (const int lane : groups[static_cast<std::size_t>(group_index)]) {
            if (span.start > lane_end[static_cast<std::size_t>(lane - 1)]) {
                candidates.push_back(lane);
            }
        }

        int chosen_lane = span.lane;
        if (candidates.empty()) {
            ++fallback_count;
        } else {
            std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
            chosen_lane = candidates[dist(rng)];
        }

        chart.notes[span.index].lane = chosen_lane;
        const auto lane_index = static_cast<std::size_t>(chosen_lane - 1);
        lane_end[lane_index] = std::max(lane_end[lane_index], span.end);
    }

    for (auto& mine : chart.mines) {
        if (mine.lane <= 0 || mine.lane > lane_count || is_scratch_lane(chart, mine.lane)) {
            continue;
        }
        const int group_index = group_by_lane[static_cast<std::size_t>(mine.lane)];
        if (group_index < 0 || group_index >= static_cast<int>(groups.size()) ||
            groups[static_cast<std::size_t>(group_index)].empty()) {
            continue;
        }
        const auto& group = groups[static_cast<std::size_t>(group_index)];
        std::uniform_int_distribution<std::size_t> dist(0, group.size() - 1);
        mine.lane = group[dist(rng)];
    }

    sort_notes_for_gameplay(chart);
    if (fallback_count > 0) {
        warnings.push_back("SR fallback: overlapping notes could not be fully avoided (count=" +
                           std::to_string(fallback_count) + ").");
    }
}

}  // namespace

ModeApplyResult apply_mode_settings(const GameplayChart& chart,
                                    const ModeSettings& settings,
                                    const ModeApplyContext& context) {
    ModeApplyResult result;
    result.chart = chart;
    result.chart.lane_count = resolve_lane_count(chart);

    const int target_count = target_lane_count(settings.key_mode);
    ScratchExtraction extracted;
    const GameplayChart* conversion_source = &result.chart;
    bool scratch_extracted = false;
    if (target_count > 0 && !result.chart.scratch_lanes.empty()) {
        extracted = extract_key_part(result.chart);
        if (extracted.chart.lane_count > 0) {
            conversion_source = &extracted.chart;
            scratch_extracted = true;
        }
    }
    const int conversion_source_lane_count = conversion_source->lane_count;

    bool conversion_succeeded = target_count <= 0;
    if (target_count > 0) {
        const bool nk3_remaster =
            settings.key_conversion_algorithm == KeyModeConversionAlgorithm::NK3;
        if (target_count == conversion_source->lane_count && !nk3_remaster) {
            result.chart = *conversion_source;
            conversion_succeeded = true;
        } else {
            const bool grouped_applicable =
                effective_group_size(*conversion_source) > 0 && target_count % 2 == 0;
            if (grouped_applicable) {
                GameplayChart grouped_chart;
                if (convert_grouped_chart(*conversion_source,
                                          target_count,
                                          settings.random_seed,
                                          settings.key_conversion_algorithm,
                                          settings.key_conversion_nk2_preset,
                                          context,
                                          grouped_chart,
                                          result.warnings)) {
                    result.chart = std::move(grouped_chart);
                    conversion_succeeded = true;
                }
            } else {
                auto converted = convert_key_mode_chart(
                    *conversion_source,
                    default_converter_options(conversion_source->lane_count,
                                              target_count,
                                              settings.random_seed,
                                              settings.key_conversion_algorithm,
                                              settings.key_conversion_nk2_preset,
                                              context));
                result.warnings.insert(result.warnings.end(),
                                       converted.warnings.begin(),
                                       converted.warnings.end());
                if (converted.converted) {
                    result.chart = std::move(converted.chart);
                    result.chart.lane_group_size = 0;
                    conversion_succeeded = true;
                }
            }
        }

        if (!conversion_succeeded) {
            if (scratch_extracted) {
                result.chart = chart;
                result.chart.lane_count = resolve_lane_count(chart);
                result.warnings.push_back(
                    "Scratch-aware key conversion could not remap the key lanes; the native scratch layout was kept.");
            } else {
                apply_legacy_key_mode_fallback(result.chart, target_count, result.warnings);
            }
        } else if (scratch_extracted) {
            result.warnings.push_back(
                "Scratch-aware key conversion excluded " + std::to_string(extracted.scratch_note_count) +
                " scratch note(s), remapped only the key lanes, and moved " +
                std::to_string(extracted.autoplay_cue_count) +
                " followed scratch keysound(s) to autoplay.");
        }
    }

    if (settings.dp_flip) {
        apply_dp_flip(result.chart);
    }

    if (target_count > 0 && conversion_succeeded) {
        remap_mines_proportionally(result.chart, conversion_source_lane_count);
    }

    if (settings.random == RandomMode::Mirror) {
        apply_mirror(result.chart);
    } else if (settings.random == RandomMode::RotateRandom) {
        apply_rotate_random(result.chart, settings.random_seed);
    } else if (settings.random == RandomMode::FullRandom) {
        apply_full_random(result.chart, settings.random_seed);
    } else if (settings.random == RandomMode::SuperRandom) {
        apply_super_random(result.chart, settings.random_seed, result.warnings);
    }

    return result;
}

}  // namespace tenriff::gameplay
