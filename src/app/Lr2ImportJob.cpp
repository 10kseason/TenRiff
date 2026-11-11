#include "app/Lr2ImportJob.h"

#include <cctype>
#include <chrono>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

#include "app/ChartFileHash.h"
#include "app/BmsGameplayBuilder.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {
Lr2ExEvaluation evaluate_chart(const Lr2ScoreRow& row, const std::filesystem::path& path) {
    // EX needs the same native notes, RANK, TOTAL, LN/mines and RANDOM flags as
    // gameplay, but never audio or background paths. In particular, missing
    // keysounds must not trigger a directory-wide media lookup per chart.
    const auto parsed = chart::BmsParser{}.parseFile(path.u8string());
    if (parsed.success()) {
        const auto normalized = chart::BmsChartNormalizer{}.normalize(parsed.chart);
        if (normalized.success()) {
            const auto timeline = chart::BmsTimelineBuilder{}.build(normalized.chart, 44100);
            if (timeline.success()) {
                auto built = build_bms_gameplay_chart(timeline.timeline, parsed.chart, 1.0);
                // Gameplay falls back to EASY for absent/unsupported RANK.
                // Import must instead refuse EX when the LR2 window is unknown.
                const auto header = parsed.chart.headers.find("RANK");
                const std::string_view rank = header == parsed.chart.headers.end()
                    ? std::string_view{} : std::string_view(header->second);
                const auto first = rank.find_first_not_of(" \t\r\n\f\v");
                if (first == std::string_view::npos || first != rank.find_last_not_of(" \t\r\n\f\v") ||
                    rank[first] < '0' || rank[first] > '3') built.chart.bms_rank.reset();
                return evaluate_lr2_ex_worst_case(row, built.chart, ChartFormat::Bms);
            }
        }
    }
    Lr2ExEvaluation failed;
    failed.reason = "chart_load_failed";
    return failed;
}
}  // namespace

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
        result.source = db.source;
        result.skipped_rows = db.skipped_rows;
        if (db.source == Lr2ScoreDbSource::Lr2oraja) {
            result.rows = db.lr2oraja_rows.size() + db.skipped_rows;
            // The song index already has the same SHA-256 keys. No directory
            // scan or TenRiff gauge evaluation is needed for these scores.
            for (const auto& row : db.lr2oraja_rows) {
                if (cancelled()) return result;
                result.lr2oraja_scores[row.sha256][row.ln_mode] = row.score;
            }
            if (result.rows != 0 && result.lr2oraja_scores.empty())
                result.error = "No usable LR2oraja chart scores.";
            cancelled();
            return result;
        }
        result.rows = db.rows.size();
        std::unordered_map<std::string, const Lr2ScoreRow*> pending;
        for (const auto& row : db.rows) {
            if (cancelled()) return result;
            auto lamp = lr2_base_lamp(row);
            if (!lamp || row.hash.size() != 32) continue;
            result.lamps.emplace(row.hash, std::move(*lamp));
            pending.emplace(row.hash, &row);
        }
        std::unordered_set<std::filesystem::path> visited;
        for (const auto& source_folder : song_folders) {
            if (cancelled()) return result;
            if (pending.empty()) break;
            std::error_code ec;
            auto folder = std::filesystem::weakly_canonical(source_folder, ec);
            if (ec) { ++result.skipped_folders; continue; }
            if (!visited.insert(folder).second) continue;
            std::filesystem::recursive_directory_iterator it(folder,
                std::filesystem::directory_options::skip_permission_denied, ec), end;
            if (ec) { ++result.skipped_folders; continue; }
            while (it != end && !pending.empty()) {
                if (cancelled()) return result;
                // Preserve first-match order while avoiding repeated traversal
                // when registered roots overlap (child first or parent first).
                if (it->is_directory(ec) && !visited.insert(it->path()).second)
                    it.disable_recursion_pending();
                if (it->is_regular_file(ec)) {
                    std::string ext = it->path().extension().u8string();
                    for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    if (ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".pms") {
                        ++result.scanned;
                        const auto md5 = md5_chart_file(it->path());
                        const auto found = pending.find(md5);
                        if (found != pending.end()) {
                            if (cancelled()) return result;
                            apply_lr2_ex_evaluation(result.lamps[md5], evaluate_chart(*found->second, it->path()));
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
        result.error = "Could not read the score database or song folders.";
    }
    return result;
}

namespace {
template <typename Serialize>
bool save_import_file(const std::filesystem::path& profile, const std::string& filename,
                      Serialize serialize, std::string& error) {
    error.clear();
    std::error_code ec;
    if (!std::filesystem::is_directory(profile, ec)) {
        error = "Profile folder does not exist.";
        return false;
    }
    const auto output = profile / filename;
    static std::atomic<unsigned long long> serial{0};
    const auto temporary = profile / (filename + ".tmp-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
        std::to_string(serial.fetch_add(1)));
    try {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << serialize() << '\n';
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
}  // namespace

bool save_lr2_import(const std::filesystem::path& profile,
                     const Lr2ImportedLamps& lamps, std::string& error) {
    return save_import_file(profile, "lr2_import.json",
        [&] { return serialize_lr2_imported_lamps(lamps); }, error);
}

bool save_score_import(const std::filesystem::path& profile,
                       const Lr2ImportResult& result, std::string& error) {
    if (!result.success()) {
        error = "Score import did not complete.";
        return false;
    }
    if (result.source == Lr2ScoreDbSource::Lr2oraja) {
        return save_import_file(profile, kLr2orajaImportFilename,
            [&] { return serialize_lr2oraja_scores(result.lr2oraja_scores); }, error);
    }
    return save_lr2_import(profile, result.lamps, error);
}

}  // namespace tenriff::app
