#include "dan_rating.h"
#include "revive10_reference.h"
#include <algorithm>
#include <cmath>

namespace pattern {
template<std::size_t N>
DanEstimate CompareDan(double strain, const std::array<DanBand, N>& curve) {
    if (!std::isfinite(strain) || strain < 0.0) return {};
    std::size_t index = 0;
    while (index + 1 < N && strain >= (curve[index].strain + curve[index + 1].strain) / 2.0) ++index;
    DanEstimate result;
    result.available = true;
    result.low = curve[index].low;
    result.high = curve[index].high;
    result.outside = strain < curve.front().strain ? -1 : strain > curve.back().strain ? 1 : 0;
    const double anchor = curve[index].strain;
    const double left = index ? (curve[index - 1].strain + anchor) / 2.0 : anchor;
    const double right = index + 1 < N ? (anchor + curve[index + 1].strain) / 2.0 : anchor;
    // Anchor = neutral "수준" even when neighboring courses have unequal gaps.
    // Heights describe position within a reference band, not clear probability.
    const double position = strain < anchor ? 0.5 * (strain - left) / std::max(1e-9, anchor - left) :
        0.5 + 0.5 * (strain - anchor) / std::max(1e-9, right - anchor);
    result.height = position < 0.25 ? DanHeight::Low : position < 0.5 ? DanHeight::MidLow :
        position < 0.75 ? DanHeight::Level : DanHeight::High;
    result.progress = std::clamp(100.0 * ((result.low + result.high) / 2.0 - curve.front().low) /
        std::max(1, curve.back().high - curve.front().low), 0.0, 100.0);
    return result;
}

DanEstimate EstimateDan(RatingProfile profile, double strain) {
    switch (profile) {
    case RatingProfile::Osu4: return CompareDan(strain, Dan4Reference);
    case RatingProfile::Osu7: return CompareDan(strain, Dan7Reference);
    case RatingProfile::Osu8: return CompareDan(strain, Dan8Reference);
    case RatingProfile::Osu10: return CompareDan(strain, Revive10Reference);
    default: return {};
    }
}

DanEstimate EstimateDan(const Snapshot& snapshot) {
    return EstimateDan(snapshot.profile, snapshot.profile == RatingProfile::Osu10 ? snapshot.rating : snapshot.stars);
}

const wchar_t* DanBasis(RatingProfile profile) {
    switch (profile) {
    case RatingProfile::Osu4: return L"4K Reform v1";
    case RatingProfile::Osu7: return L"7K Jinjin Regular";
    case RatingProfile::Osu8: return L"8K Regular v1";
    case RatingProfile::Osu10: return L"10K Revive 2026";
    default: return L"";
    }
}

std::wstring DanLabel(RatingProfile profile, int rank) {
    if (profile == RatingProfile::Osu4) {
        if (rank >= -2 && rank <= 0) return L"입문" + std::to_wstring(rank + 3);
        if (rank >= 11 && rank <= 16) return std::wstring(1, L"αβγδεζ"[rank - 11]);
    }
    if (profile == RatingProfile::Osu7 && rank >= 11 && rank <= 14)
        return std::wstring(1, L"γψζ★"[rank - 11]);
    if (profile == RatingProfile::Osu10 && rank == 15) return L"개전";
    if (profile == RatingProfile::Osu10 && rank == 16) return L"오버조이";
    if (profile == RatingProfile::Osu8 && rank == 11) return L"Flare";
    if (profile == RatingProfile::Osu8 && rank == 12) return L"Aqua";
    if (profile == RatingProfile::Osu8 && rank == 13) return L"Life";
    if (profile == RatingProfile::Osu8 && rank == 14) return L"Oblatus";
    if (profile == RatingProfile::Osu8 && rank == 15) return L"Demise";
    return std::to_wstring(rank);
}

std::wstring FormatDan(RatingProfile profile, const DanEstimate& estimate) {
    if (!estimate.available) return L"";
    std::wstring value = DanLabel(profile, estimate.low);
    if (estimate.low != estimate.high) value += L"~" + DanLabel(profile, estimate.high);
    // Named 8K ranks retain their course labels individually. A pooled range
    // uses the author's rank numbers so it stays readable on the compact HUD.
    if (profile == RatingProfile::Osu8 && estimate.low != estimate.high && estimate.high >= 11)
        value = std::to_wstring(estimate.low) + L"~" + std::to_wstring(estimate.high);
    if (profile != RatingProfile::Osu10 || estimate.high < 15) value += L"단";
    if (estimate.outside < 0) value += L"↓";
    if (estimate.outside > 0) value += L"↑";
    if (estimate.outside == 0) {
        switch (estimate.height) {
        case DanHeight::Low: value += L" low"; break;
        case DanHeight::MidLow: value += L" mid low"; break;
        case DanHeight::Level: value += L" 수준"; break;
        case DanHeight::High: value += L" high"; break;
        }
    }
    return value;
}
}
