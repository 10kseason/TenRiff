#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "nk2/chart.hpp"

#include "nk2/nk2_convert.hpp"
#include "keyweaver/osu_file.hpp"

namespace keyweaver {

// Written into [Metadata] Tags so a re-drop of the same folder skips the
// difficulties this tool already produced.
inline constexpr const char* kConvertedTag = "keyweaver-nk2";

struct ConvertOptions {
    int sourceKeyCount = 7;  // 0 accepts any mania key count
    std::vector<int> excludedSourceKeyCounts;  // source key counts to skip
    int targetKeyCount = 8;
    keyconv::nk2::Mode mode = keyconv::nk2::Mode::Remaster;
    bool superSymmetry = false;
    double nativeWeight = 0.5;
    double remixWeight = 0.5;
    keyconv::nk2::LayoutWeights layoutWeights;  // panel / bridge / fullField
    int sameTimeEpsilonMs = 2;
    double supportBudgetRatio = 0.0;  // 0 = 모드 기본 예산 사용
    int supportJackWindowMs = 0;      // 0 = 모드 기본 안전창 사용
    int supportSameSourceGapMs = 0;  // 0 = 모드 기본값 사용
    double supportDensityReferenceNps = 0.0;  // 0 = 엔진 기본(10nps), 음수 = 비활성
    double lnSpread = 0.0;  // 0 = holds pinned near source lane, 1 = free as taps
    double anchorBias = 0.0;   // 8K 전용 확산 튜닝을 되돌리는 정도 0~1
    double gapLaneBoost = 2.0;     // 원본이 매핑되지 않는 빈 레인 가산점
    int minSameLaneGapMs = 30;  // 같은 레인 최소 간격 (이보다 가까우면 정리)
    // 축소 변환 시 코드를 키 비율만큼 솎아내는 강도 0~1 (1 = 키 비율 그대로)
    double downscaleThin = 1.0;
    bool lnFill = false;      // LN 구간 보조 노트를 롱노트로 채움
    std::string versionSuffix;  // empty -> "<target>K"
    bool overwrite = false;
    bool dryRun = false;
    bool verbose = false;
    bool showReport = false;
};

enum class ConvertStatus {
    Converted,
    SkippedNotMania,
    SkippedKeyCount,
    SkippedAlreadyConverted,
    SkippedOutputExists,
    Failed,
};

struct ConvertOutcome {
    ConvertStatus status = ConvertStatus::Failed;
    std::string detail;
    std::filesystem::path outputPath;
    int sourceKeyCount = 0;
    int sourceNotes = 0;
    int outputNotes = 0;
    int addedNotes = 0;
    int droppedNotes = 0;
    int repairedNotes = 0;
    int thinnedNotes = 0;   // 축소 전처리에서 솎아낸 노트
    std::vector<int> droppedNoteTimes;  // source timestamps nK2 could not place
    std::vector<std::string> warnings;
    keyconv::nk2::NK2Report report;  // full engine metrics, for tuning
    std::vector<int> laneDistribution;
};

// Windows: the \\?\ extended-length form of an absolute path, so files and
// directories past MAX_PATH stay openable. Identity on other platforms.
std::filesystem::path extendedPath(const std::filesystem::path& path);

keyconv::Chart chartFromOsu(const osu::File& file, int keyCount);

// Rebuilds `base` as a target-key difficulty carrying the converted notes.
// `repaired` receives the number of notes dropped or shortened by the final
// legality pass.
osu::File osuFromChart(osu::File base,
                       const keyconv::Chart& chart,
                       const std::vector<osu::HitObject>& sourceObjects,
                       int targetKeyCount,
                       const std::string& newVersion,
                       int minSameLaneGapMs,
                       int& repaired);

std::string convertedVersionName(const std::string& version, const ConvertOptions& options);
std::filesystem::path outputPathFor(const std::filesystem::path& input, const std::string& newVersion);

ConvertOutcome convertOsuFile(const std::filesystem::path& input, const ConvertOptions& options);

}  // namespace keyweaver
