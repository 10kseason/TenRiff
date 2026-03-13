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
    return ext == ".bms" || ext == ".bme" || ext == ".bml";
}

bool parse_u64(std::string_view text, std::uint64_t& value) {
    if (text.empty()) {
        return false;
    }
    auto first = text.data();
    auto last = text.data() + text.size();
    auto result = std::from_chars(first, last, value);
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
    std::cout << "Usage: " << (argv0 ? argv0 : "bms_realworld_smoke")
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
        fs::path explicit_root = options.pack_root->lexically_normal();
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
                charts.push_back(ChartEntry{
                    entry.path().lexically_normal(),
                    relative.generic_u8string(),
                });
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
    return shuffled;
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

    for (const auto& chart : selected) {
        const auto result = loader.load(chart.full_path.u8string(), kSampleRate, kPlaybackRate, "ignore");
        const bool ok = result.success() && result.messages.empty();
        if (ok) {
            ++passed;
            std::cout << "[pass] " << chart.relative_path << '\n';
            continue;
        }

        ++failed;
        std::cout << "[fail] " << chart.relative_path << '\n';
        if (!result.error.empty()) {
            std::cout << "  error: " << result.error << '\n';
        }
        for (const auto& message : result.messages) {
            std::cout << "  message: " << message << '\n';
        }
    }

    std::cout << "[summary] seed=" << seed
              << " selected=" << selected.size()
              << " passed=" << passed
              << " failed=" << failed << '\n';
    std::cout << "[summary] selected charts:\n";
    for (const auto& chart : selected) {
        std::cout << "  - " << chart.relative_path << '\n';
    }

    return failed == 0 ? 0 : 1;
}
