#include <algorithm>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "gameplay/GameplayEngine.h"
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

TEST_CASE("Mirror reverses lanes deterministically while preserving note metadata") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 5;
    append_note(chart, 1, 100, 260, true);
    append_note(chart, 2, 400);
    append_note(chart, 3, 700, 940, false);
    append_note(chart, 5, 1200);
    chart.mines.push_back(MineEvent{1, 500, 5.0, false, 3, 77});
    chart.notes[0].audio_asset_id = 7;
    chart.notes[0].audio_gain = 0.65f;

    ModeSettings settings;
    settings.random = RandomMode::Mirror;
    settings.random_seed = 1;

    const auto first = apply_mode_settings(chart, settings);
    settings.random_seed = 9999;
    const auto second = apply_mode_settings(chart, settings);

    CHECK(first.chart.lane_count == chart.lane_count);
    CHECK(chart_signature(first.chart) == chart_signature(second.chart));
    CHECK(is_time_sorted(first.chart));
    REQUIRE(first.chart.notes.size() == chart.notes.size());
    REQUIRE(first.chart.mines.size() == 1u);
    REQUIRE(second.chart.mines.size() == 1u);
    CHECK(first.chart.mines[0].lane == 5);
    CHECK(second.chart.mines[0].lane == 5);

    for (const auto& original : chart.notes) {
        const auto mirrored = std::find_if(first.chart.notes.begin(), first.chart.notes.end(), [&](const auto& note) {
            return note.note_id == original.note_id;
        });
        REQUIRE(mirrored != first.chart.notes.end());
        CHECK(mirrored->lane == chart.lane_count + 1 - original.lane);
        CHECK(mirrored->start_sample == original.start_sample);
        CHECK(mirrored->end_sample == original.end_sample);
        CHECK(mirrored->release_required == original.release_required);
        CHECK(mirrored->audio_asset_id == original.audio_asset_id);
        CHECK(mirrored->audio_gain == doctest::Approx(original.audio_gain));
    }

    const auto restored = apply_mode_settings(first.chart, settings);
    CHECK(chart_signature(restored.chart) == chart_signature(chart));
    REQUIRE(restored.chart.mines.size() == 1u);
    CHECK(restored.chart.mines[0].lane == 1);
}

TEST_CASE("random mode parser accepts Mirror tokens") {
    using tenriff::gameplay::RandomMode;
    using tenriff::gameplay::parse_random_mode;
    using tenriff::gameplay::to_string;

    REQUIRE(parse_random_mode("mirror").has_value());
    CHECK(parse_random_mode("mirror").value() == RandomMode::Mirror);
    CHECK(to_string(RandomMode::Mirror) == "MIRROR");
}

TEST_CASE("Mirror preserves player halves for 10K and 16K layouts") {
    using namespace tenriff::gameplay;

    for (const int lane_count : {10, 16}) {
        GameplayChart chart;
        chart.lane_count = lane_count;
        for (int lane = 1; lane <= lane_count; ++lane) {
            append_note(chart, lane, 100);
        }

        ModeSettings settings;
        settings.random = RandomMode::Mirror;
        const auto result = apply_mode_settings(chart, settings);
        const int half = lane_count / 2;

        CHECK(is_time_sorted(result.chart));
        REQUIRE(result.chart.notes.size() == chart.notes.size());
        for (const auto& original : chart.notes) {
            const auto mirrored = std::find_if(result.chart.notes.begin(), result.chart.notes.end(), [&](const auto& note) {
                return note.note_id == original.note_id;
            });
            REQUIRE(mirrored != result.chart.notes.end());
            const int group_start = original.lane <= half ? 1 : half + 1;
            const int expected_lane = group_start + half - 1 - (original.lane - group_start);
            CHECK(mirrored->lane == expected_lane);
            CHECK((mirrored->lane <= half) == (original.lane <= half));
        }
    }
}

TEST_CASE("key mode conversion runs before Mirror") {
    using namespace tenriff::gameplay;

    GameplayChart chart = make_hold_chart(8);
    chart.mines.push_back(MineEvent{8, 1500, 5.0, false, 4, 91});
    ModeApplyContext context;
    context.base_bpm = 176.0;
    context.sample_rate = 44100;

    ModeSettings converted_settings;
    converted_settings.key_mode = KeyMode::Keys4;
    converted_settings.random_seed = 31337;
    const auto converted = apply_mode_settings(chart, converted_settings, context);

    ModeSettings mirror_settings = converted_settings;
    mirror_settings.random = RandomMode::Mirror;
    const auto mirrored = apply_mode_settings(chart, mirror_settings, context);

    REQUIRE(converted.chart.lane_count == 4);
    REQUIRE(mirrored.chart.lane_count == converted.chart.lane_count);
    REQUIRE(mirrored.chart.notes.size() == converted.chart.notes.size());
    REQUIRE(converted.chart.mines.size() == 1u);
    REQUIRE(mirrored.chart.mines.size() == 1u);
    CHECK(converted.chart.mines[0].lane == 4);
    CHECK(mirrored.chart.mines[0].lane == 1);
    for (const auto& converted_note : converted.chart.notes) {
        const auto mirrored_note = std::find_if(
            mirrored.chart.notes.begin(), mirrored.chart.notes.end(), [&](const auto& note) {
                return note.note_id == converted_note.note_id;
            });
        REQUIRE(mirrored_note != mirrored.chart.notes.end());
        CHECK(mirrored_note->lane == converted.chart.lane_count + 1 - converted_note.lane);
        CHECK(mirrored_note->start_sample == converted_note.start_sample);
        CHECK(mirrored_note->end_sample == converted_note.end_sample);
        CHECK(mirrored_note->release_required == converted_note.release_required);
    }
}

TEST_CASE("key mode parser accepts none plus 4K through 16K") {
    using tenriff::gameplay::KeyMode;
    using tenriff::gameplay::parse_key_mode;

    REQUIRE(parse_key_mode("none").has_value());
    CHECK(parse_key_mode("none").value() == KeyMode::Auto);
    REQUIRE(parse_key_mode("auto").has_value());
    CHECK(parse_key_mode("auto").value() == KeyMode::Auto);
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
    REQUIRE(parse_key_mode("12k").has_value());
    CHECK(parse_key_mode("12k").value() == KeyMode::Keys12);
    REQUIRE(parse_key_mode("14k").has_value());
    CHECK(parse_key_mode("14k").value() == KeyMode::Keys14);
    REQUIRE(parse_key_mode("16k").has_value());
    CHECK(parse_key_mode("16k").value() == KeyMode::Keys16);
}

TEST_CASE("key conversion algorithm parser accepts Krrcream and KeyWeaver nK2 aliases") {
    using tenriff::gameplay::KeyModeConversionAlgorithm;
    using tenriff::gameplay::parse_key_mode_conversion_algorithm;

    REQUIRE(parse_key_mode_conversion_algorithm("krrcream").has_value());
    CHECK(parse_key_mode_conversion_algorithm("krrcream").value() ==
          KeyModeConversionAlgorithm::Krrcream);
    REQUIRE(parse_key_mode_conversion_algorithm("keyweaver").has_value());
    CHECK(parse_key_mode_conversion_algorithm("keyweaver").value() ==
          KeyModeConversionAlgorithm::NK2);
    REQUIRE(parse_key_mode_conversion_algorithm("nk2").has_value());
    CHECK(parse_key_mode_conversion_algorithm("nk2").value() ==
          KeyModeConversionAlgorithm::NK2);
    CHECK_FALSE(parse_key_mode_conversion_algorithm("unknown").has_value());
}

TEST_CASE("nK2 preset parser accepts Native 12 and Transform 35 tokens") {
    using tenriff::gameplay::Nk2Preset;
    using tenriff::gameplay::parse_nk2_preset;

    REQUIRE(parse_nk2_preset("native").has_value());
    CHECK(parse_nk2_preset("native12").value() == Nk2Preset::Native);
    REQUIRE(parse_nk2_preset("transform").has_value());
    CHECK(parse_nk2_preset("transform35").value() == Nk2Preset::Transform);
    CHECK_FALSE(parse_nk2_preset("harder").has_value());
}
TEST_CASE("gauge mode parser accepts the Gauge Shift token") {
    using tenriff::gameplay::GaugeMode;
    using tenriff::gameplay::parse_gauge_mode;

    REQUIRE(parse_gauge_mode("shift").has_value());
    CHECK(parse_gauge_mode("shift").value() == GaugeMode::Shift);
    REQUIRE(parse_gauge_mode("gauge_shift").has_value());
    CHECK(parse_gauge_mode("gauge_shift").value() == GaugeMode::Shift);
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

TEST_CASE("7+1 SP to 8K converts only the seven key lanes and autoplays scratch keysounds") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 8;
    chart.scratch_lanes = {1};
    const std::size_t scratch_asset = chart.intern_audio_asset("scratch.wav");

    append_note(chart, 1, 100);
    chart.notes.back().audio_asset_id = scratch_asset;
    for (int lane = 2; lane <= 8; ++lane) {
        append_note(chart, lane, static_cast<int64_t>(lane) * 10000);
    }

    const auto result = apply_key_mode(chart, KeyMode::Keys8, 4242, 176.0);

    CHECK(result.chart.lane_count == 8);
    CHECK(result.chart.scratch_lanes.empty());
    REQUIRE(result.chart.notes.size() >= 7u);
    CHECK(std::none_of(result.chart.notes.begin(), result.chart.notes.end(), [](const NoteEvent& note) {
        return note.note_id == 1u;
    }));
    for (std::size_t key_note_id = 2; key_note_id <= 8; ++key_note_id) {
        CHECK(std::count_if(result.chart.notes.begin(), result.chart.notes.end(), [key_note_id](const NoteEvent& note) {
            return note.note_id == key_note_id;
        }) == 1);
    }
    for (const auto& note : result.chart.notes) {
        CHECK(note.lane >= 1);
        CHECK(note.lane <= 8);
    }

    REQUIRE(result.chart.audio_cues.size() == 1u);
    CHECK(result.chart.audio_cues.front().start_sample == 100);
    CHECK(result.chart.audio_cues.front().asset_id == scratch_asset);
    CHECK(contains_warning(result.warnings, "remapped only the key lanes"));
}

TEST_CASE("native 8K remains unchanged when it has no scratch lane metadata") {
    using namespace tenriff::gameplay;

    const GameplayChart chart = make_representative_chart(8);
    const auto result = apply_key_mode(chart, KeyMode::Keys8, 4242, 176.0);

    CHECK(chart_signature(result.chart) == chart_signature(chart));
    CHECK(result.warnings.empty());
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

TEST_CASE("runtime 10K key mode uses krrcream 10K preset defaults") {
    using namespace tenriff::gameplay;

    const GameplayChart chart = make_representative_chart(4);

    ModeSettings settings;
    settings.key_mode = KeyMode::Keys10;
    settings.random_seed = 9999;

    ModeApplyContext context;
    context.base_bpm = 174.0;
    context.sample_rate = 48000;

    const auto runtime_result = apply_mode_settings(chart, settings, context);

    KeyModeConverterOptions expected_options;
    expected_options.target_lane_count = 10;
    expected_options.max_keys = 10;
    expected_options.min_keys = 1;
    expected_options.transform_speed_slot = 5;
    expected_options.seed = 0;
    expected_options.base_bpm = context.base_bpm;
    expected_options.sample_rate = context.sample_rate;

    const auto expected_result = convert_key_mode_chart(chart, expected_options);

    REQUIRE(runtime_result.chart.lane_count == 10);
    REQUIRE(expected_result.converted);
    CHECK(chart_signature(runtime_result.chart) == chart_signature(expected_result.chart));
    CHECK(contains_warning(runtime_result.warnings, "Key mode converter remapped"));
}
TEST_CASE("runtime mode settings use the selected KeyWeaver nK2 converter") {
    using namespace tenriff::gameplay;

    const GameplayChart chart = make_representative_chart(4);

    ModeSettings settings;
    settings.key_mode = KeyMode::Keys8;
    settings.key_conversion_algorithm = KeyModeConversionAlgorithm::NK2;
    settings.random_seed = 999;

    ModeApplyContext context;
    context.base_bpm = 120.0;
    context.sample_rate = 1000;

    const auto result = apply_mode_settings(chart, settings, context);

    REQUIRE(result.chart.lane_count == 8);
    CHECK_FALSE(result.chart.notes.empty());
    CHECK(is_time_sorted(result.chart));
    CHECK_FALSE(has_lane_overlap(result.chart));
    CHECK(contains_warning(result.warnings, "nK2 remapped"));
}

TEST_CASE("nK2 support budgets taper with source density") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 4;
    chart.duration_samples = 102000;
    for (int index = 0; index < 100; ++index) {
        append_note(chart, index % 4 + 1, static_cast<int64_t>(index + 1) * 1000);
    }

    KeyModeConverterOptions native_options;
    native_options.target_lane_count = 8;
    native_options.base_bpm = 120.0;
    native_options.sample_rate = 1000;
    native_options.algorithm = KeyModeConversionAlgorithm::NK2;
    native_options.nk2_preset = Nk2Preset::Native;

    KeyModeConverterOptions transform_options = native_options;
    transform_options.nk2_preset = Nk2Preset::Transform;

    const auto native_result = convert_key_mode_chart(chart, native_options);
    const auto transform_result = convert_key_mode_chart(chart, transform_options);

    REQUIRE(native_result.converted);
    REQUIRE(transform_result.converted);
    // The headline 12% / 35% are caps, not targets. 100 notes over 99 seconds is
    // ~1 note/sec, a tenth of the engine's 10 nps reference, so each preset keeps
    // a tenth of its budget: 12% -> 2 notes, 35% -> 4. Dense charts run into the
    // support safety windows well before the budget and are unaffected.
    CHECK(native_result.chart.notes.size() == chart.notes.size() + 2u);
    CHECK(transform_result.chart.notes.size() == chart.notes.size() + 4u);
    CHECK(native_result.chart.notes.size() < transform_result.chart.notes.size());
    CHECK(contains_warning(native_result.warnings, "Native 12%"));
    CHECK(contains_warning(transform_result.warnings, "Transform 35%"));
    CHECK_FALSE(has_lane_overlap(native_result.chart));
    CHECK_FALSE(has_lane_overlap(transform_result.chart));
}
TEST_CASE("nK2 key-count reduction preserves source timing and remains hittable") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 8;
    chart.duration_samples = 2000;
    for (int lane = 1; lane <= chart.lane_count; ++lane) {
        append_note(chart, lane, 1000);
    }

    KeyModeConverterOptions options;
    options.target_lane_count = 4;
    options.base_bpm = 120.0;
    options.sample_rate = 1000;
    options.algorithm = KeyModeConversionAlgorithm::NK2;

    const auto result = convert_key_mode_chart(chart, options);
    REQUIRE(result.converted);
    CHECK(result.chart.lane_count == 4);
    REQUIRE_FALSE(result.chart.notes.empty());

    std::set<int> hit_lanes;
    for (const auto& note : result.chart.notes) {
        CHECK(note.start_sample == 1000);
        CHECK_FALSE(note.end_sample.has_value());
        CHECK(hit_lanes.insert(note.lane).second);
    }
    CHECK(hit_lanes.size() == result.chart.notes.size());
    CHECK(hit_lanes.size() <= 4u);

    GameplayConfig gameplay_config;
    gameplay_config.sample_rate = 1000;
    GameplayEngine engine(result.chart, gameplay_config);
    for (const int lane : hit_lanes) {
        const auto hit = engine.handle_input(lane, tenriff::input::InputState::Pressed, 1000);
        REQUIRE(hit.has_value());
        CHECK(hit->start_sample == 1000);
    }
    CHECK(engine.stats().counts.pg == static_cast<int>(result.chart.notes.size()));
    CHECK(engine.stats().counts.pr == 0);
}

TEST_CASE("nK2 converter is selectable and ignores Krrcream-only tuning fields") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 4;
    chart.duration_samples = 2200;
    append_note(chart, 1, 100);
    append_note(chart, 2, 280);
    append_note(chart, 3, 460);
    append_note(chart, 4, 640);
    append_note(chart, 1, 820, 1220, true);
    append_note(chart, 2, 1040);
    append_note(chart, 3, 1420);
    append_note(chart, 4, 1780);

    KeyModeConverterOptions options;
    options.target_lane_count = 8;
    options.max_keys = 1;
    options.min_keys = 1;
    options.transform_speed_slot = 0;
    options.seed = 1;
    options.base_bpm = 120.0;
    options.sample_rate = 1000;
    options.algorithm = KeyModeConversionAlgorithm::NK2;

    const auto first = convert_key_mode_chart(chart, options);
    REQUIRE(first.converted);
    CHECK(first.chart.lane_count == 8);
    CHECK(first.chart.notes.size() > chart.notes.size());
    CHECK(is_time_sorted(first.chart));
    CHECK_FALSE(has_lane_overlap(first.chart));
    CHECK(contains_warning(first.warnings, "nK2 remapped"));
    CHECK(contains_warning(first.warnings, "added="));
    CHECK(std::any_of(first.chart.notes.begin(), first.chart.notes.end(), [](const NoteEvent& note) {
        return note.end_sample.has_value() && *note.end_sample == 1220 && note.release_required;
    }));
    std::set<std::size_t> source_note_ids;
    bool found_generated_support_note = false;
    for (const auto& note : first.chart.notes) {
        CHECK(note.lane >= 1);
        CHECK(note.lane <= 8);
        CHECK(note.note_id >= 1u);
        CHECK(note.note_id <= chart.notes.size());
        if (!source_note_ids.insert(note.note_id).second) {
            found_generated_support_note = true;
        }
    }
    CHECK(found_generated_support_note);
    options.max_keys = 8;
    options.min_keys = 8;
    options.transform_speed_slot = 8;
    options.seed = 999999;
    const auto second = convert_key_mode_chart(chart, options);
    REQUIRE(second.converted);
    CHECK(chart_signature(second.chart) == chart_signature(first.chart));
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

TEST_CASE("5+1 SP forced key mode removes scratch and converts only five key lanes") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 6;
    chart.scratch_lanes = {6};
    const std::size_t scratch_asset = chart.intern_audio_asset("scratch-5k.wav");
    append_note(chart, 6, 100);
    chart.notes.back().audio_asset_id = scratch_asset;
    const std::size_t scratch_note_id = chart.notes.back().note_id;
    for (int lane = 1; lane <= 5; ++lane) {
        append_note(chart, lane, 1000 + lane * 1000);
    }

    const auto result = apply_key_mode(chart, KeyMode::Keys8, 55, 180.0);
    CHECK(result.chart.lane_count == 8);
    CHECK(result.chart.scratch_lanes.empty());
    CHECK(std::none_of(result.chart.notes.begin(), result.chart.notes.end(),
                       [scratch_note_id](const NoteEvent& note) {
                           return note.note_id == scratch_note_id;
                       }));
    REQUIRE(result.chart.audio_cues.size() == 1u);
    CHECK(result.chart.audio_cues.front().asset_id == scratch_asset);
}

TEST_CASE("10+2 DP to 12K removes both scratches and converts player halves independently") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 12;
    chart.lane_group_size = 6;
    chart.scratch_lanes = {6, 12};
    const std::size_t scratch_asset = chart.intern_audio_asset("dp-scratch.wav");
    append_note(chart, 6, 100);
    chart.notes.back().audio_asset_id = scratch_asset;
    append_note(chart, 12, 200);
    chart.notes.back().audio_asset_id = scratch_asset;
    for (int lane = 1; lane <= 5; ++lane) {
        append_note(chart, lane, 1000 + lane * 1000);
    }
    for (int lane = 7; lane <= 11; ++lane) {
        append_note(chart, lane, 10000 + lane * 1000);
    }

    const auto result = apply_key_mode(chart, KeyMode::Keys12, 2026, 180.0);
    CHECK(result.chart.lane_count == 12);
    CHECK(result.chart.lane_group_size == 6);
    CHECK(result.chart.scratch_lanes.empty());
    REQUIRE(result.chart.audio_cues.size() == 2u);
    for (const auto& note : result.chart.notes) {
        REQUIRE(note.note_id >= 3u);
        if (note.note_id <= 7u) {
            CHECK(note.lane >= 1);
            CHECK(note.lane <= 6);
        } else {
            CHECK(note.lane >= 7);
            CHECK(note.lane <= 12);
        }
    }
}

TEST_CASE("14+2 DP supports scratch-free 14K and independent 16K expansion") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 16;
    chart.lane_group_size = 8;
    chart.scratch_lanes = {6, 14};
    for (int lane = 1; lane <= 16; ++lane) {
        append_note(chart, lane, 1000 + lane * 1000);
    }

    const auto fourteen = apply_key_mode(chart, KeyMode::Keys14, 14, 180.0);
    CHECK(fourteen.chart.lane_count == 14);
    CHECK(fourteen.chart.lane_group_size == 7);
    CHECK(fourteen.chart.scratch_lanes.empty());
    CHECK(fourteen.chart.notes.size() == 14u);

    const auto sixteen = apply_key_mode(chart, KeyMode::Keys16, 16, 180.0);
    CHECK(sixteen.chart.lane_count == 16);
    CHECK(sixteen.chart.lane_group_size == 8);
    CHECK(sixteen.chart.scratch_lanes.empty());
    for (const auto& note : sixteen.chart.notes) {
        if (note.note_id <= 8u && note.note_id != 6u) {
            CHECK(note.lane <= 8);
        } else if (note.note_id > 8u && note.note_id != 14u) {
            CHECK(note.lane >= 9);
        }
    }
}

TEST_CASE("R-Random keeps scratches fixed and rotates DP halves independently") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 16;
    chart.lane_group_size = 8;
    chart.scratch_lanes = {6, 14};
    for (int lane = 1; lane <= 16; ++lane) {
        append_note(chart, lane, 100);
    }

    ModeSettings settings;
    settings.random = RandomMode::RotateRandom;
    settings.random_seed = 77;
    const auto first = apply_mode_settings(chart, settings);
    const auto second = apply_mode_settings(chart, settings);
    CHECK(chart_signature(first.chart) == chart_signature(second.chart));

    for (const auto& original : chart.notes) {
        const auto transformed = std::find_if(
            first.chart.notes.begin(), first.chart.notes.end(), [&](const NoteEvent& note) {
                return note.note_id == original.note_id;
            });
        REQUIRE(transformed != first.chart.notes.end());
        if (original.lane == 6 || original.lane == 14) {
            CHECK(transformed->lane == original.lane);
        } else {
            CHECK((transformed->lane <= 8) == (original.lane <= 8));
        }
    }
}

TEST_CASE("DP Flip swaps player fields without changing note timing") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 14;
    chart.lane_group_size = 7;
    append_note(chart, 1, 100);
    append_note(chart, 8, 200);

    ModeSettings settings;
    settings.dp_flip = true;
    const auto result = apply_mode_settings(chart, settings);
    REQUIRE(result.chart.notes.size() == 2u);
    CHECK(result.chart.notes[0].note_id == 1u);
    CHECK(result.chart.notes[0].lane == 8);
    CHECK(result.chart.notes[0].start_sample == 100);
    CHECK(result.chart.notes[1].note_id == 2u);
    CHECK(result.chart.notes[1].lane == 1);
    CHECK(result.chart.notes[1].start_sample == 200);
}
