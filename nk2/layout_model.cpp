#include "nk2/layout_model.hpp"

#include <algorithm>
#include <cmath>

namespace keyconv::nk2 {

KeyLayoutProfile buildKeyLayoutProfile(int keyCount, LayoutWeights weights) {
    KeyLayoutProfile layout;
    layout.keyCount = std::max(0, keyCount);
    layout.weights = weights;
    if (layout.keyCount <= 0) {
        return layout;
    }

    const int split = layout.keyCount / 2;
    layout.hasCenter = layout.keyCount % 2 != 0;
    layout.centerLane = layout.hasCenter ? split : -1;
    layout.leftStart = 0;
    layout.leftEnd = split - 1;
    layout.rightStart = layout.hasCenter ? split + 1 : split;
    layout.rightEnd = layout.keyCount - 1;
    layout.hasPanels = layout.keyCount >= 4;

    // Three center lanes for odd fields and four for even fields keep the
    // original 10K 3..6 bridge while scaling the same geometry through 18K.
    if (layout.keyCount >= 7) {
        layout.hasBridge = true;
        const int bridgeWidth = layout.hasCenter ? 3 : 4;
        layout.bridgeStart = std::max(0, (layout.keyCount - bridgeWidth) / 2);
        layout.bridgeEnd = std::min(layout.keyCount - 1, layout.bridgeStart + bridgeWidth - 1);
    }
    return layout;
}

LaneSide laneSideFor(const KeyLayoutProfile& layout, int lane) {
    if (lane < 0 || lane >= layout.keyCount) {
        return LaneSide::Center;
    }
    if (layout.hasCenter && lane == layout.centerLane) {
        return LaneSide::Center;
    }
    return lane <= layout.leftEnd ? LaneSide::Left : LaneSide::Right;
}

bool laneIsInBridge(const KeyLayoutProfile& layout, int lane) {
    return layout.hasBridge && lane >= layout.bridgeStart && lane <= layout.bridgeEnd;
}

int mirroredLaneFor(const KeyLayoutProfile& layout, int lane) {
    if (lane < 0 || lane >= layout.keyCount) {
        return lane;
    }
    return layout.keyCount - 1 - lane;
}

double normalizedLanePosition(const KeyLayoutProfile& layout, int lane) {
    if (layout.keyCount <= 1) {
        return 0.5;
    }
    const int clamped = std::clamp(lane, 0, layout.keyCount - 1);
    return static_cast<double>(clamped) / static_cast<double>(layout.keyCount - 1);
}

int laneForNormalizedPosition(const KeyLayoutProfile& layout, double position) {
    if (layout.keyCount <= 1) {
        return 0;
    }
    const double clamped = std::clamp(position, 0.0, 1.0);
    return std::clamp(static_cast<int>(std::round(clamped * static_cast<double>(layout.keyCount - 1))),
                      0,
                      layout.keyCount - 1);
}

double desiredLaneShareFor(const KeyLayoutProfile& layout, int lane) {
    if (layout.keyCount <= 0 || lane < 0 || lane >= layout.keyCount) {
        return 0.0;
    }

    const double totalWeight = static_cast<double>(
        std::max(1, layout.weights.panel + layout.weights.bridge + layout.weights.fullField));
    double weightedShare =
        static_cast<double>(layout.weights.fullField + layout.weights.panel) /
        static_cast<double>(layout.keyCount);
    if (layout.hasBridge) {
        if (laneIsInBridge(layout, lane)) {
            const int bridgeWidth = std::max(1, layout.bridgeEnd - layout.bridgeStart + 1);
            weightedShare += static_cast<double>(layout.weights.bridge) /
                             static_cast<double>(bridgeWidth);
        }
    } else {
        weightedShare += static_cast<double>(layout.weights.bridge) /
                         static_cast<double>(layout.keyCount);
    }
    return weightedShare / totalWeight;
}

TargetLayoutSummary buildTargetLayoutSummary(int targetKeyCount, LayoutWeights weights) {
    TargetLayoutSummary layout;
    layout.targetKeyCount = targetKeyCount;
    layout.weights = weights;

    const auto profile = buildKeyLayoutProfile(targetKeyCount, weights);
    layout.hasPanels = profile.hasPanels;
    layout.leftPanelStart = profile.leftStart;
    layout.leftPanelEnd = profile.leftEnd;
    layout.rightPanelStart = profile.rightStart;
    layout.rightPanelEnd = profile.rightEnd;
    layout.hasBridge = profile.hasBridge;
    layout.bridgeStart = profile.bridgeStart;
    layout.bridgeEnd = profile.bridgeEnd;

    return layout;
}

std::string layoutKindName(const TargetLayoutSummary& layout) {
    if (layout.targetKeyCount == 10) {
        return "10K panel-bridge-fullfield";
    }
    if (layout.hasPanels && layout.hasBridge) {
        return "panel-bridge";
    }
    if (layout.hasPanels) {
        return "panel";
    }
    if (layout.hasBridge) {
        return "bridge";
    }
    return "fullfield";
}

}  // namespace keyconv::nk2
