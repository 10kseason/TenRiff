#include "app/BmsKeyConverter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "app/BmsGameplayBuilder.h"
#include "chart/BmsChartNorm.h"
#include "chart/BmsParser.h"
#include "chart/BmsTimeline.h"
#include "gameplay/KeyModeConverter.h"

namespace tenriff::app {

namespace {

constexpr double kPositionEpsilon = 1e-9;
constexpr int kQuantizeSliceCount = 1920;
constexpr int kMaxExactSliceCount = 19200;
constexpr int kAutoSampleRateFallback = 44100;
constexpr int kMinAudioSampleRate = 8000;
constexpr int kMaxAudioSampleRate = 192000;
constexpr double kBeatsPerMeasure = 4.0;
constexpr double kStopTicksPerBeat = 48.0;

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

std::string to_upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'a' && ch <= 'z') {
            return static_cast<char>(ch - ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string to_upper_ascii(std::string_view value) {
    return to_upper_ascii(std::string(value));
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch + ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string to_lower_ascii(std::string_view value) {
    return to_lower_ascii(std::string(value));
}

std::string normalize_preset_token(std::string_view token) {
    std::string normalized;
    normalized.reserve(token.size());
    for (unsigned char ch : token) {
        if (std::isspace(ch) != 0 || ch == '-' || ch == '_') {
            continue;
        }
        if (ch >= 'A' && ch <= 'Z') {
            normalized.push_back(static_cast<char>(ch + ('a' - 'A')));
        } else {
            normalized.push_back(static_cast<char>(ch));
        }
    }
    return normalized;
}

bool is_charge_note_lnmode(std::string_view value) {
    return trim_copy(value) == "2";
}

bool is_note_lane_channel(std::string_view channel) {
    return channel.size() == 2 && (channel[0] == '1' || channel[0] == '2' || channel[0] == '5' || channel[0] == '6');
}

bool is_mine_lane_channel(std::string_view channel) {
    return channel.size() == 2 && (channel[0] == 'D' || channel[0] == 'E');
}

std::string mine_channel_for_note_channel(std::string_view channel) {
    if (channel.size() != 2 || (channel[0] != '1' && channel[0] != '2')) {
        return {};
    }
    std::string mine_channel(channel);
    mine_channel[0] = channel[0] == '1' ? 'D' : 'E';
    return mine_channel;
}

bool is_mode_token(std::string_view token) {
    if (token.empty()) {
        return false;
    }
    std::string normalized = to_upper_ascii(token);
    if (normalized.size() < 2 || normalized.back() != 'K') {
        return false;
    }
    return std::all_of(normalized.begin(), normalized.end() - 1, [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

bool is_filtered_header_key(std::string_view key) {
    const std::string normalized = to_upper_ascii(key);
    return normalized == "PLAYER" || normalized == "PLAYMODE" || normalized == "KEYMODE" ||
           normalized == "LNOBJ" || normalized == "LNMODE" || is_mode_token(normalized);
}

int64_t scale_samples(int64_t samples, double rate) {
    if (rate <= 0.0 || !std::isfinite(rate)) {
        return samples;
    }
    return static_cast<int64_t>(std::llround(static_cast<double>(samples) / rate));
}

bool is_plausible_audio_sample_rate(int sample_rate) {
    return sample_rate >= kMinAudioSampleRate && sample_rate <= kMaxAudioSampleRate;
}

uint16_t read_le_u16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8u);
}

uint32_t read_le_u32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8u) |
           (static_cast<uint32_t>(data[2]) << 16u) |
           (static_cast<uint32_t>(data[3]) << 24u);
}

std::filesystem::path path_from_utf8(std::string_view value) {
#ifdef _WIN32
    try {
        return std::filesystem::u8path(std::string(value));
    } catch (...) {
        return std::filesystem::path(std::string(value));
    }
#else
    return std::filesystem::path(std::string(value));
#endif
}

std::string normalize_path_key(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return to_lower_ascii(std::move(value));
}

std::string normalize_asset_reference(std::string value) {
    value = trim_copy(value);
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            value = trim_copy(value.substr(1, value.size() - 2));
        }
    }
    return value;
}

bool is_audio_extension(std::string_view extension) {
    return extension == ".wav" || extension == ".wave" || extension == ".ogg" || extension == ".mp3";
}

std::filesystem::path normalize_resolved_path(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path normalized = path;
    if (!normalized.is_absolute()) {
        const fs::path absolute = fs::absolute(normalized, ec);
        if (!ec && !absolute.empty()) {
            normalized = absolute;
        }
        ec.clear();
    }

    const fs::path canonical = fs::weakly_canonical(normalized, ec);
    if (!ec && !canonical.empty()) {
        return canonical;
    }
    return normalized.lexically_normal();
}

struct AudioAssetLookup {
    bool built = false;
    std::unordered_map<std::string, std::filesystem::path> by_relative;
    std::unordered_map<std::string, std::filesystem::path> by_filename;
};

void build_audio_asset_lookup(const std::filesystem::path& chart_path, AudioAssetLookup& lookup) {
    if (lookup.built) {
        return;
    }
    lookup.built = true;

    namespace fs = std::filesystem;
    const fs::path root = chart_path.parent_path();
    if (root.empty()) {
        return;
    }

    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    while (it != end) {
        if (ec) {
            ec.clear();
            it.increment(ec);
            continue;
        }

        const fs::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        ec.clear();

        const fs::path full = normalize_resolved_path(entry.path());
        const std::string extension = to_lower_ascii(full.extension().u8string());
        if (!is_audio_extension(extension)) {
            it.increment(ec);
            continue;
        }

        fs::path relative = fs::relative(full, root, ec);
        if (ec || relative.empty()) {
            ec.clear();
            relative = full.lexically_relative(root);
        }
        if (!relative.empty()) {
            lookup.by_relative.emplace(normalize_path_key(relative.generic_u8string()), full);
        }
        lookup.by_filename.emplace(to_lower_ascii(full.filename().u8string()), full);

        it.increment(ec);
    }
}

std::vector<std::filesystem::path> build_audio_reference_candidates(const std::string& reference) {
    namespace fs = std::filesystem;
    const fs::path ref_path = path_from_utf8(reference);
    const std::string extension = to_lower_ascii(ref_path.extension().u8string());
    const bool prefer_ogg_for_wav_reference = extension == ".wav" || extension == ".wave";

    std::vector<fs::path> candidates;
    std::unordered_set<std::string> seen;
    auto push = [&](const fs::path& candidate) {
        const std::string key = normalize_path_key(candidate.generic_u8string());
        if (seen.emplace(key).second) {
            candidates.push_back(candidate);
        }
    };
    auto push_replaced_extension = [&](std::string_view next_extension) {
        fs::path candidate = ref_path;
        if (extension.empty()) {
            candidate += next_extension;
        } else {
            candidate.replace_extension(next_extension);
        }
        push(candidate);
    };

    if (!prefer_ogg_for_wav_reference) {
        push(ref_path);
    }

    if (extension.empty()) {
        push_replaced_extension(".ogg");
        push_replaced_extension(".wav");
        push_replaced_extension(".wave");
        push_replaced_extension(".mp3");
    } else if (extension == ".wav" || extension == ".wave") {
        push_replaced_extension(".ogg");
        push(ref_path);
        push_replaced_extension(extension == ".wav" ? ".wave" : ".wav");
        push_replaced_extension(".mp3");
    } else if (is_audio_extension(extension)) {
        push_replaced_extension(".ogg");
        push_replaced_extension(".wav");
        push_replaced_extension(".wave");
        push_replaced_extension(".mp3");
    }

    return candidates;
}

std::optional<std::filesystem::path> lookup_audio_candidate(const std::filesystem::path& chart_path,
                                                            const std::filesystem::path& candidate,
                                                            AudioAssetLookup& lookup) {
    namespace fs = std::filesystem;
    const fs::path direct = candidate.is_absolute()
                                ? candidate.lexically_normal()
                                : (chart_path.parent_path() / candidate).lexically_normal();
    std::error_code ec;
    if (!direct.empty() && fs::exists(direct, ec) && !ec) {
        return normalize_resolved_path(direct);
    }

    build_audio_asset_lookup(chart_path, lookup);

    auto rel_it = lookup.by_relative.find(normalize_path_key(candidate.generic_u8string()));
    if (rel_it != lookup.by_relative.end()) {
        return rel_it->second;
    }

    auto file_it = lookup.by_filename.find(to_lower_ascii(candidate.filename().u8string()));
    if (file_it != lookup.by_filename.end()) {
        return file_it->second;
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> resolve_audio_reference_path(const std::filesystem::path& chart_path,
                                                                  const std::string& reference,
                                                                  AudioAssetLookup& lookup) {
    const auto candidates = build_audio_reference_candidates(reference);
    for (const auto& candidate : candidates) {
        auto resolved = lookup_audio_candidate(chart_path, candidate, lookup);
        if (resolved.has_value()) {
            return resolved;
        }
    }
    return std::nullopt;
}

std::optional<int> probe_wav_sample_rate(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::array<uint8_t, 12> riff{};
    file.read(reinterpret_cast<char*>(riff.data()), static_cast<std::streamsize>(riff.size()));
    if (file.gcount() != static_cast<std::streamsize>(riff.size()) ||
        std::string_view(reinterpret_cast<const char*>(riff.data()), 4) != "RIFF" ||
        std::string_view(reinterpret_cast<const char*>(riff.data() + 8), 4) != "WAVE") {
        return std::nullopt;
    }

    while (file) {
        std::array<uint8_t, 8> chunk{};
        file.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
        if (file.gcount() != static_cast<std::streamsize>(chunk.size())) {
            break;
        }

        const uint32_t chunk_size = read_le_u32(chunk.data() + 4);
        if (std::string_view(reinterpret_cast<const char*>(chunk.data()), 4) == "fmt " && chunk_size >= 16) {
            std::array<uint8_t, 16> fmt{};
            file.read(reinterpret_cast<char*>(fmt.data()), static_cast<std::streamsize>(fmt.size()));
            if (file.gcount() != static_cast<std::streamsize>(fmt.size())) {
                return std::nullopt;
            }
            const uint16_t channels = read_le_u16(fmt.data() + 2);
            const int sample_rate = static_cast<int>(read_le_u32(fmt.data() + 4));
            if (channels > 0 && is_plausible_audio_sample_rate(sample_rate)) {
                return sample_rate;
            }
            return std::nullopt;
        }

        const std::streamoff skip = static_cast<std::streamoff>(chunk_size + (chunk_size & 1u));
        file.seekg(skip, std::ios::cur);
    }

    return std::nullopt;
}

std::optional<int> probe_ogg_vorbis_sample_rate_light(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::vector<uint8_t> bytes(64 * 1024);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(std::max<std::streamsize>(0, file.gcount())));

    static constexpr std::array<uint8_t, 7> kVorbisId = {0x01, 'v', 'o', 'r', 'b', 'i', 's'};
    for (std::size_t i = 0; i + 16 <= bytes.size(); ++i) {
        if (!std::equal(kVorbisId.begin(), kVorbisId.end(), bytes.begin() + static_cast<std::ptrdiff_t>(i))) {
            continue;
        }
        const int sample_rate = static_cast<int>(read_le_u32(bytes.data() + i + 12));
        if (is_plausible_audio_sample_rate(sample_rate)) {
            return sample_rate;
        }
    }

    return std::nullopt;
}

std::optional<int> probe_mp3_sample_rate_light(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::vector<uint8_t> bytes(1024 * 1024);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(std::max<std::streamsize>(0, file.gcount())));

    std::size_t begin = 0;
    if (bytes.size() >= 10 && bytes[0] == 'I' && bytes[1] == 'D' && bytes[2] == '3') {
        const std::size_t tag_size = (static_cast<std::size_t>(bytes[6] & 0x7f) << 21u) |
                                     (static_cast<std::size_t>(bytes[7] & 0x7f) << 14u) |
                                     (static_cast<std::size_t>(bytes[8] & 0x7f) << 7u) |
                                     static_cast<std::size_t>(bytes[9] & 0x7f);
        begin = std::min<std::size_t>(bytes.size(), 10u + tag_size);
    }

    static constexpr int kMpeg1Rates[3] = {44100, 48000, 32000};
    static constexpr int kMpeg2Rates[3] = {22050, 24000, 16000};
    static constexpr int kMpeg25Rates[3] = {11025, 12000, 8000};

    for (std::size_t i = begin; i + 4 <= bytes.size(); ++i) {
        if (bytes[i] != 0xff || (bytes[i + 1] & 0xe0u) != 0xe0u) {
            continue;
        }

        const int version = (bytes[i + 1] >> 3u) & 0x03u;
        const int layer = (bytes[i + 1] >> 1u) & 0x03u;
        const int sample_rate_index = (bytes[i + 2] >> 2u) & 0x03u;
        if (version == 1 || layer == 0 || sample_rate_index == 3) {
            continue;
        }

        int sample_rate = 0;
        if (version == 3) {
            sample_rate = kMpeg1Rates[sample_rate_index];
        } else if (version == 2) {
            sample_rate = kMpeg2Rates[sample_rate_index];
        } else {
            sample_rate = kMpeg25Rates[sample_rate_index];
        }
        if (is_plausible_audio_sample_rate(sample_rate)) {
            return sample_rate;
        }
    }

    return std::nullopt;
}

std::optional<int> probe_audio_sample_rate_light(const std::filesystem::path& path) {
    const std::string extension = to_lower_ascii(path.extension().u8string());
    if (extension == ".wav" || extension == ".wave") {
        return probe_wav_sample_rate(path);
    }
    if (extension == ".ogg") {
        return probe_ogg_vorbis_sample_rate_light(path);
    }
    if (extension == ".mp3") {
        return probe_mp3_sample_rate_light(path);
    }
    return std::nullopt;
}

std::map<std::string, std::size_t> collect_bms_audio_reference_counts(const chart::BmsChart& chart,
                                                                      bool note_lanes) {
    std::map<std::string, std::size_t> counts;
    for (const auto& command : chart.commands) {
        const bool matches = note_lanes ? is_note_lane_channel(command.channel) : command.channel == "01";
        if (!matches || command.data.size() % 2 != 0) {
            continue;
        }
        for (std::size_t i = 0; i + 2 <= command.data.size(); i += 2) {
            const std::string token = to_upper_ascii(command.data.substr(i, 2));
            if (token != "00") {
                ++counts[token];
            }
        }
    }
    return counts;
}

std::optional<int> choose_sample_rate_from_references(const std::filesystem::path& chart_path,
                                                      const chart::BmsChart& chart,
                                                      const std::map<std::string, std::size_t>& reference_counts) {
    if (reference_counts.empty()) {
        return std::nullopt;
    }

    struct RateScore {
        std::size_t weight = 0;
        std::string first_path;
    };

    AudioAssetLookup lookup;
    std::map<int, RateScore> scores;
    for (const auto& [object_id, count] : reference_counts) {
        auto wav_it = chart.wav.find(object_id);
        if (wav_it == chart.wav.end()) {
            continue;
        }

        const std::string reference = normalize_asset_reference(wav_it->second);
        if (reference.empty()) {
            continue;
        }

        auto resolved = resolve_audio_reference_path(chart_path, reference, lookup);
        if (!resolved.has_value()) {
            continue;
        }

        auto sample_rate = probe_audio_sample_rate_light(resolved.value());
        if (!sample_rate.has_value()) {
            continue;
        }

        auto& score = scores[sample_rate.value()];
        score.weight += std::max<std::size_t>(1u, count);
        if (score.first_path.empty()) {
            score.first_path = resolved->filename().u8string();
        }
    }

    if (scores.empty()) {
        return std::nullopt;
    }

    auto best = scores.begin();
    for (auto it = std::next(scores.begin()); it != scores.end(); ++it) {
        if (it->second.weight > best->second.weight) {
            best = it;
            continue;
        }
        if (it->second.weight == best->second.weight) {
            const int lhs_distance = std::abs(it->first - kAutoSampleRateFallback);
            const int rhs_distance = std::abs(best->first - kAutoSampleRateFallback);
            if (lhs_distance < rhs_distance || (lhs_distance == rhs_distance && it->first < best->first)) {
                best = it;
            }
        }
    }

    return best->first;
}

int resolve_effective_sample_rate(const std::string& input_path,
                                  const chart::BmsChart& chart,
                                  int requested_sample_rate,
                                  std::vector<std::string>& warnings) {
    if (requested_sample_rate > 0) {
        return requested_sample_rate;
    }

    const std::filesystem::path chart_path = path_from_utf8(input_path);
    auto keysound_rate = choose_sample_rate_from_references(chart_path,
                                                           chart,
                                                           collect_bms_audio_reference_counts(chart, true));
    if (keysound_rate.has_value()) {
        return keysound_rate.value();
    }

    auto bgm_rate = choose_sample_rate_from_references(chart_path,
                                                       chart,
                                                       collect_bms_audio_reference_counts(chart, false));
    if (bgm_rate.has_value()) {
        return bgm_rate.value();
    }

    warnings.push_back("Auto sample rate fell back to " + std::to_string(kAutoSampleRateFallback) +
                       " Hz because no referenced BMS audio sample rate could be probed.");
    return kAutoSampleRateFallback;
}

std::size_t hold_count(const gameplay::GameplayChart& chart) {
    return static_cast<std::size_t>(std::count_if(chart.notes.begin(), chart.notes.end(), [](const auto& note) {
        return note.end_sample.has_value();
    }));
}

struct BmsPositionSlot {
    int measure = 0;
    int64_t numerator = 0;
    int64_t denominator = 1;
    double measure_fraction = 0.0;
    double absolute_position = 0.0;
};

struct SourceNotePlacement {
    int source_lane = 0;
    int64_t start_sample = 0;
    std::optional<int64_t> end_sample;
    BmsPositionSlot start;
    std::optional<BmsPositionSlot> end;
    std::string object_id;
    bool release_required = false;
};

struct SourceMinePlacement {
    int64_t sample = 0;
    BmsPositionSlot slot;
    std::string object_id;
};

struct PendingLongNote {
    int lane = 0;
    int64_t start_sample = 0;
    BmsPositionSlot start;
    std::string object_id;
    std::size_t sequence = 0;
};

struct PlacementEntry {
    SourceNotePlacement placement;
    std::size_t sequence = 0;
};

struct LayoutDefinition {
    int lane_count = 0;
    std::vector<std::string> lane_channels;
    std::optional<std::string> player_header;
    std::optional<std::pair<std::string, std::string>> value_header;
    std::optional<std::string> flag_header;
};

const std::vector<BmsKeyConverterPreset>& preset_table() {
    static const std::vector<BmsKeyConverterPreset> presets = {
        {"default", "Default (8K)", 8, 8, 2, 4, true},
        {"10k", "10K Preset", 10, 10, 1, 5, true, 0u},
        {"9k", "9K Preset", 9, 8, 2, 4, true},
        {"8k", "8K Preset", 8, 8, 2, 4, true},
        {"7k", "7K Preset", 7, 7, 2, 4, false},
        {"a8k", "A8K", 8, 7, 7, 2, true},
        {"a9k", "A9K", 9, 7, 7, 2, true},
        {"a10k", "A10K", 10, 7, 7, 2, true},
        {"dt6", "DownTo6K", 6, 6, 4, 2, true},
        {"dt4", "DownTo4K", 4, 4, 4, 2, true},
    };
    return presets;
}

bool preset_matches_token(const BmsKeyConverterPreset& preset, std::string_view token) {
    const std::string normalized = normalize_preset_token(token);
    if (normalized.empty()) {
        return false;
    }

    if (normalized == normalize_preset_token(preset.token)) {
        return true;
    }

    if ((preset.token == "10k" && normalized == "tenk") ||
        (preset.token == "9k" && normalized == "ninek") ||
        (preset.token == "8k" && normalized == "eightk") ||
        (preset.token == "7k" && normalized == "sevenk") ||
        (preset.token == "dt6" && normalized == "downto6k") ||
        (preset.token == "dt4" && normalized == "downto4k")) {
        return true;
    }

    return false;
}
std::optional<gameplay::KeyModeConversionAlgorithm> parse_conversion_algorithm(std::string_view token) {
    const std::string normalized = normalize_preset_token(token);
    if (normalized.empty() || normalized == "krr" || normalized == "krrcream" ||
        normalized == "legacy" || normalized == "krrlegacy" || normalized == "n2nc") {
        return gameplay::KeyModeConversionAlgorithm::Krrcream;
    }
    if (normalized == "nk2" || normalized == "nativek2") {
        return gameplay::KeyModeConversionAlgorithm::NK2;
    }
    if (normalized == "nk3" || normalized == "keyweavernk3" ||
        normalized == "vcrr") {
        return gameplay::KeyModeConversionAlgorithm::NK3;
    }
    return std::nullopt;
}

std::string conversion_algorithm_name(gameplay::KeyModeConversionAlgorithm algorithm) {
    if (algorithm == gameplay::KeyModeConversionAlgorithm::NK3) {
        return "NK3";
    }
    return algorithm == gameplay::KeyModeConversionAlgorithm::NK2 ? "nK2"
                                                                   : "krrcream";
}

LayoutDefinition resolve_layout_definition(int lane_count) {
    switch (lane_count) {
    case 4:
        return LayoutDefinition{4, {"11", "12", "13", "14"}, std::nullopt, std::nullopt, std::nullopt};
    case 5:
        return LayoutDefinition{5, {"11", "12", "13", "14", "15"}, std::nullopt, std::nullopt, std::nullopt};
    case 6:
        return LayoutDefinition{6, {"11", "12", "13", "14", "15", "16"}, std::string("1"), std::nullopt, std::string("5K")};
    case 8:
        return LayoutDefinition{8, {"11", "12", "13", "14", "15", "16", "18", "19"},
                                std::string("1"), std::nullopt, std::string("7K")};
    case 9:
        return LayoutDefinition{9, {"11", "12", "13", "14", "15", "22", "23", "24", "25"},
                                std::nullopt, std::make_pair(std::string("PLAYMODE"), std::string("9K")), std::nullopt};
    case 10:
        return LayoutDefinition{10, {"11", "12", "13", "14", "15", "21", "22", "23", "24", "25"},
                                std::string("3"), std::nullopt, std::nullopt};
    case 16:
        return LayoutDefinition{16, {"11", "12", "13", "14", "15", "16", "18", "19",
                                     "21", "22", "23", "24", "25", "26", "28", "29"},
                                std::string("3"), std::make_pair(std::string("PLAYMODE"), std::string("16K")), std::nullopt};
    default:
        break;
    }

    if (lane_count < 1 || lane_count > 18) {
        return {};
    }

    // Preserve TenRiff's established standard layouts above. The remaining
    // counts use every visible BMS lane channel in deterministic left/right
    // order and carry an explicit key-count header so they round-trip without
    // relying on a parser's layout heuristic.
    static constexpr std::array<std::string_view, 9> kPlayerOne = {
        "11", "12", "13", "14", "15", "16", "17", "18", "19"};
    static constexpr std::array<std::string_view, 9> kPlayerTwo = {
        "21", "22", "23", "24", "25", "26", "27", "28", "29"};

    std::vector<std::string> channels;
    channels.reserve(static_cast<std::size_t>(lane_count));
    if (lane_count <= 9) {
        for (int lane = 0; lane < lane_count; ++lane) {
            channels.emplace_back(kPlayerOne[static_cast<std::size_t>(lane)]);
        }
    } else {
        const int player_one_count = (lane_count + 1) / 2;
        const int player_two_count = lane_count - player_one_count;
        for (int lane = 0; lane < player_one_count; ++lane) {
            channels.emplace_back(kPlayerOne[static_cast<std::size_t>(lane)]);
        }
        for (int lane = 0; lane < player_two_count; ++lane) {
            channels.emplace_back(kPlayerTwo[static_cast<std::size_t>(lane)]);
        }
    }

    return LayoutDefinition{lane_count,
                            std::move(channels),
                            std::string(lane_count <= 9 ? "1" : "3"),
                            std::make_pair(std::string("PLAYMODE"),
                                           std::to_string(lane_count) + "K"),
                            std::nullopt};
}

gameplay::KeyModeConverterOptions default_converter_options(int source_lane_count,
                                                            int target_lane_count,
                                                            uint32_t seed,
                                                            double base_bpm,
                                                            int sample_rate) {
    gameplay::KeyModeConverterOptions options;
    options.target_lane_count = target_lane_count;
    options.seed = seed;
    options.base_bpm = base_bpm;
    options.sample_rate = sample_rate;

    if (target_lane_count >= 8 && target_lane_count > source_lane_count && source_lane_count <= 7) {
        options.max_keys = std::max(1, source_lane_count);
        options.min_keys = std::max(1, source_lane_count);
        options.transform_speed_slot = 2;
        return options;
    }

    if (target_lane_count <= 4) {
        options.max_keys = target_lane_count;
        options.min_keys = target_lane_count;
        options.transform_speed_slot = 2;
        return options;
    }

    if (target_lane_count <= 6) {
        options.max_keys = target_lane_count;
        options.min_keys = std::min(target_lane_count, 4);
        options.transform_speed_slot = 2;
        return options;
    }

    options.max_keys = std::min(target_lane_count, 8);
    options.min_keys = 2;
    options.transform_speed_slot = 4;
    return options;
}

BmsPositionSlot make_slot_from_event(const chart::BmsNormalizedEvent& event) {
    BmsPositionSlot slot;
    slot.measure = event.measure;
    slot.numerator = static_cast<int64_t>(event.slice_index);
    slot.denominator = static_cast<int64_t>(std::max<std::size_t>(1, event.slice_count));
    slot.measure_fraction = event.slice_count > 0
                                ? static_cast<double>(event.slice_index) / static_cast<double>(event.slice_count)
                                : 0.0;
    slot.absolute_position = event.position;
    return slot;
}

std::vector<SourceNotePlacement> build_source_note_placements(const chart::BmsTimeline& timeline,
                                                              const chart::BmsChart& parsed_chart,
                                                              double rate) {
    std::vector<PlacementEntry> entries;
    entries.reserve(timeline.events.size());
    std::unordered_map<int, std::size_t> last_normal_note_by_lane;
    std::unordered_map<int, PendingLongNote> pending_long_notes_by_lane;

    const auto is_long_note_channel = [](std::string_view channel) {
        return channel.size() == 2 && (channel[0] == '5' || channel[0] == '6');
    };

    const std::string lnobj = [&parsed_chart]() {
        auto it = parsed_chart.headers.find("LNOBJ");
        if (it == parsed_chart.headers.end()) {
            return std::string{};
        }
        return to_upper_ascii(trim_copy(it->second));
    }();
    const bool release_required = [&parsed_chart]() {
        auto it = parsed_chart.headers.find("LNMODE");
        if (it == parsed_chart.headers.end()) {
            return false;
        }
        return is_charge_note_lnmode(it->second);
    }();

    std::size_t sequence = 0;
    for (const auto& scheduled : timeline.events) {
        if (scheduled.event.type != chart::BmsNormalizedEventType::Note || !scheduled.event.lane.has_value()) {
            continue;
        }

        const int lane = static_cast<int>(scheduled.event.lane.value());
        const int64_t sample = std::max<int64_t>(0, scale_samples(scheduled.time_samples, rate));
        const BmsPositionSlot slot = make_slot_from_event(scheduled.event);
        const std::string object_id = to_upper_ascii(scheduled.event.object_id);

        if (is_long_note_channel(scheduled.event.channel)) {
            auto pending_it = pending_long_notes_by_lane.find(lane);
            if (pending_it == pending_long_notes_by_lane.end()) {
                pending_long_notes_by_lane.emplace(lane, PendingLongNote{lane, sample, slot, object_id, sequence++});
            } else {
                PlacementEntry entry;
                entry.placement.source_lane = lane;
                entry.placement.start_sample = pending_it->second.start_sample;
                entry.placement.start = pending_it->second.start;
                entry.placement.object_id = pending_it->second.object_id;
                entry.sequence = pending_it->second.sequence;
                if (sample > pending_it->second.start_sample) {
                    entry.placement.end_sample = sample;
                    entry.placement.end = slot;
                    entry.placement.release_required = release_required;
                }
                entries.push_back(std::move(entry));
                pending_long_notes_by_lane.erase(pending_it);
            }
            continue;
        }

        if (!lnobj.empty() && object_id == lnobj) {
            auto last_it = last_normal_note_by_lane.find(lane);
            if (last_it == last_normal_note_by_lane.end() || last_it->second >= entries.size()) {
                continue;
            }
            auto& placement = entries[last_it->second].placement;
            if (!placement.end_sample.has_value() && sample > placement.start_sample) {
                placement.end_sample = sample;
                placement.end = slot;
                placement.release_required = release_required;
            }
            last_normal_note_by_lane.erase(last_it);
            continue;
        }

        PlacementEntry entry;
        entry.placement.source_lane = lane;
        entry.placement.start_sample = sample;
        entry.placement.start = slot;
        entry.placement.object_id = object_id;
        entry.sequence = sequence++;
        last_normal_note_by_lane[lane] = entries.size();
        entries.push_back(std::move(entry));
    }

    for (const auto& [lane, pending] : pending_long_notes_by_lane) {
        PlacementEntry entry;
        entry.placement.source_lane = lane;
        entry.placement.start_sample = pending.start_sample;
        entry.placement.start = pending.start;
        entry.placement.object_id = pending.object_id;
        entry.sequence = pending.sequence;
        entries.push_back(std::move(entry));
    }

    std::stable_sort(entries.begin(), entries.end(), [](const PlacementEntry& lhs, const PlacementEntry& rhs) {
        if (lhs.placement.start_sample != rhs.placement.start_sample) {
            return lhs.placement.start_sample < rhs.placement.start_sample;
        }
        if (lhs.placement.source_lane != rhs.placement.source_lane) {
            return lhs.placement.source_lane < rhs.placement.source_lane;
        }
        return lhs.sequence < rhs.sequence;
    });

    std::vector<SourceNotePlacement> placements;
    placements.reserve(entries.size());
    for (auto& entry : entries) {
        placements.push_back(std::move(entry.placement));
    }
    return placements;
}

std::vector<SourceMinePlacement> build_source_mine_placements(const chart::BmsTimeline& timeline,
                                                              double rate) {
    std::vector<SourceMinePlacement> placements;
    for (const auto& scheduled : timeline.events) {
        if (scheduled.event.type != chart::BmsNormalizedEventType::Mine ||
            !scheduled.event.lane.has_value()) {
            continue;
        }
        SourceMinePlacement placement;
        placement.sample = std::max<int64_t>(0, scale_samples(scheduled.time_samples, rate));
        placement.slot = make_slot_from_event(scheduled.event);
        placement.object_id = to_upper_ascii(scheduled.event.object_id);
        placements.push_back(std::move(placement));
    }
    return placements;
}

double seconds_from_position_delta(double delta, double bpm) {
    if (delta <= 0.0 || bpm <= 0.0 || !std::isfinite(bpm)) {
        return 0.0;
    }
    return ((delta * kBeatsPerMeasure) * 60.0) / bpm;
}

double stop_ticks_to_seconds(double stop_value, double bpm) {
    if (stop_value <= 0.0 || bpm <= 0.0 || !std::isfinite(bpm)) {
        return 0.0;
    }
    return ((stop_value / kStopTicksPerBeat) * 60.0) / bpm;
}

struct TimingSegment {
    double start_sample = 0.0;
    double end_sample = 0.0;
    double start_position = 0.0;
    double end_position = 0.0;
    bool is_stop = false;
};

struct SamplePositionMap {
    std::vector<TimingSegment> segments;
    double chart_end_position = 0.0;
    double duration_samples = 0.0;

    [[nodiscard]] double position_for_sample(int64_t sample) const {
        const double clamped_sample = std::clamp(static_cast<double>(sample), 0.0, duration_samples);
        if (segments.empty()) {
            return 0.0;
        }

        auto it = std::lower_bound(segments.begin(), segments.end(), clamped_sample, [](const TimingSegment& segment, double value) {
            return segment.end_sample + 0.5 < value;
        });
        if (it == segments.end()) {
            return chart_end_position;
        }
        if (it->is_stop || std::abs(it->end_sample - it->start_sample) <= 1e-9) {
            return it->start_position;
        }
        const double ratio = std::clamp((clamped_sample - it->start_sample) / (it->end_sample - it->start_sample), 0.0, 1.0);
        return it->start_position + ((it->end_position - it->start_position) * ratio);
    }
};

SamplePositionMap build_sample_position_map(const chart::BmsNormalizedChart& chart, int sample_rate_hz) {
    SamplePositionMap map;
    if (sample_rate_hz <= 0) {
        return map;
    }

    const double sample_rate = static_cast<double>(sample_rate_hz);
    double current_bpm = chart.base_bpm > 0.0 ? chart.base_bpm : 120.0;
    double current_position = 0.0;
    double current_time_samples = 0.0;
    std::size_t index = 0;

    while (index < chart.events.size()) {
        const double group_position = chart.events[index].position;
        const double delta_position = std::max(0.0, group_position - current_position);
        if (delta_position > kPositionEpsilon) {
            const double delta_samples = seconds_from_position_delta(delta_position, current_bpm) * sample_rate;
            if (delta_samples > 1e-9) {
                map.segments.push_back(TimingSegment{current_time_samples, current_time_samples + delta_samples,
                                                     current_position, group_position, false});
            }
            current_time_samples += delta_samples;
            current_position = group_position;
        }

        std::size_t group_end = index;
        while (group_end < chart.events.size() &&
               std::abs(chart.events[group_end].position - group_position) <= kPositionEpsilon) {
            ++group_end;
        }

        double stop_seconds = 0.0;
        for (std::size_t i = index; i < group_end; ++i) {
            const auto& event = chart.events[i];
            if (event.type == chart::BmsNormalizedEventType::BpmChange && event.value.has_value() &&
                event.value.value() > 0.0 && std::isfinite(event.value.value())) {
                current_bpm = event.value.value();
            } else if (event.type == chart::BmsNormalizedEventType::Stop && event.value.has_value()) {
                stop_seconds += stop_ticks_to_seconds(event.value.value(), current_bpm);
            }
        }

        if (stop_seconds > 0.0) {
            const double stop_samples = stop_seconds * sample_rate;
            map.segments.push_back(TimingSegment{current_time_samples, current_time_samples + stop_samples,
                                                 current_position, current_position, true});
            current_time_samples += stop_samples;
        }

        index = group_end;
    }

    if (!chart.measures.empty()) {
        const double chart_end_position = chart.measures.back().end();
        const double remaining = std::max(0.0, chart_end_position - current_position);
        if (remaining > kPositionEpsilon) {
            const double delta_samples = seconds_from_position_delta(remaining, current_bpm) * sample_rate;
            if (delta_samples > 1e-9) {
                map.segments.push_back(TimingSegment{current_time_samples, current_time_samples + delta_samples,
                                                     current_position, chart_end_position, false});
            }
            current_time_samples += delta_samples;
            current_position = chart_end_position;
        }
    }

    map.chart_end_position = current_position;
    map.duration_samples = current_time_samples;
    return map;
}

std::optional<BmsPositionSlot> quantize_position_to_slot(const chart::BmsNormalizedChart& chart,
                                                         double absolute_position,
                                                         int max_slice_count) {
    if (chart.measures.empty()) {
        BmsPositionSlot slot;
        slot.measure = 0;
        slot.numerator = 0;
        slot.denominator = 1;
        slot.measure_fraction = 0.0;
        slot.absolute_position = 0.0;
        return slot;
    }

    int measure_index = static_cast<int>(chart.measures.size()) - 1;
    for (int i = 0; i < static_cast<int>(chart.measures.size()); ++i) {
        if (absolute_position < chart.measures[static_cast<std::size_t>(i)].end() - kPositionEpsilon ||
            std::abs(absolute_position - chart.measures[static_cast<std::size_t>(i)].start) <= kPositionEpsilon) {
            measure_index = i;
            break;
        }
    }

    if (measure_index < 0) {
        return std::nullopt;
    }

    const auto& measure = chart.measures[static_cast<std::size_t>(measure_index)];
    if (measure.length <= 0.0) {
        return std::nullopt;
    }

    double fraction = (absolute_position - measure.start) / measure.length;
    fraction = std::clamp(fraction, 0.0, 1.0);

    int64_t numerator = static_cast<int64_t>(std::llround(fraction * static_cast<double>(max_slice_count)));
    if (numerator >= max_slice_count) {
        BmsPositionSlot slot;
        slot.measure = measure_index + 1;
        slot.numerator = 0;
        slot.denominator = 1;
        slot.measure_fraction = 0.0;
        slot.absolute_position = measure.end();
        return slot;
    }

    const int64_t denominator = max_slice_count;
    const int64_t divisor = std::gcd(std::max<int64_t>(1, numerator), denominator);

    BmsPositionSlot slot;
    slot.measure = measure_index;
    slot.numerator = numerator / divisor;
    slot.denominator = denominator / divisor;
    slot.measure_fraction = static_cast<double>(slot.numerator) / static_cast<double>(slot.denominator);
    slot.absolute_position = measure.start + (measure.length * slot.measure_fraction);
    return slot;
}

bool position_after(const BmsPositionSlot& lhs, const BmsPositionSlot& rhs) {
    return lhs.absolute_position > rhs.absolute_position + kPositionEpsilon;
}

std::set<std::string> collect_used_object_tokens(const chart::BmsChart& parsed_chart,
                                                 const std::vector<SourceNotePlacement>& placements) {
    std::set<std::string> used;
    for (const auto& command : parsed_chart.commands) {
        if (!is_note_lane_channel(command.channel) || command.data.size() % 2 != 0) {
            continue;
        }
        for (std::size_t index = 0; index < command.data.size(); index += 2) {
            used.insert(to_upper_ascii(command.data.substr(index, 2)));
        }
    }
    for (const auto& [slot, _] : parsed_chart.wav) {
        used.insert(to_upper_ascii(slot));
    }
    for (const auto& placement : placements) {
        if (!placement.object_id.empty()) {
            used.insert(to_upper_ascii(placement.object_id));
        }
    }
    return used;
}

std::optional<std::string> allocate_lnobj_token(const chart::BmsChart& parsed_chart,
                                                const std::vector<SourceNotePlacement>& placements) {
    static constexpr std::string_view kAlphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::set<std::string> used = collect_used_object_tokens(parsed_chart, placements);

    for (auto it_hi = kAlphabet.rbegin(); it_hi != kAlphabet.rend(); ++it_hi) {
        for (auto it_lo = kAlphabet.rbegin(); it_lo != kAlphabet.rend(); ++it_lo) {
            std::string token;
            token.push_back(*it_hi);
            token.push_back(*it_lo);
            if (used.find(token) == used.end()) {
                return token;
            }
        }
    }
    return std::nullopt;
}

struct OutputEvent {
    BmsPositionSlot slot;
    std::string channel;
    std::string token;
};

uint64_t lcm_with_cap(uint64_t lhs, uint64_t rhs, uint64_t cap) {
    if (lhs == 0 || rhs == 0) {
        return 0;
    }
    const uint64_t divisor = std::gcd(lhs, rhs);
    const uint64_t scaled = lhs / divisor;
    if (scaled > cap / rhs) {
        return 0;
    }
    const uint64_t value = scaled * rhs;
    return value > cap ? 0 : value;
}

std::string build_measure_data(const std::vector<OutputEvent>& events, std::vector<std::string>& warnings) {
    if (events.empty()) {
        return {};
    }

    uint64_t exact_slice_count = 1;
    bool exact = true;
    for (const auto& event : events) {
        const uint64_t denominator = static_cast<uint64_t>(std::max<int64_t>(1, event.slot.denominator));
        const uint64_t next = lcm_with_cap(exact_slice_count, denominator, kMaxExactSliceCount);
        if (next == 0) {
            exact = false;
            break;
        }
        exact_slice_count = next;
    }

    const std::size_t slice_count = static_cast<std::size_t>(exact ? exact_slice_count : kQuantizeSliceCount);
    std::vector<std::string> slots(slice_count, "00");

    for (const auto& event : events) {
        std::size_t index = 0;
        if (exact && event.slot.denominator > 0 && (exact_slice_count % static_cast<uint64_t>(event.slot.denominator)) == 0) {
            index = static_cast<std::size_t>(event.slot.numerator *
                                             static_cast<int64_t>(exact_slice_count / static_cast<uint64_t>(event.slot.denominator)));
        } else {
            const double scaled = std::round(event.slot.measure_fraction * static_cast<double>(slice_count));
            index = static_cast<std::size_t>(std::clamp<int64_t>(static_cast<int64_t>(scaled), 0, static_cast<int64_t>(slice_count) - 1));
        }
        if (index >= slots.size()) {
            index = slots.size() - 1;
        }
        if (slots[index] != "00" && slots[index] != event.token) {
            warnings.push_back("Dropped a colliding note token while writing BMS output.");
            continue;
        }
        slots[index] = event.token;
    }

    std::string data;
    data.reserve(slots.size() * 2u);
    for (const auto& slot : slots) {
        data += slot;
    }
    return data;
}

std::string build_bms_text(const chart::BmsChart& parsed_chart,
                           const chart::BmsNormalizedChart& normalized_chart,
                           const std::vector<SourceNotePlacement>& placements,
                           const std::vector<SourceMinePlacement>& mine_placements,
                           const gameplay::GameplayChart& converted_chart,
                           const LayoutDefinition& layout,
                           int sample_rate,
                           std::vector<std::string>& warnings) {
    const bool any_holds = hold_count(converted_chart) > 0;
    const bool any_charge = std::any_of(converted_chart.notes.begin(), converted_chart.notes.end(), [](const auto& note) {
        return note.end_sample.has_value() && note.release_required;
    });
    const std::optional<std::string> lnobj_token = any_holds ? allocate_lnobj_token(parsed_chart, placements) : std::optional<std::string>{};

    SamplePositionMap position_map = build_sample_position_map(normalized_chart, sample_rate);

    std::vector<OutputEvent> generated_events;
    generated_events.reserve(converted_chart.notes.size() * 2u + converted_chart.mines.size());

    for (const auto& note : converted_chart.notes) {
        if (note.lane <= 0 || note.lane > static_cast<int>(layout.lane_channels.size())) {
            warnings.push_back("Skipped a converted note because its lane was outside the target layout.");
            continue;
        }
        if (note.note_id >= placements.size()) {
            warnings.push_back("Skipped a converted note because its source placement could not be resolved.");
            continue;
        }

        const auto& source = placements[note.note_id];
        BmsPositionSlot start_slot = source.start;
        if (note.start_sample != source.start_sample) {
            const double absolute_position = position_map.position_for_sample(note.start_sample);
            const auto quantized_start =
                quantize_position_to_slot(normalized_chart, absolute_position, kQuantizeSliceCount);
            if (!quantized_start.has_value()) {
                warnings.push_back("Skipped a converted note because its generated start could not be represented in BMS.");
                continue;
            }
            start_slot = *quantized_start;
        }

        const std::string& channel = layout.lane_channels[static_cast<std::size_t>(note.lane - 1)];
        generated_events.push_back(OutputEvent{start_slot, channel, source.object_id});

        if (!note.end_sample.has_value() || !lnobj_token.has_value()) {
            continue;
        }

        std::optional<BmsPositionSlot> end_slot;
        if (source.end_sample.has_value() && source.end.has_value() && source.end_sample.value() == note.end_sample.value()) {
            end_slot = source.end;
        } else {
            const double absolute_position = position_map.position_for_sample(note.end_sample.value());
            end_slot = quantize_position_to_slot(normalized_chart, absolute_position, kQuantizeSliceCount);
        }

        if (!end_slot.has_value() || !position_after(end_slot.value(), start_slot)) {
            warnings.push_back("Converted a clipped long note to a tap because its tail could not be represented in BMS.");
            continue;
        }

        generated_events.push_back(OutputEvent{end_slot.value(), channel, lnobj_token.value()});
    }

    for (const auto& mine : converted_chart.mines) {
        if (mine.lane <= 0 || mine.lane > static_cast<int>(layout.lane_channels.size())) {
            warnings.push_back("Skipped a converted mine because its lane was outside the target layout.");
            continue;
        }
        if (mine.mine_id >= mine_placements.size()) {
            warnings.push_back("Skipped a converted mine because its source placement could not be resolved.");
            continue;
        }

        const auto& source = mine_placements[mine.mine_id];
        BmsPositionSlot slot = source.slot;
        if (mine.sample != source.sample) {
            const double absolute_position = position_map.position_for_sample(mine.sample);
            const auto quantized = quantize_position_to_slot(normalized_chart, absolute_position, kQuantizeSliceCount);
            if (!quantized.has_value()) {
                warnings.push_back("Skipped a converted mine because its position could not be represented in BMS.");
                continue;
            }
            slot = *quantized;
        }

        const std::string channel =
            mine_channel_for_note_channel(layout.lane_channels[static_cast<std::size_t>(mine.lane - 1)]);
        if (channel.empty()) {
            warnings.push_back("Skipped a converted mine because the target lane has no BMS mine channel.");
            continue;
        }
        generated_events.push_back(OutputEvent{slot, channel, source.object_id});
    }

    std::map<int, std::map<std::string, std::vector<OutputEvent>>> generated_by_measure;
    for (const auto& event : generated_events) {
        generated_by_measure[event.slot.measure][event.channel].push_back(event);
    }

    std::vector<chart::BmsMeasureCommand> commands;
    commands.reserve(parsed_chart.commands.size() + generated_by_measure.size() * 2u +
                     layout.lane_channels.size());
    for (const auto& command : parsed_chart.commands) {
        if (is_note_lane_channel(command.channel) || is_mine_lane_channel(command.channel)) {
            continue;
        }
        commands.push_back(command);
    }

    // BMS has no separate lane-layout table. Preserve empty target lanes with
    // zero-only visible-channel declarations so a sparse converted chart still
    // round-trips at its requested key count. Other players ignore the 00 data.
    for (const auto& visible_channel : layout.lane_channels) {
        const bool lane_has_event = std::any_of(
            generated_events.begin(), generated_events.end(),
            [&](const OutputEvent& event) {
                if (event.channel.size() != 2 || visible_channel.size() != 2 ||
                    event.channel[1] != visible_channel[1]) {
                    return false;
                }
                const char visible_player = visible_channel[0];
                return event.channel[0] == visible_player ||
                       (visible_player == '1' &&
                        (event.channel[0] == '5' || event.channel[0] == 'D')) ||
                       (visible_player == '2' &&
                        (event.channel[0] == '6' || event.channel[0] == 'E'));
            });
        if (!lane_has_event) {
            commands.push_back(chart::BmsMeasureCommand{0, visible_channel, "00"});
        }
    }

    for (auto& [measure, channels] : generated_by_measure) {
        for (auto& [channel, events] : channels) {
            std::sort(events.begin(), events.end(), [](const OutputEvent& lhs, const OutputEvent& rhs) {
                if (lhs.slot.absolute_position != rhs.slot.absolute_position) {
                    return lhs.slot.absolute_position < rhs.slot.absolute_position;
                }
                return lhs.token < rhs.token;
            });
            std::string data = build_measure_data(events, warnings);
            if (!data.empty()) {
                commands.push_back(chart::BmsMeasureCommand{measure, channel, std::move(data)});
            }
        }
    }

    std::sort(commands.begin(), commands.end(), [](const chart::BmsMeasureCommand& lhs, const chart::BmsMeasureCommand& rhs) {
        if (lhs.measure != rhs.measure) {
            return lhs.measure < rhs.measure;
        }
        return lhs.channel < rhs.channel;
    });

    std::ostringstream out;
    auto emit_header_value = [&](std::string_view key, std::string_view value) {
        out << '#' << key;
        if (!value.empty()) {
            out << ' ' << value;
        }
        out << '\n';
    };
    auto emit_header_flag = [&](std::string_view key) {
        out << '#' << key << '\n';
    };

    std::vector<std::string> remaining_keys;
    remaining_keys.reserve(parsed_chart.headers.size());
    for (const auto& [key, value] : parsed_chart.headers) {
        if (!is_filtered_header_key(key)) {
            remaining_keys.push_back(key);
        }
    }
    std::sort(remaining_keys.begin(), remaining_keys.end());

    static constexpr std::array<std::string_view, 12> kHeaderOrder = {"TITLE", "SUBTITLE", "ARTIST", "SUBARTIST", "GENRE",
                                                                      "BPM", "PLAYLEVEL", "RANK", "TOTAL", "VOLWAV",
                                                                      "DIFFICULTY", "STAGEFILE"};
    std::set<std::string> emitted_headers;
    for (std::string_view key : kHeaderOrder) {
        auto it = parsed_chart.headers.find(std::string(key));
        if (it != parsed_chart.headers.end() && !is_filtered_header_key(it->first)) {
            emit_header_value(it->first, it->second);
            emitted_headers.insert(it->first);
        }
    }

    if (layout.player_header.has_value()) {
        emit_header_value("PLAYER", layout.player_header.value());
    }
    if (layout.value_header.has_value()) {
        emit_header_value(layout.value_header->first, layout.value_header->second);
    }
    if (layout.flag_header.has_value()) {
        emit_header_flag(layout.flag_header.value());
    }
    if (any_holds && lnobj_token.has_value()) {
        emit_header_value("LNOBJ", lnobj_token.value());
    }
    if (any_charge) {
        emit_header_value("LNMODE", "2");
    }

    for (const auto& key : remaining_keys) {
        if (emitted_headers.find(key) != emitted_headers.end()) {
            continue;
        }
        auto it = parsed_chart.headers.find(key);
        if (it != parsed_chart.headers.end()) {
            emit_header_value(it->first, it->second);
        }
    }

    auto emit_dictionary = [&](std::string_view prefix, const auto& values) {
        std::vector<std::string> keys;
        keys.reserve(values.size());
        for (const auto& [key, value] : values) {
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        for (const auto& key : keys) {
            const auto it = values.find(key);
            if (it != values.end()) {
                out << '#' << prefix << key << ' ' << it->second << '\n';
            }
        }
    };

    auto emit_numeric_dictionary = [&](std::string_view prefix, const auto& values) {
        std::vector<std::string> keys;
        keys.reserve(values.size());
        for (const auto& [key, value] : values) {
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        for (const auto& key : keys) {
            const auto it = values.find(key);
            if (it == values.end()) {
                continue;
            }
            std::ostringstream value_stream;
            value_stream.imbue(std::locale::classic());
            value_stream << std::setprecision(12) << it->second;
            out << '#' << prefix << key << ' ' << value_stream.str() << '\n';
        }
    };

    emit_dictionary("WAV", parsed_chart.wav);
    emit_dictionary("BMP", parsed_chart.bmp);
    emit_numeric_dictionary("BPM", parsed_chart.bpm);
    emit_numeric_dictionary("STOP", parsed_chart.stop);
    emit_numeric_dictionary("SCROLL", parsed_chart.scroll);

    out << '\n';

    for (const auto& command : commands) {
        out << '#' << std::setw(3) << std::setfill('0') << command.measure
            << command.channel << ':' << command.data << '\n';
    }

    return out.str();
}

bool write_text_file(const std::string& path, const std::string& text, std::string& error) {
    std::ofstream stream;
#ifdef _WIN32
    try {
        stream.open(std::filesystem::u8path(path), std::ios::binary);
    } catch (...) {
        stream.open(path, std::ios::binary);
    }
#else
    stream.open(path, std::ios::binary);
#endif
    if (!stream) {
        error = "Failed to open output file: " + path;
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream) {
        error = "Failed to write output file: " + path;
        return false;
    }
    return true;
}

}  // namespace

const std::vector<BmsKeyConverterPreset>& bms_key_converter_presets() {
    return preset_table();
}

bool find_bms_key_converter_preset(std::string_view token, BmsKeyConverterPreset& preset) {
    for (const auto& candidate : preset_table()) {
        if (!preset_matches_token(candidate, token)) {
            continue;
        }
        preset = candidate;
        return true;
    }
    return false;
}

BmsKeyConverterResult convert_bms_chart_file(const BmsKeyConverterOptions& options) {
    BmsKeyConverterResult result;

    if (options.input_path.empty()) {
        result.error = "Input path is required.";
        return result;
    }
    if (options.output_path.empty()) {
        result.error = "Output path is required.";
        return result;
    }
    if (options.sample_rate < 0) {
        result.error = "Sample rate must be positive, or 0 for auto.";
        return result;
    }

    const LayoutDefinition layout = resolve_layout_definition(options.target_lane_count);
    if (layout.lane_count <= 0) {
        result.error = "Unsupported target lane count. Supported values: 1 through 18.";
        return result;
    }

    const auto conversion_algorithm = parse_conversion_algorithm(options.conversion_algorithm);
    if (!conversion_algorithm.has_value()) {
        result.error = "Unsupported conversion algorithm. Supported values: krrcream, nk2.";
        return result;
    }
    chart::BmsParser parser;
    chart::BmsParseResult parsed = parser.parseFile(options.input_path);
    if (!parsed.success()) {
        result.error = "Failed to parse the input BMS file.";
        for (const auto& message : parsed.messages) {
            result.warnings.push_back("line " + std::to_string(message.line) + ": " + message.text);
        }
        return result;
    }

    const int effective_sample_rate =
        resolve_effective_sample_rate(options.input_path, parsed.chart, options.sample_rate, result.warnings);
    result.sample_rate = effective_sample_rate;
    result.sample_rate_auto = options.sample_rate == 0;

    chart::BmsChartNormalizer normalizer;
    chart::BmsNormalizationResult normalization = normalizer.normalize(parsed.chart);
    if (!normalization.success()) {
        result.error = "Failed to normalize the input BMS chart.";
        for (const auto& message : normalization.messages) {
            result.warnings.push_back("measure " + std::to_string(message.measure) + ": " + message.text);
        }
        return result;
    }

    chart::BmsTimelineBuilder timeline_builder;
    chart::BmsTimelineResult timeline = timeline_builder.build(normalization.chart, effective_sample_rate);
    if (!timeline.success()) {
        result.error = "Failed to build a playable timeline from the input BMS chart.";
        for (const auto& message : timeline.messages) {
            result.warnings.push_back("measure " + std::to_string(message.measure) + ": " + message.text);
        }
        return result;
    }

    BmsGameplayBuildResult source_build = build_bms_gameplay_chart(timeline.timeline, parsed.chart, 1.0);
    std::vector<SourceNotePlacement> placements = build_source_note_placements(timeline.timeline, parsed.chart, 1.0);
    std::vector<SourceMinePlacement> mine_placements = build_source_mine_placements(timeline.timeline, 1.0);
    if (placements.size() != source_build.chart.notes.size()) {
        result.error = "Internal note-mapping mismatch while preparing the BMS conversion.";
        return result;
    }
    if (mine_placements.size() != source_build.chart.mines.size()) {
        result.error = "Internal mine-mapping mismatch while preparing the BMS conversion.";
        return result;
    }

    for (std::size_t i = 0; i < source_build.chart.notes.size(); ++i) {
        source_build.chart.notes[i].note_id = i;
    }

    result.source_lane_count = source_build.chart.lane_count;
    result.target_lane_count = layout.lane_count;
    if (result.source_lane_count <= 0) {
        result.error = "The input chart does not contain playable note lanes.";
        return result;
    }

    gameplay::KeyModeConverterOptions converter_options =
        default_converter_options(result.source_lane_count,
                                  layout.lane_count,
                                  options.seed,
                                  normalization.chart.base_bpm,
                                  effective_sample_rate);
    if (options.max_keys > 0) {
        converter_options.max_keys = options.max_keys;
    }
    if (options.min_keys > 0) {
        converter_options.min_keys = options.min_keys;
    }
    converter_options.transform_speed_slot = options.transform_speed_slot;
    converter_options.algorithm = *conversion_algorithm;

    gameplay::KeyModeConverterResult converted =
        gameplay::convert_key_mode_chart(source_build.chart, converter_options);
    if (*conversion_algorithm == gameplay::KeyModeConversionAlgorithm::NK2) {
        result.warnings.push_back(
            "nK2 uses its native 50/50 profile; Krrcream Max/Min/Speed/Seed tuning is not applied.");
    } else if (*conversion_algorithm == gameplay::KeyModeConversionAlgorithm::NK3) {
        result.warnings.push_back(
            "NK3 uses P64 plus the host beam safety solver; the generalized MLP is limited to non-10K -> 10K; "
            "Krrcream Max/Min/Speed/Seed tuning is not applied.");
    }
    result.warnings.insert(result.warnings.end(), converted.warnings.begin(), converted.warnings.end());

    if (!converted.converted) {
        result.error = "The BMS chart was not converted. Check the target lane count and input chart lanes.";
        return result;
    }

    if (source_build.chart.lane_count > 0 && converted.chart.lane_count > 0 &&
        source_build.chart.lane_count != converted.chart.lane_count) {
        for (auto& mine : converted.chart.mines) {
            if (mine.lane <= 0 || mine.lane > source_build.chart.lane_count) {
                continue;
            }
            const double normalized =
                (static_cast<double>(mine.lane) - 0.5) / static_cast<double>(source_build.chart.lane_count);
            mine.lane = std::clamp(
                static_cast<int>(std::floor(normalized * static_cast<double>(converted.chart.lane_count))) + 1,
                1, converted.chart.lane_count);
        }
    }

    result.warnings.push_back("Conversion algorithm: " + conversion_algorithm_name(*conversion_algorithm) + ".");

    const std::string output_text =
        build_bms_text(parsed.chart, normalization.chart, placements, mine_placements, converted.chart, layout,
                       effective_sample_rate, result.warnings);

    std::string write_error;
    if (!write_text_file(options.output_path, output_text, write_error)) {
        result.error = std::move(write_error);
        return result;
    }

    result.success = true;
    result.note_count = converted.chart.notes.size();
    result.hold_count = hold_count(converted.chart);
    return result;
}

}  // namespace tenriff::app
