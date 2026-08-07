#include "keyweaver/osu_pipeline.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>

namespace keyweaver {

namespace fs = std::filesystem;

namespace {

constexpr const char* kSourceIdPrefix = "osu-";

std::string trim(const std::string& value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string sourceId(std::size_t index) {
    return std::string(kSourceIdPrefix) + std::to_string(index);
}

std::optional<std::size_t> sourceIndexFromId(const std::string& id, std::size_t sourceCount) {
    if (id.rfind(kSourceIdPrefix, 0) != 0) {
        return std::nullopt;
    }
    std::size_t value = 0;
    const std::size_t start = std::char_traits<char>::length(kSourceIdPrefix);
    if (start >= id.size()) {
        return std::nullopt;
    }
    for (std::size_t cursor = start; cursor < id.size(); ++cursor) {
        const char ch = id[cursor];
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        value = value * 10u + static_cast<std::size_t>(ch - '0');
    }
    return value < sourceCount ? std::optional<std::size_t>(value) : std::nullopt;
}

// osu! strips only the characters Windows forbids when it names a difficulty
// file, so keep spacing intact - the caller trims when building a new name.
std::string sanitizeForFilename(const std::string& value) {
    static const std::string invalid = "\\/:*?\"<>|";
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        if (invalid.find(ch) != std::string::npos) {
            continue;
        }
        if (static_cast<unsigned char>(ch) < 0x20) {
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

bool tagsContain(const std::string& tags, const std::string& token) {
    std::istringstream stream(tags);
    std::string word;
    while (stream >> word) {
        if (word == token) {
            return true;
        }
    }
    return false;
}

std::string readWholeFile(const fs::path& path, bool& ok) {
    std::ifstream stream(extendedPath(path), std::ios::binary);
    if (!stream) {
        ok = false;
        return {};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    ok = true;
    return buffer.str();
}

bool writeWholeFile(const fs::path& path, const std::string& bytes) {
    std::ofstream stream(extendedPath(path), std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
}

// Between two notes fighting for the same lane, give up the one that carries
// the least of the original chart.
bool prefersDroppingLater(osu::Origin earlier, osu::Origin later) {
    if (later != earlier) {
        return later > earlier;  // Generated > Shifted > Source
    }
    return true;  // same provenance: keep the earlier note
}

// Guarantees a chart osu! can read *and* a human can hit: one note per
// (time, lane), no hold running into the next note, and no same-lane pair
// closer than minGapMs. nK2 rolls unplaceable notes 16ms off their beat, which
// is legal but reads as a stacked note in the editor.
int repairLaneConflicts(std::vector<osu::HitObject>& objects, int keyCount, int minGapMs) {
    std::map<int, std::vector<std::size_t>> byColumn;
    for (std::size_t index = 0; index < objects.size(); ++index) {
        byColumn[osu::xToColumn(objects[index].x, keyCount)].push_back(index);
    }

    std::vector<bool> dropped(objects.size(), false);
    int repaired = 0;
    for (auto& [column, indices] : byColumn) {
        std::stable_sort(indices.begin(), indices.end(), [&](std::size_t lhs, std::size_t rhs) {
            return objects[lhs].time < objects[rhs].time;
        });

        std::optional<std::size_t> previous;
        for (const std::size_t index : indices) {
            if (!previous.has_value()) {
                previous = index;
                continue;
            }
            osu::HitObject& earlier = objects[*previous];
            osu::HitObject& later = objects[index];
            const bool earlierIsHold = (earlier.type & osu::kHoldFlag) != 0;
            const int earlierEnd =
                earlierIsHold ? earlier.endTime.value_or(earlier.time) : earlier.time;

            if (later.time - earlierEnd >= minGapMs) {
                previous = index;
                continue;
            }

            // A hold that merely runs long can be shortened instead of dropped,
            // as long as that leaves a hold worth playing.
            if (earlierIsHold && later.time - earlier.time > minGapMs) {
                earlier.endTime = later.time - minGapMs;
                ++repaired;
                previous = index;
                continue;
            }

            if (prefersDroppingLater(earlier.origin, later.origin)) {
                dropped[index] = true;
            } else {
                dropped[*previous] = true;
                previous = index;
            }
            ++repaired;
        }
    }

    if (repaired > 0) {
        std::vector<osu::HitObject> kept;
        kept.reserve(objects.size());
        for (std::size_t index = 0; index < objects.size(); ++index) {
            if (!dropped[index]) {
                kept.push_back(std::move(objects[index]));
            }
        }
        objects = std::move(kept);
    }
    return repaired;
}

// Fewer lanes hold fewer simultaneous notes. Handing the engine a chart that
// still carries 7-key chords means it must either invent jacks or drop whatever
// it cannot place, and it does both. Thinning each chord to the key ratio first
// keeps the rhythm skeleton intact and leaves the target field with room.
int thinChordsForDownscale(keyconv::Chart& chart,
                           int sourceKeyCount,
                           int targetKeyCount,
                           double strength,
                           int sameTimeEpsilonMs) {
    if (targetKeyCount >= sourceKeyCount || sourceKeyCount <= 0 || strength <= 0.0) {
        return 0;
    }
    const double keyRatio = static_cast<double>(targetKeyCount) / static_cast<double>(sourceKeyCount);
    const double keep = 1.0 - std::min(1.0, strength) * (1.0 - keyRatio);

    std::vector<keyconv::Note> kept;
    kept.reserve(chart.notes.size());
    std::size_t index = 0;
    int removed = 0;
    while (index < chart.notes.size()) {
        std::size_t end = index + 1;
        while (end < chart.notes.size() &&
               std::abs(chart.notes[end].time - chart.notes[index].time) <= sameTimeEpsilonMs) {
            ++end;
        }
        const std::size_t chord = end - index;
        const std::size_t allowed = std::max<std::size_t>(
            1, static_cast<std::size_t>(std::ceil(static_cast<double>(chord) * keep)));
        if (allowed >= chord) {
            for (std::size_t i = index; i < end; ++i) {
                kept.push_back(chart.notes[i]);
            }
        } else {
            // Spread the survivors across the chord's lanes so the shape of the
            // chord survives instead of collapsing to one side.
            std::vector<std::size_t> order(chord);
            for (std::size_t i = 0; i < chord; ++i) {
                order[i] = index + i;
            }
            std::stable_sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
                return chart.notes[lhs].lane < chart.notes[rhs].lane;
            });
            std::vector<bool> take(chord, false);
            for (std::size_t slot = 0; slot < allowed; ++slot) {
                const std::size_t pick =
                    static_cast<std::size_t>((slot * chord + chord / 2) / allowed);
                take[std::min(pick, chord - 1)] = true;
            }
            for (std::size_t i = 0; i < chord; ++i) {
                if (take[i]) {
                    kept.push_back(chart.notes[order[i]]);
                } else {
                    ++removed;
                }
            }
        }
        index = end;
    }

    std::stable_sort(kept.begin(), kept.end(),
                     [](const keyconv::Note& lhs, const keyconv::Note& rhs) {
                         if (lhs.time != rhs.time) {
                             return lhs.time < rhs.time;
                         }
                         return lhs.lane < rhs.lane;
                     });
    chart.notes = std::move(kept);
    return removed;
}

}  // namespace

fs::path extendedPath(const fs::path& path) {
#ifdef _WIN32
    // std::filesystem enumerates a few characters past MAX_PATH, but opening a
    // file or descending a directory still goes through CreateFileW, which caps
    // at 260 characters without the extended-length prefix. osu! beatmap names
    // blow past that routinely, so normalise and prefix every absolute path.
    std::error_code error;
    const fs::path absolute = path.is_absolute() ? path : fs::absolute(path, error);
    if (error) {
        return path;
    }
    std::wstring native = absolute.lexically_normal().native();
    if (native.empty() || native.rfind(L"\\\\?\\", 0) == 0) {
        return path;
    }
    std::replace(native.begin(), native.end(), L'/', L'\\');
    // "\\server\share" has to become "\\?\UNC\server\share".
    if (native.rfind(L"\\\\", 0) == 0) {
        return fs::path(L"\\\\?\\UNC" + native.substr(1));
    }
    return fs::path(L"\\\\?\\" + native);
#else
    return path;
#endif
}

keyconv::Chart chartFromOsu(const osu::File& file, int keyCount) {
    keyconv::Chart chart;
    chart.meta.format = "osu";
    chart.meta.mode = file.mode;
    chart.meta.sourceKeyCount = keyCount;
    chart.meta.version = file.version;
    chart.meta.title = file.title;
    chart.meta.artist = file.artist;
    chart.meta.creator = file.creator;

    if (const osu::Section* timing = file.findSection("TimingPoints")) {
        for (const auto& line : timing->lines) {
            const std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed.rfind("//", 0) == 0) {
                continue;
            }
            std::vector<std::string> fields;
            std::size_t cursor = 0;
            while (true) {
                const std::size_t comma = trimmed.find(',', cursor);
                if (comma == std::string::npos) {
                    fields.push_back(trimmed.substr(cursor));
                    break;
                }
                fields.push_back(trimmed.substr(cursor, comma - cursor));
                cursor = comma + 1;
            }
            if (fields.size() < 2) {
                continue;
            }

            keyconv::TimingPoint point;
            point.rawLine = trimmed;
            try {
                point.time = static_cast<int>(std::stod(fields[0]));
                point.beatLength = std::stod(fields[1]);
            } catch (...) {
                continue;
            }
            if (fields.size() > 6) {
                try {
                    point.uninherited = std::stoi(trim(fields[6])) != 0;
                } catch (...) {
                }
            }
            if (!point.uninherited.has_value()) {
                point.uninherited = point.beatLength > 0.0;
            }
            chart.timingPoints.push_back(std::move(point));
        }
        std::stable_sort(chart.timingPoints.begin(),
                         chart.timingPoints.end(),
                         [](const keyconv::TimingPoint& lhs, const keyconv::TimingPoint& rhs) {
                             return lhs.time < rhs.time;
                         });
    }

    chart.notes.reserve(file.hitObjects.size());
    for (std::size_t index = 0; index < file.hitObjects.size(); ++index) {
        const osu::HitObject& object = file.hitObjects[index];
        keyconv::Note note;
        note.id = sourceId(index);
        note.time = object.time;
        note.lane = osu::xToColumn(object.x, keyCount);
        note.sourceLane = note.lane;
        note.raw = object.rawLine;
        if ((object.type & osu::kHoldFlag) != 0 && object.endTime.has_value() &&
            *object.endTime > object.time) {
            note.type = keyconv::NoteType::Hold;
            note.endTime = object.endTime;
        }
        chart.notes.push_back(std::move(note));
    }

    std::stable_sort(chart.notes.begin(),
                     chart.notes.end(),
                     [](const keyconv::Note& lhs, const keyconv::Note& rhs) {
                         if (lhs.time != rhs.time) {
                             return lhs.time < rhs.time;
                         }
                         return lhs.lane < rhs.lane;
                     });
    return chart;
}

osu::File osuFromChart(osu::File base,
                       const keyconv::Chart& chart,
                       const std::vector<osu::HitObject>& sourceObjects,
                       int targetKeyCount,
                       const std::string& newVersion,
                       int minSameLaneGapMs,
                       int& repaired) {
    std::vector<osu::HitObject> objects;
    objects.reserve(chart.notes.size());
    for (const auto& note : chart.notes) {
        if (note.lane < 0 || note.lane >= targetKeyCount) {
            continue;
        }
        osu::HitObject object;
        object.x = osu::columnToX(note.lane, targetKeyCount);
        object.y = osu::kManiaY;
        object.time = note.time;
        if (const auto index = sourceIndexFromId(note.id, sourceObjects.size())) {
            object.hitSound = sourceObjects[*index].hitSound;
            object.hitSample = sourceObjects[*index].hitSample;
            object.origin = note.time == sourceObjects[*index].time ? osu::Origin::Source
                                                                   : osu::Origin::Shifted;
        } else {
            object.origin = osu::Origin::Generated;
            // nK2 support note: inherit the timing point's sample set instead of
            // borrowing a hit sound that was authored for a different lane.
            object.hitSound = 0;
            object.hitSample = "0:0:0:0:";
        }
        if (note.type == keyconv::NoteType::Hold && note.endTime.has_value() &&
            *note.endTime > note.time) {
            object.type = osu::kHoldFlag;
            object.endTime = note.endTime;
        } else {
            object.type = 1;
        }
        objects.push_back(std::move(object));
    }

    std::stable_sort(objects.begin(), objects.end(), [](const osu::HitObject& lhs, const osu::HitObject& rhs) {
        if (lhs.time != rhs.time) {
            return lhs.time < rhs.time;
        }
        return lhs.x < rhs.x;
    });
    repaired = repairLaneConflicts(objects, targetKeyCount, minSameLaneGapMs);

    base.keyCount = targetKeyCount;
    base.version = newVersion;
    base.setField("Difficulty", "CircleSize", std::to_string(targetKeyCount));
    base.setField("Metadata", "Version", newVersion);
    base.setField("Metadata", "BeatmapID", "0");

    const std::string tags = base.getField("Metadata", "Tags").value_or("");
    if (!tagsContain(tags, kConvertedTag)) {
        base.setField("Metadata", "Tags", tags.empty() ? kConvertedTag : tags + " " + kConvertedTag);
    }
    // osu! only honours the 8K special (7+1) layout when this is on; a plain
    // relane must not inherit it from the source difficulty.
    if (base.getField("General", "SpecialStyle").has_value()) {
        base.setField("General", "SpecialStyle", "0");
    }

    osu::Section* hitSection = base.findSection("HitObjects");
    if (hitSection == nullptr) {
        base.sections.push_back(osu::Section{"HitObjects", {}});
        hitSection = &base.sections.back();
    }
    hitSection->lines.clear();
    hitSection->lines.reserve(objects.size());
    for (const auto& object : objects) {
        hitSection->lines.push_back(osu::formatHitObject(object));
    }

    base.hitObjects = std::move(objects);
    return base;
}

std::string convertedVersionName(const std::string& version, const ConvertOptions& options) {
    const std::string suffix =
        options.versionSuffix.empty() ? std::to_string(options.targetKeyCount) + "K" : options.versionSuffix;
    if (version.empty()) {
        return suffix;
    }
    if (version.size() >= suffix.size() &&
        version.compare(version.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return version;
    }
    return version + " " + suffix;
}

fs::path outputPathFor(const fs::path& input, const std::string& newVersion) {
    using Char = fs::path::value_type;
    using Str = fs::path::string_type;
    constexpr Char kOpen = static_cast<Char>('[');
    constexpr Char kClose = static_cast<Char>(']');
    constexpr Char kSpace = static_cast<Char>(' ');

    // Difficulty names arrive as UTF-8 bytes from the file. Stay in the path's
    // native encoding the whole way: building a path out of a narrow string
    // makes Windows decode it as ANSI, which throws on most non-ASCII names.
    const Str version = fs::u8path(trim(sanitizeForFilename(newVersion))).native();
    const Str stem = input.stem().native();
    const Str extension = fs::path(".osu").native();

    // Beatmap files are named "Artist - Title (Creator) [Version]". Replace that
    // trailing group instead of appending, or a difficulty called "[FULL LN]Twist"
    // grows another bracket pair on every pass. Match by nesting depth so inner
    // brackets inside the difficulty name do not end the scan early.
    if (!stem.empty() && stem.back() == kClose) {
        int depth = 0;
        for (std::size_t cursor = stem.size(); cursor-- > 0;) {
            if (stem[cursor] == kClose) {
                ++depth;
            } else if (stem[cursor] == kOpen) {
                --depth;
                if (depth == 0) {
                    return input.parent_path() /
                           (stem.substr(0, cursor) + kOpen + version + kClose + extension);
                }
            }
        }
    }
    return input.parent_path() / (stem + kSpace + kOpen + version + kClose + extension);
}

ConvertOutcome convertOsuFile(const fs::path& input, const ConvertOptions& options) {
    ConvertOutcome outcome;

    bool readOk = false;
    const std::string bytes = readWholeFile(input, readOk);
    if (!readOk) {
        outcome.status = ConvertStatus::Failed;
        outcome.detail = "file could not be read";
        return outcome;
    }

    auto parsed = osu::parseFile(bytes);
    if (!parsed.ok) {
        outcome.status = ConvertStatus::Failed;
        outcome.detail = parsed.error;
        return outcome;
    }

    osu::File& file = parsed.file;
    outcome.sourceKeyCount = file.keyCount;
    outcome.sourceNotes = static_cast<int>(file.hitObjects.size());

    if (file.mode != osu::kManiaMode) {
        outcome.status = ConvertStatus::SkippedNotMania;
        outcome.detail = "Mode:" + std::to_string(file.mode) + " (mania only)";
        return outcome;
    }
    if (tagsContain(file.getField("Metadata", "Tags").value_or(""), kConvertedTag)) {
        outcome.status = ConvertStatus::SkippedAlreadyConverted;
        outcome.detail = "already produced by this tool";
        return outcome;
    }
    if (file.keyCount <= 0 || file.keyCount > keyconv::nk2::kMaxSupportedKeyCount) {
        outcome.status = ConvertStatus::SkippedKeyCount;
        outcome.detail = "unsupported CircleSize " + std::to_string(file.keyCount);
        return outcome;
    }
    if (options.sourceKeyCount > 0 && file.keyCount != options.sourceKeyCount) {
        outcome.status = ConvertStatus::SkippedKeyCount;
        outcome.detail = std::to_string(file.keyCount) + "K";
        return outcome;
    }
    if (std::find(options.excludedSourceKeyCounts.begin(),
                  options.excludedSourceKeyCounts.end(),
                  file.keyCount) != options.excludedSourceKeyCounts.end()) {
        outcome.status = ConvertStatus::SkippedKeyCount;
        outcome.detail = std::to_string(file.keyCount) + "K (제외됨)";
        return outcome;
    }
    if (file.keyCount == options.targetKeyCount && options.mode != keyconv::nk2::Mode::Transform) {
        outcome.status = ConvertStatus::SkippedKeyCount;
        outcome.detail = "already " + std::to_string(options.targetKeyCount) + "K";
        return outcome;
    }
    if (file.hitObjects.empty()) {
        outcome.status = ConvertStatus::SkippedKeyCount;
        outcome.detail = "no hit objects";
        return outcome;
    }

    const std::string newVersion = convertedVersionName(file.version, options);
    const fs::path output = outputPathFor(input, newVersion);
    outcome.outputPath = output;
    if (!options.overwrite && fs::exists(extendedPath(output))) {
        outcome.status = ConvertStatus::SkippedOutputExists;
        const auto utf8 = output.filename().u8string();
        outcome.detail.assign(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        return outcome;
    }

    keyconv::Chart source = chartFromOsu(file, file.keyCount);
    outcome.thinnedNotes = thinChordsForDownscale(source,
                                                  file.keyCount,
                                                  options.targetKeyCount,
                                                  options.downscaleThin,
                                                  options.sameTimeEpsilonMs);

    keyconv::nk2::NK2Options nk2Options;
    nk2Options.sourceKeyCount = file.keyCount;
    nk2Options.targetKeyCount = options.targetKeyCount;
    nk2Options.mode = options.mode;
    nk2Options.superSymmetry = options.superSymmetry;
    nk2Options.nativeWeight = options.nativeWeight;
    nk2Options.remixWeight = options.remixWeight;
    nk2Options.layoutWeights = options.layoutWeights;
    nk2Options.sameTimeEpsilonMs = options.sameTimeEpsilonMs;
    nk2Options.supportBudgetRatio = options.supportBudgetRatio;
    nk2Options.supportJackWindowMs = options.supportJackWindowMs;
    nk2Options.supportSameSourceGapMs = options.supportSameSourceGapMs;
    nk2Options.supportDensityReferenceNps = options.supportDensityReferenceNps;
    nk2Options.lnSpread = options.lnSpread;
    nk2Options.lnFill = options.lnFill;
    nk2Options.anchorBias = options.anchorBias;
    nk2Options.gapLaneBoost = options.gapLaneBoost;

    const auto converted = keyconv::nk2::convertChart(source, nk2Options);
    outcome.report = converted.report;
    outcome.warnings = converted.report.warnings;
    outcome.addedNotes = converted.report.addedNotes;
    outcome.droppedNotes = converted.report.droppedNotes;

    if (!converted.report.chartMutated || converted.report.noOp || converted.chart.notes.empty()) {
        outcome.status = ConvertStatus::Failed;
        outcome.detail = converted.report.noOpReason.empty() ? "nK2 produced no notes"
                                                             : converted.report.noOpReason;
        return outcome;
    }

    // nK2 drops notes it cannot place under its safety gates. Recover which
    // source rows vanished so the caller can point at them.
    // Note ids index file.hitObjects, which chartFromOsu never reorders.
    std::vector<bool> survived(file.hitObjects.size(), false);
    for (const auto& note : converted.chart.notes) {
        if (const auto index = sourceIndexFromId(note.id, file.hitObjects.size())) {
            survived[*index] = true;
        }
    }
    for (std::size_t index = 0; index < file.hitObjects.size(); ++index) {
        if (!survived[index]) {
            outcome.droppedNoteTimes.push_back(file.hitObjects[index].time);
        }
    }
    std::sort(outcome.droppedNoteTimes.begin(), outcome.droppedNoteTimes.end());

    int repaired = 0;
    const osu::File result =
        osuFromChart(file, converted.chart, file.hitObjects, options.targetKeyCount, newVersion,
                     options.minSameLaneGapMs, repaired);
    outcome.repairedNotes = repaired;
    outcome.outputNotes = static_cast<int>(result.hitObjects.size());

    // Measured on the written chart, so it reflects the repair pass too.
    outcome.laneDistribution.assign(static_cast<std::size_t>(options.targetKeyCount), 0);
    for (const auto& object : result.hitObjects) {
        const int column = osu::xToColumn(object.x, options.targetKeyCount);
        ++outcome.laneDistribution[static_cast<std::size_t>(column)];
    }

    if (!options.dryRun && !writeWholeFile(output, osu::serializeFile(result))) {
        outcome.status = ConvertStatus::Failed;
        outcome.detail = "output could not be written";
        return outcome;
    }

    outcome.status = ConvertStatus::Converted;
    return outcome;
}

}  // namespace keyweaver
