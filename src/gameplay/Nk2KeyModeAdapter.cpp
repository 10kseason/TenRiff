#include "gameplay/Nk2KeyModeAdapter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "nk2/nk2_convert.hpp"

namespace tenriff::gameplay {

namespace {

constexpr const char *kSourceIdPrefix = "tenriff-note[";

int resolve_lane_count(const GameplayChart &chart) {
  int lane_count = chart.lane_count;
  for (const auto &note : chart.notes) {
    lane_count = std::max(lane_count, note.lane);
  }
  return std::max(0, lane_count);
}

int resolve_sample_rate(int sample_rate) {
  return sample_rate > 0 ? sample_rate : 44100;
}

double resolve_base_bpm(double bpm) {
  return std::isfinite(bpm) && bpm > 0.0 ? bpm : 180.0;
}

int samples_to_ms(int64_t samples, int sample_rate) {
  const long double milliseconds = static_cast<long double>(samples) * 1000.0L /
                                   static_cast<long double>(sample_rate);
  const long double rounded = std::round(milliseconds);
  return static_cast<int>(std::clamp<long double>(
      rounded, static_cast<long double>((std::numeric_limits<int>::min)()),
      static_cast<long double>((std::numeric_limits<int>::max)())));
}

int64_t ms_to_samples(int milliseconds, int sample_rate) {
  return static_cast<int64_t>(
      std::llround(static_cast<long double>(milliseconds) *
                   static_cast<long double>(sample_rate) / 1000.0L));
}

std::string source_id(std::size_t note_index) {
  return std::string(kSourceIdPrefix) + std::to_string(note_index) + "]";
}

std::optional<std::size_t> source_index_from_id(const std::string &id,
                                                std::size_t source_count) {
  const std::size_t prefix = id.find(kSourceIdPrefix);
  if (prefix == std::string::npos) {
    return std::nullopt;
  }

  const std::size_t number_start =
      prefix + std::char_traits<char>::length(kSourceIdPrefix);
  const std::size_t number_end = id.find(']', number_start);
  if (number_end == std::string::npos || number_end == number_start) {
    return std::nullopt;
  }

  std::size_t value = 0;
  for (std::size_t cursor = number_start; cursor < number_end; ++cursor) {
    const char ch = id[cursor];
    if (ch < '0' || ch > '9') {
      return std::nullopt;
    }
    const std::size_t digit = static_cast<std::size_t>(ch - '0');
    if (value > ((std::numeric_limits<std::size_t>::max)() - digit) / 10u) {
      return std::nullopt;
    }
    value = value * 10u + digit;
  }
  return value < source_count ? std::optional<std::size_t>(value)
                              : std::nullopt;
}

keyconv::Chart to_nk2_chart(const GameplayChart &source, int source_lane_count,
                            int target_lane_count, int sample_rate,
                            double base_bpm) {
  keyconv::Chart converted;
  converted.meta.sourceKeyCount = source_lane_count;
  converted.meta.targetKeyCount = target_lane_count;
  converted.meta.version = std::to_string(source_lane_count) + "K";
  converted.timingPoints.push_back(keyconv::TimingPoint{0, 60000.0 / base_bpm});
  converted.notes.reserve(source.notes.size());

  for (std::size_t index = 0; index < source.notes.size(); ++index) {
    const auto &note = source.notes[index];
    if (note.lane <= 0 || note.lane > source_lane_count) {
      continue;
    }

    keyconv::Note mapped;
    mapped.id = source_id(index);
    mapped.time = samples_to_ms(note.start_sample, sample_rate);
    mapped.lane = note.lane - 1;
    mapped.sourceLane = mapped.lane;
    if (note.end_sample.has_value() && *note.end_sample > note.start_sample) {
      mapped.type = keyconv::NoteType::Hold;
      mapped.endTime = samples_to_ms(*note.end_sample, sample_rate);
    }
    converted.notes.push_back(std::move(mapped));
  }
  return converted;
}

int64_t mapped_sample_time(int nk2_time, int64_t source_time, int sample_rate) {
  return nk2_time == samples_to_ms(source_time, sample_rate)
             ? source_time
             : ms_to_samples(nk2_time, sample_rate);
}

} // namespace

KeyModeConverterResult
convert_key_mode_chart_nk2(const GameplayChart &chart,
                           const KeyModeConverterOptions &options) {
  KeyModeConverterResult result;
  result.chart = chart;

  const int source_lane_count = resolve_lane_count(chart);
  // Runtime key-count reduction must only relane source notes. nK2 may roll
  // overflowing chord notes by a few milliseconds, which makes the rendered
  // chart and realtime judgement disagree with the source beat.
  const bool preserve_source_timing =
      options.target_lane_count > 0 && options.target_lane_count < source_lane_count;
  if (source_lane_count <= 0 || chart.notes.empty() ||
      options.target_lane_count <= 0 ||
      options.target_lane_count == source_lane_count) {
    return result;
  }
  if (source_lane_count > keyconv::nk2::kMaxSupportedKeyCount ||
      options.target_lane_count > keyconv::nk2::kMaxSupportedKeyCount) {
    result.warnings.push_back("nK2 supports key counts from 1K through 18K.");
    return result;
  }

  const int sample_rate = resolve_sample_rate(options.sample_rate);
  const double base_bpm = resolve_base_bpm(options.base_bpm);
  const keyconv::Chart nk2_source =
      to_nk2_chart(chart, source_lane_count, options.target_lane_count,
                   sample_rate, base_bpm);

  keyconv::nk2::NK2Options nk2_options;
  nk2_options.sourceKeyCount = source_lane_count;
  nk2_options.targetKeyCount = options.target_lane_count;
  nk2_options.mode = keyconv::nk2::Mode::Native;
  nk2_options.nativeWeight = 0.5;
  nk2_options.remixWeight = 0.5;

  const keyconv::nk2::NK2ConversionResult converted =
      keyconv::nk2::convertChart(nk2_source, nk2_options);
  for (const auto &warning : converted.report.warnings) {
    result.warnings.push_back("nK2: " + warning);
  }
  if (!converted.report.chartMutated || converted.report.noOp ||
      converted.chart.notes.empty()) {
    if (!converted.report.noOpReason.empty()) {
      result.warnings.push_back("nK2 did not convert the chart: " +
                                converted.report.noOpReason + ".");
    }
    return result;
  }

  GameplayChart rebuilt = chart;
  rebuilt.lane_count = options.target_lane_count;
  rebuilt.scratch_lanes.clear();
  rebuilt.lane_group_size = 0;
  rebuilt.notes.clear();
  rebuilt.notes.reserve(converted.chart.notes.size());

  std::size_t unresolved_source_notes = 0;
  for (const auto &nk2_note : converted.chart.notes) {
    const auto source_index =
        source_index_from_id(nk2_note.id, chart.notes.size());
    if (!source_index.has_value() || nk2_note.lane < 0 ||
        nk2_note.lane >= options.target_lane_count) {
      ++unresolved_source_notes;
      continue;
    }

    const NoteEvent &source = chart.notes[*source_index];
    NoteEvent mapped = source;
    mapped.lane = nk2_note.lane + 1;
    mapped.start_sample =
        preserve_source_timing
            ? source.start_sample
            : mapped_sample_time(nk2_note.time, source.start_sample, sample_rate);

    if (nk2_note.type == keyconv::NoteType::Hold &&
        nk2_note.endTime.has_value() && *nk2_note.endTime > nk2_note.time) {
      const int64_t source_end =
          source.end_sample.value_or(source.start_sample);
      mapped.end_sample =
          preserve_source_timing
              ? source_end
              : mapped_sample_time(*nk2_note.endTime, source_end, sample_rate);
      mapped.release_required = source.release_required;
    } else {
      mapped.end_sample.reset();
      mapped.release_required = false;
    }
    rebuilt.duration_samples =
        std::max(rebuilt.duration_samples,
                 mapped.end_sample.value_or(mapped.start_sample));
    rebuilt.notes.push_back(std::move(mapped));
  }

  std::stable_sort(rebuilt.notes.begin(), rebuilt.notes.end(),
                   [](const NoteEvent &lhs, const NoteEvent &rhs) {
                     if (lhs.start_sample != rhs.start_sample) {
                       return lhs.start_sample < rhs.start_sample;
                     }
                     if (lhs.lane != rhs.lane) {
                       return lhs.lane < rhs.lane;
                     }
                     return lhs.note_id < rhs.note_id;
                   });

  if (rebuilt.notes.empty()) {
    result.warnings.push_back(
        "nK2 produced no TenRiff-compatible playable notes.");
    return result;
  }
  if (unresolved_source_notes > 0) {
    result.warnings.push_back(
        "nK2 skipped " + std::to_string(unresolved_source_notes) +
        " note(s) whose source metadata could not be resolved.");
  }

  result.chart = std::move(rebuilt);
  result.converted = true;
  result.warnings.push_back(
      "nK2 remapped " + std::to_string(source_lane_count) + "K to " +
      std::to_string(options.target_lane_count) +
      "K using the native 50/50 profile " +
      "(added=" + std::to_string(converted.report.addedNotes) +
      ", dropped=" + std::to_string(converted.report.droppedNotes) + ").");
  return result;
}

} // namespace tenriff::gameplay
