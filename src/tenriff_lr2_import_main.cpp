// Imports LR2 clear lamps into a TenRiff profile as display-only data.
//
//   tenriff-lr2-import <LR2 score .db> <profile folder> [songs folder ...]
//
// Easy/Groove/Hard lamps are copied by chart MD5. For charts found under the
// given song folders, the LR2 judgement order is also replayed through the
// EX-HARD gauge under worst-case timing; only plays that survive that are
// recorded as EX. The LR2 database is opened read-only and never modified.
#include "app/ChartFileHash.h"
#include "app/ChartLoader.h"
#include "app/Lr2ClearImport.h"
#include "app/Lr2ScoreDb.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using namespace tenriff;

bool is_bms_file(const std::filesystem::path& path) {
    std::string extension = path.extension().u8string();
    for (char& ch : extension) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return extension == ".bms" || extension == ".bme" || extension == ".bml" || extension == ".pms";
}

int run(const std::vector<std::filesystem::path>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: tenriff-lr2-import <LR2 score .db> <profile folder> [songs folder ...]\n";
        return 2;
    }
    const auto db = app::read_lr2_score_db(args[0]);
    if (!db.success()) {
        std::cerr << "[error] " << db.error << '\n';
        return 1;
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(args[1], ec)) {
        std::cerr << "[error] Profile folder does not exist.\n";
        return 1;
    }

    app::Lr2ImportedLamps lamps;
    std::unordered_map<std::string, const app::Lr2ScoreRow*> pending;
    std::map<std::string, int> summary;
    for (const auto& row : db.rows) {
        auto lamp = app::lr2_base_lamp(row);
        if (!lamp || row.hash.size() != 32) continue;
        ++summary["lamp." + lamp->clear_status];
        lamps.emplace(row.hash, std::move(*lamp));
        pending.emplace(row.hash, &row);
    }

    std::size_t scanned = 0;
    for (std::size_t i = 2; i < args.size(); ++i) {
        std::filesystem::recursive_directory_iterator it(
            args[i], std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) {
            std::cerr << "[warn] Cannot scan a songs folder.\n";
            continue;
        }
        for (const std::filesystem::recursive_directory_iterator end; it != end; it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file(ec) || !is_bms_file(it->path())) continue;
            ++scanned;
            const auto hashes = app::hash_chart_file(it->path());
            if (!hashes.valid()) continue;
            const auto found = pending.find(hashes.md5);
            if (found == pending.end()) continue;
            const auto loaded = app::ChartLoader{}.load(it->path().u8string(), 44100, 1.0);
            app::Lr2ExEvaluation evaluation;
            if (loaded.success()) {
                evaluation = app::evaluate_lr2_ex_worst_case(*found->second, loaded.chart, loaded.format);
            } else {
                evaluation.reason = "chart_load_failed";
            }
            app::apply_lr2_ex_evaluation(lamps[hashes.md5], evaluation);
            pending.erase(found);
        }
    }
    for (const auto& entry : lamps) ++summary["ex." + entry.second.ex_note];

    const auto output = std::filesystem::path(args[1]) / "lr2_import.json";
    const auto temporary = std::filesystem::path(args[1]) / "lr2_import.json.tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << app::serialize_lr2_imported_lamps(lamps) << '\n';
        if (!stream) {
            std::cerr << "[error] Cannot write the import file.\n";
            return 1;
        }
    }
    std::filesystem::rename(temporary, output, ec);
    if (ec) {
        std::cerr << "[error] Cannot replace the import file.\n";
        return 1;
    }

    std::cout << "LR2 rows: " << db.rows.size() << "\nImported lamps: " << lamps.size()
              << "\nChart files scanned: " << scanned << '\n';
    for (const auto& [key, count] : summary) std::cout << "  " << key << ": " << count << '\n';
    std::cout << "Written: " << output.u8string() << '\n';
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
