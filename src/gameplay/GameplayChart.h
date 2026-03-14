#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "chart/BmsTimeline.h"
#include "chart/OsuManiaLoader.h"

namespace tenriff::gameplay {

constexpr std::size_t kInvalidAudioAssetId = std::numeric_limits<std::size_t>::max();

struct AudioAsset {
    std::string path;
};

struct NoteEvent {
    int lane = 0;  // 1-based lane index.
    int64_t start_sample = 0;
    std::optional<int64_t> end_sample;  // Hold note end sample if present.
    std::size_t audio_asset_id = kInvalidAudioAssetId;
    std::size_t note_id = 0;
};

struct AudioCueEvent {
    int64_t start_sample = 0;
    std::size_t asset_id = kInvalidAudioAssetId;
};

struct GameplayChart {
    int lane_count = 0;
    int64_t duration_samples = 0;
    std::vector<AudioAsset> audio_assets;
    std::vector<NoteEvent> notes;
    std::vector<AudioCueEvent> audio_cues;

    [[nodiscard]] std::size_t intern_audio_asset(std::string path);
    [[nodiscard]] const std::string* audio_asset_path(std::size_t asset_id) const;
};

GameplayChart from_bms_timeline(const chart::BmsTimeline& timeline, double rate);
GameplayChart from_osu_mania(const chart::OsuManiaChart& chart, int sample_rate, double rate);

}  // namespace tenriff::gameplay
