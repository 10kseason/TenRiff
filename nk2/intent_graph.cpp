#include "nk2/intent_graph.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace keyconv::nk2 {

namespace {

struct SliceInfo {
    int time = 0;
    std::vector<const Note*> notes;
};

int sourceLaneOf(const Note& note) {
    return note.sourceLane.value_or(note.lane);
}

std::vector<const Note*> sortedNoteRefs(const Chart& chart) {
    std::vector<const Note*> notes;
    notes.reserve(chart.notes.size());
    for (const auto& note : chart.notes) {
        notes.push_back(&note);
    }
    std::stable_sort(notes.begin(), notes.end(), [](const Note* lhs, const Note* rhs) {
        if (lhs->time != rhs->time) {
            return lhs->time < rhs->time;
        }
        const int lhsLane = sourceLaneOf(*lhs);
        const int rhsLane = sourceLaneOf(*rhs);
        if (lhsLane != rhsLane) {
            return lhsLane < rhsLane;
        }
        return lhs->id < rhs->id;
    });
    return notes;
}

std::vector<SliceInfo> buildSlices(const std::vector<const Note*>& notes, int sameTimeEpsilonMs) {
    std::vector<SliceInfo> slices;
    for (const Note* note : notes) {
        if (slices.empty() || std::abs(note->time - slices.back().time) > sameTimeEpsilonMs) {
            SliceInfo slice;
            slice.time = note->time;
            slice.notes.push_back(note);
            slices.push_back(std::move(slice));
        } else {
            slices.back().notes.push_back(note);
        }
    }
    return slices;
}

std::optional<int> singleLane(const SliceInfo& slice) {
    if (slice.notes.size() != 1) {
        return std::nullopt;
    }
    return sourceLaneOf(*slice.notes.front());
}

bool isStrongBeat(int time, const std::vector<TimingPoint>& timingPoints) {
    double beatLength = 500.0;
    int timingBase = 0;
    for (const auto& point : timingPoints) {
        if (point.time > time) {
            break;
        }
        if (point.beatLength > 0.0 && (!point.uninherited.has_value() || *point.uninherited)) {
            beatLength = point.beatLength;
            timingBase = point.time;
        }
    }
    if (beatLength <= 0.0) {
        return false;
    }
    const double beatPosition = (static_cast<double>(time - timingBase) / beatLength);
    const double nearest = std::round(beatPosition);
    return std::abs(beatPosition - nearest) <= 0.04;
}

bool isAlternatingPair(const std::vector<int>& lanes) {
    if (lanes.size() < 4) {
        return false;
    }
    const int first = lanes[0];
    const int second = lanes[1];
    if (first == second) {
        return false;
    }
    for (std::size_t i = 0; i < lanes.size(); ++i) {
        const int expected = (i % 2 == 0) ? first : second;
        if (lanes[i] != expected) {
            return false;
        }
    }
    return true;
}

bool isDirectionalRun(const std::vector<int>& lanes) {
    if (lanes.size() < 3) {
        return false;
    }
    int direction = 0;
    for (std::size_t i = 1; i < lanes.size(); ++i) {
        const int delta = lanes[i] - lanes[i - 1];
        if (delta == 0) {
            return false;
        }
        const int stepDirection = delta > 0 ? 1 : -1;
        if (direction == 0) {
            direction = stepDirection;
        } else if (direction != stepDirection) {
            return false;
        }
    }
    return true;
}

}  // namespace

IntentGraphSummary buildIntentGraphSummary(const Chart& chart,
                                           int sourceKeyCount,
                                           int targetKeyCount,
                                           int sameTimeEpsilonMs) {
    IntentGraphSummary summary;
    summary.sourceKeyCount = sourceKeyCount;
    summary.targetKeyCount = targetKeyCount;
    summary.totalNotes = static_cast<int>(chart.notes.size());

    for (const auto& note : chart.notes) {
        if (note.type == NoteType::Hold) {
            ++summary.holdNotes;
        } else {
            ++summary.tapNotes;
        }
    }

    const auto notes = sortedNoteRefs(chart);
    const auto slices = buildSlices(notes, sameTimeEpsilonMs);
    summary.slices = static_cast<int>(slices.size());
    for (const auto& slice : slices) {
        if (slice.notes.size() > 1) {
            ++summary.chordSlices;
        }
        bool hasHold = false;
        for (const Note* note : slice.notes) {
            hasHold = hasHold || note->type == NoteType::Hold;
        }
        if (hasHold) {
            ++summary.lnAnchors;
        }
        if (isStrongBeat(slice.time, chart.timingPoints)) {
            ++summary.strongBeatAnchors;
        }
    }

    std::map<int, int> laneCounts;
    for (const auto& note : chart.notes) {
        ++laneCounts[sourceLaneOf(note)];
    }
    summary.recognizabilityAnchors = static_cast<int>(laneCounts.size());

    for (std::size_t i = 1; i < notes.size(); ++i) {
        const int dt = notes[i]->time - notes[i - 1]->time;
        if (dt > 0 && dt <= 500 && sourceLaneOf(*notes[i]) == sourceLaneOf(*notes[i - 1])) {
            ++summary.jackMotifs;
        }
    }

    constexpr std::size_t kWindow = 5;
    for (std::size_t start = 0; start + 2 < slices.size(); ++start) {
        std::vector<int> lanes;
        std::vector<int> times;
        for (std::size_t offset = 0; offset < kWindow && start + offset < slices.size(); ++offset) {
            const auto lane = singleLane(slices[start + offset]);
            if (!lane.has_value()) {
                break;
            }
            lanes.push_back(*lane);
            times.push_back(slices[start + offset].time);
        }
        if (lanes.size() >= 4) {
            for (std::size_t i = 0; i + 3 < lanes.size(); ++i) {
                const std::vector<int> window{lanes[i], lanes[i + 1], lanes[i + 2], lanes[i + 3]};
                if (isAlternatingPair(window)) {
                    ++summary.trillMotifs;
                    break;
                }
            }
        }
        if (lanes.size() >= 3) {
            for (std::size_t i = 0; i + 2 < lanes.size(); ++i) {
                const std::vector<int> window{lanes[i], lanes[i + 1], lanes[i + 2]};
                if (isDirectionalRun(window)) {
                    ++summary.stairMotifs;
                    break;
                }
            }
        }
        if (lanes.size() >= 5 && times.back() - times.front() <= 1400) {
            ++summary.streamMotifs;
        }
    }

    if (targetKeyCount > sourceKeyCount) {
        summary.mirrorSupportCandidates =
            summary.strongBeatAnchors + summary.lnAnchors + summary.stairMotifs + summary.streamMotifs;
    }

    if (notes.size() >= 2) {
        const int start = notes.front()->time;
        const int end = notes.back()->time;
        const double seconds = std::max(0.001, static_cast<double>(end - start) / 1000.0);
        summary.averageLocalNps = static_cast<double>(notes.size()) / seconds;
    }

    return summary;
}

}  // namespace keyconv::nk2
