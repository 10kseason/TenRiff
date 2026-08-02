#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "app/SongIndex.h"
#include "config/SimpleJson.h"

namespace {

namespace fs = std::filesystem;
using tenriff::app::SongEntry;
using tenriff::app::SongIndex;
using tenriff::app::SongIndexOptions;
using tenriff::app::scan_songs;
using tenriff::config::JsonArray;
using tenriff::config::JsonObject;
using tenriff::config::JsonParseResult;
using tenriff::config::JsonValue;
using tenriff::config::json_stringify;
using tenriff::config::parse_json;

constexpr std::size_t kDefaultCount = 10;

struct Options {
    std::optional<fs::path> songs_root;
    std::size_t count = kDefaultCount;
    std::optional<std::uint64_t> seed;
};

struct TempDirGuard {
    fs::path path;

    ~TempDirGuard() {
        if (!path.empty()) {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }
};

struct PythonResult {
    std::string relative_path;
    bool ok = false;
    std::string error;
    double rating = 0.0;
    int revive_level = 0;
    int key_count = 0;
    std::string mode_name;
};

struct CompareRow {
    SongEntry tenriff;
    PythonResult python;
    double rating_delta = 0.0;
    bool level_match = false;
};

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - ('A' - 'a'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

bool is_bms_path(std::string_view path) {
    const std::string lower = to_lower_ascii(std::string(path));
    const auto has_suffix = [&lower](std::string_view suffix) {
        return lower.size() >= suffix.size() &&
               lower.compare(lower.size() - suffix.size(), suffix.size(), suffix.data(), suffix.size()) == 0;
    };
    return has_suffix(".bms") || has_suffix(".bme") || has_suffix(".bml") || has_suffix(".pms");
}

bool parse_u64(std::string_view text, std::uint64_t& value) {
    if (text.empty()) {
        return false;
    }
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc() && result.ptr == last;
}

bool parse_size(std::string_view text, std::size_t& value) {
    std::uint64_t raw = 0;
    if (!parse_u64(text, raw) || raw == 0) {
        return false;
    }
    value = static_cast<std::size_t>(raw);
    return static_cast<std::uint64_t>(value) == raw;
}

void print_usage(const char* argv0) {
    std::cout << "Usage: " << (argv0 ? argv0 : "bms_10k_compare_smoke")
              << " [--songs-root <path>] [--count <n>] [--seed <u64>]\n";
}

bool parse_options(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        }
        if (arg == "--songs-root") {
            if (i + 1 >= argc) {
                std::cerr << "[error] --songs-root requires a path.\n";
                return false;
            }
            options.songs_root = fs::u8path(argv[++i]);
            continue;
        }
        if (arg == "--count") {
            if (i + 1 >= argc || !parse_size(argv[i + 1], options.count)) {
                std::cerr << "[error] --count requires a positive integer.\n";
                return false;
            }
            ++i;
            continue;
        }
        if (arg == "--seed") {
            if (i + 1 >= argc) {
                std::cerr << "[error] --seed requires an unsigned integer.\n";
                return false;
            }
            std::uint64_t parsed = 0;
            if (!parse_u64(argv[i + 1], parsed)) {
                std::cerr << "[error] --seed requires an unsigned integer.\n";
                return false;
            }
            options.seed = parsed;
            ++i;
            continue;
        }
        std::cerr << "[error] Unknown argument: " << arg << '\n';
        print_usage(argv[0]);
        return false;
    }
    return true;
}

std::uint64_t default_seed() {
    return static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

fs::path executable_directory(const char* argv0) {
    std::error_code ec;
    fs::path executable_path = fs::absolute(fs::u8path(argv0 ? argv0 : ""), ec);
    if (ec || executable_path.empty()) {
        return fs::current_path();
    }
    if (executable_path.has_filename()) {
        executable_path = executable_path.parent_path();
    }
    return executable_path.lexically_normal();
}

std::optional<fs::path> resolve_repo_root() {
    fs::path current = fs::current_path();
    for (;;) {
        if (fs::exists(current / "10k-calc" / "new_calc.py") && fs::exists(current / "src" / "app" / "SongIndex.cpp")) {
            return current;
        }
        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return std::nullopt;
}

std::string compact_diagnostics(std::string text) {
    for (char& ch : text) {
        if (ch == '\r' || ch == '\n' || ch == '\t') {
            ch = ' ';
        }
    }
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    if (text.size() > 220) {
        text.resize(217);
        text += "...";
    }
    return text;
}

std::optional<fs::path> resolve_songs_root(const Options& options, const char* argv0) {
    auto is_valid_dir = [](const fs::path& path) {
        std::error_code ec;
        return !path.empty() && fs::exists(path, ec) && fs::is_directory(path, ec) && !ec;
    };

    if (options.songs_root.has_value()) {
        fs::path explicit_root = options.songs_root->lexically_normal();
        if (is_valid_dir(explicit_root)) {
            return explicit_root;
        }
        std::cerr << "[error] songs root not found: " << explicit_root.u8string() << '\n';
        return std::nullopt;
    }

    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    const fs::path exe_dir = executable_directory(argv0);
    const std::array<fs::path, 6> candidates = {
        (cwd / "build" / "Release" / "Songs").lexically_normal(),
        (exe_dir / "Songs").lexically_normal(),
        (cwd / "Baepoks" / "TenRiff-0.7.9" / "songs").lexically_normal(),
        (cwd / "Baepoks" / "TenRiff-0.7.8" / "songs").lexically_normal(),
        (cwd / "Baepoks" / "TenRiff-0.7.7" / "songs").lexically_normal(),
        (cwd / "Baepoks" / "TenRiff-0.7.6" / "songs").lexically_normal(),
    };

    for (const auto& candidate : candidates) {
        if (is_valid_dir(candidate)) {
            return candidate;
        }
    }

    std::cerr << "[error] failed to locate songs root automatically.\n";
    for (const auto& candidate : candidates) {
        std::cerr << "  tried: " << candidate.u8string() << '\n';
    }
    return std::nullopt;
}

std::vector<SongEntry> select_random_entries(const SongIndex& index, std::size_t count, std::uint64_t seed) {
    std::vector<SongEntry> entries;
    entries.reserve(index.entries.size());
    for (const auto& entry : index.entries) {
        if (entry.format == "bms" && is_bms_path(entry.path) && entry.rating > 0.0) {
            entries.push_back(entry);
        }
    }

    std::mt19937_64 rng(seed);
    std::shuffle(entries.begin(), entries.end(), rng);
    if (entries.size() > count) {
        entries.resize(count);
    }
    std::sort(entries.begin(), entries.end(), [](const SongEntry& lhs, const SongEntry& rhs) {
        return lhs.path < rhs.path;
    });
    return entries;
}

fs::path make_temp_dir() {
    const fs::path base = fs::temp_directory_path() / "tenriff_bms_10k_compare";
    fs::create_directories(base);
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const fs::path candidate = base / ("case_" + std::to_string(attempt));
        std::error_code ec;
        if (fs::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

JsonObject case_to_json(const fs::path& songs_root, const SongEntry& entry) {
    JsonObject object;
    object.emplace("relative_path", JsonValue(entry.path));
    object.emplace("full_path", JsonValue((songs_root / fs::u8path(entry.path)).lexically_normal().u8string()));
    return object;
}

std::string read_pipe(FILE* pipe) {
    std::string output;
    std::array<char, 4096> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output.append(buffer.data());
    }
    return output;
}

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

#ifdef _WIN32
std::wstring quote_arg(const std::wstring& text) {
    std::wstring quoted = L"\"";
    for (const wchar_t ch : text) {
        if (ch == L'"') {
            quoted.push_back(L'\\');
        }
        quoted.push_back(ch);
    }
    quoted.push_back(L'"');
    return quoted;
}

CommandResult run_command(const std::wstring& command) {
    CommandResult result;
    FILE* pipe = _wpopen(command.c_str(), L"r");
    if (pipe == nullptr) {
        return result;
    }
    result.output = read_pipe(pipe);
    result.exit_code = _pclose(pipe);
    return result;
}
#else
std::string quote_arg(const std::string& text) {
    std::string quoted = "\"";
    for (const char ch : text) {
        if (ch == '"') {
            quoted.push_back('\\');
        }
        quoted.push_back(ch);
    }
    quoted.push_back('"');
    return quoted;
}

CommandResult run_command(const std::string& command) {
    CommandResult result;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return result;
    }
    result.output = read_pipe(pipe);
    result.exit_code = pclose(pipe);
    return result;
}
#endif

std::optional<std::unordered_map<std::string, PythonResult>> run_python_compare(const fs::path& repo_root,
                                                                                const fs::path& songs_root,
                                                                                const std::vector<SongEntry>& entries,
                                                                                std::string& diagnostics) {
    TempDirGuard temp_dir{make_temp_dir()};
    if (temp_dir.path.empty()) {
        diagnostics = "failed to create temp directory";
        return std::nullopt;
    }

    JsonArray cases;
    cases.reserve(entries.size());
    for (const auto& entry : entries) {
        cases.emplace_back(case_to_json(songs_root, entry));
    }

    JsonObject input_root;
    input_root.emplace("cases", JsonValue(std::move(cases)));

    const fs::path input_path = temp_dir.path / "input.json";
    {
        std::ofstream output(input_path, std::ios::binary);
        output << json_stringify(JsonValue(std::move(input_root)), 2);
    }

    const fs::path script_path = temp_dir.path / "compare_realworld.py";
    {
        std::ofstream output(script_path, std::ios::binary);
        output <<
            "import contextlib\n"
            "import importlib.util\n"
            "import io\n"
            "import json\n"
            "import pathlib\n"
            "import sys\n"
            "import yaml\n"
            "\n"
            "repo_root = pathlib.Path(sys.argv[1])\n"
            "input_path = pathlib.Path(sys.argv[2])\n"
            "calc_root = repo_root / '10k-calc'\n"
            "sys.path.insert(0, str(calc_root))\n"
            "\n"
            "import bms_parser\n"
            "spec = importlib.util.spec_from_file_location('tenriff_new_calc', calc_root / 'new_calc.py')\n"
            "new_calc = importlib.util.module_from_spec(spec)\n"
            "spec.loader.exec_module(new_calc)\n"
            "\n"
            "with (calc_root / 'config.yaml').open('r', encoding='utf-8') as fh:\n"
            "    config = yaml.safe_load(fh)\n"
            "with input_path.open('r', encoding='utf-8') as fh:\n"
            "    payload = json.load(fh)\n"
            "\n"
            "results = []\n"
            "for item in payload['cases']:\n"
            "    relative_path = item['relative_path']\n"
            "    full_path = item['full_path']\n"
            "    row = {'relative_path': relative_path}\n"
            "    try:\n"
            "        parser = bms_parser.BMSParser(full_path)\n"
            "        with contextlib.redirect_stdout(io.StringIO()):\n"
            "            notes = parser.parse()\n"
            "            duration = getattr(parser, 'duration', 0.0) or 0.0\n"
            "            duration = duration if duration > 0 else 1.0\n"
            "            key_count = getattr(parser, 'key_count', None) or 7\n"
            "            mode_name = getattr(parser, 'detected_mode', None)\n"
            "            total_diff = new_calc.calculate_total_difficulty(\n"
            "                notes,\n"
            "                duration,\n"
            "                key_mode=key_count,\n"
            "                preset_name='qwilight_bms_ez',\n"
            "                mode_name=mode_name,\n"
            "                config=config,\n"
            "            )\n"
            "        row['ok'] = True\n"
            "        row['rating'] = float(total_diff.get('circus_rating', 0.0))\n"
            "        row['revive_level'] = int(total_diff.get('revive_lv', 0))\n"
            "        row['key_count'] = int(key_count)\n"
            "        row['mode_name'] = '' if mode_name is None else str(mode_name)\n"
            "    except Exception as exc:\n"
            "        row['ok'] = False\n"
            "        row['error'] = str(exc)\n"
            "    results.append(row)\n"
            "\n"
            "print(json.dumps({'results': results}, ensure_ascii=False))\n";
    }

#ifdef _WIN32
    const std::vector<std::wstring> interpreters = {L"python", L"py -3"};
    for (const auto& interpreter : interpreters) {
        const std::wstring command = interpreter + L" " + quote_arg(script_path.wstring()) + L" " +
                                     quote_arg(repo_root.wstring()) + L" " + quote_arg(input_path.wstring()) + L" 2>&1";
        const auto result = run_command(command);
        if (result.exit_code != 0) {
            diagnostics = result.output;
            continue;
        }

        JsonParseResult parsed = parse_json(result.output);
        if (!parsed.success()) {
            diagnostics = "failed to parse python output: " + parsed.error + "\nraw:\n" + result.output;
            continue;
        }

        const auto* root = parsed.root->as_object();
        if (root == nullptr) {
            diagnostics = "python output root was not an object";
            continue;
        }
        const auto results_it = root->find("results");
        if (results_it == root->end()) {
            diagnostics = "python output missing results";
            continue;
        }
        const auto* results = results_it->second.as_array();
        if (results == nullptr) {
            diagnostics = "python results was not an array";
            continue;
        }

        std::unordered_map<std::string, PythonResult> by_path;
        for (const auto& row_value : *results) {
            const auto* row = row_value.as_object();
            if (row == nullptr) {
                diagnostics = "python result row was not an object";
                return std::nullopt;
            }
            PythonResult parsed_row;
            parsed_row.relative_path = row->at("relative_path").as_string();
            parsed_row.ok = row->at("ok").as_bool();
            parsed_row.error = row->count("error") ? row->at("error").as_string() : std::string{};
            parsed_row.rating = row->count("rating") ? row->at("rating").as_number() : 0.0;
            parsed_row.revive_level = row->count("revive_level") ? static_cast<int>(row->at("revive_level").as_number()) : 0;
            parsed_row.key_count = row->count("key_count") ? static_cast<int>(row->at("key_count").as_number()) : 0;
            parsed_row.mode_name = row->count("mode_name") ? row->at("mode_name").as_string() : std::string{};
            by_path.emplace(parsed_row.relative_path, std::move(parsed_row));
        }
        return by_path;
    }
#else
    const std::vector<std::string> interpreters = {"python3", "python"};
    for (const auto& interpreter : interpreters) {
        const std::string command = interpreter + " " + quote_arg(script_path.string()) + " " +
                                    quote_arg(repo_root.string()) + " " + quote_arg(input_path.string()) + " 2>&1";
        const auto result = run_command(command);
        if (result.exit_code != 0) {
            diagnostics = result.output;
            continue;
        }

        JsonParseResult parsed = parse_json(result.output);
        if (!parsed.success()) {
            diagnostics = "failed to parse python output: " + parsed.error + "\nraw:\n" + result.output;
            continue;
        }

        const auto* root = parsed.root->as_object();
        if (root == nullptr) {
            diagnostics = "python output root was not an object";
            continue;
        }
        const auto results_it = root->find("results");
        if (results_it == root->end()) {
            diagnostics = "python output missing results";
            continue;
        }
        const auto* results = results_it->second.as_array();
        if (results == nullptr) {
            diagnostics = "python results was not an array";
            continue;
        }

        std::unordered_map<std::string, PythonResult> by_path;
        for (const auto& row_value : *results) {
            const auto* row = row_value.as_object();
            if (row == nullptr) {
                diagnostics = "python result row was not an object";
                return std::nullopt;
            }
            PythonResult parsed_row;
            parsed_row.relative_path = row->at("relative_path").as_string();
            parsed_row.ok = row->at("ok").as_bool();
            parsed_row.error = row->count("error") ? row->at("error").as_string() : std::string{};
            parsed_row.rating = row->count("rating") ? row->at("rating").as_number() : 0.0;
            parsed_row.revive_level = row->count("revive_level") ? static_cast<int>(row->at("revive_level").as_number()) : 0;
            parsed_row.key_count = row->count("key_count") ? static_cast<int>(row->at("key_count").as_number()) : 0;
            parsed_row.mode_name = row->count("mode_name") ? row->at("mode_name").as_string() : std::string{};
            by_path.emplace(parsed_row.relative_path, std::move(parsed_row));
        }
        return by_path;
    }
#endif

    return std::nullopt;
}

int run_compare(const Options& options, const char* argv0) {
    const auto repo_root = resolve_repo_root();
    if (!repo_root.has_value()) {
        std::cout << "[skip] 10k-calc python reference not available; skipping bms_10k_compare_smoke.\n";
        return 0;
    }
    const auto songs_root = resolve_songs_root(options, argv0);
    if (!songs_root.has_value()) {
        return 2;
    }

    std::vector<std::string> warnings;
    SongIndexOptions index_options;
    index_options.calculate_difficulty = true;
    SongIndex index = scan_songs(songs_root->u8string(), nullptr, warnings, {}, index_options);
    if (!warnings.empty()) {
        std::cout << "[warn] scan warnings: " << warnings.size() << '\n';
    }

    const std::uint64_t seed = options.seed.value_or(default_seed());
    const std::vector<SongEntry> selected = select_random_entries(index, options.count, seed);
    if (selected.empty()) {
        std::cerr << "[error] no comparable BMS entries found under " << songs_root->u8string() << '\n';
        return 2;
    }

    std::string diagnostics;
    const auto python_map = run_python_compare(*repo_root, *songs_root, selected, diagnostics);
    if (!python_map.has_value()) {
        std::cout << "[skip] python reference unavailable: " << compact_diagnostics(diagnostics) << '\n';
        return 0;
    }

    std::vector<CompareRow> rows;
    rows.reserve(selected.size());
    int python_failures = 0;
    for (const auto& entry : selected) {
        auto it = python_map->find(entry.path);
        if (it == python_map->end()) {
            std::cerr << "[warn] missing python result for " << entry.path << '\n';
            continue;
        }
        if (!it->second.ok) {
            ++python_failures;
            std::cout << "[skip] " << entry.path << " python error: " << it->second.error << '\n';
            continue;
        }
        CompareRow row;
        row.tenriff = entry;
        row.python = it->second;
        row.rating_delta = std::abs(entry.rating - it->second.rating);
        row.level_match = entry.level == it->second.revive_level;
        rows.push_back(std::move(row));
    }

    if (rows.empty()) {
        std::cerr << "[error] no successful comparisons were produced.\n";
        return 2;
    }

    int exact_rating_matches = 0;
    int exact_level_matches = 0;
    double total_abs_delta = 0.0;
    double max_abs_delta = 0.0;
    const CompareRow* max_row = nullptr;

    std::cout << "songs_root=" << songs_root->u8string() << '\n';
    std::cout << "seed=" << seed << " sample_count=" << rows.size() << " python_failures=" << python_failures << '\n';
    std::cout << '\n';

    for (const auto& row : rows) {
        if (row.rating_delta <= 1e-9) {
            ++exact_rating_matches;
        }
        if (row.level_match) {
            ++exact_level_matches;
        }
        total_abs_delta += row.rating_delta;
        if (row.rating_delta > max_abs_delta) {
            max_abs_delta = row.rating_delta;
            max_row = &row;
        }

        std::cout << row.tenriff.path << '\n';
        std::cout << "  tenriff rating=" << row.tenriff.rating
                  << " level=" << row.tenriff.level
                  << " layout=" << row.tenriff.layout_label << '\n';
        std::cout << "  python  rating=" << row.python.rating
                  << " level=" << row.python.revive_level
                  << " mode=" << row.python.mode_name
                  << " key_count=" << row.python.key_count << '\n';
        std::cout << "  delta   rating_abs=" << row.rating_delta
                  << " level_match=" << (row.level_match ? "yes" : "no") << '\n';
        std::cout << '\n';
    }

    const double avg_abs_delta = total_abs_delta / static_cast<double>(rows.size());
    std::cout << "summary\n";
    std::cout << "  exact_rating_matches=" << exact_rating_matches << "/" << rows.size() << '\n';
    std::cout << "  exact_level_matches=" << exact_level_matches << "/" << rows.size() << '\n';
    std::cout << "  avg_abs_rating_delta=" << avg_abs_delta << '\n';
    std::cout << "  max_abs_rating_delta=" << max_abs_delta;
    if (max_row != nullptr) {
        std::cout << " (" << max_row->tenriff.path << ")";
    }
    std::cout << '\n';

    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        return 2;
    }
    return run_compare(options, argv[0]);
}
