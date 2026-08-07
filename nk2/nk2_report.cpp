#include "nk2/nk2_report.hpp"

#include <sstream>
#include <stdexcept>

namespace keyconv::nk2 {

namespace {

std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
            case '\\':
                out << "\\\\";
                break;
            case '"':
                out << "\\\"";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                out << ch;
                break;
        }
    }
    return out.str();
}

}  // namespace

std::string toString(Engine engine) {
    switch (engine) {
        case Engine::Classic:
            return "classic";
        case Engine::NK2:
            return "nk2";
    }
    return "classic";
}

std::string toString(Mode mode) {
    switch (mode) {
        case Mode::Native:
            return "native";
        case Mode::Faithful:
            return "faithful";
        case Mode::Harder:
            return "harder";
        case Mode::Transform:
            return "transform";
        case Mode::RemixedRemastered:
            return "remixed-remastered";
        case Mode::Remaster:
            return "remaster";
        case Mode::Report:
            return "report";
    }
    return "native";
}

Mode parseModeOrThrow(const std::string& value) {
    if (value == "native") {
        return Mode::Native;
    }
    if (value == "faithful") {
        return Mode::Faithful;
    }
    if (value == "harder") {
        return Mode::Harder;
    }
    if (value == "transform") {
        return Mode::Transform;
    }
    if (value == "remixed-remastered" || value == "remixed-remasterd" || value == "rr") {
        return Mode::RemixedRemastered;
    }
    if (value == "remaster" || value == "rm") {
        return Mode::Remaster;
    }
    if (value == "report") {
        return Mode::Report;
    }
    throw std::runtime_error("Invalid NK2 mode: " + value);
}

Engine parseEngineOrThrow(const std::string& value) {
    if (value == "classic") {
        return Engine::Classic;
    }
    if (value == "nk2") {
        return Engine::NK2;
    }
    throw std::runtime_error("Invalid engine: " + value);
}

std::string reportToJson(const NK2Report& report) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"engine\": \"nk2\",\n";
    out << "  \"mode\": \"" << toString(report.options.mode) << "\",\n";
    out << "  \"superSymmetry\": " << (report.options.superSymmetry ? "true" : "false") << ",\n";
    out << "  \"chartMutated\": " << (report.chartMutated ? "true" : "false") << ",\n";
    out << "  \"noOp\": " << (report.noOp ? "true" : "false") << ",\n";
    out << "  \"noOpReason\": \"" << jsonEscape(report.noOpReason) << "\",\n";
    out << "  \"prototypeName\": \"" << jsonEscape(report.prototypeName) << "\",\n";
    out << "  \"weights\": {\n";
    out << "    \"native\": " << report.options.nativeWeight << ",\n";
    out << "    \"remix\": " << report.options.remixWeight << ",\n";
    out << "    \"panel\": " << report.options.layoutWeights.panel << ",\n";
    out << "    \"bridge\": " << report.options.layoutWeights.bridge << ",\n";
    out << "    \"fullField\": " << report.options.layoutWeights.fullField << "\n";
    out << "  },\n";
    out << "  \"layout\": {\n";
    out << "    \"kind\": \"" << layoutKindName(report.layout) << "\",\n";
    out << "    \"targetKeyCount\": " << report.layout.targetKeyCount << ",\n";
    out << "    \"hasPanels\": " << (report.layout.hasPanels ? "true" : "false") << ",\n";
    out << "    \"hasBridge\": " << (report.layout.hasBridge ? "true" : "false") << ",\n";
    out << "    \"leftPanel\": [" << report.layout.leftPanelStart << ", " << report.layout.leftPanelEnd << "],\n";
    out << "    \"rightPanel\": [" << report.layout.rightPanelStart << ", " << report.layout.rightPanelEnd << "],\n";
    out << "    \"bridge\": [" << report.layout.bridgeStart << ", " << report.layout.bridgeEnd << "]\n";
    out << "  },\n";
    out << "  \"intent\": {\n";
    out << "    \"sourceKeyCount\": " << report.intent.sourceKeyCount << ",\n";
    out << "    \"targetKeyCount\": " << report.intent.targetKeyCount << ",\n";
    out << "    \"totalNotes\": " << report.intent.totalNotes << ",\n";
    out << "    \"tapNotes\": " << report.intent.tapNotes << ",\n";
    out << "    \"holdNotes\": " << report.intent.holdNotes << ",\n";
    out << "    \"slices\": " << report.intent.slices << ",\n";
    out << "    \"chordSlices\": " << report.intent.chordSlices << ",\n";
    out << "    \"jackMotifs\": " << report.intent.jackMotifs << ",\n";
    out << "    \"trillMotifs\": " << report.intent.trillMotifs << ",\n";
    out << "    \"stairMotifs\": " << report.intent.stairMotifs << ",\n";
    out << "    \"streamMotifs\": " << report.intent.streamMotifs << ",\n";
    out << "    \"lnAnchors\": " << report.intent.lnAnchors << ",\n";
    out << "    \"strongBeatAnchors\": " << report.intent.strongBeatAnchors << ",\n";
    out << "    \"mirrorSupportCandidates\": " << report.intent.mirrorSupportCandidates << ",\n";
    out << "    \"recognizabilityAnchors\": " << report.intent.recognizabilityAnchors << ",\n";
    out << "    \"averageLocalNps\": " << report.intent.averageLocalNps << "\n";
    out << "  },\n";
    out << "  \"placement\": {\n";
    out << "    \"outputNotes\": " << report.outputNotes << ",\n";
    out << "    \"addedNotes\": " << report.addedNotes << ",\n";
    out << "    \"droppedNotes\": " << report.droppedNotes << ",\n";
    out << "    \"localSolverWindows\": " << report.localSolverWindows << ",\n";
    out << "    \"localSolverCandidates\": " << report.localSolverCandidates << ",\n";
    out << "    \"localSolverFallbacks\": " << report.localSolverFallbacks << ",\n";
    out << "    \"lowerKeyRolledNotes\": " << report.lowerKeyRolledNotes << ",\n";
    out << "    \"superSymmetryMirrorAnchors\": " << report.superSymmetryMirrorAnchors << ",\n";
    out << "    \"superSymmetryGaplessStairs\": " << report.superSymmetryGaplessStairs << ",\n";
    out << "    \"sameTimeCollisions\": " << report.sameTimeCollisions << ",\n";
    out << "    \"longNoteConflicts\": " << report.longNoteConflicts << ",\n";
    out << "    \"createdJacks\": " << report.createdJacks << ",\n";
    out << "    \"preservedSourceJacks\": " << report.preservedSourceJacks << ",\n";
    out << "    \"sourceAnchorMatches\": " << report.sourceAnchorMatches << ",\n";
    out << "    \"sourceAnchorTotal\": " << report.sourceAnchorTotal << ",\n";
    out << "    \"sourceAnchorScore\": " << report.sourceAnchorScore << ",\n";
    out << "    \"laneDistribution\": [";
    for (std::size_t i = 0; i < report.laneDistribution.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << report.laneDistribution[i];
    }
    out << "]\n";
    out << "  },\n";
    out << "  \"support\": {\n";
    out << "    \"lnCandidates\": " << report.lnSupportCandidates << ",\n";
    out << "    \"lnAccepted\": " << report.lnSupportAccepted << ",\n";
    out << "    \"lnRejected\": " << report.lnSupportRejected << ",\n";
    out << "    \"strongBeatCandidates\": " << report.strongBeatSupportCandidates << ",\n";
    out << "    \"strongBeatAccepted\": " << report.strongBeatSupportAccepted << ",\n";
    out << "    \"strongBeatRejected\": " << report.strongBeatSupportRejected << ",\n";
    out << "    \"mirrorCandidates\": " << report.mirrorSupportCandidates << ",\n";
    out << "    \"mirrorAccepted\": " << report.mirrorSupportAccepted << ",\n";
    out << "    \"mirrorRejected\": " << report.mirrorSupportRejected << ",\n";
    out << "    \"rejectedByBudget\": " << report.supportRejectedByBudget << ",\n";
    out << "    \"rejectedByPhraseBudget\": " << report.supportRejectedByPhraseBudget << ",\n";
    out << "    \"phraseWindows\": " << report.supportPhraseWindows << ",\n";
    out << "    \"rejectedBySafety\": " << report.supportRejectedBySafety << "\n";
    out << "  },\n";
    out << "  \"motifPlacements\": {\n";
    out << "    \"jack\": " << report.motifJackPlacements << ",\n";
    out << "    \"trill\": " << report.motifTrillPlacements << ",\n";
    out << "    \"stair\": " << report.motifStairPlacements << ",\n";
    out << "    \"stream\": " << report.motifStreamPlacements << ",\n";
    out << "    \"chord\": " << report.motifChordPlacements << ",\n";
    out << "    \"ln\": " << report.motifLnPlacements << ",\n";
    out << "    \"neutral\": " << report.motifNeutralPlacements << "\n";
    out << "  },\n";
    out << "  \"generatedProvenance\": {\n";
    out << "    \"fromJack\": " << report.generatedFromJackMotif << ",\n";
    out << "    \"fromTrill\": " << report.generatedFromTrillMotif << ",\n";
    out << "    \"fromStair\": " << report.generatedFromStairMotif << ",\n";
    out << "    \"fromStream\": " << report.generatedFromStreamMotif << ",\n";
    out << "    \"fromChord\": " << report.generatedFromChordMotif << ",\n";
    out << "    \"fromLn\": " << report.generatedFromLnMotif << ",\n";
    out << "    \"fromNeutral\": " << report.generatedFromNeutralMotif << "\n";
    out << "  },\n";
    out << "  \"layoutScores\": {\n";
    out << "    \"panel\": " << report.panelScore << ",\n";
    out << "    \"leftPanel\": " << report.leftPanelScore << ",\n";
    out << "    \"rightPanel\": " << report.rightPanelScore << ",\n";
    out << "    \"bridge\": " << report.bridgeScore << ",\n";
    out << "    \"fullField\": " << report.fullFieldScore << ",\n";
    out << "    \"coverage\": " << report.layoutCoverageScore << "\n";
    out << "  },\n";
    out << "  \"phraseProfile\": {\n";
    out << "    \"score\": " << report.phraseProfileScore << ",\n";
    out << "    \"windows\": " << report.phraseProfileWindows << ",\n";
    out << "    \"overBudgetWindows\": " << report.phraseProfileOverBudgetWindows << "\n";
    out << "  },\n";
    out << "  \"warnings\": [";
    for (std::size_t i = 0; i < report.warnings.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << "\"" << jsonEscape(report.warnings[i]) << "\"";
    }
    out << "]\n";
    out << "}\n";
    return out.str();
}

std::string reportToText(const NK2Report& report) {
    std::ostringstream out;
    out << (report.chartMutated ? "NK2 prototype conversion\n" : "NK2 report-only analysis\n");
    out << "Mode: " << toString(report.options.mode) << "\n";
    out << "Source keys: " << report.options.sourceKeyCount << "\n";
    out << "Target keys: " << report.options.targetKeyCount << "\n";
    out << "Super symmetry: " << (report.options.superSymmetry ? "on" : "off") << "\n";
    out << "Chart mutated: " << (report.chartMutated ? "yes" : "no") << "\n";
    if (!report.prototypeName.empty()) {
        out << "Prototype: " << report.prototypeName << "\n";
    }
    if (report.noOp) {
        out << "No-op: " << report.noOpReason << "\n";
    }
    out << "Native/remix weights: " << report.options.nativeWeight << " / "
        << report.options.remixWeight << "  LN spread: " << report.options.lnSpread << "\n";
    out << "Layout: " << layoutKindName(report.layout)
        << " panel/bridge/full-field=" << report.options.layoutWeights.panel << "/"
        << report.options.layoutWeights.bridge << "/" << report.options.layoutWeights.fullField << "\n";
    out << "Intent: notes=" << report.intent.totalNotes
        << " taps=" << report.intent.tapNotes
        << " holds=" << report.intent.holdNotes
        << " slices=" << report.intent.slices
        << " chords=" << report.intent.chordSlices << "\n";
    out << "Motifs: jacks=" << report.intent.jackMotifs
        << " trills=" << report.intent.trillMotifs
        << " stairs=" << report.intent.stairMotifs
        << " streams=" << report.intent.streamMotifs << "\n";
    out << "Anchors: LN=" << report.intent.lnAnchors
        << " strongBeat=" << report.intent.strongBeatAnchors
        << " mirrorSupportCandidates=" << report.intent.mirrorSupportCandidates
        << " recognizability=" << report.intent.recognizabilityAnchors << "\n";
    out << "Placement: outputNotes=" << report.outputNotes
        << " addedNotes=" << report.addedNotes
        << " droppedNotes=" << report.droppedNotes
        << " localSolverWindows=" << report.localSolverWindows
        << " localSolverCandidates=" << report.localSolverCandidates
        << " localSolverFallbacks=" << report.localSolverFallbacks
        << " lowerKeyRolledNotes=" << report.lowerKeyRolledNotes
        << " superSymmetryMirrorAnchors=" << report.superSymmetryMirrorAnchors
        << " superSymmetryGaplessStairs=" << report.superSymmetryGaplessStairs
        << " collisions=" << report.sameTimeCollisions
        << " lnConflicts=" << report.longNoteConflicts
        << " createdJacks=" << report.createdJacks
        << " preservedSourceJacks=" << report.preservedSourceJacks
        << " sourceAnchor=" << report.sourceAnchorMatches << "/"
        << report.sourceAnchorTotal << " score=" << report.sourceAnchorScore << "\n";
    out << "Support: LN=" << report.lnSupportAccepted << "/" << report.lnSupportCandidates
        << " rejected=" << report.lnSupportRejected
        << " strongBeat=" << report.strongBeatSupportAccepted << "/"
        << report.strongBeatSupportCandidates
        << " rejected=" << report.strongBeatSupportRejected
        << " mirror=" << report.mirrorSupportAccepted << "/"
        << report.mirrorSupportCandidates
        << " rejected=" << report.mirrorSupportRejected
        << " budgetRejects=" << report.supportRejectedByBudget
        << " phraseBudgetRejects=" << report.supportRejectedByPhraseBudget
        << " phraseWindows=" << report.supportPhraseWindows
        << " safetyRejects=" << report.supportRejectedBySafety << "\n";
    out << "Motif placements: jack=" << report.motifJackPlacements
        << " trill=" << report.motifTrillPlacements
        << " stair=" << report.motifStairPlacements
        << " stream=" << report.motifStreamPlacements
        << " chord=" << report.motifChordPlacements
        << " ln=" << report.motifLnPlacements
        << " neutral=" << report.motifNeutralPlacements << "\n";
    out << "Generated provenance: jack=" << report.generatedFromJackMotif
        << " trill=" << report.generatedFromTrillMotif
        << " stair=" << report.generatedFromStairMotif
        << " stream=" << report.generatedFromStreamMotif
        << " chord=" << report.generatedFromChordMotif
        << " ln=" << report.generatedFromLnMotif
        << " neutral=" << report.generatedFromNeutralMotif << "\n";
    out << "Layout scores: panel=" << report.panelScore
        << " leftPanel=" << report.leftPanelScore
        << " rightPanel=" << report.rightPanelScore
        << " bridge=" << report.bridgeScore
        << " fullField=" << report.fullFieldScore
        << " coverage=" << report.layoutCoverageScore << "\n";
    out << "Phrase profile: score=" << report.phraseProfileScore
        << " windows=" << report.phraseProfileWindows
        << " overBudgetWindows=" << report.phraseProfileOverBudgetWindows << "\n";
    if (!report.laneDistribution.empty()) {
        out << "Lane distribution:";
        for (std::size_t i = 0; i < report.laneDistribution.size(); ++i) {
            out << (i == 0 ? " " : ",") << i << "=" << report.laneDistribution[i];
        }
        out << "\n";
    }
    for (const auto& warning : report.warnings) {
        out << "Warning: " << warning << "\n";
    }
    return out.str();
}

}  // namespace keyconv::nk2
