#include "gameplay/GameplayChart.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace tenriff::gameplay {

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
    if (scroll_segments.empty()) {
        return static_cast<double>(sample);
    }
    const auto it = std::upper_bound(
        scroll_segments.begin(), scroll_segments.end(), sample,
        [](int64_t value, const ScrollSegment& segment) { return value < segment.start_sample; });
    const ScrollSegment* segment = nullptr;
    if (it == scroll_segments.begin()) {
        segment = &scroll_segments.front();
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
    for (auto& cue : chart.audio_cues) {
        cue.start_sample = add_sample_offset(cue.start_sample, sample_offset);
    }
    for (auto& cue : chart.visual_cues) {
        cue.start_sample = add_sample_offset(cue.start_sample, sample_offset);
    }
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

    std::stable_sort(chart.notes.begin(), chart.notes.end(), [](const NoteEvent& lhs, const NoteEvent& rhs) {
        if (lhs.start_sample != rhs.start_sample) {
            return lhs.start_sample < rhs.start_sample;
        }
        return lhs.lane < rhs.lane;
    });

    return chart;
}

}  // namespace tenriff::gameplay
