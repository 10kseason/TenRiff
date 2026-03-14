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
    GaugeDeltaTable hard{0.13752, 0.09144, 0.02304, -1.94425, -3.88850};
    GaugeDeltaTable normal{0.23123, 0.15438, 0.03877, -1.54583, -3.11025};
    GaugeDeltaTable easy{0.30664, 0.20443, 0.05143, -1.16116, -2.32232};
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
