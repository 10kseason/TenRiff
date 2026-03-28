#include "app/OsuHitsoundResolver.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_set>

namespace tenriff::app {

namespace {

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
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
            value = trim_copy(std::string_view(value).substr(1, value.size() - 2));
        }
    }
    return value;
}

int sanitize_sample_set(int value) {
    if (value < 0 || value > 3) {
        return 0;
    }
    return value;
}

int sample_set_or_default(int value, int fallback) {
    value = sanitize_sample_set(value);
    if (value != 0) {
        return value;
    }
    fallback = sanitize_sample_set(fallback);
    return fallback == 0 ? 1 : fallback;
}

std::string sample_set_name(int value) {
    switch (sample_set_or_default(value, 1)) {
        case 2: return "soft";
        case 3: return "drum";
        case 1:
        default: return "normal";
    }
}

std::string hit_sound_name_from_bit(int bit) {
    switch (bit) {
        case 0: return "normal";
        case 1: return "whistle";
        case 2: return "finish";
        case 3: return "clap";
        default: return {};
    }
}

const chart::OsuManiaTimingPoint* find_effective_timing_point(const chart::OsuManiaChart& chart, int64_t time_ms) {
    const chart::OsuManiaTimingPoint* active = nullptr;
    for (const auto& point : chart.timing_points) {
        if (point.time_ms > static_cast<double>(time_ms)) {
            break;
        }
        active = &point;
    }
    return active;
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

std::vector<std::filesystem::path> build_reference_candidates(const std::string& reference) {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;
    const std::string normalized = normalize_asset_reference(reference);
    if (normalized.empty()) {
        return candidates;
    }
#ifdef _WIN32
    fs::path ref_path = fs::u8path(normalized);
#else
    fs::path ref_path(normalized);
#endif
    candidates.push_back(ref_path);
    if (!ref_path.has_extension()) {
        auto push_with_suffix = [&](std::string_view suffix) {
            fs::path candidate = ref_path;
            candidate += suffix;
            candidates.push_back(std::move(candidate));
        };
        push_with_suffix(".wav");
        push_with_suffix(".ogg");
        push_with_suffix(".wave");
        push_with_suffix(".mp3");
    } else {
        const std::string ext = to_lower_ascii(ref_path.extension().u8string());
        if (ext == ".wav") {
            fs::path ogg = ref_path;
            ogg.replace_extension(".ogg");
            candidates.push_back(ogg);
            fs::path wave = ref_path;
            wave.replace_extension(".wave");
            candidates.push_back(wave);
            fs::path mp3 = ref_path;
            mp3.replace_extension(".mp3");
            candidates.push_back(mp3);
        }
    }
    return candidates;
}

std::optional<std::filesystem::path> resolve_local_audio_path(const std::filesystem::path& chart_path,
                                                              const std::string& reference) {
    namespace fs = std::filesystem;
    const fs::path base_dir = chart_path.parent_path();
    for (const auto& candidate : build_reference_candidates(reference)) {
        const fs::path full = candidate.is_absolute() ? candidate : (base_dir / candidate);
        std::error_code ec;
        if (fs::exists(full, ec) && fs::is_regular_file(full, ec)) {
            return normalize_resolved_path(full);
        }
    }
    return std::nullopt;
}

void append_asset_path(OsuResolvedNoteHitsound& out, const std::string& path) {
    if (path.empty()) {
        return;
    }
    for (std::size_t i = 0; i < out.asset_count; ++i) {
        if (out.asset_paths[i] == path) {
            return;
        }
    }
    if (out.asset_count >= out.asset_paths.size()) {
        return;
    }
    out.asset_paths[out.asset_count++] = path;
}

}  // namespace

OsuHitsoundResolveResult resolve_osu_mania_hitsounds(const std::filesystem::path& chart_path,
                                                     const chart::OsuManiaChart& chart) {
    OsuHitsoundResolveResult result;
    result.note_hitsounds.resize(chart.notes.size());
    std::unordered_set<std::string> warned_missing;

    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        const auto& note = chart.notes[i];
        auto& resolved = result.note_hitsounds[i];

        const auto* timing = find_effective_timing_point(chart, note.start_time_ms);
        const int timing_sample_set =
            sample_set_or_default(timing ? timing->sample_set : 0, chart.general_sample_set);
        const int effective_normal_set =
            sample_set_or_default(note.normal_set, timing_sample_set);
        const int effective_addition_set =
            sample_set_or_default(note.addition_set, effective_normal_set);
        const int effective_index = [&]() {
            const int value = (note.sample_index != 0) ? note.sample_index : (timing ? timing->sample_index : 0);
            return value <= 0 ? 1 : value;
        }();
        const int effective_volume = [&]() {
            const int value = (note.sample_volume != 0) ? note.sample_volume : (timing ? timing->volume : 100);
            return std::clamp(value, 1, 100);
        }();
        resolved.gain = static_cast<float>(effective_volume) / 100.0f;

        const std::string custom_filename = normalize_asset_reference(note.custom_filename);
        if (!custom_filename.empty()) {
            if (auto path = resolve_local_audio_path(chart_path, custom_filename); path.has_value()) {
                append_asset_path(resolved, path->u8string());
            } else if (warned_missing.emplace(custom_filename).second) {
                result.messages.push_back("osu!mania hitsound file not found: " + custom_filename);
            }
            continue;
        }

        int hit_mask = note.hit_sound & 0xF;
        if (hit_mask == 0) {
            hit_mask = 1;
        }

        for (int bit = 0; bit < 4; ++bit) {
            if ((hit_mask & (1 << bit)) == 0) {
                continue;
            }
            const bool is_normal = (bit == 0);
            const std::string sample_set =
                sample_set_name(is_normal ? effective_normal_set : effective_addition_set);
            const std::string hit_sound = hit_sound_name_from_bit(bit);
            std::string filename = sample_set + "-hit" + hit_sound;
            if (effective_index > 1) {
                filename += std::to_string(effective_index);
            }
            filename += ".wav";

            if (auto path = resolve_local_audio_path(chart_path, filename); path.has_value()) {
                append_asset_path(resolved, path->u8string());
            }
        }
    }

    return result;
}

}  // namespace tenriff::app
