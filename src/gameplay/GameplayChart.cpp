#include "gameplay/GameplayChart.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>
#include <set>

namespace tenriff::gameplay {

std::size_t coalesce_bms_long_note_heads(GameplayChart& chart) {
    if (!chart.bms_rank.has_value()) return 0;  // Only authored BMS charts.
    using Cell = std::pair<int64_t, int>;
    std::map<Cell, std::size_t> heads;
    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        const auto& note = chart.notes[i];
        if (note.lane > 0 && note.end_sample && *note.end_sample > note.start_sample)
            heads.emplace(Cell{note.start_sample, note.lane}, i);
    }
    if (heads.empty()) return 0;
    std::vector<bool> remove(chart.notes.size(), false);
    GameplayChart removed;
    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        const auto& tap = chart.notes[i];
        if (tap.end_sample) continue;
        const auto found = heads.find({tap.start_sample, tap.lane});
        if (found == heads.end()) continue;
        auto& hold = chart.notes[found->second];
        // Preserve the hold's ID, duration and release policy. Equal-gain sounds
        // can share its trigger; overflow/different-gain sounds remain timed BGM.
        if (tap.audio_gain == hold.audio_gain && note_audio_asset_count(tap) > 0) {
            if (hold.audio_asset_count == 0 && hold.audio_asset_id != kInvalidAudioAssetId)
                hold.add_audio_asset(hold.audio_asset_id);
            for (std::size_t sound = 0; sound < note_audio_asset_count(tap); ++sound)
                hold.add_audio_asset(note_audio_asset_at(tap, sound));
        }
        removed.notes.push_back(tap);
        remove[i] = true;
    }
    if (removed.notes.empty()) return 0;
    std::size_t keep = 0;
    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        if (remove[i]) continue;
        if (keep != i) chart.notes[keep] = std::move(chart.notes[i]);
        ++keep;
    }
    chart.notes.resize(keep);
    preserve_chart_keysounds(removed, chart);
    return removed.notes.size();
}

void preserve_chart_keysounds(const GameplayChart& source, GameplayChart& converted) {
    // Match the live keysound scheduler's one voice per chart timestamp/asset.
    // Lane and note IDs may change during conversion; neither identifies sound.
    // Existing BGM is already audible, and authored duplicate cues stay intact.
    using Sound = std::pair<int64_t, std::size_t>;
    std::set<Sound> audible;
    for (const auto& cue : converted.audio_cues) audible.emplace(cue.start_sample, cue.asset_id);
    for (const auto& note : converted.notes) {
        for (std::size_t i = 0; i < note_audio_asset_count(note); ++i) {
            audible.emplace(note.start_sample, note_audio_asset_at(note, i));
        }
    }
    bool added = false;
    for (const auto& note : source.notes) {
        if (!std::isfinite(note.audio_gain) || note.audio_gain <= 0.0f) continue;
        for (std::size_t i = 0; i < note_audio_asset_count(note); ++i) {
            const auto asset = note_audio_asset_at(note, i);
            if (asset >= converted.audio_assets.size() ||
                !audible.emplace(note.start_sample, asset).second) continue;
            converted.audio_cues.push_back({note.start_sample, asset, std::clamp(note.audio_gain, 0.0f, 1.0f)});
            added = true;
        }
    }
    if (added) std::stable_sort(converted.audio_cues.begin(), converted.audio_cues.end(),
        [](const AudioCueEvent& a, const AudioCueEvent& b) {
            return a.start_sample != b.start_sample ? a.start_sample < b.start_sample : a.asset_id < b.asset_id;
        });
}

bool NoteEvent::add_audio_asset(std::size_t asset_id) {
    if (asset_id == kInvalidAudioAssetId) {
        return false;
    }
    for (std::size_t i = 0; i < audio_asset_count; ++i) {
        if (audio_asset_ids[i] == asset_id) {
            if (audio_asset_id == kInvalidAudioAssetId) {
                audio_asset_id = audio_asset_ids[0];
            }
            return false;
        }
    }
    if (audio_asset_count >= audio_asset_ids.size()) {
        if (audio_asset_id == kInvalidAudioAssetId) {
            audio_asset_id = audio_asset_ids[0];
        }
        return false;
    }
    audio_asset_ids[audio_asset_count++] = asset_id;
    audio_asset_id = audio_asset_ids[0];
    return true;
}

void NoteEvent::clear_audio_assets() {
    audio_asset_id = kInvalidAudioAssetId;
    audio_asset_count = 0;
    audio_asset_ids.fill(kInvalidAudioAssetId);
}

std::size_t GameplayChart::intern_audio_asset(std::string path) {
    if (path.empty()) {
        return kInvalidAudioAssetId;
    }
    for (std::size_t index = 0; index < audio_assets.size(); ++index) {
        if (audio_assets[index].path == path) {
            return index;
        }
    }
    audio_assets.push_back(AudioAsset{std::move(path)});
    return audio_assets.size() - 1;
}

const std::string* GameplayChart::audio_asset_path(std::size_t asset_id) const {
    if (asset_id >= audio_assets.size()) {
        return nullptr;
    }
    return &audio_assets[asset_id].path;
}

std::size_t GameplayChart::intern_visual_asset(std::string path) {
    if (path.empty()) {
        return kInvalidVisualAssetId;
    }
    for (std::size_t index = 0; index < visual_assets.size(); ++index) {
        if (visual_assets[index].path == path) {
            return index;
        }
    }
    visual_assets.push_back(VisualAsset{std::move(path)});
    return visual_assets.size() - 1;
}

double GameplayChart::visual_position_at(int64_t sample) const {
    return scroll_visual_position_at(scroll_segments, sample);
}

double scroll_visual_position_at(const std::vector<ScrollSegment>& segments, int64_t sample) {
    if (segments.empty()) {
        return static_cast<double>(sample);
    }
    const auto it = std::upper_bound(
        segments.begin(), segments.end(), sample,
        [](int64_t value, const ScrollSegment& segment) { return value < segment.start_sample; });
    const ScrollSegment* segment = nullptr;
    if (it == segments.begin()) {
        segment = &segments.front();
        if (sample <= segment->start_sample) {
            return segment->start_position;
        }
    } else {
        segment = &*std::prev(it);
    }
    const int64_t duration = segment->end_sample - segment->start_sample;
    if (duration <= 0 || sample >= segment->end_sample) {
        return segment->end_position;
    }
    const double progress = std::clamp(
        static_cast<double>(sample - segment->start_sample) / static_cast<double>(duration),
        0.0, 1.0);
    return segment->start_position +
           (segment->end_position - segment->start_position) * progress;
}

double GameplayChart::visual_velocity_at(int64_t sample) const {
    if (scroll_segments.empty()) {
        return 1.0;
    }
    const auto it = std::upper_bound(
        scroll_segments.begin(), scroll_segments.end(), sample,
        [](int64_t value, const ScrollSegment& segment) { return value < segment.start_sample; });
    const ScrollSegment& segment =
        it == scroll_segments.begin() ? scroll_segments.front() : *std::prev(it);
    if (sample < segment.start_sample || sample >= segment.end_sample) {
        return 0.0;
    }
    const int64_t duration = segment.end_sample - segment.start_sample;
    return duration > 0
               ? (segment.end_position - segment.start_position) / static_cast<double>(duration)
               : 0.0;
}

int64_t GameplayChart::sample_after_visual_travel(int64_t start_sample,
                                                  double visual_distance) const {
    if (scroll_segments.empty() || !std::isfinite(visual_distance) || visual_distance <= 0.0) {
        return start_sample;
    }

    double remaining = visual_distance;
    const auto first = std::lower_bound(
        scroll_segments.begin(), scroll_segments.end(), start_sample,
        [](const ScrollSegment& segment, int64_t sample) {
            return segment.end_sample <= sample;
        });
    for (auto it = first; it != scroll_segments.end(); ++it) {
        const ScrollSegment& segment = *it;
        const int64_t segment_duration = segment.end_sample - segment.start_sample;
        const int64_t visible_start = std::max(start_sample, segment.start_sample);
        const int64_t visible_duration = segment.end_sample - visible_start;
        if (segment_duration <= 0 || visible_duration <= 0) {
            continue;
        }

        const double start_ratio = std::clamp(
            static_cast<double>(visible_start - segment.start_sample) /
                static_cast<double>(segment_duration),
            0.0, 1.0);
        const double visible_start_position =
            segment.start_position +
            (segment.end_position - segment.start_position) * start_ratio;
        const double available = std::abs(segment.end_position - visible_start_position);
        if (available <= 1e-12) {
            continue;
        }
        if (available + 1e-12 >= remaining) {
            const double travel_ratio = std::clamp(remaining / available, 0.0, 1.0);
            const int64_t sample_offset = static_cast<int64_t>(std::llround(
                static_cast<double>(visible_duration) * travel_ratio));
            return std::clamp(visible_start + sample_offset,
                              visible_start,
                              segment.end_sample);
        }
        remaining -= available;
    }
    return std::max(start_sample, scroll_segments.back().end_sample);
}

const std::string* GameplayChart::visual_asset_path(std::size_t asset_id) const {
    if (asset_id >= visual_assets.size()) {
        return nullptr;
    }
    return &visual_assets[asset_id].path;
}

std::size_t note_audio_asset_count(const NoteEvent& note) {
    if (note.audio_asset_count > 0) {
        return std::min(note.audio_asset_count, note.audio_asset_ids.size());
    }
    return (note.audio_asset_id == kInvalidAudioAssetId) ? 0u : 1u;
}

std::size_t note_audio_asset_at(const NoteEvent& note, std::size_t index) {
    if (note.audio_asset_count > 0) {
        if (index >= note.audio_asset_count || index >= note.audio_asset_ids.size()) {
            return kInvalidAudioAssetId;
        }
        return note.audio_asset_ids[index];
    }
    if (index == 0) {
        return note.audio_asset_id;
    }
    return kInvalidAudioAssetId;
}

namespace {

int64_t scale_samples(int64_t samples, double rate) {
    if (rate <= 0.0 || !std::isfinite(rate)) {
        return samples;
    }
    return static_cast<int64_t>(std::llround(static_cast<double>(samples) / rate));
}

int64_t add_sample_offset(int64_t sample, int64_t sample_offset) {
    if (sample_offset <= 0) {
        return sample;
    }
    constexpr int64_t kMaxSample = (std::numeric_limits<int64_t>::max)();
    if (sample > kMaxSample - sample_offset) {
        return kMaxSample;
    }
    return sample + sample_offset;
}

}  // namespace

void offset_gameplay_chart_samples(GameplayChart& chart, int64_t sample_offset) {
    if (sample_offset <= 0) {
        return;
    }

    chart.duration_samples = add_sample_offset(chart.duration_samples, sample_offset);
    for (auto& note : chart.notes) {
        note.start_sample = add_sample_offset(note.start_sample, sample_offset);
        if (note.end_sample.has_value()) {
            note.end_sample = add_sample_offset(note.end_sample.value(), sample_offset);
        }
    }
    for (auto& mine : chart.mines) {
        mine.sample = add_sample_offset(mine.sample, sample_offset);
    }
    for (auto& segment : chart.scroll_segments) {
        segment.start_sample = add_sample_offset(segment.start_sample, sample_offset);
        segment.end_sample = add_sample_offset(segment.end_sample, sample_offset);
    }
    for (auto& measure : chart.measure_samples) measure = add_sample_offset(measure, sample_offset);
    for (auto& cue : chart.audio_cues) {
        cue.start_sample = add_sample_offset(cue.start_sample, sample_offset);
    }
    for (auto& cue : chart.visual_cues) {
        cue.start_sample = add_sample_offset(cue.start_sample, sample_offset);
    }
}

void trim_gameplay_chart_before_sample(GameplayChart& chart, int64_t sample) {
    if (sample <= 0) return;
    chart.duration_samples = std::max<int64_t>(0, chart.duration_samples - sample);
    chart.measure_samples.erase(chart.measure_samples.begin(),
        std::lower_bound(chart.measure_samples.begin(), chart.measure_samples.end(), sample));
    for (auto& measure : chart.measure_samples) measure -= sample;
    chart.notes.erase(std::remove_if(chart.notes.begin(), chart.notes.end(),
        [sample](auto& note) {
            const int64_t end = note.end_sample.value_or(note.start_sample);
            if (!note.end_sample.has_value() && note.start_sample < sample) return true;
            if (note.end_sample.has_value() && end <= sample) return true;
            note.start_sample = std::max<int64_t>(0, note.start_sample - sample);
            if (note.end_sample.has_value()) note.end_sample = std::max<int64_t>(1, end - sample);
            return false;
        }), chart.notes.end());
    chart.mines.erase(std::remove_if(chart.mines.begin(), chart.mines.end(),
        [sample](auto& mine) {
            if (mine.sample < sample) return true;
            mine.sample -= sample; return false;
        }), chart.mines.end());
    chart.audio_cues.erase(std::remove_if(chart.audio_cues.begin(), chart.audio_cues.end(),
        [sample](const auto& cue) { return cue.start_sample < sample; }), chart.audio_cues.end());
    for (auto& cue : chart.audio_cues) cue.start_sample -= sample;
    chart.visual_cues.erase(std::remove_if(chart.visual_cues.begin(), chart.visual_cues.end(),
        [sample](const auto& cue) { return cue.start_sample < sample; }), chart.visual_cues.end());
    for (auto& cue : chart.visual_cues) cue.start_sample -= sample;
    chart.scroll_segments.erase(std::remove_if(chart.scroll_segments.begin(), chart.scroll_segments.end(),
        [sample](auto& segment) {
            if (segment.end_sample <= sample) return true;
            segment.start_sample = std::max<int64_t>(0, segment.start_sample - sample);
            segment.end_sample = std::max<int64_t>(0, segment.end_sample - sample);
            return false;
        }), chart.scroll_segments.end());
}

GameplayChart from_bms_timeline(const chart::BmsTimeline& timeline, double rate) {
    GameplayChart chart;
    int max_lane = 0;

    for (const auto& scheduled : timeline.events) {
        if (scheduled.event.type != chart::BmsNormalizedEventType::Note) {
            continue;
        }
        if (!scheduled.event.lane.has_value()) {
            continue;
        }
        NoteEvent note;
        note.lane = static_cast<int>(scheduled.event.lane.value());
        note.start_sample = scale_samples(scheduled.time_samples, rate);
        chart.notes.push_back(note);
        max_lane = std::max(max_lane, note.lane);
    }

    chart.lane_count = max_lane > 0 ? max_lane : 10;
    chart.scroll_segments.reserve(timeline.scroll_segments.size());
    for (const auto& segment : timeline.scroll_segments) {
        chart.scroll_segments.push_back(ScrollSegment{
            scale_samples(segment.start_sample, rate),
            scale_samples(segment.end_sample, rate),
            segment.start_position,
            segment.end_position,
        });
    }

    chart.duration_samples = scale_samples(timeline.duration_samples, rate);
    for (const auto sample : timeline.measure_samples)
        chart.measure_samples.push_back(scale_samples(sample, rate));

    std::stable_sort(chart.notes.begin(), chart.notes.end(), [](const NoteEvent& lhs, const NoteEvent& rhs) {
        if (lhs.start_sample != rhs.start_sample) {
            return lhs.start_sample < rhs.start_sample;
        }
        return lhs.lane < rhs.lane;
    });

    return chart;
}

}  // namespace tenriff::gameplay
