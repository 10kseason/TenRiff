#include "app/BmsKeyConverter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "app/BmsGameplayBuilder.h"
#include "chart/BmsChartNorm.h"
#include "chart/BmsParser.h"
#include "chart/BmsTimeline.h"
#include "gameplay/KeyModeConverter.h"

namespace tenriff::app {

namespace {

constexpr double kPositionEpsilon = 1e-9;
constexpr int kQuantizeSliceCount = 1920;
constexpr int kMaxExactSliceCount = 19200;
constexpr double kBeatsPerMeasure = 4.0;
constexpr double kStopTicksPerBeat = 48.0;

std::string trim_copy(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

std::string to_upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'a' && ch <= 'z') {
            return static_cast<char>(ch - ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string to_upper_ascii(std::string_view value) {
    return to_upper_ascii(std::string(value));
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch + ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string to_lower_ascii(std::string_view value) {
    return to_lower_ascii(std::string(value));
}

std::string normalize_preset_token(std::string_view token) {
    std::string normalized;
    normalized.reserve(token.size());
    for (unsigned char ch : token) {
        if (std::isspace(ch) != 0 || ch == '-' || ch == '_') {
            continue;
        }
        if (ch >= 'A' && ch <= 'Z') {
            normalized.push_back(static_cast<char>(ch + ('a' - 'A')));
        } else {
            normalized.push_back(static_cast<char>(ch));
        }
    }
    return normalized;
}

bool is_charge_note_lnmode(std::string_view value) {
    return trim_copy(value) == "2";
}

bool is_note_lane_channel(std::string_view channel) {
    return channel.size() == 2 && (channel[0] == '1' || channel[0] == '2' || channel[0] == '5' || channel[0] == '6');
}

bool is_mode_token(std::string_view token) {
    if (token.empty()) {
        return false;
    }
    std::string normalized = to_upper_ascii(token);
    if (normalized.size() < 2 || normalized.back() != 'K') {
        return false;
    }
    return std::all_of(normalized.begin(), normalized.end() - 1, [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

bool is_filtered_header_key(std::string_view key) {
    const std::string normalized = to_upper_ascii(key);
    return normalized == "PLAYER" || normalized == "PLAYMODE" || normalized == "KEYMODE" ||
           normalized == "LNOBJ" || normalized == "LNMODE" || is_mode_token(normalized);
}

int64_t scale_samples(int64_t samples, double rate) {
    if (rate <= 0.0 || !std::isfinite(rate)) {
        return samples;
    }
    return static_cast<int64_t>(std::llround(static_cast<double>(samples) / rate));
}

std::size_t hold_count(const gameplay::GameplayChart& chart) {
    return static_cast<std::size_t>(std::count_if(chart.notes.begin(), chart.notes.end(), [](const auto& note) {
        return note.end_sample.has_value();
    }));
}

struct BmsPositionSlot {
    int measure = 0;
    int64_t numerator = 0;
    int64_t denominator = 1;
    double measure_fraction = 0.0;
    double absolute_position = 0.0;
};

struct SourceNotePlacement {
    int source_lane = 0;
    int64_t start_sample = 0;
    std::optional<int64_t> end_sample;
    BmsPositionSlot start;
    std::optional<BmsPositionSlot> end;
    std::string object_id;
    bool release_required = false;
};

struct PendingLongNote {
    int lane = 0;
    int64_t start_sample = 0;
    BmsPositionSlot start;
    std::string object_id;
    std::size_t sequence = 0;
};

struct PlacementEntry {
    SourceNotePlacement placement;
    std::size_t sequence = 0;
};

struct LayoutDefinition {
    int lane_count = 0;
    std::vector<std::string> lane_channels;
    std::optional<std::string> player_header;
    std::optional<std::pair<std::string, std::string>> value_header;
    std::optional<std::string> flag_header;
};

const std::vector<BmsKeyConverterPreset>& preset_table() {
    static const std::vector<BmsKeyConverterPreset> presets = {
        {"default", "Default (8K)", 8, 8, 2, 4, true},
        {"10k", "10K Preset", 10, 8, 2, 4, true},
        {"9k", "9K Preset", 9, 8, 2, 4, true},
        {"8k", "8K Preset", 8, 8, 2, 4, true},
        {"7k", "7K Preset", 7, 7, 2, 4, false},
        {"a8k", "A8K", 8, 7, 7, 2, true},
        {"a9k", "A9K", 9, 7, 7, 2, true},
        {"a10k", "A10K", 10, 7, 7, 2, true},
        {"dt6", "DownTo6K", 6, 6, 4, 2, true},
        {"dt4", "DownTo4K", 4, 4, 4, 2, true},
    };
    return presets;
}

bool preset_matches_token(const BmsKeyConverterPreset& preset, std::string_view token) {
    const std::string normalized = normalize_preset_token(token);
    if (normalized.empty()) {
        return false;
    }

    if (normalized == normalize_preset_token(preset.token)) {
        return true;
    }

    if ((preset.token == "10k" && normalized == "tenk") ||
        (preset.token == "9k" && normalized == "ninek") ||
        (preset.token == "8k" && normalized == "eightk") ||
        (preset.token == "7k" && normalized == "sevenk") ||
        (preset.token == "dt6" && normalized == "downto6k") ||
        (preset.token == "dt4" && normalized == "downto4k")) {
        return true;
    }

    return false;
}

LayoutDefinition resolve_layout_definition(int lane_count) {
    switch (lane_count) {
    case 4:
        return LayoutDefinition{4, {"11", "12", "13", "14"}, std::nullopt, std::nullopt, std::nullopt};
    case 5:
        return LayoutDefinition{5, {"11", "12", "13", "14", "15"}, std::nullopt, std::nullopt, std::nullopt};
    case 6:
        return LayoutDefinition{6, {"11", "12", "13", "14", "15", "16"}, std::string("1"), std::nullopt, std::string("5K")};
    case 8:
        return LayoutDefinition{8, {"11", "12", "13", "14", "15", "16", "18", "19"},
                                std::string("1"), std::nullopt, std::string("7K")};
    case 9:
        return LayoutDefinition{9, {"11", "12", "13", "14", "15", "22", "23", "24", "25"},
                                std::nullopt, std::make_pair(std::string("PLAYMODE"), std::string("9K")), std::nullopt};
    case 10:
        return LayoutDefinition{10, {"11", "12", "13", "14", "15", "21", "22", "23", "24", "25"},
                                std::string("3"), std::nullopt, std::nullopt};
    case 16:
        return LayoutDefinition{16, {"11", "12", "13", "14", "15", "16", "18", "19",
                                     "21", "22", "23", "24", "25", "26", "28", "29"},
                                std::string("3"), std::make_pair(std::string("PLAYMODE"), std::string("16K")), std::nullopt};
    default:
        return {};
    }
}

gameplay::KeyModeConverterOptions default_converter_options(int source_lane_count,
                                                            int target_lane_count,
                                                            uint32_t seed,
                                                            double base_bpm,
                                                            int sample_rate) {
    gameplay::KeyModeConverterOptions options;
    options.target_lane_count = target_lane_count;
    options.seed = seed;
    options.base_bpm = base_bpm;
    options.sample_rate = sample_rate;

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

BmsPositionSlot make_slot_from_event(const chart::BmsNormalizedEvent& event) {
    BmsPositionSlot slot;
    slot.measure = event.measure;
    slot.numerator = static_cast<int64_t>(event.slice_index);
    slot.denominator = static_cast<int64_t>(std::max<std::size_t>(1, event.slice_count));
    slot.measure_fraction = event.slice_count > 0
                                ? static_cast<double>(event.slice_index) / static_cast<double>(event.slice_count)
                                : 0.0;
    slot.absolute_position = event.position;
    return slot;
}

std::vector<SourceNotePlacement> build_source_note_placements(const chart::BmsTimeline& timeline,
                                                              const chart::BmsChart& parsed_chart,
                                                              double rate) {
    std::vector<PlacementEntry> entries;
    entries.reserve(timeline.events.size());
    std::unordered_map<int, std::size_t> last_normal_note_by_lane;
    std::unordered_map<int, PendingLongNote> pending_long_notes_by_lane;

    const auto is_long_note_channel = [](std::string_view channel) {
        return channel.size() == 2 && (channel[0] == '5' || channel[0] == '6');
    };

    const std::string lnobj = [&parsed_chart]() {
        auto it = parsed_chart.headers.find("LNOBJ");
        if (it == parsed_chart.headers.end()) {
            return std::string{};
        }
        return to_upper_ascii(trim_copy(it->second));
    }();
    const bool release_required = [&parsed_chart]() {
        auto it = parsed_chart.headers.find("LNMODE");
        if (it == parsed_chart.headers.end()) {
            return false;
        }
        return is_charge_note_lnmode(it->second);
    }();

    std::size_t sequence = 0;
    for (const auto& scheduled : timeline.events) {
        if (scheduled.event.type != chart::BmsNormalizedEventType::Note || !scheduled.event.lane.has_value()) {
            continue;
        }

        const int lane = static_cast<int>(scheduled.event.lane.value());
        const int64_t sample = std::max<int64_t>(0, scale_samples(scheduled.time_samples, rate));
        const BmsPositionSlot slot = make_slot_from_event(scheduled.event);
        const std::string object_id = to_upper_ascii(scheduled.event.object_id);

        if (is_long_note_channel(scheduled.event.channel)) {
            auto pending_it = pending_long_notes_by_lane.find(lane);
            if (pending_it == pending_long_notes_by_lane.end()) {
                pending_long_notes_by_lane.emplace(lane, PendingLongNote{lane, sample, slot, object_id, sequence++});
            } else {
                PlacementEntry entry;
                entry.placement.source_lane = lane;
                entry.placement.start_sample = pending_it->second.start_sample;
                entry.placement.start = pending_it->second.start;
                entry.placement.object_id = pending_it->second.object_id;
                entry.sequence = pending_it->second.sequence;
                if (sample > pending_it->second.start_sample) {
                    entry.placement.end_sample = sample;
                    entry.placement.end = slot;
                    entry.placement.release_required = release_required;
                }
                entries.push_back(std::move(entry));
                pending_long_notes_by_lane.erase(pending_it);
            }
            continue;
        }

        if (!lnobj.empty() && object_id == lnobj) {
            auto last_it = last_normal_note_by_lane.find(lane);
            if (last_it == last_normal_note_by_lane.end() || last_it->second >= entries.size()) {
                continue;
            }
            auto& placement = entries[last_it->second].placement;
            if (!placement.end_sample.has_value() && sample > placement.start_sample) {
                placement.end_sample = sample;
                placement.end = slot;
                placement.release_required = release_required;
            }
            last_normal_note_by_lane.erase(last_it);
            continue;
        }

        PlacementEntry entry;
        entry.placement.source_lane = lane;
        entry.placement.start_sample = sample;
        entry.placement.start = slot;
        entry.placement.object_id = object_id;
        entry.sequence = sequence++;
        last_normal_note_by_lane[lane] = entries.size();
        entries.push_back(std::move(entry));
    }

    for (const auto& [lane, pending] : pending_long_notes_by_lane) {
        PlacementEntry entry;
        entry.placement.source_lane = lane;
        entry.placement.start_sample = pending.start_sample;
        entry.placement.start = pending.start;
        entry.placement.object_id = pending.object_id;
        entry.sequence = pending.sequence;
        entries.push_back(std::move(entry));
    }

    std::stable_sort(entries.begin(), entries.end(), [](const PlacementEntry& lhs, const PlacementEntry& rhs) {
        if (lhs.placement.start_sample != rhs.placement.start_sample) {
            return lhs.placement.start_sample < rhs.placement.start_sample;
        }
        if (lhs.placement.source_lane != rhs.placement.source_lane) {
            return lhs.placement.source_lane < rhs.placement.source_lane;
        }
        return lhs.sequence < rhs.sequence;
    });

    std::vector<SourceNotePlacement> placements;
    placements.reserve(entries.size());
    for (auto& entry : entries) {
        placements.push_back(std::move(entry.placement));
    }
    return placements;
}

double seconds_from_position_delta(double delta, double bpm) {
    if (delta <= 0.0 || bpm <= 0.0 || !std::isfinite(bpm)) {
        return 0.0;
    }
    return ((delta * kBeatsPerMeasure) * 60.0) / bpm;
}

double stop_ticks_to_seconds(double stop_value, double bpm) {
    if (stop_value <= 0.0 || bpm <= 0.0 || !std::isfinite(bpm)) {
        return 0.0;
    }
    return ((stop_value / kStopTicksPerBeat) * 60.0) / bpm;
}

struct TimingSegment {
    double start_sample = 0.0;
    double end_sample = 0.0;
    double start_position = 0.0;
    double end_position = 0.0;
    bool is_stop = false;
};

struct SamplePositionMap {
    std::vector<TimingSegment> segments;
    double chart_end_position = 0.0;
    double duration_samples = 0.0;

    [[nodiscard]] double position_for_sample(int64_t sample) const {
        const double clamped_sample = std::clamp(static_cast<double>(sample), 0.0, duration_samples);
        if (segments.empty()) {
            return 0.0;
        }

        auto it = std::lower_bound(segments.begin(), segments.end(), clamped_sample, [](const TimingSegment& segment, double value) {
            return segment.end_sample + 0.5 < value;
        });
        if (it == segments.end()) {
            return chart_end_position;
        }
        if (it->is_stop || std::abs(it->end_sample - it->start_sample) <= 1e-9) {
            return it->start_position;
        }
        const double ratio = std::clamp((clamped_sample - it->start_sample) / (it->end_sample - it->start_sample), 0.0, 1.0);
        return it->start_position + ((it->end_position - it->start_position) * ratio);
    }
};

SamplePositionMap build_sample_position_map(const chart::BmsNormalizedChart& chart, int sample_rate_hz) {
    SamplePositionMap map;
    if (sample_rate_hz <= 0) {
        return map;
    }

    const double sample_rate = static_cast<double>(sample_rate_hz);
    double current_bpm = chart.base_bpm > 0.0 ? chart.base_bpm : 120.0;
    double current_position = 0.0;
    double current_time_samples = 0.0;
    std::size_t index = 0;

    while (index < chart.events.size()) {
        const double group_position = chart.events[index].position;
        const double delta_position = std::max(0.0, group_position - current_position);
        if (delta_position > kPositionEpsilon) {
            const double delta_samples = seconds_from_position_delta(delta_position, current_bpm) * sample_rate;
            if (delta_samples > 1e-9) {
                map.segments.push_back(TimingSegment{current_time_samples, current_time_samples + delta_samples,
                                                     current_position, group_position, false});
            }
            current_time_samples += delta_samples;
            current_position = group_position;
        }

        std::size_t group_end = index;
        while (group_end < chart.events.size() &&
               std::abs(chart.events[group_end].position - group_position) <= kPositionEpsilon) {
            ++group_end;
        }

        double stop_seconds = 0.0;
        for (std::size_t i = index; i < group_end; ++i) {
            const auto& event = chart.events[i];
            if (event.type == chart::BmsNormalizedEventType::BpmChange && event.value.has_value() &&
                event.value.value() > 0.0 && std::isfinite(event.value.value())) {
                current_bpm = event.value.value();
            } else if (event.type == chart::BmsNormalizedEventType::Stop && event.value.has_value()) {
                stop_seconds += stop_ticks_to_seconds(event.value.value(), current_bpm);
            }
        }

        if (stop_seconds > 0.0) {
            const double stop_samples = stop_seconds * sample_rate;
            map.segments.push_back(TimingSegment{current_time_samples, current_time_samples + stop_samples,
                                                 current_position, current_position, true});
            current_time_samples += stop_samples;
        }

        index = group_end;
    }

    if (!chart.measures.empty()) {
        const double chart_end_position = chart.measures.back().end();
        const double remaining = std::max(0.0, chart_end_position - current_position);
        if (remaining > kPositionEpsilon) {
            const double delta_samples = seconds_from_position_delta(remaining, current_bpm) * sample_rate;
            if (delta_samples > 1e-9) {
                map.segments.push_back(TimingSegment{current_time_samples, current_time_samples + delta_samples,
                                                     current_position, chart_end_position, false});
            }
            current_time_samples += delta_samples;
            current_position = chart_end_position;
        }
    }

    map.chart_end_position = current_position;
    map.duration_samples = current_time_samples;
    return map;
}

std::optional<BmsPositionSlot> quantize_position_to_slot(const chart::BmsNormalizedChart& chart,
                                                         double absolute_position,
                                                         int max_slice_count) {
    if (chart.measures.empty()) {
        BmsPositionSlot slot;
        slot.measure = 0;
        slot.numerator = 0;
        slot.denominator = 1;
        slot.measure_fraction = 0.0;
        slot.absolute_position = 0.0;
        return slot;
    }

    int measure_index = static_cast<int>(chart.measures.size()) - 1;
    for (int i = 0; i < static_cast<int>(chart.measures.size()); ++i) {
        if (absolute_position < chart.measures[static_cast<std::size_t>(i)].end() - kPositionEpsilon ||
            std::abs(absolute_position - chart.measures[static_cast<std::size_t>(i)].start) <= kPositionEpsilon) {
            measure_index = i;
            break;
        }
    }

    if (measure_index < 0) {
        return std::nullopt;
    }

    const auto& measure = chart.measures[static_cast<std::size_t>(measure_index)];
    if (measure.length <= 0.0) {
        return std::nullopt;
    }

    double fraction = (absolute_position - measure.start) / measure.length;
    fraction = std::clamp(fraction, 0.0, 1.0);

    int64_t numerator = static_cast<int64_t>(std::llround(fraction * static_cast<double>(max_slice_count)));
    if (numerator >= max_slice_count) {
        BmsPositionSlot slot;
        slot.measure = measure_index + 1;
        slot.numerator = 0;
        slot.denominator = 1;
        slot.measure_fraction = 0.0;
        slot.absolute_position = measure.end();
        return slot;
    }

    const int64_t denominator = max_slice_count;
    const int64_t divisor = std::gcd(std::max<int64_t>(1, numerator), denominator);

    BmsPositionSlot slot;
    slot.measure = measure_index;
    slot.numerator = numerator / divisor;
    slot.denominator = denominator / divisor;
    slot.measure_fraction = static_cast<double>(slot.numerator) / static_cast<double>(slot.denominator);
    slot.absolute_position = measure.start + (measure.length * slot.measure_fraction);
    return slot;
}

bool position_after(const BmsPositionSlot& lhs, const BmsPositionSlot& rhs) {
    return lhs.absolute_position > rhs.absolute_position + kPositionEpsilon;
}

std::set<std::string> collect_used_object_tokens(const chart::BmsChart& parsed_chart,
                                                 const std::vector<SourceNotePlacement>& placements) {
    std::set<std::string> used;
    for (const auto& command : parsed_chart.commands) {
        if (!is_note_lane_channel(command.channel) || command.data.size() % 2 != 0) {
            continue;
        }
        for (std::size_t index = 0; index < command.data.size(); index += 2) {
            used.insert(to_upper_ascii(command.data.substr(index, 2)));
        }
    }
    for (const auto& [slot, _] : parsed_chart.wav) {
        used.insert(to_upper_ascii(slot));
    }
    for (const auto& placement : placements) {
        if (!placement.object_id.empty()) {
            used.insert(to_upper_ascii(placement.object_id));
        }
    }
    return used;
}

std::optional<std::string> allocate_lnobj_token(const chart::BmsChart& parsed_chart,
                                                const std::vector<SourceNotePlacement>& placements) {
    static constexpr std::string_view kAlphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::set<std::string> used = collect_used_object_tokens(parsed_chart, placements);

    for (auto it_hi = kAlphabet.rbegin(); it_hi != kAlphabet.rend(); ++it_hi) {
        for (auto it_lo = kAlphabet.rbegin(); it_lo != kAlphabet.rend(); ++it_lo) {
            std::string token;
            token.push_back(*it_hi);
            token.push_back(*it_lo);
            if (used.find(token) == used.end()) {
                return token;
            }
        }
    }
    return std::nullopt;
}

struct OutputEvent {
    BmsPositionSlot slot;
    std::string channel;
    std::string token;
};

uint64_t lcm_with_cap(uint64_t lhs, uint64_t rhs, uint64_t cap) {
    if (lhs == 0 || rhs == 0) {
        return 0;
    }
    const uint64_t divisor = std::gcd(lhs, rhs);
    const uint64_t scaled = lhs / divisor;
    if (scaled > cap / rhs) {
        return 0;
    }
    const uint64_t value = scaled * rhs;
    return value > cap ? 0 : value;
}

std::string build_measure_data(const std::vector<OutputEvent>& events, std::vector<std::string>& warnings) {
    if (events.empty()) {
        return {};
    }

    uint64_t exact_slice_count = 1;
    bool exact = true;
    for (const auto& event : events) {
        const uint64_t denominator = static_cast<uint64_t>(std::max<int64_t>(1, event.slot.denominator));
        const uint64_t next = lcm_with_cap(exact_slice_count, denominator, kMaxExactSliceCount);
        if (next == 0) {
            exact = false;
            break;
        }
        exact_slice_count = next;
    }

    const std::size_t slice_count = static_cast<std::size_t>(exact ? exact_slice_count : kQuantizeSliceCount);
    std::vector<std::string> slots(slice_count, "00");

    for (const auto& event : events) {
        std::size_t index = 0;
        if (exact && event.slot.denominator > 0 && (exact_slice_count % static_cast<uint64_t>(event.slot.denominator)) == 0) {
            index = static_cast<std::size_t>(event.slot.numerator *
                                             static_cast<int64_t>(exact_slice_count / static_cast<uint64_t>(event.slot.denominator)));
        } else {
            const double scaled = std::round(event.slot.measure_fraction * static_cast<double>(slice_count));
            index = static_cast<std::size_t>(std::clamp<int64_t>(static_cast<int64_t>(scaled), 0, static_cast<int64_t>(slice_count) - 1));
        }
        if (index >= slots.size()) {
            index = slots.size() - 1;
        }
        if (slots[index] != "00" && slots[index] != event.token) {
            warnings.push_back("Dropped a colliding note token while writing BMS output.");
            continue;
        }
        slots[index] = event.token;
    }

    std::string data;
    data.reserve(slots.size() * 2u);
    for (const auto& slot : slots) {
        data += slot;
    }
    return data;
}

std::string build_bms_text(const chart::BmsChart& parsed_chart,
                           const chart::BmsNormalizedChart& normalized_chart,
                           const std::vector<SourceNotePlacement>& placements,
                           const gameplay::GameplayChart& converted_chart,
                           const LayoutDefinition& layout,
                           int sample_rate,
                           std::vector<std::string>& warnings) {
    const bool any_holds = hold_count(converted_chart) > 0;
    const bool any_charge = std::any_of(converted_chart.notes.begin(), converted_chart.notes.end(), [](const auto& note) {
        return note.end_sample.has_value() && note.release_required;
    });
    const std::optional<std::string> lnobj_token = any_holds ? allocate_lnobj_token(parsed_chart, placements) : std::optional<std::string>{};

    SamplePositionMap position_map = build_sample_position_map(normalized_chart, sample_rate);

    std::vector<OutputEvent> generated_events;
    generated_events.reserve(converted_chart.notes.size() * 2u);

    for (const auto& note : converted_chart.notes) {
        if (note.lane <= 0 || note.lane > static_cast<int>(layout.lane_channels.size())) {
            warnings.push_back("Skipped a converted note because its lane was outside the target layout.");
            continue;
        }
        if (note.note_id >= placements.size()) {
            warnings.push_back("Skipped a converted note because its source placement could not be resolved.");
            continue;
        }

        const auto& source = placements[note.note_id];
        const std::string& channel = layout.lane_channels[static_cast<std::size_t>(note.lane - 1)];
        generated_events.push_back(OutputEvent{source.start, channel, source.object_id});

        if (!note.end_sample.has_value() || !lnobj_token.has_value()) {
            continue;
        }

        std::optional<BmsPositionSlot> end_slot;
        if (source.end_sample.has_value() && source.end.has_value() && source.end_sample.value() == note.end_sample.value()) {
            end_slot = source.end;
        } else {
            const double absolute_position = position_map.position_for_sample(note.end_sample.value());
            end_slot = quantize_position_to_slot(normalized_chart, absolute_position, kQuantizeSliceCount);
        }

        if (!end_slot.has_value() || !position_after(end_slot.value(), source.start)) {
            warnings.push_back("Converted a clipped long note to a tap because its tail could not be represented in BMS.");
            continue;
        }

        generated_events.push_back(OutputEvent{end_slot.value(), channel, lnobj_token.value()});
    }

    std::map<int, std::map<std::string, std::vector<OutputEvent>>> generated_by_measure;
    for (const auto& event : generated_events) {
        generated_by_measure[event.slot.measure][event.channel].push_back(event);
    }

    std::vector<chart::BmsMeasureCommand> commands;
    commands.reserve(parsed_chart.commands.size() + generated_by_measure.size() * 2u);
    for (const auto& command : parsed_chart.commands) {
        if (is_note_lane_channel(command.channel)) {
            continue;
        }
        commands.push_back(command);
    }

    for (auto& [measure, channels] : generated_by_measure) {
        for (auto& [channel, events] : channels) {
            std::sort(events.begin(), events.end(), [](const OutputEvent& lhs, const OutputEvent& rhs) {
                if (lhs.slot.absolute_position != rhs.slot.absolute_position) {
                    return lhs.slot.absolute_position < rhs.slot.absolute_position;
                }
                return lhs.token < rhs.token;
            });
            std::string data = build_measure_data(events, warnings);
            if (!data.empty()) {
                commands.push_back(chart::BmsMeasureCommand{measure, channel, std::move(data)});
            }
        }
    }

    std::sort(commands.begin(), commands.end(), [](const chart::BmsMeasureCommand& lhs, const chart::BmsMeasureCommand& rhs) {
        if (lhs.measure != rhs.measure) {
            return lhs.measure < rhs.measure;
        }
        return lhs.channel < rhs.channel;
    });

    std::ostringstream out;
    auto emit_header_value = [&](std::string_view key, std::string_view value) {
        out << '#' << key;
        if (!value.empty()) {
            out << ' ' << value;
        }
        out << '\n';
    };
    auto emit_header_flag = [&](std::string_view key) {
        out << '#' << key << '\n';
    };

    std::vector<std::string> remaining_keys;
    remaining_keys.reserve(parsed_chart.headers.size());
    for (const auto& [key, value] : parsed_chart.headers) {
        if (!is_filtered_header_key(key)) {
            remaining_keys.push_back(key);
        }
    }
    std::sort(remaining_keys.begin(), remaining_keys.end());

    static constexpr std::array<std::string_view, 12> kHeaderOrder = {"TITLE", "SUBTITLE", "ARTIST", "SUBARTIST", "GENRE",
                                                                      "BPM", "PLAYLEVEL", "RANK", "TOTAL", "VOLWAV",
                                                                      "DIFFICULTY", "STAGEFILE"};
    std::set<std::string> emitted_headers;
    for (std::string_view key : kHeaderOrder) {
        auto it = parsed_chart.headers.find(std::string(key));
        if (it != parsed_chart.headers.end() && !is_filtered_header_key(it->first)) {
            emit_header_value(it->first, it->second);
            emitted_headers.insert(it->first);
        }
    }

    if (layout.player_header.has_value()) {
        emit_header_value("PLAYER", layout.player_header.value());
    }
    if (layout.value_header.has_value()) {
        emit_header_value(layout.value_header->first, layout.value_header->second);
    }
    if (layout.flag_header.has_value()) {
        emit_header_flag(layout.flag_header.value());
    }
    if (any_holds && lnobj_token.has_value()) {
        emit_header_value("LNOBJ", lnobj_token.value());
    }
    if (any_charge) {
        emit_header_value("LNMODE", "2");
    }

    for (const auto& key : remaining_keys) {
        if (emitted_headers.find(key) != emitted_headers.end()) {
            continue;
        }
        auto it = parsed_chart.headers.find(key);
        if (it != parsed_chart.headers.end()) {
            emit_header_value(it->first, it->second);
        }
    }

    auto emit_dictionary = [&](std::string_view prefix, const auto& values) {
        std::vector<std::string> keys;
        keys.reserve(values.size());
        for (const auto& [key, value] : values) {
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        for (const auto& key : keys) {
            const auto it = values.find(key);
            if (it != values.end()) {
                out << '#' << prefix << key << ' ' << it->second << '\n';
            }
        }
    };

    auto emit_numeric_dictionary = [&](std::string_view prefix, const auto& values) {
        std::vector<std::string> keys;
        keys.reserve(values.size());
        for (const auto& [key, value] : values) {
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        for (const auto& key : keys) {
            const auto it = values.find(key);
            if (it == values.end()) {
                continue;
            }
            std::ostringstream value_stream;
            value_stream.imbue(std::locale::classic());
            value_stream << std::setprecision(12) << it->second;
            out << '#' << prefix << key << ' ' << value_stream.str() << '\n';
        }
    };

    emit_dictionary("WAV", parsed_chart.wav);
    emit_dictionary("BMP", parsed_chart.bmp);
    emit_numeric_dictionary("BPM", parsed_chart.bpm);
    emit_numeric_dictionary("STOP", parsed_chart.stop);

    out << '\n';

    for (const auto& command : commands) {
        out << '#' << std::setw(3) << std::setfill('0') << command.measure
            << command.channel << ':' << command.data << '\n';
    }

    return out.str();
}

bool write_text_file(const std::string& path, const std::string& text, std::string& error) {
    std::ofstream stream;
#ifdef _WIN32
    try {
        stream.open(std::filesystem::u8path(path), std::ios::binary);
    } catch (...) {
        stream.open(path, std::ios::binary);
    }
#else
    stream.open(path, std::ios::binary);
#endif
    if (!stream) {
        error = "Failed to open output file: " + path;
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream) {
        error = "Failed to write output file: " + path;
        return false;
    }
    return true;
}

}  // namespace

const std::vector<BmsKeyConverterPreset>& bms_key_converter_presets() {
    return preset_table();
}

bool find_bms_key_converter_preset(std::string_view token, BmsKeyConverterPreset& preset) {
    for (const auto& candidate : preset_table()) {
        if (!preset_matches_token(candidate, token)) {
            continue;
        }
        preset = candidate;
        return true;
    }
    return false;
}

BmsKeyConverterResult convert_bms_chart_file(const BmsKeyConverterOptions& options) {
    BmsKeyConverterResult result;

    if (options.input_path.empty()) {
        result.error = "Input path is required.";
        return result;
    }
    if (options.output_path.empty()) {
        result.error = "Output path is required.";
        return result;
    }
    if (options.sample_rate <= 0) {
        result.error = "Sample rate must be positive.";
        return result;
    }

    const LayoutDefinition layout = resolve_layout_definition(options.target_lane_count);
    if (layout.lane_count <= 0) {
        result.error = "Unsupported target lane count. Supported values: 4, 5, 6, 8, 9, 10, 16.";
        return result;
    }

    chart::BmsParser parser;
    chart::BmsParseResult parsed = parser.parseFile(options.input_path);
    if (!parsed.success()) {
        result.error = "Failed to parse the input BMS file.";
        for (const auto& message : parsed.messages) {
            result.warnings.push_back("line " + std::to_string(message.line) + ": " + message.text);
        }
        return result;
    }

    chart::BmsChartNormalizer normalizer;
    chart::BmsNormalizationResult normalization = normalizer.normalize(parsed.chart);
    if (!normalization.success()) {
        result.error = "Failed to normalize the input BMS chart.";
        for (const auto& message : normalization.messages) {
            result.warnings.push_back("measure " + std::to_string(message.measure) + ": " + message.text);
        }
        return result;
    }

    chart::BmsTimelineBuilder timeline_builder;
    chart::BmsTimelineResult timeline = timeline_builder.build(normalization.chart, options.sample_rate);
    if (!timeline.success()) {
        result.error = "Failed to build a playable timeline from the input BMS chart.";
        for (const auto& message : timeline.messages) {
            result.warnings.push_back("measure " + std::to_string(message.measure) + ": " + message.text);
        }
        return result;
    }

    BmsGameplayBuildResult source_build = build_bms_gameplay_chart(timeline.timeline, parsed.chart, 1.0);
    std::vector<SourceNotePlacement> placements = build_source_note_placements(timeline.timeline, parsed.chart, 1.0);
    if (placements.size() != source_build.chart.notes.size()) {
        result.error = "Internal note-mapping mismatch while preparing the BMS conversion.";
        return result;
    }

    for (std::size_t i = 0; i < source_build.chart.notes.size(); ++i) {
        source_build.chart.notes[i].note_id = i;
    }

    result.source_lane_count = source_build.chart.lane_count;
    result.target_lane_count = layout.lane_count;
    if (result.source_lane_count <= 0) {
        result.error = "The input chart does not contain playable note lanes.";
        return result;
    }

    gameplay::KeyModeConverterOptions converter_options =
        default_converter_options(result.source_lane_count,
                                  layout.lane_count,
                                  options.seed,
                                  normalization.chart.base_bpm,
                                  options.sample_rate);
    if (options.max_keys > 0) {
        converter_options.max_keys = options.max_keys;
    }
    if (options.min_keys > 0) {
        converter_options.min_keys = options.min_keys;
    }
    converter_options.transform_speed_slot = options.transform_speed_slot;

    gameplay::KeyModeConverterResult converted =
        gameplay::convert_key_mode_chart(source_build.chart, converter_options);
    result.warnings.insert(result.warnings.end(), converted.warnings.begin(), converted.warnings.end());

    if (!converted.converted) {
        result.error = "The BMS chart was not converted. Check the target lane count and input chart lanes.";
        return result;
    }

    const std::string output_text =
        build_bms_text(parsed.chart, normalization.chart, placements, converted.chart, layout, options.sample_rate, result.warnings);

    std::string write_error;
    if (!write_text_file(options.output_path, output_text, write_error)) {
        result.error = std::move(write_error);
        return result;
    }

    result.success = true;
    result.note_count = converted.chart.notes.size();
    result.hold_count = hold_count(converted.chart);
    return result;
}

}  // namespace tenriff::app
