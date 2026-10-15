#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "chart/BmsTimeline.h"

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
    float gain = 1.0f;
};

struct MineEvent {
    int lane = 0;
    int64_t sample = 0;
    double damage_percent = 0.0;
    bool instant_kill = false;
    std::size_t audio_asset_id = kInvalidAudioAssetId;
    std::size_t mine_id = 0;
};

struct ScrollSegment {
    int64_t start_sample = 0;
    int64_t end_sample = 0;
    double start_position = 0.0;
    double end_position = 0.0;
};

// Shared by chart queries and render-time lookup of an immutable scroll map.
// An empty map uses sample coordinates, as charts without scroll events do.
[[nodiscard]] double scroll_visual_position_at(const std::vector<ScrollSegment>& segments,
                                              int64_t sample);

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
    // Authored BMS #RANK (0 very hard through 3 easy). Absent for synthetic
    // charts; the BMS builder supplies Easy when the header is missing/invalid.
    std::optional<int> bms_rank;
    // Positive authored TOTAL only. Missing/invalid values use the runtime fallback.
    std::optional<double> bms_total;
    bool bms_has_random = false;
    // 1-based source lanes with scratch semantics. This lets key-mode transforms
    // distinguish 7+1 SP from a native eight-key chart after BMS normalization.
    std::vector<int> scratch_lanes;
    // Size of each player field for DP layouts. Zero means a single field.
    // Scratch-aware conversion strips scratches before updating this value.
    int lane_group_size = 0;
    int64_t duration_samples = 0;
    std::vector<AudioAsset> audio_assets;
    std::vector<NoteEvent> notes;
    std::vector<MineEvent> mines;
    std::vector<ScrollSegment> scroll_segments;
    std::vector<AudioCueEvent> audio_cues;
    std::vector<VisualAsset> visual_assets;
    std::vector<VisualCueEvent> visual_cues;

    [[nodiscard]] std::size_t intern_audio_asset(std::string path);
    [[nodiscard]] const std::string* audio_asset_path(std::size_t asset_id) const;
    [[nodiscard]] std::size_t intern_visual_asset(std::string path);
    [[nodiscard]] const std::string* visual_asset_path(std::size_t asset_id) const;
    [[nodiscard]] double visual_position_at(int64_t sample) const;
    [[nodiscard]] double visual_velocity_at(int64_t sample) const;
    [[nodiscard]] int64_t sample_after_visual_travel(int64_t start_sample,
                                                     double visual_distance) const;
};

[[nodiscard]] std::size_t note_audio_asset_count(const NoteEvent& note);
[[nodiscard]] std::size_t note_audio_asset_at(const NoteEvent& note, std::size_t index);

// Preserve omitted note heads at authored times without doubling retained/BGM sounds.
void preserve_chart_keysounds(const GameplayChart& source, GameplayChart& converted);

// BMS exporters can emit a visible tap and an explicit LN head at one cell.
// Keep the complete hold and its sounds; never merge nearby or other-lane notes.
std::size_t coalesce_bms_long_note_heads(GameplayChart& chart);

void offset_gameplay_chart_samples(GameplayChart& chart, int64_t sample_offset);
void trim_gameplay_chart_before_sample(GameplayChart& chart, int64_t sample);

GameplayChart from_bms_timeline(const chart::BmsTimeline& timeline, double rate);

}  // namespace tenriff::gameplay
