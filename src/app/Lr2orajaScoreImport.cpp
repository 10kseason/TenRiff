#include "app/Lr2orajaScoreImport.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

#include "config/SimpleJson.h"

namespace tenriff::app {

std::string_view lr2oraja_clear_label(int clear) noexcept {
    // LR2oraja ClearType IDs include the assist and perfect variants.
    static constexpr std::array<std::string_view, 11> labels{
        "NO PLAY", "FAILED", "ASSIST EASY", "LIGHT ASSIST EASY", "EASY CLEAR", "CLEAR",
        "HARD CLEAR", "EX-HARD CLEAR", "FULL COMBO", "PERFECT", "MAX"};
    return clear >= 0 && static_cast<std::size_t>(clear) < labels.size() ? labels[clear] : labels[0];
}

std::string serialize_lr2oraja_scores(const Lr2orajaImportedScores& scores) {
    using namespace config;
    std::vector<const Lr2orajaImportedScores::value_type*> ordered;
    ordered.reserve(scores.size());
    for (const auto& entry : scores) ordered.push_back(&entry);
    std::sort(ordered.begin(), ordered.end(),
              [](const auto* lhs, const auto* rhs) { return lhs->first < rhs->first; });

    JsonArray entries;
    const auto judges = [](const std::array<int, 6>& values) {
        JsonArray counts;
        counts.reserve(values.size());
        for (int value : values) counts.emplace_back(static_cast<double>(value));
        return counts;
    };
    for (const auto* entry : ordered) {
        for (std::size_t mode = 0; mode < entry->second.size(); ++mode) {
            const auto& score = entry->second[mode];
            if (!score) continue;
            entries.emplace_back(JsonObject{
                {"sha256", JsonValue(entry->first)},
                {"ln_mode", JsonValue(static_cast<double>(mode))},
                {"clear", JsonValue(static_cast<double>(score->clear))},
                {"notes", JsonValue(static_cast<double>(score->notes))},
                {"maxcombo", JsonValue(static_cast<double>(score->maxcombo))},
                {"minbp", JsonValue(static_cast<double>(score->minbp))},
                {"early", JsonValue(judges(score->early))},
                {"late", JsonValue(judges(score->late))}
            });
        }
    }
    return json_stringify(JsonValue(JsonObject{
        {"format", JsonValue(1.0)},
        {"source", JsonValue("lr2oraja_score_db")},
        {"entries", JsonValue(std::move(entries))}
    }), 0, std::numeric_limits<double>::max_digits10);
}

Lr2orajaImportedScores parse_lr2oraja_scores(std::string_view json) {
    using namespace config;
    const auto parsed = parse_json(json);
    const auto* root = parsed.success() ? parsed.root->as_object() : nullptr;
    if (!root) return {};
    const auto format = root->find("format");
    const auto source = root->find("source");
    const auto entries = root->find("entries");
    if (format == root->end() || format->second.as_number() != 1.0 ||
        source == root->end() || source->second.as_string() != "lr2oraja_score_db" ||
        entries == root->end() || !entries->second.as_array()) return {};

    const auto integer = [](const JsonValue& value) {
        const double number = value.as_number(-1.0);
        return number >= 0.0 && number <= (std::numeric_limits<int>::max)() && std::floor(number) == number
            ? static_cast<int>(number) : -1;
    };
    Lr2orajaImportedScores scores;
    for (const auto& entry : *entries->second.as_array()) {
        const auto* object = entry.as_object();
        if (!object) continue;
        const auto get = [&](const char* field) -> const JsonValue& {
            const auto found = object->find(field);
            static const JsonValue missing;
            return found == object->end() ? missing : found->second;
        };
        auto hash = get("sha256").as_string();
        const int mode = integer(get("ln_mode"));
        const auto* early = get("early").as_array();
        const auto* late = get("late").as_array();
        Lr2orajaScore score;
        if (!is_lr2oraja_chart_hash(hash) || mode < 0 || mode >= kLr2orajaLnModeCount ||
            !early || !late || early->size() != score.early.size() || late->size() != score.late.size()) continue;
        score.clear = integer(get("clear"));
        score.notes = integer(get("notes"));
        score.maxcombo = integer(get("maxcombo"));
        score.minbp = integer(get("minbp"));
        std::transform(early->begin(), early->end(), score.early.begin(), integer);
        std::transform(late->begin(), late->end(), score.late.begin(), integer);
        if (score.valid()) scores[std::move(hash)][mode] = score;
    }
    return scores;
}

Lr2orajaImportedScores load_lr2oraja_scores(const std::filesystem::path& profile) {
    std::ifstream stream(profile / kLr2orajaImportFilename, std::ios::binary);
    if (!stream) return {};
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return parse_lr2oraja_scores(buffer.str());
}

}  // namespace tenriff::app
