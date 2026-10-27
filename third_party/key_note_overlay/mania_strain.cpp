// Adapted from ppy/osu (MIT), revision c752ffb2260abcf4dad0b7d8e298806ab9db5b24:
// ManiaDifficultyCalculator, Strain, StrainSkill, Individual/OverallStrainEvaluator.
// Copyright (c) ppy Pty Ltd <contact@ppy.sh>. See third_party/osu-LICENCE.txt.
#include "mania_strain.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace pattern {
double ManiaStars(const std::vector<TimedNote>& notes, int columns, double cutoff, double holdWeight) {
    if (notes.size() < 2 || columns < 1 || columns > 256 || !std::isfinite(cutoff) ||
        !std::isfinite(holdWeight) || holdWeight < 0.0 || holdWeight > 1.0) return 0.0;
    const auto& first = notes.front();
    if (first.column < 0 || first.column >= columns || !std::isfinite(first.start) ||
        !std::isfinite(first.end) || first.start < 0.0 || first.end < first.start) return 0.0;
    std::array<double, 256> individual{};
    std::array<int, 256> previous;
    previous.fill(-1);
    double overall = 1.0, highest = 0.0, peak = 0.0;
    double sectionEnd = std::ceil(notes[1].start / 400.0) * 400.0;
    std::vector<double> peaks;
    for (std::size_t i = 1; i < notes.size(); ++i) {
        const auto& current = notes[i];
        if (current.column < 0 || current.column >= columns || !std::isfinite(current.start) ||
            !std::isfinite(current.end) || current.start < notes[i - 1].start || current.end < current.start) return 0.0;
        const double delta = current.start - notes[i - 1].start;
        // Avoid pathological allocations for malformed or hours-long empty gaps.
        if (delta > 60000.0) {
            if (sectionEnd >= cutoff) peaks.push_back(peak);
            highest = overall = 0.0;
            peak = 0.0;
            sectionEnd = std::ceil(current.start / 400.0) * 400.0;
        }
        while (current.start > sectionEnd) {
            if (sectionEnd >= cutoff) peaks.push_back(peak);
            const double gap = sectionEnd - notes[i - 1].start;
            peak = highest * std::pow(0.125, gap / 1000.0) + overall * std::pow(0.30, gap / 1000.0);
            sectionEnd += 400.0;
        }
        double factor = 1.0, closestEnd = current.end - current.start;
        bool overlap = false;
        for (int col = 0; col < columns; ++col) {
            const int index = previous[static_cast<std::size_t>(col)];
            if (index < 0) continue;
            const auto& other = notes[static_cast<std::size_t>(index)];
            if (other.end > current.end + 1.0 && current.start > other.start + 1.0) factor = 1.25;
            if (other.end > current.start + 1.0 && current.end > other.end + 1.0 && current.start > other.start + 1.0) overlap = true;
            closestEnd = std::min(closestEnd, std::abs(current.end - other.end));
        }
        const int lastColumn = previous[static_cast<std::size_t>(current.column)];
        const double columnDelta = current.start - (lastColumn < 0 ? 0.0 : notes[static_cast<std::size_t>(lastColumn)].start);
        auto& lane = individual[static_cast<std::size_t>(current.column)];
        lane = lane * std::pow(0.125, columnDelta / 1000.0) + 2.0 + holdWeight * (2.0 * factor - 2.0);
        highest = delta <= 1.0 ? std::max(highest, lane) : lane;
        const double release = overlap ? 1.0 / (1.0 + std::exp(0.27 * (30.0 - closestEnd))) : 0.0;
        // Scale the complete extra once. Scaling factor and release separately
        // would incorrectly apply weight squared to their interaction term.
        const double holdExtra = (1.0 + release) * factor - 1.0;
        overall = overall * std::pow(0.30, delta / 1000.0) + 1.0 + holdWeight * holdExtra;
        peak = std::max(peak, highest + overall);
        previous[static_cast<std::size_t>(current.column)] = static_cast<int>(i);
    }
    if (sectionEnd >= cutoff) peaks.push_back(peak);
    std::sort(peaks.begin(), peaks.end(), std::greater<double>());
    double value = 0.0, weight = 1.0;
    for (double p : peaks) { value += p * weight; weight *= 0.9; }
    return value * 0.018;
}
}
