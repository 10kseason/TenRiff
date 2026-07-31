#pragma once

#include <string>
#include <vector>

#include "intent_graph.hpp"
#include "layout_model.hpp"

namespace keyconv::nk2 {

inline constexpr int kMaxSupportedKeyCount = 18;

enum class Engine {
    Classic,
    NK2,
};

enum class Mode {
    Native,
    Faithful,
    Harder,
    Transform,
    Report,
};

struct NK2Options {
    int sourceKeyCount = 0;
    int targetKeyCount = 0;
    Mode mode = Mode::Native;
    double nativeWeight = 0.5;
    double remixWeight = 0.5;
    LayoutWeights layoutWeights;
    int sameTimeEpsilonMs = 2;
    bool superSymmetry = false;
};

struct NK2Report {
    NK2Options options;
    IntentGraphSummary intent;
    TargetLayoutSummary layout;
    bool chartMutated = false;
    bool noOp = false;
    std::string noOpReason;
    std::string prototypeName;
    int outputNotes = 0;
    int addedNotes = 0;
    int droppedNotes = 0;
    int localSolverWindows = 0;
    int localSolverCandidates = 0;
    int localSolverFallbacks = 0;
    int lowerKeyRolledNotes = 0;
    int superSymmetryMirrorAnchors = 0;
    int superSymmetryGaplessStairs = 0;
    int sameTimeCollisions = 0;
    int longNoteConflicts = 0;
    int createdJacks = 0;
    int preservedSourceJacks = 0;
    int sourceAnchorMatches = 0;
    int sourceAnchorTotal = 0;
    int motifJackPlacements = 0;
    int motifTrillPlacements = 0;
    int motifStairPlacements = 0;
    int motifStreamPlacements = 0;
    int motifChordPlacements = 0;
    int motifLnPlacements = 0;
    int motifNeutralPlacements = 0;
    int lnSupportCandidates = 0;
    int lnSupportAccepted = 0;
    int lnSupportRejected = 0;
    int strongBeatSupportCandidates = 0;
    int strongBeatSupportAccepted = 0;
    int strongBeatSupportRejected = 0;
    int mirrorSupportCandidates = 0;
    int mirrorSupportAccepted = 0;
    int mirrorSupportRejected = 0;
    int supportRejectedByBudget = 0;
    int supportRejectedByPhraseBudget = 0;
    int supportPhraseWindows = 0;
    int supportRejectedBySafety = 0;
    int generatedFromJackMotif = 0;
    int generatedFromTrillMotif = 0;
    int generatedFromStairMotif = 0;
    int generatedFromStreamMotif = 0;
    int generatedFromChordMotif = 0;
    int generatedFromLnMotif = 0;
    int generatedFromNeutralMotif = 0;
    double panelScore = 0.0;
    double leftPanelScore = 0.0;
    double rightPanelScore = 0.0;
    double bridgeScore = 0.0;
    double fullFieldScore = 0.0;
    double layoutCoverageScore = 0.0;
    double sourceAnchorScore = 0.0;
    double phraseProfileScore = 0.0;
    int phraseProfileWindows = 0;
    int phraseProfileOverBudgetWindows = 0;
    std::vector<int> laneDistribution;
    std::vector<std::string> warnings;
};

std::string toString(Engine engine);
std::string toString(Mode mode);
Mode parseModeOrThrow(const std::string& value);
Engine parseEngineOrThrow(const std::string& value);

std::string reportToJson(const NK2Report& report);
std::string reportToText(const NK2Report& report);

}  // namespace keyconv::nk2
