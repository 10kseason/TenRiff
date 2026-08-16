#pragma once

#include <cstdint>

#include "nk2/chart.hpp"

#include "nk2/nk2_report.hpp"

namespace keyconv::nk3 {

struct RelationalStats {
    int graphNodes = 0;
    int graphEdges = 0;
    int phrases = 0;
    int expandedStates = 0;
    int fallbacks = 0;
    int collapsedRelations = 0;
    int directionReversals = 0;
    int chordContractions = 0;
    int microRolls = 0;
    int droppedNotes = 0;
    int keptNotes = 0;
    int shiftedNotes = 0;
    int virtualCandidates = 0;
    int addedNotes = 0;
    int rejectedAdds = 0;
    int peakChromaticDemand = 0;
    std::int64_t notePressureMassUnits = 0;
    std::int64_t notePressureResidualUnits = 0;
    double effectiveFidelity = 0.0;
    double effectiveNovelty = 0.0;
    double effectiveSpread = 0.0;
    double effectiveErgonomics = 0.0;
    double effectiveHandBalance = 0.0;
    double relationPreservation = 1.0;
    double pressureMean = 0.0;
    double pressureMin = 0.0;
    double pressureMax = 0.0;
    double peakLoad = 0.0;
    double keyExpansionPressure = 0.0;
    double notePressureMass = 0.0;
    double notePressureResidual = 0.0;
    double relationExpandabilityMean = 0.0;
    double densityHeadroomMean = 0.0;
    double sourceDifficulty = 0.0;
    double resultDifficulty = 0.0;
    double difficultyGoal = 0.0;
    double relationLoss = 0.0;
    double editLoss = 0.0;
    double moveLoss = 0.0;
    double handLoss = 0.0;
    double jackLoss = 0.0;
    double timeLoss = 0.0;
    double difficultyLoss = 0.0;
    double completionGain = 0.0;
    double noveltyGain = 0.0;
    double objective = 0.0;
};

struct RelationalConversionResult {
    Chart chart;
    RelationalStats stats;
};

bool supportsRelationalRechart(const nk2::NK2Options& options);
RelationalConversionResult convertRelational(const Chart& source,
                                             const nk2::NK2Options& options);

}  // namespace keyconv::nk3
