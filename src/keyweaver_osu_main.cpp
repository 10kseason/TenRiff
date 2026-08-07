// KeyWeaver NK2 - osu!mania key converter.
//
// Drop a folder (or individual .osu files) on the executable and every mania
// difficulty inside is relaned to the target key count by the nK2 engine.
// Launched with no arguments it stays open instead, converting each path you
// drag onto the console window.

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "nk2/nk2_report.hpp"
#include "keyweaver/osu_pipeline.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kVersion = "1.0.0";

std::string toUtf8(const fs::path& path) {
    const auto utf8 = path.u8string();
#if defined(__cpp_lib_char8_t)
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
#else
    return utf8;
#endif
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trim(const std::string& value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

bool isOsuFile(const fs::path& path) {
    // Compare natively: path::string() would push the name through the ANSI
    // code page, which throws on non-ASCII beatmap names.
    fs::path::string_type extension = path.extension().native();
    for (auto& ch : extension) {
        if (ch >= static_cast<fs::path::value_type>('A') && ch <= static_cast<fs::path::value_type>('Z')) {
            ch = static_cast<fs::path::value_type>(ch - 'A' + 'a');
        }
    }
    return extension == fs::path(".osu").native();
}

void printUsage() {
    std::cout <<
        "KeyWeaver NK2 " << kVersion << " - osu!mania key converter\n"
        "\n"
        "  드롭 대기 모드: 그냥 실행하면 창이 열린 채로 대기합니다.\n"
        "                  .osu 파일이나 폴더를 창 안으로 드래그하고 Enter.\n"
        "                  7to8 / 7to4,5,6 / allto8 로 키 경로를 바꾸고,\n"
        "                  not 4 로 특정 키 수를 제외합니다.\n"
        "                  help 로 명령어 목록, q 로 종료.\n"
        "  드래그 앤 드롭: 폴더나 .osu 를 실행 파일 아이콘 위로 끌어다 놓아도 됩니다.\n"
        "  명령줄:         keyweaver_osu.exe [옵션] <경로> [경로...]\n"
        "\n"
        "  옵션:\n"
        "    --from <N>     원본 키 수 (기본 7, 0 = 모든 키 수 허용)\n"
        "    --not <목록>   제외할 원본 키 수 (예: --not 4,5)\n"
        "    --to <N|목록>  목표 키 수 (기본 8). 쉼표 목록 가능: --to 4,5,6,8,9,10\n"
        "    --mode <이름>  native | faithful | harder | transform\n"
        "                   | remixed-remastered (=rr) | remaster (=rm)\n"
        "                   기본 remaster\n"
        "                   rm  : 보조 노트 65% + --anchor 1 --native 0.8 --ln-fill\n"
        "                         (노트는 크게 늘리고 원곡 배치는 유지)\n"
        "                   rr  : 보조 노트 65% + --ln-spread 1 --ln-fill\n"
        "                         (최대 리믹스, 원곡 배치는 거의 안 남습니다)\n"
        "                   프리셋 값은 해당 옵션을 직접 주면 덮어씁니다\n"
        "    --ssym         슈퍼 시메트리(좌우 대칭 우선) 모드\n"
        "    --suffix <S>   난이도 이름 뒤에 붙일 꼬리표 (기본 \"<목표>K\")\n"
        "    --force        이미 있는 결과 파일을 덮어씁니다\n"
        "    --dry-run      파일을 쓰지 않고 결과만 출력합니다\n"
        "    --verbose      건너뛴 파일과 nK2 경고까지 모두 출력합니다\n"
        "    --report       nK2 엔진 지표 전문을 출력합니다 (튜닝용)\n"
        "    --help         이 도움말\n"
        "\n"
        "  퀄리티 튜닝 손잡이:\n"
        "    --native <w>   원본 배치 존중 가중치\n"
        "    --remix <w>    리믹스 자유도 가중치\n"
        "    --anchor <s>   원곡 배치 보존 강도 0~1\n"
        "                   1 = 원곡 레인 배치를 최대한 유지 (전 키 수에 적용)\n"
        "    --ln-spread <s> 롱노트 위치 변형 강도 0~1\n"
        "                   0 = 롱노트는 원래 레인 근처에 고정\n"
        "                   1 = 롱노트도 탭과 같은 자유도로 재배치\n"
        "    --ln-fill      롱노트 구간의 보조 노트를 탭 대신 같은 길이의\n"
        "                   롱노트로 만들어 채웁니다 (--no-ln-fill 로 끔)\n"
        "    --layout p,b,f 패널/브릿지/전면 가중치 (기본 3,2,6)\n"
        "    --epsilon <ms> 같은 코드로 묶을 시간 오차 (기본 2)\n"
        "    --budget <r>   보조 노트 예산 비율 (예: 0.4). 모드 기본값을 덮어씁니다\n"
        "    --density-ref <nps>     이 밀도 미만이면 예산을 비례해서 줄입니다\n"
        "                            (기본 10). 쉬운 채보가 과하게 채워지는 걸 막습니다.\n"
        "                            음수를 주면 비활성화\n"
        "    --jack-window <ms>      보조 노트 잭 안전창 (rm/rr 기본 200)\n"
        "                            줄이면 보조 노트가 늘고, 늘리면 줄어듭니다\n"
        "    --same-source-gap <ms>  같은 원본 레인 최소 간격 (rm/rr 기본 70)\n"
        "    --gap-boost <s> 원본이 매핑되지 않는 빈 레인 가산점 (기본 2)\n"
        "                   4K->5K 처럼 새 레인이 비는 변환에서 그 레인을 채웁니다\n"
        "    --thin <r>     축소 변환 시 코드를 솎아내는 강도 0~1 (기본 1)\n"
        "                   1 = 키 비율만큼 (7K->4K 면 4/7). 0 = 솎지 않음\n"
        "    --min-gap <ms> 같은 레인 최소 간격 (기본 30). 이보다 가까운 노트는\n"
        "                   정리합니다. 키 수를 줄일 때 생기는 겹노트 방지용\n"
        "\n"
        "  결과물은 원본과 같은 폴더에 \"... [원래난이도 8K].osu\" 로 새로 만들어집니다.\n"
        "  원본 파일은 절대 수정하지 않습니다.\n";
}

std::vector<std::string> splitOnCommas(const std::string& value) {
    std::vector<std::string> parts;
    std::size_t cursor = 0;
    while (true) {
        const std::size_t comma = value.find(',', cursor);
        if (comma == std::string::npos) {
            parts.push_back(value.substr(cursor));
            return parts;
        }
        parts.push_back(value.substr(cursor, comma - cursor));
        cursor = comma + 1;
    }
}

bool parseNonNegative(const std::string& value, int& out) {
    if (value.empty()) {
        return false;
    }
    int parsed = 0;
    for (const char ch : value) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        parsed = parsed * 10 + (ch - '0');
    }
    out = parsed;
    return true;
}

bool parseMode(const std::string& value, keyconv::nk2::Mode& mode) {
    try {
        mode = keyconv::nk2::parseModeOrThrow(lower(value));
        return true;
    } catch (...) {
        return false;
    }
}

void collectOsuFiles(const fs::path& input, std::vector<fs::path>& out) {
    // Descend through the extended-length form: beatmap folders nest deep
    // enough that plain paths stop resolving partway down.
    const fs::path root = keyweaver::extendedPath(input);
    std::error_code error;
    if (fs::is_regular_file(root, error)) {
        if (isOsuFile(root)) {
            out.push_back(root);
        }
        return;
    }
    if (!fs::is_directory(root, error)) {
        std::cout << "  [!] 경로를 찾을 수 없습니다: " << toUtf8(input) << "\n";
        return;
    }

    fs::recursive_directory_iterator iterator(root, fs::directory_options::skip_permission_denied, error);
    if (error) {
        std::cout << "  [!] 폴더를 열 수 없습니다: " << toUtf8(input) << "\n";
        return;
    }
    for (auto entry = iterator; entry != fs::recursive_directory_iterator(); entry.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        if (entry->is_regular_file(error) && isOsuFile(entry->path())) {
            out.push_back(entry->path());
        }
    }
}

// Windows quotes any dragged path that contains a space, and separates several
// of them with spaces. Fall back to treating the whole line as one path so an
// unquoted name with spaces still works.
std::vector<fs::path> parseDroppedLine(const std::string& line) {
    std::vector<fs::path> out;
    const std::string whole = trim(line);
    if (whole.empty()) {
        return out;
    }
    if (whole.front() != '"') {
        std::error_code error;
        const fs::path direct = keyweaver::extendedPath(fs::u8path(whole));
        if (fs::exists(direct, error)) {
            out.push_back(fs::u8path(whole));
            return out;
        }
    }

    std::size_t cursor = 0;
    while (cursor < whole.size()) {
        while (cursor < whole.size() && std::isspace(static_cast<unsigned char>(whole[cursor]))) {
            ++cursor;
        }
        if (cursor >= whole.size()) {
            break;
        }
        std::string token;
        if (whole[cursor] == '"') {
            ++cursor;
            while (cursor < whole.size() && whole[cursor] != '"') {
                token.push_back(whole[cursor++]);
            }
            if (cursor < whole.size()) {
                ++cursor;
            }
        } else {
            while (cursor < whole.size() && !std::isspace(static_cast<unsigned char>(whole[cursor]))) {
                token.push_back(whole[cursor++]);
            }
        }
        if (!token.empty()) {
            out.push_back(fs::u8path(token));
        }
    }
    return out;
}

// "7to8" or "7to4,5,6" retargets the conversion. Matched strictly - a dropped
// path can easily contain "to", and must not be mistaken for a command.
bool parseKeyRoute(const std::string& text, int& from, std::vector<int>& to, bool& looksLikeRoute) {
    looksLikeRoute = false;
    const std::string lowered = lower(text);
    const std::size_t at = lowered.find("to");
    if (at == std::string::npos || at == 0 || at + 2 >= lowered.size()) {
        return false;
    }
    if (lowered.find("to", at + 2) != std::string::npos) {
        return false;
    }
    const std::string head = lowered.substr(0, at);
    int source = 0;
    // "all" reads better than "0" for "any source key count".
    if (head != "all" && !parseNonNegative(head, source)) {
        return false;
    }
    std::vector<int> targets;
    for (const auto& part : splitOnCommas(lowered.substr(at + 2))) {
        int key = 0;
        if (!parseNonNegative(part, key)) {
            return false;
        }
        targets.push_back(key);
    }
    if (targets.empty()) {
        return false;
    }
    // Shape is right, so report a range problem rather than letting the line
    // fall through and be reported as a missing path.
    looksLikeRoute = true;
    if (source > keyconv::nk2::kMaxSupportedKeyCount) {
        return false;
    }
    for (const int key : targets) {
        if (key <= 0 || key > keyconv::nk2::kMaxSupportedKeyCount) {
            return false;
        }
    }
    from = source;
    to = targets;
    return true;
}

struct RunStats {
    int converted = 0;
    int failed = 0;
    int skippedNotMania = 0;
    int skippedKeys = 0;
    int skippedExisting = 0;
    int totalAdded = 0;
    int totalDropped = 0;
    int totalRepaired = 0;
};

// "not 4" / "not 4,5" excludes source key counts; bare "not" clears the list.
bool parseExclusion(const std::string& text, std::vector<int>& excluded, bool& valid) {
    valid = false;
    const std::string lowered = lower(text);
    if (lowered != "not" && lowered.rfind("not ", 0) != 0) {
        return false;
    }
    if (lowered == "not") {
        excluded.clear();
        valid = true;
        return true;
    }
    std::vector<int> keys;
    for (const auto& part : splitOnCommas(trim(lowered.substr(4)))) {
        int key = 0;
        if (!parseNonNegative(trim(part), key) || key <= 0 ||
            key > keyconv::nk2::kMaxSupportedKeyCount) {
            return true;  // recognised as a "not" command, but the values are bad
        }
        keys.push_back(key);
    }
    if (keys.empty()) {
        return true;
    }
    excluded = std::move(keys);
    valid = true;
    return true;
}

void printInteractiveHelp() {
    std::cout <<
        "\n"
        "  경로          .osu 파일이나 폴더를 창 안으로 드래그하고 Enter\n"
        "                여러 개를 한 번에 끌어다 놓아도 됩니다\n"
        "\n"
        "  7to8          7키 채보만 8키로 변환\n"
        "  7to4,5,6      7키 채보를 4·5·6키로 한 번에\n"
        "  allto8        키 수 관계없이 전부 8키로 (0to8 과 동일)\n"
        "  not 4         4키 채보는 건너뜀 (not 4,5 처럼 여러 개 가능)\n"
        "  not           제외 목록 해제\n"
        "\n"
        "  help          이 도움말\n"
        "  q             종료\n";
}

void printSettings(const keyweaver::ConvertOptions& options, const std::vector<int>& targetKeyCounts) {
    const std::string fromLabel =
        options.sourceKeyCount > 0 ? std::to_string(options.sourceKeyCount) + "K" : "모든 키";
    std::string toLabel;
    for (const int key : targetKeyCounts) {
        toLabel += (toLabel.empty() ? "" : ",") + std::to_string(key);
    }
    toLabel += "K";
    std::cout << "KeyWeaver NK2 " << kVersion << "  |  " << fromLabel << " -> " << toLabel
              << "  |  mode=" << keyconv::nk2::toString(options.mode)
              << (options.superSymmetry ? " +sSym" : "") << (options.dryRun ? "  (dry-run)" : "") << "\n";
    std::cout << "native/remix=" << options.nativeWeight << "/" << options.remixWeight
              << "  layout(p,b,f)=" << options.layoutWeights.panel << ","
              << options.layoutWeights.bridge << "," << options.layoutWeights.fullField
              << "  epsilon=" << options.sameTimeEpsilonMs << "ms"
              << "  ln-spread=" << options.lnSpread << (options.lnFill ? "  +ln-fill" : "")
              << "  anchor=" << options.anchorBias << "\n";
    if (!options.excludedSourceKeyCounts.empty()) {
        std::cout << "제외: ";
        for (std::size_t index = 0; index < options.excludedSourceKeyCounts.size(); ++index) {
            std::cout << (index == 0 ? "" : ", ") << options.excludedSourceKeyCounts[index] << "K";
        }
        std::cout << "\n";
    }
}

void runBatch(const std::vector<fs::path>& files,
              keyweaver::ConvertOptions options,
              const std::vector<int>& targetKeyCounts,
              RunStats& stats) {
    const std::string baseSuffix = options.versionSuffix;
    for (const int key : targetKeyCounts) {
        options.targetKeyCount = key;
        // A custom suffix would name every target the same, so key-tag it.
        options.versionSuffix = baseSuffix.empty() || targetKeyCounts.size() == 1
                                    ? baseSuffix
                                    : std::to_string(key) + "K " + baseSuffix;
        if (targetKeyCounts.size() > 1) {
            std::cout << "===== " << key << "K =====\n";
        }

        for (const auto& file : files) {
            // One malformed beatmap must not abort a whole Songs folder.
            keyweaver::ConvertOutcome outcome;
            try {
                outcome = keyweaver::convertOsuFile(file, options);
            } catch (const std::exception& error) {
                outcome.status = keyweaver::ConvertStatus::Failed;
                outcome.detail = std::string("예외: ") + error.what();
            } catch (...) {
                outcome.status = keyweaver::ConvertStatus::Failed;
                outcome.detail = "알 수 없는 예외";
            }
            const std::string name = toUtf8(file.filename());

            switch (outcome.status) {
                case keyweaver::ConvertStatus::Converted:
                    ++stats.converted;
                    stats.totalAdded += outcome.addedNotes;
                    stats.totalDropped += outcome.droppedNotes;
                    stats.totalRepaired += outcome.repairedNotes;
                    std::cout << "  [OK] " << name << "\n"
                              << "       -> " << toUtf8(outcome.outputPath.filename()) << "  ("
                              << outcome.sourceNotes << " -> " << outcome.outputNotes << " notes, +"
                              << outcome.addedNotes << " / -" << outcome.droppedNotes;
                    if (outcome.thinnedNotes > 0) {
                        std::cout << " / 솎음 " << outcome.thinnedNotes;
                    }
                    if (outcome.repairedNotes > 0) {
                        std::cout << " / 정리 " << outcome.repairedNotes;
                    }
                    std::cout << ")\n";
                    if (!outcome.laneDistribution.empty()) {
                        std::cout << "       레인 분포:";
                        const int total = std::max(1, outcome.outputNotes);
                        for (const int count : outcome.laneDistribution) {
                            std::cout << "  " << count << " (" << (count * 1000 / total) / 10.0 << "%)";
                        }
                        std::cout << "\n";
                    }
                    if (options.showReport) {
                        std::cout << "\n" << keyconv::nk2::reportToText(outcome.report) << "\n";
                    }
                    if (options.verbose) {
                        if (!outcome.droppedNoteTimes.empty()) {
                            std::cout << "       배치 실패한 원본 노트 시각(ms):";
                            const std::size_t shown =
                                std::min<std::size_t>(outcome.droppedNoteTimes.size(), 12);
                            for (std::size_t index = 0; index < shown; ++index) {
                                std::cout << " " << outcome.droppedNoteTimes[index];
                            }
                            if (outcome.droppedNoteTimes.size() > shown) {
                                std::cout << " ... (+" << outcome.droppedNoteTimes.size() - shown << ")";
                            }
                            std::cout << "\n";
                        }
                        for (const auto& warning : outcome.warnings) {
                            std::cout << "       nK2: " << warning << "\n";
                        }
                    }
                    break;
                case keyweaver::ConvertStatus::Failed:
                    ++stats.failed;
                    std::cout << "  [실패] " << name << " - " << outcome.detail << "\n";
                    break;
                case keyweaver::ConvertStatus::SkippedNotMania:
                    ++stats.skippedNotMania;
                    if (options.verbose) {
                        std::cout << "  [건너뜀] " << name << " - " << outcome.detail << "\n";
                    }
                    break;
                case keyweaver::ConvertStatus::SkippedOutputExists:
                    ++stats.skippedExisting;
                    if (options.verbose) {
                        std::cout << "  [건너뜀] " << name
                                  << " - 결과 파일이 이미 있습니다 (--force 로 덮어쓰기)\n";
                    }
                    break;
                case keyweaver::ConvertStatus::SkippedAlreadyConverted:
                case keyweaver::ConvertStatus::SkippedKeyCount:
                    ++stats.skippedKeys;
                    if (options.verbose) {
                        std::cout << "  [건너뜀] " << name << " - " << outcome.detail << "\n";
                    }
                    break;
            }
        }
    }
}

void printSummary(const RunStats& stats, const keyweaver::ConvertOptions& options) {
    std::cout << "\n----------------------------------------\n";
    std::cout << "변환 " << stats.converted << "개";
    if (stats.converted > 0) {
        std::cout << "  (보조 노트 +" << stats.totalAdded << ", 버려진 노트 " << stats.totalDropped;
        if (stats.totalRepaired > 0) {
            std::cout << ", 충돌 정리 " << stats.totalRepaired;
        }
        std::cout << ")";
    }
    std::cout << "\n";
    std::cout << "건너뜀 " << (stats.skippedKeys + stats.skippedNotMania + stats.skippedExisting)
              << "개  (키 수/변환 완료 " << stats.skippedKeys << ", mania 아님 " << stats.skippedNotMania
              << ", 결과 존재 " << stats.skippedExisting << ")\n";
    if (stats.failed > 0) {
        std::cout << "실패 " << stats.failed << "개\n";
    }
    if (options.dryRun) {
        std::cout << "\n※ dry-run 이라 실제로 저장된 파일은 없습니다.\n";
    }
}

// Stays open so a whole session's worth of beatmaps can be dropped one after
// another without relaunching.
int runInteractive(keyweaver::ConvertOptions options, std::vector<int> targetKeyCounts) {
    printSettings(options, targetKeyCounts);
    printInteractiveHelp();

    std::string line;
    while (true) {
        std::cout << "\n> " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }
        const std::string command = lower(trim(line));
        if (command == "q" || command == "quit" || command == "exit") {
            break;
        }
        if (command.empty()) {
            continue;
        }
        if (command == "help" || command == "?" || command == "h") {
            printSettings(options, targetKeyCounts);
            printInteractiveHelp();
            continue;
        }

        bool exclusionValid = false;
        std::vector<int> excluded = options.excludedSourceKeyCounts;
        if (parseExclusion(command, excluded, exclusionValid)) {
            if (!exclusionValid) {
                std::cout << "  제외할 키 수는 1.." << keyconv::nk2::kMaxSupportedKeyCount
                          << " 범위여야 합니다. 예: not 4  /  not 4,5  /  not\n";
                continue;
            }
            options.excludedSourceKeyCounts = std::move(excluded);
            printSettings(options, targetKeyCounts);
            continue;
        }

        int routeFrom = 0;
        std::vector<int> routeTo;
        bool looksLikeRoute = false;
        if (parseKeyRoute(command, routeFrom, routeTo, looksLikeRoute)) {
            options.sourceKeyCount = routeFrom;
            targetKeyCounts = std::move(routeTo);
            printSettings(options, targetKeyCounts);
            continue;
        }
        if (looksLikeRoute) {
            std::cout << "  키 수는 1.." << keyconv::nk2::kMaxSupportedKeyCount
                      << " 범위여야 합니다 (원본은 0 = 모든 키). 예: 7to8\n";
            continue;
        }

        std::vector<fs::path> files;
        for (const auto& input : parseDroppedLine(line)) {
            collectOsuFiles(input, files);
        }
        std::sort(files.begin(), files.end());
        files.erase(std::unique(files.begin(), files.end()), files.end());
        if (files.empty()) {
            std::cout << "  .osu 파일을 찾지 못했습니다.\n";
            continue;
        }

        std::cout << ".osu 파일 " << files.size() << "개\n";
        RunStats stats;
        runBatch(files, options, targetKeyCounts, stats);
        printSummary(stats, options);
    }
    return 0;
}

#ifdef _WIN32
bool ownsConsoleWindow() {
    DWORD pids[4] = {};
    return GetConsoleProcessList(pids, 4) <= 1;
}
#endif

}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#else
int main(int argc, char** argv) {
#endif
    std::ios::sync_with_stdio(false);

    keyweaver::ConvertOptions options;
    std::vector<fs::path> inputs;
    bool showHelp = false;
    // A preset fills these in, but only where the caller stayed silent.
    bool lnSpreadGiven = false;
    bool lnFillGiven = false;
    bool anchorGiven = false;
    bool weightsGiven = false;
    std::vector<int> targetKeyCounts;

    for (int index = 1; index < argc; ++index) {
        const fs::path argPath(argv[index]);
        const std::string arg = toUtf8(argPath);
        const bool hasNext = index + 1 < argc;

        if (arg == "--help" || arg == "-h" || arg == "/?") {
            showHelp = true;
        } else if (arg == "--from" && hasNext) {
            options.sourceKeyCount = std::atoi(toUtf8(fs::path(argv[++index])).c_str());
        } else if (arg == "--to" && hasNext) {
            targetKeyCounts.clear();
            for (const auto& part : splitOnCommas(toUtf8(fs::path(argv[++index])))) {
                int key = 0;
                if (!parseNonNegative(trim(part), key)) {
                    std::cout << "--to 는 숫자 또는 쉼표로 구분한 목록이어야 합니다 (예: 4,5,8,10)\n";
                    return 2;
                }
                targetKeyCounts.push_back(key);
            }
        } else if ((arg == "--not" || arg == "--exclude") && hasNext) {
            options.excludedSourceKeyCounts.clear();
            for (const auto& part : splitOnCommas(toUtf8(fs::path(argv[++index])))) {
                int key = 0;
                if (!parseNonNegative(trim(part), key) || key <= 0 ||
                    key > keyconv::nk2::kMaxSupportedKeyCount) {
                    std::cout << "--not 은 1.." << keyconv::nk2::kMaxSupportedKeyCount
                              << " 범위의 키 수 목록이어야 합니다 (예: 4,5)\n";
                    return 2;
                }
                options.excludedSourceKeyCounts.push_back(key);
            }
        } else if (arg == "--mode" && hasNext) {
            const std::string value = toUtf8(fs::path(argv[++index]));
            if (!parseMode(value, options.mode) || options.mode == keyconv::nk2::Mode::Report) {
                std::cout << "모드는 native / faithful / harder / transform / rr / rm 중 하나여야 합니다: "
                          << value << "\n";
                return 2;
            }
        } else if (arg == "--suffix" && hasNext) {
            options.versionSuffix = toUtf8(fs::path(argv[++index]));
        } else if (arg == "--native" && hasNext) {
            options.nativeWeight = std::atof(toUtf8(fs::path(argv[++index])).c_str());
            weightsGiven = true;
        } else if (arg == "--remix" && hasNext) {
            options.remixWeight = std::atof(toUtf8(fs::path(argv[++index])).c_str());
            weightsGiven = true;
        } else if (arg == "--ln-spread" && hasNext) {
            options.lnSpread = std::atof(toUtf8(fs::path(argv[++index])).c_str());
            lnSpreadGiven = true;
        } else if (arg == "--anchor" && hasNext) {
            options.anchorBias = std::atof(toUtf8(fs::path(argv[++index])).c_str());
            anchorGiven = true;
        } else if (arg == "--ln-fill") {
            options.lnFill = true;
            lnFillGiven = true;
        } else if (arg == "--no-ln-fill") {
            options.lnFill = false;
            lnFillGiven = true;
        } else if (arg == "--budget" && hasNext) {
            options.supportBudgetRatio = std::atof(toUtf8(fs::path(argv[++index])).c_str());
                } else if (arg == "--density-ref" && hasNext) {
            options.supportDensityReferenceNps = std::atof(toUtf8(fs::path(argv[++index])).c_str());
        } else if (arg == "--jack-window" && hasNext) {
            options.supportJackWindowMs = std::atoi(toUtf8(fs::path(argv[++index])).c_str());
        } else if (arg == "--same-source-gap" && hasNext) {
            options.supportSameSourceGapMs = std::atoi(toUtf8(fs::path(argv[++index])).c_str());
        } else if (arg == "--thin" && hasNext) {
            options.downscaleThin = std::atof(toUtf8(fs::path(argv[++index])).c_str());
        } else if (arg == "--gap-boost" && hasNext) {
            options.gapLaneBoost = std::atof(toUtf8(fs::path(argv[++index])).c_str());
        } else if (arg == "--min-gap" && hasNext) {
            options.minSameLaneGapMs = std::atoi(toUtf8(fs::path(argv[++index])).c_str());
        } else if (arg == "--epsilon" && hasNext) {
            options.sameTimeEpsilonMs = std::atoi(toUtf8(fs::path(argv[++index])).c_str());
        } else if (arg == "--layout" && hasNext) {
            const std::string value = toUtf8(fs::path(argv[++index]));
            const auto parts = splitOnCommas(value);
            int weights[3] = {-1, -1, -1};
            bool valid = parts.size() == 3;
            for (std::size_t part = 0; valid && part < 3; ++part) {
                valid = parseNonNegative(trim(parts[part]), weights[part]);
            }
            if (!valid || weights[0] + weights[1] + weights[2] <= 0) {
                std::cout << "--layout 은 panel,bridge,fullField 형식이어야 합니다 (예: 3,2,6): " << value
                          << "\n";
                return 2;
            }
            options.layoutWeights = {weights[0], weights[1], weights[2]};
        } else if (arg == "--report") {
            options.showReport = true;
        } else if (arg == "--ssym") {
            options.superSymmetry = true;
        } else if (arg == "--force") {
            options.overwrite = true;
        } else if (arg == "--dry-run") {
            options.dryRun = true;
        } else if (arg == "--verbose" || arg == "-v") {
            options.verbose = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cout << "알 수 없는 옵션입니다: " << arg << "\n";
            return 2;
        } else {
            inputs.push_back(argPath);
        }
    }

    if (showHelp) {
        printUsage();
#ifdef _WIN32
        if (ownsConsoleWindow()) {
            std::cout << "\n계속하려면 Enter 키를 누르세요..." << std::flush;
            std::cin.get();
        }
#endif
        return 0;
    }

    if (options.mode == keyconv::nk2::Mode::RemixedRemastered) {
        if (!lnSpreadGiven) {
            options.lnSpread = 1.0;
        }
        if (!lnFillGiven) {
            options.lnFill = true;
        }
    } else if (options.mode == keyconv::nk2::Mode::Remaster) {
        // Same 65% budget as remixed-remastered, but every knob points at
        // keeping the source placement readable.
        if (!lnFillGiven) {
            options.lnFill = true;
        }
        if (!anchorGiven) {
            options.anchorBias = 1.0;
        }
        if (!weightsGiven) {
            options.nativeWeight = 0.8;
            options.remixWeight = 0.2;
        }
    }

    if (targetKeyCounts.empty()) {
        targetKeyCounts.push_back(options.targetKeyCount);
    }
    for (const int key : targetKeyCounts) {
        if (key <= 0 || key > keyconv::nk2::kMaxSupportedKeyCount) {
            std::cout << "목표 키 수는 1..." << keyconv::nk2::kMaxSupportedKeyCount
                      << " 범위여야 합니다: " << key << "\n";
            return 2;
        }
    }
    if (options.sourceKeyCount < 0 || options.sourceKeyCount > keyconv::nk2::kMaxSupportedKeyCount) {
        std::cout << "원본 키 수는 0..." << keyconv::nk2::kMaxSupportedKeyCount << " 범위여야 합니다.\n";
        return 2;
    }

    if (inputs.empty()) {
        return runInteractive(options, targetKeyCounts);
    }

    std::vector<fs::path> files;
    for (const auto& input : inputs) {
        collectOsuFiles(input, files);
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());

    printSettings(options, targetKeyCounts);
    std::cout << ".osu 파일 " << files.size() << "개를 찾았습니다.\n\n";

    RunStats stats;
    runBatch(files, options, targetKeyCounts, stats);
    printSummary(stats, options);

#ifdef _WIN32
    if (ownsConsoleWindow()) {
        std::cout << "\n계속하려면 Enter 키를 누르세요..." << std::flush;
        std::cin.get();
    }
#endif
    return stats.failed > 0 ? 1 : 0;
}
