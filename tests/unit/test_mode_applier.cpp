#include <algorithm>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "gameplay/KeyModeConverter.h"
#include "gameplay/ModeApplier.h"

namespace {

struct Span {
    int lane = 0;
    int64_t start = 0;
    int64_t end = 0;
};

bool contains_warning(const std::vector<std::string>& warnings, std::string_view needle) {
    for (const auto& warning : warnings) {
        if (warning.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool has_lane_overlap(const tenriff::gameplay::GameplayChart& chart) {
    const int lane_count = chart.lane_count;
    if (lane_count <= 0) {
        return false;
    }

    std::vector<Span> spans;
    spans.reserve(chart.notes.size());
    for (const auto& note : chart.notes) {
        int64_t end = note.end_sample.value_or(note.start_sample);
        if (end < note.start_sample) {
            end = note.start_sample;
        }
        spans.push_back({note.lane, note.start_sample, end});
    }

    std::sort(spans.begin(), spans.end(), [](const Span& lhs, const Span& rhs) {
        if (lhs.start == rhs.start) {
            return lhs.end < rhs.end;
        }
        return lhs.start < rhs.start;
    });

    std::vector<int64_t> lane_end(static_cast<std::size_t>(lane_count), std::numeric_limits<int64_t>::min());
    for (const auto& span : spans) {
        if (span.lane <= 0 || span.lane > lane_count) {
            continue;
        }
        const auto index = static_cast<std::size_t>(span.lane - 1);
        if (span.start <= lane_end[index]) {
            return true;
        }
        lane_end[index] = std::max(lane_end[index], span.end);
    }
    return false;
}

bool is_time_sorted(const tenriff::gameplay::GameplayChart& chart) {
    for (std::size_t i = 1; i < chart.notes.size(); ++i) {
        const auto& previous = chart.notes[i - 1];
        const auto& current = chart.notes[i];
        if (current.start_sample < previous.start_sample) {
            return false;
        }
        if (current.start_sample == previous.start_sample && current.lane < previous.lane) {
            return false;
        }
    }
    return true;
}

std::string chart_signature(const tenriff::gameplay::GameplayChart& chart) {
    std::string signature;
    signature.reserve(chart.notes.size() * 32);
    signature += std::to_string(chart.lane_count);
    signature.push_back('|');

    for (const auto& note : chart.notes) {
        signature += std::to_string(note.lane);
        signature.push_back('@');
        signature += std::to_string(note.start_sample);
        signature.push_back(':');
        if (note.end_sample.has_value()) {
            signature += std::to_string(note.end_sample.value());
        } else {
            signature.push_back('_');
        }
        signature.push_back(':');
        signature.push_back(note.release_required ? 'R' : 'N');
        signature.push_back(';');
    }

    return signature;
}

void append_note(tenriff::gameplay::GameplayChart& chart,
                 int lane,
                 int64_t start_sample,
                 std::optional<int64_t> end_sample = std::nullopt,
                 bool release_required = false) {
    tenriff::gameplay::NoteEvent note;
    note.lane = lane;
    note.start_sample = start_sample;
    note.end_sample = end_sample;
    note.release_required = release_required;
    note.note_id = chart.notes.size() + 1;
    chart.notes.push_back(note);
    chart.duration_samples = std::max(chart.duration_samples, note.end_sample.value_or(note.start_sample));
}

tenriff::gameplay::GameplayChart make_representative_chart(int lane_count) {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = lane_count;

    for (int lane = 1; lane <= lane_count; ++lane) {
        append_note(chart, lane, 100);
    }
    for (int lane = 1; lane <= lane_count; ++lane) {
        append_note(chart, lane, 300 + lane * 37);
    }
    for (int lane = 1; lane <= lane_count; lane += 2) {
        append_note(chart, lane, 800 + lane * 23, 1100 + lane * 23, lane % 4 == 1);
    }
    for (int lane = 2; lane <= lane_count; lane += 2) {
        append_note(chart, lane, 1450 + lane * 29);
    }
    for (int lane = 1; lane <= lane_count; ++lane) {
        append_note(chart, lane, 1850 + ((lane % 3) * 11));
    }

    chart.duration_samples = std::max<int64_t>(chart.duration_samples, 2200);
    return chart;
}

tenriff::gameplay::GameplayChart make_hold_chart(int lane_count) {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = lane_count;

    append_note(chart, 1, 120, 360, true);
    append_note(chart, std::max(2, lane_count / 2), 540);
    append_note(chart, lane_count, 760, 1080, false);
    append_note(chart, std::max(1, lane_count - 1), 1320);
    append_note(chart, std::max(1, (lane_count + 1) / 2), 1560, 1910, true);
    chart.duration_samples = std::max<int64_t>(chart.duration_samples, 2100);

    return chart;
}

tenriff::gameplay::ModeApplyResult apply_key_mode(const tenriff::gameplay::GameplayChart& chart,
                                                  tenriff::gameplay::KeyMode key_mode,
                                                  uint32_t seed,
                                                  double base_bpm = 180.0,
                                                  int sample_rate = 44100) {
    tenriff::gameplay::ModeSettings settings;
    settings.key_mode = key_mode;
    settings.random_seed = seed;

    tenriff::gameplay::ModeApplyContext context;
    context.base_bpm = base_bpm;
    context.sample_rate = sample_rate;
    return tenriff::gameplay::apply_mode_settings(chart, settings, context);
}

bool contains_note_shape(const tenriff::gameplay::GameplayChart& chart,
                         const tenriff::gameplay::NoteEvent& reference_note) {
    return std::any_of(chart.notes.begin(), chart.notes.end(), [&reference_note](const auto& note) {
        return note.start_sample == reference_note.start_sample &&
               note.end_sample == reference_note.end_sample &&
               note.release_required == reference_note.release_required;
    });
}

}  // namespace

TEST_CASE("Super Random avoids overlapping lanes") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 4;
    chart.duration_samples = 1000;
    chart.notes = {
        {1, 100, 150},
        {2, 100, std::nullopt},
        {3, 120, std::nullopt},
        {4, 140, std::nullopt},
        {1, 200, std::nullopt},
        {2, 240, 320},
        {3, 260, std::nullopt}
    };

    ModeSettings settings;
    settings.random = RandomMode::SuperRandom;
    settings.random_seed = 1234;

    auto result = apply_mode_settings(chart, settings);
    CHECK_FALSE(has_lane_overlap(result.chart));
    CHECK(is_time_sorted(result.chart));
}

TEST_CASE("key mode parser accepts 4K through 16K") {
    using tenriff::gameplay::KeyMode;
    using tenriff::gameplay::parse_key_mode;

    REQUIRE(parse_key_mode("4k").has_value());
    CHECK(parse_key_mode("4k").value() == KeyMode::Keys4);
    REQUIRE(parse_key_mode("5K").has_value());
    CHECK(parse_key_mode("5K").value() == KeyMode::Keys5);
    REQUIRE(parse_key_mode("6key").has_value());
    CHECK(parse_key_mode("6key").value() == KeyMode::Keys6);
    REQUIRE(parse_key_mode("7k").has_value());
    CHECK(parse_key_mode("7k").value() == KeyMode::Keys7);
    REQUIRE(parse_key_mode("8k").has_value());
    CHECK(parse_key_mode("8k").value() == KeyMode::Keys8);
    REQUIRE(parse_key_mode("9k").has_value());
    CHECK(parse_key_mode("9k").value() == KeyMode::Keys9);
    REQUIRE(parse_key_mode("10k").has_value());
    CHECK(parse_key_mode("10k").value() == KeyMode::Keys10);
    REQUIRE(parse_key_mode("16k").has_value());
    CHECK(parse_key_mode("16k").value() == KeyMode::Keys16);
}

TEST_CASE("key mode converter preserves notes when reducing lanes") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 6;
    chart.duration_samples = 500;
    chart.notes = {
        {5, 100, std::nullopt},
        {6, 220, std::nullopt}
    };

    ModeSettings settings;
    settings.key_mode = KeyMode::Keys4;
    settings.random_seed = 7;

    ModeApplyContext context;
    context.base_bpm = 180.0;
    context.sample_rate = 44100;

    auto result = apply_mode_settings(chart, settings, context);
    CHECK(result.chart.lane_count == 4);
    CHECK(result.chart.notes.size() == 2u);
    CHECK(result.chart.notes[0].start_sample == 100);
    CHECK(result.chart.notes[1].start_sample == 220);
    for (const auto& note : result.chart.notes) {
        CHECK(note.lane >= 1);
        CHECK(note.lane <= 4);
    }
}

TEST_CASE("key mode converter expands into new lanes when increasing lanes") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 4;
    chart.duration_samples = 800;
    chart.notes = {
        {1, 100, std::nullopt},
        {2, 100, std::nullopt},
        {3, 220, 360},
        {4, 420, std::nullopt}
    };

    ModeSettings settings;
    settings.key_mode = KeyMode::Keys7;
    settings.random_seed = 11;

    ModeApplyContext context;
    context.base_bpm = 170.0;
    context.sample_rate = 44100;

    auto result = apply_mode_settings(chart, settings, context);
    CHECK(result.chart.lane_count == 7);
    CHECK_FALSE(result.chart.notes.empty());
    CHECK(is_time_sorted(result.chart));
    CHECK(contains_warning(result.warnings, "Key mode converter remapped"));

    for (const auto& note : result.chart.notes) {
        CHECK(note.lane >= 1);
        CHECK(note.lane <= 7);
    }
}

TEST_CASE("key mode converter keeps representative conversion cases valid") {
    using namespace tenriff::gameplay;

    struct ConversionCase {
        const char* label = "";
        GameplayChart chart;
        KeyMode key_mode = KeyMode::Auto;
        int target_lanes = 0;
        uint32_t seed = 0;
    };

    const std::vector<ConversionCase> cases = {
        {"10k->4k", make_representative_chart(10), KeyMode::Keys4, 4, 101},
        {"10k->6k", make_representative_chart(10), KeyMode::Keys6, 6, 202},
        {"8k->5k", make_representative_chart(8), KeyMode::Keys5, 5, 303},
        {"7k->10k", make_representative_chart(7), KeyMode::Keys10, 10, 404},
        {"4k->16k", make_representative_chart(4), KeyMode::Keys16, 16, 505}
    };

    for (const auto& case_data : cases) {
        const auto result = apply_key_mode(case_data.chart, case_data.key_mode, case_data.seed, 174.0);

        CHECK(result.chart.lane_count == case_data.target_lanes);
        CHECK_FALSE(result.chart.notes.empty());
        CHECK(is_time_sorted(result.chart));
        CHECK(result.chart.duration_samples == case_data.chart.duration_samples);
        CHECK(contains_warning(result.warnings, "Key mode converter remapped"));

        for (const auto& note : result.chart.notes) {
            CHECK(note.lane >= 1);
            CHECK(note.lane <= case_data.target_lanes);
            if (note.end_sample.has_value()) {
                CHECK(note.end_sample.value() > note.start_sample);
            }
        }
    }
}

TEST_CASE("key mode converter is deterministic for a fixed seed and varies across seeds") {
    using namespace tenriff::gameplay;

    const GameplayChart chart = make_representative_chart(4);

    const auto fixed_a = apply_key_mode(chart, KeyMode::Keys16, 777, 168.0);
    const auto fixed_b = apply_key_mode(chart, KeyMode::Keys16, 777, 168.0);
    CHECK(chart_signature(fixed_a.chart) == chart_signature(fixed_b.chart));

    std::set<std::string> unique_layouts;
    for (uint32_t seed : {11u, 22u, 33u, 44u, 55u, 66u}) {
        unique_layouts.insert(chart_signature(apply_key_mode(chart, KeyMode::Keys16, seed, 168.0).chart));
    }
    CHECK(unique_layouts.size() >= 2u);
}

TEST_CASE("key mode converter preserves hold timing metadata when reducing lanes") {
    using namespace tenriff::gameplay;

    const GameplayChart chart = make_hold_chart(8);
    const auto result = apply_key_mode(chart, KeyMode::Keys4, 909, 182.0);

    CHECK(result.chart.lane_count == 4);
    CHECK(contains_warning(result.warnings, "Key mode converter remapped"));
    CHECK_FALSE(has_lane_overlap(result.chart));

    for (const auto& note : chart.notes) {
        if (!note.end_sample.has_value()) {
            continue;
        }
        CHECK(contains_note_shape(result.chart, note));
    }
}

TEST_CASE("key mode converter preserves hold timing metadata when increasing lanes") {
    using namespace tenriff::gameplay;

    const GameplayChart chart = make_hold_chart(4);
    const auto result = apply_key_mode(chart, KeyMode::Keys10, 1212, 160.0);

    CHECK(result.chart.lane_count == 10);
    CHECK(contains_warning(result.warnings, "Key mode converter remapped"));
    CHECK_FALSE(result.chart.notes.empty());
    CHECK(is_time_sorted(result.chart));

    for (const auto& note : chart.notes) {
        if (!note.end_sample.has_value()) {
            continue;
        }
        CHECK(contains_note_shape(result.chart, note));
    }
}

TEST_CASE("key mode converter trims dense chords to the target lane budget on reduction") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 10;
    chart.duration_samples = 300;
    for (int lane = 1; lane <= 10; ++lane) {
        append_note(chart, lane, 120);
    }

    const auto result = apply_key_mode(chart, KeyMode::Keys4, 31337, 180.0);
    CHECK(result.chart.lane_count == 4);
    CHECK(result.chart.notes.size() == 4u);
    for (const auto& note : result.chart.notes) {
        CHECK(note.start_sample == 120);
        CHECK(note.lane >= 1);
        CHECK(note.lane <= 4);
    }
}

TEST_CASE("key mode converter keeps source chord width when expanding to 8-plus targets") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 6;
    chart.duration_samples = 300;
    for (int lane = 1; lane <= 6; ++lane) {
        append_note(chart, lane, 180);
    }

    const auto result = apply_key_mode(chart, KeyMode::Keys8, 4242, 176.0);
    CHECK(result.chart.lane_count == 8);
    CHECK(result.chart.notes.size() == 6u);

    bool used_new_lane = false;
    for (const auto& note : result.chart.notes) {
        CHECK(note.start_sample == 180);
        if (note.lane > 6) {
            used_new_lane = true;
        }
    }
    CHECK(used_new_lane);
}

TEST_CASE("key mode converter resolves missing source lane count from note data") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 0;
    chart.duration_samples = 700;
    chart.notes = {
        {2, 100, std::nullopt},
        {5, 260, std::nullopt},
        {7, 420, 560}
    };

    const auto result = apply_key_mode(chart, KeyMode::Keys5, 5150, 180.0);
    CHECK(result.chart.lane_count == 5);
    CHECK_FALSE(result.chart.notes.empty());
    for (const auto& note : result.chart.notes) {
        CHECK(note.lane >= 1);
        CHECK(note.lane <= 5);
    }
}

TEST_CASE("key mode conversion followed by super random stays within target lanes without overlap") {
    using namespace tenriff::gameplay;

    GameplayChart chart = make_hold_chart(8);

    ModeSettings settings;
    settings.key_mode = KeyMode::Keys4;
    settings.random = RandomMode::SuperRandom;
    settings.random_seed = 9090;

    ModeApplyContext context;
    context.base_bpm = 176.0;
    context.sample_rate = 44100;

    const auto result = apply_mode_settings(chart, settings, context);
    CHECK(result.chart.lane_count == 4);
    CHECK_FALSE(result.chart.notes.empty());
    CHECK_FALSE(has_lane_overlap(result.chart));

    for (const auto& note : result.chart.notes) {
        CHECK(note.lane >= 1);
        CHECK(note.lane <= 4);
    }
}
