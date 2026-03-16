#include "app/SongIndex.h"
#include "app/SongIndexBudget.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <malloc.h>
#include <mutex>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "app/BmsGameplayBuilder.h"
#include "chart/BmsChartNorm.h"
#include "chart/BmsParser.h"
#include "chart/BmsTimeline.h"
#include "chart/OsuDifficulty.h"
#include "chart/OsuManiaLoader.h"
#include "config/SimpleJson.h"
#include "util/Utf8Compat.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace tenriff::app {

namespace {

constexpr int kSongIndexVersion = 7;

std::filesystem::path path_from_utf8(std::string_view value) {
    try {
        return util::path_from_utf8_lossy(value);
    } catch (...) {
        return {};
    }
}

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string normalize_cache_key_path(std::string_view value) {
    std::filesystem::path raw_path = path_from_utf8(value);
    if (raw_path.empty()) {
        return {};
    }
    try {
        std::error_code ec;
        std::filesystem::path normalized = raw_path;
        const std::filesystem::path absolute = std::filesystem::absolute(normalized, ec);
        if (!ec && !absolute.empty()) {
            normalized = absolute;
        } else {
            ec.clear();
        }

        const std::filesystem::path canonical = std::filesystem::weakly_canonical(normalized, ec);
        if (!ec && !canonical.empty()) {
            normalized = canonical;
        } else {
            normalized = normalized.lexically_normal();
        }
        return to_lower(normalized.generic_u8string());
    } catch (...) {
        return {};
    }
}

std::uint64_t fnv1a_64(std::string_view value) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : value) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string hex_u64(std::uint64_t value) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << value;
    return out.str();
}

std::string trim_copy(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

bool is_bms_extension(std::string_view ext) {
    return ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".pms";
}

bool is_osu_extension(std::string_view ext) {
    return ext == ".osu";
}

bool is_menu_bms_key_count(int key_count) {
    return (key_count >= 4 && key_count <= 10) || key_count == 16;
}

bool is_menu_song_entry(const SongEntry& entry, const SongIndexOptions& options) {
    const std::string format = to_lower(entry.format);
    if (format == "bms") {
        return is_menu_bms_key_count(entry.key_count);
    }
    if (options.include_osu && format == "osu") {
        return entry.key_count >= 4 && entry.key_count <= 10;
    }
    return false;
}

bool needs_difficulty_refresh(const SongEntry& entry) {
    static_cast<void>(entry);
    return false;
}

int64_t file_time_seconds(const std::filesystem::file_time_type& time) {
    using namespace std::chrono;
    return duration_cast<seconds>(time.time_since_epoch()).count();
}

std::string fallback_title(const std::filesystem::path& path) {
    try {
        if (!path.stem().empty()) {
            return util::sanitize_ui_text(path.stem().u8string());
        }
        return util::sanitize_ui_text(path.filename().u8string());
    } catch (...) {
        return {};
    }
}

std::string read_text_file(const std::filesystem::path& path, std::string* error = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) {
            *error = "Failed to open file.";
        }
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void to_upper_ascii(std::string& value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'a' && ch <= 'z') {
            return static_cast<char>(ch - ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
}

std::string canonical_bms_note_channel(std::string_view channel) {
    std::string normalized = trim_copy(channel);
    to_upper_ascii(normalized);
    if (normalized.size() != 2) {
        return {};
    }
    if (normalized[0] == '5') {
        normalized[0] = '1';
    } else if (normalized[0] == '6') {
        normalized[0] = '2';
    }
    return normalized;
}

bool is_bms_long_note_channel(std::string_view channel) {
    return channel.size() == 2 && (channel[0] == '5' || channel[0] == '6');
}

const std::vector<std::string>& difficulty_lane_order(std::string_view layout_label) {
    static const std::vector<std::string> kEmpty;
    static const std::vector<std::string> k5p1 = {"16", "11", "12", "13", "14", "15"};
    static const std::vector<std::string> k7p1 = {"16", "11", "12", "13", "14", "15", "18", "19"};
    static const std::vector<std::string> k10p2 = {"16", "11", "12", "13", "14", "15", "21", "22", "23", "24", "25", "26"};
    static const std::vector<std::string> k14p2 = {"16", "11", "12", "13", "14", "15", "18", "19",
                                                    "21", "22", "23", "24", "25", "28", "29", "26"};
    static const std::vector<std::string> kPms = {"11", "12", "13", "14", "15", "22", "23", "24", "25"};

    const std::string layout = util::sanitize_ui_text(std::string(layout_label));
    if (layout == "5+1 SP") {
        return k5p1;
    }
    if (layout == "7+1 SP") {
        return k7p1;
    }
    if (layout == "10+2 DP") {
        return k10p2;
    }
    if (layout == "14+2 DP") {
        return k14p2;
    }
    if (layout == "PMS 9K") {
        return kPms;
    }
    return kEmpty;
}

std::optional<int> difficulty_lane_for_channel(const chart::BmsChart& parsed_chart, std::string_view channel) {
    const auto& order = difficulty_lane_order(parsed_chart.layout_label);
    const std::string canonical = canonical_bms_note_channel(channel);
    if (canonical.empty()) {
        return std::nullopt;
    }

    if (!order.empty()) {
        const auto it = std::find(order.begin(), order.end(), canonical);
        if (it == order.end()) {
            return std::nullopt;
        }
        return static_cast<int>(std::distance(order.begin(), it));
    }

    const auto lane = parsed_chart.lane_mapping.laneForChannel(canonical);
    if (!lane.has_value()) {
        return std::nullopt;
    }
    return static_cast<int>(lane.value() - 1);
}

struct OsuMenuProbe {
    std::optional<bool> mania_mode;
    std::optional<int> key_count;
};

OsuMenuProbe probe_osu_menu_candidate(std::string_view content) {
    OsuMenuProbe probe;
    std::istringstream stream{std::string(content)};
    std::string line;
    std::string section;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto comment = line.find("//");
        if (comment == 0) {
            continue;
        }
        if (comment != std::string::npos) {
            line.erase(comment);
        }

        auto trim_in_place = [](std::string& text) {
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
                text.erase(text.begin());
            }
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
                text.pop_back();
            }
        };
        trim_in_place(line);
        if (line.empty()) {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            to_upper_ascii(section);
            continue;
        }

        const auto sep = line.find(':');
        if (sep == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, sep);
        std::string value = line.substr(sep + 1);
        trim_in_place(key);
        trim_in_place(value);
        to_upper_ascii(key);
        if (section == "GENERAL" && key == "MODE") {
            try {
                probe.mania_mode = (std::stoi(value) == 3);
            } catch (...) {
            }
        } else if (section == "DIFFICULTY" && (key == "KEYCOUNT" || key == "CIRCLESIZE")) {
            try {
                probe.key_count = std::stoi(value);
            } catch (...) {
            }
        }
    }
    return probe;
}

std::string relative_path_string(const std::filesystem::path& root, const std::filesystem::path& full_path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path relative = fs::relative(full_path, root, ec);
    if (ec || relative.empty()) {
        relative = full_path.lexically_relative(root);
    }
    if (relative.empty()) {
        relative = full_path.filename();
    }
    return relative.generic_u8string();
}

std::optional<chart::OsuDifficultyMetrics> calculate_bms_difficulty(const chart::BmsChart& parsed_chart) {
    const int key_count = parsed_chart.declared_key_count > 0 ? parsed_chart.declared_key_count : 10;
    if (key_count < 4 || (key_count > 10 && key_count != 16)) {
        return std::nullopt;
    }

    chart::BmsChartNormalizer normalizer;
    auto normalized = normalizer.normalize(parsed_chart);
    if (!normalized.success()) {
        return std::nullopt;
    }

    chart::BmsTimelineBuilder timeline_builder;
    auto timeline = timeline_builder.build(normalized.chart, 1000);
    if (!timeline.success()) {
        return std::nullopt;
    }

    chart::OsuManiaChart difficulty_chart;
    difficulty_chart.key_count = key_count;
    difficulty_chart.base_bpm = parsed_chart.base_bpm;
    difficulty_chart.overall_difficulty = 8.0;

    struct PendingLongNote {
        int column = 0;
        int64_t start_time_ms = 0;
    };

    const std::string lnobj = [&parsed_chart]() {
        auto it = parsed_chart.headers.find("LNOBJ");
        if (it == parsed_chart.headers.end()) {
            return std::string{};
        }
        std::string value = trim_copy(it->second);
        to_upper_ascii(value);
        return value;
    }();

    std::unordered_map<int, PendingLongNote> pending_long_notes;
    std::unordered_map<int, std::size_t> last_normal_note_by_column;
    difficulty_chart.notes.reserve(timeline.timeline.events.size());
    for (const auto& scheduled : timeline.timeline.events) {
        if (scheduled.event.type != chart::BmsNormalizedEventType::Note) {
            continue;
        }

        const auto column = difficulty_lane_for_channel(parsed_chart, scheduled.event.channel);
        if (!column.has_value()) {
            continue;
        }

        const int64_t time_ms = std::max<int64_t>(0, scheduled.time_samples);
        std::string object_id = trim_copy(scheduled.event.object_id);
        to_upper_ascii(object_id);

        if (is_bms_long_note_channel(scheduled.event.channel)) {
            auto pending_it = pending_long_notes.find(*column);
            if (pending_it == pending_long_notes.end()) {
                pending_long_notes.emplace(*column, PendingLongNote{*column, time_ms});
            } else {
                if (time_ms > pending_it->second.start_time_ms) {
                    difficulty_chart.notes.push_back(
                        chart::OsuManiaNote{*column, pending_it->second.start_time_ms, time_ms, 0});
                } else {
                    difficulty_chart.notes.push_back(
                        chart::OsuManiaNote{*column, pending_it->second.start_time_ms, std::nullopt, 0});
                }
                pending_long_notes.erase(pending_it);
            }
            continue;
        }

        if (!lnobj.empty() && object_id == lnobj) {
            auto last_it = last_normal_note_by_column.find(*column);
            if (last_it != last_normal_note_by_column.end() && last_it->second < difficulty_chart.notes.size()) {
                auto& note = difficulty_chart.notes[last_it->second];
                if (!note.end_time_ms.has_value() && time_ms > note.start_time_ms) {
                    note.end_time_ms = time_ms;
                }
                last_normal_note_by_column.erase(last_it);
            }
            continue;
        }

        last_normal_note_by_column[*column] = difficulty_chart.notes.size();
        difficulty_chart.notes.push_back(chart::OsuManiaNote{*column, time_ms, std::nullopt, 0});
    }

    for (const auto& [column, pending] : pending_long_notes) {
        difficulty_chart.notes.push_back(chart::OsuManiaNote{column, pending.start_time_ms, std::nullopt, 0});
    }

    std::sort(difficulty_chart.notes.begin(), difficulty_chart.notes.end(), [](const chart::OsuManiaNote& lhs,
                                                                               const chart::OsuManiaNote& rhs) {
        if (lhs.start_time_ms != rhs.start_time_ms) {
            return lhs.start_time_ms < rhs.start_time_ms;
        }
        return lhs.column < rhs.column;
    });

    if (difficulty_chart.notes.empty()) {
        return std::nullopt;
    }

    chart::ManiaDifficultyOptions options;
    options.preset = chart::DifficultyPreset::QwilightBmsEz;
    const std::string layout = util::sanitize_ui_text(parsed_chart.layout_label);
    if (layout == "5+1 SP") {
        options.mode_name = "5+1";
    } else if (layout == "7+1 SP") {
        options.mode_name = "7+1";
    } else if (layout == "14+2 DP") {
        options.mode_name = "DP16";
    } else if (layout == "10+2 DP") {
        options.mode_name = "DP12";
    } else if (layout == "PMS 9K") {
        options.mode_name = "9K";
    }

    return chart::calculate_osu_mania_difficulty(difficulty_chart, options);
}

SongEntry build_bms_entry(std::string relative_path,
                          const std::filesystem::path& full_path,
                          int64_t mtime,
                          std::vector<std::string>& warnings) {
    SongEntry entry;
    entry.path = std::move(relative_path);
    entry.format = "bms";
    entry.mtime = mtime;

    chart::BmsParser parser;
    chart::BmsParserOptions parser_options;
    parser_options.retain_wav_bmp = false;
    parser_options.retain_unknown_headers = false;
    parser_options.retain_nonessential_commands = false;
    auto parsed = parser.parseFile(full_path.u8string(), parser_options);
    if (!parsed.success()) {
        warnings.push_back("Failed to parse BMS: " + entry.path);
        entry.title = fallback_title(full_path);
        return entry;
    }

    entry.key_count = parsed.chart.declared_key_count > 0 ? parsed.chart.declared_key_count : 10;
    entry.layout_label = util::sanitize_ui_text(parsed.chart.layout_label);

    auto title_it = parsed.chart.headers.find("TITLE");
    if (title_it != parsed.chart.headers.end()) {
        entry.title = util::sanitize_ui_text(title_it->second);
    } else {
        entry.title = fallback_title(full_path);
    }

    auto artist_it = parsed.chart.headers.find("ARTIST");
    if (artist_it != parsed.chart.headers.end()) {
        entry.artist = util::sanitize_ui_text(artist_it->second);
    }

    auto level_it = parsed.chart.headers.find("PLAYLEVEL");
    if (level_it != parsed.chart.headers.end()) {
        try {
            entry.level = std::stoi(level_it->second);
        } catch (...) {
            entry.level = 0;
        }
    }

    entry.bpm = parsed.chart.base_bpm;
    const int metadata_level = entry.level;
    auto difficulty = calculate_bms_difficulty(parsed.chart);
    if (difficulty.has_value() && difficulty->note_count > 0) {
        entry.level = difficulty->revive_level;
        entry.rating = difficulty->circus_rating;
    } else {
        entry.level = metadata_level;
    }
    return entry;
}

SongEntry build_osu_entry(std::string relative_path,
                          const std::filesystem::path& full_path,
                          int64_t mtime,
                          std::vector<std::string>& warnings) {
    SongEntry entry;
    entry.path = std::move(relative_path);
    entry.format = "osu";
    entry.mtime = mtime;

    std::string read_error;
    const std::string content = read_text_file(full_path, &read_error);
    if (content.empty() && !read_error.empty()) {
        warnings.push_back("Failed to read osu!mania chart: " + entry.path);
        entry.title = fallback_title(full_path);
        return entry;
    }

    const OsuMenuProbe probe = probe_osu_menu_candidate(content);
    if (probe.mania_mode.has_value() && !probe.mania_mode.value()) {
        return SongEntry{};
    }
    if (probe.key_count.has_value() && (probe.key_count.value() < 4 || probe.key_count.value() > 10)) {
        return SongEntry{};
    }

    chart::OsuManiaLoader loader;
    auto parsed = loader.parse(content);
    if (!parsed.success()) {
        warnings.push_back("Failed to parse osu!mania chart: " + entry.path);
        entry.title = fallback_title(full_path);
        return entry;
    }
    if (parsed.chart.key_count < 4 || parsed.chart.key_count > 10) {
        return SongEntry{};
    }

    entry.key_count = parsed.chart.key_count;
    entry.title = parsed.chart.title.empty() ? fallback_title(full_path) : util::sanitize_ui_text(parsed.chart.title);
    entry.artist = util::sanitize_ui_text(parsed.chart.artist);
    entry.bpm = parsed.chart.base_bpm;

    const auto difficulty = chart::calculate_osu_mania_difficulty(parsed.chart);
    if (difficulty.note_count > 0) {
        entry.level = difficulty.revive_level;
        entry.rating = difficulty.circus_rating;
    }
    return entry;
}

const config::JsonObject* get_object(const config::JsonObject& root, std::string_view key) {
    auto it = root.find(std::string(key));
    if (it == root.end()) {
        return nullptr;
    }
    return it->second.as_object();
}

const config::JsonValue* get_value(const config::JsonObject& root, std::string_view key) {
    auto it = root.find(std::string(key));
    if (it == root.end()) {
        return nullptr;
    }
    return &it->second;
}

std::string get_string(const config::JsonObject& root, std::string_view key, std::string fallback) {
    auto value = get_value(root, key);
    if (!value) {
        return fallback;
    }
    return value->as_string(std::move(fallback));
}

int get_int(const config::JsonObject& root, std::string_view key, int fallback) {
    auto value = get_value(root, key);
    if (!value) {
        return fallback;
    }
    return static_cast<int>(value->as_number(fallback));
}

int64_t get_int64(const config::JsonObject& root, std::string_view key, int64_t fallback) {
    auto value = get_value(root, key);
    if (!value) {
        return fallback;
    }
    return static_cast<int64_t>(value->as_number(static_cast<double>(fallback)));
}

SongIndexMemorySnapshot query_song_index_memory_snapshot() {
    SongIndexMemorySnapshot snapshot;
#ifdef _WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status) != 0) {
        snapshot.available_bytes =
            (std::min)(static_cast<std::uint64_t>(status.ullAvailPhys), static_cast<std::uint64_t>(status.ullAvailPageFile));
        snapshot.total_bytes = static_cast<std::uint64_t>(status.ullTotalPhys);
    }
#endif
    return snapshot;
}

void trim_song_index_process_memory() {
#ifdef _WIN32
    _heapmin();
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
#endif
}

bool should_trim_song_index_process_memory(SongIndexProfile profile, int processed, int total) {
    if (processed <= 0) {
        return false;
    }
    const int trim_interval = (profile == SongIndexProfile::Fast) ? 2048 : 512;
    if ((processed % trim_interval) == 0) {
        return true;
    }
    return total > 0 && processed >= total;
}

void publish_progress(const SongIndexProgressCallback& progress,
                      SongIndexProgressStage stage,
                      int processed,
                      int total) {
    if (!progress) {
        return;
    }
    progress(SongIndexProgress{stage, processed, total});
}

int clamp_progress_count(std::uint64_t value) {
    return static_cast<int>((std::min)(value, static_cast<std::uint64_t>((std::numeric_limits<int>::max)())));
}

struct SongIndexCandidate {
    std::filesystem::path full_path;
    std::string relative_path;
    int64_t mtime = 0;
};

bool enumerate_song_candidates(const std::filesystem::path& root_dir,
                               bool include_osu,
                               std::vector<std::string>* warnings,
                               const std::function<void(SongIndexCandidate&&)>& on_candidate,
                               std::uint64_t* out_candidate_count = nullptr,
                               const SongIndexProgressCallback& progress = {}) {
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::directory_options scan_options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(root_dir, scan_options, ec);
    fs::recursive_directory_iterator end;

    std::uint64_t discovered = 0;
    if (progress) {
        publish_progress(progress, SongIndexProgressStage::ScanningFiles, 0, -1);
    }

    while (it != end) {
        if (ec) {
            if (warnings) {
                warnings->push_back("Song scan skipped entry: " + ec.message());
            }
            ec.clear();
            it.increment(ec);
            continue;
        }

        const fs::directory_entry& entry = *it;
        if (entry.is_directory(ec)) {
            if (!ec && entry.path().filename() == ".tenriff") {
                it.disable_recursion_pending();
            }
            ec.clear();
            it.increment(ec);
            continue;
        }
        if (ec) {
            if (warnings) {
                warnings->push_back("Song scan skipped entry: " + ec.message());
            }
            ec.clear();
            it.increment(ec);
            continue;
        }

        if (!entry.is_regular_file(ec)) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        ec.clear();

        const std::string ext = to_lower(entry.path().extension().u8string());
        if (!is_bms_extension(ext) && !(include_osu && is_osu_extension(ext))) {
            it.increment(ec);
            continue;
        }

        SongIndexCandidate candidate;
        candidate.full_path = entry.path();
        candidate.relative_path = relative_path_string(root_dir, candidate.full_path);
        auto mtime = entry.last_write_time(ec);
        if (!ec) {
            candidate.mtime = file_time_seconds(mtime);
        } else {
            ec.clear();
        }

        ++discovered;
        if (progress && ((discovered % 256u) == 0u)) {
            publish_progress(progress, SongIndexProgressStage::ScanningFiles, clamp_progress_count(discovered), -1);
        }
        on_candidate(std::move(candidate));
        it.increment(ec);
    }

    if (out_candidate_count) {
        *out_candidate_count = discovered;
    }
    return true;
}

void process_song_index_batch(std::vector<SongIndexCandidate>& batch,
                              const std::unordered_map<std::string_view, const SongEntry*>& cached,
                              std::vector<std::string>& warnings,
                              SongIndex& result,
                              const SongIndexOptions& options,
                              std::atomic<int>& processed,
                              int total_hint,
                              unsigned reported_threads,
                              const SongIndexProgressCallback& progress) {
    if (batch.empty()) {
        return;
    }

    const auto budget = choose_song_index_work_budget(
        options.profile,
        reported_threads,
        batch.size(),
        query_song_index_memory_snapshot());
    const std::size_t batch_size = batch.size();
    const std::size_t worker_count = std::min<std::size_t>(budget.worker_count, batch_size);

    std::vector<SongEntry> batch_entries(batch_size);
    std::vector<uint8_t> batch_valid(batch_size, 0);
    std::vector<std::vector<std::string>> worker_warnings(worker_count);
    std::atomic<std::size_t> next_index{0};

    auto worker = [&](std::size_t worker_slot) {
        auto& local_warnings = worker_warnings[worker_slot];
        std::vector<std::string> item_warnings;
        for (;;) {
            const std::size_t batch_index = next_index.fetch_add(1, std::memory_order_relaxed);
            if (batch_index >= batch_size) {
                break;
            }

            const auto& candidate = batch[batch_index];
            SongEntry entry;
            auto cached_it = cached.find(candidate.relative_path);
            if (cached_it != cached.end() && cached_it->second->mtime == candidate.mtime &&
                !needs_difficulty_refresh(*cached_it->second)) {
                entry = *cached_it->second;
            } else {
                item_warnings.clear();
                const std::string ext = to_lower(candidate.full_path.extension().u8string());
                if (is_osu_extension(ext)) {
                    entry = build_osu_entry(candidate.relative_path, candidate.full_path, candidate.mtime, item_warnings);
                } else {
                    entry = build_bms_entry(candidate.relative_path, candidate.full_path, candidate.mtime, item_warnings);
                }
                if (!item_warnings.empty()) {
                    local_warnings.insert(local_warnings.end(), item_warnings.begin(), item_warnings.end());
                }
            }

            batch_valid[batch_index] =
                (!entry.path.empty() && is_menu_song_entry(entry, options)) ? static_cast<uint8_t>(1u)
                                                                            : static_cast<uint8_t>(0u);
            batch_entries[batch_index] = std::move(entry);

            const int current = processed.fetch_add(1, std::memory_order_relaxed) + 1;
            if (current == total_hint || (current % 32) == 0) {
                publish_progress(progress, SongIndexProgressStage::BuildingMetadata, current, total_hint);
            }
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(worker_count > 0 ? worker_count - 1 : 0);
    for (std::size_t i = 1; i < worker_count; ++i) {
        workers.emplace_back(worker, i);
    }
    worker(0);
    for (auto& th : workers) {
        if (th.joinable()) {
            th.join();
        }
    }

    for (const auto& local_warnings : worker_warnings) {
        if (!local_warnings.empty()) {
            warnings.insert(warnings.end(), local_warnings.begin(), local_warnings.end());
        }
    }

    std::size_t valid_count = 0;
    for (uint8_t valid : batch_valid) {
        valid_count += (valid != 0u) ? 1u : 0u;
    }
    if (valid_count > 0) {
        result.entries.reserve(result.entries.size() + valid_count);
        for (std::size_t i = 0; i < batch_size; ++i) {
            if (batch_valid[i] != 0u) {
                result.entries.push_back(std::move(batch_entries[i]));
            }
        }
    }

    batch.clear();
}

void write_json_string(std::ostream& out, std::string_view value) {
    out << config::json_stringify(config::JsonValue{std::string(value)});
}

class SongIndexStreamReader {
public:
    explicit SongIndexStreamReader(std::istream& stream) : stream_(stream) {}

    [[nodiscard]] const std::string& error() const { return error_; }
    [[nodiscard]] int version() const { return version_; }
    [[nodiscard]] bool cache_includes_osu() const { return cache_includes_osu_; }
    [[nodiscard]] const std::vector<SongEntry>& entries() const { return entries_; }

    bool parse(const SongIndexOptions& options) {
        if (!expect('{', "Song index root must be an object.")) {
            return false;
        }

        skip_ws();
        if (consume_if('}')) {
            return true;
        }

        while (good()) {
            auto key = parse_string();
            if (!key.has_value()) {
                return false;
            }
            if (!expect(':', "Expected ':' after song index field name.")) {
                return false;
            }

            if (*key == "version") {
                auto number = parse_number();
                if (!number.has_value()) {
                    return false;
                }
                version_ = static_cast<int>(std::llround(number.value()));
                if (version_ != kSongIndexVersion) {
                    return true;
                }
            } else if (*key == "include_osu") {
                auto value = parse_bool();
                if (!value.has_value()) {
                    return false;
                }
                cache_includes_osu_ = value.value();
            } else if (*key == "entries") {
                if (version_ != kSongIndexVersion) {
                    if (!skip_value()) {
                        return false;
                    }
                } else if (options.include_osu && !cache_includes_osu_) {
                    if (!skip_value()) {
                        return false;
                    }
                } else if (!parse_entries_array(options)) {
                    return false;
                }
            } else {
                if (!skip_value()) {
                    return false;
                }
            }

            skip_ws();
            if (consume_if(',')) {
                continue;
            }
            if (consume_if('}')) {
                return true;
            }
            set_error("Expected ',' or '}' after song index field.");
            return false;
        }

        set_error("Unexpected end of input while parsing song index.");
        return false;
    }

private:
    std::istream& stream_;
    std::string error_;
    int version_ = 0;
    bool cache_includes_osu_ = false;
    std::vector<SongEntry> entries_;

    [[nodiscard]] bool good() const {
        return static_cast<bool>(stream_);
    }

    void set_error(std::string message) {
        if (error_.empty()) {
            error_ = std::move(message);
        }
    }

    void skip_ws() {
        while (good()) {
            const int ch = stream_.peek();
            if (ch == EOF || std::isspace(static_cast<unsigned char>(ch)) == 0) {
                return;
            }
            stream_.get();
        }
    }

    bool consume_if(char expected) {
        skip_ws();
        if (stream_.peek() != expected) {
            return false;
        }
        stream_.get();
        return true;
    }

    bool expect(char expected, std::string message) {
        if (!consume_if(expected)) {
            set_error(std::move(message));
            return false;
        }
        return true;
    }

    bool consume_literal(std::string_view literal, std::string message) {
        skip_ws();
        for (char ch : literal) {
            if (stream_.peek() != ch) {
                set_error(std::move(message));
                return false;
            }
            stream_.get();
        }
        return true;
    }

    std::optional<bool> parse_bool() {
        skip_ws();
        const int ch = stream_.peek();
        if (ch == 't') {
            if (!consume_literal("true", "Expected 'true' while parsing boolean value.")) {
                return std::nullopt;
            }
            return true;
        }
        if (ch == 'f') {
            if (!consume_literal("false", "Expected 'false' while parsing boolean value.")) {
                return std::nullopt;
            }
            return false;
        }
        set_error("Expected boolean value.");
        return std::nullopt;
    }

    bool parse_null() {
        return consume_literal("null", "Expected 'null' while parsing JSON value.");
    }

    std::optional<double> parse_number() {
        skip_ws();
        std::string token;
        while (good()) {
            const int ch = stream_.peek();
            if (ch == EOF) {
                break;
            }
            const char value = static_cast<char>(ch);
            if (!(std::isdigit(static_cast<unsigned char>(value)) != 0 || value == '-' || value == '+' ||
                  value == '.' || value == 'e' || value == 'E')) {
                break;
            }
            token.push_back(value);
            stream_.get();
        }
        if (token.empty()) {
            set_error("Expected numeric value.");
            return std::nullopt;
        }
        try {
            return std::stod(token);
        } catch (...) {
            set_error("Invalid numeric value in song index.");
            return std::nullopt;
        }
    }

    std::optional<std::string> parse_string() {
        skip_ws();
        if (stream_.peek() != '"') {
            set_error("Expected JSON string.");
            return std::nullopt;
        }
        stream_.get();

        std::string out;
        while (good()) {
            const int raw = stream_.get();
            if (raw == EOF) {
                break;
            }
            const char ch = static_cast<char>(raw);
            if (ch == '"') {
                return out;
            }
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }

            const int escaped = stream_.get();
            if (escaped == EOF) {
                break;
            }
            switch (static_cast<char>(escaped)) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned int code = 0;
                    for (int i = 0; i < 4; ++i) {
                        const int hex_raw = stream_.get();
                        if (hex_raw == EOF) {
                            set_error("Invalid unicode escape in JSON string.");
                            return std::nullopt;
                        }
                        const char hex = static_cast<char>(hex_raw);
                        code <<= 4;
                        if (hex >= '0' && hex <= '9') {
                            code += static_cast<unsigned int>(hex - '0');
                        } else if (hex >= 'a' && hex <= 'f') {
                            code += static_cast<unsigned int>(hex - 'a' + 10);
                        } else if (hex >= 'A' && hex <= 'F') {
                            code += static_cast<unsigned int>(hex - 'A' + 10);
                        } else {
                            set_error("Invalid hex digit in unicode escape.");
                            return std::nullopt;
                        }
                    }
                    if (code <= 0x7F) {
                        out.push_back(static_cast<char>(code));
                    } else {
                        out.push_back('?');
                    }
                    break;
                }
                default:
                    set_error("Unsupported escape sequence in JSON string.");
                    return std::nullopt;
            }
        }

        set_error("Unterminated JSON string.");
        return std::nullopt;
    }

    bool skip_array() {
        if (!expect('[', "Expected '[' to start JSON array.")) {
            return false;
        }
        skip_ws();
        if (consume_if(']')) {
            return true;
        }
        while (good()) {
            if (!skip_value()) {
                return false;
            }
            skip_ws();
            if (consume_if(',')) {
                continue;
            }
            if (consume_if(']')) {
                return true;
            }
            set_error("Expected ',' or ']' while skipping JSON array.");
            return false;
        }
        set_error("Unexpected end of input while skipping JSON array.");
        return false;
    }

    bool skip_object() {
        if (!expect('{', "Expected '{' to start JSON object.")) {
            return false;
        }
        skip_ws();
        if (consume_if('}')) {
            return true;
        }
        while (good()) {
            if (!parse_string().has_value()) {
                return false;
            }
            if (!expect(':', "Expected ':' after JSON object key.")) {
                return false;
            }
            if (!skip_value()) {
                return false;
            }
            skip_ws();
            if (consume_if(',')) {
                continue;
            }
            if (consume_if('}')) {
                return true;
            }
            set_error("Expected ',' or '}' while skipping JSON object.");
            return false;
        }
        set_error("Unexpected end of input while skipping JSON object.");
        return false;
    }

    bool skip_value() {
        skip_ws();
        const int ch = stream_.peek();
        if (ch == EOF) {
            set_error("Unexpected end of input while skipping JSON value.");
            return false;
        }
        switch (static_cast<char>(ch)) {
            case '{': return skip_object();
            case '[': return skip_array();
            case '"': return parse_string().has_value();
            case 't':
            case 'f': return parse_bool().has_value();
            case 'n': return parse_null();
            default: return parse_number().has_value();
        }
    }

    bool parse_entries_array(const SongIndexOptions& options) {
        if (!expect('[', "Song index entries must be an array.")) {
            return false;
        }
        skip_ws();
        if (consume_if(']')) {
            return true;
        }

        while (good()) {
            SongEntry entry;
            if (!parse_entry_object(entry)) {
                return false;
            }
            if (!entry.path.empty() && is_menu_song_entry(entry, options)) {
                entries_.push_back(std::move(entry));
            }

            skip_ws();
            if (consume_if(',')) {
                continue;
            }
            if (consume_if(']')) {
                return true;
            }
            set_error("Expected ',' or ']' after song index entry.");
            return false;
        }

        set_error("Unexpected end of input while parsing song index entries.");
        return false;
    }

    bool parse_entry_object(SongEntry& entry) {
        if (!expect('{', "Song index entry must be an object.")) {
            return false;
        }
        skip_ws();
        if (consume_if('}')) {
            return true;
        }

        while (good()) {
            auto key = parse_string();
            if (!key.has_value()) {
                return false;
            }
            if (!expect(':', "Expected ':' after song entry field name.")) {
                return false;
            }

            if (*key == "path") {
                auto value = parse_string();
                if (!value.has_value()) {
                    return false;
                }
                entry.path = util::ensure_utf8_text(value.value());
            } else if (*key == "title") {
                auto value = parse_string();
                if (!value.has_value()) {
                    return false;
                }
                entry.title = util::sanitize_ui_text(value.value());
            } else if (*key == "artist") {
                auto value = parse_string();
                if (!value.has_value()) {
                    return false;
                }
                entry.artist = util::sanitize_ui_text(value.value());
            } else if (*key == "format") {
                auto value = parse_string();
                if (!value.has_value()) {
                    return false;
                }
                entry.format = util::sanitize_ui_text(value.value());
            } else if (*key == "layout_label") {
                auto value = parse_string();
                if (!value.has_value()) {
                    return false;
                }
                entry.layout_label = util::sanitize_ui_text(value.value());
            } else if (*key == "key_count") {
                auto value = parse_number();
                if (!value.has_value()) {
                    return false;
                }
                entry.key_count = static_cast<int>(std::llround(value.value()));
            } else if (*key == "level") {
                auto value = parse_number();
                if (!value.has_value()) {
                    return false;
                }
                entry.level = static_cast<int>(std::llround(value.value()));
            } else if (*key == "rating") {
                auto value = parse_number();
                if (!value.has_value()) {
                    return false;
                }
                entry.rating = value.value();
            } else if (*key == "bpm") {
                auto value = parse_number();
                if (!value.has_value()) {
                    return false;
                }
                entry.bpm = value.value();
            } else if (*key == "mtime") {
                auto value = parse_number();
                if (!value.has_value()) {
                    return false;
                }
                entry.mtime = static_cast<int64_t>(std::llround(value.value()));
            } else {
                if (!skip_value()) {
                    return false;
                }
            }

            skip_ws();
            if (consume_if(',')) {
                continue;
            }
            if (consume_if('}')) {
                return true;
            }
            set_error("Expected ',' or '}' after song entry field.");
            return false;
        }

        set_error("Unexpected end of input while parsing song entry object.");
        return false;
    }
};

}  // namespace

std::string song_index_cache_path_for_source(std::string_view profile_root, std::string_view source_root) {
    const std::filesystem::path profile_path = path_from_utf8(profile_root);
    if (profile_path.empty()) {
        return {};
    }

    std::string cache_key = normalize_cache_key_path(source_root);
    if (cache_key.empty()) {
        cache_key = std::string(source_root);
    }
    if (cache_key.empty()) {
        cache_key = "default-song-source";
    }

    const std::filesystem::path cache_path =
        profile_path / ".tenriff" / "song-index" / (hex_u64(fnv1a_64(cache_key)) + ".json");
    return cache_path.u8string();
}

std::string legacy_song_index_cache_path_for_source(std::string_view source_root) {
    const std::filesystem::path source_path = path_from_utf8(source_root);
    if (source_path.empty()) {
        return {};
    }
    return (source_path / ".tenriff" / "song_index.json").u8string();
}

SongIndexLoadResult load_song_index(const std::string& path, const SongIndexOptions& options) {
    SongIndexLoadResult result;
    std::ifstream file(path_from_utf8(path), std::ios::binary);
    if (!file) {
        result.warnings.push_back("Song index not found; scanning songs.");
        return result;
    }

    SongIndexStreamReader reader(file);
    if (!reader.parse(options)) {
        result.error = reader.error().empty() ? "Failed to parse song index." : reader.error();
        return result;
    }

    if (reader.version() != kSongIndexVersion) {
        return result;
    }
    if (options.include_osu && !reader.cache_includes_osu()) {
        return result;
    }

    result.loaded_from_file = true;
    result.index.entries = reader.entries();
    return result;
}

bool save_song_index(const std::string& path,
                     const SongIndex& index,
                     const SongIndexOptions& options,
                     std::string* error,
                     SongIndexProgressCallback progress) {
    const std::filesystem::path file_path = path_from_utf8(path);
    if (file_path.empty()) {
        if (error) {
            *error = "Failed to resolve song index path.";
        }
        return false;
    }

    std::error_code ec;
    const std::filesystem::path parent_path = file_path.parent_path();
    if (!parent_path.empty()) {
        std::filesystem::create_directories(parent_path, ec);
        if (ec) {
            if (error) {
                *error = "Failed to create song index cache directory: " + ec.message();
            }
            return false;
        }
    }

    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        if (error) {
            *error = "Failed to write song index.";
        }
        return false;
    }

    const int total = static_cast<int>(index.entries.size());
    publish_progress(progress, SongIndexProgressStage::SavingCache, 0, total);

    file << "{\n";
    file << "  \"version\": " << kSongIndexVersion << ",\n";
    file << "  \"include_osu\": " << (options.include_osu ? "true" : "false") << ",\n";
    file << "  \"entries\": [\n";

    for (std::size_t i = 0; i < index.entries.size(); ++i) {
        const auto& entry = index.entries[i];
        if (i != 0) {
            file << ",\n";
        }
        file << "    {";
        file << "\"path\":";
        write_json_string(file, entry.path);
        file << ",\"title\":";
        write_json_string(file, entry.title);
        file << ",\"artist\":";
        write_json_string(file, entry.artist);
        file << ",\"format\":";
        write_json_string(file, entry.format);
        file << ",\"layout_label\":";
        write_json_string(file, entry.layout_label);
        file << ",\"key_count\":" << entry.key_count;
        file << ",\"level\":" << entry.level;
        file << ",\"rating\":" << entry.rating;
        file << ",\"bpm\":" << entry.bpm;
        file << ",\"mtime\":" << entry.mtime;
        file << "}";

        const int processed = static_cast<int>(i + 1);
        if (processed == total || (processed % 64) == 0) {
            publish_progress(progress, SongIndexProgressStage::SavingCache, processed, total);
        }
    }

    file << "\n  ]\n";
    file << "}\n";
    file.flush();
    if (!file.good()) {
        if (error) {
            *error = "Failed while streaming song index.";
        }
        return false;
    }

    publish_progress(progress, SongIndexProgressStage::SavingCache, total, total);
    return true;
}

SongIndex scan_songs(const std::string& root_path,
                     const SongIndex* cache,
                     std::vector<std::string>& warnings,
                     SongIndexProgressCallback progress,
                     const SongIndexOptions& options) {
    SongIndex result;
    namespace fs = std::filesystem;

    std::error_code ec;
    const fs::path root_dir = path_from_utf8(root_path);
    if (!fs::exists(root_dir, ec) || !fs::is_directory(root_dir, ec)) {
        warnings.push_back("Songs path not found: " + root_path);
        return result;
    }

    std::unordered_map<std::string_view, const SongEntry*> cached;
    if (cache) {
        cached.reserve(cache->entries.size());
        for (const auto& entry : cache->entries) {
            cached.emplace(std::string_view(entry.path), &entry);
        }
    }

    std::uint64_t total_candidates_u64 = 0;
    enumerate_song_candidates(root_dir, options.include_osu, nullptr, [](SongIndexCandidate&&) {}, &total_candidates_u64, progress);
    const int total = clamp_progress_count(total_candidates_u64);
    publish_progress(progress, SongIndexProgressStage::BuildingMetadata, 0, total);

    result.entries.clear();
    if (total_candidates_u64 > 0) {
        const unsigned hc = std::thread::hardware_concurrency();
        std::atomic<int> processed{0};
        const std::size_t total_candidates =
            static_cast<std::size_t>((std::min)(total_candidates_u64,
                                                static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())));
        const auto initial_budget = choose_song_index_work_budget(
            options.profile,
            hc,
            (total_candidates > 0) ? total_candidates : 1u,
            query_song_index_memory_snapshot());
        std::size_t batch_limit = std::clamp<std::size_t>(initial_budget.batch_size, initial_budget.worker_count, 64u);
        std::vector<SongIndexCandidate> batch;
        batch.reserve(batch_limit);

        enumerate_song_candidates(
            root_dir,
            options.include_osu,
            &warnings,
            [&](SongIndexCandidate&& candidate) {
                batch.push_back(std::move(candidate));
                if (batch.size() >= batch_limit) {
                    process_song_index_batch(batch, cached, warnings, result, options, processed, total, hc, progress);
                    const int processed_now = processed.load(std::memory_order_acquire);
                    if (should_trim_song_index_process_memory(options.profile, processed_now, total)) {
                        trim_song_index_process_memory();
                    }
                    const std::uint64_t remaining_u64 =
                        (total_candidates_u64 > static_cast<std::uint64_t>(processed_now))
                            ? (total_candidates_u64 - static_cast<std::uint64_t>(processed_now))
                            : 1u;
                    const auto next_budget = choose_song_index_work_budget(
                        options.profile,
                        hc,
                        static_cast<std::size_t>((std::min)(
                            remaining_u64, static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))),
                        query_song_index_memory_snapshot());
                    batch_limit = std::clamp<std::size_t>(next_budget.batch_size, next_budget.worker_count, 64u);
                    if (batch.capacity() < batch_limit) {
                        batch.reserve(batch_limit);
                    }
                }
            });

        process_song_index_batch(batch, cached, warnings, result, options, processed, total, hc, progress);
        const int processed_now = processed.load(std::memory_order_acquire);
        if (should_trim_song_index_process_memory(options.profile, processed_now, total)) {
            trim_song_index_process_memory();
        }
    }

    std::sort(result.entries.begin(), result.entries.end(), [](const SongEntry& lhs, const SongEntry& rhs) {
        const auto lhs_title = to_lower(lhs.title);
        const auto rhs_title = to_lower(rhs.title);
        if (lhs_title == rhs_title) {
            return lhs.path < rhs.path;
        }
        return lhs_title < rhs_title;
    });

    publish_progress(progress, SongIndexProgressStage::BuildingMetadata, total, total);

    return result;
}

}  // namespace tenriff::app
