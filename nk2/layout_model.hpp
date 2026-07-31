#pragma once

#include <string>

namespace keyconv::nk2 {

struct LayoutWeights {
    int panel = 3;
    int bridge = 2;
    int fullField = 6;
};

enum class LaneSide {
    Left = -1,
    Center = 0,
    Right = 1,
};

struct KeyLayoutProfile {
    int keyCount = 0;
    bool hasPanels = false;
    int leftStart = 0;
    int leftEnd = -1;
    int rightStart = 0;
    int rightEnd = -1;
    bool hasCenter = false;
    int centerLane = -1;
    bool hasBridge = false;
    int bridgeStart = 0;
    int bridgeEnd = -1;
    LayoutWeights weights;
};

struct TargetLayoutSummary {
    int targetKeyCount = 0;
    bool hasPanels = false;
    bool hasBridge = false;
    int leftPanelStart = 0;
    int leftPanelEnd = 0;
    int rightPanelStart = 0;
    int rightPanelEnd = 0;
    int bridgeStart = 0;
    int bridgeEnd = 0;
    LayoutWeights weights;
};

KeyLayoutProfile buildKeyLayoutProfile(int keyCount, LayoutWeights weights = {});
LaneSide laneSideFor(const KeyLayoutProfile& layout, int lane);
bool laneIsInBridge(const KeyLayoutProfile& layout, int lane);
int mirroredLaneFor(const KeyLayoutProfile& layout, int lane);
double normalizedLanePosition(const KeyLayoutProfile& layout, int lane);
int laneForNormalizedPosition(const KeyLayoutProfile& layout, double position);
double desiredLaneShareFor(const KeyLayoutProfile& layout, int lane);

TargetLayoutSummary buildTargetLayoutSummary(int targetKeyCount, LayoutWeights weights);
std::string layoutKindName(const TargetLayoutSummary& layout);

}  // namespace keyconv::nk2
