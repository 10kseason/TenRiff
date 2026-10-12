#include "gameplay/TimingAnalysisJson.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace tenriff::gameplay {
namespace {

using config::JsonArray;
using config::JsonObject;
using config::JsonValue;

// JSON's numeric storage is double. Never accept integers that it cannot retain
// exactly, or timing magnitudes large enough to overflow downstream UI math.
constexpr uint64_t kMaxExactInteger = (uint64_t{1} << 53) - 1;
constexpr double kMaxDeltaMs = 1'000'000'000.0;
constexpr std::array<const char*, kTimingEventKindCount> kKindNames{
    "tap", "ln_head", "ln_release", "hold_failure", "auto_completion", "empty_poor"};

const JsonValue* find(const JsonObject& object, std::string_view key) {
    const auto it = object.find(std::string(key));
    return it == object.end() ? nullptr : &it->second;
}

bool number(const JsonObject& object, std::string_view key, double& out) {
    const auto* value = find(object, key);
    if (!value || !value->is_number()) return false;
    out = value->as_number();
    return std::isfinite(out);
}

bool counter(const JsonValue& value, uint64_t& out) {
    if (!value.is_number()) return false;
    const double parsed = value.as_number();
    if (!std::isfinite(parsed) || parsed < 0.0 || parsed > static_cast<double>(kMaxExactInteger) ||
        std::floor(parsed) != parsed) return false;
    out = static_cast<uint64_t>(parsed);
    return true;
}

bool counter(const JsonObject& object, std::string_view key, uint64_t& out) {
    const auto* value = find(object, key);
    return value && counter(*value, out);
}

bool nearly_equal(double a, double b) {
    return std::abs(a - b) <= 1e-10 * std::max({1.0, std::abs(a), std::abs(b)});
}

bool valid_distribution(const TimingDistribution& d) {
    if (d.event_count > kMaxExactInteger || d.measured_count > d.event_count ||
        d.unmeasured_miss_count > d.event_count - d.measured_count ||
        d.early_count > d.measured_count || d.late_count > d.measured_count ||
        d.center_count > d.measured_count ||
        d.early_count + d.late_count + d.center_count != d.measured_count ||
        d.fast_count > d.early_count || d.slow_count > d.late_count ||
        !std::isfinite(d.mean_delta_ms) || !std::isfinite(d.mean_abs_delta_ms) ||
        !std::isfinite(d.m2_delta_ms) || std::abs(d.mean_delta_ms) > kMaxDeltaMs ||
        d.mean_abs_delta_ms < 0.0 || d.mean_abs_delta_ms > kMaxDeltaMs || d.m2_delta_ms < 0.0 ||
        d.m2_delta_ms > static_cast<double>(d.measured_count) * kMaxDeltaMs * kMaxDeltaMs * 4.0 ||
        (d.mean_abs_delta_ms < std::abs(d.mean_delta_ms) &&
         !nearly_equal(d.mean_abs_delta_ms, std::abs(d.mean_delta_ms)))) return false;
    uint64_t sum = 0;
    for (const auto count : d.histogram) {
        if (count > d.measured_count || count > d.measured_count - sum) return false;
        sum += count;
    }
    if (sum != d.measured_count) return false;
    if (d.measured_count == 0 &&
        (d.mean_delta_ms != 0.0 || d.mean_abs_delta_ms != 0.0 || d.m2_delta_ms != 0.0)) return false;
    return d.measured_count != 1 || d.m2_delta_ms == 0.0;
}

void add_counts(TimingDistribution& sum, const TimingDistribution& item) {
    sum.event_count += item.event_count;
    sum.measured_count += item.measured_count;
    sum.unmeasured_miss_count += item.unmeasured_miss_count;
    sum.early_count += item.early_count;
    sum.late_count += item.late_count;
    sum.center_count += item.center_count;
    sum.fast_count += item.fast_count;
    sum.slow_count += item.slow_count;
    for (std::size_t i = 0; i < sum.histogram.size(); ++i) sum.histogram[i] += item.histogram[i];
}

bool same_counts(const TimingDistribution& a, const TimingDistribution& b) {
    return a.event_count == b.event_count && a.measured_count == b.measured_count &&
           a.unmeasured_miss_count == b.unmeasured_miss_count && a.early_count == b.early_count &&
           a.late_count == b.late_count && a.center_count == b.center_count &&
           a.fast_count == b.fast_count && a.slow_count == b.slow_count && a.histogram == b.histogram;
}

bool valid_analysis(const TimingAnalysis& a) {
    if (!a.available || a.lane_count <= 0 || a.lane_count > static_cast<int>(a.by_lane.size()) ||
        a.sample_rate <= 0 || a.duration_samples < 0 ||
        static_cast<uint64_t>(a.duration_samples) > kMaxExactInteger || !valid_distribution(a.total)) return false;
    TimingDistribution lane_sum;
    for (std::size_t i = 0; i < a.by_lane.size(); ++i) {
        if (!valid_distribution(a.by_lane[i]) ||
            (i >= static_cast<std::size_t>(a.lane_count) && a.by_lane[i].event_count != 0)) return false;
        add_counts(lane_sum, a.by_lane[i]);
    }
    TimingDistribution kind_sum;
    for (std::size_t i = 0; i < a.by_kind.size(); ++i) {
        const auto& kind = a.by_kind[i];
        if (!valid_distribution(kind)) return false;
        const bool automatic = i == static_cast<std::size_t>(TimingEventKind::AutoCompletion) ||
                               i == static_cast<std::size_t>(TimingEventKind::EmptyPoor);
        if (automatic ? (kind.measured_count != 0 || kind.unmeasured_miss_count != 0)
                      : (kind.measured_count + kind.unmeasured_miss_count != kind.event_count)) return false;
        add_counts(kind_sum, kind);
    }
    if (!same_counts(a.total, lane_sum) || !same_counts(a.total, kind_sum)) return false;
    uint64_t segment_events = 0;
    for (const auto& bucket : a.segments) {
        if (bucket.event_count > a.total.event_count ||
            bucket.event_count > a.total.event_count - segment_events ||
            !std::isfinite(bucket.accuracy_points) || !std::isfinite(bucket.accuracy_weight) ||
            bucket.accuracy_points < 0.0 || bucket.accuracy_weight < 0.0 ||
            bucket.accuracy_points > bucket.accuracy_weight ||
            bucket.accuracy_weight > static_cast<double>(bucket.event_count) ||
            (bucket.event_count > 0 && bucket.accuracy_weight == 0.0)) return false;
        segment_events += bucket.event_count;
    }
    return segment_events == a.total.event_count;
}

JsonValue distribution_json(const TimingDistribution& d) {
    JsonObject object;
    object.emplace("event_count", JsonValue{static_cast<double>(d.event_count)});
    object.emplace("measured_count", JsonValue{static_cast<double>(d.measured_count)});
    object.emplace("unmeasured_miss_count", JsonValue{static_cast<double>(d.unmeasured_miss_count)});
    object.emplace("early_count", JsonValue{static_cast<double>(d.early_count)});
    object.emplace("late_count", JsonValue{static_cast<double>(d.late_count)});
    object.emplace("center_count", JsonValue{static_cast<double>(d.center_count)});
    object.emplace("fast_count", JsonValue{static_cast<double>(d.fast_count)});
    object.emplace("slow_count", JsonValue{static_cast<double>(d.slow_count)});
    object.emplace("mean_delta_ms", JsonValue{d.mean_delta_ms});
    object.emplace("mean_abs_delta_ms", JsonValue{d.mean_abs_delta_ms});
    object.emplace("m2_delta_ms", JsonValue{d.m2_delta_ms});
    JsonArray histogram;
    histogram.reserve(d.histogram.size());
    for (const auto count : d.histogram) histogram.emplace_back(static_cast<double>(count));
    object.emplace("histogram", JsonValue{std::move(histogram)});
    return JsonValue{std::move(object)};
}

bool parse_distribution(const JsonValue* value, TimingDistribution& d) {
    const auto* object = value ? value->as_object() : nullptr;
    if (!object || !counter(*object, "event_count", d.event_count) ||
        !counter(*object, "measured_count", d.measured_count) ||
        !counter(*object, "unmeasured_miss_count", d.unmeasured_miss_count) ||
        !counter(*object, "early_count", d.early_count) || !counter(*object, "late_count", d.late_count) ||
        !counter(*object, "center_count", d.center_count) || !counter(*object, "fast_count", d.fast_count) ||
        !counter(*object, "slow_count", d.slow_count) || !number(*object, "mean_delta_ms", d.mean_delta_ms) ||
        !number(*object, "mean_abs_delta_ms", d.mean_abs_delta_ms) || !number(*object, "m2_delta_ms", d.m2_delta_ms)) return false;
    const auto* histogram_value = find(*object, "histogram");
    const auto* histogram = histogram_value ? histogram_value->as_array() : nullptr;
    if (!histogram || histogram->size() != d.histogram.size()) return false;
    for (std::size_t i = 0; i < histogram->size(); ++i) {
        if (!counter((*histogram)[i], d.histogram[i])) return false;
    }
    return true;
}

}  // namespace

config::JsonValue build_timing_analysis_json(const TimingAnalysis& analysis) {
    if (!valid_analysis(analysis)) return JsonValue{};
    JsonObject object;
    object.emplace("version", JsonValue{1.0});
    object.emplace("lane_count", JsonValue{static_cast<double>(analysis.lane_count)});
    object.emplace("sample_rate", JsonValue{static_cast<double>(analysis.sample_rate)});
    object.emplace("duration_samples", JsonValue{static_cast<double>(analysis.duration_samples)});
    object.emplace("histogram_min_ms", JsonValue{kTimingHistogramMinMs});
    object.emplace("histogram_bin_width_ms", JsonValue{kTimingHistogramBinWidthMs});
    object.emplace("total", distribution_json(analysis.total));
    JsonArray lanes;
    for (int i = 0; i < analysis.lane_count; ++i) lanes.push_back(distribution_json(analysis.by_lane[i]));
    object.emplace("by_lane", JsonValue{std::move(lanes)});
    JsonObject kinds;
    for (std::size_t i = 0; i < kKindNames.size(); ++i) kinds.emplace(kKindNames[i], distribution_json(analysis.by_kind[i]));
    object.emplace("by_kind", JsonValue{std::move(kinds)});
    JsonArray segments;
    for (const auto& bucket : analysis.segments) {
        JsonObject item;
        item.emplace("event_count", JsonValue{static_cast<double>(bucket.event_count)});
        item.emplace("accuracy_points", JsonValue{bucket.accuracy_points});
        item.emplace("accuracy_weight", JsonValue{bucket.accuracy_weight});
        segments.emplace_back(std::move(item));
    }
    object.emplace("segments", JsonValue{std::move(segments)});
    return JsonValue{std::move(object)};
}

TimingAnalysis parse_timing_analysis_json(const config::JsonValue* value) {
    TimingAnalysis analysis;
    const auto* object = value ? value->as_object() : nullptr;
    uint64_t version = 0, lanes = 0, rate = 0, duration = 0;
    double histogram_min = 0.0, histogram_width = 0.0;
    if (!object || !counter(*object, "version", version) || version != 1 ||
        !counter(*object, "lane_count", lanes) || lanes == 0 || lanes > analysis.by_lane.size() ||
        !counter(*object, "sample_rate", rate) || rate == 0 || rate > std::numeric_limits<int>::max() ||
        !counter(*object, "duration_samples", duration) ||
        !number(*object, "histogram_min_ms", histogram_min) || histogram_min != kTimingHistogramMinMs ||
        !number(*object, "histogram_bin_width_ms", histogram_width) || histogram_width != kTimingHistogramBinWidthMs ||
        !parse_distribution(find(*object, "total"), analysis.total)) return {};
    analysis.lane_count = static_cast<int>(lanes);
    analysis.sample_rate = static_cast<int>(rate);
    analysis.duration_samples = static_cast<int64_t>(duration);
    const auto* lane_value = find(*object, "by_lane");
    const auto* lane_array = lane_value ? lane_value->as_array() : nullptr;
    if (!lane_array || lane_array->size() != lanes) return {};
    for (std::size_t i = 0; i < lane_array->size(); ++i) {
        if (!parse_distribution(&(*lane_array)[i], analysis.by_lane[i])) return {};
    }
    const auto* kind_value = find(*object, "by_kind");
    const auto* kinds = kind_value ? kind_value->as_object() : nullptr;
    if (!kinds || kinds->size() != kKindNames.size()) return {};
    for (std::size_t i = 0; i < kKindNames.size(); ++i) {
        if (!parse_distribution(find(*kinds, kKindNames[i]), analysis.by_kind[i])) return {};
    }
    const auto* segment_value = find(*object, "segments");
    const auto* segments = segment_value ? segment_value->as_array() : nullptr;
    if (!segments || segments->size() != analysis.segments.size()) return {};
    for (std::size_t i = 0; i < segments->size(); ++i) {
        const auto* item = (*segments)[i].as_object();
        auto& bucket = analysis.segments[i];
        if (!item || !counter(*item, "event_count", bucket.event_count) ||
            !number(*item, "accuracy_points", bucket.accuracy_points) ||
            !number(*item, "accuracy_weight", bucket.accuracy_weight)) return {};
    }
    analysis.available = true;
    return valid_analysis(analysis) ? analysis : TimingAnalysis{};
}

}  // namespace tenriff::gameplay
