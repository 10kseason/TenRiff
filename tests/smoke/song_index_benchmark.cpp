#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "app/SongIndex.h"

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using tenriff::app::SongIndex;
using tenriff::app::SongIndexLoadResult;
using tenriff::app::SongIndexOptions;
using tenriff::app::SongIndexProgress;
using tenriff::app::SongIndexProgressStage;
using tenriff::app::load_song_index;
using tenriff::app::save_song_index;
using tenriff::app::scan_songs;

struct Options {
    fs::path songs_root;
    fs::path cache_path;
    bool full_index = false;
    std::size_t sample_count = 2048;
    std::optional<std::uint64_t> seed;
    std::string profile = "safe";
};

struct StageTiming {
    SongIndexProgressStage stage = SongIndexProgressStage::ScanningFiles;
    Clock::time_point started{};
    Clock::time_point ended{};
    int processed = 0;
    int total = -1;
    bool seen = false;
};

struct SampleCandidate {
    fs::path full_path;
    std::string extension;
};

struct CensusResult {
    std::uint64_t candidate_count = 0;
    std::uint64_t directory_error_count = 0;
    std::uint64_t total_path_chars = 0;
    double elapsed_ms = 0.0;
    std::vector<SampleCandidate> samples;
    std::vector<std::string> warning_samples;
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

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - ('A' - 'a'));
        }
        return static_cast<char>(ch);
    });
    return value;
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

double elapsed_ms(const Clock::time_point& begin, const Clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

std::string stage_name(SongIndexProgressStage stage) {
    switch (stage) {
    case SongIndexProgressStage::ScanningFiles:
        return "ScanningFiles";
    case SongIndexProgressStage::BuildingMetadata:
        return "BuildingMetadata";
    case SongIndexProgressStage::SavingCache:
        return "SavingCache";
    default:
        return "Unknown";
    }
}

bool is_chart_extension(std::string_view lower_ext) {
    return lower_ext == ".bms" || lower_ext == ".bme" || lower_ext == ".bml" || lower_ext == ".pms";
}

std::string normalize_profile(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "fast" || value == "performance") {
        return "fast";
    }
    return "safe";
}

void print_usage(const char* argv0) {
    std::cout << "Usage: " << (argv0 ? argv0 : "song_index_benchmark")
              << " --songs-root <path> [--cache-path <path>] [--full-index]"
              << " [--sample-count <n>] [--seed <u64>] [--profile safe|fast]\n";
}

bool parse_options(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--songs-root") {
            if (i + 1 >= argc) {
                std::cerr << "[error] --songs-root requires a path.\n";
                return false;
            }
            options.songs_root = fs::u8path(argv[++i]);
            continue;
        }
        if (arg == "--cache-path") {
            if (i + 1 >= argc) {
                std::cerr << "[error] --cache-path requires a path.\n";
                return false;
            }
            options.cache_path = fs::u8path(argv[++i]);
            continue;
        }
        if (arg == "--full-index") {
            options.full_index = true;
            continue;
        }
        if (arg == "--sample-count") {
            if (i + 1 >= argc || !parse_size(argv[i + 1], options.sample_count)) {
                std::cerr << "[error] --sample-count requires a positive integer.\n";
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
        if (arg == "--profile") {
            if (i + 1 >= argc) {
                std::cerr << "[error] --profile requires safe or fast.\n";
                return false;
            }
            options.profile = normalize_profile(argv[++i]);
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        }
        std::cerr << "[error] Unknown argument: " << arg << '\n';
        return false;
    }

    if (options.songs_root.empty()) {
        std::cerr << "[error] --songs-root is required.\n";
        return false;
    }
    return true;
}

std::array<StageTiming, 3> make_stage_timings() {
    return {{
        {SongIndexProgressStage::ScanningFiles},
        {SongIndexProgressStage::BuildingMetadata},
        {SongIndexProgressStage::SavingCache},
    }};
}

StageTiming& stage_slot(std::array<StageTiming, 3>& stages, SongIndexProgressStage stage) {
    switch (stage) {
    case SongIndexProgressStage::ScanningFiles:
        return stages[0];
    case SongIndexProgressStage::BuildingMetadata:
        return stages[1];
    case SongIndexProgressStage::SavingCache:
        return stages[2];
    default:
        return stages[0];
    }
}

void print_stage_report(const std::array<StageTiming, 3>& stages) {
    std::cout << "stage timings:\n";
    for (const auto& stage : stages) {
        if (!stage.seen) {
            continue;
        }
        std::cout << "  - " << stage_name(stage.stage)
                  << ": " << std::fixed << std::setprecision(2)
                  << elapsed_ms(stage.started, stage.ended) << " ms"
                  << " (processed=" << stage.processed
                  << ", total=" << stage.total << ")\n";
    }
}

CensusResult run_census(const fs::path& songs_root,
                        std::size_t sample_count,
                        std::uint64_t seed) {
    CensusResult result;
    result.samples.reserve(sample_count);

    const auto begin = Clock::now();
    std::mt19937_64 rng(seed);
    std::error_code ec;
    fs::directory_options scan_options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(songs_root, scan_options, ec);
    fs::recursive_directory_iterator end;
    while (it != end) {
        if (ec) {
            ++result.directory_error_count;
            if (result.warning_samples.size() < 8u) {
                result.warning_samples.push_back(ec.message());
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
            ++result.directory_error_count;
            if (result.warning_samples.size() < 8u) {
                result.warning_samples.push_back(ec.message());
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

        const std::string ext = to_lower_ascii(entry.path().extension().u8string());
        if (!is_chart_extension(ext)) {
            it.increment(ec);
            continue;
        }

        ++result.candidate_count;
        result.total_path_chars += static_cast<std::uint64_t>(entry.path().generic_u8string().size());

        SampleCandidate candidate{entry.path(), ext};
        if (sample_count > 0) {
            if (result.samples.size() < sample_count) {
                result.samples.push_back(std::move(candidate));
            } else {
                std::uniform_int_distribution<std::uint64_t> dist(0, result.candidate_count - 1);
                const std::uint64_t slot = dist(rng);
                if (slot < result.samples.size()) {
                    result.samples[static_cast<std::size_t>(slot)] = std::move(candidate);
                }
            }
        }

        it.increment(ec);
    }

    result.elapsed_ms = elapsed_ms(begin, Clock::now());
    return result;
}

bool stage_sample_files(const std::vector<SampleCandidate>& samples, TempDirGuard& temp_dir, std::string& error) {
    std::error_code ec;
    const fs::path base = fs::current_path(ec) / ".bench-cache" /
                          ("song-index-sample-" +
                           std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                              Clock::now().time_since_epoch())
                                              .count()));
    fs::create_directories(base, ec);
    if (ec) {
        error = "Failed to create sample benchmark directory: " + ec.message();
        return false;
    }
    temp_dir.path = base;

    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto& sample = samples[i];
        const std::string filename = "sample_" + std::to_string(i) + sample.extension;
        const fs::path target = base / fs::u8path(filename);

        ec.clear();
        fs::create_hard_link(sample.full_path, target, ec);
        if (!ec) {
            continue;
        }

        ec.clear();
        fs::copy_file(sample.full_path, target, fs::copy_options::overwrite_existing, ec);
        if (!ec) {
            continue;
        }

        error = "Failed to stage sample chart '" + sample.full_path.u8string() + "': " + ec.message();
        return false;
    }

    return true;
}

int run_full_index_benchmark(const Options& options) {
    SongIndexOptions index_options;
    index_options.profile =
        (normalize_profile(options.profile) == "fast") ? tenriff::app::SongIndexProfile::Fast
                                                       : tenriff::app::SongIndexProfile::Safe;
    std::array<StageTiming, 3> stages = make_stage_timings();

    std::cout << "mode: full-index\n";
    std::cout << "songs_root: " << options.songs_root.u8string() << '\n';
    std::cout << "cache_path: " << options.cache_path.u8string() << '\n';
    std::cout << "profile: " << normalize_profile(options.profile) << '\n';

    const auto cold_load_begin = Clock::now();
    SongIndexLoadResult cold_load = load_song_index(options.cache_path.u8string(), index_options);
    const auto cold_load_end = Clock::now();

    std::vector<std::string> warnings;
    const auto scan_begin = Clock::now();
    SongIndex index = scan_songs(
        options.songs_root.u8string(),
        (cold_load.success() && cold_load.loaded_from_file) ? &cold_load.index : nullptr,
        warnings,
        [&](const SongIndexProgress& progress) {
            StageTiming& slot = stage_slot(stages, progress.stage);
            const auto now = Clock::now();
            if (!slot.seen) {
                slot.seen = true;
                slot.started = now;
            }
            slot.ended = now;
            slot.processed = progress.processed;
            slot.total = progress.total;
        },
        index_options);
    const auto scan_end = Clock::now();

    std::string save_error;
    const auto save_begin = Clock::now();
    const bool saved = save_song_index(
        options.cache_path.u8string(),
        index,
        index_options,
        &save_error,
        [&](const SongIndexProgress& progress) {
            StageTiming& slot = stage_slot(stages, progress.stage);
            const auto now = Clock::now();
            if (!slot.seen) {
                slot.seen = true;
                slot.started = now;
            }
            slot.ended = now;
            slot.processed = progress.processed;
            slot.total = progress.total;
        });
    const auto save_end = Clock::now();

    const auto warm_load_begin = Clock::now();
    SongIndexLoadResult warm_load = load_song_index(options.cache_path.u8string(), index_options);
    const auto warm_load_end = Clock::now();

    std::cout << "\nresults:\n";
    std::cout << "  cold_cache_load_ms: " << std::fixed << std::setprecision(2)
              << elapsed_ms(cold_load_begin, cold_load_end) << '\n';
    std::cout << "  cold_cache_loaded: " << (cold_load.loaded_from_file ? "true" : "false") << '\n';
    std::cout << "  scan_total_ms: " << elapsed_ms(scan_begin, scan_end) << '\n';
    std::cout << "  indexed_entries: " << index.entries.size() << '\n';
    std::cout << "  warning_count: " << warnings.size() << '\n';
    std::cout << "  save_cache_ms: " << elapsed_ms(save_begin, save_end) << '\n';
    std::cout << "  save_cache_ok: " << (saved ? "true" : "false") << '\n';
    std::cout << "  warm_cache_load_ms: " << elapsed_ms(warm_load_begin, warm_load_end) << '\n';
    std::cout << "  warm_cache_loaded: " << (warm_load.loaded_from_file ? "true" : "false") << '\n';
    std::cout << "  warm_cache_entries: " << warm_load.index.entries.size() << '\n';
    if (!save_error.empty()) {
        std::cout << "  save_cache_error: " << save_error << '\n';
    }
    if (!cold_load.error.empty()) {
        std::cout << "  cold_cache_error: " << cold_load.error << '\n';
    }
    if (!warm_load.error.empty()) {
        std::cout << "  warm_cache_error: " << warm_load.error << '\n';
    }
    print_stage_report(stages);
    return 0;
}

int run_lightweight_benchmark(const Options& options) {
    const std::uint64_t seed = options.seed.value_or(
        static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    SongIndexOptions index_options;
    index_options.profile =
        (normalize_profile(options.profile) == "fast") ? tenriff::app::SongIndexProfile::Fast
                                                       : tenriff::app::SongIndexProfile::Safe;

    std::cout << "mode: lightweight-census-sample\n";
    std::cout << "songs_root: " << options.songs_root.u8string() << '\n';
    std::cout << "cache_path: " << options.cache_path.u8string() << '\n';
    std::cout << "profile: " << normalize_profile(options.profile) << '\n';
    std::cout << "sample_count: " << options.sample_count << '\n';
    std::cout << "seed: " << seed << '\n';

    CensusResult census = run_census(options.songs_root, options.sample_count, seed);
    std::cout << "\ncensus:\n";
    std::cout << "  candidate_count: " << census.candidate_count << '\n';
    std::cout << "  scan_only_ms: " << std::fixed << std::setprecision(2) << census.elapsed_ms << '\n';
    std::cout << "  sampled_candidates: " << census.samples.size() << '\n';
    std::cout << "  directory_error_count: " << census.directory_error_count << '\n';
    if (census.candidate_count > 0) {
        const double avg_path_chars =
            static_cast<double>(census.total_path_chars) / static_cast<double>(census.candidate_count);
        std::cout << "  avg_full_path_chars: " << avg_path_chars << '\n';
    }

    if (census.samples.empty()) {
        std::cout << "\nNo chart candidates found.\n";
        return 0;
    }

    TempDirGuard temp_dir;
    std::string stage_error;
    if (!stage_sample_files(census.samples, temp_dir, stage_error)) {
        std::cerr << "[error] " << stage_error << '\n';
        return 1;
    }

    std::vector<std::string> sample_warnings;
    std::array<StageTiming, 3> stages = make_stage_timings();
    const auto sample_begin = Clock::now();
    SongIndex sample_index = scan_songs(
        temp_dir.path.u8string(),
        nullptr,
        sample_warnings,
        [&](const SongIndexProgress& progress) {
            StageTiming& slot = stage_slot(stages, progress.stage);
            const auto now = Clock::now();
            if (!slot.seen) {
                slot.seen = true;
                slot.started = now;
            }
            slot.ended = now;
            slot.processed = progress.processed;
            slot.total = progress.total;
        },
        index_options);
    const auto sample_end = Clock::now();

    std::string save_error;
    const fs::path sample_cache = temp_dir.path / "sample_song_index.json";
    const auto save_begin = Clock::now();
    const bool saved = save_song_index(sample_cache.u8string(), sample_index, index_options, &save_error);
    const auto save_end = Clock::now();

    const auto warm_load_begin = Clock::now();
    SongIndexLoadResult warm_load = load_song_index(sample_cache.u8string(), index_options);
    const auto warm_load_end = Clock::now();

    const double sample_total_ms = elapsed_ms(sample_begin, sample_end);
    const double sample_rate = (sample_total_ms > 0.0)
                                   ? (static_cast<double>(census.samples.size()) / (sample_total_ms / 1000.0))
                                   : 0.0;
    const double estimated_full_metadata_ms = (sample_rate > 0.0)
                                                  ? (static_cast<double>(census.candidate_count) / sample_rate) * 1000.0
                                                  : 0.0;
    const double estimated_total_cold_ms = census.elapsed_ms + estimated_full_metadata_ms;

    std::cout << "\nsample index:\n";
    std::cout << "  sample_index_ms: " << sample_total_ms << '\n';
    std::cout << "  sample_entries_indexed: " << sample_index.entries.size() << '\n';
    std::cout << "  sample_warning_count: " << sample_warnings.size() << '\n';
    std::cout << "  sample_entries_per_second: " << sample_rate << '\n';
    std::cout << "  estimated_full_metadata_ms: " << estimated_full_metadata_ms << '\n';
    std::cout << "  estimated_total_cold_scan_plus_metadata_ms: " << estimated_total_cold_ms << '\n';
    std::cout << "  sample_save_cache_ms: " << elapsed_ms(save_begin, save_end) << '\n';
    std::cout << "  sample_save_cache_ok: " << (saved ? "true" : "false") << '\n';
    if (!save_error.empty()) {
        std::cout << "  sample_save_cache_error: " << save_error << '\n';
    }
    std::cout << "  sample_warm_cache_load_ms: " << elapsed_ms(warm_load_begin, warm_load_end) << '\n';
    std::cout << "  sample_warm_cache_loaded: " << (warm_load.loaded_from_file ? "true" : "false") << '\n';
    std::cout << "  sample_warm_cache_entries: " << warm_load.index.entries.size() << '\n';
    print_stage_report(stages);

    if (!census.warning_samples.empty()) {
        std::cout << "\ncensus warning sample (" << census.warning_samples.size() << "):\n";
        for (const auto& warning : census.warning_samples) {
            std::cout << "  - " << warning << '\n';
        }
    }
    if (!sample_warnings.empty()) {
        const std::size_t sample_count = (std::min)(sample_warnings.size(), static_cast<std::size_t>(8));
        std::cout << "\nsample parse warning sample (" << sample_count << "/" << sample_warnings.size() << "):\n";
        for (std::size_t i = 0; i < sample_count; ++i) {
            std::cout << "  - " << sample_warnings[i] << '\n';
        }
    }

    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage(argv[0]);
        return 1;
    }

    std::error_code ec;
    options.songs_root = fs::absolute(options.songs_root, ec).lexically_normal();
    if (ec || options.songs_root.empty() || !fs::exists(options.songs_root, ec) || !fs::is_directory(options.songs_root, ec)) {
        std::cerr << "[error] songs root not found: " << options.songs_root.u8string() << '\n';
        return 1;
    }

    if (options.cache_path.empty()) {
        const fs::path bench_root = fs::current_path(ec) / ".bench-cache";
        options.cache_path = bench_root / "song-index-benchmark.json";
    }
    options.cache_path = fs::absolute(options.cache_path, ec).lexically_normal();

    if (options.full_index) {
        return run_full_index_benchmark(options);
    }
    return run_lightweight_benchmark(options);
}
