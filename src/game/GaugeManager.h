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
    GaugeDeltaTable hard{0.16000000, 0.09000000, 0.01000000, -10.00000, -2.00000};
    GaugeDeltaTable normal{0.19000000, 0.15000000, 0.01000000, -6.25000, -2.00000};
    GaugeDeltaTable easy{0.25000000, 0.20000000, 0.01000000, -4.10000, -1.60000};
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
