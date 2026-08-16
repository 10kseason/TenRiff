#include "nk2/nk3_relational.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace keyconv::nk3 {

namespace {

constexpr int kPhraseSliceLimit = 32;
constexpr int kBeamWidth = 64;
constexpr int kCreatedJackWindowMs = 500;
constexpr int kMaximumTimeShiftMs = 48;
constexpr int kVirtualMinimumGapMs = 45;
constexpr int kNoSource = -99;
using Pressure = std::int64_t;
constexpr Pressure kPressureScale = 1'000'000;
constexpr Pressure kMaximumExpansionPressure = 800'000;

Pressure fixedFromDouble(double value) {
    return static_cast<Pressure>(std::llround(value *
                                              static_cast<double>(kPressureScale)));
}

double fixedToDouble(Pressure value) {
    return static_cast<double>(value) /
           static_cast<double>(kPressureScale);
}

Pressure multiplyFixed(Pressure lhs, Pressure rhs) {
    return (lhs * rhs) / kPressureScale;
}

Pressure keyExpansionPressureFor(int sourceKeys, int targetKeys) {
    if (sourceKeys <= 0 || targetKeys <= sourceKeys) {
        return 0;
    }
    // Deterministic fixed-point second-order expansion of
    // (M/N)^0.85 - 1: 0.85u - 0.06375u^2, u=(M-N)/N.
    const Pressure u = static_cast<Pressure>(targetKeys - sourceKeys) *
                       kPressureScale / sourceKeys;
    const Pressure linear = multiplyFixed(u, 850'000);
    const Pressure quadratic = multiplyFixed(multiplyFixed(u, u), 63'750);
    return std::clamp(linear - quadratic,
                      Pressure{0},
                      kMaximumExpansionPressure);
}

struct Personality {
    double fidelity = 0.72;
    double novelty = 0.40;
    double spread = 0.80;
    double ergonomics = 0.70;
    double handBalance = 0.55;
};

struct RelationEdge {
    std::size_t from = 0;
    double normalizedLaneDelta = 0.0;
    double absoluteLaneDelta = 0.0;
    double normalizedTimeDelta = 0.0;
    double rhythmPhaseDelta = 0.0;
    double repeatSimilarity = 0.0;
    int deltaTime = 0;
    bool sameTime = false;
    bool sameSourceLane = false;
    bool overlapsHold = false;
    double rhythmWeight = 1.0;
};

struct RelationalNode {
    const Note* note = nullptr;
    int sourceLane = 0;
    int slice = 0;
    std::vector<RelationEdge> incoming;
};

struct PhraseRange {
    std::size_t begin = 0;
    std::size_t end = 0;
};

struct RelationalGraphV2 {
    std::vector<RelationalNode> nodes;
    std::vector<PhraseRange> phrases;
    int edgeCount = 0;
};

struct ObjectiveBreakdown {
    double relation = 0.0;
    double edit = 0.0;
    double move = 0.0;
    double hand = 0.0;
    double jack = 0.0;
    double time = 0.0;
    double difficulty = 0.0;
    double completion = 0.0;
    double novelty = 0.0;

    double total() const {
        return relation + edit + move + hand + jack + time + difficulty -
               completion - novelty;
    }
};

struct PhraseMetrics {
    int chromaticDemand = 1;
    double peakLoad = 0.0;
    Pressure peakLoadUnits = 0;
    double pressure = 0.0;
    double addCost = 0.0;
    double dropCost = 0.0;
};

struct BeamState {
    double cost = 0.0;
    ObjectiveBreakdown objective;
    std::vector<int> assignments;
    std::array<int, nk2::kMaxSupportedKeyCount> laneUse{};
    std::array<int, nk2::kMaxSupportedKeyCount> busyUntil{};
    std::array<int, nk2::kMaxSupportedKeyCount> lastSourceByLane{};
    std::array<int, nk2::kMaxSupportedKeyCount> lastTimeByLane{};
    std::array<int, nk2::kMaxSupportedKeyCount> lastLaneBySource{};
    std::array<int, nk2::kMaxSupportedKeyCount> lastTimeBySource{};
    int currentSlice = -1;
    std::uint32_t occupied = 0;
    int lastChordLane = -1;
    int lastEventLane = -1;
    int lastEventTime = -1000000000;
    int boundary = 3;
    int leftLoad = 0;
    int rightLoad = 0;
    int totalNotes = 0;
};

int sourceLaneOf(const Note& note) {
    return note.sourceLane.value_or(note.lane);
}

double clamp01(double value) {
    return std::max(0.0, std::min(1.0, value));
}

Personality personalityFor(nk2::Mode mode) {
    switch (mode) {
        case nk2::Mode::Faithful:
            return {0.98, 0.02, 0.28, 0.82, 0.62};
        case nk2::Mode::Native:
            return {0.86, 0.16, 0.55, 0.78, 0.58};
        case nk2::Mode::Harder:
            return {0.76, 0.30, 0.72, 0.66, 0.56};
        case nk2::Mode::Transform:
            return {0.58, 0.54, 0.86, 0.58, 0.50};
        case nk2::Mode::RemixedRemastered:
            return {0.36, 0.82, 0.96, 0.48, 0.44};
        case nk2::Mode::Remaster:
            return {0.72, 0.40, 0.82, 0.70, 0.56};
        case nk2::Mode::Report:
            return {1.0, 0.0, 0.0, 1.0, 0.5};
    }
    return {};
}

Personality personalityFor(const nk2::NK2Options& options) {
    Personality personality = personalityFor(options.mode);
    const auto apply = [](double overrideValue, double preset) {
        return overrideValue < 0.0 ? preset : clamp01(overrideValue);
    };
    personality.fidelity = apply(options.relationalFidelity, personality.fidelity);
    personality.novelty = apply(options.relationalNovelty, personality.novelty);
    personality.spread = apply(options.relationalSpread, personality.spread);
    personality.ergonomics = apply(options.relationalErgonomics, personality.ergonomics);
    personality.handBalance =
        apply(options.relationalHandBalance, personality.handBalance);
    return personality;
}

double beatLengthAt(const Chart& chart, int time) {
    double beatLength = 500.0;
    for (const auto& point : chart.timingPoints) {
        if (point.time > time) {
            break;
        }
        if (point.beatLength > 0.0 && point.uninherited.value_or(true)) {
            beatLength = point.beatLength;
        }
    }
    return beatLength;
}

double rhythmPhaseAt(const Chart& chart, int time) {
    const TimingPoint* active = nullptr;
    for (const auto& point : chart.timingPoints) {
        if (point.time > time) {
            break;
        }
        if (point.beatLength > 0.0 && point.uninherited.value_or(true)) {
            active = &point;
        }
    }
    if (active == nullptr || active->beatLength <= 0.0) {
        return 0.0;
    }
    const double beat = static_cast<double>(time - active->time) / active->beatLength;
    return beat - std::floor(beat);
}

double huber(double value, double delta = 0.25) {
    const double absolute = std::abs(value);
    return absolute <= delta ? 0.5 * absolute * absolute
                             : delta * (absolute - 0.5 * delta);
}

double softplus(double value) {
    if (value > 30.0) {
        return value;
    }
    if (value < -30.0) {
        return std::exp(value);
    }
    return std::log1p(std::exp(value));
}

double rhythmWeightAt(const Chart& chart, int time) {
    const TimingPoint* active = nullptr;
    for (const auto& point : chart.timingPoints) {
        if (point.time > time) {
            break;
        }
        if (point.beatLength > 0.0 && point.uninherited.value_or(true)) {
            active = &point;
        }
    }
    if (active == nullptr || active->beatLength <= 0.0) {
        return 1.0;
    }
    const double beat = static_cast<double>(time - active->time) / active->beatLength;
    const double phase = std::abs(beat - std::round(beat));
    return phase <= 0.08 ? 1.25 : (phase <= 0.28 ? 1.08 : 1.0);
}

RelationalGraphV2 buildRelationalGraph(const Chart& chart,
                                       int sourceKeys,
                                       int epsilonMs) {
    std::vector<const Note*> ordered;
    ordered.reserve(chart.notes.size());
    for (const auto& note : chart.notes) {
        ordered.push_back(&note);
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const Note* lhs, const Note* rhs) {
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

    RelationalGraphV2 graph;
    graph.nodes.reserve(ordered.size());
    std::array<std::optional<std::size_t>, nk2::kMaxSupportedKeyCount> lastBySource;
    int slice = -1;
    int sliceTime = -1000000000;
    for (std::size_t index = 0; index < ordered.size(); ++index) {
        const Note* note = ordered[index];
        if (slice < 0 || std::abs(note->time - sliceTime) > epsilonMs) {
            ++slice;
            sliceTime = note->time;
        }

        RelationalNode node;
        node.note = note;
        node.sourceLane = sourceLaneOf(*note);
        node.slice = slice;
        std::set<std::size_t> predecessors;
        if (index > 0) {
            predecessors.insert(index - 1);
        }
        if (index > 1) {
            predecessors.insert(index - 2);
        }
        if (node.sourceLane >= 0 && node.sourceLane < sourceKeys &&
            lastBySource[static_cast<std::size_t>(node.sourceLane)].has_value()) {
            predecessors.insert(*lastBySource[static_cast<std::size_t>(node.sourceLane)]);
        }
        for (std::size_t previous = index; previous > 0;) {
            --previous;
            if (std::abs(ordered[previous]->time - note->time) > epsilonMs) {
                break;
            }
            predecessors.insert(previous);
        }

        for (const std::size_t previous : predecessors) {
            const Note* from = ordered[previous];
            const int fromLane = sourceLaneOf(*from);
            RelationEdge edge;
            edge.from = previous;
            edge.normalizedLaneDelta =
                static_cast<double>(node.sourceLane - fromLane) /
                static_cast<double>(std::max(1, sourceKeys - 1));
            edge.absoluteLaneDelta = std::abs(edge.normalizedLaneDelta);
            edge.deltaTime = note->time - from->time;
            edge.normalizedTimeDelta =
                static_cast<double>(edge.deltaTime) / beatLengthAt(chart, from->time);
            edge.rhythmPhaseDelta = rhythmPhaseAt(chart, note->time) -
                                    rhythmPhaseAt(chart, from->time);
            edge.sameTime = std::abs(edge.deltaTime) <= epsilonMs;
            edge.sameSourceLane = node.sourceLane == fromLane;
            edge.overlapsHold = from->type == NoteType::Hold && from->endTime.has_value() &&
                                *from->endTime >= note->time;
            edge.repeatSimilarity = edge.sameSourceLane ? 1.0 : 0.0;
            edge.rhythmWeight = rhythmWeightAt(chart, note->time);
            node.incoming.push_back(edge);
            ++graph.edgeCount;
        }
        const int rememberedSourceLane = node.sourceLane;
        graph.nodes.push_back(std::move(node));
        if (rememberedSourceLane >= 0 && rememberedSourceLane < sourceKeys) {
            lastBySource[static_cast<std::size_t>(rememberedSourceLane)] = index;
        }
    }

    if (graph.nodes.empty()) {
        return graph;
    }
    std::size_t phraseBegin = 0;
    int phraseFirstSlice = graph.nodes.front().slice;
    int previousSlice = phraseFirstSlice;
    int previousTime = graph.nodes.front().note->time;
    for (std::size_t index = 1; index < graph.nodes.size(); ++index) {
        const auto& node = graph.nodes[index];
        if (node.slice == previousSlice) {
            continue;
        }
        const bool sliceLimit = node.slice - phraseFirstSlice >= kPhraseSliceLimit;
        const bool longRest = node.note->time - previousTime > 1200;
        if (sliceLimit || longRest) {
            graph.phrases.push_back({phraseBegin, index});
            phraseBegin = index;
            phraseFirstSlice = node.slice;
        }
        previousSlice = node.slice;
        previousTime = node.note->time;
    }
    graph.phrases.push_back({phraseBegin, graph.nodes.size()});
    return graph;
}

BeamState initialState(int targetKeys) {
    BeamState state;
    state.busyUntil.fill(-1);
    state.lastSourceByLane.fill(kNoSource);
    state.lastTimeByLane.fill(-1000000000);
    state.lastLaneBySource.fill(-1);
    state.lastTimeBySource.fill(-1000000000);
    state.boundary = targetKeys / 2 - 1;
    return state;
}

std::vector<int> boundaryCandidates(int targetKeys) {
    const int middle = targetKeys / 2 - 1;
    if (targetKeys < 8) {
        return {middle};
    }
    return {std::max(1, middle - 1), middle, std::min(targetKeys - 2, middle + 1)};
}

bool stateBefore(const BeamState& lhs, const BeamState& rhs) {
    if (lhs.cost != rhs.cost) {
        return lhs.cost < rhs.cost;
    }
    return lhs.assignments < rhs.assignments;
}

int assignedLane(const RelationEdge& edge,
                 const BeamState& state,
                 const PhraseRange& phrase,
                 const std::vector<int>& committed) {
    if (edge.from >= phrase.begin) {
        const std::size_t local = edge.from - phrase.begin;
        return local < state.assignments.size() ? state.assignments[local] : -1;
    }
    return edge.from < committed.size() ? committed[edge.from] : -1;
}

double relationLoss(const RelationalNode& node,
                    int lane,
                    const BeamState& state,
                    const PhraseRange& phrase,
                    const std::vector<int>& committed,
                    int targetKeys) {
    double loss = 0.0;
    for (const auto& edge : node.incoming) {
        const int previousLane = assignedLane(edge, state, phrase, committed);
        if (previousLane < 0) {
            continue;
        }
        const double targetDelta = static_cast<double>(lane - previousLane) /
                                   static_cast<double>(targetKeys - 1);
        const double targetAbsolute = std::abs(targetDelta);
        const double targetSameLane = lane == previousLane ? 1.0 : 0.0;
        double edgeLoss = 2.2 * huber(targetDelta - edge.normalizedLaneDelta) +
                          1.2 * huber(targetAbsolute - edge.absoluteLaneDelta) +
                          1.4 * huber(targetSameLane -
                                      (edge.sameSourceLane ? 1.0 : 0.0),
                                      0.5) +
                          huber(targetSameLane - edge.repeatSimilarity, 0.5);
        if ((edge.normalizedLaneDelta > 0.0 && targetDelta <= 0.0) ||
            (edge.normalizedLaneDelta < 0.0 && targetDelta >= 0.0)) {
            edgeLoss += 1.1;
        }
        edgeLoss *= edge.sameTime ? 1.45 : 1.0;
        if (edge.overlapsHold && lane == previousLane) {
            edgeLoss += 2.0;
        }
        loss += edgeLoss * edge.rhythmWeight;
    }
    return loss;
}

ObjectiveBreakdown placementObjective(const RelationalNode& node,
                                      int lane,
                                      int boundary,
                                      const BeamState& state,
                                      const PhraseRange& phrase,
                                      const std::vector<int>& committed,
                                      const Personality& personality,
                                      int sourceKeys,
                                      int targetKeys) {
    ObjectiveBreakdown objective;
    const double relation = relationLoss(node, lane, state, phrase, committed, targetKeys);
    const double sourcePosition = static_cast<double>(node.sourceLane) /
                                  static_cast<double>(std::max(1, sourceKeys - 1));
    const double targetPosition = static_cast<double>(lane) /
                                  static_cast<double>(targetKeys - 1);
    const double positionDistance = std::abs(targetPosition - sourcePosition);

    const int laneUses = state.laneUse[static_cast<std::size_t>(lane)];
    const double usage = state.totalNotes <= 0
                             ? 0.0
                             : static_cast<double>(laneUses) /
                                   static_cast<double>(state.totalNotes);
    const double coveragePenalty = usage;

    const double sourceCenter = static_cast<double>(sourceKeys - 1) * 0.5;
    const int sourceHand = static_cast<double>(node.sourceLane) < sourceCenter
                               ? -1
                               : (static_cast<double>(node.sourceLane) > sourceCenter ? 1 : 0);
    const int targetHand = lane <= boundary ? -1 : 1;
    double hand = sourceHand != 0 && sourceHand != targetHand ? 0.65 : 0.0;
    double movement = 0.0;
    if (state.lastEventLane >= 0 && node.note->time - state.lastEventTime <= 280) {
        const double jump = static_cast<double>(std::abs(lane - state.lastEventLane)) /
                            static_cast<double>(targetKeys - 1);
        const double seconds = std::max(0.030,
                                        static_cast<double>(node.note->time -
                                                            state.lastEventTime) /
                                            1000.0);
        movement += std::pow(jump, 1.35) / seconds;
    }
    hand += std::pow(static_cast<double>(boundary - state.boundary), 2.0) * 0.06;

    const int leftAfter = state.leftLoad + (targetHand < 0 ? 1 : 0);
    const int rightAfter = state.rightLoad + (targetHand > 0 ? 1 : 0);
    const double handImbalance = static_cast<double>(std::abs(leftAfter - rightAfter)) /
                                 static_cast<double>(std::max(1, leftAfter + rightAfter));

    objective.relation = personality.fidelity *
                         (relation + positionDistance * 0.45);
    objective.move = personality.ergonomics * movement * 0.12;
    objective.hand = personality.ergonomics * hand +
                     personality.handBalance * handImbalance * 0.10;
    objective.edit = personality.spread * coveragePenalty * 0.65;
    objective.completion =
        laneUses == 0
            ? personality.spread * (targetKeys > sourceKeys ? 1.30 : 0.72)
            : 0.0;
    objective.novelty = personality.novelty * std::min(0.30, positionDistance) * 0.22;
    return objective;
}

void expandState(const RelationalNode& node,
                 const BeamState& original,
                 const PhraseRange& phrase,
                 const std::vector<int>& committed,
                 const Personality& personality,
                 int sourceKeys,
                 int targetKeys,
                 bool relaxCreatedJack,
                 std::vector<BeamState>& output) {
    BeamState base = original;
    if (base.currentSlice != node.slice) {
        base.currentSlice = node.slice;
        base.occupied = 0;
        base.lastChordLane = -1;
    }

    for (const int boundary : boundaryCandidates(targetKeys)) {
        for (int lane = 0; lane < targetKeys; ++lane) {
            const std::uint32_t mask = std::uint32_t{1} << lane;
            if ((base.occupied & mask) != 0 ||
                base.busyUntil[static_cast<std::size_t>(lane)] >= node.note->time ||
                (base.lastChordLane >= 0 && lane <= base.lastChordLane)) {
                continue;
            }
            const int previousSource = base.lastSourceByLane[static_cast<std::size_t>(lane)];
            const int previousTime = base.lastTimeByLane[static_cast<std::size_t>(lane)];
            if (!relaxCreatedJack && previousSource != kNoSource &&
                previousSource != node.sourceLane &&
                node.note->time - previousTime <= kCreatedJackWindowMs) {
                continue;
            }

            BeamState next = base;
            const auto increment = placementObjective(node,
                                                      lane,
                                                      boundary,
                                                      base,
                                                      phrase,
                                                      committed,
                                                      personality,
                                                      sourceKeys,
                                                      targetKeys);
            next.objective.relation += increment.relation;
            next.objective.edit += increment.edit;
            next.objective.move += increment.move;
            next.objective.hand += increment.hand;
            next.objective.completion += increment.completion;
            next.objective.novelty += increment.novelty;
            next.cost = next.objective.total();
            next.assignments.push_back(lane);
            next.occupied |= mask;
            next.lastChordLane = lane;
            next.lastEventLane = lane;
            next.lastEventTime = node.note->time;
            next.boundary = boundary;
            ++next.laneUse[static_cast<std::size_t>(lane)];
            ++next.totalNotes;
            next.lastSourceByLane[static_cast<std::size_t>(lane)] = node.sourceLane;
            next.lastTimeByLane[static_cast<std::size_t>(lane)] = node.note->time;
            if (node.note->type == NoteType::Hold && node.note->endTime.has_value()) {
                next.busyUntil[static_cast<std::size_t>(lane)] = *node.note->endTime;
            } else {
                next.busyUntil[static_cast<std::size_t>(lane)] = node.note->time;
            }
            if (node.sourceLane >= 0 && node.sourceLane < sourceKeys) {
                next.lastLaneBySource[static_cast<std::size_t>(node.sourceLane)] = lane;
                next.lastTimeBySource[static_cast<std::size_t>(node.sourceLane)] = node.note->time;
            }
            if (lane <= boundary) {
                ++next.leftLoad;
            } else {
                ++next.rightLoad;
            }
            output.push_back(std::move(next));
        }
    }
}

double missingLanePenalty(const BeamState& state,
                          const Personality& personality,
                          int targetKeys) {
    int missing = 0;
    for (int lane = 0; lane < targetKeys; ++lane) {
        missing += state.laneUse[static_cast<std::size_t>(lane)] == 0 ? 1 : 0;
    }
    return static_cast<double>(missing) * personality.spread * 0.9;
}

using RelationMatrix =
    std::array<std::array<double, nk2::kMaxSupportedKeyCount>,
               nk2::kMaxSupportedKeyCount>;

struct ColoringState {
    double cost = 0.0;
    std::vector<int> colors;
};

struct ContractionRuntime {
    std::array<int, nk2::kMaxSupportedKeyCount> busyUntil{};
    std::array<int, nk2::kMaxSupportedKeyCount> lastSourceByLane{};
    std::array<int, nk2::kMaxSupportedKeyCount> lastTimeByLane{};
    std::array<int, nk2::kMaxSupportedKeyCount> lastLaneBySource{};
    std::array<int, nk2::kMaxSupportedKeyCount> lastTimeBySource{};
    int lastEventSource = kNoSource;
    int lastEventLane = -1;
    int lastEventTime = -1000000000;
};

RelationMatrix relationMatrixFor(const RelationalGraphV2& graph,
                                 const PhraseRange& phrase,
                                 int sourceKeys) {
    RelationMatrix weights{};
    for (std::size_t to = phrase.begin; to < phrase.end; ++to) {
        const auto& node = graph.nodes[to];
        for (const auto& edge : node.incoming) {
            if (edge.from < phrase.begin || edge.from >= phrase.end) {
                continue;
            }
            const int fromLane = graph.nodes[edge.from].sourceLane;
            const int toLane = node.sourceLane;
            if (fromLane < 0 || fromLane >= sourceKeys || toLane < 0 ||
                toLane >= sourceKeys || fromLane == toLane) {
                continue;
            }
            double weight = edge.sameTime ? 120.0 : (edge.deltaTime <= 500 ? 24.0 : 3.0);
            weight *= edge.rhythmWeight;
            weights[static_cast<std::size_t>(fromLane)][static_cast<std::size_t>(toLane)] += weight;
            weights[static_cast<std::size_t>(toLane)][static_cast<std::size_t>(fromLane)] += weight;
        }
    }
    return weights;
}

int approximateChromaticDemand(const RelationMatrix& weights, int sourceKeys) {
    std::vector<int> order(static_cast<std::size_t>(sourceKeys));
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        double lhsDegree = 0.0;
        double rhsDegree = 0.0;
        for (int lane = 0; lane < sourceKeys; ++lane) {
            lhsDegree += weights[static_cast<std::size_t>(lhs)][static_cast<std::size_t>(lane)];
            rhsDegree += weights[static_cast<std::size_t>(rhs)][static_cast<std::size_t>(lane)];
        }
        if (lhsDegree != rhsDegree) {
            return lhsDegree > rhsDegree;
        }
        return lhs < rhs;
    });

    std::array<int, nk2::kMaxSupportedKeyCount> color{};
    color.fill(-1);
    int colorCount = 0;
    for (const int lane : order) {
        std::array<bool, nk2::kMaxSupportedKeyCount> blocked{};
        for (int other = 0; other < sourceKeys; ++other) {
            if (color[static_cast<std::size_t>(other)] < 0 ||
                weights[static_cast<std::size_t>(lane)][static_cast<std::size_t>(other)] <
                    10.0) {
                continue;
            }
            blocked[static_cast<std::size_t>(color[static_cast<std::size_t>(other)])] = true;
        }
        int selected = 0;
        while (selected < colorCount && blocked[static_cast<std::size_t>(selected)]) {
            ++selected;
        }
        if (selected == colorCount) {
            ++colorCount;
        }
        color[static_cast<std::size_t>(lane)] = selected;
    }
    return std::max(1, colorCount);
}

Pressure peakPhraseLoadUnits(const RelationalGraphV2& graph,
                             const PhraseRange& phrase) {
    Pressure peak = 0;
    std::size_t windowBegin = phrase.begin;
    for (std::size_t index = phrase.begin; index < phrase.end; ++index) {
        const int time = graph.nodes[index].note->time;
        while (windowBegin < index &&
               graph.nodes[windowBegin].note->time < time - 500) {
            ++windowBegin;
        }
        int simultaneous = 0;
        int activeHolds = 0;
        for (std::size_t candidate = phrase.begin; candidate < phrase.end; ++candidate) {
            const Note& note = *graph.nodes[candidate].note;
            simultaneous += std::abs(note.time - time) <= 2 ? 1 : 0;
            activeHolds += note.type == NoteType::Hold && note.time < time &&
                                   note.endTime.has_value() && *note.endTime >= time
                               ? 1
                               : 0;
        }
        const int halfSecondNotes = static_cast<int>(index - windowBegin + 1);
        const Pressure load =
            static_cast<Pressure>(simultaneous + activeHolds) * kPressureScale +
            static_cast<Pressure>(halfSecondNotes) * 300'000;
        peak = std::max(peak, load);
    }
    return peak;
}

PhraseMetrics phraseMetricsFor(const RelationalGraphV2& graph,
                               const PhraseRange& phrase,
                               int sourceKeys,
                               int targetKeys) {
    PhraseMetrics metrics;
    const auto weights = relationMatrixFor(graph, phrase, sourceKeys);
    metrics.chromaticDemand = approximateChromaticDemand(weights, sourceKeys);
    metrics.peakLoadUnits = peakPhraseLoadUnits(graph, phrase);
    metrics.peakLoad = fixedToDouble(metrics.peakLoadUnits);
    const double playableLoad = std::max(2.0, static_cast<double>(targetKeys) * 0.72 + 1.0);
    metrics.pressure =
        0.52 * (static_cast<double>(metrics.chromaticDemand) /
                    static_cast<double>(std::max(1, targetKeys)) -
                1.0) +
        0.48 * (metrics.peakLoad / playableLoad - 1.0);
    metrics.addCost = softplus(0.35 + 1.8 * metrics.pressure);
    metrics.dropCost = softplus(0.35 - 1.8 * metrics.pressure);
    return metrics;
}

std::vector<PhraseMetrics> buildPhraseMetrics(const RelationalGraphV2& graph,
                                              int sourceKeys,
                                              int targetKeys,
                                              RelationalStats& stats) {
    std::vector<PhraseMetrics> metrics;
    metrics.reserve(graph.phrases.size());
    double pressureSum = 0.0;
    bool first = true;
    for (const auto& phrase : graph.phrases) {
        const auto value = phraseMetricsFor(graph, phrase, sourceKeys, targetKeys);
        metrics.push_back(value);
        pressureSum += value.pressure;
        stats.peakChromaticDemand =
            std::max(stats.peakChromaticDemand, value.chromaticDemand);
        stats.peakLoad = std::max(stats.peakLoad, value.peakLoad);
        if (first) {
            stats.pressureMin = value.pressure;
            stats.pressureMax = value.pressure;
            first = false;
        } else {
            stats.pressureMin = std::min(stats.pressureMin, value.pressure);
            stats.pressureMax = std::max(stats.pressureMax, value.pressure);
        }
    }
    stats.pressureMean = metrics.empty() ? 0.0
                                         : pressureSum /
                                               static_cast<double>(metrics.size());
    return metrics;
}

struct DifficultyFeatures {
    double peakNps = 0.0;
    double chordLoad = 0.0;
    double jackLoad = 0.0;
    double movementLoad = 0.0;
    double lnLoad = 0.0;
    double fingerIndependence = 0.0;

    double score() const {
        return 0.28 * peakNps + 0.20 * chordLoad + 0.14 * jackLoad +
               0.18 * movementLoad + 0.10 * lnLoad + 0.10 * fingerIndependence;
    }
};

DifficultyFeatures difficultyFor(const Chart& chart, int keyCount) {
    DifficultyFeatures difficulty;
    if (chart.notes.empty()) {
        return difficulty;
    }
    std::vector<const Note*> ordered;
    ordered.reserve(chart.notes.size());
    for (const auto& note : chart.notes) {
        ordered.push_back(&note);
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const Note* lhs, const Note* rhs) {
        if (lhs->time != rhs->time) {
            return lhs->time < rhs->time;
        }
        return lhs->lane < rhs->lane;
    });

    std::size_t windowBegin = 0;
    int peakCount = 0;
    int chordSlices = 0;
    double chordSum = 0.0;
    int jacks = 0;
    double movement = 0.0;
    double holdBeats = 0.0;
    std::array<int, nk2::kMaxSupportedKeyCount> lastLaneTime{};
    lastLaneTime.fill(-1000000000);
    std::set<int> usedLanes;
    for (std::size_t index = 0; index < ordered.size(); ++index) {
        while (windowBegin < index && ordered[windowBegin]->time < ordered[index]->time - 1000) {
            ++windowBegin;
        }
        peakCount = std::max(peakCount, static_cast<int>(index - windowBegin + 1));
        const Note& note = *ordered[index];
        usedLanes.insert(note.lane);
        if (note.lane >= 0 && note.lane < keyCount) {
            jacks += note.time - lastLaneTime[static_cast<std::size_t>(note.lane)] <= 180 ? 1 : 0;
            lastLaneTime[static_cast<std::size_t>(note.lane)] = note.time;
        }
        if (index > 0 && note.time > ordered[index - 1]->time) {
            const double normalizedMove =
                static_cast<double>(std::abs(note.lane - ordered[index - 1]->lane)) /
                static_cast<double>(std::max(1, keyCount - 1));
            const double seconds = std::max(0.03,
                                            static_cast<double>(note.time -
                                                                ordered[index - 1]->time) /
                                                1000.0);
            movement += std::pow(normalizedMove, 1.25) / seconds;
        }
        if (note.type == NoteType::Hold && note.endTime.has_value()) {
            holdBeats += static_cast<double>(*note.endTime - note.time) /
                         beatLengthAt(chart, note.time);
        }
        if (index == 0 || note.time != ordered[index - 1]->time) {
            int chord = 1;
            for (std::size_t cursor = index + 1;
                 cursor < ordered.size() && ordered[cursor]->time == note.time;
                 ++cursor) {
                ++chord;
            }
            chordSum += std::pow(static_cast<double>(chord) /
                                     static_cast<double>(std::max(1, keyCount)),
                                 1.35);
            ++chordSlices;
        }
    }
    difficulty.peakNps = static_cast<double>(peakCount) / 10.0;
    difficulty.chordLoad = chordSlices == 0 ? 0.0 : chordSum / chordSlices;
    difficulty.jackLoad = static_cast<double>(jacks) /
                          static_cast<double>(std::max<std::size_t>(1, ordered.size()));
    difficulty.movementLoad = movement /
                              static_cast<double>(std::max<std::size_t>(1, ordered.size() - 1));
    difficulty.lnLoad = holdBeats /
                        static_cast<double>(std::max<std::size_t>(1, ordered.size()));
    difficulty.fingerIndependence = static_cast<double>(usedLanes.size()) /
                                    static_cast<double>(std::max(1, keyCount));
    return difficulty;
}

void measureContractionPlayability(const Chart& chart,
                                   const nk2::NK2Options& options,
                                   const Personality& personality,
                                   RelationalStats& stats) {
    std::vector<const Note*> ordered;
    ordered.reserve(chart.notes.size());
    for (const auto& note : chart.notes) {
        ordered.push_back(&note);
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const Note* lhs, const Note* rhs) {
        if (lhs->time != rhs->time) {
            return lhs->time < rhs->time;
        }
        return lhs->lane < rhs->lane;
    });
    const int boundary = options.targetKeyCount / 2 - 1;
    const double sourceCenter = static_cast<double>(options.sourceKeyCount - 1) * 0.5;
    int leftLoad = 0;
    int rightLoad = 0;
    std::array<const Note*, nk2::kMaxSupportedKeyCount> lastByLane{};
    for (std::size_t index = 0; index < ordered.size(); ++index) {
        const Note& note = *ordered[index];
        const int targetHand = note.lane <= boundary ? -1 : 1;
        leftLoad += targetHand < 0 ? 1 : 0;
        rightLoad += targetHand > 0 ? 1 : 0;
        if (note.sourceLane.has_value()) {
            const int sourceHand = static_cast<double>(*note.sourceLane) < sourceCenter
                                       ? -1
                                       : (static_cast<double>(*note.sourceLane) > sourceCenter
                                              ? 1
                                              : 0);
            if (sourceHand != 0 && sourceHand != targetHand) {
                stats.handLoss += personality.ergonomics * 0.18;
            }
        }
        if (index > 0 && note.time > ordered[index - 1]->time) {
            const int previousHand = ordered[index - 1]->lane <= boundary ? -1 : 1;
            if (previousHand == targetHand) {
                const double normalizedMove =
                    static_cast<double>(std::abs(note.lane - ordered[index - 1]->lane)) /
                    static_cast<double>(std::max(1, options.targetKeyCount - 1));
                const double seconds = std::max(
                    0.03,
                    static_cast<double>(note.time - ordered[index - 1]->time) / 1000.0);
                stats.moveLoss += personality.ergonomics *
                                  std::pow(normalizedMove, 1.35) / seconds * 0.08;
            }
        }
        if (note.lane >= 0 && note.lane < options.targetKeyCount) {
            const Note* previous = lastByLane[static_cast<std::size_t>(note.lane)];
            if (previous != nullptr && note.time > previous->time &&
                note.time - previous->time <= kCreatedJackWindowMs &&
                previous->sourceLane.has_value() && note.sourceLane.has_value() &&
                previous->sourceLane != note.sourceLane &&
                note.raw != "nk3-micro-roll") {
                stats.jackLoss += 1.0;
            }
            lastByLane[static_cast<std::size_t>(note.lane)] = &note;
        }
    }
    const int total = leftLoad + rightLoad;
    if (total > 0) {
        stats.handLoss += personality.handBalance *
                          static_cast<double>(std::abs(leftLoad - rightLoad)) /
                          static_cast<double>(total);
    }
}

void finalizeDifficulty(const Chart& source,
                        const Chart& result,
                        const nk2::NK2Options& options,
                        RelationalStats& stats) {
    const Personality personality = personalityFor(options);
    stats.effectiveFidelity = personality.fidelity;
    stats.effectiveNovelty = personality.novelty;
    stats.effectiveSpread = personality.spread;
    stats.effectiveErgonomics = personality.ergonomics;
    stats.effectiveHandBalance = personality.handBalance;
    stats.sourceDifficulty = difficultyFor(source, options.sourceKeyCount).score();
    stats.resultDifficulty = difficultyFor(result, options.targetKeyCount).score();
    stats.difficultyGoal = stats.sourceDifficulty *
                           std::max(0.1, options.relationalDifficultyGoal);
    const double delta = stats.resultDifficulty - stats.difficultyGoal;
    stats.difficultyLoss = delta * delta;
    stats.objective = stats.relationLoss + stats.editLoss + stats.moveLoss +
                      stats.handLoss + stats.jackLoss + stats.timeLoss +
                      stats.difficultyLoss - stats.completionGain -
                      stats.noveltyGain;
}

std::vector<int> contractionColoring(const RelationMatrix& weights,
                                     int sourceKeys,
                                     int targetKeys,
                                     const Personality& personality,
                                     RelationalStats& stats) {
    std::vector<ColoringState> beam(1);
    for (int sourceLane = 0; sourceLane < sourceKeys; ++sourceLane) {
        std::vector<ColoringState> next;
        for (const auto& state : beam) {
            const int firstColor = state.colors.empty() ? 0 : state.colors.back();
            for (int color = firstColor; color < targetKeys; ++color) {
                ColoringState candidate = state;
                const double sourcePosition = static_cast<double>(sourceLane) /
                                              static_cast<double>(std::max(1, sourceKeys - 1));
                const double targetPosition = static_cast<double>(color) /
                                              static_cast<double>(std::max(1, targetKeys - 1));
                candidate.cost += std::abs(sourcePosition - targetPosition) *
                                  (0.16 + personality.fidelity * 0.34);
                for (int previous = 0; previous < sourceLane; ++previous) {
                    if (candidate.colors[static_cast<std::size_t>(previous)] == color) {
                        candidate.cost +=
                            weights[static_cast<std::size_t>(sourceLane)]
                                   [static_cast<std::size_t>(previous)] *
                            (0.45 + personality.fidelity * 0.85);
                    }
                }
                candidate.colors.push_back(color);
                next.push_back(std::move(candidate));
            }
        }
        stats.expandedStates += static_cast<int>(next.size());
        std::stable_sort(next.begin(), next.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.cost != rhs.cost) {
                return lhs.cost < rhs.cost;
            }
            return lhs.colors < rhs.colors;
        });
        if (next.size() > 128) {
            next.resize(128);
        }
        beam = std::move(next);
    }
    if (beam.empty()) {
        ++stats.fallbacks;
        return {};
    }
    return beam.front().colors;
}

int informationValue(const RelationalGraphV2& graph,
                     std::size_t nodeIndex,
                     int sourceKeys) {
    const auto& node = graph.nodes[nodeIndex];
    int value = node.note->type == NoteType::Hold ? 10000 : 0;
    value += (node.sourceLane == 0 || node.sourceLane + 1 == sourceKeys) ? 160 : 0;
    for (const auto& edge : node.incoming) {
        value += edge.sameSourceLane ? 220 : 0;
        value += !edge.sameTime && edge.deltaTime <= 500 ? 80 : 0;
        value += edge.sameTime ? 20 : 0;
    }
    return value;
}

double relationDegreeWeight(const RelationalGraphV2& graph, std::size_t nodeIndex) {
    double degree = 0.0;
    for (const auto& edge : graph.nodes[nodeIndex].incoming) {
        degree += edge.rhythmWeight * (edge.sameTime ? 1.5 : 1.0);
    }
    for (std::size_t index = nodeIndex + 1; index < graph.nodes.size(); ++index) {
        for (const auto& edge : graph.nodes[index].incoming) {
            if (edge.from == nodeIndex) {
                degree += edge.rhythmWeight * (edge.sameTime ? 1.5 : 1.0);
            }
        }
    }
    return degree;
}

int chooseContractionLane(const RelationalNode& node,
                          std::size_t nodeIndex,
                          const RelationalGraphV2& graph,
                          const std::vector<int>& assignedLane,
                          const std::vector<int>& coloring,
                          ContractionRuntime& runtime,
                          int targetKeys,
                          int time,
                          std::uint32_t occupied,
                          int minimumLane,
                          bool intentionalRoll) {
    int bestLane = -1;
    int bestCost = std::numeric_limits<int>::max();
    const int preferred = coloring[static_cast<std::size_t>(node.sourceLane)];
    for (int lane = 0; lane < targetKeys; ++lane) {
        const std::uint32_t mask = std::uint32_t{1} << lane;
        if ((occupied & mask) != 0 || lane <= minimumLane ||
            runtime.busyUntil[static_cast<std::size_t>(lane)] >= time) {
            continue;
        }
        const int previousSource = runtime.lastSourceByLane[static_cast<std::size_t>(lane)];
        const int previousTime = runtime.lastTimeByLane[static_cast<std::size_t>(lane)];
        if (!intentionalRoll && previousSource != kNoSource &&
            previousSource != node.sourceLane &&
            time - previousTime <= kCreatedJackWindowMs) {
            continue;
        }
        if (runtime.lastEventSource != kNoSource &&
            time - runtime.lastEventTime <= kCreatedJackWindowMs) {
            const int sourceDelta = node.sourceLane - runtime.lastEventSource;
            const int targetDelta = lane - runtime.lastEventLane;
            if ((sourceDelta > 0 && targetDelta < 0) ||
                (sourceDelta < 0 && targetDelta > 0)) {
                continue;
            }
        }
        bool reversesRelation = false;
        for (const auto& edge : node.incoming) {
            if (edge.from >= assignedLane.size() || assignedLane[edge.from] < 0) {
                continue;
            }
            const int sourceDelta =
                graph.nodes[nodeIndex].sourceLane - graph.nodes[edge.from].sourceLane;
            const int targetDelta = lane - assignedLane[edge.from];
            if ((sourceDelta > 0 && targetDelta < 0) ||
                (sourceDelta < 0 && targetDelta > 0)) {
                reversesRelation = true;
                break;
            }
        }
        if (reversesRelation) {
            continue;
        }
        int cost = std::abs(lane - preferred) * 20;
        if (node.sourceLane >= 0 &&
            runtime.lastTimeBySource[static_cast<std::size_t>(node.sourceLane)] > -1000000000 &&
            time - runtime.lastTimeBySource[static_cast<std::size_t>(node.sourceLane)] <= 500) {
            cost += lane == runtime.lastLaneBySource[static_cast<std::size_t>(node.sourceLane)]
                        ? -200
                        : 200;
        }
        if (cost < bestCost) {
            bestCost = cost;
            bestLane = lane;
        }
    }
    return bestLane;
}

void rememberContractionPlacement(const RelationalNode& node,
                                  ContractionRuntime& runtime,
                                  int lane,
                                  int time) {
    runtime.lastSourceByLane[static_cast<std::size_t>(lane)] = node.sourceLane;
    runtime.lastTimeByLane[static_cast<std::size_t>(lane)] = time;
    runtime.lastLaneBySource[static_cast<std::size_t>(node.sourceLane)] = lane;
    runtime.lastTimeBySource[static_cast<std::size_t>(node.sourceLane)] = time;
    runtime.lastEventSource = node.sourceLane;
    runtime.lastEventLane = lane;
    runtime.lastEventTime = time;
    runtime.busyUntil[static_cast<std::size_t>(lane)] =
        node.note->type == NoteType::Hold && node.note->endTime.has_value()
            ? *node.note->endTime
            : time;
}

RelationalConversionResult convertContraction(const Chart& source,
                                              const nk2::NK2Options& options,
                                              const RelationalGraphV2& graph) {
    RelationalConversionResult result;
    result.chart = source;
    result.chart.notes.clear();
    result.chart.notes.reserve(source.notes.size());
    result.stats.graphNodes = static_cast<int>(graph.nodes.size());
    result.stats.graphEdges = graph.edgeCount;
    result.stats.phrases = static_cast<int>(graph.phrases.size());
    const auto phraseMetrics = buildPhraseMetrics(graph,
                                                  options.sourceKeyCount,
                                                  options.targetKeyCount,
                                                  result.stats);
    const Personality personality = personalityFor(options);

    ContractionRuntime runtime;
    runtime.busyUntil.fill(-1);
    runtime.lastSourceByLane.fill(kNoSource);
    runtime.lastTimeByLane.fill(-1000000000);
    runtime.lastLaneBySource.fill(-1);
    runtime.lastTimeBySource.fill(-1000000000);
    std::vector<int> assignedLane(graph.nodes.size(), -1);
    std::vector<int> assignedTime(graph.nodes.size(), -1);

    for (std::size_t phraseIndex = 0; phraseIndex < graph.phrases.size(); ++phraseIndex) {
        const auto& phrase = graph.phrases[phraseIndex];
        const auto& metrics = phraseMetrics[phraseIndex];
        const auto weights = relationMatrixFor(graph, phrase, options.sourceKeyCount);
        const auto coloring = contractionColoring(
            weights,
            options.sourceKeyCount,
            options.targetKeyCount,
            personality,
            result.stats);
        if (coloring.size() != static_cast<std::size_t>(options.sourceKeyCount)) {
            result.chart.notes.clear();
            return result;
        }

        std::size_t sliceBegin = phrase.begin;
        while (sliceBegin < phrase.end) {
            std::size_t sliceEnd = sliceBegin + 1;
            while (sliceEnd < phrase.end &&
                   graph.nodes[sliceEnd].slice == graph.nodes[sliceBegin].slice) {
                ++sliceEnd;
            }
            std::vector<std::size_t> order;
            for (std::size_t index = sliceBegin; index < sliceEnd; ++index) {
                order.push_back(index);
            }
            std::stable_sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
                const int lhsValue = informationValue(graph, lhs, options.sourceKeyCount);
                const int rhsValue = informationValue(graph, rhs, options.sourceKeyCount);
                if (lhsValue != rhsValue) {
                    return lhsValue > rhsValue;
                }
                return graph.nodes[lhs].sourceLane < graph.nodes[rhs].sourceLane;
            });

            std::vector<bool> keep(sliceEnd - sliceBegin, true);
            if (order.size() > static_cast<std::size_t>(options.targetKeyCount)) {
                ++result.stats.chordContractions;
                keep.assign(sliceEnd - sliceBegin, false);
                for (int slot = 0; slot < options.targetKeyCount; ++slot) {
                    keep[order[static_cast<std::size_t>(slot)] - sliceBegin] = true;
                }
            }

            std::vector<std::size_t> base;
            std::vector<std::size_t> overflow;
            for (std::size_t index = sliceBegin; index < sliceEnd; ++index) {
                (keep[index - sliceBegin] ? base : overflow).push_back(index);
            }
            std::stable_sort(base.begin(), base.end(), [&](std::size_t lhs, std::size_t rhs) {
                return graph.nodes[lhs].sourceLane < graph.nodes[rhs].sourceLane;
            });
            std::stable_sort(overflow.begin(), overflow.end(), [&](std::size_t lhs, std::size_t rhs) {
                return informationValue(graph, lhs, options.sourceKeyCount) >
                       informationValue(graph, rhs, options.sourceKeyCount);
            });

            const int time = graph.nodes[sliceBegin].note->time;
            std::uint32_t occupied = 0;
            int previousLane = -1;
            for (const std::size_t index : base) {
                const auto& node = graph.nodes[index];
                const int lane = chooseContractionLane(node,
                                                       index,
                                                       graph,
                                                       assignedLane,
                                                       coloring,
                                                       runtime,
                                                       options.targetKeyCount,
                                                       time,
                                                       occupied,
                                                       previousLane,
                                                       false);
                if (lane < 0) {
                    ++result.stats.droppedNotes;
                    const double importance =
                        1.0 + std::min(8.0,
                                       static_cast<double>(informationValue(
                                           graph, index, options.sourceKeyCount)) /
                                           240.0);
                    result.stats.editLoss +=
                        importance * metrics.dropCost *
                            (0.65 + personality.fidelity * 0.70) +
                                             relationDegreeWeight(graph, index) * 0.34;
                    continue;
                }
                Note mapped = *node.note;
                mapped.sourceLane = node.sourceLane;
                mapped.lane = lane;
                result.chart.notes.push_back(std::move(mapped));
                assignedLane[index] = lane;
                assignedTime[index] = time;
                ++result.stats.keptNotes;
                occupied |= std::uint32_t{1} << lane;
                previousLane = lane;
                rememberContractionPlacement(node, runtime, lane, time);
            }

            const int nextTime = sliceEnd < graph.nodes.size()
                                     ? graph.nodes[sliceEnd].note->time
                                     : std::numeric_limits<int>::max();
            bool rolledOne = false;
            for (const std::size_t index : overflow) {
                const auto& node = graph.nodes[index];
                const int rollTime = time + 30;
                int lane = -1;
                const double importance =
                    1.0 + std::min(8.0,
                                   static_cast<double>(informationValue(
                                       graph, index, options.sourceKeyCount)) /
                                       240.0);
                const double dropLoss = importance * metrics.dropCost *
                                            (0.65 + personality.fidelity * 0.70) +
                                        relationDegreeWeight(graph, index) * 0.34;
                const double timeLoss = std::pow(
                    static_cast<double>(rollTime - time) /
                        static_cast<double>(kMaximumTimeShiftMs),
                    2.0);
                const double rollLoss =
                    timeLoss * (0.55 + personality.ergonomics * 0.70) +
                    0.28 * personality.fidelity +
                                        std::max(0.0,
                                                 1.0 -
                                                     options.relationalDifficultyGoal) *
                                            0.55;
                if (!rolledOne && node.note->type == NoteType::Tap &&
                    nextTime - rollTime >= 30 && rollLoss < dropLoss) {
                    lane = chooseContractionLane(node,
                                                 index,
                                                 graph,
                                                 assignedLane,
                                                 coloring,
                                                 runtime,
                                                 options.targetKeyCount,
                                                 rollTime,
                                                 0,
                                                 -1,
                                                 true);
                }
                if (lane < 0) {
                    ++result.stats.droppedNotes;
                    result.stats.editLoss += dropLoss;
                    continue;
                }
                Note rolled = *node.note;
                rolled.sourceLane = node.sourceLane;
                rolled.lane = lane;
                rolled.time = rollTime;
                rolled.raw = "nk3-micro-roll";
                result.chart.notes.push_back(std::move(rolled));
                assignedLane[index] = lane;
                assignedTime[index] = rollTime;
                rememberContractionPlacement(node, runtime, lane, rollTime);
                ++result.stats.microRolls;
                ++result.stats.shiftedNotes;
                result.stats.timeLoss += timeLoss;
                rolledOne = true;
            }
            sliceBegin = sliceEnd;
        }
    }

    double totalRelationWeight = 0.0;
    double lostRelationWeight = 0.0;
    double totalRelationDistance = 0.0;
    for (std::size_t to = 0; to < graph.nodes.size(); ++to) {
        for (const auto& edge : graph.nodes[to].incoming) {
            const double weight = edge.rhythmWeight * (edge.sameTime ? 2.0 : 1.0);
            totalRelationWeight += weight;
            if (assignedLane[to] < 0 || assignedLane[edge.from] < 0) {
                lostRelationWeight += weight;
                totalRelationDistance += weight * 1.5;
                continue;
            }
            const int sourceDelta = graph.nodes[to].sourceLane - graph.nodes[edge.from].sourceLane;
            const int targetDelta = assignedLane[to] - assignedLane[edge.from];
            const double normalizedTargetDelta =
                static_cast<double>(targetDelta) /
                static_cast<double>(std::max(1, options.targetKeyCount - 1));
            const double targetAbsolute = std::abs(normalizedTargetDelta);
            const double targetSameLane = targetDelta == 0 ? 1.0 : 0.0;
            const double targetNormalizedTime =
                static_cast<double>(assignedTime[to] - assignedTime[edge.from]) /
                beatLengthAt(source, graph.nodes[edge.from].note->time);
            const double targetPhaseDelta =
                rhythmPhaseAt(source, assignedTime[to]) -
                rhythmPhaseAt(source, assignedTime[edge.from]);
            const double targetSameTime =
                std::abs(assignedTime[to] - assignedTime[edge.from]) <=
                        options.sameTimeEpsilonMs
                    ? 1.0
                    : 0.0;
            double distance =
                2.2 * huber(normalizedTargetDelta - edge.normalizedLaneDelta) +
                1.2 * huber(targetAbsolute - edge.absoluteLaneDelta) +
                1.4 * huber(targetSameLane -
                            (edge.sameSourceLane ? 1.0 : 0.0),
                            0.5) +
                huber(targetSameLane - edge.repeatSimilarity, 0.5) +
                1.2 * huber(targetNormalizedTime - edge.normalizedTimeDelta) +
                0.5 * huber(targetPhaseDelta - edge.rhythmPhaseDelta) +
                0.6 * huber(targetSameTime - (edge.sameTime ? 1.0 : 0.0), 0.5);
            if (sourceDelta != 0 && targetDelta == 0) {
                ++result.stats.collapsedRelations;
                distance += 0.8;
            } else if ((sourceDelta > 0 && targetDelta < 0) ||
                       (sourceDelta < 0 && targetDelta > 0)) {
                ++result.stats.directionReversals;
                distance += 1.1;
            }
            totalRelationDistance += weight * distance;
            lostRelationWeight += weight * clamp01(distance / 2.2);
        }
    }
    result.stats.relationPreservation = totalRelationWeight <= 0.0
                                            ? 1.0
                                            : clamp01(1.0 - lostRelationWeight /
                                                                 totalRelationWeight);
    if (totalRelationWeight > 0.0) {
        result.stats.relationLoss = totalRelationDistance / totalRelationWeight;
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
    measureContractionPlayability(result.chart, options, personality, result.stats);
    finalizeDifficulty(source, result.chart, options, result.stats);
    return result;
}

int snapToRhythmGrid(const Chart& chart, int desiredTime) {
    int anchor = 0;
    double beatLength = 500.0;
    for (const auto& point : chart.timingPoints) {
        if (point.time > desiredTime) {
            break;
        }
        if (point.beatLength > 0.0 && point.uninherited.value_or(true)) {
            anchor = point.time;
            beatLength = point.beatLength;
        }
    }
    const double quantum = std::max(30.0, beatLength / 4.0);
    return anchor + static_cast<int>(
                        std::llround(static_cast<double>(desiredTime - anchor) / quantum) *
                        quantum);
}

bool virtualCandidateSafe(const Chart& chart,
                          int time,
                          int lane,
                          int targetKeys,
                          int sameTimeEpsilonMs) {
    int simultaneous = 0;
    for (const auto& note : chart.notes) {
        if (std::abs(note.time - time) <= sameTimeEpsilonMs) {
            ++simultaneous;
            if (note.lane == lane) {
                return false;
            }
        }
        if (note.lane != lane) {
            continue;
        }
        if (std::abs(note.time - time) < kVirtualMinimumGapMs) {
            return false;
        }
        if (note.type == NoteType::Hold && note.endTime.has_value() &&
            note.time <= time && *note.endTime >= time) {
            return false;
        }
        // A virtual event has no source identity that can justify a new jack.
        if (std::abs(note.time - time) <= 180) {
            return false;
        }
    }
    return simultaneous < targetKeys;
}

struct SliceRepresentative {
    int time = 0;
    int lane = 0;
    int sourceLane = 0;
    const Note* anchor = nullptr;
};

struct VirtualPressureCandidate {
    int time = 0;
    int lane = 0;
    std::size_t anchorIndex = 0;
    Pressure weight = 0;
    ObjectiveBreakdown objective;
};

std::vector<SliceRepresentative> phraseRepresentatives(
    const RelationalGraphV2& graph,
    const PhraseRange& phrase,
    const std::vector<int>& committed) {
    std::vector<SliceRepresentative> representatives;
    std::size_t begin = phrase.begin;
    while (begin < phrase.end) {
        std::size_t end = begin + 1;
        int laneSum = committed[begin];
        int sourceLaneSum = graph.nodes[begin].sourceLane;
        while (end < phrase.end &&
               graph.nodes[end].slice == graph.nodes[begin].slice) {
            laneSum += committed[end];
            sourceLaneSum += graph.nodes[end].sourceLane;
            ++end;
        }
        representatives.push_back(
            {graph.nodes[begin].note->time,
             static_cast<int>(std::llround(
                 static_cast<double>(laneSum) /
                 static_cast<double>(end - begin))),
             static_cast<int>(std::llround(
                 static_cast<double>(sourceLaneSum) /
                 static_cast<double>(end - begin))),
             graph.nodes[begin].note});
        begin = end;
    }
    return representatives;
}

Pressure ratioFixed(int numerator, int denominator) {
    if (numerator <= 0 || denominator <= 0) {
        return 0;
    }
    return std::min(kPressureScale,
                    static_cast<Pressure>(numerator) * kPressureScale /
                        denominator);
}

Pressure relationExpandabilityFor(
    const std::vector<SliceRepresentative>& representatives) {
    if (representatives.size() < 3) {
        return 150'000;
    }
    int periodicMatches = 0;
    int periodicComparisons = 0;
    int stateRepeats = 0;
    int stateComparisons = 0;
    int alternatingDirections = 0;
    int directionComparisons = 0;
    for (std::size_t index = 2; index < representatives.size(); ++index) {
        const int previousInterval =
            representatives[index - 1].time - representatives[index - 2].time;
        const int currentInterval =
            representatives[index].time - representatives[index - 1].time;
        if (previousInterval > 0 && currentInterval > 0) {
            ++periodicComparisons;
            const int tolerance = std::max(previousInterval, currentInterval) / 5;
            periodicMatches +=
                std::abs(previousInterval - currentInterval) <= tolerance ? 1 : 0;
        }
        ++stateComparisons;
        stateRepeats += representatives[index].sourceLane ==
                                representatives[index - 2].sourceLane
                            ? 1
                            : 0;
        const int previousDirection =
            representatives[index - 1].sourceLane -
            representatives[index - 2].sourceLane;
        const int currentDirection =
            representatives[index].sourceLane -
            representatives[index - 1].sourceLane;
        if (previousDirection != 0 && currentDirection != 0) {
            ++directionComparisons;
            alternatingDirections +=
                (previousDirection > 0) != (currentDirection > 0) ? 1 : 0;
        }
    }
    const Pressure periodic = ratioFixed(periodicMatches, periodicComparisons);
    const Pressure recurrence = ratioFixed(stateRepeats, stateComparisons);
    const Pressure alternation =
        ratioFixed(alternatingDirections, directionComparisons);
    return std::clamp(120'000 + multiplyFixed(420'000, periodic) +
                          multiplyFixed(320'000, recurrence) +
                          multiplyFixed(140'000, alternation),
                      Pressure{0},
                      kPressureScale);
}

Pressure densityHeadroomFor(const PhraseMetrics& metrics, int targetKeys) {
    const Pressure playableLoad = std::max(
        Pressure{2} * kPressureScale,
        static_cast<Pressure>(targetKeys) * 720'000 + kPressureScale);
    const Pressure loadRatio = std::min(
        kPressureScale,
        metrics.peakLoadUnits * kPressureScale / playableLoad);
    return kPressureScale - loadRatio;
}

Pressure phraseNotePressureMass(std::size_t noteCount,
                                Pressure keyExpansionPressure,
                                Pressure relationExpandability,
                                Pressure densityHeadroom,
                                Pressure difficultyFactor) {
    Pressure mass = static_cast<Pressure>(noteCount) * keyExpansionPressure;
    mass = multiplyFixed(mass, relationExpandability);
    mass = multiplyFixed(mass, densityHeadroom);
    return multiplyFixed(mass, difficultyFactor);
}

void activateVirtualNotes(RelationalConversionResult& result,
                          const Chart& source,
                          const nk2::NK2Options& options,
                          const RelationalGraphV2& graph,
                          const std::vector<PhraseMetrics>& phraseMetrics,
                          const std::vector<int>& committed,
                          const Personality& personality) {
    std::vector<int> laneUse(static_cast<std::size_t>(options.targetKeyCount), 0);
    for (const auto& note : result.chart.notes) {
        if (note.lane >= 0 && note.lane < options.targetKeyCount) {
            ++laneUse[static_cast<std::size_t>(note.lane)];
        }
    }

    const Pressure keyExpansionPressure = keyExpansionPressureFor(
        options.sourceKeyCount, options.targetKeyCount);
    const Pressure difficultyFactor = std::clamp(
        fixedFromDouble(options.relationalDifficultyGoal),
        Pressure{250'000},
        Pressure{3'000'000});
    result.stats.keyExpansionPressure = fixedToDouble(keyExpansionPressure);
    Pressure pressureResidual = 0;
    Pressure relationExpandabilitySum = 0;
    Pressure densityHeadroomSum = 0;
    int measuredPhrases = 0;
    int virtualId = 0;
    for (std::size_t phraseIndex = 0; phraseIndex < graph.phrases.size(); ++phraseIndex) {
        const auto& metrics = phraseMetrics[phraseIndex];
        const auto representatives = phraseRepresentatives(
            graph, graph.phrases[phraseIndex], committed);
        if (representatives.size() < 2) {
            continue;
        }
        Pressure relationExpandability =
            relationExpandabilityFor(representatives);
        const Pressure densityHeadroom =
            densityHeadroomFor(metrics, options.targetKeyCount);
        std::vector<VirtualPressureCandidate> candidates;
        for (std::size_t index = 1; index < representatives.size(); ++index) {
            const auto& left = representatives[index - 1];
            const auto& right = representatives[index];
            const int interval = right.time - left.time;
            if (interval < 180 || interval > 1000) {
                continue;
            }
            const int candidateTime = snapToRhythmGrid(
                source, left.time + interval / 2);
            if (candidateTime - left.time < kVirtualMinimumGapMs ||
                right.time - candidateTime < kVirtualMinimumGapMs) {
                continue;
            }
            ++result.stats.virtualCandidates;

            int bestLane = -1;
            Pressure bestWeight = 0;
            ObjectiveBreakdown bestObjective;
            for (int lane = 0; lane < options.targetKeyCount; ++lane) {
                if (!virtualCandidateSafe(result.chart,
                                          candidateTime,
                                          lane,
                                          options.targetKeyCount,
                                          std::max(0, options.sameTimeEpsilonMs))) {
                    continue;
                }
                const int span = std::max(1, options.targetKeyCount - 1);
                const bool unused = laneUse[static_cast<std::size_t>(lane)] == 0;
                const Pressure timingSymmetry = std::clamp(
                    kPressureScale -
                        static_cast<Pressure>(std::abs(
                            (candidateTime - left.time) -
                            (right.time - candidateTime))) *
                            kPressureScale / interval,
                    Pressure{0},
                    kPressureScale);
                Pressure spatialCompletion = 0;
                if (left.lane == right.lane) {
                    spatialCompletion =
                        static_cast<Pressure>(std::abs(lane - left.lane)) *
                        kPressureScale / span;
                } else {
                    const Pressure midpointDistance =
                        static_cast<Pressure>(std::abs(
                            2 * lane - left.lane - right.lane)) *
                        kPressureScale / (2 * span);
                    spatialCompletion = std::max(
                        Pressure{0}, kPressureScale - midpointDistance);
                }
                const Pressure distinctRole =
                    lane != left.lane && lane != right.lane
                        ? kPressureScale
                        : 0;
                const Pressure completionShape =
                    multiplyFixed(750'000, timingSymmetry) +
                    multiplyFixed(450'000, spatialCompletion) +
                    (unused ? 500'000 : 0) +
                    multiplyFixed(200'000, distinctRole);
                const Pressure completionGainFixed = multiplyFixed(
                    850'000 + multiplyFixed(fixedFromDouble(personality.spread),
                                            650'000),
                    completionShape);
                const Pressure noveltyShape =
                    120'000 + (unused ? 420'000 : 0) +
                    multiplyFixed(160'000, distinctRole);
                const Pressure noveltyGainFixed = multiplyFixed(
                    fixedFromDouble(personality.novelty), noveltyShape);
                const Pressure movementShape =
                    static_cast<Pressure>(std::abs(lane - left.lane) +
                                          std::abs(right.lane - lane)) *
                    kPressureScale / span;
                const Pressure movementLossFixed = multiplyFixed(
                    multiplyFixed(fixedFromDouble(personality.ergonomics),
                                  movementShape),
                    180'000);
                const Pressure difficultyLossFixed = multiplyFixed(
                    std::max(Pressure{0}, kPressureScale - difficultyFactor),
                    700'000);
                ObjectiveBreakdown objective;
                objective.edit = metrics.addCost *
                                 (1.20 - personality.novelty * 0.42);
                objective.move = fixedToDouble(movementLossFixed);
                objective.difficulty = fixedToDouble(difficultyLossFixed);
                objective.completion = fixedToDouble(completionGainFixed);
                objective.novelty = fixedToDouble(noveltyGainFixed);
                const Pressure weight = std::max(
                    Pressure{0},
                    completionGainFixed + noveltyGainFixed - movementLossFixed -
                        difficultyLossFixed);
                ++result.stats.expandedStates;
                if (weight > bestWeight ||
                    (weight == bestWeight && weight > 0 && lane < bestLane)) {
                    bestLane = lane;
                    bestWeight = weight;
                    bestObjective = objective;
                }
            }

            if (bestLane < 0 || bestWeight <= 0) {
                ++result.stats.rejectedAdds;
                continue;
            }
            candidates.push_back(
                {candidateTime, bestLane, index - 1, bestWeight, bestObjective});
        }

        const int potentialCandidates =
            static_cast<int>(representatives.size() - 1);
        relationExpandability = multiplyFixed(
            relationExpandability,
            ratioFixed(static_cast<int>(candidates.size()), potentialCandidates));
        relationExpandabilitySum += relationExpandability;
        densityHeadroomSum += densityHeadroom;
        ++measuredPhrases;

        const Pressure phraseMass = phraseNotePressureMass(
            graph.phrases[phraseIndex].end - graph.phrases[phraseIndex].begin,
            keyExpansionPressure,
            relationExpandability,
            densityHeadroom,
            difficultyFactor);
        result.stats.notePressureMassUnits += phraseMass;
        if (candidates.empty() || phraseMass <= 0) {
            continue;
        }
        std::stable_sort(candidates.begin(), candidates.end(), [](const auto& lhs,
                                                                  const auto& rhs) {
            if (lhs.time != rhs.time) {
                return lhs.time < rhs.time;
            }
            if (lhs.anchorIndex != rhs.anchorIndex) {
                return lhs.anchorIndex < rhs.anchorIndex;
            }
            return lhs.lane < rhs.lane;
        });
        const Pressure totalWeight = std::accumulate(
            candidates.begin(),
            candidates.end(),
            Pressure{0},
            [](Pressure sum, const auto& candidate) {
                return sum + candidate.weight;
            });
        Pressure allocatedMass = 0;
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            const auto& candidate = candidates[index];
            const Pressure share =
                index + 1 == candidates.size()
                    ? phraseMass - allocatedMass
                    : phraseMass * candidate.weight / totalWeight;
            allocatedMass += share;
            pressureResidual += share;
            const bool triggered = pressureResidual >= kPressureScale;
            const bool safe = triggered && virtualCandidateSafe(
                                               result.chart,
                                               candidate.time,
                                               candidate.lane,
                                               options.targetKeyCount,
                                               std::max(0,
                                                        options.sameTimeEpsilonMs));
            if (!safe) {
                ++result.stats.rejectedAdds;
                continue;
            }
            Note added;
            const auto& anchor = representatives[candidate.anchorIndex];
            added.id = "nk3-virtual-" + anchor.anchor->id + "-" +
                       std::to_string(virtualId++);
            added.time = candidate.time;
            added.lane = candidate.lane;
            added.sourceLane = anchor.sourceLane;
            added.type = NoteType::Tap;
            added.raw = "nk3-deterministic-pressure-add";
            result.chart.notes.push_back(std::move(added));
            ++laneUse[static_cast<std::size_t>(candidate.lane)];
            ++result.stats.addedNotes;
            pressureResidual -= kPressureScale;
            result.stats.editLoss += candidate.objective.edit;
            result.stats.moveLoss += candidate.objective.move;
            result.stats.difficultyLoss += candidate.objective.difficulty;
            result.stats.completionGain += candidate.objective.completion;
            result.stats.noveltyGain += candidate.objective.novelty;
        }
    }

    result.stats.notePressureResidualUnits = pressureResidual;
    result.stats.notePressureMass =
        fixedToDouble(result.stats.notePressureMassUnits);
    result.stats.notePressureResidual = fixedToDouble(pressureResidual);
    if (measuredPhrases > 0) {
        result.stats.relationExpandabilityMean = fixedToDouble(
            relationExpandabilitySum / measuredPhrases);
        result.stats.densityHeadroomMean = fixedToDouble(
            densityHeadroomSum / measuredPhrases);
    }

    std::stable_sort(result.chart.notes.begin(), result.chart.notes.end(), [](const Note& lhs,
                                                                             const Note& rhs) {
        if (lhs.time != rhs.time) {
            return lhs.time < rhs.time;
        }
        if (lhs.lane != rhs.lane) {
            return lhs.lane < rhs.lane;
        }
        return lhs.id < rhs.id;
    });
}

}  // namespace

bool supportsRelationalRechart(const nk2::NK2Options& options) {
    if (!options.useNk3 || options.mode == nk2::Mode::Report ||
        options.sourceKeyCount <= 0 ||
        options.sourceKeyCount > nk2::kMaxSupportedKeyCount ||
        options.targetKeyCount <= 0 ||
        options.targetKeyCount > nk2::kMaxSupportedKeyCount) {
        return false;
    }
    if (options.sourceKeyCount != options.targetKeyCount) {
        return true;
    }
    // A one-key same-key chart has no spatial relation to re-render.
    return options.sourceKeyCount > 1 &&
           (options.mode == nk2::Mode::Transform ||
            options.mode == nk2::Mode::RemixedRemastered ||
            options.mode == nk2::Mode::Remaster);
}

RelationalConversionResult convertRelational(const Chart& source,
                                             const nk2::NK2Options& options) {
    RelationalConversionResult result;
    result.chart = source;
    const auto graph = buildRelationalGraph(source,
                                            options.sourceKeyCount,
                                            std::max(0, options.sameTimeEpsilonMs));
    result.stats.graphNodes = static_cast<int>(graph.nodes.size());
    result.stats.graphEdges = graph.edgeCount;
    result.stats.phrases = static_cast<int>(graph.phrases.size());
    if (graph.nodes.empty()) {
        return result;
    }
    if (options.targetKeyCount < options.sourceKeyCount) {
        return convertContraction(source, options, graph);
    }

    const auto phraseMetrics = buildPhraseMetrics(graph,
                                                  options.sourceKeyCount,
                                                  options.targetKeyCount,
                                                  result.stats);
    const Personality personality = personalityFor(options);
    BeamState seed = initialState(options.targetKeyCount);
    std::vector<int> committed;
    committed.reserve(graph.nodes.size());

    for (const auto& phrase : graph.phrases) {
        seed.assignments.clear();
        std::vector<BeamState> beam{seed};
        for (std::size_t index = phrase.begin; index < phrase.end; ++index) {
            std::vector<BeamState> next;
            next.reserve(beam.size() * static_cast<std::size_t>(options.targetKeyCount));
            for (const auto& state : beam) {
                expandState(graph.nodes[index],
                            state,
                            phrase,
                            committed,
                            personality,
                            options.sourceKeyCount,
                            options.targetKeyCount,
                            false,
                            next);
            }
            if (next.empty()) {
                ++result.stats.fallbacks;
                for (const auto& state : beam) {
                    expandState(graph.nodes[index],
                                state,
                                phrase,
                                committed,
                                personality,
                                options.sourceKeyCount,
                                options.targetKeyCount,
                                true,
                                next);
                }
            }
            if (next.empty()) {
                break;
            }
            result.stats.expandedStates += static_cast<int>(next.size());
            std::stable_sort(next.begin(), next.end(), stateBefore);
            if (next.size() > kBeamWidth) {
                next.resize(kBeamWidth);
            }
            beam = std::move(next);
        }
        if (beam.empty() || beam.front().assignments.size() != phrase.end - phrase.begin) {
            ++result.stats.fallbacks;
            break;
        }
        for (auto& state : beam) {
            state.objective.edit +=
                missingLanePenalty(state, personality, options.targetKeyCount);
            state.cost = state.objective.total();
        }
        std::stable_sort(beam.begin(), beam.end(), stateBefore);
        seed = beam.front();
        committed.insert(committed.end(), seed.assignments.begin(), seed.assignments.end());
    }

    if (committed.size() != graph.nodes.size()) {
        result.chart.notes.clear();
        return result;
    }
    result.stats.relationLoss = seed.objective.relation;
    result.stats.editLoss = seed.objective.edit;
    result.stats.moveLoss = seed.objective.move;
    result.stats.handLoss = seed.objective.hand;
    result.stats.jackLoss = seed.objective.jack;
    result.stats.timeLoss = seed.objective.time;
    result.stats.completionGain = seed.objective.completion;
    result.stats.noveltyGain = seed.objective.novelty;
    result.stats.keptNotes = static_cast<int>(graph.nodes.size());
    result.stats.relationPreservation =
        graph.edgeCount <= 0
            ? 1.0
            : clamp01(1.0 - result.stats.relationLoss /
                                 static_cast<double>(graph.edgeCount));
    result.chart.notes.clear();
    result.chart.notes.reserve(graph.nodes.size() + graph.phrases.size() * 8);
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        Note mapped = *graph.nodes[index].note;
        mapped.sourceLane = graph.nodes[index].sourceLane;
        mapped.lane = committed[index];
        result.chart.notes.push_back(std::move(mapped));
    }
    activateVirtualNotes(result,
                         source,
                         options,
                         graph,
                         phraseMetrics,
                         committed,
                         personality);
    finalizeDifficulty(source, result.chart, options, result.stats);
    return result;
}

}  // namespace keyconv::nk3
