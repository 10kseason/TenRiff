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
    ExHard,
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
    GaugeDeltaTable ex_hard{0.08000000, 0.04000000, 0.00000000, -18.00000, -4.00000};
    GaugeDeltaTable hard{0.16000000, 0.09000000, 0.01000000, -10.00000, -2.00000};
    GaugeDeltaTable normal{0.19000000, 0.15000000, 0.01000000, -6.25000, -2.00000};
    GaugeDeltaTable easy{0.25000000, 0.20000000, 0.01000000, -4.10000, -1.60000};
};

// Session-only gauge behavior. Keep this separate from GaugeConfig so battle
// rules can opt in without changing persisted single-player gauge tuning.
struct GaugeRuntimePolicy {
    // Dan / Session Mix gauge: keep the Normal gauge identity while using
    // recovery halfway between Normal and Hard plus Easy damage deltas.
    bool course_hybrid_deltas = false;
    bool hard_to_normal_shift = false;
    double hard_to_normal_threshold = 66.0;
    bool normal_to_easy_shift = false;
    double normal_to_easy_threshold = 33.0;
    bool refill_on_shift = true;
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
    explicit GaugeManager(GaugeConfig config = {}, GaugeRuntimePolicy policy = {});

    [[nodiscard]] const GaugeConfig& config() const noexcept { return config_; }
    [[nodiscard]] const GaugeRuntimePolicy& policy() const noexcept { return policy_; }

    [[nodiscard]] GaugeState initialState(GaugeType type) const noexcept;

    GaugeResult applyJudgement(GaugeState& state, Judgement judgement, double time_ms) const;
    GaugeResult applyDamage(GaugeState& state, double damage_percent, double time_ms = 0.0) const;
    GaugeResult applyJudgementWeighted(GaugeState& state, Judgement judgement, double time_ms, double weight) const;

private:
    [[nodiscard]] GaugeDeltaTable tableFor(GaugeType type) const noexcept;
    [[nodiscard]] double deltaFor(GaugeType type, Judgement judgement) const noexcept;

    GaugeConfig config_;
    GaugeRuntimePolicy policy_;
};

}  // namespace tenriff::game
