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
    // Transform pushed further: a 65% support budget and tighter safety windows,
    // for a deliberate rewrite rather than a relane.
    RemixedRemastered,
    // Same budget as RemixedRemastered, but the caller pairs it with a high
    // anchor bias so the extra notes land around the original placement.
    Remaster,
    Report,
};

struct NK2Options {
    int sourceKeyCount = 0;
    int targetKeyCount = 0;
    Mode mode = Mode::Native;
    // Explicit opt-in keeps existing nK2 replay semantics stable.
    bool useNk3 = false;
    double nativeWeight = 0.5;
    double remixWeight = 0.5;
    LayoutWeights layoutWeights;
    int sameTimeEpsilonMs = 2;
    bool superSymmetry = false;
    // Hold notes are excluded from the "free tap" relaning budget, so they stay
    // near their source lane. 0 keeps that behaviour; 1 gives them the same
    // freedom an unconstrained tap gets.
    // Overrides the mode's support-note budget when > 0.
    double supportBudgetRatio = 0.0;
    // Override the mode's support safety windows when > 0. The jack window is
    // the bigger lever on how many support notes actually land.
    int supportJackWindowMs = 0;
    int supportSameSourceGapMs = 0;
    // Source density (notes/sec) at which the support budget stops being
    // tapered. 0 uses the engine default, negative disables the taper.
    double supportDensityReferenceNps = 0.0;
    double lnSpread = 0.0;
    // 8K targets carry hardcoded tuning that deliberately spreads notes off
    // their source lane. 0 keeps it; 1 neutralises it so the relane stays
    // recognisable as the original chart.
    double anchorBias = 0.0;
    // Extra pull toward target lanes that no source lane maps onto. 0 disables.
    double gapLaneBoost = 0.0;
    // Support notes are always taps. With this on, one anchored to an LN head
    // becomes a hold spanning the same range, filling the LN section.
    bool lnFill = false;
    double relationalFidelity = -1.0;
    double relationalNovelty = -1.0;
    double relationalSpread = -1.0;
    double relationalErgonomics = -1.0;
    double relationalHandBalance = -1.0;
    double relationalDifficultyGoal = 1.0;
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
