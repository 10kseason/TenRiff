#include "app/BmsEditorPreview.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "app/AudioFileDecoder.h"
#include "app/AudioMixPolicy.h"
#include "app/SongPreviewPlayback.h"
#include "chart/BmsChartNorm.h"
#include "chart/BmsParser.h"
#include "chart/BmsTimeline.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {
namespace {

using chart::BmsNormalizedEvent;
using chart::BmsNormalizedEventType;

bool cancelled(const BmsEditorPreviewCancelFlag& flag) {
    return flag && flag->load(std::memory_order_acquire);
}

std::filesystem::path path_from_utf8(const std::string& value) {
    try { return util::path_from_utf8_lossy(value); }
    catch (...) { return {}; }
}

std::string trim(std::string value) {
    const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) {
        return !is_space(static_cast<unsigned char>(ch));
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) {
        return !is_space(static_cast<unsigned char>(ch));
    }).base(), value.end());
    return value;
}

std::optional<std::filesystem::path> find_asset(const std::filesystem::path& chart_path,
                                                 const std::string& reference) {
    if (chart_path.empty() || reference.empty()) return std::nullopt;
    std::string ref = trim(reference);
    if (ref.size() >= 2 && ((ref.front() == '"' && ref.back() == '"') ||
                            (ref.front() == '\'' && ref.back() == '\''))) {
        ref = ref.substr(1, ref.size() - 2);
    }
    std::filesystem::path wanted = path_from_utf8(ref);
    const auto root = chart_path.parent_path();
    const auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    };
    const auto normalized_key = [&](const std::filesystem::path& value) {
        std::string key = value.generic_u8string();
        std::replace(key.begin(), key.end(), '\\', '/');
        return lower(key);
    };
    std::vector<std::filesystem::path> candidates;
    candidates.push_back(wanted);
    static constexpr std::string_view kAudioExtensions[] = {".ogg", ".wav", ".wave", ".mp3"};
    const std::string extension = lower(wanted.extension().u8string());
    if (extension.empty()) {
        for (const auto ext : kAudioExtensions) {
            auto candidate = wanted;
            candidate += ext;
            candidates.push_back(std::move(candidate));
        }
    } else if (extension == ".ogg" || extension == ".wav" || extension == ".wave" || extension == ".mp3") {
        for (const auto ext : kAudioExtensions) {
            if (extension == ext) continue;
            auto candidate = wanted;
            candidate.replace_extension(ext);
            candidates.push_back(std::move(candidate));
        }
    }
    std::error_code ec;
    for (const auto& candidate : candidates) {
        auto direct = candidate.is_absolute() ? candidate : root / candidate;
        if (std::filesystem::is_regular_file(direct, ec) && !ec) return direct;
        ec.clear();
    }
    ec.clear();
    std::unordered_set<std::string> candidate_names;
    std::unordered_set<std::string> candidate_relatives;
    for (const auto& candidate : candidates) {
        candidate_names.insert(normalized_key(candidate.filename()));
        candidate_relatives.insert(normalized_key(candidate));
    }
    for (std::filesystem::recursive_directory_iterator it(
             root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (it->is_regular_file(ec) && !ec) {
            const auto name = normalized_key(it->path().filename());
            const auto relative = std::filesystem::relative(it->path(), root, ec);
            const auto relative_key = ec ? std::string{} : normalized_key(relative);
            if (candidate_names.count(name) != 0 || candidate_relatives.count(relative_key) != 0)
                return it->path();
        }
        ec.clear();
    }
    return std::nullopt;
}

void fail(BmsEditorPreviewResult& result, std::string text) {
    result.error = std::move(text);
    result.stereo_samples.reset();
}

}  // namespace

BmsEditorPreviewResult build_bms_editor_preview(
    const BmsEditorPreviewRequest& request, BmsEditorPreviewCancelFlag cancel_flag) {
    BmsEditorPreviewResult result;
    result.sample_rate = request.sample_rate;
    if (cancelled(cancel_flag)) { fail(result, "cancelled"); return result; }
    if (request.chart_text.empty()) { fail(result, "BMS editor buffer is empty."); return result; }
    if (request.sample_rate < 8'000 || request.sample_rate > 192'000) {
        fail(result, "Preview sample rate is outside 8000..192000 Hz."); return result;
    }
    // Editor previews are intentionally short so a malformed or huge chart
    // cannot turn an interactive audition into a large allocation.
    const int safe_seconds = std::clamp(request.max_duration_seconds, 1, 45);

    chart::BmsParser parser;
    chart::BmsParserOptions options;
    options.tolerant = true;
    auto parsed = parser.parse(request.chart_text, options);
    if (!parsed.success()) { fail(result, "Failed to parse edited BMS text."); return result; }
    if (cancelled(cancel_flag)) { fail(result, "cancelled"); return result; }

    chart::BmsChartNormalizer normalizer;
    auto normalized = normalizer.normalize(parsed.chart);
    if (!normalized.success()) { fail(result, "Failed to normalize edited BMS text."); return result; }

    // A marker event lets the production timeline builder account for every
    // BPM change and STOP before the cursor without duplicating its timing math.
    const int cursor_measure = std::max(0, request.cursor_measure);
    const double fraction = std::clamp(request.cursor_fraction, 0.0, 0.999999999);
    const auto& measures = normalized.chart.measures;
    const double cursor_position = cursor_measure < static_cast<int>(measures.size())
        ? measures[cursor_measure].start + fraction * measures[cursor_measure].length
        : (measures.empty() ? 0.0 : measures.back().end()) +
          std::max(0, cursor_measure - static_cast<int>(measures.size())) + fraction;
    BmsNormalizedEvent marker;
    marker.type = BmsNormalizedEventType::Unknown;
    marker.measure = cursor_measure;
    marker.position = cursor_position;
    marker.intra_measure = fraction;
    marker.object_id = "__TENRIFF_EDITOR_CURSOR__";
    normalized.chart.events.push_back(marker);
    std::stable_sort(normalized.chart.events.begin(), normalized.chart.events.end(),
                     [](const BmsNormalizedEvent& lhs, const BmsNormalizedEvent& rhs) {
                         return lhs.position < rhs.position;
                     });
    chart::BmsTimelineBuilder timeline_builder;
    auto timeline = timeline_builder.build(normalized.chart, request.sample_rate);
    if (!timeline.success()) { fail(result, "Failed to build edited BMS timeline."); return result; }
    result.chart_duration_samples = timeline.timeline.duration_samples;
    for (const auto& scheduled : timeline.timeline.events) {
        if (scheduled.event.object_id == marker.object_id) {
            result.cursor_sample = std::max<int64_t>(0, scheduled.time_samples);
            break;
        }
    }
    const bool full_chart = request.play_to_end && !request.keysound_only;
    const int64_t window_start = result.cursor_sample;
    // Bound malformed-chart allocations explicitly instead of silently truncating
    // a full-song audition. Only one decoded asset is retained at a time.
    constexpr int64_t kMaxFrames = (512LL * 1024 * 1024) / (2 * sizeof(float));
    int64_t window_frames = full_chart
        ? std::max<int64_t>(1, timeline.timeline.duration_samples - window_start)
        : static_cast<int64_t>(request.sample_rate) * safe_seconds;
    if (window_frames > kMaxFrames) {
        fail(result, "Full editor preview exceeds the 512 MiB audio buffer limit."); return result;
    }
    std::vector<float> output(static_cast<std::size_t>(window_frames) * 2u, 0.0f);
    std::unordered_map<std::string, std::vector<int64_t>> cues;
    if (request.keysound_only) {
        if (request.keysound_id.empty() || !parsed.chart.wav.count(request.keysound_id)) {
            fail(result, "Requested keysound id is not declared in the BMS."); return result;
        }
        cues[request.keysound_id].push_back(window_start);
    } else {
        std::unordered_set<std::string> active_ln_channels;
        const auto lnobj = parsed.chart.headers.find("LNOBJ");
        for (const auto& scheduled : timeline.timeline.events) {
            const auto& event = scheduled.event;
            if (event.type != BmsNormalizedEventType::Bgm && event.type != BmsNormalizedEventType::Note)
                continue;
            // Dedicated LN pairs and LNOBJ tails release a note; they are not
            // extra key presses and must not retrigger the keysound.
            if (event.type == BmsNormalizedEventType::Note) {
                if (lnobj != parsed.chart.headers.end() && event.object_id == lnobj->second) continue;
                if (!event.channel.empty() && (event.channel[0] == '5' || event.channel[0] == '6')) {
                    if (active_ln_channels.erase(event.channel) != 0) continue;
                    active_ln_channels.insert(event.channel);
                }
            }
            if (!full_chart && scheduled.time_samples >= window_start + window_frames) continue;
            if (parsed.chart.wav.count(event.object_id)) cues[event.object_id].push_back(scheduled.time_samples);
        }
    }
    const auto chart_path = path_from_utf8(request.source_chart_path);
    std::size_t mixed = 0;
    std::size_t missing = 0;
    std::string first_error;
    for (const auto& [id, starts] : cues) {
        if (cancelled(cancel_flag)) { fail(result, "cancelled"); return result; }
        const auto asset = find_asset(chart_path, parsed.chart.wav.at(id));
        std::vector<float> clip;
        std::string error;
        const int64_t earliest = *std::min_element(starts.begin(), starts.end());
        const int64_t decode_frames = full_chart ? kMaxFrames : window_start + window_frames - earliest;
        if (decode_frames > kMaxFrames) {
            fail(result, "Editor preview seek exceeds the 512 MiB audio buffer limit."); return result;
        }
        if (!asset || !decode_audio_file_stereo_resampled(asset->u8string(), request.sample_rate, clip,
                                                           &error, static_cast<std::size_t>(decode_frames))) {
            ++missing;
            const std::string detail = asset ? asset->u8string() + ": " + error
                : "audio asset was not found: " + parsed.chart.wav.at(id);
            if (first_error.empty()) first_error = detail;
            if (request.keysound_only) {
                fail(result, "Requested keysound asset could not be decoded: " + detail); return result;
            }
            continue;
        }
        if (full_chart) {
            const int64_t last = *std::max_element(starts.begin(), starts.end());
            const int64_t tail = last + static_cast<int64_t>(clip.size() / 2u) - window_start;
            if (tail >= kMaxFrames) {
                fail(result, "Full editor preview exceeds the 512 MiB audio buffer limit."); return result;
            }
            if (tail > window_frames) {
                window_frames = tail;
                output.resize(static_cast<std::size_t>(window_frames) * 2u, 0.0f);
            }
        }
        for (const int64_t start : starts) {
            if (cancelled(cancel_flag)) { fail(result, "cancelled"); return result; }
            if (mix_song_preview_clip_into_window(clip, start, window_start, output) != 0u) ++mixed;
        }
    }
    if (cancelled(cancel_flag)) { fail(result, "cancelled"); return result; }
    if (mixed == 0 && missing != 0) {
        fail(result, "No chart audio could be decoded: " + first_error); return result;
    }
    for (float& sample : output) sample = soft_limit_audio_sample(sample);
    if (missing != 0) result.warning = std::to_string(missing) + " BMS audio assets unavailable: " + first_error;
    result.stereo_samples = std::make_shared<const std::vector<float>>(std::move(output));
    return result;
}

}  // namespace tenriff::app
