#include "e_fullfield.hpp"
#include "safety_audit.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace keyweaver_eplus::nk2 {
namespace {

// Fixed storage capacity keeps the 10K arithmetic/order unchanged. For 8K,
// unused cells stay zero and are never candidate destinations.
constexpr int kLaneCapacity = 10;
constexpr std::uint64_t kSafetyWorkLimit = 2000000;
using Counts = std::array<std::int64_t, kLaneCapacity>;
using Histogram = std::map<std::int64_t, Counts>;
using Indices = std::vector<std::size_t>;

std::int64_t bucket(int time, int width) {
    const auto t = static_cast<std::int64_t>(time);
    return t / width - (t % width < 0 ? 1 : 0);
}

struct Group {
    Indices notes;
    std::map<std::int64_t, std::int64_t> counts2s;
    std::map<std::int64_t, std::int64_t> counts8s;
    std::map<int, std::int64_t> originals;
    std::map<std::pair<int, std::int64_t>, std::int64_t> originals8s;
};

std::pair<double, double> metrics(const Histogram& histogram) {
    double active = 0.0, concentration = 0.0;
    for (const auto& [index, counts] : histogram) {
        (void)index;
        const double total = static_cast<double>(std::accumulate(counts.begin(), counts.end(), std::int64_t{0}));
        for (const auto count : counts) {
            active += count > 0 ? 1.0 : 0.0;
            const double share = static_cast<double>(count) / total;
            concentration += share * share;
        }
    }
    const double windows = static_cast<double>(std::max<std::size_t>(1, histogram.size()));
    return {active / windows, concentration / windows};
}

double concentrationGain(const Counts& counts, int from, int to, std::int64_t n) {
    const double a = static_cast<double>(counts[from]);
    const double b = static_cast<double>(counts[to]);
    const double moved = static_cast<double>(n);
    const double total = static_cast<double>(std::accumulate(counts.begin(), counts.end(), std::int64_t{0}));
    return 2.0 * moved * (a - b - moved) / (total * total);
}

}  // namespace

EFullFieldStats spreadEFullField(const Chart& source, Chart& completedE, int targetKeyCount) {
    EFullFieldStats stats;
    // A remix is never a repair for a failed E chart. The original-source audit
    // also retains generated-note provenance rather than treating E as source.
    if ((targetKeyCount != 8 && targetKeyCount != 10) ||
        !auditSafety(source, completedE, targetKeyCount, 500, true).passed()) {
        stats.reverted = true;
        return stats;
    }
    if (completedE.notes.empty()) return stats;

    const auto& notes = completedE.notes;
    std::map<std::string, const Note*> originals;
    for (const auto& note : source.notes) originals.emplace(note.id, &note);
    std::vector<const Note*> provenance;
    std::vector<int> baselineLanes;
    std::array<Indices, kLaneCapacity> lanes;
    Histogram h2, h8, sourceUse;
    std::map<std::pair<int, std::int64_t>, Counts> sourceUse8;
    Counts global{};
    for (std::size_t i = 0; i < notes.size(); ++i) {
        const auto& note = notes[i];
        const auto found = originals.find(note.id);
        provenance.push_back(found == originals.end() ? nullptr : found->second);
        baselineLanes.push_back(note.lane);
        lanes[note.lane].push_back(i);
        ++h2[bucket(note.time, 2000)][note.lane];
        ++h8[bucket(note.time, 8000)][note.lane];
        ++global[note.lane];
        if (provenance.back()) {
            const int origin = provenance.back()->sourceLane.value_or(provenance.back()->lane);
            ++sourceUse[origin][note.lane];
            ++sourceUse8[{origin, bucket(note.time, 8000)}][note.lane];
        }
    }
    const auto before = metrics(h2);
    stats.activeLanesBefore = before.first;
    stats.concentrationBefore = before.second;
    stats.sourceConcentrationBefore = metrics(sourceUse).second;
    const auto earlier = [&](std::size_t a, std::size_t b) {
        if (notes[a].time != notes[b].time) return notes[a].time < notes[b].time;
        if (notes[a].id != notes[b].id) return notes[a].id < notes[b].id;
        return a < b;
    };

    std::vector<Group> groups;
    for (auto& lane : lanes) {
        std::sort(lane.begin(), lane.end(), earlier);
        for (std::size_t begin = 0; begin < lane.size();) {
            auto chainEnd = begin + 1;
            while (chainEnd < lane.size() && static_cast<std::int64_t>(notes[lane[chainEnd]].time) -
                   notes[lane[chainEnd - 1]].time <= 500) ++chainEnd;
            // A 500ms safety window must not pin a source stream for a whole
            // song. Keep short chains intact, including across window edges;
            // only long chains may be segmented at fixed 2s boundaries.
            const bool longChain = static_cast<std::int64_t>(notes[lane[chainEnd - 1]].time) -
                                   notes[lane[begin]].time > 2000;
            if (longChain) ++stats.splitLongChains;
            while (begin < chainEnd) {
                auto end = chainEnd;
                if (longChain) {
                    end = begin + 1;
                    while (end < chainEnd && bucket(notes[lane[end]].time, 2000) ==
                           bucket(notes[lane[begin]].time, 2000)) ++end;
                }
                Group group;
                for (auto i = begin; i < end; ++i) {
                    const auto index = lane[i];
                    group.notes.push_back(index);
                    ++group.counts2s[bucket(notes[index].time, 2000)];
                    ++group.counts8s[bucket(notes[index].time, 8000)];
                    if (provenance[index]) {
                        const int origin = provenance[index]->sourceLane.value_or(provenance[index]->lane);
                        ++group.originals[origin];
                        ++group.originals8s[{origin, bucket(notes[index].time, 8000)}];
                    }
                }
                groups.push_back(std::move(group));
                begin = end;
            }
        }
    }
    std::sort(groups.begin(), groups.end(), [&](const auto& a, const auto& b) {
        return earlier(a.notes.front(), b.notes.front());
    });

    const auto laneSafe = [&](const Indices& indices) {
        std::optional<std::int64_t> activeHoldEnd;
        std::optional<std::size_t> previous;
        for (const auto index : indices) {
            if (++stats.safetyWork > kSafetyWorkLimit) {
                stats.budgetLimited = true;
                return false;
            }
            const auto& note = notes[index];
            if (activeHoldEnd && *activeHoldEnd >= note.time) return false;
            if (previous) {
                const auto gap = static_cast<std::int64_t>(note.time) - notes[*previous].time;
                if (gap == 0) return false;
                if (gap <= 500) {
                    const auto* a = provenance[*previous];
                    const auto* b = provenance[index];
                    if (!a || !b || a->sourceLane.value_or(a->lane) != b->sourceLane.value_or(b->lane)) return false;
                    const auto originalGap = std::abs(static_cast<std::int64_t>(a->time) - b->time);
                    if (originalGap <= 0 || originalGap > 500) return false;
                }
            }
            if (note.type == NoteType::Hold) activeHoldEnd = *note.endTime;
            previous = index;
        }
        return true;
    };

    // Two deterministic passes and a comparison-count budget, never a wall-clock
    // timeout. Unvisited groups keep their existing safe E placement.
    for (int pass = 0; pass < 2 && !stats.budgetLimited; ++pass) {
        for (const auto& group : groups) {
            if (stats.budgetLimited) break;
            const int from = notes[group.notes.front()].lane;
            const auto n = static_cast<std::int64_t>(group.notes.size());
            Indices remaining;
            std::set_difference(lanes[from].begin(), lanes[from].end(), group.notes.begin(), group.notes.end(),
                                std::back_inserter(remaining), earlier);
            if (!laneSafe(remaining)) continue;
            int bestLane = from;
            double bestGain = 1e-12;
            Indices bestMerged;
            for (int to = 0; to < targetKeyCount && !stats.budgetLimited; ++to) {
                if (to == from) continue;
                double gain2 = 0.0, gain8 = 0.0;
                int activeGain = 0;
                for (const auto& [index, count] : group.counts2s) {
                    const auto& counts = h2.at(index);
                    gain2 += concentrationGain(counts, from, to, count);
                    activeGain += (counts[from] > count ? 1 : 0) + (counts[to] + count > 0 ? 1 : 0)
                                - (counts[from] > 0 ? 1 : 0) - (counts[to] > 0 ? 1 : 0);
                }
                if (activeGain < 0 || gain2 < -1e-12) continue;
                for (const auto& [index, count] : group.counts8s) {
                    gain8 += concentrationGain(h8.at(index), from, to, count);
                }
                double sourceGain = 0.0;
                for (const auto& [origin, count] : group.originals)
                    sourceGain += concentrationGain(sourceUse.at(origin), from, to, count);
                for (const auto& [key, count] : group.originals8s)
                    sourceGain += 2.0 * concentrationGain(sourceUse8.at(key), from, to, count);
                const double gain = 4.0 * gain2 + gain8 + concentrationGain(global, from, to, n) + sourceGain;
                const bool better = gain > bestGain + 1e-12;
                const bool closerTie = bestLane != from && std::abs(gain - bestGain) <= 1e-12 &&
                                       std::abs(to - from) < std::abs(bestLane - from);
                if (!better && !closerTie) continue;
                Indices merged;
                std::merge(lanes[to].begin(), lanes[to].end(), group.notes.begin(), group.notes.end(),
                           std::back_inserter(merged), earlier);
                if (!laneSafe(merged)) continue;
                bestLane = to;
                bestGain = gain;
                bestMerged = std::move(merged);
            }
            if (bestLane == from) continue;
            lanes[from] = std::move(remaining);
            lanes[bestLane] = std::move(bestMerged);
            for (const auto index : group.notes) completedE.notes[index].lane = bestLane;
            for (const auto& [index, count] : group.counts2s) { h2[index][from] -= count; h2[index][bestLane] += count; }
            for (const auto& [index, count] : group.counts8s) { h8[index][from] -= count; h8[index][bestLane] += count; }
            global[from] -= n;
            global[bestLane] += n;
            for (const auto& [origin, count] : group.originals) {
                sourceUse[origin][from] -= count; sourceUse[origin][bestLane] += count;
            }
            for (const auto& [key, count] : group.originals8s) {
                sourceUse8[key][from] -= count; sourceUse8[key][bestLane] += count;
            }
            ++stats.acceptedMoves;
        }
    }

    // A field can be balanced while each original source stays pinned. Swap
    // whole lane contents within a 2s window to escape that local minimum.
    // Such a permutation preserves that window's active count/concentration.
    // Both changed lanes must pass safety before either is committed.
    std::map<std::int64_t, std::array<Indices, kLaneCapacity>> windows;
    std::set<std::pair<std::int64_t, int>> locked;
    for (std::size_t i = 0; i < notes.size(); ++i)
        windows[bucket(notes[i].time, 2000)][notes[i].lane].push_back(i);
    for (const auto& group : groups) if (group.counts2s.size() > 1) {
        for (const auto& [index, count] : group.counts2s) {
            (void)count;
            locked.emplace(index, notes[group.notes.front()].lane);
        }
    }
    for (auto& [window, members] : windows) {
        if (stats.budgetLimited) break;
        const auto window8 = window / 4 - (window % 4 < 0 ? 1 : 0);
        Histogram windowSources;
        for (auto& lane : members) {
            std::sort(lane.begin(), lane.end(), earlier);
            for (const auto i : lane) if (provenance[i])
                ++windowSources[provenance[i]->sourceLane.value_or(provenance[i]->lane)][notes[i].lane];
        }
        for (int a = 0; a < targetKeyCount && !stats.budgetLimited; ++a) {
            for (int b = a + 1; b < targetKeyCount && !stats.budgetLimited; ++b) {
                if (locked.count({window, a}) || locked.count({window, b})) continue;
                double sourceGain = 0.0;
                for (const auto& [origin, counts] : windowSources) {
                    const auto delta = counts[a] - counts[b];
                    sourceGain += concentrationGain(sourceUse.at(origin), a, b, delta) +
                        2.0 * concentrationGain(sourceUse8.at({static_cast<int>(origin), window8}), a, b, delta);
                }
                if (sourceGain <= 1e-12) continue;
                const auto delta = h2.at(window)[a] - h2.at(window)[b];
                if (sourceGain + concentrationGain(h8.at(window8), a, b, delta) +
                    concentrationGain(global, a, b, delta) <= 1e-12) continue;
                const auto swappedLane = [&](int from, int to) {
                    Indices remaining, merged;
                    std::set_difference(lanes[from].begin(), lanes[from].end(), members[from].begin(),
                        members[from].end(), std::back_inserter(remaining), earlier);
                    std::merge(remaining.begin(), remaining.end(), members[to].begin(), members[to].end(),
                        std::back_inserter(merged), earlier);
                    return merged;
                };
                auto newA = swappedLane(a, b);
                if (!laneSafe(newA)) continue;
                auto newB = swappedLane(b, a);
                if (!laneSafe(newB)) continue;
                lanes[a] = std::move(newA); lanes[b] = std::move(newB);
                for (const auto i : members[a]) completedE.notes[i].lane = b;
                for (const auto i : members[b]) completedE.notes[i].lane = a;
                std::swap(members[a], members[b]);
                std::swap(h2[window][a], h2[window][b]);
                h8[window8][a] -= delta; h8[window8][b] += delta;
                global[a] -= delta; global[b] += delta;
                for (auto& [origin, counts] : windowSources) {
                    const auto moved = counts[a] - counts[b];
                    auto& local = sourceUse8.at({static_cast<int>(origin), window8});
                    local[a] -= moved; local[b] += moved;
                    sourceUse[origin][a] -= moved; sourceUse[origin][b] += moved;
                    std::swap(counts[a], counts[b]);
                }
                ++stats.acceptedMoves;
                ++stats.phraseSwaps;
            }
        }
    }

    if (!auditSafety(source, completedE, targetKeyCount, 500, true).passed()) {
        for (std::size_t i = 0; i < notes.size(); ++i) completedE.notes[i].lane = baselineLanes[i];
        stats.reverted = true;
        stats.activeLanesAfter = stats.activeLanesBefore;
        stats.concentrationAfter = stats.concentrationBefore;
        stats.sourceConcentrationAfter = stats.sourceConcentrationBefore;
        return stats;
    }
    for (std::size_t i = 0; i < notes.size(); ++i) if (notes[i].lane != baselineLanes[i]) ++stats.movedNotes;
    for (const auto& group : groups) {
        if (notes[group.notes.front()].lane != baselineLanes[group.notes.front()]) ++stats.movedGroups;
    }
    const auto after = metrics(h2);
    stats.activeLanesAfter = after.first;
    stats.concentrationAfter = after.second;
    stats.sourceConcentrationAfter = metrics(sourceUse).second;
    return stats;
}

}  // namespace keyweaver_eplus::nk2
