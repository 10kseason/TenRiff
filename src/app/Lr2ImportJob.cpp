#include "app/Lr2ImportJob.h"

#include <cctype>
#include <chrono>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

#include "app/ChartFileHash.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

Lr2ImportResult prepare_lr2_import(const std::filesystem::path& database,
                                  const std::vector<std::filesystem::path>& song_folders,
                                  const std::atomic<bool>* cancel) {
    Lr2ImportResult result;
    const auto cancelled = [&] {
        result.cancelled = cancel && cancel->load(std::memory_order_relaxed);
        return result.cancelled;
    };
    if (cancelled()) return result;
    try {
        const auto db = read_lr2_score_db(database);
        if (!db.success()) { result.error = db.error; return result; }
        result.rows = db.rows.size();
        std::unordered_map<std::string, const Lr2ScoreRow*> pending;
        for (const auto& row : db.rows) {
            if (cancelled()) return result;
            auto lamp = lr2_base_lamp(row);
            if (!lamp || row.hash.size() != 32) continue;
            result.lamps.emplace(row.hash, std::move(*lamp));
            pending.emplace(row.hash, &row);
        }
        std::unordered_set<std::string> visited;
        for (const auto& folder : song_folders) {
            if (cancelled()) return result;
            if (!visited.insert(folder.lexically_normal().generic_u8string()).second) continue;
            std::error_code ec;
            std::filesystem::recursive_directory_iterator it(folder,
                std::filesystem::directory_options::skip_permission_denied, ec), end;
            if (ec) { ++result.skipped_folders; continue; }
            while (it != end) {
                if (cancelled()) return result;
                if (it->is_regular_file(ec)) {
                    std::string ext = it->path().extension().u8string();
                    for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    if (ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".pms") {
                        ++result.scanned;
                        const auto hashes = hash_chart_file(it->path());
                        const auto found = hashes.valid() ? pending.find(hashes.md5) : pending.end();
                        if (found != pending.end()) {
                            const auto loaded = ChartLoader{}.load(it->path().u8string(), 44100, 1.0);
                            Lr2ExEvaluation evaluation;
                            if (loaded.success()) evaluation = evaluate_lr2_ex_worst_case(
                                *found->second, loaded.chart, loaded.format);
                            else evaluation.reason = "chart_load_failed";
                            apply_lr2_ex_evaluation(result.lamps[hashes.md5], evaluation);
                            pending.erase(found);
                        }
                    }
                }
                ec.clear();
                it.increment(ec);
                if (ec) { ++result.skipped_folders; break; }
            }
        }
        cancelled();
    } catch (const std::exception&) {
        // Do not leak local paths from exception messages into user-facing text.
        result.error = "Could not read the LR2 database or song folders.";
    }
    return result;
}

bool save_lr2_import(const std::filesystem::path& profile,
                     const Lr2ImportedLamps& lamps, std::string& error) {
    error.clear();
    std::error_code ec;
    if (!std::filesystem::is_directory(profile, ec)) {
        error = "Profile folder does not exist.";
        return false;
    }
    const auto output = profile / "lr2_import.json";
    static std::atomic<unsigned long long> serial{0};
    const auto temporary = profile / ("lr2_import.json.tmp-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
        std::to_string(serial.fetch_add(1)));
    try {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << serialize_lr2_imported_lamps(lamps) << '\n';
        stream.close();
        if (!stream) error = "Cannot write the import file.";
        else {
            std::filesystem::rename(temporary, output, ec);
            if (ec) error = "Cannot replace the import file.";
        }
    } catch (const std::exception&) {
        error = "Cannot write the import file.";
    }
    if (!error.empty()) std::filesystem::remove(temporary, ec);
    return error.empty();
}

}  // namespace tenriff::app
