#pragma once

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

#include "app/Lr2ClearImport.h"
#include "app/Lr2orajaScoreImport.h"

namespace tenriff::app {

struct Lr2ImportResult {
    Lr2ScoreDbSource source = Lr2ScoreDbSource::Lr2;
    Lr2ImportedLamps lamps;
    Lr2orajaImportedScores lr2oraja_scores;
    std::size_t rows = 0;
    std::size_t skipped_rows = 0;
    std::size_t scanned = 0;
    std::size_t skipped_folders = 0;
    std::string error;
    bool cancelled = false;
    [[nodiscard]] bool success() const { return error.empty() && !cancelled; }
};

// Shared by the menu worker and CLI. Only reads inputs; the caller commits a
// completed result, so cancel/close cannot partially replace profile data.
[[nodiscard]] Lr2ImportResult prepare_lr2_import(
    const std::filesystem::path& database,
    const std::vector<std::filesystem::path>& song_folders,
    const std::atomic<bool>* cancel = nullptr);

// An atomic replacement keeps the previous import intact if writing fails.
[[nodiscard]] bool save_lr2_import(const std::filesystem::path& profile,
                                  const Lr2ImportedLamps& lamps, std::string& error);
[[nodiscard]] bool save_score_import(const std::filesystem::path& profile,
                                    const Lr2ImportResult& result, std::string& error);

}  // namespace tenriff::app
