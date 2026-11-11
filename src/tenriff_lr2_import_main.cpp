// Imports LR2 lamps and LR2oraja scores into a TenRiff profile for display.
//
//   tenriff-lr2-import <score .db> <profile folder> [songs folder ...]
//
// Easy/Groove/Hard lamps are copied by chart MD5. For charts found under the
// given song folders, the LR2 judgement order is also replayed through the
// EX-HARD gauge under worst-case timing; only plays that survive that are
// recorded as EX. The LR2 database is opened read-only and never modified.
#include "app/Lr2ImportJob.h"
#include "util/Utf8Compat.h"

#include <filesystem>
#include <iostream>
#include <map>
#include <vector>

namespace {
int run(const std::vector<std::filesystem::path>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: tenriff-lr2-import <LR2 or LR2oraja score .db> <profile folder> [songs folder ...]\n";
        return 2;
    }
    const std::vector<std::filesystem::path> folders(args.begin() + 2, args.end());
    const auto imported = tenriff::app::prepare_lr2_import(args[0], folders);
    if (!imported.success()) {
        std::cerr << "[error] " << imported.error << '\n';
        return 1;
    }
    std::string error;
    if (!tenriff::app::save_score_import(args[1], imported, error)) {
        std::cerr << "[error] " << error << '\n';
        return 1;
    }
    if (imported.source == tenriff::app::Lr2ScoreDbSource::Lr2oraja) {
        std::cout << "LR2oraja rows: " << imported.rows << "\nImported charts: " << imported.lr2oraja_scores.size()
                  << "\nSkipped rows: " << imported.skipped_rows << "\nWritten: "
                  << (args[1] / tenriff::app::kLr2orajaImportFilename).u8string() << '\n';
        return 0;
    }
    std::map<std::string, int> summary;
    for (const auto& [hash, lamp] : imported.lamps) {
        ++summary["lamp." + lamp.clear_status];
        ++summary["ex." + lamp.ex_note];
    }
    std::cout << "LR2 rows: " << imported.rows << "\nImported lamps: " << imported.lamps.size()
              << "\nChart files scanned: " << imported.scanned << '\n';
    for (const auto& [key, count] : summary) std::cout << "  " << key << ": " << count << '\n';
    if (imported.skipped_folders) std::cerr << "[warn] Some song folders could not be scanned.\n";
    std::cout << "Written: " << (args[1] / "lr2_import.json").u8string() << '\n';
    return 0;
}
}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::filesystem::path> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    return run(args);
}
#else
int main(int argc, char** argv) {
    std::vector<std::filesystem::path> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    return run(args);
}
#endif
