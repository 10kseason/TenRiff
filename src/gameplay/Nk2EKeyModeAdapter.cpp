#include "gameplay/Nk2EKeyModeAdapter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include "nk2e/nk2_convert.hpp"

namespace tenriff::gameplay {
namespace {
int milliseconds(int64_t sample, int rate) {
    const auto value = std::round(static_cast<long double>(sample) * 1000 / rate);
    if (value < (std::numeric_limits<int>::min)() || value > (std::numeric_limits<int>::max)())
        throw std::runtime_error("chart time exceeds NK2 integer millisecond range");
    return static_cast<int>(value);
}

int64_t samples(int time, int rate) {
    return static_cast<int64_t>(std::llround(static_cast<long double>(time) * rate / 1000));
}
}

KeyModeConverterResult convert_key_mode_chart_nk2_e(
    const GameplayChart& chart, const KeyModeConverterOptions& options) {
    KeyModeConverterResult result{chart, false, {}};
    int source_keys = chart.lane_count;
    for (const auto& note : chart.notes) source_keys = std::max(source_keys, note.lane);
    if (chart.notes.empty() || source_keys == options.target_lane_count) return result;
    if (source_keys < 1 || source_keys > 18 || options.target_lane_count < 4 || options.target_lane_count > 16 ||
        options.target_lane_count == 11 || options.target_lane_count == 13 || options.target_lane_count == 15) {
        result.warnings.push_back("NK2 E supports 1-18 source lanes and menu targets 4K-10K/12K/14K/16K.");
        return result;
    }
    const int rate = options.sample_rate > 0 ? options.sample_rate : 44100;
    try {
        keyweaver_e::Chart source;
        source.meta.sourceKeyCount = source_keys;
        source.meta.targetKeyCount = options.target_lane_count;
        source.meta.version = "TenRiff";
        const double bpm = std::isfinite(options.base_bpm) && options.base_bpm > 0 ? options.base_bpm : 180;
        source.timingPoints.push_back({0, 60000.0 / bpm});
        std::map<std::string, std::size_t> originals;
        for (std::size_t i = 0; i < chart.notes.size(); ++i) {
            const auto& note = chart.notes[i];
            if (note.lane < 1 || note.lane > source_keys) throw std::runtime_error("invalid source lane");
            keyweaver_e::Note mapped;
            mapped.id = "tenriff-note[" + std::to_string(i) + "]";
            originals.emplace(mapped.id, i);
            mapped.time = milliseconds(note.start_sample, rate);
            mapped.lane = note.lane - 1;
            mapped.sourceLane = mapped.lane;
            if (note.end_sample && *note.end_sample > note.start_sample) {
                mapped.type = keyweaver_e::NoteType::Hold;
                mapped.endTime = milliseconds(*note.end_sample, rate);
                // Sub-millisecond holds cannot be represented safely by this upstream engine.
                if (*mapped.endTime <= mapped.time) throw std::runtime_error("LN shorter than NK2 time resolution");
            }
            source.notes.push_back(std::move(mapped));
        }
        keyweaver_e::nk2::NK2Options settings;
        settings.sourceKeyCount = source_keys;
        settings.targetKeyCount = options.target_lane_count;
        settings.mode = keyweaver_e::nk2::Mode::Faithful;
        settings.phraseSearch = true;
        settings.independentSafetyAudit = true;
        // E's added-note phase applies only to widening. Reduction keeps phrase
        // placement and original timing; it cannot create room by rolling notes.
        settings.phraseSupport = settings.targetKeyCount > settings.sourceKeyCount;
        settings.phraseDenseLn = settings.phraseSupport;
        settings.phraseSourceLn = settings.phraseSupport;
        const auto converted = keyweaver_e::nk2::convertChart(source, settings);
        if (!converted.report.independentSafety || !converted.report.independentSafety->passed())
            throw std::runtime_error("independent source/LN/collision audit failed");
        if (!converted.report.chartMutated || converted.chart.notes.empty())
            throw std::runtime_error("conversion produced no playable notes");

        auto rebuilt = chart;
        rebuilt.lane_count = options.target_lane_count;
        rebuilt.scratch_lanes.clear();
        rebuilt.lane_group_size = 0;
        rebuilt.notes.clear();
        std::size_t generated_id = chart.notes.size();
        for (const auto& note : converted.chart.notes) {
            NoteEvent mapped;
            const auto original = originals.find(note.id);
            if (original != originals.end()) {
                // Keep source sample precision, keysounds, hold semantics and provenance.
                mapped = chart.notes[original->second];
            } else {
                // Added accents have no new keysound. Copying an anchor's asset would
                // double BMS audio and a copied note ID would restore its old LN shape.
                mapped.note_id = generated_id++;
                mapped.start_sample = samples(note.time, rate);
                if (note.type == keyweaver_e::NoteType::Hold && note.endTime) {
                    mapped.end_sample = samples(*note.endTime, rate);
                    bool covered = false;
                    for (const auto& authored : chart.notes) {
                        if (!authored.end_sample || *authored.end_sample <= authored.start_sample) continue;
                        if (milliseconds(authored.start_sample, rate) > note.time ||
                            milliseconds(*authored.end_sample, rate) < *note.endTime) continue;
                        // ms rounding must not extend a generated LN beyond the exact
                        // authored sample interval used to authorize it.
                        mapped.start_sample = std::max(mapped.start_sample, authored.start_sample);
                        mapped.end_sample = std::min(*mapped.end_sample, *authored.end_sample);
                        mapped.release_required = authored.release_required;
                        covered = *mapped.end_sample > mapped.start_sample;
                        if (covered) break;
                    }
                    if (!covered) throw std::runtime_error("generated LN is outside an authored LN interval");
                }
            }
            mapped.lane = note.lane + 1;
            rebuilt.notes.push_back(std::move(mapped));
        }
        std::stable_sort(rebuilt.notes.begin(), rebuilt.notes.end(), [](const auto& a, const auto& b) {
            if (a.start_sample != b.start_sample) return a.start_sample < b.start_sample;
            if (a.lane != b.lane) return a.lane < b.lane;
            return a.note_id < b.note_id;
        });
        // Recheck at TenRiff's exact sample resolution after restoring source times.
        std::vector<int64_t> ends(options.target_lane_count, (std::numeric_limits<int64_t>::min)());
        for (const auto& note : rebuilt.notes) {
            auto& end = ends.at(static_cast<std::size_t>(note.lane - 1));
            if (note.start_sample <= end) throw std::runtime_error("sample-resolution lane overlap");
            end = note.end_sample.value_or(note.start_sample);
            rebuilt.duration_samples = std::max(rebuilt.duration_samples, end);
        }
        result.chart = std::move(rebuilt);
        result.converted = true;
        result.warnings.push_back("NK2 E Source LN remapped " + std::to_string(source_keys) + "K to " +
            std::to_string(options.target_lane_count) + "K (added=" + std::to_string(converted.report.addedNotes) +
            ", added_LN=" + std::to_string(converted.report.addedHoldNotes) + ", dropped=" +
            std::to_string(converted.report.droppedNotes) + ").");
    } catch (const std::exception& error) {
        result.warnings.push_back(std::string("NK2 E conversion failed: ") + error.what());
    }
    return result;
}
}
