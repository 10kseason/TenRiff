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
    GaugeDeltaTable hard{0.01000000, 1.0 / 20.0, 1.0 / 65.0, -4.00000, -4.00000};
    GaugeDeltaTable normal{0.01000000, 1.0 / 20.0, 1.0 / 65.0, -2.00000, -2.00000};
    GaugeDeltaTable easy{0.03200000, 0.03200000 / 20.0, 0.03200000 / 50.0, -2.00000, -2.00000};
};

struct GaugeState {
    GaugeType type = GaugeType::Normal;
    double value = 100.0;  // 0..100
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
