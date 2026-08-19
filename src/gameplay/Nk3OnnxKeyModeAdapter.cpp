#include "gameplay/Nk3OnnxKeyModeAdapter.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if defined(TENRIFF_ENABLE_NK3_ONNX)
#include <openvino/openvino.hpp>
#include <openvino/runtime/properties.hpp>
#endif

namespace tenriff::gameplay {
namespace {

constexpr int kSlices = 64;
constexpr int kNoteFeatures = 12;
constexpr int kTargetLanes = 18;
constexpr int kCandidateTypes = 8;
constexpr int kContextDim = 32;
constexpr int kMemorySlots = 64;
constexpr int64_t kPressureScale = 65'536;
constexpr int kCreatedJackWindowMs = 180;
constexpr double kMinimumGapSeconds = 0.045;
constexpr double kDistributionWindowSeconds = 2.0;
constexpr double kCreatedKeepJackWindowSeconds = 0.500;
constexpr double kTimeComparisonEpsilonSeconds = 1.0e-6;
constexpr int kGeneratedLnContextMs = 1'000;
constexpr int kGeneratedLnMinimumCount = 2;
constexpr double kGeneratedLnMinimumRatio = 0.40;
constexpr int kGeneratedLnMinimumDurationMs = 60;
constexpr double kMinusInfinityTime = -1.0e30;
constexpr int kPatternFeatureDim = 28;
constexpr int kPatternBatchStates = 32;
constexpr int kPatternOutputRoles = 2;
constexpr double kPatternWeight = 0.15;
constexpr double kPatternMaxResidual = 1.0;

using PatternFeatures = std::array<float, kTargetLanes * kPatternFeatureDim>;
using PatternScores = std::array<float, kTargetLanes * kCandidateTypes>;

enum class Origin { Original, Shifted, Created };

struct SourceNote {
    int64_t time_ms = 0;
    int64_t end_ms = 0;
    int source_lane = 0;
    std::size_t source_index = 0;
    bool hold = false;
};

struct Selection {
    int slice = 0;
    int lane = 0;
    int type = 0;
    int32_t score = 0;
};

struct PlacedNote {
    int64_t time_ms = 0;
    int64_t end_ms = 0;
    int source_lane = 0;
    std::size_t source_index = 0;
    int output_lane = 0;
    Origin origin = Origin::Original;
    int candidate_type = 0;
    int32_t score = 0;

    [[nodiscard]] bool hold() const { return end_ms > time_ms; }
};

struct BeamState {
    int64_t score = 0;
    std::vector<int> signature;
    std::vector<Selection> selections;
    std::array<double, kTargetLanes> last_time{};
    std::array<double, kTargetLanes> busy_until{};
    std::array<int, kTargetLanes> last_source{};
    std::array<int, kTargetLanes> lane_use{};
    int total_selections = 0;
    std::vector<std::pair<double, int>> recent_events;
};

struct ModelState {
    int64_t pressure = 0;
    std::array<float, kContextDim> fast{};
    std::array<float, kContextDim> slow{};
    std::array<float, kMemorySlots * kContextDim> memory{};
    std::array<uint8_t, kMemorySlots> memory_mask{};
};

struct BlockData {
    std::array<float, kSlices * kNoteFeatures> model_notes{};
    std::array<float, kSlices * kNoteFeatures> solver_notes{};
    std::array<uint8_t, kSlices> mask{};
    std::array<float, 16> parameters{};
    std::array<int64_t, kSlices> pressure_increment{};
};

struct BlockOutput {
    std::array<int32_t, kSlices * kTargetLanes * kCandidateTypes> scores{};
    std::array<uint8_t, kSlices * kTargetLanes * kCandidateTypes> valid{};
    std::array<int64_t, kSlices> addition_count{};
    ModelState next;
};

struct Candidate {
    int32_t score = 0;
    int id = 0;
    int lane = 0;
};

struct QualityReport {
    int same_time_collisions = 0;
    int long_note_conflicts = 0;
    int minimum_gap_violations = 0;
    int created_jacks = 0;
    int created_addition_jacks = 0;
    int novel_jacks = 0;
    int impossible_chords = 0;

    [[nodiscard]] bool safe() const {
        return same_time_collisions == 0 && long_note_conflicts == 0 &&
               minimum_gap_violations == 0 && created_jacks == 0 &&
               created_addition_jacks == 0 && novel_jacks == 0 &&
               impossible_chords == 0;
    }
};

int resolve_lane_count(const GameplayChart& chart) {
    int count = chart.lane_count;
    for (const auto& note : chart.notes) {
        count = std::max(count, note.lane);
    }
    return std::max(0, count);
}

int resolve_sample_rate(int sample_rate) {
    return sample_rate > 0 ? sample_rate : 44'100;
}

double resolve_base_bpm(double bpm) {
    return std::isfinite(bpm) && bpm > 0.0 ? bpm : 180.0;
}

int64_t samples_to_ms(int64_t samples, int sample_rate) {
    return static_cast<int64_t>(std::llround(
        static_cast<long double>(samples) * 1000.0L /
        static_cast<long double>(sample_rate)));
}

int64_t ms_to_samples(int64_t milliseconds, int sample_rate) {
    return static_cast<int64_t>(std::llround(
        static_cast<long double>(milliseconds) *
        static_cast<long double>(sample_rate) / 1000.0L));
}

int direct_lane(int source_lane, int source_keys, int target_keys) {
    if (source_keys <= 1) {
        return target_keys / 2;
    }
    return static_cast<int>(std::llround(
        static_cast<double>(source_lane) * (target_keys - 1) /
        static_cast<double>(source_keys - 1)));
}

double pressure_ratio(int source_keys, int target_keys) {
    if (target_keys <= source_keys) {
        return 0.0;
    }
    return std::min(0.65,
                    (std::pow(static_cast<double>(target_keys) / source_keys, 0.65) - 1.0) *
                        0.75);
}

double beat_phase(int64_t time_ms, double base_bpm) {
    const double beat_length = 60'000.0 / base_bpm;
    const double beat = static_cast<double>(time_ms) / beat_length;
    return beat - std::floor(beat);
}

std::vector<SourceNote> source_notes(const GameplayChart& chart, int source_keys,
                                     int sample_rate) {
    std::vector<SourceNote> notes;
    notes.reserve(chart.notes.size());
    for (std::size_t index = 0; index < chart.notes.size(); ++index) {
        const auto& note = chart.notes[index];
        if (note.lane <= 0 || note.lane > source_keys) {
            continue;
        }
        SourceNote mapped;
        mapped.time_ms = samples_to_ms(note.start_sample, sample_rate);
        mapped.end_ms = mapped.time_ms;
        mapped.source_lane = note.lane - 1;
        mapped.source_index = index;
        if (note.end_sample.has_value() && *note.end_sample > note.start_sample) {
            mapped.end_ms = samples_to_ms(*note.end_sample, sample_rate);
            mapped.hold = mapped.end_ms > mapped.time_ms;
        }
        notes.push_back(mapped);
    }
    std::stable_sort(notes.begin(), notes.end(), [](const SourceNote& lhs, const SourceNote& rhs) {
        if (lhs.time_ms != rhs.time_ms) {
            return lhs.time_ms < rhs.time_ms;
        }
        return lhs.source_index < rhs.source_index;
    });
    return notes;
}

BlockData build_block(const std::vector<SourceNote>& notes, std::size_t begin,
                      int source_keys, int target_keys, double base_bpm,
                      const ModelState& state) {
    BlockData block;
    if (begin >= notes.size()) {
        return block;
    }
    std::vector<int64_t> all_times;
    all_times.reserve(notes.size());
    std::unordered_map<int64_t, int> chord_sizes;
    for (const auto& note : notes) {
        all_times.push_back(note.time_ms);
        ++chord_sizes[note.time_ms];
    }
    const int64_t block_start = notes[begin].time_ms;
    const double base_pressure = pressure_ratio(source_keys, target_keys);
    const std::size_t count = std::min<std::size_t>(kSlices, notes.size() - begin);
    for (std::size_t slice = 0; slice < count; ++slice) {
        const auto& note = notes[begin + slice];
        const auto left = std::lower_bound(all_times.begin(), all_times.end(), note.time_ms - 500);
        const auto right = std::upper_bound(all_times.begin(), all_times.end(), note.time_ms + 500);
        const int local_nps = std::max<int>(1, static_cast<int>(right - left));
        const double capacity_nps = std::max(1.0, target_keys * 5.0);
        const double density_headroom =
            std::clamp(1.0 - local_nps / capacity_nps, 0.15, 1.0);
        const double lane_norm =
            static_cast<double>(note.source_lane) / std::max(1, source_keys - 1);
        const double phase = beat_phase(note.time_ms, base_bpm);
        const double chord_norm =
            static_cast<double>(chord_sizes[note.time_ms]) / source_keys;
        const double duration = (note.end_ms - note.time_ms) / 1000.0;
        const double rhythm_weight =
            1.0 - std::min({std::abs(phase), std::abs(phase - 0.5), std::abs(phase - 1.0)});
        const double salience =
            std::min(1.0, 0.55 + (note.hold ? 0.30 : 0.0) + 0.15 * chord_norm);
        const std::array<float, kNoteFeatures> features = {
            static_cast<float>((note.time_ms - block_start) / 1000.0),
            static_cast<float>(note.source_lane),
            static_cast<float>(lane_norm),
            static_cast<float>(duration),
            note.hold ? 1.0f : 0.0f,
            static_cast<float>(phase),
            static_cast<float>(lane_norm),
            static_cast<float>(std::min(1.0, local_nps / capacity_nps)),
            static_cast<float>(chord_norm),
            0.0f,
            static_cast<float>(rhythm_weight),
            static_cast<float>(salience),
        };
        std::copy(features.begin(), features.end(),
                  block.model_notes.begin() + static_cast<std::ptrdiff_t>(slice * kNoteFeatures));
        std::copy(features.begin(), features.end(),
                  block.solver_notes.begin() + static_cast<std::ptrdiff_t>(slice * kNoteFeatures));
        block.solver_notes[slice * kNoteFeatures] = note.time_ms / 1000.0f;
        block.mask[slice] = 1;
        block.pressure_increment[slice] = static_cast<int64_t>(std::llround(
            kPressureScale * base_pressure * density_headroom));
    }
    block.parameters = {static_cast<float>(source_keys), static_cast<float>(target_keys),
                        0.40f, 1.0f, 0.72f, 0.0f, 0.85f, 1.0f,
                        0.35f, 0.70f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    (void)state;
    return block;
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - ('a' - 'A'))
                                      : static_cast<char>(ch);
    });
    return value;
}

std::string selected_device() {
    const char* configured = std::getenv("TENRIFF_NK3_DEVICE");
    const std::string device = upper_ascii(configured ? configured : "GPU");
    if (device != "GPU" && device != "CPU") {
        throw std::runtime_error("TENRIFF_NK3_DEVICE must be GPU or CPU");
    }
    return device;
}

std::filesystem::path executable_directory() {
#ifdef _WIN32
    std::wstring path(32'768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length > 0 && length < path.size()) {
        path.resize(length);
        return std::filesystem::path(path).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

std::filesystem::path p64_model_path() {
    if (const char* configured = std::getenv("TENRIFF_NK3_MODEL")) {
        if (*configured != '\0') {
            return std::filesystem::path(configured);
        }
    }
    const auto beside_executable = executable_directory() / "models" / "NK3-P64-hybrid.onnx";
    if (std::filesystem::is_regular_file(beside_executable)) {
        return beside_executable;
    }
    return std::filesystem::current_path() / "models" / "NK3-P64-hybrid.onnx";
}

std::filesystem::path pattern_model_path(int target_keys) {
    const std::string filename =
        "NK3-general-pattern-" + std::to_string(target_keys) + "K.onnx";
    const auto beside_executable = executable_directory() / "models" / filename;
    if (std::filesystem::is_regular_file(beside_executable)) {
        return beside_executable;
    }
    return std::filesystem::current_path() / "models" / filename;
}

class PatternMlpEvaluator;

#if defined(TENRIFF_ENABLE_NK3_ONNX)
class OpenVinoEvaluator {
public:
    OpenVinoEvaluator(const std::filesystem::path& path, std::string device)
        : device_(std::move(device)) {
        const auto available = core_.get_available_devices();
        if (std::find(available.begin(), available.end(), device_) == available.end()) {
            throw std::runtime_error("OpenVINO device " + device_ + " is unavailable");
        }
        model_ = core_.read_model(path.wstring());
        op_count_ = model_->get_ordered_ops().size();
        queried_op_count_ = core_.query_model(model_, device_).size();
        compiled_ = core_.compile_model(model_, device_);
        execution_devices_ = compiled_.get_property(ov::execution_devices);
        const bool strict = !execution_devices_.empty() &&
                            std::all_of(execution_devices_.begin(), execution_devices_.end(),
                                        [&](const std::string& execution) {
                                            return execution == device_ ||
                                                   execution.rfind(device_ + ".", 0) == 0;
                                        });
        if (!strict) {
            throw std::runtime_error("OpenVINO did not execute NK3 on requested " + device_);
        }
    }

    BlockOutput run(const BlockData& block, const ModelState& state) {
        ov::Tensor notes(ov::element::f32, {1, kSlices, kNoteFeatures});
        ov::Tensor mask(ov::element::boolean, {1, kSlices});
        ov::Tensor parameters(ov::element::f32, {1, 16});
        ov::Tensor pressure(ov::element::i64, {1, 1});
        ov::Tensor increments(ov::element::i64, {1, kSlices});
        ov::Tensor fast(ov::element::f32, {1, kContextDim});
        ov::Tensor slow(ov::element::f32, {1, kContextDim});
        ov::Tensor memory(ov::element::f32, {1, kMemorySlots, kContextDim});
        ov::Tensor memory_mask(ov::element::boolean, {1, kMemorySlots});
        ov::Tensor hard_mask(ov::element::boolean,
                             {1, kSlices, kTargetLanes, kCandidateTypes});
        std::copy(block.model_notes.begin(), block.model_notes.end(), notes.data<float>());
        std::copy(block.mask.begin(), block.mask.end(), mask.data<uint8_t>());
        std::copy(block.parameters.begin(), block.parameters.end(), parameters.data<float>());
        pressure.data<int64_t>()[0] = state.pressure;
        std::copy(block.pressure_increment.begin(), block.pressure_increment.end(),
                  increments.data<int64_t>());
        std::copy(state.fast.begin(), state.fast.end(), fast.data<float>());
        std::copy(state.slow.begin(), state.slow.end(), slow.data<float>());
        std::copy(state.memory.begin(), state.memory.end(), memory.data<float>());
        std::copy(state.memory_mask.begin(), state.memory_mask.end(),
                  memory_mask.data<uint8_t>());
        std::fill_n(hard_mask.data<uint8_t>(),
                    kSlices * kTargetLanes * kCandidateTypes, uint8_t{1});

        auto request = compiled_.create_infer_request();
        request.set_tensor("notes", notes);
        request.set_tensor("mask", mask);
        request.set_tensor("parameters", parameters);
        request.set_tensor("pressure_state_q16", pressure);
        request.set_tensor("pressure_increment_q16", increments);
        request.set_tensor("fast_context_state", fast);
        request.set_tensor("slow_context_state", slow);
        request.set_tensor("memory", memory);
        request.set_tensor("memory_mask", memory_mask);
        request.set_tensor("hard_constraint_mask", hard_mask);
        request.infer();

        BlockOutput output;
        const auto scores = request.get_tensor("candidate_scores_q16");
        const auto valid = request.get_tensor("candidate_valid");
        const auto additions = request.get_tensor("addition_count");
        std::copy_n(scores.data<const int32_t>(), output.scores.size(), output.scores.begin());
        std::copy_n(valid.data<const uint8_t>(), output.valid.size(), output.valid.begin());
        std::copy_n(additions.data<const int64_t>(), output.addition_count.size(),
                    output.addition_count.begin());
        output.next.pressure =
            request.get_tensor("next_pressure_q16").data<const int64_t>()[0];
        std::copy_n(request.get_tensor("next_fast_context").data<const float>(),
                    output.next.fast.size(), output.next.fast.begin());
        std::copy_n(request.get_tensor("next_slow_context").data<const float>(),
                    output.next.slow.size(), output.next.slow.begin());
        std::copy_n(request.get_tensor("next_memory").data<const float>(),
                    output.next.memory.size(), output.next.memory.begin());
        std::copy_n(request.get_tensor("next_memory_mask").data<const uint8_t>(),
                    output.next.memory_mask.size(), output.next.memory_mask.begin());
        return output;
    }

    [[nodiscard]] std::string evidence() const {
        std::string devices;
        for (const auto& device : execution_devices_) {
            if (!devices.empty()) {
                devices += ',';
            }
            devices += device;
        }
        return "evaluator=OpenVINO " + device_ + " EXECUTION_DEVICES=" + devices +
               " query=" + std::to_string(queried_op_count_) + "/" +
               std::to_string(op_count_);
    }

private:
    std::string device_;
    ov::Core core_;
    std::shared_ptr<ov::Model> model_;
    ov::CompiledModel compiled_;
    std::size_t op_count_ = 0;
    std::size_t queried_op_count_ = 0;
    std::vector<std::string> execution_devices_;
};

OpenVinoEvaluator& shared_evaluator(const std::filesystem::path& path,
                                    const std::string& device) {
    static std::mutex evaluator_mutex;
    static std::map<std::string, std::unique_ptr<OpenVinoEvaluator>> evaluators;
    const std::string key = path.u8string() + '\n' + device;
    std::lock_guard<std::mutex> lock(evaluator_mutex);
    auto& evaluator = evaluators[key];
    if (!evaluator) {
        evaluator = std::make_unique<OpenVinoEvaluator>(path, device);
    }
    return *evaluator;
}

class PatternMlpEvaluator {
public:
    PatternMlpEvaluator(const std::filesystem::path& path, int target_keys)
        : target_keys_(target_keys), batch_rows_(target_keys * kPatternBatchStates) {
        const auto available = core_.get_available_devices();
        model_ = core_.read_model(path.wstring());
        model_->reshape({{"features", ov::PartialShape{batch_rows_, kPatternFeatureDim}}});
        op_count_ = model_->get_ordered_ops().size();
        std::string failures;
        for (const std::string candidate : {"NPU", "GPU", "CPU"}) {
            if (std::find(available.begin(), available.end(), candidate) == available.end()) {
                if (!failures.empty()) {
                    failures += "; ";
                }
                failures += candidate + ": unavailable";
                continue;
            }
            try {
                const std::size_t queried = core_.query_model(model_, candidate).size();
                auto compiled = core_.compile_model(model_, candidate);
                auto execution_devices = compiled.get_property(ov::execution_devices);
                const bool strict = !execution_devices.empty() &&
                                    std::all_of(
                                        execution_devices.begin(), execution_devices.end(),
                                        [&](const std::string& execution) {
                                            return execution == candidate ||
                                                   execution.rfind(candidate + ".", 0) == 0;
                                        });
                if (!strict) {
                    throw std::runtime_error("execution device mismatch");
                }
                device_ = candidate;
                queried_op_count_ = queried;
                compiled_ = std::move(compiled);
                execution_devices_ = std::move(execution_devices);
                features_ = ov::Tensor(
                    ov::element::f32,
                    {static_cast<std::size_t>(batch_rows_), kPatternFeatureDim});
                request_ = compiled_.create_infer_request();
                request_.set_tensor("features", features_);
                return;
            } catch (const std::exception& error) {
                if (!failures.empty()) {
                    failures += "; ";
                }
                failures += candidate + ": " + error.what();
            }
        }
        throw std::runtime_error("pattern MLP NPU/GPU/CPU routing failed (" + failures + ")");
    }

    [[nodiscard]] std::vector<PatternScores> predict_batch(
        const std::vector<PatternFeatures>& values) {
        std::vector<PatternScores> result(values.size());
        if (values.empty()) {
            return result;
        }
        auto maximum = maximum_request_states_.load(std::memory_order_relaxed);
        while (maximum < values.size() &&
               !maximum_request_states_.compare_exchange_weak(
                   maximum, values.size(), std::memory_order_relaxed)) {
        }
        std::lock_guard<std::mutex> lock(inference_mutex_);
        for (std::size_t begin = 0; begin < values.size(); begin += kPatternBatchStates) {
            const std::size_t count =
                std::min<std::size_t>(kPatternBatchStates, values.size() - begin);
            std::fill_n(features_.data<float>(),
                        static_cast<std::size_t>(batch_rows_) * kPatternFeatureDim, 0.0f);
            for (std::size_t index = 0; index < count; ++index) {
                std::copy_n(
                    values[begin + index].begin(), target_keys_ * kPatternFeatureDim,
                    features_.data<float>() + index * target_keys_ * kPatternFeatureDim);
            }
            request_.infer();
            const auto logits = request_.get_tensor("logits");
            if (logits.get_size() <
                static_cast<std::size_t>(batch_rows_) * kCandidateTypes) {
                throw std::runtime_error("pattern MLP returned too few candidate logits");
            }
            for (std::size_t index = 0; index < count; ++index) {
                std::copy_n(logits.data<const float>() +
                                index * target_keys_ * kCandidateTypes,
                            target_keys_ * kCandidateTypes,
                            result[begin + index].begin());
            }
            inference_count_.fetch_add(1, std::memory_order_relaxed);
            evaluated_state_count_.fetch_add(count, std::memory_order_relaxed);
        }
        return result;
    }

    [[nodiscard]] std::string evidence() const {
        std::string devices;
        for (const auto& device : execution_devices_) {
            if (!devices.empty()) {
                devices += ',';
            }
            devices += device;
        }
        return std::to_string(target_keys_) +
               "K generalized pattern MLP evaluator=OpenVINO " + device_ +
               " EXECUTION_DEVICES=" + devices +
               " query=" + std::to_string(queried_op_count_) + "/" +
               std::to_string(op_count_) + " inferences=" +
               std::to_string(inference_count_.load(std::memory_order_relaxed)) +
               " evaluated-states=" +
               std::to_string(evaluated_state_count_.load(std::memory_order_relaxed)) +
               " batch-capacity=" + std::to_string(kPatternBatchStates) +
               " schema=v3 features=" + std::to_string(kPatternFeatureDim) +
               " roles=" + std::to_string(kPatternOutputRoles) +
               " residual-bound=" + std::to_string(kPatternMaxResidual) +
               " max-requested-states=" +
               std::to_string(maximum_request_states_.load(std::memory_order_relaxed));
    }

private:
    int target_keys_ = 0;
    std::string device_;
    ov::Core core_;
    std::shared_ptr<ov::Model> model_;
    ov::CompiledModel compiled_;
    std::size_t op_count_ = 0;
    std::size_t queried_op_count_ = 0;
    std::vector<std::string> execution_devices_;
    int batch_rows_ = 0;
    ov::Tensor features_;
    ov::InferRequest request_;
    std::mutex inference_mutex_;
    std::atomic<std::uint64_t> inference_count_{0};
    std::atomic<std::uint64_t> evaluated_state_count_{0};
    std::atomic<std::size_t> maximum_request_states_{0};
};

PatternMlpEvaluator& shared_pattern_evaluator(const std::filesystem::path& path,
                                              int target_keys) {
    static std::mutex evaluator_mutex;
    static std::map<std::string, std::unique_ptr<PatternMlpEvaluator>> evaluators;
    const std::string key = path.u8string() + '\n' + std::to_string(target_keys);
    std::lock_guard<std::mutex> lock(evaluator_mutex);
    auto& evaluator = evaluators[key];
    if (!evaluator) {
        evaluator = std::make_unique<PatternMlpEvaluator>(path, target_keys);
    }
    return *evaluator;
}
#endif

std::size_t candidate_offset(int slice, int lane, int type) {
    return (static_cast<std::size_t>(slice) * kTargetLanes + lane) * kCandidateTypes + type;
}

std::vector<Candidate> ranked_candidates(
    const BlockOutput& output, int slice, int type_begin, int type_end,
    const std::array<int64_t, kTargetLanes * kCandidateTypes>* pattern_scores = nullptr) {
    std::vector<Candidate> ranked;
    for (int lane = 0; lane < kTargetLanes; ++lane) {
        for (int type = type_begin; type < type_end; ++type) {
            const std::size_t offset = candidate_offset(slice, lane, type);
            if (output.valid[offset] != 0) {
                int64_t score = output.scores[offset];
                if (pattern_scores) {
                    score += (*pattern_scores)[lane * kCandidateTypes + type];
                }
                ranked.push_back({
                    static_cast<int32_t>(std::clamp<int64_t>(
                        score, std::numeric_limits<int32_t>::min(),
                        std::numeric_limits<int32_t>::max())),
                    lane * kCandidateTypes + type, lane});
            }
        }
    }
    std::sort(ranked.begin(), ranked.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.id < rhs.id;
    });
    return ranked;
}

std::vector<Candidate> best_per_lane(const std::vector<Candidate>& ranked) {
    std::array<bool, kTargetLanes> seen{};
    std::vector<Candidate> result;
    for (const auto& candidate : ranked) {
        if (!seen[candidate.lane]) {
            seen[candidate.lane] = true;
            result.push_back(candidate);
        }
    }
    return result;
}

double lane_coverage_need(const std::array<int, kTargetLanes>& lane_use, int total,
                          int target_keys, int lane) {
    const double desired = 1.0 / std::max(1, target_keys);
    const double virtual_total = std::max(4.0, target_keys * 0.5);
    const double actual = lane_use[lane] + desired * virtual_total;
    const double expected = desired * (total + virtual_total + 1.0);
    const double ratio = expected > 0.0 ? actual / expected : 1.0;
    if (ratio <= 1.0) {
        return std::min(1.0, (1.0 - ratio) * 1.8);
    }
    return -std::min(1.0, (ratio - 1.0) * 0.9);
}

double pattern_gap_feature(double seconds) {
    if (!std::isfinite(seconds) || seconds >= 1.0e20) {
        return 1.0;
    }
    seconds = std::max(0.0, seconds);
    return std::min(1.0, std::log1p(seconds * 8.0) / std::log1p(16.0));
}

PatternFeatures build_pattern_features(
    const BeamState& state, const std::vector<std::pair<double, int>>& recent_events,
    int source_lane, int source_keys, double current_time, int target_keys,
    double source_chord_size_ratio, double local_density, double phase, bool has_hold,
    double duration) {
    std::vector<std::pair<double, int>> active_recent;
    std::array<float, kTargetLanes> recent_counts{};
    for (const auto& event : recent_events) {
        if (event.first < current_time - kDistributionWindowSeconds ||
            event.first >= current_time - 1.0e-7 || event.second < 0 ||
            event.second >= target_keys) {
            continue;
        }
        active_recent.push_back(event);
        recent_counts[event.second] += 1.0f;
    }

    std::vector<int> previous_lanes;
    if (!active_recent.empty()) {
        const auto previous = std::max_element(
            active_recent.begin(), active_recent.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
        for (const auto& event : active_recent) {
            if (std::abs(event.first - previous->first) <= 1.0e-7) {
                previous_lanes.push_back(event.second);
            }
        }
    }

    const bool has_previous = !previous_lanes.empty();
    double previous_center = (target_keys - 1) * 0.5;
    double previous_spread = 0.0;
    double previous_size = 0.0;
    if (has_previous) {
        previous_center = std::accumulate(previous_lanes.begin(), previous_lanes.end(), 0.0) /
                          previous_lanes.size();
        const auto [minimum, maximum] =
            std::minmax_element(previous_lanes.begin(), previous_lanes.end());
        previous_spread = static_cast<double>(*maximum - *minimum) /
                          std::max(1, target_keys - 1);
        std::set<int> unique(previous_lanes.begin(), previous_lanes.end());
        previous_size = std::min(1.0, static_cast<double>(unique.size()) / target_keys);
    }

    const int total_use = std::accumulate(state.lane_use.begin(),
                                          state.lane_use.begin() + target_keys, 0);
    const double recent_total = std::accumulate(recent_counts.begin(),
                                                recent_counts.begin() + target_keys, 0.0);
    const double lane_scale = std::max(1, target_keys - 1);
    const double source_position = source_keys > 1
                                       ? static_cast<double>(source_lane) /
                                             static_cast<double>(source_keys - 1)
                                       : 0.5;
    const int mapped_lane = std::clamp(
        static_cast<int>(std::nearbyint(source_position * (target_keys - 1))),
        0, target_keys - 1);
    const double source_to_target =
        std::min(2.0, source_keys / static_cast<double>(target_keys)) - 1.0;
    const double target_to_source =
        std::min(2.0, target_keys / static_cast<double>(source_keys)) - 1.0;
    const double key_delta =
        std::clamp((target_keys - source_keys) / 16.0, -1.0, 1.0);
    PatternFeatures rows{};
    for (int lane = 0; lane < target_keys; ++lane) {
        const double lane_position = lane / lane_scale;
        const double global_ratio =
            (state.lane_use[lane] + 0.5) /
            (total_use + 0.5 * target_keys) * target_keys;
        const double recent_ratio =
            (recent_counts[lane] + 0.25) /
            (recent_total + 0.25 * target_keys) * target_keys;
        const double left_gap = lane > 0
                                    ? pattern_gap_feature(
                                          current_time - state.last_time[lane - 1])
                                    : 1.0;
        const double right_gap = lane + 1 < target_keys
                                     ? pattern_gap_feature(
                                           current_time - state.last_time[lane + 1])
                                     : 1.0;
        const double delta_center = (lane - previous_center) / lane_scale;
        const double split = target_keys / 2.0;
        const double same_hand = has_previous
                                     ? static_cast<double>((lane < split) ==
                                                           (previous_center < split))
                                     : 0.0;
        const std::array<float, kPatternFeatureDim> row = {
            static_cast<float>(lane_position * 2.0 - 1.0),
            static_cast<float>(std::min(3.0, global_ratio)),
            static_cast<float>(std::min(3.0, recent_ratio)),
            state.lane_use[lane] == 0 ? 1.0f : 0.0f,
            static_cast<float>(pattern_gap_feature(current_time - state.last_time[lane])),
            static_cast<float>(left_gap),
            static_cast<float>(right_gap),
            static_cast<float>(delta_center),
            static_cast<float>(std::abs(delta_center)),
            static_cast<float>(previous_spread),
            static_cast<float>(previous_size),
            static_cast<float>(std::clamp(source_chord_size_ratio, 0.0, 1.0)),
            static_cast<float>(std::clamp(local_density, 0.0, 1.0)),
            static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * phase)),
            static_cast<float>(std::cos(2.0 * 3.14159265358979323846 * phase)),
            has_hold ? 1.0f : 0.0f,
            static_cast<float>(std::clamp(duration / 2.0, 0.0, 1.0)),
            lane == 0 || lane == target_keys - 1 ? 1.0f : 0.0f,
            static_cast<float>(same_hand),
            has_previous ? 1.0f : 0.0f,
            static_cast<float>(source_position * 2.0 - 1.0),
            static_cast<float>(mapped_lane / lane_scale * 2.0 - 1.0),
            static_cast<float>((lane - mapped_lane) / lane_scale),
            static_cast<float>(std::abs((lane - mapped_lane) / lane_scale)),
            static_cast<float>(source_to_target),
            static_cast<float>(target_to_source),
            lane == mapped_lane ? 1.0f : 0.0f,
            static_cast<float>(key_delta),
        };
        std::copy(row.begin(), row.end(),
                  rows.begin() + static_cast<std::ptrdiff_t>(lane * kPatternFeatureDim));
    }
    return rows;
}

BeamState empty_state() {
    BeamState state;
    state.last_time.fill(kMinusInfinityTime);
    state.busy_until.fill(kMinusInfinityTime);
    state.last_source.fill(-1);
    state.lane_use.fill(0);
    return state;
}

bool signature_less(const BeamState& lhs, const BeamState& rhs) {
    if (lhs.score != rhs.score) {
        return lhs.score > rhs.score;
    }
    return std::lexicographical_compare(lhs.signature.begin(), lhs.signature.end(),
                                        rhs.signature.begin(), rhs.signature.end());
}

BeamState solve_candidate_field(const BlockData& block, const BlockOutput& output,
                                const std::array<int64_t, kSlices>& addition_count,
                                int source_keys, int target_keys, int beam_width,
                                const std::optional<BeamState>& initial_state,
                                bool allow_source_drops,
                                const std::array<int, kSlices>* forced_base_lanes,
                                PatternMlpEvaluator* pattern_mlp) {
    BeamState start = initial_state.value_or(empty_state());
    start.score = 0;
    start.signature.clear();
    start.selections.clear();
    std::vector<BeamState> beam{std::move(start)};

    for (int slice = 0; slice < kSlices; ++slice) {
        if (block.mask[slice] == 0) {
            continue;
        }
        const double time = block.solver_notes[slice * kNoteFeatures];
        const double duration =
            std::max(0.0f, block.solver_notes[slice * kNoteFeatures + 3]);
        const int source_lane =
            static_cast<int>(std::llround(block.solver_notes[slice * kNoteFeatures + 1]));
        std::vector<BeamState> expanded;

        struct PreparedState {
            std::vector<std::pair<double, int>> recent;
            std::array<int, kTargetLanes> recent_use{};
            std::array<int64_t, kTargetLanes * kCandidateTypes> pattern_scores{};
        };
        std::vector<PreparedState> prepared(beam.size());
#if defined(TENRIFF_ENABLE_NK3_ONNX)
        std::vector<PatternFeatures> pattern_features;
        if (pattern_mlp) {
            pattern_features.reserve(beam.size());
        }
#endif

        for (std::size_t state_index = 0; state_index < beam.size(); ++state_index) {
            const auto& state = beam[state_index];
            auto& state_data = prepared[state_index];
            for (const auto& event : state.recent_events) {
                if (event.first >= time - kDistributionWindowSeconds) {
                    state_data.recent.push_back(event);
                    ++state_data.recent_use[event.second];
                }
            }
#if defined(TENRIFF_ENABLE_NK3_ONNX)
            if (pattern_mlp) {
                pattern_features.push_back(build_pattern_features(
                    state, state_data.recent, source_lane, source_keys, time, target_keys,
                    block.solver_notes[slice * kNoteFeatures + 8],
                    block.solver_notes[slice * kNoteFeatures + 7],
                    block.solver_notes[slice * kNoteFeatures + 5],
                    block.solver_notes[slice * kNoteFeatures + 4] > 0.5f,
                    duration));
            }
#endif
        }

#if defined(TENRIFF_ENABLE_NK3_ONNX)
        if (pattern_mlp) {
            const auto pattern_logits = pattern_mlp->predict_batch(pattern_features);
            for (std::size_t state_index = 0; state_index < beam.size(); ++state_index) {
                for (int lane = 0; lane < target_keys; ++lane) {
                    for (int type = 0; type < kCandidateTypes; ++type) {
                        const std::size_t offset = lane * kCandidateTypes + type;
                        const double clipped = std::clamp(
                            static_cast<double>(pattern_logits[state_index][offset]),
                            -kPatternMaxResidual, kPatternMaxResidual);
                        prepared[state_index].pattern_scores[offset] =
                            static_cast<int64_t>(std::nearbyint(
                                clipped * kPatternWeight * kPressureScale));
                    }
                }
            }
        }
#else
        (void)pattern_mlp;
#endif

        for (std::size_t state_index = 0; state_index < beam.size(); ++state_index) {
            const auto& state = beam[state_index];
            const auto& recent = prepared[state_index].recent;
            const auto& recent_use = prepared[state_index].recent_use;
            const int recent_total = static_cast<int>(recent.size());
            const auto& pattern_scores = prepared[state_index].pattern_scores;
            auto base_ranked = best_per_lane(
                ranked_candidates(output, slice, 0, 2, &pattern_scores));
            if (forced_base_lanes && (*forced_base_lanes)[slice] >= 0) {
                const int forced = (*forced_base_lanes)[slice];
                base_ranked.erase(
                    std::remove_if(base_ranked.begin(), base_ranked.end(),
                                   [&](const Candidate& candidate) {
                                       return candidate.lane != forced;
                                   }),
                    base_ranked.end());
            }
            const auto add_ranked = best_per_lane(
                ranked_candidates(output, slice, 2, kCandidateTypes, &pattern_scores));
            for (const auto& base : base_ranked) {
                const bool same_source = state.last_source[base.lane] == source_lane;
                const double inherited_hold_end =
                    state.busy_until[base.lane] - kMinimumGapSeconds;
                if (state.busy_until[base.lane] >= time &&
                    !(same_source && inherited_hold_end < time)) {
                    continue;
                }
                if (time - state.last_time[base.lane] < kMinimumGapSeconds &&
                    !same_source) {
                    continue;
                }
                if (state.last_source[base.lane] >= 0 && !same_source &&
                    time - state.last_time[base.lane] <=
                        kCreatedJackWindowMs / 1000.0 +
                            kTimeComparisonEpsilonSeconds) {
                    continue;
                }
                if (base.id % kCandidateTypes == 0 && state.last_source[base.lane] >= 0 &&
                    !same_source &&
                    time - state.last_time[base.lane] <=
                        kCreatedKeepJackWindowSeconds +
                            kTimeComparisonEpsilonSeconds) {
                    continue;
                }

                std::vector<Candidate> available;
                for (const auto& candidate : add_ranked) {
                    if (candidate.lane == base.lane ||
                        state.busy_until[candidate.lane] >= time ||
                        time - state.last_time[candidate.lane] < kMinimumGapSeconds) {
                        continue;
                    }
                    if (state.last_source[candidate.lane] >= 0 &&
                        state.last_source[candidate.lane] != source_lane &&
                        time - state.last_time[candidate.lane] <=
                            kCreatedKeepJackWindowSeconds +
                                kTimeComparisonEpsilonSeconds) {
                        continue;
                    }
                    available.push_back(candidate);
                    if (available.size() == 12) {
                        break;
                    }
                }
                const int wanted = std::min({std::max<int64_t>(0, addition_count[slice]),
                                             static_cast<int64_t>(available.size()), int64_t{3}});
                std::vector<Candidate> chosen;
                std::function<void(std::size_t, int)> emit = [&](std::size_t cursor,
                                                                  int remaining) {
                    if (remaining > 0) {
                        for (std::size_t i = cursor;
                             i + static_cast<std::size_t>(remaining) <= available.size(); ++i) {
                            chosen.push_back(available[i]);
                            emit(i + 1, remaining - 1);
                            chosen.pop_back();
                        }
                        return;
                    }
                    BeamState next = state;
                    next.last_time[base.lane] = time;
                    next.last_source[base.lane] = source_lane;
                    if (duration > 0.0) {
                        next.busy_until[base.lane] =
                            std::max(next.busy_until[base.lane],
                                     time + duration + kMinimumGapSeconds);
                    }
                    std::vector<Candidate> choices{base};
                    choices.insert(choices.end(), chosen.begin(), chosen.end());
                    for (const auto& add : chosen) {
                        next.last_time[add.lane] = time;
                        next.last_source[add.lane] = source_lane;
                    }
                    auto choice_recent_use = recent_use;
                    int choice_recent_total = recent_total;
                    next.recent_events = recent;
                    int64_t adjusted = 0;
                    for (const auto& choice : choices) {
                        const int type = choice.id % kCandidateTypes;
                        const double global_need = lane_coverage_need(
                            state.lane_use, state.total_selections, target_keys, choice.lane);
                        const double local_need = lane_coverage_need(
                            choice_recent_use, choice_recent_total, target_keys, choice.lane);
                        const bool base_type = type < 2;
                        const double spread = (base_type ? 1.25 : 1.80) * global_need +
                                              (base_type ? 0.75 : 1.00) * local_need +
                                              (state.lane_use[choice.lane] == 0
                                                   ? (base_type ? 0.45 : 0.70)
                                                   : 0.0);
                        adjusted += choice.score +
                                    static_cast<int64_t>(std::llround(kPressureScale * spread));
                        ++next.lane_use[choice.lane];
                        ++choice_recent_use[choice.lane];
                        ++choice_recent_total;
                        next.recent_events.emplace_back(time, choice.lane);
                        next.signature.push_back(choice.id);
                        next.selections.push_back(
                            {slice, choice.lane, type, choice.score});
                    }
                    next.score = state.score + adjusted;
                    next.total_selections = state.total_selections +
                                            static_cast<int>(choices.size());
                    expanded.push_back(std::move(next));
                };
                emit(0, wanted);
            }
        }

        if (expanded.empty()) {
            if (!allow_source_drops) {
                throw std::runtime_error("no valid hybrid solver transition at slice " +
                                         std::to_string(slice));
            }
            const int skip_id = kTargetLanes * kCandidateTypes;
            for (auto& state : beam) {
                state.signature.push_back(skip_id);
                state.recent_events.erase(
                    std::remove_if(state.recent_events.begin(), state.recent_events.end(),
                                   [&](const auto& event) {
                                       return event.first < time - kDistributionWindowSeconds;
                                   }),
                    state.recent_events.end());
            }
            continue;
        }
        std::sort(expanded.begin(), expanded.end(), signature_less);
        if (expanded.size() > static_cast<std::size_t>(beam_width)) {
            expanded.resize(static_cast<std::size_t>(beam_width));
        }
        beam = std::move(expanded);
    }
    return beam.front();
}

std::pair<BeamState, bool> solve_with_retry(
    const BlockData& block, const BlockOutput& output, int source_keys, int target_keys,
    const std::optional<BeamState>& initial_state, bool allow_source_drops,
    const std::array<int, kSlices>* forced_base_lanes,
    PatternMlpEvaluator* pattern_mlp) {
    try {
        return {solve_candidate_field(block, output, output.addition_count,
                                      source_keys, target_keys, 32,
                                      initial_state, false, forced_base_lanes, pattern_mlp),
                false};
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find("no valid hybrid solver transition") ==
            std::string::npos) {
            throw;
        }
    }
    std::array<int64_t, kSlices> no_additions{};
    try {
        return {solve_candidate_field(block, output, no_additions,
                                      source_keys, target_keys, 256,
                                      initial_state, false, forced_base_lanes, pattern_mlp),
                true};
    } catch (const std::runtime_error& error) {
        if (!allow_source_drops ||
            std::string(error.what()).find("no valid hybrid solver transition") ==
                std::string::npos) {
            throw;
        }
    }
    return {solve_candidate_field(block, output, no_additions,
                                  source_keys, target_keys, 256,
                                  initial_state, true, forced_base_lanes, pattern_mlp),
            true};
}

std::array<int, kSlices> direct_base_lanes(const std::vector<SourceNote>& notes,
                                           std::size_t begin, int source_keys,
                                           int target_keys) {
    std::array<int, kSlices> lanes{};
    lanes.fill(-1);
    const std::size_t count = std::min<std::size_t>(kSlices, notes.size() - begin);
    for (std::size_t slice = 0; slice < count; ++slice) {
        lanes[slice] = direct_lane(notes[begin + slice].source_lane, source_keys, target_keys);
    }
    return lanes;
}

BeamState direct_source_state(const std::vector<SourceNote>& notes, std::size_t count,
                              int source_keys, int target_keys) {
    BeamState state = empty_state();
    count = std::min(count, notes.size());
    for (std::size_t index = 0; index < count; ++index) {
        const auto& note = notes[index];
        const int lane = direct_lane(note.source_lane, source_keys, target_keys);
        const double time = note.time_ms / 1000.0;
        const double duration = (note.end_ms - note.time_ms) / 1000.0;
        state.last_time[lane] = time;
        state.last_source[lane] = note.source_lane;
        if (duration > 0.0) {
            state.busy_until[lane] =
                std::max(state.busy_until[lane], time + duration + kMinimumGapSeconds);
        }
        ++state.lane_use[lane];
        ++state.total_selections;
        state.recent_events.emplace_back(time, lane);
    }
    if (count > 0) {
        const double cutoff = notes[count - 1].time_ms / 1000.0 - kDistributionWindowSeconds;
        state.recent_events.erase(
            std::remove_if(state.recent_events.begin(), state.recent_events.end(),
                           [&](const auto& event) { return event.first < cutoff; }),
            state.recent_events.end());
    }
    return state;
}

std::vector<PlacedNote> direct_source_objects(const std::vector<SourceNote>& notes,
                                              std::size_t count, int source_keys,
                                              int target_keys) {
    std::vector<PlacedNote> placed;
    count = std::min(count, notes.size());
    placed.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto& note = notes[index];
        placed.push_back({note.time_ms, note.end_ms, note.source_lane, note.source_index,
                          direct_lane(note.source_lane, source_keys, target_keys),
                          Origin::Original, 0, 0});
    }
    return placed;
}

std::unordered_map<std::size_t, int64_t>
generated_ln_ends(const std::vector<SourceNote>& notes) {
    std::unordered_map<std::size_t, int64_t> result;
    std::vector<int64_t> times;
    times.reserve(notes.size());
    for (const auto& note : notes) {
        times.push_back(note.time_ms);
    }
    for (const auto& note : notes) {
        if (note.hold) {
            if (note.end_ms - note.time_ms >= kGeneratedLnMinimumDurationMs) {
                result[note.source_index] = note.end_ms;
            }
            continue;
        }
        const auto left = std::lower_bound(times.begin(), times.end(),
                                           note.time_ms - kGeneratedLnContextMs);
        const auto right = std::upper_bound(times.begin(), times.end(),
                                            note.time_ms + kGeneratedLnContextMs);
        const std::size_t first = static_cast<std::size_t>(left - times.begin());
        const std::size_t last = static_cast<std::size_t>(right - times.begin());
        std::vector<const SourceNote*> nearby;
        for (std::size_t index = first; index < last; ++index) {
            if (notes[index].hold &&
                notes[index].end_ms - notes[index].time_ms >= kGeneratedLnMinimumDurationMs) {
                nearby.push_back(&notes[index]);
            }
        }
        if (nearby.size() < kGeneratedLnMinimumCount ||
            static_cast<double>(nearby.size()) / std::max<std::size_t>(1, last - first) <
                kGeneratedLnMinimumRatio) {
            continue;
        }
        const auto best = *std::min_element(
            nearby.begin(), nearby.end(), [&](const SourceNote* lhs, const SourceNote* rhs) {
                return std::tuple{std::llabs(lhs->time_ms - note.time_ms),
                                  std::abs(lhs->source_lane - note.source_lane), lhs->source_index} <
                       std::tuple{std::llabs(rhs->time_ms - note.time_ms),
                                  std::abs(rhs->source_lane - note.source_lane), rhs->source_index};
            });
        result[note.source_index] = note.time_ms + best->end_ms - best->time_ms;
    }
    return result;
}

std::tuple<std::vector<PlacedNote>, int, int>
repair_created_notes(const std::vector<PlacedNote>& objects, int target_keys) {
    std::vector<PlacedNote> accepted;
    std::vector<PlacedNote> created;
    std::array<int, kTargetLanes> lane_use{};
    for (const auto& note : objects) {
        if (note.origin == Origin::Created) {
            created.push_back(note);
        } else {
            accepted.push_back(note);
            ++lane_use[note.output_lane];
        }
    }
    std::sort(created.begin(), created.end(), [](const PlacedNote& lhs, const PlacedNote& rhs) {
        return std::tuple{lhs.time_ms, lhs.source_index, lhs.candidate_type, lhs.output_lane} <
               std::tuple{rhs.time_ms, rhs.source_index, rhs.candidate_type, rhs.output_lane};
    });
    const int split = target_keys / 2;
    int relocated = 0;
    int dropped = 0;
    const auto safe = [&](const PlacedNote& note, int lane) {
        for (const auto& other : accepted) {
            if (other.output_lane != lane) {
                continue;
            }
            if (other.time_ms == note.time_ms) {
                return false;
            }
            const PlacedNote& earlier = other.time_ms < note.time_ms ? other : note;
            const PlacedNote& later = other.time_ms < note.time_ms ? note : other;
            if (later.time_ms - earlier.end_ms < 30) {
                return false;
            }
            if (std::llabs(other.time_ms - note.time_ms) <= kCreatedJackWindowMs &&
                other.source_lane != note.source_lane) {
                return false;
            }
        }
        return true;
    };
    const auto preserves_cross_source = [&](const PlacedNote& note, int lane) {
        for (const auto& other : accepted) {
            if (other.output_lane != lane || other.source_lane == note.source_lane ||
                std::llabs(other.time_ms - note.time_ms) > 500) {
                continue;
            }
            const PlacedNote& later = note.time_ms >= other.time_ms ? note : other;
            if (later.origin != Origin::Shifted) {
                return false;
            }
        }
        return true;
    };
    for (auto note : created) {
        const int hand_begin = note.output_lane < split ? 0 : split;
        const int hand_end = note.output_lane < split ? split : target_keys;
        int chosen = -1;
        if (safe(note, note.output_lane) &&
            preserves_cross_source(note, note.output_lane)) {
            chosen = note.output_lane;
        } else {
            for (int lane = hand_begin; lane < hand_end; ++lane) {
                if (lane == note.output_lane || !safe(note, lane) ||
                    !preserves_cross_source(note, lane)) {
                    continue;
                }
                if (chosen < 0 ||
                    std::tuple{lane_use[lane], std::abs(lane - note.output_lane), lane} <
                        std::tuple{lane_use[chosen], std::abs(chosen - note.output_lane), chosen}) {
                    chosen = lane;
                }
            }
            if (chosen >= 0) {
                ++relocated;
            }
        }
        if (chosen < 0) {
            ++dropped;
            continue;
        }
        note.output_lane = chosen;
        ++lane_use[chosen];
        accepted.push_back(std::move(note));
    }
    std::stable_sort(accepted.begin(), accepted.end(), [](const PlacedNote& lhs,
                                                          const PlacedNote& rhs) {
        return std::tuple{lhs.time_ms, lhs.source_index,
                          lhs.origin == Origin::Created ? 1 : 0, lhs.output_lane} <
               std::tuple{rhs.time_ms, rhs.source_index,
                          rhs.origin == Origin::Created ? 1 : 0, rhs.output_lane};
    });
    return {std::move(accepted), relocated, dropped};
}

QualityReport inspect_quality(const std::vector<PlacedNote>& objects, int target_keys,
                              const GameplayChart& source_chart) {
    QualityReport report;
    std::array<std::vector<PlacedNote>, kTargetLanes> by_lane;
    std::map<int64_t, int> by_time;
    const auto start_sample = [&](const PlacedNote& note) {
        return source_chart.notes.at(note.source_index).start_sample;
    };
    for (const auto& note : objects) {
        if (note.output_lane >= 0 && note.output_lane < target_keys) {
            by_lane[note.output_lane].push_back(note);
        }
        ++by_time[start_sample(note)];
    }
    for (const auto& [time, count] : by_time) {
        (void)time;
        report.impossible_chords += count > target_keys ? 1 : 0;
    }
    for (int lane = 0; lane < target_keys; ++lane) {
        auto& notes = by_lane[lane];
        std::sort(notes.begin(), notes.end(), [&](const PlacedNote& lhs, const PlacedNote& rhs) {
            return std::tuple{start_sample(lhs), lhs.source_index,
                              static_cast<int>(lhs.origin)} <
                   std::tuple{start_sample(rhs), rhs.source_index,
                              static_cast<int>(rhs.origin)};
        });
        for (std::size_t index = 1; index < notes.size(); ++index) {
            const auto& earlier = notes[index - 1];
            const auto& later = notes[index];
            report.same_time_collisions +=
                start_sample(earlier) == start_sample(later) ? 1 : 0;
            report.long_note_conflicts +=
                earlier.hold() && later.time_ms <= earlier.end_ms ? 1 : 0;
            if (later.time_ms - earlier.end_ms < 30 &&
                (earlier.source_lane != later.source_lane ||
                 earlier.origin == Origin::Created || later.origin == Origin::Created)) {
                ++report.minimum_gap_violations;
            }
            if (later.time_ms - earlier.time_ms <= 500 &&
                earlier.source_lane != later.source_lane && later.origin != Origin::Shifted) {
                ++report.created_jacks;
            }
            if (later.time_ms > earlier.time_ms &&
                later.time_ms - earlier.time_ms <= kCreatedJackWindowMs &&
                earlier.source_lane != later.source_lane) {
                ++report.novel_jacks;
                if (earlier.origin == Origin::Created || later.origin == Origin::Created) {
                    ++report.created_addition_jacks;
                }
            }
        }
    }
    return report;
}

}  // namespace

KeyModeConverterResult
convert_key_mode_chart_nk3_onnx(const GameplayChart& chart,
                                const KeyModeConverterOptions& options) {
    KeyModeConverterResult result;
    result.chart = chart;
#if !defined(TENRIFF_ENABLE_NK3_ONNX)
    result.warnings.push_back("NK3 ONNX is unavailable in this TenRiff build.");
    return result;
#else
    const int source_keys = resolve_lane_count(chart);
    if (source_keys <= 0 || chart.notes.empty() || options.target_lane_count <= 0 ||
        source_keys > kTargetLanes || options.target_lane_count > kTargetLanes) {
        result.warnings.push_back("NK3 ONNX supports non-empty charts from 1K through 18K.");
        return result;
    }
    const int sample_rate = resolve_sample_rate(options.sample_rate);
    const double base_bpm = resolve_base_bpm(options.base_bpm);
    const auto notes = source_notes(chart, source_keys, sample_rate);
    if (notes.empty()) {
        result.warnings.push_back("NK3 ONNX found no playable source notes.");
        return result;
    }

    try {
        const auto path = p64_model_path();
        if (!std::filesystem::is_regular_file(path)) {
            throw std::runtime_error("model not found: " + path.u8string());
        }
        OpenVinoEvaluator& evaluator = shared_evaluator(path, selected_device());
        PatternMlpEvaluator* pattern_mlp = nullptr;
        if (options.target_lane_count >= 2) {
            const auto pattern_path = pattern_model_path(options.target_lane_count);
            if (!std::filesystem::is_regular_file(pattern_path)) {
                throw std::runtime_error("generalized pattern MLP model not found: " +
                                         pattern_path.u8string());
            }
            pattern_mlp = &shared_pattern_evaluator(pattern_path,
                                                    options.target_lane_count);
        }
        ModelState model_state;
        std::optional<BeamState> solver_state;
        std::vector<PlacedNote> placed;
        const auto ln_ends = generated_ln_ends(notes);
        const bool compression = options.target_lane_count < source_keys;
        bool stable_fallback = false;
        int blocks = 0;
        int safety_retries = 0;
        int fallback_blocks = 0;
        int dropped_sources = 0;
        int64_t requested_additions = 0;

        for (std::size_t begin = 0; begin < notes.size(); begin += kSlices) {
            const BlockData block = build_block(notes, begin, source_keys,
                                                options.target_lane_count, base_bpm,
                                                model_state);
            const BlockOutput output = evaluator.run(block, model_state);
            std::optional<std::array<int, kSlices>> forced;
            if (stable_fallback) {
                forced = direct_base_lanes(notes, begin, source_keys,
                                           options.target_lane_count);
            }
            BeamState solved;
            bool retried = false;
            try {
                std::tie(solved, retried) = solve_with_retry(
                    block, output, source_keys, options.target_lane_count, solver_state,
                    compression, forced ? &*forced : nullptr, pattern_mlp);
            } catch (const std::runtime_error& error) {
                if (compression || stable_fallback ||
                    std::string(error.what()).find("no valid hybrid solver transition") ==
                        std::string::npos) {
                    throw;
                }
                stable_fallback = true;
                placed = direct_source_objects(notes, begin, source_keys,
                                               options.target_lane_count);
                solver_state = direct_source_state(notes, begin, source_keys,
                                                   options.target_lane_count);
                forced = direct_base_lanes(notes, begin, source_keys,
                                           options.target_lane_count);
                std::tie(solved, retried) = solve_with_retry(
                    block, output, source_keys, options.target_lane_count, solver_state,
                    false, &*forced, pattern_mlp);
            }
            safety_retries += retried ? 1 : 0;
            if (stable_fallback) {
                ++fallback_blocks;
                solver_state = direct_source_state(
                    notes, std::min(notes.size(), begin + kSlices), source_keys,
                    options.target_lane_count);
            } else {
                solver_state = solved;
            }
            model_state = output.next;
            requested_additions += std::accumulate(output.addition_count.begin(),
                                                   output.addition_count.end(), int64_t{0});

            std::array<std::vector<Selection>, kSlices> by_slice;
            for (const auto& selection : solved.selections) {
                by_slice[selection.slice].push_back(selection);
            }
            const std::size_t count = std::min<std::size_t>(kSlices, notes.size() - begin);
            for (std::size_t slice = 0; slice < count; ++slice) {
                const auto& source = notes[begin + slice];
                const auto base = std::find_if(by_slice[slice].begin(), by_slice[slice].end(),
                                               [](const Selection& selection) {
                                                   return selection.type < 2;
                                               });
                if (base == by_slice[slice].end()) {
                    if (compression) {
                        ++dropped_sources;
                        continue;
                    }
                    throw std::runtime_error("NK3 ONNX lost a required source assignment");
                }
                const int direct = direct_lane(source.source_lane, source_keys,
                                               options.target_lane_count);
                placed.push_back({source.time_ms, source.end_ms, source.source_lane,
                                  source.source_index, base->lane,
                                  base->type == 1 || base->lane != direct
                                      ? Origin::Shifted
                                      : Origin::Original,
                                  base->type, base->score});
                for (const auto& add : by_slice[slice]) {
                    if (add.type < 2) {
                        continue;
                    }
                    const auto generated_end = ln_ends.find(source.source_index);
                    const int64_t end =
                        generated_end != ln_ends.end() &&
                                generated_end->second - source.time_ms >=
                                    kGeneratedLnMinimumDurationMs
                            ? generated_end->second
                            : source.time_ms;
                    placed.push_back({source.time_ms, end, source.source_lane,
                                      source.source_index, add.lane, Origin::Created,
                                      add.type, add.score});
                }
            }
            ++blocks;
        }

        int relocated = 0;
        int dropped_additions = 0;
        std::tie(placed, relocated, dropped_additions) =
            repair_created_notes(placed, options.target_lane_count);
        const QualityReport quality = inspect_quality(placed, options.target_lane_count, chart);
        if (!quality.safe()) {
            throw std::runtime_error(
                "unsafe output (collisions=" + std::to_string(quality.same_time_collisions) +
                ", ln=" + std::to_string(quality.long_note_conflicts) +
                ", gap=" + std::to_string(quality.minimum_gap_violations) +
                ", created-jack=" + std::to_string(quality.created_jacks) +
                ", created-addition-jack=" +
                std::to_string(quality.created_addition_jacks) +
                ", novel-jack=" + std::to_string(quality.novel_jacks) +
                ", impossible-chord=" + std::to_string(quality.impossible_chords) + ")");
        }

        GameplayChart rebuilt = chart;
        rebuilt.lane_count = options.target_lane_count;
        rebuilt.scratch_lanes.clear();
        rebuilt.lane_group_size = 0;
        rebuilt.notes.clear();
        rebuilt.notes.reserve(placed.size());
        int additions = 0;
        int generated_lns = 0;
        for (const auto& note : placed) {
            NoteEvent mapped = chart.notes[note.source_index];
            mapped.lane = note.output_lane + 1;
            mapped.start_sample = chart.notes[note.source_index].start_sample;
            if (note.origin == Origin::Created) {
                ++additions;
                if (note.hold()) {
                    ++generated_lns;
                    mapped.end_sample = ms_to_samples(note.end_ms, sample_rate);
                } else {
                    mapped.end_sample.reset();
                    mapped.release_required = false;
                }
            } else if (!note.hold()) {
                mapped.end_sample.reset();
                mapped.release_required = false;
            }
            rebuilt.duration_samples =
                std::max(rebuilt.duration_samples,
                         mapped.end_sample.value_or(mapped.start_sample));
            rebuilt.notes.push_back(std::move(mapped));
        }
        std::stable_sort(rebuilt.notes.begin(), rebuilt.notes.end(),
                         [](const NoteEvent& lhs, const NoteEvent& rhs) {
                             if (lhs.start_sample != rhs.start_sample) {
                                 return lhs.start_sample < rhs.start_sample;
                             }
                             if (lhs.lane != rhs.lane) {
                                 return lhs.lane < rhs.lane;
                             }
                             return lhs.note_id < rhs.note_id;
                         });
        result.chart = std::move(rebuilt);
        result.converted = true;
        result.warnings.push_back("NK3 P64 hybrid ONNX + host beam32 " +
                                  evaluator.evidence() + " (strict, no device fallback).");
        if (pattern_mlp) {
            result.warnings.push_back(
                "NK3 target routing: " + pattern_mlp->evidence() +
                " (NPU preferred, GPU then CPU fallback).");
        } else {
            result.warnings.push_back(
                "NK3 target routing: generalized pattern MLP off for 1K; "
                "using P64 ONNX only.");
        }
        result.warnings.push_back(
            "NK3 remapped " + std::to_string(source_keys) + "K to " +
            std::to_string(options.target_lane_count) + "K (source=" +
            std::to_string(notes.size()) + ", output=" +
            std::to_string(result.chart.notes.size()) + ", additions=" +
            std::to_string(additions) + ", generated-ln=" +
            std::to_string(generated_lns) + ", moved-additions=" +
            std::to_string(relocated) + ", dropped-additions=" +
            std::to_string(dropped_additions) + ", source-dropped=" +
            std::to_string(dropped_sources) + ", safety-retry=" +
            std::to_string(safety_retries) + ", base-fallback=" +
            std::to_string(fallback_blocks) + ", blocks=" +
            std::to_string(blocks) + ", requested-additions=" +
            std::to_string(requested_additions) + ").");
        return result;
    } catch (const std::exception& error) {
        result.warnings.push_back(std::string("NK3 ONNX strict conversion failed: ") +
                                  error.what() + ".");
        return result;
    }
#endif
}

}  // namespace tenriff::gameplay
