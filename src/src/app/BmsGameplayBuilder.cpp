#include "app/BmsGameplayBuilder.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <string_view>
#include <unordered_map>

namespace tenriff::app {

namespace {

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
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

int64_t scale_samples(int64_t samples, double rate) {
    if (rate <= 0.0 || !std::isfinite(rate)) {
        return samples;
    }
    return static_cast<int64_t>(std::llround(static_cast<double>(samples) / rate));
}

}  // namespace

BmsGameplayBuildResult build_bms_gameplay_chart(const chart::BmsTimeline& timeline,
                                                const chart::BmsChart& parsed_chart,
                                                double rate) {
    struct BmsNoteEntry {
        gameplay::NoteEvent note;
        std::string object_id;
        std::size_t sequence = 0;
    };

    struct PendingLongNote {
        int lane = 0;
        int64_t start_sample = 0;
        std::string object_id;
        std::size_t sequence = 0;
    };

    BmsGameplayBuildResult result;
    std::vector<BmsNoteEntry> entries;
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

    int max_lane = 0;
    std::size_t sequence = 0;
    for (const auto& scheduled : timeline.events) {
        if (scheduled.event.type != chart::BmsNormalizedEventType::Note || !scheduled.event.lane.has_value()) {
            continue;
        }

        const int lane = static_cast<int>(scheduled.event.lane.value());
        const int64_t sample = std::max<int64_t>(0, scale_samples(scheduled.time_samples, rate));
        const std::string object_id = to_upper_ascii(scheduled.event.object_id);

        max_lane = std::max(max_lane, lane);

        if (is_long_note_channel(scheduled.event.channel)) {
            auto pending_it = pending_long_notes_by_lane.find(lane);
            if (pending_it == pending_long_notes_by_lane.end()) {
                pending_long_notes_by_lane.emplace(lane, PendingLongNote{lane, sample, object_id, sequence++});
            } else {
                if (sample > pending_it->second.start_sample) {
                    BmsNoteEntry entry;
                    entry.note.lane = lane;
                    entry.note.start_sample = pending_it->second.start_sample;
                    entry.note.end_sample = sample;
                    entry.object_id = pending_it->second.object_id;
                    entry.sequence = pending_it->second.sequence;
                    entries.push_back(std::move(entry));
                } else {
                    result.messages.push_back("Long note end was not after its start on lane " +
                                              std::to_string(lane) + "; falling back to tap note.");
                    BmsNoteEntry entry;
                    entry.note.lane = lane;
                    entry.note.start_sample = pending_it->second.start_sample;
                    entry.object_id = pending_it->second.object_id;
                    entry.sequence = pending_it->second.sequence;
                    entries.push_back(std::move(entry));
                }
                pending_long_notes_by_lane.erase(pending_it);
            }
            continue;
        }

        if (!lnobj.empty() && object_id == lnobj) {
            auto last_it = last_normal_note_by_lane.find(lane);
            if (last_it == last_normal_note_by_lane.end() || last_it->second >= entries.size()) {
                result.messages.push_back("LNOBJ tail was encountered without a preceding note on lane " +
                                          std::to_string(lane) + ".");
                continue;
            }
            auto& note = entries[last_it->second].note;
            if (!note.end_sample.has_value() && sample > note.start_sample) {
                note.end_sample = sample;
            }
            last_normal_note_by_lane.erase(last_it);
            continue;
        }

        BmsNoteEntry entry;
        entry.note.lane = lane;
        entry.note.start_sample = sample;
        entry.object_id = object_id;
        entry.sequence = sequence++;
        last_normal_note_by_lane[lane] = entries.size();
        entries.push_back(std::move(entry));
    }

    for (const auto& [lane, pending] : pending_long_notes_by_lane) {
        BmsNoteEntry entry;
        entry.note.lane = lane;
        entry.note.start_sample = pending.start_sample;
        entry.object_id = pending.object_id;
        entry.sequence = pending.sequence;
        entries.push_back(std::move(entry));
        result.messages.push_back("Unclosed long note on lane " + std::to_string(lane) +
                                  "; falling back to tap note.");
    }

    std::stable_sort(entries.begin(), entries.end(), [](const BmsNoteEntry& lhs, const BmsNoteEntry& rhs) {
        if (lhs.note.start_sample != rhs.note.start_sample) {
            return lhs.note.start_sample < rhs.note.start_sample;
        }
        if (lhs.note.lane != rhs.note.lane) {
            return lhs.note.lane < rhs.note.lane;
        }
        return lhs.sequence < rhs.sequence;
    });

    result.chart.lane_count = max_lane > 0 ? max_lane : 10;
    result.chart.duration_samples = std::max<int64_t>(0, scale_samples(timeline.duration_samples, rate));
    result.chart.notes.reserve(entries.size());
    result.note_object_ids.reserve(entries.size());

    for (auto& entry : entries) {
        result.chart.notes.push_back(std::move(entry.note));
        result.note_object_ids.push_back(std::move(entry.object_id));
    }

    return result;
}

}  // namespace tenriff::app
