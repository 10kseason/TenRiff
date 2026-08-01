#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "chart/BmsTimeline.h"
#include "chart/OsuManiaLoader.h"

namespace tenriff::gameplay {

constexpr std::size_t kInvalidAudioAssetId = std::numeric_limits<std::size_t>::max();
constexpr std::size_t kInvalidVisualAssetId = std::numeric_limits<std::size_t>::max();
constexpr std::size_t kMaxNoteAudioAssets = 4;

struct AudioAsset {
    std::string path;
};

struct NoteEvent {
    int lane = 0;  // 1-based lane index.
    int64_t start_sample = 0;
    std::optional<int64_t> end_sample;  // Hold note end sample if present.
    bool release_required = false;      // Tail uses release timing judgement when true.
    std::size_t audio_asset_id = kInvalidAudioAssetId;
    std::size_t note_id = 0;
    float audio_gain = 1.0f;
    std::size_t audio_asset_count = 0;
    std::array<std::size_t, kMaxNoteAudioAssets> audio_asset_ids{
        kInvalidAudioAssetId,
        kInvalidAudioAssetId,
        kInvalidAudioAssetId,
        kInvalidAudioAssetId,
    };

    bool add_audio_asset(std::size_t asset_id);
    void clear_audio_assets();
};

struct AudioCueEvent {
    int64_t start_sample = 0;
    std::size_t asset_id = kInvalidAudioAssetId;
};

enum class VisualLayer {
    Base,
    Overlay,
};

struct VisualAsset {
    std::string path;
};

struct VisualCueEvent {
    int64_t start_sample = 0;
    std::size_t asset_id = kInvalidVisualAssetId;
    VisualLayer layer = VisualLayer::Base;
};

struct GameplayChart {
    int lane_count = 0;
    // 1-based source lanes with scratch semantics. This lets key-mode transforms
    // distinguish 7+1 SP from a native eight-key chart after BMS normalization.
    std::vector<int> scratch_lanes;
    // Size of each player field for DP layouts. Zero means a single field.
    // Scratch-aware conversion strips scratches before updating this value.
    int lane_group_size = 0;
    int64_t duration_samples = 0;
    std::vector<AudioAsset> audio_assets;
    std::vector<NoteEvent> notes;
    std::vector<AudioCueEvent> audio_cues;
    std::vector<VisualAsset> visual_assets;
    std::vector<VisualCueEvent> visual_cues;

    [[nodiscard]] std::size_t intern_audio_asset(std::string path);
    [[nodiscard]] const std::string* audio_asset_path(std::size_t asset_id) const;
    [[nodiscard]] std::size_t intern_visual_asset(std::string path);
    [[nodiscard]] const std::string* visual_asset_path(std::size_t asset_id) const;
};

[[nodiscard]] std::size_t note_audio_asset_count(const NoteEvent& note);
[[nodiscard]] std::size_t note_audio_asset_at(const NoteEvent& note, std::size_t index);

void offset_gameplay_chart_samples(GameplayChart& chart, int64_t sample_offset);

GameplayChart from_bms_timeline(const chart::BmsTimeline& timeline, double rate);
GameplayChart from_osu_mania(const chart::OsuManiaChart& chart, int sample_rate, double rate);

}  // namespace tenriff::gameplay
