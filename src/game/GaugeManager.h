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
    double refill_normal = 20.0;
    double refill_easy = 30.0;
    double shift_cooldown_ms = 2000.0;
    GaugeDeltaTable hard{0.24, 0.16, 0.04, -1.125, -2.25};
    GaugeDeltaTable normal{0.42, 0.28, 0.07, -0.86, -1.73};
    GaugeDeltaTable easy{0.60, 0.40, 0.10, -0.60, -1.20};
};

struct GaugeState {
    GaugeType type = GaugeType::Normal;
    double value = 50.0;  // 0..100
    double cooldown_until_ms = 0.0;
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

private:
    [[nodiscard]] GaugeDeltaTable tableFor(GaugeType type) const noexcept;

    GaugeConfig config_;
};

}  // namespace tenriff::game

