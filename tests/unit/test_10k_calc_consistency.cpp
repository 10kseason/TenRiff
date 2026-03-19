#include "doctest/doctest.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "chart/OsuDifficulty.h"
#include "config/SimpleJson.h"

using tenriff::chart::DifficultyPreset;
using tenriff::chart::ManiaDifficultyOptions;
using tenriff::chart::OsuDifficultyMetrics;
using tenriff::chart::OsuManiaChart;
using tenriff::chart::OsuManiaNote;
using tenriff::chart::calculate_osu_mania_difficulty;
using tenriff::config::JsonArray;
using tenriff::config::JsonObject;
using tenriff::config::JsonParseResult;
using tenriff::config::JsonValue;
using tenriff::config::json_stringify;
using tenriff::config::parse_json;

namespace {

struct TempDirGuard {
    std::filesystem::path path;

    ~TempDirGuard() {
        if (!path.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    }
};

struct CompareCase {
    std::string name;
    OsuManiaChart chart;
    ManiaDifficultyOptions options;
    std::string preset_name;
    std::string mode_name;
};

struct PythonMetrics {
    double circus_rating = 0.0;
    int revive_level = 0;
    double peak_nps = 0.0;
    double average_nps = 0.0;
};

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

OsuManiaChart make_chart(int key_count) {
    OsuManiaChart chart;
    chart.key_count = key_count;
    chart.overall_difficulty = 8.0;
    chart.base_bpm = 180.0;
    return chart;
}

void add_tap(OsuManiaChart& chart, int column, int time_ms) {
    chart.notes.push_back(OsuManiaNote{column, time_ms, std::nullopt, 0});
}

void add_hold(OsuManiaChart& chart, int column, int start_ms, int end_ms) {
    chart.notes.push_back(OsuManiaNote{column, start_ms, end_ms, 0});
}

std::filesystem::path make_temp_dir() {
    const auto base = std::filesystem::temp_directory_path() / "tenriff_10k_calc_compare";
    std::filesystem::create_directories(base);
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const auto candidate = base / ("case_" + std::to_string(attempt));
        std::error_code ec;
        if (std::filesystem::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

std::filesystem::path find_repo_root() {
    auto current = std::filesystem::current_path();
    for (;;) {
        if (std::filesystem::exists(current / "10k-calc" / "new_calc.py") &&
            std::filesystem::exists(current / "10k-calc" / "config.yaml")) {
            return current;
        }
        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return {};
}

std::string compact_diagnostics(std::string text) {
    for (char& ch : text) {
        if (ch == '\r' || ch == '\n' || ch == '\t') {
            ch = ' ';
        }
    }
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    if (text.size() > 220) {
        text.resize(217);
        text += "...";
    }
    return text;
}

JsonObject note_to_json(const OsuManiaNote& note) {
    JsonObject object;
    object.emplace("column", JsonValue(static_cast<double>(note.column)));
    object.emplace("start_time_ms", JsonValue(static_cast<double>(note.start_time_ms)));
    if (note.end_time_ms.has_value()) {
        object.emplace("end_time_ms", JsonValue(static_cast<double>(*note.end_time_ms)));
    } else {
        object.emplace("end_time_ms", JsonValue());
    }
    return object;
}

JsonObject case_to_json(const CompareCase& test_case) {
    JsonArray notes;
    notes.reserve(test_case.chart.notes.size());
    for (const auto& note : test_case.chart.notes) {
        notes.emplace_back(note_to_json(note));
    }

    JsonObject object;
    object.emplace("name", JsonValue(test_case.name));
    object.emplace("key_count", JsonValue(static_cast<double>(test_case.chart.key_count)));
    object.emplace("preset_name", JsonValue(test_case.preset_name));
    object.emplace("mode_name", JsonValue(test_case.mode_name));
    object.emplace("notes", JsonValue(std::move(notes)));
    return object;
}

std::vector<CompareCase> build_cases() {
    std::vector<CompareCase> cases;

    {
        CompareCase test_case;
        test_case.name = "dense_10k";
        test_case.chart = make_chart(10);
        for (int i = 0; i < 48; ++i) {
            add_tap(test_case.chart, i % 10, i * 75);
        }
        test_case.options.mode_name = "10K";
        test_case.preset_name = "osu_od_interpolate_8.0";
        test_case.mode_name = "10K";
        cases.push_back(std::move(test_case));
    }

    {
        CompareCase test_case;
        test_case.name = "jack_10k";
        test_case.chart = make_chart(10);
        for (int i = 0; i < 64; ++i) {
            add_tap(test_case.chart, 4, i * 70);
        }
        test_case.options.mode_name = "10K";
        test_case.preset_name = "osu_od_interpolate_8.0";
        test_case.mode_name = "10K";
        cases.push_back(std::move(test_case));
    }

    {
        CompareCase test_case;
        test_case.name = "mixed_hold_10k";
        test_case.chart = make_chart(10);
        for (int i = 0; i < 32; ++i) {
            add_tap(test_case.chart, i % 10, i * 160);
            if (i % 8 == 0) {
                add_hold(test_case.chart, 2, i * 160 + 40, i * 160 + 280);
            }
        }
        test_case.options.mode_name = "10K";
        test_case.preset_name = "osu_od_interpolate_8.0";
        test_case.mode_name = "10K";
        cases.push_back(std::move(test_case));
    }

    {
        OsuManiaChart chart = make_chart(6);
        for (int i = 0; i < 24; ++i) {
            add_tap(chart, i % 6, i * 110);
        }

        CompareCase scratch_case;
        scratch_case.name = "scratch_5p1";
        scratch_case.chart = chart;
        scratch_case.options.preset = DifficultyPreset::QwilightBmsEz;
        scratch_case.options.mode_name = "5+1";
        scratch_case.preset_name = "qwilight_bms_ez";
        scratch_case.mode_name = "5+1";
        cases.push_back(std::move(scratch_case));

        CompareCase full_case;
        full_case.name = "full_6k";
        full_case.chart = std::move(chart);
        full_case.options.preset = DifficultyPreset::QwilightBmsEz;
        full_case.options.mode_name = "6K";
        full_case.preset_name = "qwilight_bms_ez";
        full_case.mode_name = "6K";
        cases.push_back(std::move(full_case));
    }

    {
        OsuManiaChart chart = make_chart(16);
        for (int i = 0; i < 32; ++i) {
            add_tap(chart, i % 16, i * 130);
        }

        CompareCase scratch_case;
        scratch_case.name = "scratch_dp16";
        scratch_case.chart = chart;
        scratch_case.options.preset = DifficultyPreset::QwilightBmsEz;
        scratch_case.options.mode_name = "DP16";
        scratch_case.preset_name = "qwilight_bms_ez";
        scratch_case.mode_name = "DP16";
        cases.push_back(std::move(scratch_case));

        CompareCase full_case;
        full_case.name = "full_16k";
        full_case.chart = std::move(chart);
        full_case.options.preset = DifficultyPreset::QwilightBmsEz;
        full_case.options.mode_name = "16K";
        full_case.preset_name = "qwilight_bms_ez";
        full_case.mode_name = "16K";
        cases.push_back(std::move(full_case));
    }

    return cases;
}

std::string read_pipe_output(FILE* pipe) {
    std::string output;
    std::array<char, 4096> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output.append(buffer.data());
    }
    return output;
}

#ifdef _WIN32
std::wstring quote_arg(const std::wstring& text) {
    std::wstring quoted = L"\"";
    for (const wchar_t ch : text) {
        if (ch == L'"') {
            quoted.push_back(L'\\');
        }
        quoted.push_back(ch);
    }
    quoted.push_back(L'"');
    return quoted;
}

CommandResult run_command(const std::wstring& command) {
    CommandResult result;
    FILE* pipe = _wpopen(command.c_str(), L"r");
    if (pipe == nullptr) {
        return result;
    }
    result.output = read_pipe_output(pipe);
    result.exit_code = _pclose(pipe);
    return result;
}
#else
std::string quote_arg(const std::string& text) {
    std::string quoted = "\"";
    for (const char ch : text) {
        if (ch == '"') {
            quoted.push_back('\\');
        }
        quoted.push_back(ch);
    }
    quoted.push_back('"');
    return quoted;
}

CommandResult run_command(const std::string& command) {
    CommandResult result;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return result;
    }
    result.output = read_pipe_output(pipe);
    result.exit_code = pclose(pipe);
    return result;
}
#endif

std::optional<std::vector<PythonMetrics>> run_python_reference(const std::filesystem::path& repo_root,
                                                               const std::vector<CompareCase>& cases,
                                                               std::string& diagnostics) {
    TempDirGuard temp_dir{make_temp_dir()};
    if (temp_dir.path.empty()) {
        diagnostics = "failed to create temp directory";
        return std::nullopt;
    }

    JsonArray case_array;
    case_array.reserve(cases.size());
    for (const auto& test_case : cases) {
        case_array.emplace_back(case_to_json(test_case));
    }
    JsonObject root;
    root.emplace("cases", JsonValue(std::move(case_array)));

    const auto input_path = temp_dir.path / "input.json";
    {
        std::ofstream output(input_path, std::ios::binary);
        output << json_stringify(JsonValue(std::move(root)), 2);
    }

    const auto script_path = temp_dir.path / "run_compare.py";
    {
        std::ofstream output(script_path, std::ios::binary);
        output <<
            "import contextlib\n"
            "import io\n"
            "import importlib.util\n"
            "import json\n"
            "import pathlib\n"
            "import sys\n"
            "import yaml\n"
            "\n"
            "repo_root = pathlib.Path(sys.argv[1])\n"
            "input_path = pathlib.Path(sys.argv[2])\n"
            "calc_path = repo_root / '10k-calc' / 'new_calc.py'\n"
            "config_path = repo_root / '10k-calc' / 'config.yaml'\n"
            "\n"
            "spec = importlib.util.spec_from_file_location('tenriff_new_calc', calc_path)\n"
            "module = importlib.util.module_from_spec(spec)\n"
            "spec.loader.exec_module(module)\n"
            "\n"
            "with config_path.open('r', encoding='utf-8') as fh:\n"
            "    config = yaml.safe_load(fh)\n"
            "with input_path.open('r', encoding='utf-8') as fh:\n"
            "    payload = json.load(fh)\n"
            "\n"
            "type_order = {'ln_end': 0, 'note': 1, 'ln_start': 2}\n"
            "results = []\n"
            "for case in payload['cases']:\n"
            "    notes = []\n"
            "    for note in case['notes']:\n"
            "        column = int(note['column']) + 1\n"
            "        start_time_ms = int(note['start_time_ms'])\n"
            "        end_time_ms = note.get('end_time_ms')\n"
            "        if end_time_ms is not None and int(end_time_ms) > start_time_ms:\n"
            "            notes.append({'time': start_time_ms / 1000.0, 'column': column, 'type': 'ln_start'})\n"
            "            notes.append({'time': int(end_time_ms) / 1000.0, 'column': column, 'type': 'ln_end'})\n"
            "        else:\n"
            "            notes.append({'time': start_time_ms / 1000.0, 'column': column, 'type': 'note'})\n"
            "    notes.sort(key=lambda item: (item['time'], type_order.get(item.get('type', 'note'), 1), item['column']))\n"
            "    if notes:\n"
            "        duration = max(notes[-1]['time'] - notes[0]['time'], 1.0)\n"
            "    else:\n"
            "        duration = 0.0\n"
            "    with contextlib.redirect_stdout(io.StringIO()):\n"
            "        metrics = module.calculate_total_difficulty(\n"
            "            notes,\n"
            "            duration,\n"
            "            key_mode=int(case['key_count']),\n"
            "            preset_name=case['preset_name'],\n"
            "            mode_name=case['mode_name'],\n"
            "            config=config,\n"
            "        )\n"
            "    results.append({\n"
            "        'name': case['name'],\n"
            "        'circus_rating': metrics['circus_rating'],\n"
            "        'revive_level': metrics['revive_lv'],\n"
            "        'peak_nps': metrics['peak_nps'],\n"
            "        'average_nps': metrics['global_nps'],\n"
            "    })\n"
            "\n"
            "print(json.dumps({'results': results}, ensure_ascii=False))\n";
    }

#ifdef _WIN32
    const std::vector<std::wstring> interpreters = {L"python", L"py -3"};
    for (const auto& interpreter : interpreters) {
        const auto command = interpreter + L" " + quote_arg(script_path.wstring()) + L" " +
                             quote_arg(repo_root.wstring()) + L" " + quote_arg(input_path.wstring()) + L" 2>&1";
        const auto result = run_command(command);
        if (result.exit_code != 0) {
            diagnostics = result.output;
            continue;
        }

        const JsonParseResult parsed = parse_json(result.output);
        if (!parsed.success()) {
            diagnostics = "failed to parse python output: " + parsed.error + "\nraw:\n" + result.output;
            continue;
        }

        const auto* root_object = parsed.root->as_object();
        if (root_object == nullptr) {
            diagnostics = "python output root was not an object";
            continue;
        }
        const auto results_it = root_object->find("results");
        if (results_it == root_object->end() || results_it->second.as_array() == nullptr) {
            diagnostics = "python output did not contain a results array";
            continue;
        }

        std::vector<PythonMetrics> metrics;
        for (const auto& entry : *results_it->second.as_array()) {
            const auto* object = entry.as_object();
            if (object == nullptr) {
                diagnostics = "python result entry was not an object";
                return std::nullopt;
            }
            PythonMetrics values;
            values.circus_rating = object->at("circus_rating").as_number();
            values.revive_level = static_cast<int>(object->at("revive_level").as_number());
            values.peak_nps = object->at("peak_nps").as_number();
            values.average_nps = object->at("average_nps").as_number();
            metrics.push_back(values);
        }
        return metrics;
    }
#else
    const std::vector<std::string> interpreters = {"python3", "python"};
    for (const auto& interpreter : interpreters) {
        const auto command = interpreter + " " + quote_arg(script_path.string()) + " " +
                             quote_arg(repo_root.string()) + " " + quote_arg(input_path.string()) + " 2>&1";
        const auto result = run_command(command);
        if (result.exit_code != 0) {
            diagnostics = result.output;
            continue;
        }

        const JsonParseResult parsed = parse_json(result.output);
        if (!parsed.success()) {
            diagnostics = "failed to parse python output: " + parsed.error + "\nraw:\n" + result.output;
            continue;
        }

        const auto* root_object = parsed.root->as_object();
        if (root_object == nullptr) {
            diagnostics = "python output root was not an object";
            continue;
        }
        const auto results_it = root_object->find("results");
        if (results_it == root_object->end() || results_it->second.as_array() == nullptr) {
            diagnostics = "python output did not contain a results array";
            continue;
        }

        std::vector<PythonMetrics> metrics;
        for (const auto& entry : *results_it->second.as_array()) {
            const auto* object = entry.as_object();
            if (object == nullptr) {
                diagnostics = "python result entry was not an object";
                return std::nullopt;
            }
            PythonMetrics values;
            values.circus_rating = object->at("circus_rating").as_number();
            values.revive_level = static_cast<int>(object->at("revive_level").as_number());
            values.peak_nps = object->at("peak_nps").as_number();
            values.average_nps = object->at("average_nps").as_number();
            metrics.push_back(values);
        }
        return metrics;
    }
#endif

    return std::nullopt;
}

}  // namespace

TEST_CASE("10k-calc python reference matches C++ port on representative fixtures") {
    const auto repo_root = find_repo_root();
    if (repo_root.empty()) {
        std::cout << "[skip] 10k-calc python reference not available; skipping optional consistency check\n";
        return;
    }

    const auto cases = build_cases();
    REQUIRE_FALSE(cases.empty());

    std::string diagnostics;
    const auto python_metrics = run_python_reference(repo_root, cases, diagnostics);
    if (!python_metrics.has_value()) {
        std::cout << "[skip] 10k-calc python reference unavailable: " << compact_diagnostics(diagnostics) << '\n';
        return;
    }
    REQUIRE_EQ(python_metrics->size(), cases.size());

    const auto check_close = [](std::string_view case_name, std::string_view field, double cpp_value, double py_value) {
        if (std::abs(cpp_value - py_value) <= 1e-9) {
            return;
        }
        throw doctest::TestFailure(std::string(case_name) + " " + std::string(field) + " mismatch (" +
                                   std::to_string(cpp_value) + " vs " + std::to_string(py_value) + ")");
    };

    for (std::size_t i = 0; i < cases.size(); ++i) {
        const auto cpp_metrics = calculate_osu_mania_difficulty(cases[i].chart, cases[i].options);
        const auto& py_metrics = (*python_metrics)[i];

        check_close(cases[i].name, "circus_rating", cpp_metrics.circus_rating, py_metrics.circus_rating);
        if (cpp_metrics.revive_level != py_metrics.revive_level) {
            throw doctest::TestFailure(cases[i].name + " revive_level mismatch (" +
                                       std::to_string(cpp_metrics.revive_level) + " vs " +
                                       std::to_string(py_metrics.revive_level) + ")");
        }
        check_close(cases[i].name, "peak_nps", cpp_metrics.peak_nps, py_metrics.peak_nps);
        check_close(cases[i].name, "average_nps", cpp_metrics.average_nps, py_metrics.average_nps);
    }
}
