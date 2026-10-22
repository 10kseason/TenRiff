#include "safety_audit.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace keyweaver_e::nk2 {
namespace {

bool isGeneratedId(std::string_view id) {
    return id.starts_with("gen:") || id.starts_with("nk2-ln-") ||
           id.starts_with("nk2-mirror-") || id.starts_with("nk2-beat-");
}

bool isValidHold(const Note& note) {
    return note.type == NoteType::Hold && note.endTime.has_value() &&
           *note.endTime > note.time;
}

bool hasChangedHoldDuration(const Note& original, const Note& converted) {
    if (original.type != converted.type) {
        return true;
    }
    if (original.type != NoteType::Hold) {
        return false;
    }
    if (!original.endTime.has_value() || !converted.endTime.has_value()) {
        return original.endTime != converted.endTime;
    }
    const auto oldDuration = static_cast<long long>(*original.endTime) - original.time;
    const auto newDuration = static_cast<long long>(*converted.endTime) - converted.time;
    return oldDuration != newDuration;
}

struct AuditedNote {
    const Note* output = nullptr;
    const Note* source = nullptr;
};

bool preservesSourceRepeat(const AuditedNote& first,
                           const AuditedNote& second,
                           int jackWindowMs) {
    if (first.source == nullptr || second.source == nullptr) {
        return false;
    }
    if (first.source->sourceLane.value_or(first.source->lane) !=
        second.source->sourceLane.value_or(second.source->lane)) {
        return false;
    }
    const auto firstTime = static_cast<long long>(first.source->time);
    const auto secondTime = static_cast<long long>(second.source->time);
    const auto gap = firstTime < secondTime ? secondTime - firstTime : firstTime - secondTime;
    return gap > 0 && gap <= jackWindowMs;
}

}  // namespace

bool SafetyAudit::passed() const {
    return invalidLanes == 0 && invalidHolds == 0 && sameTimeCollisions == 0 &&
           longNoteConflicts == 0 && createdLaneRepeats == 0 &&
           changedHoldDurations == 0 && unknownSourceNotes == 0 &&
           (!sourceLnCoverageRequired || generatedHoldsOutsideSourceLn == 0);
}

SafetyAudit auditSafety(const Chart& source,
                        const Chart& output,
                        int targetKeyCount,
                        int jackWindowMs,
                        bool requireSourceLnCoverage) {
    SafetyAudit audit;
    audit.sourceLnCoverageRequired = requireSourceLnCoverage;
    std::unordered_map<std::string, const Note*> originals;
    originals.reserve(source.notes.size());
    std::vector<std::pair<int, int>> sourceHoldCoverage;
    for (const auto& note : source.notes) {
        if (isValidHold(note)) {
            sourceHoldCoverage.emplace_back(note.time, *note.endTime);
        }
        if (note.id.empty()) {
            continue;
        }
        const auto [entry, inserted] = originals.emplace(note.id, &note);
        if (!inserted) {
            // An ambiguous ID cannot establish provenance for either object.
            entry->second = nullptr;
        }
    }
    std::sort(sourceHoldCoverage.begin(), sourceHoldCoverage.end());
    for (std::size_t index = 1; index < sourceHoldCoverage.size(); ++index) {
        sourceHoldCoverage[index].second =
            std::max(sourceHoldCoverage[index].second, sourceHoldCoverage[index - 1].second);
    }

    std::set<std::string> retainedIds;
    std::map<int, std::vector<AuditedNote>> lanes;
    if (targetKeyCount <= 0 && output.notes.empty()) {
        ++audit.invalidLanes;
    }
    for (const auto& note : output.notes) {
        const Note* original = nullptr;
        const auto found = originals.find(note.id);
        if (found != originals.end() && found->second != nullptr &&
            retainedIds.insert(note.id).second) {
            original = found->second;
            ++audit.retainedSourceNotes;
            if (note.time != original->time) {
                ++audit.retimedSourceNotes;
            }
            if (hasChangedHoldDuration(*original, note)) {
                ++audit.changedHoldDurations;
            }
        } else if (found == originals.end() && isGeneratedId(note.id)) {
            ++audit.addedNotes;
            if (isValidHold(note)) {
                // The maximum tail among source LNs starting before this head
                // belongs to one actual LN; it never joins adjacent intervals.
                auto covering = std::upper_bound(sourceHoldCoverage.begin(), sourceHoldCoverage.end(),
                    note.time, [](int head, const auto& interval) { return head < interval.first; });
                if (covering == sourceHoldCoverage.begin() || (--covering)->second < *note.endTime) {
                    ++audit.generatedHoldsOutsideSourceLn;
                }
            }
        } else {
            // Includes duplicate copies of an original note, not only unknown IDs.
            ++audit.unknownSourceNotes;
        }

        if (note.type == NoteType::Hold && !isValidHold(note)) {
            ++audit.invalidHolds;
        }
        if (note.lane < 0 || note.lane >= targetKeyCount) {
            ++audit.invalidLanes;
            continue;
        }
        lanes[note.lane].push_back({&note, original});
    }
    audit.droppedSourceNotes = static_cast<int>(source.notes.size()) - audit.retainedSourceNotes;

    for (auto& [lane, notes] : lanes) {
        (void)lane;
        std::stable_sort(notes.begin(), notes.end(), [](const AuditedNote& a, const AuditedNote& b) {
            if (a.output->time != b.output->time) {
                return a.output->time < b.output->time;
            }
            return a.output->id < b.output->id;
        });

        // Sweep groups of equal timestamps: simultaneous heads are counted as
        // collisions, while only earlier heads contribute body/tail conflicts.
        std::multiset<int> activeHoldEnds;
        for (std::size_t begin = 0; begin < notes.size();) {
            const int time = notes[begin].output->time;
            std::size_t end = begin + 1;
            while (end < notes.size() && notes[end].output->time == time) {
                ++end;
            }
            audit.sameTimeCollisions += static_cast<int>(end - begin - 1);
            activeHoldEnds.erase(activeHoldEnds.begin(), activeHoldEnds.lower_bound(time));
            for (std::size_t index = begin; index < end; ++index) {
                audit.longNoteConflicts += static_cast<int>(activeHoldEnds.size());
            }
            for (std::size_t index = begin; index < end; ++index) {
                const auto& note = *notes[index].output;
                if (isValidHold(note)) {
                    activeHoldEnds.insert(*note.endTime);
                }
            }
            begin = end;
        }

        for (std::size_t index = 1; index < notes.size(); ++index) {
            const auto& first = notes[index - 1];
            const auto& second = notes[index];
            const auto gap = static_cast<long long>(second.output->time) - first.output->time;
            if (gap <= 0 || gap > jackWindowMs) {
                continue;
            }
            if (preservesSourceRepeat(first, second, jackWindowMs)) {
                ++audit.preservedLaneRepeats;
            } else {
                ++audit.createdLaneRepeats;
            }
        }
    }
    return audit;
}

}  // namespace keyweaver_e::nk2
