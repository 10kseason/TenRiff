#pragma once

namespace tenriff::game {

enum class Judgement {
    PG,
    GR,
    GD,
    BD,
    PR,
};

enum class GaugeType {
    Hard,
    Normal,
    Easy,
};

struct GaugeDeltaTable {
    double pg = 0.0;
    double gr = 0.0;
    double gd = 0.0;
    double bd = 0.0;
    double pr = 0.0;
};

struct GaugeConfig {
    bool auto_shift = true;
    double hard_to_normal_threshold = 66.0;
    double normal_to_easy_threshold = 33.0;
    GaugeDeltaTable hard{0.03666667, 0.02444444, 0.00611111, -5.50000, -7.50000};
    GaugeDeltaTable normal{0.05238095, 0.03492063, 0.00873016, -2.75000, -3.75000};
    GaugeDeltaTable easy{0.10000000, 0.06666667, 0.01666667, -2.06250, -2.81250};
};

struct GaugeState {
    GaugeType type = GaugeType::Normal;
    double value = 50.0;  // 0..100
    bool game_over = false;
};

struct GaugeResult {
    bool downshifted = false;
    bool game_over = false;
};

class GaugeManager {
public:
    explicit GaugeManager(GaugeConfig config = {});

    [[nodiscard]] const GaugeConfig& config() const noexcept { return config_; }

    [[nodiscard]] GaugeState initialState(GaugeType type) const noexcept;

    GaugeResult applyJudgement(GaugeState& state, Judgement judgement, double time_ms) const;
    GaugeResult applyJudgementWeighted(GaugeState& state, Judgement judgement, double time_ms, double weight) const;

private:
    [[nodiscard]] GaugeDeltaTable tableFor(GaugeType type) const noexcept;
    [[nodiscard]] double deltaFor(GaugeType type, Judgement judgement) const noexcept;

    GaugeConfig config_;
};

}  // namespace tenriff::game
