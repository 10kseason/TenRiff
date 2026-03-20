#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "chart/OsuManiaLoader.h"
#include "gameplay/GameplayChart.h"

namespace tenriff::app {

struct OsuResolvedNoteHitsound {
    std::size_t asset_count = 0;
    std::array<std::string, tenriff::gameplay::kMaxNoteAudioAssets> asset_paths{};
    float gain = 1.0f;
};

struct OsuHitsoundResolveResult {
    std::vector<OsuResolvedNoteHitsound> note_hitsounds;
    std::vector<std::string> messages;
};

[[nodiscard]] OsuHitsoundResolveResult resolve_osu_mania_hitsounds(
    const std::filesystem::path& chart_path,
    const chart::OsuManiaChart& chart);

}  // namespace tenriff::app
