#include "nk2e/nk2_convert.hpp"

#include "nk2e/intent_graph.hpp"
#include "nk2e/layout_model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace keyweaver_e::nk2 {

namespace {

struct SliceInfo {
    int time = 0;
    std::vector<std::size_t> noteIndices;
};

struct PlacedNote {
    int time = 0;
    int lane = 0;
    int sourceLane = 0;
    NoteType type = NoteType::Tap;
    std::optional<int> endTime;
};

constexpr double kFourToFiveFillAddedRatio = 0.12;
constexpr double kFourToFiveFillPhraseRatio = 0.30;
constexpr int kFourToFiveFillMinimumBudget = 8;
constexpr int kFourToFiveFillMinimumPhraseBudget = 3;
constexpr int kFourToFiveFillMaximumPhraseBudget = 12;
constexpr int kDefaultSupportJackWindowMs = 500;
constexpr int kFourToFiveSupportJackWindowMs = 240;
constexpr int kDefaultSupportSameSourceGapMs = 120;
constexpr int kFourToFiveSupportSameSourceGapMs = 80;
constexpr int kMaxKeyCount = kMaxSupportedKeyCount;

using LaneMask = std::uint32_t;
static_assert(kMaxKeyCount <= 32, "LaneMask must cover every supported NK2 lane");

struct Candidate {
    int lane = 0;
    double score = 0.0;
};

struct LanePool {
    std::array<int, kMaxKeyCount> lanes{};
    int count = 0;
};

struct CandidateList {
    std::array<Candidate, kMaxKeyCount> candidates{};
    int count = 0;

    const Candidate* begin() const {
        return candidates.data();
    }

    const Candidate* end() const {
        return candidates.data() + count;
    }
};

enum class MotifKind {
    Neutral,
    Jack,
    Trill,
    Stair,
    Stream,
    Chord,
    LnAnchor,
};

struct SolvedPlacement {
    Note note;
    MotifKind motif = MotifKind::Neutral;
};

struct RecentSingleNote {
    int time = 0;
    int sourceLane = 0;
    int targetLane = 0;
};

enum class SupportKind {
    Ln,
    StrongBeat,
    Mirror,
};

struct SupportEvent {
    SupportKind kind = SupportKind::Ln;
    MotifKind anchorMotif = MotifKind::Neutral;
    int time = 0;
    int sourceLane = 0;
    int anchorLane = 0;
    std::string anchorId;
};

struct PlacementStats {
    int sameTimeCollisions = 0;
    int longNoteConflicts = 0;
    int createdJacks = 0;
    int preservedSourceJacks = 0;
    int sourceAnchorMatches = 0;
    int sourceAnchorTotal = 0;
    std::vector<int> laneDistribution;
};

using LastBySource = std::array<std::optional<PlacedNote>, kMaxKeyCount>;

int sourceLaneOf(const Note& note);

struct SupportSafetyIndex {
    std::array<std::vector<PlacedNote>, kMaxKeyCount> lanes;
};

void addToSupportSafetyIndex(SupportSafetyIndex& index, const Note& note) {
    if (note.lane < 0 || note.lane >= kMaxKeyCount) {
        return;
    }
    index.lanes[static_cast<std::size_t>(note.lane)].push_back(
        PlacedNote{note.time, note.lane, sourceLaneOf(note), note.type, note.endTime});
}

SupportSafetyIndex buildSupportSafetyIndex(const Chart& chart) {
    SupportSafetyIndex index;
    for (const auto& note : chart.notes) {
        addToSupportSafetyIndex(index, note);
    }
    return index;
}

int sourceLaneOf(const Note& note) {
    return note.sourceLane.value_or(note.lane);
}

int clampInt(int value, int lower, int upper) {
    return std::max(lower, std::min(upper, value));
}

double clamp01(double value) {
    return std::max(0.0, std::min(1.0, value));
}

bool isFourToFiveFillOptions(const NK2Options& options) {
    return options.sourceKeyCount == 4 && options.targetKeyCount == 5 && options.mode != Mode::Report;
}

int supportJackWindowMsFor(const NK2Options& options) {
    if (options.phraseSupport) return kDefaultSupportJackWindowMs;
    return isFourToFiveFillOptions(options) ? kFourToFiveSupportJackWindowMs
                                           : kDefaultSupportJackWindowMs;
}

int directLane(int sourceLane, int sourceKeyCount, int targetKeyCount) {
    if (sourceKeyCount <= 1 || targetKeyCount <= 1) {
        return 0;
    }
    const auto sourceLayout = buildKeyLayoutProfile(sourceKeyCount);
    const auto targetLayout = buildKeyLayoutProfile(targetKeyCount);
    return laneForNormalizedPosition(targetLayout, normalizedLanePosition(sourceLayout, sourceLane));
}

bool isNk2GeneratedNote(const Note& note) {
    return note.id.rfind("nk2-", 0) == 0;
}

std::string convertedDifficultyName(const std::optional<std::string>& existing,
                                    int targetKeyCount,
                                    bool superSymmetry) {
    std::string marker = "KeyWeaverNK2-" + std::to_string(targetKeyCount) + "K";
    if (superSymmetry) {
        marker += "-sSym";
    }
    if (!existing.has_value() || existing->empty()) {
        return marker;
    }
    if (existing->find("KeyWeaver") != std::string::npos) {
        return *existing;
    }
    return *existing + " " + marker;
}

std::vector<std::size_t> sortedNoteOrder(const Chart& chart) {
    std::vector<std::size_t> order(chart.notes.size());
    for (std::size_t index = 0; index < chart.notes.size(); ++index) {
        order[index] = index;
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
        const auto& lhsNote = chart.notes[lhs];
        const auto& rhsNote = chart.notes[rhs];
        if (lhsNote.time != rhsNote.time) {
            return lhsNote.time < rhsNote.time;
        }
        const int lhsLane = sourceLaneOf(lhsNote);
        const int rhsLane = sourceLaneOf(rhsNote);
        if (lhsLane != rhsLane) {
            return lhsLane < rhsLane;
        }
        return lhsNote.id < rhsNote.id;
    });
    return order;
}

std::vector<SliceInfo> buildSlices(const Chart& chart, int sameTimeEpsilonMs) {
    std::vector<SliceInfo> slices;
    for (const auto index : sortedNoteOrder(chart)) {
        const auto& note = chart.notes[index];
        if (slices.empty() || std::abs(note.time - slices.back().time) > sameTimeEpsilonMs) {
            SliceInfo slice;
            slice.time = note.time;
            slice.noteIndices.push_back(index);
            slices.push_back(std::move(slice));
        } else {
            slices.back().noteIndices.push_back(index);
        }
    }
    return slices;
}

void addUnique(LanePool& lanes, int lane, int targetKeyCount) {
    if (lane < 0 || lane >= targetKeyCount || lanes.count >= kMaxKeyCount) {
        return;
    }
    for (int index = 0; index < lanes.count; ++index) {
        if (lanes.lanes[static_cast<std::size_t>(index)] == lane) {
            return;
        }
    }
    lanes.lanes[static_cast<std::size_t>(lanes.count++)] = lane;
}

void addCandidate(CandidateList& candidates, int lane, double score) {
    if (candidates.count >= kMaxKeyCount) {
        return;
    }
    candidates.candidates[static_cast<std::size_t>(candidates.count++)] = Candidate{lane, score};
}

bool candidateBefore(const Candidate& lhs, const Candidate& rhs) {
    if (lhs.score != rhs.score) {
        return lhs.score > rhs.score;
    }
    return lhs.lane < rhs.lane;
}

void sortCandidates(CandidateList& candidates) {
    for (int index = 1; index < candidates.count; ++index) {
        const Candidate current = candidates.candidates[static_cast<std::size_t>(index)];
        int insert = index;
        while (insert > 0 && candidateBefore(current, candidates.candidates[static_cast<std::size_t>(insert - 1)])) {
            candidates.candidates[static_cast<std::size_t>(insert)] =
                candidates.candidates[static_cast<std::size_t>(insert - 1)];
            --insert;
        }
        candidates.candidates[static_cast<std::size_t>(insert)] = current;
    }
}

LanePool candidatePoolFor(int sourceLane, int sourceKeyCount, int targetKeyCount) {
    LanePool lanes;
    const int direct = directLane(sourceLane, sourceKeyCount, targetKeyCount);
    addUnique(lanes, direct, targetKeyCount);
    addUnique(lanes, direct - 1, targetKeyCount);
    addUnique(lanes, direct + 1, targetKeyCount);
    addUnique(lanes, targetKeyCount - 1 - direct, targetKeyCount);
    for (int lane = 0; lane < targetKeyCount; ++lane) {
        addUnique(lanes, lane, targetKeyCount);
    }
    return lanes;
}

bool laneInSourcePanel(int sourceLane,
                       int lane,
                       int sourceKeyCount,
                       int targetKeyCount) {
    const auto sourceLayout = buildKeyLayoutProfile(sourceKeyCount);
    const auto targetLayout = buildKeyLayoutProfile(targetKeyCount);
    const auto sourceSide = laneSideFor(sourceLayout, sourceLane);
    const auto targetSide = laneSideFor(targetLayout, lane);
    if (sourceSide == LaneSide::Center) {
        return targetSide == LaneSide::Center || laneIsInBridge(targetLayout, lane);
    }
    return sourceSide == targetSide;
}

bool laneInBridge(int lane, int targetKeyCount) {
    return laneIsInBridge(buildKeyLayoutProfile(targetKeyCount), lane);
}

bool sameDirection(int lhs, int rhs) {
    return lhs != 0 && rhs != 0 && (lhs > 0) == (rhs > 0);
}

double desiredLaneShare(int lane, int targetKeyCount, const LayoutWeights& weights) {
    const double weighted = desiredLaneShareFor(buildKeyLayoutProfile(targetKeyCount, weights), lane);
    if (targetKeyCount == 8) {
        constexpr double kWholeFieldBlend = 0.35;
        const double uniform = 1.0 / static_cast<double>(targetKeyCount);
        return weighted * (1.0 - kWholeFieldBlend) + uniform * kWholeFieldBlend;
    }
    return weighted;
}

double genericTargetFreedomMultiplier(int targetKeyCount) {
    if (targetKeyCount == 8) {
        return 1.35;
    }
    if (targetKeyCount == 5) {
        return 1.55;
    }
    return 1.0;
}

double genericTargetAnchorLockMultiplier(int targetKeyCount) {
    if (targetKeyCount == 8) {
        return 0.62;
    }
    return 1.0;
}

int holdDurationMs(const Note& note) {
    if (note.type != NoteType::Hold || !note.endTime.has_value()) {
        return 0;
    }
    return std::max(0, *note.endTime - note.time);
}

bool isLongHoldForAdjacentCopy(const Note& note) {
    return holdDurationMs(note) >= 1000;
}

LanePool adjacentCopyLaneCandidates(const Note& note,
                                    const NK2Options& options,
                                    const LastBySource& lastBySource) {
    LanePool lanes;
    const int sourceLane = sourceLaneOf(note);
    for (const int sourceDelta : {-1, 1}) {
        const int adjacentSource = sourceLane + sourceDelta;
        if (adjacentSource < 0 || adjacentSource >= options.sourceKeyCount) {
            continue;
        }

        const auto& adjacentPlaced = lastBySource[static_cast<std::size_t>(adjacentSource)];
        if (adjacentPlaced.has_value()) {
            addUnique(lanes, adjacentPlaced->lane - sourceDelta, options.targetKeyCount);
            addUnique(lanes, adjacentPlaced->lane, options.targetKeyCount);
        }

        const int adjacentDirect = directLane(
            adjacentSource, options.sourceKeyCount, options.targetKeyCount);
        addUnique(lanes, adjacentDirect - sourceDelta, options.targetKeyCount);
    }
    return lanes;
}

double adjacentCopyPreferenceScore(int lane, const LanePool& adjacentCopyLanes) {
    double score = 0.0;
    for (int index = 0; index < adjacentCopyLanes.count; ++index) {
        const int distance = std::abs(lane - adjacentCopyLanes.lanes[static_cast<std::size_t>(index)]);
        const double rankDecay = static_cast<double>(index) * 0.08;
        if (distance == 0) {
            score = std::max(score, 1.35 - rankDecay);
        } else if (distance == 1) {
            score = std::max(score, 0.45 - rankDecay * 0.5);
        }
    }
    return score;
}

int mirroredLane(int lane, int keyCount) {
    return keyCount - 1 - lane;
}

std::optional<int> sameTimeMirrorAnchorTargetLane(const Note& note,
                                                  const std::vector<PlacedNote>& placed,
                                                  const NK2Options& options) {
    if (!options.superSymmetry || options.sourceKeyCount <= 1 || options.targetKeyCount <= 1) {
        return std::nullopt;
    }

    const int sourceLane = sourceLaneOf(note);
    const int mirrorSourceLane = mirroredLane(sourceLane, options.sourceKeyCount);
    if (mirrorSourceLane == sourceLane) {
        return std::nullopt;
    }

    for (auto it = placed.rbegin(); it != placed.rend(); ++it) {
        if (std::abs(note.time - it->time) > options.sameTimeEpsilonMs) {
            if (note.time > it->time) {
                break;
            }
            continue;
        }
        if (it->sourceLane == mirrorSourceLane) {
            return mirroredLane(it->lane, options.targetKeyCount);
        }
    }
    return std::nullopt;
}

bool preservesGaplessStair(int sourceLane,
                           int targetLane,
                           const std::optional<int>& previousSingleSourceLane,
                           const std::optional<int>& previousSingleTargetLane) {
    if (!previousSingleSourceLane.has_value() || !previousSingleTargetLane.has_value()) {
        return false;
    }
    const int sourceDelta = sourceLane - *previousSingleSourceLane;
    const int targetDelta = targetLane - *previousSingleTargetLane;
    return std::abs(sourceDelta) == 1 && std::abs(targetDelta) == 1 &&
           sameDirection(sourceDelta, targetDelta);
}

double superSymmetryScoreAdjustment(const Note& note,
                                    int lane,
                                    const NK2Options& options,
                                    const std::vector<PlacedNote>& placed,
                                    const std::optional<int>& previousSingleSourceLane,
                                    const std::optional<int>& previousSingleTargetLane) {
    if (!options.superSymmetry) {
        return 0.0;
    }

    double score = 0.0;
    const int sourceLane = sourceLaneOf(note);
    const auto mirrorAnchor = sameTimeMirrorAnchorTargetLane(note, placed, options);
    if (mirrorAnchor.has_value()) {
        score += lane == *mirrorAnchor ? 4.0 : -1.25;
    }

    if (previousSingleSourceLane.has_value() && previousSingleTargetLane.has_value()) {
        const int sourceDelta = sourceLane - *previousSingleSourceLane;
        const int targetDelta = lane - *previousSingleTargetLane;
        if (std::abs(sourceDelta) == 1) {
            if (targetDelta != 0 && sameDirection(sourceDelta, targetDelta)) {
                score += std::abs(targetDelta) == 1 ? 2.35 : -0.80;
            } else {
                score -= 1.80;
            }
        }
    }
    return score;
}

std::optional<std::pair<int, int>> panelRangeForLane(int lane, int targetKeyCount) {
    const auto layout = buildKeyLayoutProfile(targetKeyCount);
    if (!layout.hasPanels || lane < 0 || lane >= targetKeyCount) {
        return std::nullopt;
    }
    const auto side = laneSideFor(layout, lane);
    if (side == LaneSide::Left) {
        return std::make_pair(layout.leftStart, layout.leftEnd);
    }
    if (side == LaneSide::Right) {
        return std::make_pair(layout.rightStart, layout.rightEnd);
    }
    return std::nullopt;
}

std::optional<int> panelSideForLane(int lane, int targetKeyCount) {
    const auto layout = buildKeyLayoutProfile(targetKeyCount);
    if (!layout.hasPanels || lane < 0 || lane >= targetKeyCount) {
        return std::nullopt;
    }
    const auto side = laneSideFor(layout, lane);
    if (side == LaneSide::Center) {
        return std::nullopt;
    }
    return side == LaneSide::Left ? -1 : 1;
}

int mirrorCadenceSide(std::size_t placedOriginalNotes) {
    // LRRL gives NK2 a mirrored two-note hand cadence without hard-assigning lanes.
    constexpr int kLeftRightRightLeft[] = {-1, 1, 1, -1};
    return kLeftRightRightLeft[placedOriginalNotes % 4];
}

struct LaneUseContext {
    int sourceKeyCount = 0;
    int targetKeyCount = 0;
    LayoutWeights weights;
    std::array<int, kMaxKeyCount> use{};
    std::array<double, kMaxKeyCount> desiredShare{};
    int total = 0;
    int leftPanelTotal = 0;
    int rightPanelTotal = 0;
};

LaneUseContext buildLaneUseContext(const std::vector<int>& laneUse,
                                   int sourceKeyCount,
                                   int targetKeyCount,
                                   const LayoutWeights& weights) {
    LaneUseContext context;
    context.sourceKeyCount = std::max(0, std::min(kMaxKeyCount, sourceKeyCount));
    context.targetKeyCount = std::max(0, std::min(kMaxKeyCount, targetKeyCount));
    context.weights = weights;
    const auto targetLayout = buildKeyLayoutProfile(context.targetKeyCount, weights);
    for (int lane = 0; lane < context.targetKeyCount; ++lane) {
        const int count = lane < static_cast<int>(laneUse.size()) ? laneUse[static_cast<std::size_t>(lane)] : 0;
        context.use[static_cast<std::size_t>(lane)] = count;
        context.desiredShare[static_cast<std::size_t>(lane)] = desiredLaneShare(lane, targetKeyCount, weights);
        context.total += count;
        const auto side = laneSideFor(targetLayout, lane);
        if (side == LaneSide::Left) {
            context.leftPanelTotal += count;
        } else if (side == LaneSide::Right) {
            context.rightPanelTotal += count;
        }
    }
    return context;
}

double laneCoverageNeedScoreFast(const LaneUseContext& context, int lane) {
    if (context.targetKeyCount <= 0 || lane < 0 || lane >= context.targetKeyCount) {
        return 0.0;
    }

    const double desired = context.desiredShare[static_cast<std::size_t>(lane)];
    if (desired <= 0.0) {
        return 0.0;
    }

    const double virtualTotal = std::max(4.0, static_cast<double>(context.targetKeyCount) * 0.5);
    const double actual = static_cast<double>(context.use[static_cast<std::size_t>(lane)]) +
                          desired * virtualTotal;
    const double expected = desired * (static_cast<double>(context.total) + virtualTotal + 1.0);
    const double ratio = expected <= 0.0 ? 1.0 : actual / expected;
    if (ratio <= 1.0) {
        return clamp01((1.0 - ratio) * 1.8);
    }
    return -clamp01((ratio - 1.0) * 0.9);
}

double wholeFieldNeedScoreFast(const LaneUseContext& context, int lane) {
    if (context.targetKeyCount <= 0 || lane < 0 || lane >= context.targetKeyCount) {
        return 0.0;
    }

    const double virtualTotal = static_cast<double>(context.targetKeyCount);
    const double actual = static_cast<double>(context.use[static_cast<std::size_t>(lane)]) + 1.0;
    const double expected = (static_cast<double>(context.total) + virtualTotal + 1.0) /
                            static_cast<double>(context.targetKeyCount);
    const double ratio = expected <= 0.0 ? 1.0 : actual / expected;
    if (ratio <= 1.0) {
        return clamp01((1.0 - ratio) * 2.2);
    }
    return -clamp01((ratio - 1.0) * 0.8);
}

double panelLaneNeedScoreFast(const LaneUseContext& context, int lane) {
    if (lane < 0 || lane >= context.targetKeyCount) {
        return 0.0;
    }

    const auto range = panelRangeForLane(lane, context.targetKeyCount);
    if (!range.has_value()) {
        return 0.0;
    }

    const auto [start, end] = *range;
    const int panelTotal = lane < context.targetKeyCount / 2 ? context.leftPanelTotal : context.rightPanelTotal;
    const int width = std::max(1, end - start + 1);
    const double virtualTotal = static_cast<double>(width);
    const double actual = static_cast<double>(context.use[static_cast<std::size_t>(lane)]) +
                          virtualTotal / static_cast<double>(width);
    const double expected = (static_cast<double>(panelTotal) + virtualTotal + 1.0) /
                            static_cast<double>(width);
    const double ratio = expected <= 0.0 ? 1.0 : actual / expected;
    if (ratio <= 1.0) {
        return clamp01((1.0 - ratio) * 1.7);
    }
    return -clamp01((ratio - 1.0) * 0.7);
}

double laneCoveragePreferenceFast(const LaneUseContext& context, int lane) {
    const double globalNeed = laneCoverageNeedScoreFast(context, lane);
    const double panelNeed = panelLaneNeedScoreFast(context, lane);
    const double freedomBoost = genericTargetFreedomMultiplier(context.targetKeyCount) - 1.0;
    const double wholeFieldNeed = wholeFieldNeedScoreFast(context, lane);
    const double combinedNeed = std::max(
        -1.0, std::min(1.0, globalNeed + panelNeed * 0.45 + freedomBoost * wholeFieldNeed));
    return 0.5 + 0.5 * combinedNeed;
}

double remixScoreForLaneFast(int sourceLane, int lane, const LaneUseContext& context) {
    const double coverageScore = laneCoveragePreferenceFast(context, lane);
    const double panelScore = laneInSourcePanel(
        sourceLane, lane, context.sourceKeyCount, context.targetKeyCount) ? 1.0 : 0.0;
    const double bridgeScore = laneInBridge(lane, context.targetKeyCount) ? 1.0 : 0.0;
    const double totalWeight = std::max(1, context.weights.panel + context.weights.bridge + context.weights.fullField);
    return (static_cast<double>(context.weights.panel) * panelScore +
            static_cast<double>(context.weights.bridge) * bridgeScore +
            static_cast<double>(context.weights.fullField) * coverageScore) /
           totalWeight;
}

double mirrorCadenceScoreAdjustment(int lane,
                                    const NK2Options& options,
                                    const LaneUseContext& laneContext,
                                    const std::vector<PlacedNote>& placed,
                                    bool freeOriginalTap,
                                    bool sourceJackContinuation) {
    if (sourceJackContinuation || !freeOriginalTap) {
        return 0.0;
    }

    const auto side = panelSideForLane(lane, options.targetKeyCount);
    if (!side.has_value()) {
        return 0.0;
    }

    const int leftTotal = laneContext.leftPanelTotal;
    const int rightTotal = laneContext.rightPanelTotal;

    double score = 0.0;
    const int desiredSide = mirrorCadenceSide(placed.size());
    if (*side == desiredSide) {
        score += freeOriginalTap ? 0.64 : 0.26;
    } else {
        score -= freeOriginalTap ? 0.30 : 0.10;
    }

    const int imbalance = leftTotal - rightTotal;
    if (imbalance != 0) {
        const bool candidateHelpsBalance =
            (imbalance > 0 && *side > 0) || (imbalance < 0 && *side < 0);
        const double magnitude = static_cast<double>(std::abs(imbalance));
        if (candidateHelpsBalance) {
            score += std::min(0.45, 0.08 * magnitude);
        } else {
            score -= std::min(0.24, 0.04 * magnitude);
        }
    }

    return score;
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
    const double beatPosition = static_cast<double>(time - timingBase) / beatLength;
    const double nearest = std::round(beatPosition);
    return std::abs(beatPosition - nearest) <= 0.04;
}

bool hasSameTimeCollision(LaneMask occupiedLanes, int lane) {
    if (lane < 0 || lane >= kMaxKeyCount) {
        return false;
    }
    return (occupiedLanes & (LaneMask{1} << lane)) != 0;
}

void occupyLane(LaneMask& occupiedLanes, int lane) {
    if (lane < 0 || lane >= kMaxKeyCount) {
        return;
    }
    occupiedLanes = static_cast<LaneMask>(occupiedLanes | (LaneMask{1} << lane));
}

bool hasLongNoteConflictWithPlaced(const PlacedNote& existing, const Note& note, int lane) {
    if (existing.lane != lane) {
        return false;
    }
    if (existing.type == NoteType::Hold && existing.endTime.has_value() &&
        note.time > existing.time && note.time <= *existing.endTime) {
        return true;
    }
    if (note.type == NoteType::Hold && note.endTime.has_value()) {
        const bool existingStartsInside = existing.time > note.time && existing.time <= *note.endTime;
        const bool holdOverlap = existing.type == NoteType::Hold && existing.endTime.has_value() &&
                                 note.time <= *existing.endTime && *note.endTime >= existing.time;
        return existingStartsInside || holdOverlap;
    }
    return false;
}

std::optional<PlacedNote> lastPlacedForSource(const LastBySource& lastBySource, int sourceLane) {
    if (sourceLane < 0 || sourceLane >= kMaxKeyCount) {
        return std::nullopt;
    }
    return lastBySource[static_cast<std::size_t>(sourceLane)];
}

void rememberLastBySource(LastBySource& lastBySource, int sourceLane, const PlacedNote& placedNote) {
    if (sourceLane < 0 || sourceLane >= kMaxKeyCount) {
        return;
    }
    lastBySource[static_cast<std::size_t>(sourceLane)] = placedNote;
}

bool isImmediateSourceJackContinuation(const std::vector<PlacedNote>& placed,
                                       int sourceLane,
                                       int time,
                                       int jackWindowMs) {
    for (auto it = placed.rbegin(); it != placed.rend(); ++it) {
        const int dt = time - it->time;
        if (dt <= 0) {
            continue;
        }
        if (dt > jackWindowMs) {
            return false;
        }
        return it->sourceLane == sourceLane;
    }
    return false;
}

struct PlacementSafety {
    LaneMask conflicts = 0;
    LaneMask createdJacks = 0;
    LaneMask futureCreatedJacks = 0;
    LaneMask sourceJackLanes = 0;

    LaneMask blockedLanes(LaneMask occupiedLanes, bool sourceJackContinuation) const {
        return occupiedLanes | conflicts | futureCreatedJacks |
               (createdJacks & ~(sourceJackContinuation ? sourceJackLanes : LaneMask{0}));
    }
};

PlacementSafety buildPlacementSafety(const std::vector<PlacedNote>& placed,
                                     const Note& note,
                                     int sourceLane,
                                     int jackWindowMs) {
    PlacementSafety safety;
    std::array<const PlacedNote*, kMaxKeyCount> previousByLane{};
    // All candidates share the same history. Scan it once instead of once per
    // candidate. Rolls can put history on either side of this timestamp, so
    // insertion order cannot bound the time window.
    for (const auto& existing : placed) {
        if (existing.lane >= 0 && existing.lane < kMaxKeyCount && existing.time < note.time) {
            auto& previous = previousByLane[static_cast<std::size_t>(existing.lane)];
            if (previous == nullptr || previous->time < existing.time) previous = &existing;
        }
        if (existing.time == note.time ||
            hasLongNoteConflictWithPlaced(existing, note, existing.lane)) {
            occupyLane(safety.conflicts, existing.lane);
        }
        const auto dt = static_cast<long long>(note.time) - existing.time;
        if (dt != 0 && dt >= -jackWindowMs && dt <= jackWindowMs &&
            existing.sourceLane != sourceLane) {
            occupyLane(safety.createdJacks, existing.lane);
            // Continuing a past source jack never permits a new jack into an
            // already placed future note from a different source lane.
            if (dt < 0) {
                occupyLane(safety.futureCreatedJacks, existing.lane);
            }
        }
    }
    // A source jack can bypass an older different-source head only on the
    // lane where its immediate predecessor actually belongs to that source.
    // A matching note elsewhere in the chart must not unlock unrelated lanes.
    for (int lane = 0; lane < kMaxKeyCount; ++lane) {
        const auto* previous = previousByLane[static_cast<std::size_t>(lane)];
        if (previous != nullptr && previous->sourceLane == sourceLane) {
            occupyLane(safety.sourceJackLanes, lane);
        }
    }
    return safety;
}

MotifKind classifyMotif(const Note& note,
                        bool singleNoteSlice,
                        bool fastSingleContinuation,
                        bool sourceJackContinuation,
                        const std::vector<RecentSingleNote>& recentSingles) {
    if (sourceJackContinuation) {
        return MotifKind::Jack;
    }
    if (note.type == NoteType::Hold) {
        return MotifKind::LnAnchor;
    }
    if (!singleNoteSlice) {
        return MotifKind::Chord;
    }
    if (!fastSingleContinuation || recentSingles.empty()) {
        return MotifKind::Neutral;
    }

    const int sourceLane = sourceLaneOf(note);
    if (recentSingles.size() >= 3) {
        const auto& a = recentSingles[recentSingles.size() - 3];
        const auto& b = recentSingles[recentSingles.size() - 2];
        const auto& c = recentSingles[recentSingles.size() - 1];
        if (a.sourceLane == c.sourceLane && b.sourceLane == sourceLane &&
            a.sourceLane != b.sourceLane && note.time - a.time <= 900) {
            return MotifKind::Trill;
        }
    }

    if (recentSingles.size() >= 2) {
        const auto& previous = recentSingles.back();
        const auto& beforePrevious = recentSingles[recentSingles.size() - 2];
        const int previousDelta = previous.sourceLane - beforePrevious.sourceLane;
        const int currentDelta = sourceLane - previous.sourceLane;
        if (sameDirection(previousDelta, currentDelta) && note.time - beforePrevious.time <= 900) {
            return MotifKind::Stair;
        }
    }

    if (recentSingles.size() >= 3 && note.time - recentSingles[recentSingles.size() - 3].time <= 900) {
        return MotifKind::Stream;
    }
    return MotifKind::Stream;
}

void countMotifPlacement(NK2Report& report, MotifKind motif) {
    switch (motif) {
        case MotifKind::Jack:
            ++report.motifJackPlacements;
            break;
        case MotifKind::Trill:
            ++report.motifTrillPlacements;
            break;
        case MotifKind::Stair:
            ++report.motifStairPlacements;
            break;
        case MotifKind::Stream:
            ++report.motifStreamPlacements;
            break;
        case MotifKind::Chord:
            ++report.motifChordPlacements;
            break;
        case MotifKind::LnAnchor:
            ++report.motifLnPlacements;
            break;
        case MotifKind::Neutral:
            ++report.motifNeutralPlacements;
            break;
    }
}

std::string motifSlug(MotifKind motif) {
    switch (motif) {
        case MotifKind::Jack:
            return "jack";
        case MotifKind::Trill:
            return "trill";
        case MotifKind::Stair:
            return "stair";
        case MotifKind::Stream:
            return "stream";
        case MotifKind::Chord:
            return "chord";
        case MotifKind::LnAnchor:
            return "ln";
        case MotifKind::Neutral:
            return "neutral";
    }
    return "neutral";
}

void countGeneratedProvenance(NK2Report& report, MotifKind motif) {
    switch (motif) {
        case MotifKind::Jack:
            ++report.generatedFromJackMotif;
            break;
        case MotifKind::Trill:
            ++report.generatedFromTrillMotif;
            break;
        case MotifKind::Stair:
            ++report.generatedFromStairMotif;
            break;
        case MotifKind::Stream:
            ++report.generatedFromStreamMotif;
            break;
        case MotifKind::Chord:
            ++report.generatedFromChordMotif;
            break;
        case MotifKind::LnAnchor:
            ++report.generatedFromLnMotif;
            break;
        case MotifKind::Neutral:
            ++report.generatedFromNeutralMotif;
            break;
    }
}

double motifScoreAdjustment(MotifKind motif,
                            const Note& note,
                            int lane,
                            int direct,
                            int sourceLane,
                            const NK2Options& options,
                            const LaneUseContext& laneContext,
                            const std::optional<int>& previousSingleSourceLane,
                            const std::optional<int>& previousSingleTargetLane) {
    double score = 0.0;
    const bool hasPrevious = previousSingleSourceLane.has_value() && previousSingleTargetLane.has_value();
    const int sourceDelta = hasPrevious ? sourceLane - *previousSingleSourceLane : 0;
    const int targetDelta = hasPrevious ? lane - *previousSingleTargetLane : 0;
    const double longHoldSpreadScale = isLongHoldForAdjacentCopy(note) ? 0.25 : 1.0;

    switch (motif) {
        case MotifKind::Jack:
            score += lane == direct ? 0.25 : -0.20;
            break;
        case MotifKind::Trill:
            if (hasPrevious) {
                if (targetDelta == 0) {
                    score -= 1.30;
                } else {
                    score += 0.65;
                    if (std::abs(targetDelta) <= 3) {
                        score += 0.25;
                    } else {
                        score -= 0.20;
                    }
                }
            }
            if (laneInSourcePanel(
                    sourceLane, lane, options.sourceKeyCount, options.targetKeyCount)) {
                score += 0.10;
            }
            break;
        case MotifKind::Stair:
            if (hasPrevious && sourceDelta != 0) {
                if (targetDelta != 0 && sameDirection(sourceDelta, targetDelta)) {
                    score += 0.75;
                    if (options.superSymmetry && std::abs(sourceDelta) == 1) {
                        score += std::abs(targetDelta) == 1 ? 0.95 : -0.55;
                    } else if (std::abs(targetDelta) >= std::abs(sourceDelta)) {
                        score += 0.30;
                    }
                } else {
                    score -= 0.90;
                }
            }
            break;
        case MotifKind::Stream:
            if (hasPrevious) {
                if (targetDelta == 0) {
                    score -= 0.80;
                } else {
                    score += 0.25;
                    if (std::abs(targetDelta) <= 4) {
                        score += 0.15;
                    } else {
                        score -= 0.10;
                    }
                }
                if (sourceDelta != 0 && targetDelta != 0 && sameDirection(sourceDelta, targetDelta)) {
                    score += 0.20;
                    if (options.superSymmetry && std::abs(sourceDelta) == 1) {
                        score += std::abs(targetDelta) == 1 ? 0.75 : -0.35;
                    }
                }
            }
            score += 0.35 * laneCoverageNeedScoreFast(laneContext, lane);
            break;
        case MotifKind::Chord:
            score += laneInSourcePanel(
                         sourceLane, lane, options.sourceKeyCount, options.targetKeyCount) ? 0.35 : -0.35;
            if (laneInBridge(lane, options.targetKeyCount) &&
                laneSideFor(buildKeyLayoutProfile(options.sourceKeyCount), sourceLane) == LaneSide::Center) {
                score += 0.20;
            }
            break;
        case MotifKind::LnAnchor:
            score += lane == direct ? 0.05 * genericTargetAnchorLockMultiplier(options.targetKeyCount) : 0.0;
            score += 0.45 * longHoldSpreadScale * laneCoverageNeedScoreFast(laneContext, lane);
            score += 0.20 * longHoldSpreadScale * panelLaneNeedScoreFast(laneContext, lane);
            if (!laneInSourcePanel(
                    sourceLane, lane, options.sourceKeyCount, options.targetKeyCount)) {
                score += 0.10 * longHoldSpreadScale * laneCoverageNeedScoreFast(laneContext, lane);
            }
            if (laneInBridge(lane, options.targetKeyCount)) {
                score += 0.05;
            }
            break;
        case MotifKind::Neutral:
            score += lane == direct ? 0.02 * genericTargetAnchorLockMultiplier(options.targetKeyCount) : 0.0;
            score += 0.45 * laneCoverageNeedScoreFast(laneContext, lane);
            if (!laneInSourcePanel(
                    sourceLane, lane, options.sourceKeyCount, options.targetKeyCount)) {
                score += 0.10 * laneCoverageNeedScoreFast(laneContext, lane);
            }
            break;
    }

    if (note.type == NoteType::Hold && motif != MotifKind::LnAnchor) {
        score += lane == direct ? 0.10 * genericTargetAnchorLockMultiplier(options.targetKeyCount) : 0.0;
    }
    return score;
}

CandidateList rankedCandidates(const Note& note,
                                const NK2Options& options,
                                const std::vector<int>& laneUse,
                                const LastBySource& lastBySource,
                                const std::vector<PlacedNote>& placed,
                                const std::optional<int>& previousSingleSourceLane,
                                const std::optional<int>& previousSingleTargetLane,
                                bool sourceJackContinuation,
                                MotifKind motif,
                                LaneMask createdJackLanes) {
    const int sourceLane = sourceLaneOf(note);
    CandidateList ranked;
    const auto pool = candidatePoolFor(sourceLane, options.sourceKeyCount, options.targetKeyCount);
    const auto laneContext = buildLaneUseContext(laneUse,
                                                 options.sourceKeyCount,
                                                 options.targetKeyCount,
                                                 options.layoutWeights);
    const int direct = directLane(sourceLane, options.sourceKeyCount, options.targetKeyCount);
    const double totalBlend = std::max(0.001, options.nativeWeight + options.remixWeight);
    const double freedomMultiplier = genericTargetFreedomMultiplier(options.targetKeyCount);
    const double freedomBoost = freedomMultiplier - 1.0;
    const double anchorLockScale =
        genericTargetAnchorLockMultiplier(options.targetKeyCount) / freedomMultiplier;
    const bool sameKeyTransform = options.mode == Mode::Transform &&
                                  options.sourceKeyCount == options.targetKeyCount;
    const bool longHoldCopy = isLongHoldForAdjacentCopy(note);
    const double longHoldSpreadScale = longHoldCopy ? 0.25 : 1.0;
    LanePool adjacentCopyLanes;
    if (longHoldCopy) {
        adjacentCopyLanes = adjacentCopyLaneCandidates(note, options, lastBySource);
    }

    std::optional<int> sourceJackLane;
    if (sourceJackContinuation) {
        const auto last = lastPlacedForSource(lastBySource, sourceLane);
        if (last.has_value()) {
            sourceJackLane = last->lane;
        }
    }

    for (int poolIndex = 0; poolIndex < pool.count; ++poolIndex) {
        const int lane = pool.lanes[static_cast<std::size_t>(poolIndex)];
        const bool freeOriginalTap =
            note.type == NoteType::Tap && (motif == MotifKind::Neutral || motif == MotifKind::Stream);
        const double distance = std::abs(lane - direct);
        const double fieldScale = std::max(1.0, static_cast<double>(options.targetKeyCount) / 10.0);
        const double nativeDistance =
            (freeOriginalTap ? 7.0 : 4.0) * fieldScale * freedomMultiplier;
        const double rawNativeScore = std::max(0.0, 1.0 - distance / nativeDistance);
        const double nativeAnchorContrast = options.targetKeyCount == 8 ? 0.55 : 1.0;
        const double nativeScore = 0.5 + (rawNativeScore - 0.5) * nativeAnchorContrast;
        const double remixScore = remixScoreForLaneFast(sourceLane, lane, laneContext);
        const double coverageNeed = laneCoverageNeedScoreFast(laneContext, lane);
        const double panelNeed = panelLaneNeedScoreFast(laneContext, lane);
        const double wholeFieldNeed = wholeFieldNeedScoreFast(laneContext, lane);
        double score = (options.nativeWeight * nativeScore + options.remixWeight * remixScore) / totalBlend;
        score += (options.remixWeight / totalBlend) * (freeOriginalTap ? 1.35 : 1.0) * coverageNeed;
        score += (options.remixWeight / totalBlend) * freedomBoost *
                 (freeOriginalTap ? 2.35 : 1.55) * wholeFieldNeed * longHoldSpreadScale;
        if (longHoldCopy) {
            score += adjacentCopyPreferenceScore(lane, adjacentCopyLanes);
        }
        if (sameKeyTransform && !sourceJackContinuation) {
            const int mirror = options.targetKeyCount - 1 - direct;
            score += lane == direct ? -0.90 : 0.0;
            score += lane == mirror ? 0.55 : 0.0;
            score += distance == 1.0 ? 0.20 : 0.0;
        }
        if (laneInSourcePanel(
                sourceLane, lane, options.sourceKeyCount, options.targetKeyCount)) {
            score += (options.nativeWeight / totalBlend) * (freeOriginalTap ? 0.10 : 0.35) *
                     panelNeed * anchorLockScale;
            score += (options.remixWeight / totalBlend) * (freeOriginalTap ? 0.35 : 0.70) *
                     panelNeed;
        }

        const bool freeLnAnchor = motif == MotifKind::LnAnchor;
        const bool freerAnchor = freeLnAnchor || freeOriginalTap;
        if (lane == direct) {
            const double anchorPreference = std::min(
                laneCoveragePreferenceFast(laneContext, lane),
                0.5 + 0.5 * panelLaneNeedScoreFast(laneContext, lane));
            const double anchorScale = clamp01(0.10 + 0.90 * anchorPreference);
            score += (freerAnchor ? 0.10 : 0.45) * anchorScale * anchorLockScale;
            if (options.targetKeyCount == 8 && !sourceJackContinuation && motif != MotifKind::Jack) {
                score -= freerAnchor ? 0.16 : 0.09;
            }
        }
        if (laneInSourcePanel(
                sourceLane, lane, options.sourceKeyCount, options.targetKeyCount)) {
            score += (freerAnchor ? 0.03 : 0.15) * anchorLockScale;
        }
        if (note.type == NoteType::Hold && laneInBridge(lane, options.targetKeyCount)) {
            score += freeLnAnchor ? 0.03 : 0.08;
        }
        if (sourceJackLane.has_value()) {
            score += lane == *sourceJackLane ? 2.0 : -1.0;
        }
        if (previousSingleSourceLane.has_value() && previousSingleTargetLane.has_value()) {
            const int sourceDelta = sourceLane - *previousSingleSourceLane;
            const int targetDelta = lane - *previousSingleTargetLane;
            if (sourceDelta != 0 && targetDelta != 0 && (sourceDelta > 0) == (targetDelta > 0)) {
                score += 0.35;
                if (std::abs(targetDelta) >= std::abs(sourceDelta)) {
                    score += 0.12;
                }
            } else if (sourceDelta != 0) {
                score -= 0.50;
            }
        }
        if (hasSameTimeCollision(createdJackLanes, lane)) {
            score -= 1.5;
        }
        score += motifScoreAdjustment(motif,
                                      note,
                                      lane,
                                      direct,
                                      sourceLane,
                                      options,
                                      laneContext,
                                      previousSingleSourceLane,
                                      previousSingleTargetLane);
        score += mirrorCadenceScoreAdjustment(lane,
                                              options,
                                              laneContext,
                                              placed,
                                              freeOriginalTap,
                                              sourceJackContinuation);
        score += superSymmetryScoreAdjustment(note,
                                              lane,
                                              options,
                                              placed,
                                              previousSingleSourceLane,
                                              previousSingleTargetLane);
        addCandidate(ranked, lane, score);
    }

    sortCandidates(ranked);
    return ranked;
}

std::optional<int> chooseLaneStrict(const Note& note,
                                    const NK2Options& options,
                                    LaneMask occupiedLanes,
                                    const std::vector<int>& laneUse,
                                    const LastBySource& lastBySource,
                                    const std::vector<PlacedNote>& placed,
                                    const std::optional<int>& previousSingleSourceLane,
                                    const std::optional<int>& previousSingleTargetLane,
                                    MotifKind motif) {
    const int sourceLane = sourceLaneOf(note);
    const bool sourceJackContinuation =
        isImmediateSourceJackContinuation(placed, sourceLane, note.time, 500);
    const auto safety = buildPlacementSafety(placed, note, sourceLane, 500);
    const auto blockedLanes = safety.blockedLanes(occupiedLanes, sourceJackContinuation);
    const auto ranked = rankedCandidates(note,
                                         options,
                                         laneUse,
                                         lastBySource,
                                         placed,
                                         previousSingleSourceLane,
                                         previousSingleTargetLane,
                                         sourceJackContinuation,
                                         sourceJackContinuation ? MotifKind::Jack : motif,
                                         safety.createdJacks);
    for (const auto& candidate : ranked) {
        if (hasSameTimeCollision(blockedLanes, candidate.lane)) {
            continue;
        }
        return candidate.lane;
    }
    return std::nullopt;
}

struct SliceBeamState {
    std::vector<SolvedPlacement> solved;
    LaneMask occupiedLanes = 0;
    std::vector<int> laneUse;
    std::vector<PlacedNote> placed;
    LastBySource lastBySource;
    double score = 0.0;
};

std::optional<std::vector<SolvedPlacement>> solveSliceWithLocalBeam(const Chart& chart,
                                                                    const SliceInfo& slice,
                                                                    const NK2Options& options,
                                                                    const std::vector<int>& laneUse,
                                                                    const LastBySource& lastBySource,
                                                                    const std::vector<PlacedNote>& placed,
                                                                    NK2Report& report) {
    if (slice.noteIndices.size() <= 1 || slice.noteIndices.size() > kMaxKeyCount) {
        return std::nullopt;
    }

    ++report.localSolverWindows;
    std::vector<SliceBeamState> beam;
    SliceBeamState initial;
    initial.laneUse = laneUse;
    initial.placed = placed;
    initial.lastBySource = lastBySource;
    beam.push_back(std::move(initial));

    constexpr int kCandidateLimitPerState = 8;
    constexpr std::size_t kBeamWidth = 16;

    for (const auto noteIndex : slice.noteIndices) {
        const auto& sourceNote = chart.notes[noteIndex];
        std::vector<SliceBeamState> nextBeam;

        for (const auto& state : beam) {
            Note note = sourceNote;
            const int sourceLane = sourceLaneOf(note);
            const bool sourceJackContinuation =
                isImmediateSourceJackContinuation(state.placed, sourceLane, note.time, 500);
            const auto motif = classifyMotif(note, false, false, sourceJackContinuation, {});
            const auto safety = buildPlacementSafety(state.placed, note, sourceLane, 500);
            const auto blockedLanes = safety.blockedLanes(state.occupiedLanes, sourceJackContinuation);
            const auto ranked = rankedCandidates(note,
                                                 options,
                                                 state.laneUse,
                                                 state.lastBySource,
                                                 state.placed,
                                                 std::nullopt,
                                                 std::nullopt,
                                                 sourceJackContinuation,
                                                 sourceJackContinuation ? MotifKind::Jack : motif,
                                                 safety.createdJacks);

            int acceptedCandidates = 0;
            for (const auto& candidate : ranked) {
                if (hasSameTimeCollision(blockedLanes, candidate.lane)) {
                    continue;
                }

                SliceBeamState next = state;
                note.sourceLane = sourceLane;
                note.lane = candidate.lane;
                next.solved.push_back({note, sourceJackContinuation ? MotifKind::Jack : motif});
                occupyLane(next.occupiedLanes, candidate.lane);
                if (candidate.lane >= 0 && candidate.lane < options.targetKeyCount) {
                    ++next.laneUse[static_cast<std::size_t>(candidate.lane)];
                }

                PlacedNote placedNote{note.time, note.lane, sourceLane, note.type, note.endTime};
                next.placed.push_back(placedNote);
                rememberLastBySource(next.lastBySource, sourceLane, placedNote);
                next.score += candidate.score;
                nextBeam.push_back(std::move(next));
                ++report.localSolverCandidates;

                if (++acceptedCandidates >= kCandidateLimitPerState) {
                    break;
                }
            }
        }

        if (nextBeam.empty()) {
            ++report.localSolverFallbacks;
            return std::nullopt;
        }

        std::stable_sort(nextBeam.begin(), nextBeam.end(), [](const SliceBeamState& lhs, const SliceBeamState& rhs) {
            if (lhs.score != rhs.score) {
                return lhs.score > rhs.score;
            }
            return lhs.solved.size() > rhs.solved.size();
        });
        if (nextBeam.size() > kBeamWidth) {
            nextBeam.resize(kBeamWidth);
        }
        beam = std::move(nextBeam);
    }

    if (beam.empty()) {
        ++report.localSolverFallbacks;
        return std::nullopt;
    }
    return beam.front().solved;
}

std::optional<SolvedPlacement> tryRollLowerKeyOverflowTap(const Note& original,
                                                          const NK2Options& options,
                                                          const std::vector<int>& laneUse,
                                                          const LastBySource& lastBySource,
                                                          const std::vector<PlacedNote>& placed,
                                                          MotifKind motif) {
    if (options.targetKeyCount >= options.sourceKeyCount || original.type != NoteType::Tap) {
        return std::nullopt;
    }

    constexpr int kRollOffsets[] = {16, -16, 32, -32, 48, -48, 64, -64, 96, -96};
    const int sourceLane = sourceLaneOf(original);
    for (const int offset : kRollOffsets) {
        Note rolled = original;
        rolled.time = std::max(0, original.time + offset);
        const bool sourceJackContinuation =
            isImmediateSourceJackContinuation(placed, sourceLane, rolled.time, 500);
        const auto safety = buildPlacementSafety(placed, rolled, sourceLane, 500);
        const auto blockedLanes = safety.blockedLanes(0, sourceJackContinuation);
        const auto ranked = rankedCandidates(rolled,
                                             options,
                                             laneUse,
                                             lastBySource,
                                             placed,
                                             std::nullopt,
                                             std::nullopt,
                                             sourceJackContinuation,
                                             sourceJackContinuation ? MotifKind::Jack : motif,
                                             safety.createdJacks);
        for (const auto& candidate : ranked) {
            if (hasSameTimeCollision(blockedLanes, candidate.lane)) {
                continue;
            }
            rolled.sourceLane = sourceLane;
            rolled.lane = candidate.lane;
            return SolvedPlacement{rolled, sourceJackContinuation ? MotifKind::Jack : motif};
        }
    }
    return std::nullopt;
}

PlacementStats collectPlacementStats(const Chart& converted,
                                     int sourceKeyCount,
                                     int targetKeyCount,
                                     int createdJackWindowMs) {
    PlacementStats stats;
    stats.laneDistribution.assign(static_cast<std::size_t>(std::max(0, targetKeyCount)), 0);
    std::array<std::vector<PlacedNote>, kMaxKeyCount> placedByLane;
    for (const auto& note : converted.notes) {
        if (note.lane >= 0 && note.lane < targetKeyCount) {
            ++stats.laneDistribution[static_cast<std::size_t>(note.lane)];
        }
        const int sourceLane = sourceLaneOf(note);
        if (!isNk2GeneratedNote(note) && sourceLane >= 0 && sourceLane < sourceKeyCount) {
            ++stats.sourceAnchorTotal;
            if (note.lane == directLane(sourceLane, sourceKeyCount, targetKeyCount)) {
                ++stats.sourceAnchorMatches;
            }
        }
        if (note.lane >= 0 && note.lane < kMaxKeyCount) {
            auto& lanePlaced = placedByLane[static_cast<std::size_t>(note.lane)];
            for (const auto& existing : lanePlaced) {
                if (existing.time == note.time) {
                    ++stats.sameTimeCollisions;
                }
                if (hasLongNoteConflictWithPlaced(existing, note, note.lane)) {
                    ++stats.longNoteConflicts;
                }
            }
            lanePlaced.push_back({note.time, note.lane, sourceLane, note.type, note.endTime});
        }
    }

    std::vector<const Note*> convertedSorted;
    convertedSorted.reserve(converted.notes.size());
    for (const auto& note : converted.notes) {
        convertedSorted.push_back(&note);
    }
    std::stable_sort(convertedSorted.begin(), convertedSorted.end(), [](const Note* lhs, const Note* rhs) {
        if (lhs->time != rhs->time) {
            return lhs->time < rhs->time;
        }
        if (lhs->lane != rhs->lane) {
            return lhs->lane < rhs->lane;
        }
        return lhs->id < rhs->id;
    });

    for (std::size_t i = 1; i < convertedSorted.size(); ++i) {
        const auto* previous = convertedSorted[i - 1];
        const auto* current = convertedSorted[i];
        const int dt = current->time - previous->time;
        if (dt <= 0 || dt > createdJackWindowMs || current->lane != previous->lane) {
            continue;
        }
        const int previousSource = sourceLaneOf(*previous);
        const int currentSource = sourceLaneOf(*current);
        if (previousSource == currentSource) {
            ++stats.preservedSourceJacks;
        } else {
            ++stats.createdJacks;
        }
    }
    return stats;
}

double laneEntropyScore(const std::vector<int>& distribution) {
    const int total = std::accumulate(distribution.begin(), distribution.end(), 0);
    if (total <= 0 || distribution.size() <= 1) {
        return 0.0;
    }

    double entropy = 0.0;
    for (const int count : distribution) {
        if (count <= 0) {
            continue;
        }
        const double p = static_cast<double>(count) / static_cast<double>(total);
        entropy -= p * std::log(p);
    }
    return entropy / std::log(static_cast<double>(distribution.size()));
}

double handPanelBalanceScore(const std::vector<int>& distribution) {
    const int total = std::accumulate(distribution.begin(), distribution.end(), 0);
    if (total <= 0 || distribution.empty()) {
        return 0.0;
    }
    const auto layout = buildKeyLayoutProfile(static_cast<int>(distribution.size()));
    int left = 0;
    int right = 0;
    for (int lane = 0; lane < static_cast<int>(distribution.size()); ++lane) {
        const auto side = laneSideFor(layout, lane);
        if (side == LaneSide::Left) {
            left += distribution[static_cast<std::size_t>(lane)];
        } else if (side == LaneSide::Right) {
            right += distribution[static_cast<std::size_t>(lane)];
        }
    }
    const int sideTotal = left + right;
    return sideTotal <= 0
               ? 0.0
               : clamp01(1.0 - static_cast<double>(std::abs(left - right)) /
                                   static_cast<double>(sideTotal));
}

double panelSpreadFitScore(const std::vector<int>& distribution, int start, int end) {
    if (distribution.empty() || start < 0 || end < start ||
        end >= static_cast<int>(distribution.size())) {
        return 0.0;
    }

    int panelTotal = 0;
    for (int lane = start; lane <= end; ++lane) {
        panelTotal += distribution[static_cast<std::size_t>(lane)];
    }
    if (panelTotal <= 0) {
        return 0.0;
    }

    const int width = std::max(1, end - start + 1);
    double distance = 0.0;
    for (int lane = start; lane <= end; ++lane) {
        const double actual = static_cast<double>(distribution[static_cast<std::size_t>(lane)]) /
                              static_cast<double>(panelTotal);
        const double desired = 1.0 / static_cast<double>(width);
        distance += std::abs(actual - desired);
    }
    return clamp01(1.0 - 0.5 * distance);
}

double bridgeFitScore(const std::vector<int>& distribution, const LayoutWeights& weights) {
    const int targetKeyCount = static_cast<int>(distribution.size());
    const int total = std::accumulate(distribution.begin(), distribution.end(), 0);
    if (targetKeyCount <= 0 || total <= 0) {
        return 0.0;
    }

    const auto layout = buildKeyLayoutProfile(targetKeyCount, weights);
    if (!layout.hasBridge) {
        return 0.0;
    }
    double desiredBridge = 0.0;
    int actualBridge = 0;
    for (int lane = 0; lane < targetKeyCount; ++lane) {
        if (laneIsInBridge(layout, lane)) {
            desiredBridge += desiredLaneShare(lane, targetKeyCount, weights);
            actualBridge += distribution[static_cast<std::size_t>(lane)];
        }
    }
    if (desiredBridge <= 0.0) {
        return 0.0;
    }

    const double actual = static_cast<double>(actualBridge) / static_cast<double>(total);
    return clamp01(1.0 - std::abs(actual - desiredBridge) / desiredBridge);
}

double layoutCoverageFitScore(const std::vector<int>& distribution, const LayoutWeights& weights) {
    const int targetKeyCount = static_cast<int>(distribution.size());
    const int total = std::accumulate(distribution.begin(), distribution.end(), 0);
    if (targetKeyCount <= 0 || total <= 0) {
        return 0.0;
    }

    double distance = 0.0;
    for (int lane = 0; lane < targetKeyCount; ++lane) {
        const double actual = static_cast<double>(distribution[static_cast<std::size_t>(lane)]) /
                              static_cast<double>(total);
        const double desired = desiredLaneShare(lane, targetKeyCount, weights);
        distance += std::abs(actual - desired);
    }
    return clamp01(1.0 - 0.5 * distance);
}

void fillLayoutScores(NK2Report& report) {
    report.panelScore = handPanelBalanceScore(report.laneDistribution);
    const auto layout = buildKeyLayoutProfile(
        static_cast<int>(report.laneDistribution.size()), report.options.layoutWeights);
    if (layout.hasPanels) {
        report.leftPanelScore = panelSpreadFitScore(
            report.laneDistribution, layout.leftStart, layout.leftEnd);
        report.rightPanelScore = panelSpreadFitScore(
            report.laneDistribution, layout.rightStart, layout.rightEnd);
    }
    report.bridgeScore = bridgeFitScore(report.laneDistribution, report.options.layoutWeights);
    report.fullFieldScore = laneEntropyScore(report.laneDistribution);
    report.layoutCoverageScore = layoutCoverageFitScore(report.laneDistribution, report.options.layoutWeights);
}

int supportPhraseWindowForTime(int time);
int phraseSupportBudgetFor(const NK2Options& options, int sourceNoteCount);

void fillPhraseProfileScores(NK2Report& report, const Chart& converted) {
    std::map<int, int> sourceNotesByPhrase;
    std::map<int, int> supportNotesByPhrase;
    for (const auto& note : converted.notes) {
        const int window = supportPhraseWindowForTime(note.time);
        if (isNk2GeneratedNote(note)) {
            ++supportNotesByPhrase[window];
        } else {
            ++sourceNotesByPhrase[window];
        }
    }

    std::set<int> windows;
    for (const auto& [window, count] : sourceNotesByPhrase) {
        (void)count;
        windows.insert(window);
    }
    for (const auto& [window, count] : supportNotesByPhrase) {
        (void)count;
        windows.insert(window);
    }

    report.phraseProfileWindows = static_cast<int>(windows.size());
    report.phraseProfileOverBudgetWindows = 0;
    if (windows.empty()) {
        report.phraseProfileScore = 0.0;
        return;
    }

    double scoreSum = 0.0;
    for (const int window : windows) {
        const auto sourceCount = sourceNotesByPhrase.find(window);
        const auto supportCount = supportNotesByPhrase.find(window);
        const int sourceNotes = sourceCount == sourceNotesByPhrase.end() ? 0 : sourceCount->second;
        const int supportNotes = supportCount == supportNotesByPhrase.end() ? 0 : supportCount->second;
        const int budget = phraseSupportBudgetFor(report.options, sourceNotes);
        if (supportNotes > budget) {
            ++report.phraseProfileOverBudgetWindows;
            const int overflow = supportNotes - budget;
            scoreSum += clamp01(1.0 - static_cast<double>(overflow) /
                                          static_cast<double>(std::max(1, supportNotes)));
        } else {
            scoreSum += 1.0;
        }
    }
    report.phraseProfileScore = clamp01(scoreSum / static_cast<double>(windows.size()));
}

void fillDistributionAndSafety(NK2Report& report, const Chart& converted) {
    report.outputNotes = static_cast<int>(converted.notes.size());
    // The opt-in overlay reports actual additions even if the B placement had
    // to drop a source note. Preserve the legacy report convention elsewhere.
    report.addedNotes = report.options.phraseSupport
        ? report.lnSupportAccepted + report.mirrorSupportAccepted + report.strongBeatSupportAccepted +
              report.denseTapAccepted + report.denseHoldAccepted
        : std::max(0, report.outputNotes - report.intent.totalNotes);
    report.addedTapNotes = 0;
    report.addedHoldNotes = 0;
    for (const auto& note : converted.notes) {
        if (isNk2GeneratedNote(note)) {
            if (note.type == NoteType::Hold) ++report.addedHoldNotes;
            else ++report.addedTapNotes;
        }
    }
    report.sameTimeCollisions = 0;
    report.longNoteConflicts = 0;
    report.createdJacks = 0;
    report.preservedSourceJacks = 0;
    report.sourceAnchorMatches = 0;
    report.sourceAnchorTotal = 0;
    report.sourceAnchorScore = 0.0;

    const auto stats = collectPlacementStats(converted,
                                             report.options.sourceKeyCount,
                                             report.options.targetKeyCount,
                                             supportJackWindowMsFor(report.options));
    report.sameTimeCollisions = stats.sameTimeCollisions;
    report.longNoteConflicts = stats.longNoteConflicts;
    report.createdJacks = stats.createdJacks;
    report.preservedSourceJacks = stats.preservedSourceJacks;
    report.sourceAnchorMatches = stats.sourceAnchorMatches;
    report.sourceAnchorTotal = stats.sourceAnchorTotal;
    if (report.sourceAnchorTotal > 0) {
        report.sourceAnchorScore = static_cast<double>(report.sourceAnchorMatches) /
                                   static_cast<double>(report.sourceAnchorTotal);
    }
    report.laneDistribution = stats.laneDistribution;
    fillLayoutScores(report);
    fillPhraseProfileScores(report, converted);
}

int supportBudgetFor(const NK2Options& options, int sourceNoteCount) {
    const bool fourToFiveFill = isFourToFiveFillOptions(options) && !options.phraseSupport;
    if (options.targetKeyCount <= options.sourceKeyCount ||
        (!fourToFiveFill && options.mode == Mode::Faithful && !options.phraseSupport) ||
        options.mode == Mode::Report) {
        return 0;
    }
    if (sourceNoteCount <= 0) {
        return 0;
    }
    if (fourToFiveFill) {
        return std::max(kFourToFiveFillMinimumBudget,
                        static_cast<int>(std::ceil(static_cast<double>(sourceNoteCount) *
                                                   kFourToFiveFillAddedRatio)));
    }
    const double ratio = options.mode == Mode::Harder ? 0.18 : 0.12;
    const int minimum = options.mode == Mode::Harder ? 2 : 1;
    return std::max(minimum, static_cast<int>(std::ceil(static_cast<double>(sourceNoteCount) * ratio)));
}

int supportPhraseWindowMs() {
    return 2000;
}

int supportPhraseWindowForTime(int time) {
    if (time < 0) {
        return 0;
    }
    return time / supportPhraseWindowMs();
}

int phraseSupportBudgetFor(const NK2Options& options, int sourceNoteCount) {
    if (options.phraseSourceLnBoost) {
        return clampInt(static_cast<int>(std::ceil(std::max(1, sourceNoteCount) * 0.50)), 1, 14);
    }
    if (options.phraseDenseLn) {
        return clampInt(static_cast<int>(std::ceil(std::max(1, sourceNoteCount) * 0.35)), 1, 10);
    }
    const bool fourToFiveFill = isFourToFiveFillOptions(options) && !options.phraseSupport;
    if (options.targetKeyCount <= options.sourceKeyCount ||
        (!fourToFiveFill && options.mode == Mode::Faithful && !options.phraseSupport) ||
        options.mode == Mode::Report) {
        return 0;
    }
    const int safeSourceNoteCount = std::max(1, sourceNoteCount);
    if (fourToFiveFill) {
        const int budget = std::max(
            kFourToFiveFillMinimumPhraseBudget,
            static_cast<int>(std::ceil(static_cast<double>(safeSourceNoteCount) *
                                       kFourToFiveFillPhraseRatio)));
        return clampInt(
            budget, kFourToFiveFillMinimumPhraseBudget, kFourToFiveFillMaximumPhraseBudget);
    }
    const double ratio = options.mode == Mode::Harder ? 0.18 : 0.12;
    const int minimum = options.mode == Mode::Harder ? 2 : 1;
    const int maximum = options.mode == Mode::Harder ? 6 : 4;
    const int budget = std::max(
        minimum, static_cast<int>(std::ceil(static_cast<double>(safeSourceNoteCount) * ratio)));
    return clampInt(budget, minimum, maximum);
}

std::map<int, int> sourceNoteCountsBySupportPhrase(const Chart& chart) {
    std::map<int, int> counts;
    for (const auto& note : chart.notes) {
        if (!isNk2GeneratedNote(note)) {
            ++counts[supportPhraseWindowForTime(note.time)];
        }
    }
    return counts;
}

bool oppositePanelSupport(int sourceLane,
                          int lane,
                          int sourceKeyCount,
                          int targetKeyCount) {
    const auto sourceLayout = buildKeyLayoutProfile(sourceKeyCount);
    const auto targetLayout = buildKeyLayoutProfile(targetKeyCount);
    const auto sourceSide = laneSideFor(sourceLayout, sourceLane);
    const auto targetSide = laneSideFor(targetLayout, lane);
    if (sourceSide == LaneSide::Center) {
        return targetSide == LaneSide::Center || laneIsInBridge(targetLayout, lane);
    }
    return (sourceSide == LaneSide::Left && targetSide == LaneSide::Right) ||
           (sourceSide == LaneSide::Right && targetSide == LaneSide::Left);
}

CandidateList rankedSupportLanes(const SupportEvent& event,
                                 const std::vector<int>& laneUse,
                                 const NK2Options& options) {
    CandidateList ranked;
    const auto laneContext = buildLaneUseContext(laneUse,
                                                 options.sourceKeyCount,
                                                 options.targetKeyCount,
                                                 options.layoutWeights);
    const int mirrorLane = options.targetKeyCount - 1 - event.anchorLane;
    auto pool = candidatePoolFor(event.sourceLane, options.sourceKeyCount, options.targetKeyCount);
    if (event.kind == SupportKind::Mirror) {
        addUnique(pool, mirrorLane, options.targetKeyCount);
        addUnique(pool, mirrorLane - 1, options.targetKeyCount);
        addUnique(pool, mirrorLane + 1, options.targetKeyCount);
    }
    for (int poolIndex = 0; poolIndex < pool.count; ++poolIndex) {
        const int lane = pool.lanes[static_cast<std::size_t>(poolIndex)];
        if (lane == event.anchorLane) {
            continue;
        }
        double score = remixScoreForLaneFast(event.sourceLane, lane, laneContext);
        score += 0.60 * laneCoverageNeedScoreFast(laneContext, lane);
        score += 0.45 * panelLaneNeedScoreFast(laneContext, lane);
        if (lane == mirrorLane) {
            score += 0.55;
        }
        if (oppositePanelSupport(event.sourceLane,
                                 lane,
                                 options.sourceKeyCount,
                                 options.targetKeyCount)) {
            score += 0.30;
        }
        if (event.kind == SupportKind::Mirror) {
            score += lane == mirrorLane ? 1.25 : 0.0;
            score += oppositePanelSupport(event.sourceLane,
                                          lane,
                                          options.sourceKeyCount,
                                          options.targetKeyCount) ? 0.60 : -0.20;
            score += laneInSourcePanel(event.sourceLane,
                                       lane,
                                       options.sourceKeyCount,
                                       options.targetKeyCount) ? -0.30 : 0.0;
        }
        if (event.kind == SupportKind::Ln && laneInBridge(lane, options.targetKeyCount)) {
            score += 0.20;
        }
        if (event.kind == SupportKind::StrongBeat &&
            laneInSourcePanel(event.sourceLane,
                              lane,
                              options.sourceKeyCount,
                              options.targetKeyCount)) {
            score += 0.10;
        }
        addCandidate(ranked, lane, score);
    }
    sortCandidates(ranked);
    return ranked;
}

std::map<std::string, MotifKind> buildSupportAnchorMotifs(const Chart& placed,
                                                          int sameTimeEpsilonMs) {
    std::map<std::string, MotifKind> motifs;
    const auto slices = buildSlices(placed, sameTimeEpsilonMs);
    std::vector<PlacedNote> placedNotes;
    std::vector<RecentSingleNote> recentSingles;
    std::optional<int> previousSingleTime;

    for (const auto& slice : slices) {
        const bool singleNoteSlice = slice.noteIndices.size() == 1;
        for (const auto noteIndex : slice.noteIndices) {
            const auto& note = placed.notes[noteIndex];
            const int sourceLane = sourceLaneOf(note);
            const bool fastSingleContinuation =
                singleNoteSlice && previousSingleTime.has_value() &&
                note.time > *previousSingleTime && note.time - *previousSingleTime <= 280;
            if (singleNoteSlice && !fastSingleContinuation) {
                recentSingles.clear();
            }

            const bool sourceJackContinuation =
                isImmediateSourceJackContinuation(placedNotes, sourceLane, note.time, 500);
            const auto motif = classifyMotif(note,
                                             singleNoteSlice,
                                             fastSingleContinuation,
                                             sourceJackContinuation,
                                             recentSingles);
            motifs[note.id] = motif;

            placedNotes.push_back({note.time, note.lane, sourceLane, note.type, note.endTime});
            if (singleNoteSlice) {
                previousSingleTime = note.time;
                recentSingles.push_back({note.time, sourceLane, note.lane});
                if (recentSingles.size() > 6) {
                    recentSingles.erase(recentSingles.begin());
                }
            }
        }

        if (!singleNoteSlice) {
            previousSingleTime.reset();
            recentSingles.clear();
        }
    }

    return motifs;
}

bool shouldEmitMirrorSupportEvent(const Note& note,
                                  MotifKind motif,
                                  int sourceKeyCount,
                                  int targetKeyCount) {
    if (targetKeyCount <= sourceKeyCount || note.type == NoteType::Hold) {
        return false;
    }
    if (motif == MotifKind::Jack || motif == MotifKind::Trill || motif == MotifKind::LnAnchor) {
        return false;
    }
    const int sourceLane = sourceLaneOf(note);
    const auto sourceLayout = buildKeyLayoutProfile(sourceKeyCount);
    return sourceLane >= 0 && sourceLane < sourceKeyCount &&
           laneSideFor(sourceLayout, sourceLane) != LaneSide::Center;
}

std::vector<SupportEvent> buildSupportEvents(const Chart& placed, const NK2Options& options) {
    std::vector<SupportEvent> events;
    std::set<std::pair<int, std::string>> emitted;
    const auto anchorMotifs = buildSupportAnchorMotifs(placed, 2);
    for (const auto& note : placed.notes) {
        const int sourceLane = sourceLaneOf(note);
        if (note.type == NoteType::Hold && note.endTime.has_value() && *note.endTime > note.time + 120) {
            for (const auto& endpoint : {std::make_pair(note.time, std::string("head")),
                                         std::make_pair(*note.endTime, std::string("tail"))}) {
                SupportEvent event;
                event.kind = SupportKind::Ln;
                event.anchorMotif = MotifKind::LnAnchor;
                event.time = endpoint.first;
                event.sourceLane = sourceLane;
                event.anchorLane = note.lane;
                event.anchorId = note.id + "-" + endpoint.second;
                if (emitted.insert({event.time, "ln:" + event.anchorId}).second) {
                    events.push_back(std::move(event));
                }
            }
        }
    }
    for (const auto& note : placed.notes) {
        if (!isStrongBeat(note.time, placed.timingPoints)) {
            continue;
        }
        SupportEvent event;
        const auto motif = anchorMotifs.find(note.id);
        const auto anchorMotif = motif == anchorMotifs.end() ? MotifKind::Neutral : motif->second;
        if (shouldEmitMirrorSupportEvent(note,
                                         anchorMotif,
                                         options.sourceKeyCount,
                                         options.targetKeyCount)) {
            event.kind = SupportKind::Mirror;
            event.anchorMotif = anchorMotif;
            event.time = note.time;
            event.sourceLane = sourceLaneOf(note);
            event.anchorLane = note.lane;
            event.anchorId = note.id;
            if (emitted.insert({event.time, "mirror:" + event.anchorId}).second) {
                events.push_back(event);
            }
        }
        event.kind = SupportKind::StrongBeat;
        event.anchorMotif = anchorMotif;
        event.time = note.time;
        event.sourceLane = sourceLaneOf(note);
        event.anchorLane = note.lane;
        event.anchorId = note.id;
        if (emitted.insert({event.time, "beat:" + event.anchorId}).second) {
            events.push_back(std::move(event));
        }
    }
    return events;
}

std::vector<int> laneDistributionFor(const Chart& chart, int targetKeyCount) {
    std::vector<int> distribution(static_cast<std::size_t>(std::max(0, targetKeyCount)), 0);
    for (const auto& note : chart.notes) {
        if (note.lane >= 0 && note.lane < targetKeyCount) {
            ++distribution[static_cast<std::size_t>(note.lane)];
        }
    }
    return distribution;
}

bool supportCandidateIsSafe(const SupportSafetyIndex& index,
                            const Note& candidate,
                            int jackWindowMs,
                            int sameSourceGapMs) {
    if (candidate.lane < 0 || candidate.lane >= kMaxKeyCount) {
        return false;
    }
    const auto& laneNotes = index.lanes[static_cast<std::size_t>(candidate.lane)];
    for (const auto& note : laneNotes) {
        if (note.time == candidate.time) {
            return false;
        }
        if (note.type == NoteType::Hold && note.endTime.has_value() &&
            candidate.time > note.time && candidate.time <= *note.endTime) {
            return false;
        }
        const auto dt = std::abs(static_cast<long long>(note.time) - candidate.time);
        const int guardedGap = note.sourceLane == candidate.sourceLane ? sameSourceGapMs : jackWindowMs;
        if (dt > 0 && dt <= guardedGap) {
            return false;
        }
    }
    return true;
}

std::string supportIdPrefix(SupportKind kind, MotifKind motif) {
    const std::string generator = kind == SupportKind::Ln
                                      ? "nk2-ln-"
                                      : (kind == SupportKind::Mirror ? "nk2-mirror-" : "nk2-beat-");
    return generator + motifSlug(motif) + "-";
}

void countSupportCandidate(NK2Report& report, SupportKind kind) {
    if (kind == SupportKind::Ln) {
        ++report.lnSupportCandidates;
    } else if (kind == SupportKind::Mirror) {
        ++report.mirrorSupportCandidates;
    } else {
        ++report.strongBeatSupportCandidates;
    }
}

void countSupportAccepted(NK2Report& report, SupportKind kind) {
    if (kind == SupportKind::Ln) {
        ++report.lnSupportAccepted;
    } else if (kind == SupportKind::Mirror) {
        ++report.mirrorSupportAccepted;
    } else {
        ++report.strongBeatSupportAccepted;
    }
}

void countSupportRejected(NK2Report& report, SupportKind kind) {
    if (kind == SupportKind::Ln) {
        ++report.lnSupportRejected;
    } else if (kind == SupportKind::Mirror) {
        ++report.mirrorSupportRejected;
    } else {
        ++report.strongBeatSupportRejected;
    }
}

bool tryAcceptSupportEvent(Chart& chart,
                           NK2Report& report,
                           const NK2Options& options,
                           const SupportEvent& event,
                           std::vector<int>& laneUse,
                           std::map<int, int>& acceptedByPhrase,
                           const std::map<int, int>& sourceNotesByPhrase,
                           int acceptedSoFar,
                           int budget,
                           SupportSafetyIndex& safetyIndex) {
    countSupportCandidate(report, event.kind);

    if (acceptedSoFar >= budget) {
        ++report.supportRejectedByBudget;
        countSupportRejected(report, event.kind);
        return false;
    }

    const int phraseWindow = supportPhraseWindowForTime(event.time);
    const auto sourceCount = sourceNotesByPhrase.find(phraseWindow);
    const int phraseSourceNotes = sourceCount == sourceNotesByPhrase.end() ? 0 : sourceCount->second;
    const int phraseBudget = phraseSupportBudgetFor(options, phraseSourceNotes);
    if (acceptedByPhrase[phraseWindow] >= phraseBudget) {
        ++report.supportRejectedByBudget;
        ++report.supportRejectedByPhraseBudget;
        countSupportRejected(report, event.kind);
        return false;
    }

    const auto ranked = rankedSupportLanes(event, laneUse, options);
    for (const auto& candidateLane : ranked) {
        Note candidate;
        candidate.id = supportIdPrefix(event.kind, event.anchorMotif) + event.anchorId + "-" +
                       std::to_string(event.time);
        candidate.time = event.time;
        candidate.lane = candidateLane.lane;
        candidate.sourceLane = event.sourceLane;
        candidate.type = NoteType::Tap;

        const bool fourToFiveFill = isFourToFiveFillOptions(options);
        const int supportJackWindowMs = supportJackWindowMsFor(options);
        // A generated tap has no original-repeat provenance. The phrase overlay
        // therefore uses the full guard on both sides, even for its anchor lane.
        const int sameSourceGapMs = options.phraseSupport ? supportJackWindowMs :
            (fourToFiveFill ? kFourToFiveSupportSameSourceGapMs : kDefaultSupportSameSourceGapMs);
        if (!supportCandidateIsSafe(safetyIndex, candidate, supportJackWindowMs, sameSourceGapMs)) {
            continue;
        }

        chart.notes.push_back(candidate);
        addToSupportSafetyIndex(safetyIndex, candidate);
        if (candidate.lane >= 0 && candidate.lane < options.targetKeyCount) {
            ++laneUse[static_cast<std::size_t>(candidate.lane)];
        }
        countSupportAccepted(report, event.kind);
        ++acceptedByPhrase[phraseWindow];
        countGeneratedProvenance(report, event.anchorMotif);
        return true;
    }

    ++report.supportRejectedBySafety;
    countSupportRejected(report, event.kind);
    return false;
}

void applySupportNotes(Chart& chart, NK2Report& report, const NK2Options& options) {
    const int budget = supportBudgetFor(options, report.intent.totalNotes);
    const auto events = buildSupportEvents(chart, options);
    const auto sourceNotesByPhrase = sourceNoteCountsBySupportPhrase(chart);
    report.supportPhraseWindows = static_cast<int>(sourceNotesByPhrase.size());
    auto laneUse = laneDistributionFor(chart, options.targetKeyCount);
    auto safetyIndex = buildSupportSafetyIndex(chart);
    std::map<int, int> acceptedByPhrase;
    std::set<int> acceptedTimes;
    int accepted = 0;
    for (const auto& event : events) {
        // Keep the approved B pattern intact; add at most one accent per time.
        // Legacy support generation retains its existing event handling.
        if (options.phraseSupport && acceptedTimes.contains(event.time)) {
            countSupportCandidate(report, event.kind);
            countSupportRejected(report, event.kind);
            ++report.supportRejectedBySafety;
            continue;
        }
        if (tryAcceptSupportEvent(chart,
                                  report,
                                  options,
                                  event,
                                  laneUse,
                                  acceptedByPhrase,
                                  sourceNotesByPhrase,
                                  accepted,
                                  budget,
                                  safetyIndex)) {
            ++accepted;
            if (options.phraseSupport) acceptedTimes.insert(event.time);
        }
    }
    std::stable_sort(chart.notes.begin(), chart.notes.end(), [](const Note& lhs, const Note& rhs) {
        if (lhs.time != rhs.time) {
            return lhs.time < rhs.time;
        }
        if (lhs.lane != rhs.lane) {
            return lhs.lane < rhs.lane;
        }
        return lhs.id < rhs.id;
    });
}

NK2Report buildBaseReport(const Chart& chart, const NK2Options& options) {
    NK2Report report;
    report.options = options;
    report.chartMutated = false;
    report.layout = buildTargetLayoutSummary(options.targetKeyCount, options.layoutWeights);
    report.intent = buildIntentGraphSummary(chart,
                                            options.sourceKeyCount,
                                            options.targetKeyCount,
                                            options.sameTimeEpsilonMs);
    report.outputNotes = report.intent.totalNotes;

    if (options.sourceKeyCount == options.targetKeyCount && options.mode != Mode::Transform) {
        report.noOp = true;
        report.noOpReason = "same key count without an explicit transform mode";
    }
    if (options.nativeWeight != options.remixWeight) {
        report.warnings.push_back("NK2 default design target is a 50/50 native/remix blend.");
    }
    return report;
}

bool supportsSevenToTenPrototype(const NK2Options& options) {
    return options.sourceKeyCount == 7 && options.targetKeyCount == 10 &&
           options.mode != Mode::Report;
}

bool supportsGenericPrototype(const NK2Options& options) {
    return options.sourceKeyCount > 0 && options.sourceKeyCount <= kMaxKeyCount &&
           options.targetKeyCount > 0 && options.targetKeyCount <= kMaxKeyCount &&
           (options.sourceKeyCount != options.targetKeyCount || options.mode == Mode::Transform) &&
           options.mode != Mode::Report;
}

bool supportsFourToFiveFill(const NK2Options& options) {
    return isFourToFiveFillOptions(options);
}

// The optional search uses chronological lane occupancy, independently of the
// legacy placement history. A state keeps only the last object on each lane;
// older holds cannot remain active after a legal successor on that lane.
struct PhraseState {
    std::array<std::optional<PlacedNote>, kMaxKeyCount> lastByTarget;
    LastBySource lastBySource;
    std::vector<int> laneUse;
    std::vector<int> choices;
    std::vector<MotifKind> motifs;
    std::vector<RecentSingleNote> singles;
    int dropped = 0;
    int droppedHolds = 0;
    double score = 0.0;
};

bool betterPhraseState(const PhraseState& lhs, const PhraseState& rhs) {
    // Retention outranks style; preserve LNs when equal-sized solutions exist.
    if (lhs.dropped != rhs.dropped) return lhs.dropped < rhs.dropped;
    if (lhs.droppedHolds != rhs.droppedHolds) return lhs.droppedHolds < rhs.droppedHolds;
    return lhs.score > rhs.score;
}

int phraseHorizonMs(const Chart& chart, int time) {
    double beatLength = 500.0;
    int latest = std::numeric_limits<int>::min();
    for (const auto& point : chart.timingPoints) {
        if (point.time <= time && point.time >= latest && std::isfinite(point.beatLength) &&
            point.beatLength > 0.0 && point.uninherited.value_or(true)) {
            beatLength = point.beatLength;
            latest = point.time;
        }
    }
    return static_cast<int>(std::clamp(beatLength * 2.0, 200.0, 2000.0));
}

LaneMask phraseBlockedLanes(const PhraseState& state, const Note& note, int targetKeys) {
    LaneMask blocked = 0;
    for (int lane = 0; lane < targetKeys; ++lane) {
        const auto& last = state.lastByTarget[static_cast<std::size_t>(lane)];
        if (!last) continue;
        const auto gap = static_cast<long long>(note.time) - last->time;
        if (gap <= 0 ||
            (last->type == NoteType::Hold && last->endTime && note.time <= *last->endTime) ||
            (gap <= 500 && last->sourceLane != sourceLaneOf(note))) {
            occupyLane(blocked, lane);
        }
    }
    return blocked;
}

void convertWithPhraseSearch(const Chart& source, const NK2Options& options,
                             const std::vector<SliceInfo>& slices, NK2ConversionResult& result) {
    constexpr std::size_t kPhraseBeamWidth = 24;
    constexpr std::size_t kPhraseMaxNotes = 48;
    PhraseState committed;
    committed.laneUse.assign(static_cast<std::size_t>(options.targetKeyCount), 0);
    for (std::size_t begin = 0; begin < slices.size();) {
        std::size_t end = begin;
        std::vector<std::pair<std::size_t, bool>> window;
        const int horizon = phraseHorizonMs(source, slices[begin].time);
        while (end < slices.size()) {
            const auto& slice = slices[end];
            if (end > begin &&
                (static_cast<long long>(slice.time) - slices[begin].time >= horizon ||
                 window.size() + slice.noteIndices.size() > kPhraseMaxNotes)) break;
            if (slice.noteIndices.size() > kPhraseMaxNotes) {
                throw std::runtime_error("NK2 phrase search cannot process a slice larger than 48 notes");
            }
            for (const auto index : slice.noteIndices) {
                window.emplace_back(index, slice.noteIndices.size() == 1);
            }
            ++end;
        }
        ++result.report.phraseSearchWindows;
        result.report.phraseSearchPeakNotes = std::max(result.report.phraseSearchPeakNotes,
                                                       static_cast<int>(window.size()));
        committed.choices.clear();
        committed.motifs.clear();
        committed.dropped = 0;
        committed.droppedHolds = 0;
        committed.score = 0.0;
        std::vector<PhraseState> beam{committed};
        for (const auto& [index, single] : window) {
            const auto& note = source.notes[index];
            const int sourceLane = sourceLaneOf(note);
            std::vector<PhraseState> next;
            next.reserve(beam.size() * static_cast<std::size_t>(options.targetKeyCount + 1));
            for (const auto& state : beam) {
                std::vector<PlacedNote> recent;
                for (const auto& last : state.lastByTarget) {
                    if (last) recent.push_back(*last);
                }
                std::stable_sort(recent.begin(), recent.end(), [](const auto& a, const auto& b) {
                    return a.time < b.time;
                });
                const bool continuation = isImmediateSourceJackContinuation(recent, sourceLane, note.time, 500);
                const bool fastSingle = single && !state.singles.empty() &&
                    note.time > state.singles.back().time &&
                    static_cast<long long>(note.time) - state.singles.back().time <= 280;
                const auto motif = classifyMotif(note, single, fastSingle, continuation, state.singles);
                const auto blocked = phraseBlockedLanes(state, note, options.targetKeyCount);
                const auto ranked = rankedCandidates(note, options, state.laneUse, state.lastBySource, recent,
                    fastSingle ? std::optional<int>(state.singles.back().sourceLane) : std::nullopt,
                    fastSingle ? std::optional<int>(state.singles.back().targetLane) : std::nullopt,
                    continuation, motif, blocked);
                for (const auto& candidate : ranked) {
                    if (hasSameTimeCollision(blocked, candidate.lane)) continue;
                    auto branch = state;
                    branch.choices.push_back(candidate.lane);
                    branch.motifs.push_back(motif);
                    branch.score += candidate.score;
                    const auto lastSource = lastPlacedForSource(state.lastBySource, sourceLane);
                    if (lastSource && static_cast<long long>(note.time) - lastSource->time <= 500) {
                        branch.score += lastSource->lane == candidate.lane ? 1.0 : -0.5;
                    }
                    const PlacedNote placedNote{note.time, candidate.lane, sourceLane, note.type, note.endTime};
                    branch.lastByTarget[static_cast<std::size_t>(candidate.lane)] = placedNote;
                    rememberLastBySource(branch.lastBySource, sourceLane, placedNote);
                    ++branch.laneUse[static_cast<std::size_t>(candidate.lane)];
                    if (!fastSingle) branch.singles.clear();
                    if (single) {
                        branch.singles.push_back({note.time, sourceLane, candidate.lane});
                        if (branch.singles.size() > 6) branch.singles.erase(branch.singles.begin());
                    }
                    next.push_back(std::move(branch));
                    ++result.report.phraseSearchCandidates;
                }
                // Explicit omission is a candidate, never an unreported repair.
                // No retiming is permitted in this first phrase-search mode.
                auto dropped = state;
                dropped.choices.push_back(-1);
                dropped.motifs.push_back(motif);
                dropped.singles.clear();
                ++dropped.dropped;
                if (note.type == NoteType::Hold) ++dropped.droppedHolds;
                next.push_back(std::move(dropped));
                ++result.report.phraseSearchCandidates;
            }
            std::stable_sort(next.begin(), next.end(), betterPhraseState);
            if (next.size() > kPhraseBeamWidth) next.resize(kPhraseBeamWidth);
            beam = std::move(next);
        }
        const auto& best = beam.front();
        for (std::size_t offset = 0; offset < window.size(); ++offset) {
            if (best.choices[offset] < 0) continue;
            auto note = source.notes[window[offset].first];
            note.sourceLane = sourceLaneOf(note);
            note.lane = best.choices[offset];
            result.chart.notes.push_back(std::move(note));
            countMotifPlacement(result.report, best.motifs[offset]);
        }
        result.report.droppedNotes += best.dropped;
        committed = best;
        begin = end;
    }
}

bool denseSupportCandidateIsSafe(const SupportSafetyIndex& index, const Note& candidate) {
    const auto candidateEnd = candidate.endTime.value_or(candidate.time);
    for (const auto& other : index.lanes[static_cast<std::size_t>(candidate.lane)]) {
        // Generated holds must reserve their entire interval against future
        // heads and tails, not just pass the tap insertion check at their head.
        if (std::abs(static_cast<long long>(other.time) - candidate.time) <= 500) return false;
        const auto otherEnd = other.endTime.value_or(other.time);
        if (candidate.time <= otherEnd && other.time <= candidateEnd) return false;
    }
    return true;
}

void applyDensePhraseNotes(Chart& chart, NK2Report& report, const NK2Options& options,
                          const Chart& source) {
    // C is immutable input for D/E. The F pass receives the completed E chart
    // and adds only holds, so raising its budget cannot displace any E note.
    // Candidate rhythm anchors come only from retained original material.
    Chart originals = chart;
    std::erase_if(originals.notes, isNk2GeneratedNote);
    if (originals.notes.empty()) return;
    const bool boost = options.phraseSourceLnBoost;
    const int totalBudget = static_cast<int>(std::ceil(report.intent.totalNotes * (boost ? 0.50 : 0.35)));
    const int holdBudget = static_cast<int>(std::ceil(report.intent.totalNotes * (boost ? 0.22 : 0.12)));
    const auto sourceByWindow = sourceNoteCountsBySupportPhrase(originals);
    std::map<int, int> addedByWindow;
    std::map<int, int> holdsByWindow;
    std::map<int, int> addedByTime;
    std::map<int, int> holdsByTime;
    std::set<std::string> acceptedIds;
    int addedTotal = 0;
    for (const auto& note : chart.notes) {
        if (isNk2GeneratedNote(note)) {
            ++addedTotal;
            ++addedByWindow[supportPhraseWindowForTime(note.time)];
            ++addedByTime[note.time];
            if (boost && note.type == NoteType::Hold) {
                ++holdsByWindow[supportPhraseWindowForTime(note.time)];
                ++holdsByTime[note.time];
            }
        }
    }
    auto laneUse = laneDistributionFor(chart, options.targetKeyCount);
    auto safetyIndex = buildSupportSafetyIndex(chart);
    std::vector<std::pair<int, int>> sourceHoldCoverage;
    if (options.phraseSourceLn) {
        for (const auto& note : source.notes) {
            if (note.type == NoteType::Hold && note.endTime && *note.endTime > note.time) {
                sourceHoldCoverage.emplace_back(note.time, *note.endTime);
            }
        }
        // Each prefix maximum is the tail of one actual source LN. We never
        // join touching/overlapping source LNs to grant a longer allowed span.
        std::sort(sourceHoldCoverage.begin(), sourceHoldCoverage.end());
        for (std::size_t i = 1; i < sourceHoldCoverage.size(); ++i) {
            sourceHoldCoverage[i].second = std::max(sourceHoldCoverage[i].second,
                                                    sourceHoldCoverage[i - 1].second);
        }
    }
    const auto sourceHoldEndAt = [&](int time) -> std::optional<int> {
        const auto it = std::upper_bound(sourceHoldCoverage.begin(), sourceHoldCoverage.end(), time,
            [](int value, const auto& span) { return value < span.first; });
        if (it == sourceHoldCoverage.begin()) return std::nullopt;
        return std::prev(it)->second;
    };
    std::array<int, kMaxKeyCount> generatedHoldLaneUse{};
    std::array<int, 4> generatedHoldLengthUse{};
    std::optional<int> lastHoldLane;
    std::optional<int> lastHoldLengthClass;
    const auto lengthClass = [](int duration, int beat) {
        if (duration < beat * 0.75) return 0;
        if (duration < beat * 1.5) return 1;
        if (duration < beat * 3.0) return 2;
        return 3;
    };
    if (boost) {
        // Include E's generated LNs in the distribution, not just this pass.
        for (const auto& note : chart.notes) {
            if (isNk2GeneratedNote(note) && note.type == NoteType::Hold && note.endTime) {
                ++generatedHoldLaneUse[static_cast<std::size_t>(note.lane)];
                const auto category = lengthClass(*note.endTime - note.time,
                                                  phraseHorizonMs(originals, note.time) / 2);
                ++generatedHoldLengthUse[static_cast<std::size_t>(category)];
            }
        }
    }
    const auto accept = [&](const SupportEvent& event, std::optional<int> endTime) {
        ++report.denseSupportCandidates;
        const bool hold = endTime.has_value();
        if (options.phraseSourceLn && hold) {
            const auto coveredEnd = sourceHoldEndAt(event.time);
            if (!coveredEnd || *endTime > *coveredEnd || *endTime <= event.time) {
                ++report.denseRejectedBySafety;
                return false;
            }
        }
        const int window = supportPhraseWindowForTime(event.time);
        const auto found = sourceByWindow.find(window);
        const int localSources = found == sourceByWindow.end() ? 0 : found->second;
        const int localBudget = phraseSupportBudgetFor(options, localSources);
        if (addedTotal >= totalBudget || addedByWindow[window] >= localBudget ||
            addedByTime[event.time] >= (boost ? 3 : 2) || (hold &&
            (report.denseHoldAccepted >= holdBudget || holdsByWindow[window] >= (boost ? 5 : 3) ||
             holdsByTime[event.time] >= (boost ? 2 : 1)))) {
            ++report.denseRejectedByBudget;
            return false;
        }
        const auto id = std::string(hold ? (boost ? "nk2-ln-source-boost-" :
                                          options.phraseSourceLn ? "nk2-ln-source-" : "nk2-ln-dense-")
                                         : "nk2-beat-dense-") +
            event.anchorId + "-" + std::to_string(event.time) +
            (hold ? "-" + std::to_string(*endTime) : "");
        if (acceptedIds.contains(id)) {
            ++report.denseRejectedByBudget;
            return false;
        }
        const auto ranked = rankedSupportLanes(event, laneUse, options);
        std::vector<Candidate> candidates(ranked.begin(), ranked.end());
        if (options.phraseSourceLn && hold) {
            // Balance generated holds separately from taps and original LNs.
            // Safety still wins when only one lane can accept the full span.
            for (auto& candidate : candidates) {
                candidate.score -= 0.65 * generatedHoldLaneUse[static_cast<std::size_t>(candidate.lane)];
                if (lastHoldLane) {
                    if (candidate.lane == *lastHoldLane) candidate.score -= 3.0;
                    if ((candidate.lane < options.targetKeyCount / 2) ==
                        (*lastHoldLane < options.targetKeyCount / 2)) candidate.score -= 0.8;
                }
            }
            std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
                return a.score > b.score;
            });
        }
        for (const auto& lane : candidates) {
            Note candidate;
            candidate.id = id;
            candidate.time = event.time;
            candidate.lane = lane.lane;
            candidate.sourceLane = event.sourceLane;
            candidate.type = hold ? NoteType::Hold : NoteType::Tap;
            candidate.endTime = endTime;
            if (!denseSupportCandidateIsSafe(safetyIndex, candidate)) continue;
            chart.notes.push_back(candidate);
            addToSupportSafetyIndex(safetyIndex, candidate);
            ++laneUse[static_cast<std::size_t>(candidate.lane)];
            ++addedTotal;
            ++addedByWindow[window];
            ++addedByTime[event.time];
            acceptedIds.insert(id);
            if (hold) {
                ++report.denseHoldAccepted;
                if (boost) ++report.sourceLnBoostHoldAccepted;
                ++holdsByWindow[window];
                ++holdsByTime[event.time];
                if (options.phraseSourceLn) {
                    ++generatedHoldLaneUse[static_cast<std::size_t>(candidate.lane)];
                    lastHoldLane = candidate.lane;
                    const auto category = lengthClass(*endTime - event.time,
                                                       phraseHorizonMs(originals, event.time) / 2);
                    ++generatedHoldLengthUse[static_cast<std::size_t>(category)];
                    lastHoldLengthClass = category;
                }
            } else {
                ++report.denseTapAccepted;
            }
            countGeneratedProvenance(report, hold ? MotifKind::LnAnchor : event.anchorMotif);
            return true;
        }
        ++report.denseRejectedBySafety;
        return false;
    };
    const auto anchorEvent = [](const Note& note, SupportKind kind) {
        return SupportEvent{kind, kind == SupportKind::Ln ? MotifKind::LnAnchor : MotifKind::Neutral,
                            note.time, sourceLaneOf(note), note.lane, note.id};
    };

    if (options.phraseSourceLn) {
        std::set<int, std::less<>> endpoints;
        for (const auto& note : source.notes) {
            endpoints.insert(note.time);
            if (note.type == NoteType::Hold && note.endTime && *note.endTime > note.time) {
                endpoints.insert(*note.endTime);
            }
        }
        std::set<int> triedHeads;
        for (const auto& note : originals.notes) {
            if (!triedHeads.insert(note.time).second) continue;
            const auto coveredEnd = sourceHoldEndAt(note.time);
            if (!coveredEnd || static_cast<long long>(*coveredEnd) - note.time < 200) continue;
            const auto minEnd = static_cast<long long>(note.time) + 200;
            const auto maxEnd = std::min(static_cast<long long>(*coveredEnd),
                                        static_cast<long long>(note.time) + 2000);
            const auto first = endpoints.lower_bound(minEnd);
            const auto after = endpoints.upper_bound(maxEnd);
            if (first == after) continue;
            const int beat = phraseHorizonMs(originals, note.time) / 2;
            std::set<int> ends{*first, *std::prev(after)};
            for (const double multiple : {0.5, 1.0, 1.5, 2.0, 3.0, 4.0}) {
                const auto desired = std::clamp(static_cast<long long>(note.time) +
                                               static_cast<long long>(beat * multiple), minEnd, maxEnd);
                const auto it = endpoints.lower_bound(desired);
                if (it != endpoints.end() && *it <= maxEnd) ends.insert(*it);
                if (it != endpoints.begin() && *std::prev(it) >= minEnd) ends.insert(*std::prev(it));
            }
            const std::array<double, 4> desiredLengths{beat * 0.5, static_cast<double>(beat),
                                                       beat * 2.0, static_cast<double>(maxEnd - note.time)};
            const double desired = std::clamp(desiredLengths[static_cast<std::size_t>(report.denseHoldAccepted % 4)],
                                               200.0, static_cast<double>(maxEnd - note.time));
            const auto preference = [&](int end) {
                const int duration = end - note.time;
                const int category = lengthClass(duration, beat);
                return std::abs(duration - desired) / std::max(200, beat) +
                    2.0 * generatedHoldLengthUse[static_cast<std::size_t>(category)] /
                        std::max(1, report.denseHoldAccepted) +
                    (lastHoldLengthClass == category ? 1.2 : 0.0);
            };
            std::vector<int> rankedEnds(ends.begin(), ends.end());
            std::stable_sort(rankedEnds.begin(), rankedEnds.end(), [&](int a, int b) {
                return preference(a) < preference(b);
            });
            for (const int end : rankedEnds) {
                if (accept(anchorEvent(note, SupportKind::Ln), end)) break;
            }
        }
    } else {
    // First echo existing LN spans when there is a spare lane for the full span.
    // Long sustains are not copied: the first dense experiment caps them at 2s.
    for (const auto& note : originals.notes) {
        const auto duration = static_cast<long long>(note.endTime.value_or(note.time)) - note.time;
        if (note.type == NoteType::Hold && duration >= 200 && duration <= 2000) {
            accept(anchorEvent(note, SupportKind::Ln), note.endTime);
        }
    }
    // Extra short holds connect two existing rhythmic endpoints. This also
    // gives tap-heavy charts LN candidates without inventing off-grid tails.
    std::set<int, std::less<>> endpoints;
    for (const auto& note : originals.notes) {
        endpoints.insert(note.time);
        if (note.type == NoteType::Hold && note.endTime && *note.endTime > note.time) {
            endpoints.insert(*note.endTime);
        }
    }
    for (const auto& note : originals.notes) {
        const int desiredLength = clampInt(phraseHorizonMs(originals, note.time) / 2, 200, 800);
        std::optional<int> endTime;
        long long bestDistance = std::numeric_limits<long long>::max();
        for (auto it = endpoints.lower_bound(static_cast<long long>(note.time) + 200);
             it != endpoints.end() && static_cast<long long>(*it) - note.time <= 800; ++it) {
            const auto distance = std::abs(static_cast<long long>(*it) - note.time - desiredLength);
            if (distance < bestDistance) {
                bestDistance = distance;
                endTime = *it;
            }
        }
        if (endTime) accept(anchorEvent(note, SupportKind::Ln), endTime);
    }
    }
    if (!boost) {
        // Reuse the musical C accents first, then fill other original tap heads.
        for (const auto& event : buildSupportEvents(originals, options)) accept(event, std::nullopt);
        for (const auto& note : originals.notes) {
            if (note.type == NoteType::Tap) accept(anchorEvent(note, SupportKind::Mirror), std::nullopt);
        }
    }
    std::stable_sort(chart.notes.begin(), chart.notes.end(), [](const Note& a, const Note& b) {
        if (a.time != b.time) return a.time < b.time;
        if (a.lane != b.lane) return a.lane < b.lane;
        return a.id < b.id;
    });
}

void fillIndependentAudit(const Chart& source, NK2ConversionResult& result) {
    if (!result.report.options.independentSafetyAudit && !result.report.options.phraseSearch) return;
    const int targetKeys = result.report.chartMutated ? result.report.options.targetKeyCount
                                                     : result.report.options.sourceKeyCount;
    result.report.independentSafety = auditSafety(source, result.chart, targetKeys, 500,
                                                   result.report.options.phraseSourceLn);
    if (!result.report.independentSafety->passed()) {
        result.report.warnings.push_back("Independent lane-by-lane safety audit failed; inspect independentSafety counters.");
    }
}

bool supportsSupportNotes(const NK2Options& options) {
    if (options.mode == Mode::Report || options.sourceKeyCount <= 0 || options.targetKeyCount <= 0) {
        return false;
    }
    if (options.superSymmetry) {
        return false;
    }
    if (options.targetKeyCount <= options.sourceKeyCount) {
        return false;
    }
    if (options.sourceKeyCount > kMaxKeyCount || options.targetKeyCount > kMaxKeyCount) {
        return false;
    }
    // B remains placement-only even in the legacy 4K-to-5K fill special case.
    if (options.phraseSearch) return options.phraseSupport;
    return options.mode != Mode::Faithful || isFourToFiveFillOptions(options) || options.phraseSupport;
}

}  // namespace

NK2Report analyzeReportOnly(const Chart& chart, const NK2Options& options) {
    auto report = buildBaseReport(chart, options);
    if (options.mode != Mode::Report) {
        report.warnings.push_back(
            "NK2 analysis-only path was used; no chart output was mutated.");
    }
    return report;
}

NK2ConversionResult convertChart(const Chart& chart, const NK2Options& options) {
    const bool sevenKeyExpansion = options.sourceKeyCount == 7 &&
        (options.targetKeyCount == 8 || options.targetKeyCount == 10);
    // Only existing TenRiff menu targets are enabled; compression uses phrase placement.
    const bool phrasePair = options.sourceKeyCount >= 1 && options.sourceKeyCount <= 18 &&
        ((options.targetKeyCount >= 4 && options.targetKeyCount <= 10) ||
         options.targetKeyCount == 12 || options.targetKeyCount == 14 || options.targetKeyCount == 16);
    const bool phraseExpansionPair = phrasePair && options.targetKeyCount > options.sourceKeyCount;
    if (options.phraseSourceLnBoost && (!options.phraseSourceLn || !sevenKeyExpansion)) {
        throw std::runtime_error("NK2 source LN boost requires 7K to 8K/10K source LN support");
    }
    if (options.phraseSourceLn && (!options.phraseDenseLn || !options.phraseSupport || !options.phraseSearch ||
        !phraseExpansionPair ||
        options.mode != Mode::Faithful || options.superSymmetry)) {
        throw std::runtime_error("NK2 source LN support requires faithful expansion to 4K-10K/12K/14K/16K dense phrase support without super-symmetry");
    }
    if (options.phraseDenseLn && (!options.phraseSupport || !options.phraseSearch ||
        !phraseExpansionPair ||
        options.mode != Mode::Faithful || options.superSymmetry)) {
        throw std::runtime_error("NK2 dense LN support requires faithful expansion to 4K-10K/12K/14K/16K phrase support without super-symmetry");
    }
    if (options.phraseSupport && (!options.phraseSearch || !phraseExpansionPair ||
        options.mode != Mode::Faithful || options.superSymmetry)) {
        throw std::runtime_error("NK2 phrase support requires faithful expansion to 4K-10K/12K/14K/16K phrase search without super-symmetry");
    }
    if (options.phraseSearch &&
        (!phrasePair || options.mode != Mode::Faithful || options.superSymmetry)) {
        throw std::runtime_error("NK2 phrase search requires faithful conversion to 4K-10K/12K/14K/16K without super-symmetry");
    }
    NK2ConversionResult result;
    result.chart = chart;
    result.report = buildBaseReport(chart, options);

    if (options.mode == Mode::Report) {
        fillIndependentAudit(chart, result);
        return result;
    }

    if (result.report.noOp) {
        fillDistributionAndSafety(result.report, result.chart);
        fillIndependentAudit(chart, result);
        return result;
    }

    const bool sevenToTenPrototype = supportsSevenToTenPrototype(options);
    const bool genericPrototype = supportsGenericPrototype(options);
    if (!sevenToTenPrototype && !genericPrototype) {
        result.report.noOp = true;
        result.report.noOpReason = "NK2 conversion currently supports 1K..18K experimental prototypes";
        result.report.warnings.push_back(
            "Use --nk2-mode report for analysis-only on unsupported key-count pairs.");
        fillDistributionAndSafety(result.report, result.chart);
        fillIndependentAudit(chart, result);
        return result;
    }

    result.chart.meta.targetKeyCount = options.targetKeyCount;
    result.chart.meta.version =
        convertedDifficultyName(chart.meta.version, options.targetKeyCount, options.superSymmetry);
    if (options.phraseSearch) *result.chart.meta.version += "-phrase";
    if (options.phraseSupport) *result.chart.meta.version += "-notes";
    if (options.phraseDenseLn) *result.chart.meta.version += "-dense-ln";
    if (options.phraseSourceLn) *result.chart.meta.version += "-source-ln";
    if (options.phraseSourceLnBoost) *result.chart.meta.version += "-boost";
    result.chart.notes.clear();
    result.chart.notes.reserve(chart.notes.size());

    const auto slices = buildSlices(chart, options.sameTimeEpsilonMs);
    std::vector<int> laneUse(static_cast<std::size_t>(options.targetKeyCount), 0);
    std::vector<PlacedNote> placed;
    LastBySource lastBySource;
    placed.reserve(chart.notes.size());

    std::optional<int> previousSingleSourceLane;
    std::optional<int> previousSingleTargetLane;
    std::optional<int> previousSingleTime;
    std::vector<RecentSingleNote> recentSingles;

    if (options.phraseSearch) {
        convertWithPhraseSearch(chart, options, slices, result);
    } else for (const auto& slice : slices) {
        LaneMask occupiedLanes = 0;
        const bool singleNoteSlice = slice.noteIndices.size() == 1;

        if (!singleNoteSlice) {
            const auto solvedSlice = solveSliceWithLocalBeam(chart,
                                                             slice,
                                                             options,
                                                             laneUse,
                                                             lastBySource,
                                                             placed,
                                                             result.report);
            if (solvedSlice.has_value()) {
                for (const auto& solved : *solvedSlice) {
                    result.chart.notes.push_back(solved.note);
                    if (solved.note.lane >= 0 && solved.note.lane < options.targetKeyCount) {
                        ++laneUse[static_cast<std::size_t>(solved.note.lane)];
                    }

                    const int sourceLane = sourceLaneOf(solved.note);
                    PlacedNote placedNote{
                        solved.note.time, solved.note.lane, sourceLane, solved.note.type, solved.note.endTime};
                    placed.push_back(placedNote);
                    rememberLastBySource(lastBySource, sourceLane, placedNote);
                    const auto mirrorAnchor = sameTimeMirrorAnchorTargetLane(solved.note, placed, options);
                    if (mirrorAnchor.has_value() && *mirrorAnchor == solved.note.lane) {
                        ++result.report.superSymmetryMirrorAnchors;
                    }
                    countMotifPlacement(result.report, solved.motif);
                }

                previousSingleSourceLane.reset();
                previousSingleTargetLane.reset();
                previousSingleTime.reset();
                recentSingles.clear();
                continue;
            }
        }

        for (const auto noteIndex : slice.noteIndices) {
            auto note = chart.notes[noteIndex];
            const int sourceLane = sourceLaneOf(note);
            const bool fastSingleContinuation =
                singleNoteSlice && previousSingleTime.has_value() &&
                note.time > *previousSingleTime && note.time - *previousSingleTime <= 280;
            if (singleNoteSlice && !fastSingleContinuation) {
                recentSingles.clear();
            }
            const bool sourceJackContinuation =
                isImmediateSourceJackContinuation(placed, sourceLane, note.time, 500);
            const auto motif = classifyMotif(note,
                                             singleNoteSlice,
                                             fastSingleContinuation,
                                             sourceJackContinuation,
                                             recentSingles);
            auto placedMotif = motif;

            std::optional<int> chosenLane;
            chosenLane = chooseLaneStrict(note,
                                          options,
                                          occupiedLanes,
                                          laneUse,
                                          lastBySource,
                                          placed,
                                          fastSingleContinuation ? previousSingleSourceLane : std::nullopt,
                                          fastSingleContinuation ? previousSingleTargetLane : std::nullopt,
                                          motif);
            if (!chosenLane.has_value()) {
                const auto rolled = tryRollLowerKeyOverflowTap(note,
                                                               options,
                                                               laneUse,
                                                               lastBySource,
                                                               placed,
                                                               motif);
                if (!rolled.has_value()) {
                    ++result.report.droppedNotes;
                    continue;
                }
                note = rolled->note;
                chosenLane = note.lane;
                placedMotif = rolled->motif;
                ++result.report.lowerKeyRolledNotes;
            }
            note.sourceLane = sourceLane;
            note.lane = *chosenLane;
            if (std::abs(note.time - slice.time) <= options.sameTimeEpsilonMs) {
                occupyLane(occupiedLanes, *chosenLane);
            }
            if (*chosenLane >= 0 && *chosenLane < options.targetKeyCount) {
                ++laneUse[static_cast<std::size_t>(*chosenLane)];
            }

            PlacedNote placedNote{note.time, note.lane, sourceLane, note.type, note.endTime};
            const auto mirrorAnchor = sameTimeMirrorAnchorTargetLane(note, placed, options);
            if (mirrorAnchor.has_value() && *mirrorAnchor == note.lane) {
                ++result.report.superSymmetryMirrorAnchors;
            }
            if (options.superSymmetry &&
                preservesGaplessStair(sourceLane,
                                      note.lane,
                                      fastSingleContinuation ? previousSingleSourceLane : std::nullopt,
                                      fastSingleContinuation ? previousSingleTargetLane : std::nullopt)) {
                ++result.report.superSymmetryGaplessStairs;
            }
            placed.push_back(placedNote);
            rememberLastBySource(lastBySource, sourceLane, placedNote);
            countMotifPlacement(result.report, placedMotif);
            result.chart.notes.push_back(note);

            if (singleNoteSlice) {
                previousSingleSourceLane = sourceLane;
                previousSingleTargetLane = *chosenLane;
                previousSingleTime = note.time;
                recentSingles.push_back({note.time, sourceLane, *chosenLane});
                if (recentSingles.size() > 6) {
                    recentSingles.erase(recentSingles.begin());
                }
            }
        }

        if (!singleNoteSlice) {
            previousSingleSourceLane.reset();
            previousSingleTargetLane.reset();
            previousSingleTime.reset();
            recentSingles.clear();
        }
    }

    std::stable_sort(result.chart.notes.begin(), result.chart.notes.end(), [](const Note& lhs, const Note& rhs) {
        if (lhs.time != rhs.time) {
            return lhs.time < rhs.time;
        }
        if (lhs.lane != rhs.lane) {
            return lhs.lane < rhs.lane;
        }
        return lhs.id < rhs.id;
    });

    result.report.chartMutated = true;
    result.report.prototypeName = sevenToTenPrototype ? "nk2-7k10k-panel-bridge-fullfield"
                                                      : "nk2-generic-nk-relane-compress";
    if (options.phraseSearch) result.report.prototypeName = "nk2-phrase-search-v1";
    if (options.phraseSupport) result.report.prototypeName = "nk2-phrase-notes-v1";
    if (options.phraseDenseLn) result.report.prototypeName = "nk2-phrase-dense-ln-v1";
    if (options.phraseSourceLn) result.report.prototypeName = "nk2-phrase-source-ln-v1";
    if (options.phraseSourceLnBoost) result.report.prototypeName = "nk2-phrase-source-ln-boost-v1";
    const bool supportNotesEnabled = supportsSupportNotes(options);
    if (supportNotesEnabled) {
        auto baseSupportOptions = options;
        baseSupportOptions.phraseDenseLn = false;
        baseSupportOptions.phraseSourceLn = false;
        baseSupportOptions.phraseSourceLnBoost = false;
        applySupportNotes(result.chart, result.report, baseSupportOptions);
        if (options.phraseDenseLn) {
            auto denseOptions = options;
            denseOptions.phraseSourceLnBoost = false;
            applyDensePhraseNotes(result.chart, result.report, denseOptions, chart);
            if (options.phraseSourceLnBoost) applyDensePhraseNotes(result.chart, result.report, options, chart);
        }
    } else if (options.superSymmetry && options.mode != Mode::Faithful &&
               options.targetKeyCount > options.sourceKeyCount) {
        result.report.warnings.push_back(
            "NK2 support-note generation is disabled by super-symmetry mode.");
    } else if (options.mode != Mode::Faithful && options.targetKeyCount > options.sourceKeyCount) {
        result.report.warnings.push_back(
            "NK2 support-note generation is disabled for this key-count pair or mode.");
    }
    fillDistributionAndSafety(result.report, result.chart);
    if (result.report.addedNotes == 0) {
        if (options.mode == Mode::Faithful && !supportsFourToFiveFill(options) && !options.phraseSupport) {
            result.report.warnings.push_back("NK2 faithful mode disables support notes; unsafe source notes may be dropped.");
        } else if (supportNotesEnabled) {
            result.report.warnings.push_back(
                "NK2 support-note generation found no accepted safe candidates.");
        }
    }
    if (result.report.createdJacks > 0) {
        result.report.warnings.push_back("NK2 prototype created target same-lane repeats from different source lanes.");
    }
    if (result.report.sameTimeCollisions > 0 || result.report.longNoteConflicts > 0) {
        result.report.warnings.push_back("NK2 prototype produced placement conflicts that need repair.");
    }
    fillIndependentAudit(chart, result);
    return result;
}

}  // namespace keyweaver_e::nk2
