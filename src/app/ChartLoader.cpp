#include "app/ChartLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "app/BmsGameplayBuilder.h"
#include "app/MenuSongUtils.h"
#include "chart/BmsChartNorm.h"
#include "chart/BmsParser.h"
#include "chart/BmsTimeline.h"
namespace tenriff::app {

namespace {

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool is_bms_extension(std::string_view ext) {
    return ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".pms";
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

std::string normalize_asset_reference(std::string value) {
    value = trim_copy(value);
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            value = value.substr(1, value.size() - 2);
            value = trim_copy(value);
        }
    }
    return value;
}

std::filesystem::path normalize_resolved_path(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path normalized = path;
    if (!normalized.is_absolute()) {
        const fs::path absolute = fs::absolute(normalized, ec);
        if (!ec && !absolute.empty()) {
            normalized = absolute;
        } else {
            ec.clear();
        }
    }

    const fs::path canonical = fs::weakly_canonical(normalized, ec);
    if (!ec && !canonical.empty()) {
        return canonical;
    }
    return normalized.lexically_normal();
}

std::string normalize_path_key(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return to_lower(std::move(value));
}

bool is_audio_extension(std::string_view ext) {
    return ext == ".wav" || ext == ".wave" || ext == ".ogg" || ext == ".mp3";
}

struct AssetLookupIndex {
    bool built = false;
    std::unordered_map<std::string, std::filesystem::path> by_relative;
    std::unordered_map<std::string, std::filesystem::path> by_filename;
    std::vector<std::filesystem::path> audio_files;
};

void build_asset_lookup(const std::filesystem::path& chart_path,
                        AssetLookupIndex& lookup) {
    if (lookup.built) {
        return;
    }
    lookup.built = true;
    lookup.by_relative.clear();
    lookup.by_filename.clear();
    lookup.audio_files.clear();

    namespace fs = std::filesystem;
    const fs::path root = chart_path.parent_path();
    if (root.empty()) {
        return;
    }

    std::error_code ec;
    fs::directory_options options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(root, options, ec);
    fs::recursive_directory_iterator end;
    while (it != end) {
        if (ec) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        const fs::directory_entry& entry = *it;
        if (entry.is_directory(ec)) {
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

        const fs::path full = normalize_resolved_path(entry.path());
        fs::path relative = fs::relative(full, root, ec);
        if (ec || relative.empty()) {
            ec.clear();
            relative = full.lexically_relative(root);
        }
        if (!relative.empty()) {
            const std::string rel_key = normalize_path_key(relative.generic_u8string());
            lookup.by_relative.emplace(rel_key, full);
        }

        const std::string file_key = to_lower(full.filename().u8string());
        lookup.by_filename.emplace(file_key, full);
        if (is_audio_extension(to_lower(full.extension().u8string()))) {
            lookup.audio_files.push_back(full);
        }

        it.increment(ec);
    }
}

std::optional<std::filesystem::path> lookup_asset_path_candidate(const std::filesystem::path& chart_path,
                                                                  const std::filesystem::path& ref_path,
                                                                  AssetLookupIndex& lookup) {
    namespace fs = std::filesystem;
    const fs::path direct = ref_path.is_absolute()
                                ? ref_path.lexically_normal()
                                : (chart_path.parent_path() / ref_path).lexically_normal();
    std::error_code ec;
    if (!direct.empty() && fs::exists(direct, ec) && !ec) {
        return normalize_resolved_path(direct);
    }

    build_asset_lookup(chart_path, lookup);

    const std::string rel_key = normalize_path_key(ref_path.generic_u8string());
    auto rel_it = lookup.by_relative.find(rel_key);
    if (rel_it != lookup.by_relative.end()) {
        return rel_it->second;
    }

    const std::string file_key = to_lower(ref_path.filename().u8string());
    auto file_it = lookup.by_filename.find(file_key);
    if (file_it != lookup.by_filename.end()) {
        return file_it->second;
    }

    return std::nullopt;
}

std::vector<std::filesystem::path> build_asset_reference_candidates(const std::string& reference) {
    namespace fs = std::filesystem;
#ifdef _WIN32
    fs::path ref_path = fs::u8path(reference);
#else
    fs::path ref_path(reference);
#endif

    std::vector<fs::path> candidates;
    std::unordered_set<std::string> seen;
    auto push_candidate = [&](const fs::path& candidate) {
        const std::string key = normalize_path_key(candidate.generic_u8string());
        if (seen.emplace(key).second) {
            candidates.push_back(candidate);
        }
    };

    const std::string ext = to_lower(ref_path.extension().u8string());
    const bool prefer_ogg_for_wav_reference = (ext == ".wav" || ext == ".wave");
    if (!prefer_ogg_for_wav_reference) {
        push_candidate(ref_path);
    }

    auto push_replaced_extension = [&](std::string_view new_ext) {
        fs::path candidate = ref_path;
        if (ext.empty()) {
            candidate += new_ext;
        } else {
            candidate.replace_extension(new_ext);
        }
        push_candidate(candidate);
    };

    if (ext.empty()) {
        push_replaced_extension(".ogg");
        push_replaced_extension(".wav");
        push_replaced_extension(".wave");
        push_replaced_extension(".mp3");
    } else if (ext == ".wav" || ext == ".wave") {
        push_replaced_extension(".ogg");
        push_candidate(ref_path);
        if (ext == ".wav") {
            push_replaced_extension(".wave");
        } else {
            push_replaced_extension(".wav");
        }
        push_replaced_extension(".mp3");
    } else if (is_audio_extension(ext)) {
        push_replaced_extension(".ogg");
        push_replaced_extension(".wav");
        push_replaced_extension(".wave");
        push_replaced_extension(".mp3");
    }

    return candidates;
}

std::optional<std::filesystem::path> resolve_asset_path_with_fallback(const std::filesystem::path& chart_path,
                                                                       const std::string& reference,
                                                                       AssetLookupIndex& lookup) {
    const auto candidates = build_asset_reference_candidates(reference);
    for (const auto& candidate : candidates) {
        auto resolved = lookup_asset_path_candidate(chart_path, candidate, lookup);
        if (resolved.has_value()) {
            return resolved;
        }
    }
    return std::nullopt;
}

enum class BmsKeysoundPolicy {
    Ignore,
    Follow,
    Autoplay,
};

BmsKeysoundPolicy parse_bms_keysound_policy(std::string_view policy) {
    std::string token(policy);
    token = to_lower(std::move(token));
    if (token == "ignore" || token == "off") {
        return BmsKeysoundPolicy::Ignore;
    }
    if (token == "autoplay") {
        return BmsKeysoundPolicy::Autoplay;
    }
    return BmsKeysoundPolicy::Follow;
}

int64_t scale_samples(int64_t samples, double rate) {
    if (rate <= 0.0 || !std::isfinite(rate)) {
        return samples;
    }
    return static_cast<int64_t>(std::llround(static_cast<double>(samples) / rate));
}

void append_bms_messages(std::vector<std::string>& out, const std::vector<chart::BmsParseMessage>& messages) {
    for (const auto& msg : messages) {
        out.push_back(msg.text);
    }
}

void append_norm_messages(std::vector<std::string>& out, const std::vector<chart::BmsNormalizationMessage>& messages) {
    for (const auto& msg : messages) {
        out.push_back(msg.text);
    }
}

void append_timeline_messages(std::vector<std::string>& out, const std::vector<chart::BmsTimelineMessage>& messages) {
    for (const auto& msg : messages) {
        out.push_back(msg.text);
    }
}

std::optional<std::string> resolve_bms_audio_path(const std::filesystem::path& chart_path,
                                                  const chart::BmsChart& parsed_chart,
                                                  std::string_view object_id,
                                                  const char* category,
                                                  AssetLookupIndex& asset_lookup,
                                                  std::unordered_map<std::string, std::optional<std::string>>&
                                                      resolved_wav_cache,
                                                  std::unordered_set<std::string>& warned_missing_wav_id,
                                                  std::unordered_set<std::string>& warned_missing_file_ref,
                                                  std::vector<std::string>& messages) {
    const std::string object_key(object_id);
    auto wav_it = parsed_chart.wav.find(object_key);
    if (wav_it == parsed_chart.wav.end()) {
        if (warned_missing_wav_id.emplace(object_key).second) {
            messages.push_back(std::string(category) + " references undefined #WAV" + object_key + ".");
        }
        return std::nullopt;
    }

    const std::string wav_ref = normalize_asset_reference(wav_it->second);
    if (wav_ref.empty()) {
        return std::nullopt;
    }

    const std::string wav_ref_key = normalize_path_key(wav_ref);
    auto cache_it = resolved_wav_cache.find(wav_ref_key);
    if (cache_it != resolved_wav_cache.end()) {
        return cache_it->second;
    }

    std::optional<std::string> resolved_path;
    auto resolved = resolve_asset_path_with_fallback(chart_path, wav_ref, asset_lookup);
    if (resolved.has_value()) {
        resolved_path = resolved->u8string();
    } else if (warned_missing_file_ref.emplace(wav_ref_key).second) {
        messages.push_back(std::string(category) + " file not found: " + wav_ref);
    }

    resolved_wav_cache.emplace(wav_ref_key, resolved_path);
    return resolved_path;
}

std::optional<std::string> resolve_bms_visual_path(
    const std::filesystem::path& chart_path,
    const chart::BmsChart& parsed_chart,
    std::string_view object_id,
    AssetLookupIndex& asset_lookup,
    std::unordered_map<std::string, std::optional<std::string>>& resolved_bmp_cache,
    std::unordered_set<std::string>& warned_missing_bmp_id,
    std::unordered_set<std::string>& warned_missing_file_ref,
    std::vector<std::string>& messages) {
    const std::string object_key(object_id);
    const auto bmp_it = parsed_chart.bmp.find(object_key);
    if (bmp_it == parsed_chart.bmp.end()) {
        if (warned_missing_bmp_id.emplace(object_key).second) {
            messages.push_back("BGA references undefined #BMP" + object_key + ".");
        }
        return std::nullopt;
    }

    const std::string bmp_ref = normalize_asset_reference(bmp_it->second);
    if (bmp_ref.empty()) {
        return std::nullopt;
    }
    const std::string bmp_ref_key = normalize_path_key(bmp_ref);
    const auto cached = resolved_bmp_cache.find(bmp_ref_key);
    if (cached != resolved_bmp_cache.end()) {
        return cached->second;
    }

#ifdef _WIN32
    const std::filesystem::path ref_path = std::filesystem::u8path(bmp_ref);
#else
    const std::filesystem::path ref_path(bmp_ref);
#endif
    std::optional<std::string> resolved_path;
    if (auto resolved = lookup_asset_path_candidate(chart_path, ref_path, asset_lookup);
        resolved.has_value()) {
        resolved_path = resolved->u8string();
    } else if (warned_missing_file_ref.emplace(bmp_ref_key).second) {
        messages.push_back("BGA file not found: " + bmp_ref);
    }
    resolved_bmp_cache.emplace(bmp_ref_key, resolved_path);
    return resolved_path;
}

}  // namespace

ChartLoadResult ChartLoader::load(const std::string& path,
                                  int sample_rate,
                                  double rate,
                                  std::string_view bms_keysound_policy) const {
    ChartLoadResult result;
    AssetLookupIndex asset_lookup;

#ifdef _WIN32
    std::filesystem::path file_path = std::filesystem::u8path(path);
#else
    std::filesystem::path file_path(path);
#endif
    auto ext = to_lower(file_path.extension().u8string());
    if (!is_bms_extension(ext)) {
        result.error = "Unsupported chart extension.";
        return result;
    }

    chart::BmsParser parser;
    auto parse_result = parser.parseFile(file_path.u8string());
    append_bms_messages(result.messages, parse_result.messages);
    if (!parse_result.success()) {
        result.error = "Failed to parse BMS chart.";
        return result;
    }

    chart::BmsChartNormalizer normalizer;
    auto norm_result = normalizer.normalize(parse_result.chart);
    append_norm_messages(result.messages, norm_result.messages);
    if (!norm_result.success()) {
        result.error = "Failed to normalize BMS chart.";
        return result;
    }

    chart::BmsTimelineBuilder timeline_builder;
    auto timeline_result = timeline_builder.build(norm_result.chart, sample_rate);
    append_timeline_messages(result.messages, timeline_result.messages);
    if (!timeline_result.success()) {
        result.error = "Failed to build BMS timeline.";
        return result;
    }

    auto built_chart = build_bms_gameplay_chart(timeline_result.timeline, parse_result.chart, rate);
    result.chart = std::move(built_chart.chart);
    result.base_bpm = parse_result.chart.base_bpm;
    result.messages.insert(result.messages.end(), built_chart.messages.begin(), built_chart.messages.end());
    const BmsKeysoundPolicy keysound_policy = parse_bms_keysound_policy(bms_keysound_policy);
    std::unordered_map<std::string, std::optional<std::string>> resolved_wav_cache;
    std::unordered_set<std::string> warned_missing_wav_id;
    std::unordered_set<std::string> warned_missing_file_ref;
    std::unordered_map<std::string, std::optional<std::string>> resolved_bmp_cache;
    std::unordered_set<std::string> warned_missing_bmp_id;
    std::unordered_set<std::string> warned_missing_bmp_file_ref;

    for (const auto& scheduled : timeline_result.timeline.events) {
        const bool is_bgm = (scheduled.event.type == chart::BmsNormalizedEventType::Bgm);
        if (!is_bgm) {
            continue;
        }
        auto resolved_path = resolve_bms_audio_path(file_path,
                                                    parse_result.chart,
                                                    scheduled.event.object_id,
                                                    "BGM",
                                                    asset_lookup,
                                                    resolved_wav_cache,
                                                    warned_missing_wav_id,
                                                    warned_missing_file_ref,
                                                    result.messages);
        if (!resolved_path.has_value()) {
            continue;
        }

        gameplay::AudioCueEvent cue;
        cue.start_sample = std::max<int64_t>(0, scale_samples(scheduled.time_samples, rate));
        cue.asset_id = result.chart.intern_audio_asset(resolved_path.value());
        result.chart.audio_cues.push_back(std::move(cue));
    }

    bool has_base_visual_cue = false;
    for (const auto& scheduled : timeline_result.timeline.events) {
        if (scheduled.event.type != chart::BmsNormalizedEventType::Bga ||
            (scheduled.event.channel != "04" && scheduled.event.channel != "07")) {
            continue;
        }
        auto resolved_path = resolve_bms_visual_path(file_path,
                                                     parse_result.chart,
                                                     scheduled.event.object_id,
                                                     asset_lookup,
                                                     resolved_bmp_cache,
                                                     warned_missing_bmp_id,
                                                     warned_missing_bmp_file_ref,
                                                     result.messages);
        if (!resolved_path.has_value()) {
            continue;
        }

        gameplay::VisualCueEvent cue;
        cue.start_sample = std::max<int64_t>(0, scale_samples(scheduled.time_samples, rate));
        cue.asset_id = result.chart.intern_visual_asset(resolved_path.value());
        cue.layer = scheduled.event.channel == "07"
                        ? gameplay::VisualLayer::Overlay
                        : gameplay::VisualLayer::Base;
        has_base_visual_cue = has_base_visual_cue || cue.layer == gameplay::VisualLayer::Base;
        result.chart.visual_cues.push_back(std::move(cue));
    }

    if (!has_base_visual_cue) {
        const std::string fallback_background =
            menu_songs::resolve_bms_background_preview_path(file_path, parse_result.chart);
        if (!fallback_background.empty()) {
            gameplay::VisualCueEvent cue;
            cue.start_sample = 0;
            cue.asset_id = result.chart.intern_visual_asset(fallback_background);
            cue.layer = gameplay::VisualLayer::Base;
            result.chart.visual_cues.push_back(std::move(cue));
        }
    }

    if (keysound_policy != BmsKeysoundPolicy::Ignore) {
        const std::size_t note_count = std::min(result.chart.notes.size(), built_chart.note_object_ids.size());
        for (std::size_t i = 0; i < note_count; ++i) {
            if (built_chart.note_object_ids[i].empty()) {
                continue;
            }
            auto resolved_path = resolve_bms_audio_path(file_path,
                                                        parse_result.chart,
                                                        built_chart.note_object_ids[i],
                                                        "Keysound",
                                                        asset_lookup,
                                                        resolved_wav_cache,
                                                        warned_missing_wav_id,
                                                        warned_missing_file_ref,
                                                        result.messages);
            if (!resolved_path.has_value()) {
                continue;
            }

            if (keysound_policy == BmsKeysoundPolicy::Follow) {
                result.chart.notes[i].audio_asset_id = result.chart.intern_audio_asset(resolved_path.value());
            } else {
                gameplay::AudioCueEvent cue;
                cue.start_sample = result.chart.notes[i].start_sample;
                cue.asset_id = result.chart.intern_audio_asset(resolved_path.value());
                result.chart.audio_cues.push_back(std::move(cue));
            }
        }
    }

    std::stable_sort(result.chart.audio_cues.begin(), result.chart.audio_cues.end(),
                     [&result](const gameplay::AudioCueEvent& lhs, const gameplay::AudioCueEvent& rhs) {
                         if (lhs.start_sample != rhs.start_sample) {
                             return lhs.start_sample < rhs.start_sample;
                         }
                         const std::string* lhs_path = result.chart.audio_asset_path(lhs.asset_id);
                         const std::string* rhs_path = result.chart.audio_asset_path(rhs.asset_id);
                         const std::string_view lhs_view = lhs_path ? std::string_view(*lhs_path) : std::string_view{};
                         const std::string_view rhs_view = rhs_path ? std::string_view(*rhs_path) : std::string_view{};
                         return lhs_view < rhs_view;
                     });
    std::stable_sort(result.chart.visual_cues.begin(), result.chart.visual_cues.end(),
                     [](const gameplay::VisualCueEvent& lhs,
                        const gameplay::VisualCueEvent& rhs) {
                         if (lhs.start_sample != rhs.start_sample) {
                             return lhs.start_sample < rhs.start_sample;
                         }
                         return static_cast<int>(lhs.layer) < static_cast<int>(rhs.layer);
                     });
    result.format = ChartFormat::Bms;
    return result;
}

}  // namespace tenriff::app
