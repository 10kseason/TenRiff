#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "app/Lr2ScoreDb.h"

namespace tenriff::app {

inline constexpr char kLr2orajaImportFilename[] = "lr2oraja_import.json";

using Lr2orajaLnScores = std::array<std::optional<Lr2orajaScore>, kLr2orajaLnModeCount>;
using Lr2orajaImportedScores = std::unordered_map<std::string, Lr2orajaLnScores>;

[[nodiscard]] std::string_view lr2oraja_clear_label(int clear) noexcept;
[[nodiscard]] std::string serialize_lr2oraja_scores(const Lr2orajaImportedScores& scores);
[[nodiscard]] Lr2orajaImportedScores parse_lr2oraja_scores(std::string_view json);
[[nodiscard]] Lr2orajaImportedScores load_lr2oraja_scores(const std::filesystem::path& profile);

}  // namespace tenriff::app
