#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "app/ChartLoader.h"
#include "app/ModeManager.h"
#include "config/Config.h"

namespace {

namespace fs = std::filesystem;

constexpr std::size_t kDefaultCount = 10;
constexpr int kSampleRate = 44100;
constexpr double kPlaybackRate = 1.0;

struct Options {
    std::optional<fs::path> pack_root;
    std::size_t count = kDefaultCount;
    std::optional<std::uint64_t> seed;
};

enum class ParseStatus {
    Ok,
    Help,
    Error,
};

struct ChartEntry {
    fs::path full_path;
    std::string relative_path;
};

struct Span {
    int lane = 0;
    int64_t start = 0;
    int64_t end = 0;
};

struct ModeCase {
    std::string name;
    tenriff::config::ModeConfig config;
    int expected_lane_count = 0;
    bool expect_no_holds = false;
    bool expect_release_disabled = false;
    bool expect_no_ln_release_removed = false;
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

bool is_bms_chart_path(const fs::path& path) {
    const std::string ext = to_lower_ascii(path.extension().u8string());
    return ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".pms";
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

fs::path path_from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::u8path(value);
#else
    return fs::path(value);
#endif
}

fs::path executable_directory(const char* argv0) {
    std::error_code ec;
    fs::path executable_path = fs::absolute(path_from_utf8(argv0 ? argv0 : ""), ec);
    if (ec || executable_path.empty()) {
        executable_path = fs::current_path(ec);
    }
    if (executable_path.has_filename()) {
        executable_path = executable_path.parent_path();
    }
    return executable_path.lexically_normal();
}

void print_usage(const char* argv0) {
    std::cout << "Usage: " << (argv0 ? argv0 : "bms_mode_smoke")
              << " [--pack-root <path>] [--count <n>] [--seed <u64>]\n";
}

ParseStatus parse_options(int argc, char** argv, Options& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return ParseStatus::Help;
        }
        if (arg == "--pack-root") {
            if (index + 1 >= argc) {
                std::cerr << "[error] --pack-root requires a path.\n";
                return ParseStatus::Error;
            }
            options.pack_root = path_from_utf8(argv[++index]);
            continue;
        }
        if (arg == "--count") {
            if (index + 1 >= argc || !parse_size(argv[index + 1], options.count)) {
                std::cerr << "[error] --count requires a positive integer.\n";
                return ParseStatus::Error;
            }
            ++index;
            continue;
        }
        if (arg == "--seed") {
            if (index + 1 >= argc) {
                std::cerr << "[error] --seed requires an unsigned integer.\n";
                return ParseStatus::Error;
            }
            std::uint64_t parsed = 0;
            if (!parse_u64(argv[index + 1], parsed)) {
                std::cerr << "[error] --seed requires an unsigned integer.\n";
                return ParseStatus::Error;
            }
            options.seed = parsed;
            ++index;
            continue;
        }

        std::cerr << "[error] Unknown argument: " << arg << '\n';
        print_usage(argv[0]);
        return ParseStatus::Error;
    }

    return ParseStatus::Ok;
}

std::optional<fs::path> resolve_pack_root(const Options& options, const char* argv0) {
    auto is_valid_directory = [](const fs::path& path) {
        std::error_code ec;
        return !path.empty() && fs::exists(path, ec) && fs::is_directory(path, ec) && !ec;
    };

    if (options.pack_root.has_value()) {
        const fs::path explicit_root = options.pack_root->lexically_normal();
        if (is_valid_directory(explicit_root)) {
            return explicit_root;
        }
        std::cerr << "[error] Pack root not found or is not a directory: "
                  << explicit_root.u8string() << '\n';
        return std::nullopt;
    }

    std::error_code ec;
    const fs::path cwd_candidate =
        (fs::current_path(ec) / "build" / "Release" / "Songs" / "10Key-Revive-pack").lexically_normal();
    const fs::path exe_candidate =
        (executable_directory(argv0) / "Songs" / "10Key-Revive-pack").lexically_normal();

    for (const fs::path& candidate : std::array<fs::path, 2>{cwd_candidate, exe_candidate}) {
        if (is_valid_directory(candidate)) {
            return candidate;
        }
    }

    std::cerr << "[error] Failed to locate 10Key-Revive-pack.\n";
    std::cerr << "  tried: " << cwd_candidate.u8string() << '\n';
    std::cerr << "  tried: " << exe_candidate.u8string() << '\n';
    return std::nullopt;
}

std::vector<ChartEntry> collect_charts(const fs::path& pack_root) {
    std::vector<ChartEntry> charts;
    std::error_code ec;
    fs::recursive_directory_iterator it(pack_root, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;

    while (it != end) {
        if (ec) {
            ec.clear();
            it.increment(ec);
            continue;
        }

        const fs::directory_entry& entry = *it;
        if (entry.is_regular_file(ec) && !ec && is_bms_chart_path(entry.path())) {
            fs::path relative = fs::relative(entry.path(), pack_root, ec);
            if (ec || relative.empty()) {
                ec.clear();
                relative = entry.path().lexically_relative(pack_root);
            }
            if (!relative.empty()) {
                charts.push_back(ChartEntry{entry.path().lexically_normal(), relative.generic_u8string()});
            }
        }

        ec.clear();
        it.increment(ec);
    }

    std::sort(charts.begin(), charts.end(), [](const ChartEntry& lhs, const ChartEntry& rhs) {
        return lhs.relative_path < rhs.relative_path;
    });
    return charts;
}

std::uint64_t make_default_seed() {
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return static_cast<std::uint64_t>(now);
}

std::vector<ChartEntry> select_random_charts(const std::vector<ChartEntry>& charts,
                                             std::size_t count,
                                             std::uint64_t seed) {
    std::vector<ChartEntry> shuffled = charts;
    std::mt19937_64 rng(seed);
    std::shuffle(shuffled.begin(), shuffled.end(), rng);
    shuffled.resize(count);
    std::sort(shuffled.begin(), shuffled.end(), [](const ChartEntry& lhs, const ChartEntry& rhs) {
        return lhs.relative_path < rhs.relative_path;
    });
    return shuffled;
}

bool is_time_sorted(const tenriff::gameplay::GameplayChart& chart) {
    for (std::size_t i = 1; i < chart.notes.size(); ++i) {
        const auto& previous = chart.notes[i - 1];
        const auto& current = chart.notes[i];
        if (current.start_sample < previous.start_sample) {
            return false;
        }
        if (current.start_sample == previous.start_sample && current.lane < previous.lane) {
            return false;
        }
    }
    return true;
}

bool has_lane_overlap(const tenriff::gameplay::GameplayChart& chart) {
    if (chart.lane_count <= 0) {
        return false;
    }

    std::vector<Span> spans;
    spans.reserve(chart.notes.size());
    for (const auto& note : chart.notes) {
        int64_t end = note.end_sample.value_or(note.start_sample);
        if (end < note.start_sample) {
            end = note.start_sample;
        }
        spans.push_back({note.lane, note.start_sample, end});
    }

    std::sort(spans.begin(), spans.end(), [](const Span& lhs, const Span& rhs) {
        if (lhs.start == rhs.start) {
            return lhs.end < rhs.end;
        }
        return lhs.start < rhs.start;
    });

    std::vector<int64_t> lane_end(static_cast<std::size_t>(chart.lane_count), std::numeric_limits<int64_t>::min());
    for (const auto& span : spans) {
        if (span.lane <= 0 || span.lane > chart.lane_count) {
            continue;
        }
        const auto index = static_cast<std::size_t>(span.lane - 1);
        if (span.start <= lane_end[index]) {
            return true;
        }
        lane_end[index] = std::max(lane_end[index], span.end);
    }
    return false;
}

std::optional<std::string> first_lane_overlap_detail(const tenriff::gameplay::GameplayChart& chart) {
    if (chart.lane_count <= 0) {
        return std::nullopt;
    }

    std::vector<Span> spans;
    spans.reserve(chart.notes.size());
    for (const auto& note : chart.notes) {
        int64_t end = note.end_sample.value_or(note.start_sample);
        if (end < note.start_sample) {
            end = note.start_sample;
        }
        spans.push_back({note.lane, note.start_sample, end});
    }

    std::sort(spans.begin(), spans.end(), [](const Span& lhs, const Span& rhs) {
        if (lhs.lane != rhs.lane) {
            return lhs.lane < rhs.lane;
        }
        if (lhs.start != rhs.start) {
            return lhs.start < rhs.start;
        }
        return lhs.end < rhs.end;
    });

    for (std::size_t i = 1; i < spans.size(); ++i) {
        const auto& previous = spans[i - 1];
        const auto& current = spans[i];
        if (previous.lane == current.lane && current.start <= previous.end) {
            return "lane " + std::to_string(current.lane) +
                   " previous=[" + std::to_string(previous.start) + "," + std::to_string(previous.end) + "]" +
                   " current=[" + std::to_string(current.start) + "," + std::to_string(current.end) + "]";
        }
    }

    return std::nullopt;
}

bool has_active_mod(const std::vector<std::string>& mods, std::string_view token) {
    return std::find(mods.begin(), mods.end(), std::string(token)) != mods.end();
}

void append_common_failure_checks(const tenriff::app::ModeManagerResult& result,
                                  int expected_lane_count,
                                  std::vector<std::string>& failures) {
    if (result.chart.lane_count != expected_lane_count) {
        failures.push_back("lane_count mismatch");
    }
    if (result.chart.notes.empty()) {
        failures.push_back("produced empty chart");
    }
    if (!is_time_sorted(result.chart)) {
        failures.push_back("notes are not time-sorted");
    }
    if (has_lane_overlap(result.chart)) {
        failures.push_back("lane overlap detected");
        if (const auto detail = first_lane_overlap_detail(result.chart); detail.has_value()) {
            failures.push_back(*detail);
        }
    }

    for (const auto& note : result.chart.notes) {
        if (note.lane <= 0 || note.lane > result.chart.lane_count) {
            failures.push_back("note lane out of range");
            break;
        }
        if (note.end_sample.has_value() && note.end_sample.value() <= note.start_sample) {
            failures.push_back("invalid hold timing");
            break;
        }
    }
}

std::vector<ModeCase> build_mode_cases(int source_lane_count, std::uint32_t seed) {
    using tenriff::config::ModeConfig;

    std::vector<ModeCase> cases;

    auto make_case = [&](std::string name,
                         std::string key_mode,
                         std::string random,
                         std::vector<std::string> mods,
                         int expected_lane_count,
                         bool expect_no_holds,
                         bool expect_release_disabled,
                         bool expect_no_ln_release_removed) {
        ModeConfig config;
        config.key_mode = std::move(key_mode);
        config.random = std::move(random);
        config.random_seed = seed;
        config.mods = std::move(mods);
        cases.push_back(ModeCase{
            std::move(name),
            std::move(config),
            expected_lane_count,
            expect_no_holds,
            expect_release_disabled,
            expect_no_ln_release_removed,
        });
    };

    make_case("native_sr_judge_easy", "auto", "sr", {"judge_easy"}, source_lane_count, false, false, false);

    switch (source_lane_count) {
    case 10:
        make_case("10k_to_4k_sr_full_short", "4k", "sr", {"full_short_notes", "no_ln_release"}, 4, true, true, true);
        make_case("10k_to_6k_fr_native", "6k", "fr", {}, 6, false, false, false);
        make_case("10k_to_6k_fr_full_long_hard", "6k", "fr", {"full_long_notes", "judge_hard"}, 6, false, false, false);
        make_case("10k_to_8k_sr_no_release", "8k", "sr", {"no_ln_release"}, 8, false, true, false);
        break;
    case 9:
        make_case("9k_to_5k_sr_full_short", "5k", "sr", {"full_short_notes", "no_ln_release"}, 5, true, true, true);
        make_case("9k_to_10k_fr_full_long", "10k", "fr", {"full_long_notes"}, 10, false, false, false);
        break;
    case 8:
        make_case("8k_to_5k_sr_full_short", "5k", "sr", {"full_short_notes", "no_ln_release"}, 5, true, true, true);
        make_case("8k_to_10k_fr_full_long", "10k", "fr", {"full_long_notes", "judge_hard"}, 10, false, false, false);
        break;
    case 7:
        make_case("7k_to_4k_sr_full_short", "4k", "sr", {"full_short_notes", "no_ln_release"}, 4, true, true, true);
        make_case("7k_to_10k_fr_full_long", "10k", "fr", {"full_long_notes", "judge_hard"}, 10, false, false, false);
        break;
    case 6:
        make_case("6k_to_4k_sr_full_short", "4k", "sr", {"full_short_notes", "no_ln_release"}, 4, true, true, true);
        make_case("6k_to_8k_fr_full_long", "8k", "fr", {"full_long_notes"}, 8, false, false, false);
        break;
    case 5:
        make_case("5k_to_4k_sr_full_short", "4k", "sr", {"full_short_notes", "no_ln_release"}, 4, true, true, true);
        make_case("5k_to_8k_fr_full_long", "8k", "fr", {"full_long_notes", "judge_hard"}, 8, false, false, false);
        break;
    case 4:
        make_case("4k_to_8k_fr_full_long", "8k", "fr", {"full_long_notes", "judge_hard"}, 8, false, false, false);
        make_case("4k_to_16k_sr_no_release", "16k", "sr", {"no_ln_release"}, 16, false, true, false);
        break;
    case 16:
        make_case("16k_to_10k_sr_full_short", "10k", "sr", {"full_short_notes", "no_ln_release"}, 10, true, true, true);
        make_case("16k_to_8k_fr_no_release", "8k", "fr", {"no_ln_release", "judge_hard"}, 8, false, true, false);
        break;
    default:
        if (source_lane_count > 4) {
            make_case("fallback_to_4k_sr_full_short", "4k", "sr", {"full_short_notes", "no_ln_release"}, 4, true, true, true);
        }
        if (source_lane_count < 10) {
            make_case("fallback_to_10k_fr_full_long", "10k", "fr", {"full_long_notes"}, 10, false, false, false);
        }
        break;
    }

    return cases;
}

bool run_case(const ChartEntry& chart_entry,
              const tenriff::app::ChartLoadResult& load_result,
              const ModeCase& mode_case,
              std::vector<std::string>& failures,
              std::vector<std::string>& warnings) {
    warnings.clear();
    failures.clear();

    const tenriff::config::JudgeConfig judge_config;
    const auto result = tenriff::app::manage_modes(load_result.chart,
                                                   load_result.format,
                                                   mode_case.config,
                                                   judge_config,
                                                   kPlaybackRate,
                                                   load_result.base_bpm,
                                                   kSampleRate);

    append_common_failure_checks(result, mode_case.expected_lane_count, failures);

    if (mode_case.expect_no_holds) {
        for (const auto& note : result.chart.notes) {
            if (note.end_sample.has_value()) {
                failures.push_back("full_short_notes left hold notes behind");
                break;
            }
        }
    }

    if (mode_case.expect_release_disabled) {
        for (const auto& note : result.chart.notes) {
            if (note.end_sample.has_value() && note.release_required) {
                failures.push_back("release_required remained enabled");
                break;
            }
        }
    }

    if (mode_case.expect_no_ln_release_removed && has_active_mod(result.active_mods, "no_ln_release")) {
        failures.push_back("redundant no_ln_release mod was not removed");
    }

    warnings = result.warnings;
    if (load_result.format != tenriff::app::ChartFormat::Bms) {
        failures.push_back("chart format was not recognized as BMS");
    }
    if (!load_result.error.empty()) {
        failures.push_back("loader returned an error");
    }
    if (!load_result.messages.empty()) {
        warnings.insert(warnings.end(), load_result.messages.begin(), load_result.messages.end());
    }

    return failures.empty();
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    const ParseStatus parse_status = parse_options(argc, argv, options);
    if (parse_status == ParseStatus::Help) {
        return 0;
    }
    if (parse_status == ParseStatus::Error) {
        return 2;
    }

    const auto pack_root = resolve_pack_root(options, argv[0]);
    if (!pack_root.has_value()) {
        return 2;
    }

    const std::vector<ChartEntry> charts = collect_charts(*pack_root);
    if (charts.size() < options.count) {
        std::cerr << "[error] Requested " << options.count << " chart(s), but only "
                  << charts.size() << " BMS chart(s) were found under "
                  << pack_root->u8string() << ".\n";
        return 2;
    }

    const std::uint64_t seed = options.seed.value_or(make_default_seed());
    const std::vector<ChartEntry> selected = select_random_charts(charts, options.count, seed);

    std::cout << "[info] Pack root: " << pack_root->u8string() << '\n';
    std::cout << "[info] Total charts discovered: " << charts.size() << '\n';
    std::cout << "[info] Seed: " << seed << '\n';
    std::cout << "[info] Selected charts:\n";
    for (const auto& chart : selected) {
        std::cout << "  - " << chart.relative_path << '\n';
    }

    tenriff::app::ChartLoader loader;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::size_t executed_cases = 0;

    for (std::size_t chart_index = 0; chart_index < selected.size(); ++chart_index) {
        const auto& chart = selected[chart_index];
        const auto load_result = loader.load(chart.full_path.u8string(), kSampleRate, kPlaybackRate, "ignore");
        if (!load_result.success()) {
            ++failed;
            std::cout << "[fail] " << chart.relative_path << " :: load\n";
            std::cout << "  error: " << load_result.error << '\n';
            continue;
        }

        const auto cases = build_mode_cases(load_result.chart.lane_count,
                                            static_cast<std::uint32_t>(seed + static_cast<std::uint64_t>(chart_index) * 17u));
        for (const auto& mode_case : cases) {
            ++executed_cases;
            std::vector<std::string> failures;
            std::vector<std::string> warnings;
            if (run_case(chart, load_result, mode_case, failures, warnings)) {
                ++passed;
                std::cout << "[pass] " << chart.relative_path << " :: " << mode_case.name << '\n';
                continue;
            }

            ++failed;
            std::cout << "[fail] " << chart.relative_path << " :: " << mode_case.name << '\n';
            for (const auto& failure : failures) {
                std::cout << "  failure: " << failure << '\n';
            }
            for (const auto& warning : warnings) {
                std::cout << "  warning: " << warning << '\n';
            }
        }
    }

    std::cout << "[summary] seed=" << seed
              << " charts=" << selected.size()
              << " cases=" << executed_cases
              << " passed=" << passed
              << " failed=" << failed << '\n';

    return failed == 0 ? 0 : 1;
}
