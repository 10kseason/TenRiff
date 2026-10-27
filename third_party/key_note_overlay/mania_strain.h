#pragma once
#include <vector>

namespace pattern {
inline constexpr double DefaultHoldWeight = 2.0 / 3.0;
struct TimedNote { double start; double end; int column; };
// Adapted osu!mania strain rules. With cutoff=0 this rates the supplied sequence;
// a later cutoff retains earlier strain state but only weights recent sections.
// Scale only the extra hold/release strain, including their combined term.
// Weight 1 preserves the original reference calculation; 0 removes LN extras.
double ManiaStars(const std::vector<TimedNote>& notes, int columns, double cutoff = 0.0,
    double holdWeight = DefaultHoldWeight);
}
