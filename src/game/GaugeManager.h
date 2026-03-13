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
    double refill_normal = 15.911;
    double refill_easy = 23.866;
    double shift_cooldown_ms = 2000.0;
    GaugeDeltaTable hard{0.191, 0.127, 0.032, -1.414, -2.828};
    GaugeDeltaTable normal{0.334, 0.223, 0.056, -1.081, -2.175};
    GaugeDeltaTable easy{0.477, 0.318, 0.080, -0.754, -1.508};
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
    GaugeResult applyJudgementWeighted(GaugeState& state, Judgement judgement, double time_ms, double weight) const;

private:
    [[nodiscard]] GaugeDeltaTable tableFor(GaugeType type) const noexcept;
    [[nodiscard]] double deltaFor(GaugeType type, Judgement judgement) const noexcept;

    GaugeConfig config_;
};

}  // namespace tenriff::game
