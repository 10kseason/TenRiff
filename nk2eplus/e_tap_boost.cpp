#include "e_tap_boost.hpp"
#include "e_fullfield.hpp"
#include "safety_audit.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace keyweaver_eplus::nk2 {

EFullFieldTapBoostStats addEFullFieldTaps(const Chart& source, Chart& chart, int targetKeyCount) {
    EFullFieldTapBoostStats stats;
    stats.inputNotes = static_cast<std::int64_t>(chart.notes.size());
    if (!supportsEFullFieldTarget(targetKeyCount) ||
        !auditSafety(source, chart, targetKeyCount, 500, true).passed()) {
        stats.reverted = true;
        return stats;
    }
    using Counts = std::array<std::int64_t, 10>;
    // Copy times/tails into the index: appending notes must never invalidate it.
    struct LaneHead { int tail; int tapGuard; };
    std::array<std::map<int, LaneHead>, 10> lanes;
    // Redlines establish tempo; inherited SV points must not change the guard.
    // Invalid redlines reset to the conservative fallback until a valid one.
    std::map<int, int> tempoGuards;
    for (const auto& point : source.timingPoints) {
        if (point.uninherited == false) continue;
        int guard = 500;
        if (std::isfinite(point.beatLength) && point.beatLength > 0)
            guard = static_cast<int>(std::ceil(std::clamp(point.beatLength * 0.5, 180.0, 500.0)));
        tempoGuards[point.time] = guard;
    }
    std::map<std::string, const Note*> originals;
    for (const auto& note : source.notes) originals.emplace(note.id, &note);
    std::map<int, int> anchors;
    std::map<int, std::int64_t> sourceByWindow, addedByWindow;
    std::map<int, Counts> localUse, localTapUse;
    std::map<int, std::set<int>> chordLanes;
    std::set<std::string> usedIds;
    Counts globalUse{};
    std::int64_t alreadyAdded = 0;
    for (const auto& note : chart.notes) {
        const int window = fullFieldTapWindow(note.time);
        lanes[note.lane].emplace(note.time, LaneHead{note.type == NoteType::Hold ? *note.endTime : note.time, 0});
        ++localUse[window][note.lane]; ++globalUse[note.lane];
        chordLanes[note.time].insert(note.lane);
        usedIds.insert(note.id);
        const auto original = originals.find(note.id);
        if (original == originals.end()) {
            ++alreadyAdded; ++addedByWindow[window];
        } else {
            ++stats.retainedSourceNotes; ++sourceByWindow[window];
            const int origin = original->second->sourceLane.value_or(original->second->lane);
            const auto [it, inserted] = anchors.emplace(note.time, origin);
            if (!inserted) it->second = std::min(it->second, origin);
        }
    }
    stats.totalAddedBudget = fullFieldTapBudget(stats.retainedSourceNotes);
    // Local ceil() budgets can sum above the global ceiling. Reserve each
    // phrase's share before walking chronologically, so dense early phrases
    // cannot consume the final phrases' entire allowance. Unusable room is
    // left unused rather than forcing unsafe or off-rhythm additions.
    std::map<int, std::int64_t> tapQuota, windowTapCount;
    std::int64_t capacity = 0;
    for (const auto& [window, count] : sourceByWindow) {
        tapQuota[window] = std::max<std::int64_t>(0, fullFieldTapPhraseBudget(count) - addedByWindow[window]);
        capacity += tapQuota[window];
    }
    const auto remaining = std::max<std::int64_t>(0, stats.totalAddedBudget - alreadyAdded);
    if (capacity > remaining) {
        std::vector<std::pair<std::int64_t, int>> remainders;
        std::int64_t assigned = 0;
        for (auto& [window, quota] : tapQuota) {
            const auto numerator = quota * remaining;
            remainders.emplace_back(-(numerator % capacity), window);
            quota = numerator / capacity;
            assigned += quota;
        }
        std::sort(remainders.begin(), remainders.end());
        for (std::int64_t i = 0; i < remaining - assigned; ++i)
            ++tapQuota[remainders[static_cast<std::size_t>(i)].second];
    }
    // One addition per head per round gives later onsets a chance before an
    // early chord takes all available room. At most three new taps per onset.
    for (int round = 0; round < 3 && alreadyAdded + stats.addedTaps < stats.totalAddedBudget; ++round) {
        for (const auto& [time, origin] : anchors) {
            if (alreadyAdded + stats.addedTaps >= stats.totalAddedBudget) break;
            ++stats.candidates;
            const int window = fullFieldTapWindow(time);
            const auto tempo = tempoGuards.upper_bound(time);
            const int guard = tempo == tempoGuards.begin() ? 500 : std::prev(tempo)->second;
            if (windowTapCount[window] >= tapQuota.at(window) ||
                chordLanes[time].size() >= 8) {
                ++stats.rejectedByBudget;
                continue;
            }
            int best = -1;
            std::tuple<std::int64_t, std::int64_t, std::int64_t, std::int64_t, int, int> bestRank;
            for (int lane = 0; lane < targetKeyCount; ++lane) {
                const auto& indexed = lanes[lane];
                const auto next = indexed.lower_bound(time);
                // Beat-based relief applies only to this new tap. Existing
                // heads retain their audited placements; LN tails stay inclusive.
                if (next != indexed.end() && static_cast<std::int64_t>(next->first) - time <=
                    std::max(guard, next->second.tapGuard)) continue;
                if (next != indexed.begin()) {
                    const auto prev = std::prev(next);
                    if (static_cast<std::int64_t>(time) - prev->first <= std::max(guard, prev->second.tapGuard) ||
                        prev->second.tail >= time) continue;
                }
                int distance = targetKeyCount;
                for (int occupied : chordLanes[time]) distance = std::min(distance, std::abs(lane - occupied));
                // Balance the new layer itself so busy source/LN lanes do not
                // systematically lose every opportunity for an extra tap.
                const auto rank = std::make_tuple(localTapUse[window][lane], stats.addedLaneCounts[lane],
                                                 localUse.at(window)[lane], globalUse[lane], -distance, lane);
                if (best < 0 || rank < bestRank) { best = lane; bestRank = rank; }
            }
            if (best < 0) { ++stats.rejectedBySafety; continue; }
            Note tap;
            tap.id = "nk2-beat-fullfield-tap-" + std::to_string(time) + "-" + std::to_string(best);
            if (!usedIds.insert(tap.id).second) { ++stats.rejectedBySafety; continue; }
            tap.time = time; tap.lane = best; tap.sourceLane = origin;
            tap.type = NoteType::Tap;
            stats.newTapGuards.emplace(tap.id, guard);
            chart.notes.push_back(std::move(tap));
            lanes[best].emplace(time, LaneHead{time, guard});
            chordLanes[time].insert(best);
            ++localUse[window][best]; ++globalUse[best];
            ++localTapUse[window][best]; ++stats.addedLaneCounts[best];
            stats.minTapGuardMs = stats.minTapGuardMs == 0 ? guard : std::min(stats.minTapGuardMs, guard);
            stats.maxTapGuardMs = std::max(stats.maxTapGuardMs, guard);
            ++addedByWindow[window]; ++stats.addedTaps;
            ++windowTapCount[window];
        }
    }
    const auto audit = auditSafety(source, chart, targetKeyCount, 500, true, &stats.newTapGuards);
    if (!audit.passed()) {
        chart.notes.resize(static_cast<std::size_t>(stats.inputNotes));
        stats.addedTaps = 0;
        stats.newTapGuards.clear();
        stats.addedLaneCounts.fill(0);
        stats.minTapGuardMs = stats.maxTapGuardMs = 0;
        stats.reverted = true;
    } else if (stats.addedTaps > 0) {
        // osu exports in vector order. Stable time ordering keeps pre-existing
        // same-time notes in their original relative order while inserting taps.
        std::stable_sort(chart.notes.begin(), chart.notes.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });
    }
    if (!stats.reverted) {
        stats.strict500CreatedRepeats = audit.createdLaneRepeats + audit.tempoAllowedTapRepeats;
        stats.tempoAllowedTapRepeats = audit.tempoAllowedTapRepeats;
    }
    return stats;
}

}  // namespace keyweaver_eplus::nk2
