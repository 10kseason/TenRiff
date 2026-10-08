#include "gameplay/RiffMap.h"

#include <algorithm>
#include <cmath>

namespace tenriff::gameplay {

RiffMap build_riff_map(const GameplayChart& chart, std::size_t bins) {
    RiffMap result;
    bins = std::min(bins, result.values.size());
    if (bins == 0 || chart.duration_samples <= 0 || chart.notes.empty()) {
        return result;
    }

    // ceil(duration * bin / bins) keeps exact sample boundaries without a
    // potentially overflowing duration * bin or float rounding at the boundary.
    std::array<int64_t, kGameplayRiffMapBins> bin_starts{};
    const int64_t quotient = chart.duration_samples / static_cast<int64_t>(bins);
    const int64_t remainder = chart.duration_samples % static_cast<int64_t>(bins);
    for (std::size_t bin = 1; bin < bins; ++bin) {
        const auto index = static_cast<int64_t>(bin);
        bin_starts[bin] = quotient * index +
            (remainder * index + static_cast<int64_t>(bins) - 1) / static_cast<int64_t>(bins);
    }

    std::array<std::size_t, kGameplayRiffMapBins> counts{};
    for (const auto& note : chart.notes) {
        // The endpoint belongs to the final bin. Clamping also keeps malformed
        // negative/out-of-duration display timestamps inside the payload.
        const int64_t sample = std::clamp(note.start_sample, int64_t{0}, chart.duration_samples);
        const auto end = bin_starts.begin() + static_cast<std::ptrdiff_t>(bins);
        const auto upper = std::upper_bound(bin_starts.begin(), end, sample);
        const auto bin = static_cast<std::size_t>(upper - bin_starts.begin() - 1);
        ++counts[bin];
    }

    const auto maximum = *std::max_element(counts.begin(), counts.begin() + static_cast<std::ptrdiff_t>(bins));
    for (std::size_t bin = 0; bin < bins; ++bin) {
        result.values[bin] = static_cast<uint8_t>(std::lround(
            static_cast<double>(counts[bin]) * 255.0 / static_cast<double>(maximum)));
    }
    result.count = bins;
    return result;
}

}  // namespace tenriff::gameplay
