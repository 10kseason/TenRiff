#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "app/ChartLoader.h"
#include "config/Config.h"
#include "gameplay/GameplayChart.h"
#include "gameplay/ModeSettings.h"

namespace tenriff::app {

struct ModeModDescriptor {
    std::string_view token;
    std::string_view label;
    std::string_view short_label;
    std::string_view category_token;
    std::string_view category_label;
    double score_multiplier = 1.0;
    int long_note_mix_percent = 0;
    int note_add_percent = 0;
};

struct ModeModCategoryDescriptor {
    std::string_view token;
    std::string_view label;
    std::vector<const ModeModDescriptor*> mods;
};

struct ModeManagerResult {
    gameplay::ModeSettings settings;
    gameplay::GameplayChart chart;
    config::JudgeConfig judge;
    std::vector<std::string> active_mods;
    double judge_window_scale = 1.0;
    double rate_multiplier = 1.0;
    double mod_multiplier = 1.0;
    double final_multiplier = 1.0;
    std::vector<std::string> warnings;
};

[[nodiscard]] const std::vector<ModeModDescriptor>& mode_mod_registry();
[[nodiscard]] const std::vector<ModeModCategoryDescriptor>& mode_mod_categories();
[[nodiscard]] const ModeModDescriptor* find_mode_mod_descriptor(std::string_view token);
[[nodiscard]] std::vector<std::string> normalize_mode_mod_tokens(const std::vector<std::string>& tokens,
                                                                 std::vector<std::string>* warnings = nullptr);
[[nodiscard]] bool equivalent_mode_mod_tokens(const std::vector<std::string>& lhs,
                                               const std::vector<std::string>& rhs);
[[nodiscard]] bool mode_mod_adds_notes(const std::vector<std::string>& tokens);
[[nodiscard]] std::string mode_mod_summary(const std::vector<std::string>& tokens);
[[nodiscard]] std::string mode_mod_category_value(std::string_view category_token,
                                                  const std::vector<std::string>& tokens);
[[nodiscard]] double rate_score_multiplier(double rate);
[[nodiscard]] double mod_score_multiplier(const std::vector<std::string>& tokens);
[[nodiscard]] double final_score_multiplier(const std::vector<std::string>& tokens, double rate);

[[nodiscard]] ModeManagerResult manage_modes(const gameplay::GameplayChart& chart,
                                             ChartFormat chart_format,
                                             const config::ModeConfig& config,
                                             const config::JudgeConfig& judge,
                                             double rate,
                                             double base_bpm = 0.0,
                                             int sample_rate = 0);

}  // namespace tenriff::app
