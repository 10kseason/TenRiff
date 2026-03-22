#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/JudgementLoopTiming.h"
#include "gameplay/GameplayEngine.h"
#include "input/InputEvent.h"

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    int sample_rate = 44100;
    int frames_per_buffer = 256;
    int lane_count = 10;
    double duration_seconds = 120.0;
    double notes_per_second = 40.0;
    int repeats = 7;
    int hold_interval = 16;
    double hold_duration_ms = 180.0;
    double tap_release_ms = 8.0;
};

struct SyntheticInputEvent {
    int lane = 1;
    tenriff::input::InputState state = tenriff::input::InputState::Pressed;
    int64_t sample = 0;
};

struct SyntheticScenario {
    tenriff::gameplay::GameplayChart chart;
    std::vector<SyntheticInputEvent> events;
};

struct RunResult {
    double elapsed_ms = 0.0;
    std::uint64_t callback_count = 0;
    std::uint64_t judgement_tick_count = 0;
    tenriff::gameplay::JudgementCounts counts;
    int64_t raw_score = 0;
    int max_combo = 0;
    bool finished = false;
    bool game_over = false;
};

struct BenchSummary {
    std::string label;
    int polling_hz = 0;
    double median_ms = 0.0;
    std::uint64_t callback_count = 0;
    std::uint64_t judgement_tick_count = 0;
    tenriff::gameplay::JudgementCounts counts;
    int64_t raw_score = 0;
    int max_combo = 0;
    bool finished = false;
    bool game_over = false;
};

bool parse_i32(std::string_view text, int& value) {
    if (text.empty()) {
        return false;
    }
    int parsed = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc() || result.ptr != last) {
        return false;
    }
    value = parsed;
    return true;
}

bool parse_double(std::string_view text, double& value) {
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t consumed = 0;
        value = std::stod(std::string(text), &consumed);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

void print_usage(const char* argv0) {
    std::cout << "Usage: " << (argv0 ? argv0 : "gameplay_judgement_benchmark")
              << " [--sample-rate <hz>] [--frames-per-buffer <n>] [--lanes <n>]"
              << " [--seconds <s>] [--notes-per-second <n>]"
              << " [--repeats <n>] [--hold-interval <n>]"
              << " [--hold-duration-ms <ms>] [--tap-release-ms <ms>]\n";
}

bool parse_options(int argc, char** argv, Options& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        auto require_next = [&](const char* flag) -> std::optional<std::string_view> {
            if (index + 1 >= argc) {
                std::cerr << "[error] " << flag << " requires a value.\n";
                return std::nullopt;
            }
            return std::string_view(argv[++index]);
        };

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        }
        if (arg == "--sample-rate") {
            auto next = require_next("--sample-rate");
            if (!next.has_value() || !parse_i32(*next, options.sample_rate) || options.sample_rate <= 0) {
                std::cerr << "[error] --sample-rate requires a positive integer.\n";
                return false;
            }
            continue;
        }
        if (arg == "--frames-per-buffer") {
            auto next = require_next("--frames-per-buffer");
            if (!next.has_value() || !parse_i32(*next, options.frames_per_buffer) || options.frames_per_buffer <= 0) {
                std::cerr << "[error] --frames-per-buffer requires a positive integer.\n";
                return false;
            }
            continue;
        }
        if (arg == "--lanes") {
            auto next = require_next("--lanes");
            if (!next.has_value() || !parse_i32(*next, options.lane_count) || options.lane_count <= 0) {
                std::cerr << "[error] --lanes requires a positive integer.\n";
                return false;
            }
            continue;
        }
        if (arg == "--seconds") {
            auto next = require_next("--seconds");
            if (!next.has_value() || !parse_double(*next, options.duration_seconds) || options.duration_seconds <= 0.0) {
                std::cerr << "[error] --seconds requires a positive number.\n";
                return false;
            }
            continue;
        }
        if (arg == "--notes-per-second") {
            auto next = require_next("--notes-per-second");
            if (!next.has_value() || !parse_double(*next, options.notes_per_second) || options.notes_per_second <= 0.0) {
                std::cerr << "[error] --notes-per-second requires a positive number.\n";
                return false;
            }
            continue;
        }
        if (arg == "--repeats") {
            auto next = require_next("--repeats");
            if (!next.has_value() || !parse_i32(*next, options.repeats) || options.repeats <= 0) {
                std::cerr << "[error] --repeats requires a positive integer.\n";
                return false;
            }
            continue;
        }
        if (arg == "--hold-interval") {
            auto next = require_next("--hold-interval");
            if (!next.has_value() || !parse_i32(*next, options.hold_interval) || options.hold_interval < 0) {
                std::cerr << "[error] --hold-interval requires zero or a positive integer.\n";
                return false;
            }
            continue;
        }
        if (arg == "--hold-duration-ms") {
            auto next = require_next("--hold-duration-ms");
            if (!next.has_value() || !parse_double(*next, options.hold_duration_ms) || options.hold_duration_ms <= 0.0) {
                std::cerr << "[error] --hold-duration-ms requires a positive number.\n";
                return false;
            }
            continue;
        }
        if (arg == "--tap-release-ms") {
            auto next = require_next("--tap-release-ms");
            if (!next.has_value() || !parse_double(*next, options.tap_release_ms) || options.tap_release_ms < 0.0) {
                std::cerr << "[error] --tap-release-ms requires zero or a positive number.\n";
                return false;
            }
            continue;
        }

        std::cerr << "[error] Unknown argument: " << arg << '\n';
        print_usage(argv[0]);
        return false;
    }

    return true;
}

int64_t ms_to_samples(double ms, int sample_rate) {
    return static_cast<int64_t>(std::llround(ms * static_cast<double>(sample_rate) / 1000.0));
}

SyntheticScenario build_synthetic_scenario(const Options& options) {
    SyntheticScenario scenario;
    auto& chart = scenario.chart;
    chart.lane_count = options.lane_count;

    const int64_t requested_duration_samples =
        static_cast<int64_t>(std::llround(options.duration_seconds * static_cast<double>(options.sample_rate)));
    const int64_t note_interval_samples =
        std::max<int64_t>(1, static_cast<int64_t>(std::llround(static_cast<double>(options.sample_rate) /
                                                               options.notes_per_second)));
    const int64_t hold_duration_samples = std::max<int64_t>(1, ms_to_samples(options.hold_duration_ms, options.sample_rate));
    const int64_t tap_release_samples = std::max<int64_t>(0, ms_to_samples(options.tap_release_ms, options.sample_rate));

    int64_t last_relevant_sample = 0;
    std::size_t note_id = 0;
    for (int64_t sample = note_interval_samples; sample < requested_duration_samples; sample += note_interval_samples) {
        tenriff::gameplay::NoteEvent note;
        note.lane = static_cast<int>((note_id % static_cast<std::size_t>(options.lane_count)) + 1);
        note.start_sample = sample;
        note.note_id = note_id;

        const bool make_hold = options.hold_interval > 0 &&
                               ((static_cast<int>(note_id % static_cast<std::size_t>(options.hold_interval))) ==
                                (options.hold_interval - 1));
        if (make_hold) {
            const int64_t hold_end_sample =
                std::min<int64_t>(requested_duration_samples - 1, sample + hold_duration_samples);
            if (hold_end_sample > sample) {
                note.end_sample = hold_end_sample;
                note.release_required = ((note_id / static_cast<std::size_t>(options.hold_interval)) % 2u) == 0u;
                scenario.events.push_back({note.lane, tenriff::input::InputState::Released, hold_end_sample});
                last_relevant_sample = std::max(last_relevant_sample, hold_end_sample);
            }
        } else if (tap_release_samples > 0) {
            scenario.events.push_back({note.lane,
                                       tenriff::input::InputState::Released,
                                       std::min<int64_t>(requested_duration_samples - 1, sample + tap_release_samples)});
            last_relevant_sample = std::max(last_relevant_sample, sample + tap_release_samples);
        }

        scenario.events.push_back({note.lane, tenriff::input::InputState::Pressed, sample});
        chart.notes.push_back(note);
        last_relevant_sample = std::max(last_relevant_sample, sample);
        ++note_id;
    }

    std::sort(scenario.events.begin(), scenario.events.end(), [](const SyntheticInputEvent& lhs,
                                                                 const SyntheticInputEvent& rhs) {
        if (lhs.sample != rhs.sample) {
            return lhs.sample < rhs.sample;
        }
        if (lhs.state != rhs.state) {
            return lhs.state == tenriff::input::InputState::Pressed;
        }
        return lhs.lane < rhs.lane;
    });

    chart.duration_samples = std::max<int64_t>(requested_duration_samples, last_relevant_sample + options.sample_rate);
    return scenario;
}

tenriff::gameplay::GameplayConfig make_gameplay_config(const Options& options) {
    tenriff::gameplay::GameplayConfig config;
    config.sample_rate = options.sample_rate;
    config.rate = 1.0;
    config.judge = {};
    config.gauge = {};
    config.initial_gauge = tenriff::game::GaugeType::Normal;
    config.input_offset_ms = 0.0;
    return config;
}

RunResult run_once(const Options& options,
                   const SyntheticScenario& scenario,
                   std::optional<int> polling_hz) {
    tenriff::gameplay::GameplayEngine engine(scenario.chart, make_gameplay_config(options));
    const int64_t duration_samples = scenario.chart.duration_samples;
    const int64_t frames_per_buffer = options.frames_per_buffer;

    std::size_t event_index = 0;
    std::uint64_t callback_count = 0;
    std::uint64_t judgement_tick_count = 0;

    auto consume_events_until = [&](int64_t sample_limit) {
        while (event_index < scenario.events.size() && scenario.events[event_index].sample <= sample_limit) {
            const auto& event = scenario.events[event_index];
            static_cast<void>(engine.handle_input(event.lane, event.state, event.sample));
            ++event_index;
        }
    };

    auto begin = Clock::now();
    if (!polling_hz.has_value()) {
        for (int64_t buffer_start = 0; buffer_start < duration_samples; buffer_start += frames_per_buffer) {
            const int64_t buffer_end = std::min<int64_t>(duration_samples, buffer_start + frames_per_buffer);
            ++callback_count;
            consume_events_until(buffer_end);
            engine.advance(buffer_end);
            ++judgement_tick_count;
        }
    } else {
        const auto plan =
            tenriff::app::build_judgement_loop_timing_plan(options.sample_rate, polling_hz.value());
        int64_t carry = 0;
        for (int64_t buffer_start = 0; buffer_start < duration_samples; buffer_start += frames_per_buffer) {
            const int64_t buffer_end = std::min<int64_t>(duration_samples, buffer_start + frames_per_buffer);
            ++callback_count;
            int64_t tick_cursor = buffer_start;
            while (tick_cursor < buffer_end) {
                const int64_t step =
                    tenriff::app::next_judgement_loop_step_samples(plan, carry);
                const int64_t tick_end = std::min<int64_t>(buffer_end, tick_cursor + step);
                consume_events_until(tick_end);
                engine.advance(tick_end);
                ++judgement_tick_count;
                tick_cursor = tick_end;
            }
        }
    }
    consume_events_until(duration_samples);
    engine.advance(duration_samples);
    auto end = Clock::now();

    const auto& stats = engine.stats();
    RunResult result;
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - begin).count();
    result.callback_count = callback_count;
    result.judgement_tick_count = judgement_tick_count;
    result.counts = stats.counts;
    result.raw_score = stats.raw_score;
    result.max_combo = stats.max_combo;
    result.finished = engine.is_finished();
    result.game_over = engine.is_game_over();
    return result;
}

bool results_match(const RunResult& lhs, const RunResult& rhs) {
    return lhs.counts.pg == rhs.counts.pg &&
           lhs.counts.gr == rhs.counts.gr &&
           lhs.counts.gd == rhs.counts.gd &&
           lhs.counts.bd == rhs.counts.bd &&
           lhs.counts.pr == rhs.counts.pr &&
           lhs.raw_score == rhs.raw_score &&
           lhs.max_combo == rhs.max_combo &&
           lhs.finished == rhs.finished &&
           lhs.game_over == rhs.game_over;
}

BenchSummary run_benchmark(const Options& options,
                          const SyntheticScenario& scenario,
                          const std::string& label,
                          std::optional<int> polling_hz) {
    static_cast<void>(run_once(options, scenario, polling_hz));  // warm-up

    std::vector<double> samples_ms;
    samples_ms.reserve(static_cast<std::size_t>(options.repeats));

    RunResult reference = run_once(options, scenario, polling_hz);
    samples_ms.push_back(reference.elapsed_ms);
    for (int repeat = 1; repeat < options.repeats; ++repeat) {
        RunResult sample = run_once(options, scenario, polling_hz);
        if (!results_match(reference, sample)) {
            std::cerr << "[error] Non-deterministic benchmark result in mode " << label << ".\n";
            std::exit(1);
        }
        samples_ms.push_back(sample.elapsed_ms);
    }

    std::sort(samples_ms.begin(), samples_ms.end());
    const double median_ms = samples_ms[samples_ms.size() / 2];

    BenchSummary summary;
    summary.label = label;
    summary.polling_hz = polling_hz.value_or(0);
    summary.median_ms = median_ms;
    summary.callback_count = reference.callback_count;
    summary.judgement_tick_count = reference.judgement_tick_count;
    summary.counts = reference.counts;
    summary.raw_score = reference.raw_score;
    summary.max_combo = reference.max_combo;
    summary.finished = reference.finished;
    summary.game_over = reference.game_over;
    return summary;
}

void print_summary_table(const Options& options,
                         const SyntheticScenario& scenario,
                         const std::vector<BenchSummary>& summaries) {
    const double buffer_ms =
        static_cast<double>(options.frames_per_buffer) * 1000.0 / static_cast<double>(options.sample_rate);
    const double baseline_ms = summaries.empty() ? 0.0 : summaries.front().median_ms;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "config: sample_rate=" << options.sample_rate
              << " frames=" << options.frames_per_buffer
              << " buffer_ms=" << buffer_ms
              << " duration_s=" << options.duration_seconds
              << " lanes=" << options.lane_count
              << " notes=" << scenario.chart.notes.size()
              << " events=" << scenario.events.size()
              << " repeats=" << options.repeats << "\n";
    std::cout << "mode                 median_ms   rel   callback_us   budget_%   ticks      tick_per_cb\n";
    for (const auto& summary : summaries) {
        const double callback_us =
            (summary.callback_count == 0) ? 0.0 : (summary.median_ms * 1000.0 / static_cast<double>(summary.callback_count));
        const double budget_pct =
            (buffer_ms <= 0.0) ? 0.0 : ((callback_us / 1000.0) / buffer_ms * 100.0);
        const double rel =
            (baseline_ms <= 0.0) ? 1.0 : (summary.median_ms / baseline_ms);
        const double ticks_per_callback =
            (summary.callback_count == 0) ? 0.0
                                          : (static_cast<double>(summary.judgement_tick_count) /
                                             static_cast<double>(summary.callback_count));

        std::cout << std::left << std::setw(20) << summary.label
                  << std::right << std::setw(10) << summary.median_ms
                  << std::setw(7) << rel
                  << std::setw(14) << callback_us
                  << std::setw(11) << budget_pct
                  << std::setw(11) << summary.judgement_tick_count
                  << std::setw(14) << ticks_per_callback
                  << "\n";
    }

    if (!summaries.empty()) {
        const auto& reference = summaries.front();
        std::cout << "result: PG=" << reference.counts.pg
                  << " GR=" << reference.counts.gr
                  << " GD=" << reference.counts.gd
                  << " BD=" << reference.counts.bd
                  << " PR=" << reference.counts.pr
                  << " score=" << reference.raw_score
                  << " max_combo=" << reference.max_combo
                  << " finished=" << (reference.finished ? "yes" : "no")
                  << " game_over=" << (reference.game_over ? "yes" : "no") << "\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        return 1;
    }

    const SyntheticScenario scenario = build_synthetic_scenario(options);
    if (scenario.chart.notes.empty()) {
        std::cerr << "[error] Synthetic benchmark scenario produced no notes.\n";
        return 1;
    }

    std::vector<BenchSummary> summaries;
    summaries.push_back(run_benchmark(options, scenario, "baseline-buffer", std::nullopt));
    for (int polling_hz : {1000, 2000, 4000, 8000}) {
        summaries.push_back(run_benchmark(options,
                                          scenario,
                                          std::to_string(polling_hz) + "hz-substep",
                                          polling_hz));
        if (!results_match(
                RunResult{0.0,
                          summaries.front().callback_count,
                          summaries.front().judgement_tick_count,
                          summaries.front().counts,
                          summaries.front().raw_score,
                          summaries.front().max_combo,
                          summaries.front().finished,
                          summaries.front().game_over},
                RunResult{0.0,
                          summaries.back().callback_count,
                          summaries.back().judgement_tick_count,
                          summaries.back().counts,
                          summaries.back().raw_score,
                          summaries.back().max_combo,
                          summaries.back().finished,
                          summaries.back().game_over})) {
            std::cerr << "[error] Benchmark result mismatch between baseline and "
                      << summaries.back().label << ".\n";
            return 1;
        }
    }

    print_summary_table(options, scenario, summaries);
    return 0;
}
