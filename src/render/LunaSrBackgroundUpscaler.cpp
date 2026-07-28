#include "render/LunaSrBackgroundUpscaler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <deque>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#ifdef _MSC_VER
#include <ppl.h>
#endif

#include "util/Utf8Compat.h"
#endif

#if defined(TENRIFF_ENABLE_LUNASR) && defined(_WIN32)
#include <winrt/Windows.AI.MachineLearning.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/base.h>
#endif

namespace tenriff::render {

namespace {

constexpr std::uint32_t kModelInputWidth = 960;
constexpr std::uint32_t kModelInputHeight = 540;
constexpr std::size_t kCachedFrameLimit = 3;
constexpr wchar_t kModelFilename[] =
    L"lunasr_quality_rgb_staged32_intel_npu_x2_v1_e48_540p_rgb_residual_int8_qdq_winml_public.onnx";
constexpr int kBenchmarkWarmupFrames = 3;
constexpr int kBenchmarkTimedFrames = 12;

std::string lower_ascii(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

struct ProcessBenchmarkGate {
    std::mutex mutex;
    std::condition_variable changed;
    LunaSrBenchmarkState state = LunaSrBenchmarkState::Pending;
    double fps = 0.0;
    std::string detail;
};

ProcessBenchmarkGate& process_benchmark_gate() {
    static ProcessBenchmarkGate gate;
    return gate;
}

LunaSrBenchmarkStatus benchmark_status_snapshot() {
    auto& gate = process_benchmark_gate();
    std::lock_guard<std::mutex> lock(gate.mutex);
    return LunaSrBenchmarkStatus{gate.state, gate.fps, gate.detail};
}

#ifdef _WIN32
std::optional<std::filesystem::path> find_model_path() {
    namespace fs = std::filesystem;
    std::array<wchar_t, 32768> module_path{};
    const DWORD length = GetModuleFileNameW(nullptr,
                                            module_path.data(),
                                            static_cast<DWORD>(module_path.size()));
    std::vector<fs::path> roots;
    if (length > 0 && length < module_path.size()) {
        roots.push_back(fs::path(module_path.data()).parent_path());
    }
    std::error_code ec;
    const fs::path current = fs::current_path(ec);
    if (!ec && !current.empty()) {
        roots.push_back(current);
    }

    for (const auto& root : roots) {
        for (const auto& relative : {fs::path(L"tools") / L"lunasr" / kModelFilename,
                                     fs::path(L"lunasr") / kModelFilename}) {
            const fs::path candidate = root / relative;
            if (fs::is_regular_file(candidate, ec) && !ec) {
                return candidate;
            }
            ec.clear();
        }
    }
    return std::nullopt;
}

bool decode_cover_frame(IWICImagingFactory* factory,
                        std::string_view path,
                        std::vector<std::uint8_t>& out_bgra) {
    if (!factory || path.empty()) {
        return false;
    }

    const std::wstring wide_path = util::path_from_utf8_lossy(path).native();
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(wide_path.c_str(),
                                                   nullptr,
                                                   GENERIC_READ,
                                                   WICDecodeMetadataCacheOnLoad,
                                                   &decoder)) ||
        !decoder) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame)) || !frame) {
        return false;
    }

    UINT source_width = 0;
    UINT source_height = 0;
    if (FAILED(frame->GetSize(&source_width, &source_height)) ||
        source_width == 0 || source_height == 0) {
        return false;
    }

    const double source_aspect = static_cast<double>(source_width) /
                                 static_cast<double>(source_height);
    constexpr double kTargetAspect = static_cast<double>(kModelInputWidth) /
                                     static_cast<double>(kModelInputHeight);
    WICRect crop{0, 0, static_cast<INT>(source_width), static_cast<INT>(source_height)};
    if (source_aspect > kTargetAspect) {
        crop.Width = std::max(1, static_cast<INT>(std::llround(source_height * kTargetAspect)));
        crop.X = (static_cast<INT>(source_width) - crop.Width) / 2;
    } else if (source_aspect < kTargetAspect) {
        crop.Height = std::max(1, static_cast<INT>(std::llround(source_width / kTargetAspect)));
        crop.Y = (static_cast<INT>(source_height) - crop.Height) / 2;
    }

    Microsoft::WRL::ComPtr<IWICBitmapClipper> clipper;
    if (FAILED(factory->CreateBitmapClipper(&clipper)) || !clipper ||
        FAILED(clipper->Initialize(frame.Get(), &crop))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
    if (FAILED(factory->CreateBitmapScaler(&scaler)) || !scaler ||
        FAILED(scaler->Initialize(clipper.Get(),
                                  kModelInputWidth,
                                  kModelInputHeight,
                                  WICBitmapInterpolationModeFant))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter)) || !converter ||
        FAILED(converter->Initialize(scaler.Get(),
                                     GUID_WICPixelFormat32bppBGRA,
                                     WICBitmapDitherTypeNone,
                                     nullptr,
                                     0.0,
                                     WICBitmapPaletteTypeCustom))) {
        return false;
    }

    constexpr UINT kStride = kModelInputWidth * 4;
    out_bgra.resize(static_cast<std::size_t>(kStride) * kModelInputHeight);
    return SUCCEEDED(converter->CopyPixels(nullptr,
                                           kStride,
                                           static_cast<UINT>(out_bgra.size()),
                                           out_bgra.data()));
}

bool resize_cover_bgra(const std::vector<std::uint8_t>& source,
                       std::uint32_t source_width,
                       std::uint32_t source_height,
                       std::vector<std::uint8_t>& out_bgra) {
    if (source_width == 0 || source_height == 0 ||
        source.size() != static_cast<std::size_t>(source_width) * source_height * 4) {
        return false;
    }
    if (source_width == kModelInputWidth && source_height == kModelInputHeight) {
        out_bgra = source;
        return true;
    }

    const double source_aspect = static_cast<double>(source_width) / source_height;
    constexpr double kTargetAspect = static_cast<double>(kModelInputWidth) / kModelInputHeight;
    double crop_x = 0.0;
    double crop_y = 0.0;
    double crop_width = source_width;
    double crop_height = source_height;
    if (source_aspect > kTargetAspect) {
        crop_width = source_height * kTargetAspect;
        crop_x = (source_width - crop_width) * 0.5;
    } else if (source_aspect < kTargetAspect) {
        crop_height = source_width / kTargetAspect;
        crop_y = (source_height - crop_height) * 0.5;
    }

    out_bgra.resize(static_cast<std::size_t>(kModelInputWidth) * kModelInputHeight * 4);
    const auto resize_row = [&](std::uint32_t y) {
        const double source_y = crop_y +
            (static_cast<double>(y) + 0.5) * crop_height / kModelInputHeight - 0.5;
        const int y0_raw = static_cast<int>(std::floor(source_y));
        const int y1_raw = y0_raw + 1;
        const std::uint32_t y0 = static_cast<std::uint32_t>(
            std::clamp(y0_raw, 0, static_cast<int>(source_height) - 1));
        const std::uint32_t y1 = static_cast<std::uint32_t>(
            std::clamp(y1_raw, 0, static_cast<int>(source_height) - 1));
        const float wy = static_cast<float>(source_y - std::floor(source_y));
        for (std::uint32_t x = 0; x < kModelInputWidth; ++x) {
            const double source_x = crop_x +
                (static_cast<double>(x) + 0.5) * crop_width / kModelInputWidth - 0.5;
            const int x0_raw = static_cast<int>(std::floor(source_x));
            const int x1_raw = x0_raw + 1;
            const std::uint32_t x0 = static_cast<std::uint32_t>(
                std::clamp(x0_raw, 0, static_cast<int>(source_width) - 1));
            const std::uint32_t x1 = static_cast<std::uint32_t>(
                std::clamp(x1_raw, 0, static_cast<int>(source_width) - 1));
            const float wx = static_cast<float>(source_x - std::floor(source_x));
            const std::size_t p00 = (static_cast<std::size_t>(y0) * source_width + x0) * 4;
            const std::size_t p10 = (static_cast<std::size_t>(y0) * source_width + x1) * 4;
            const std::size_t p01 = (static_cast<std::size_t>(y1) * source_width + x0) * 4;
            const std::size_t p11 = (static_cast<std::size_t>(y1) * source_width + x1) * 4;
            const std::size_t output =
                (static_cast<std::size_t>(y) * kModelInputWidth + x) * 4;
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const float top = source[p00 + channel] * (1.0f - wx) +
                                  source[p10 + channel] * wx;
                const float bottom = source[p01 + channel] * (1.0f - wx) +
                                     source[p11 + channel] * wx;
                out_bgra[output + channel] = static_cast<std::uint8_t>(
                    std::clamp(std::lround(top * (1.0f - wy) + bottom * wy), 0l, 255l));
            }
        }
    };
#ifdef _MSC_VER
    concurrency::parallel_for<std::uint32_t>(0, kModelInputHeight, resize_row);
#else
    for (std::uint32_t y = 0; y < kModelInputHeight; ++y) {
        resize_row(y);
    }
#endif
    return true;
}

std::vector<float> bgra_to_nchw_rgb(const std::vector<std::uint8_t>& bgra) {
    const std::size_t pixels = static_cast<std::size_t>(kModelInputWidth) * kModelInputHeight;
    if (bgra.size() != pixels * 4) {
        return {};
    }
    std::vector<float> rgb(pixels * 3);
    for (std::size_t i = 0; i < pixels; ++i) {
        rgb[i] = bgra[i * 4 + 2] / 255.0f;
        rgb[pixels + i] = bgra[i * 4 + 1] / 255.0f;
        rgb[pixels * 2 + i] = bgra[i * 4] / 255.0f;
    }
    return rgb;
}

std::vector<std::uint8_t> compose_lunasr_bgra(
    const std::vector<std::uint8_t>& input,
    const std::vector<float>& residual) {
    const std::size_t expected = static_cast<std::size_t>(kModelInputWidth) *
                                 kModelInputHeight * 4;
    const std::size_t target_pixels = static_cast<std::size_t>(kLunaSrTargetWidth) *
                                      kLunaSrTargetHeight;
    if (input.size() != expected || residual.size() != target_pixels * 3) {
        return {};
    }

    std::vector<std::uint8_t> output(static_cast<std::size_t>(kLunaSrTargetWidth) *
                                     kLunaSrTargetHeight * 4);
    const auto compose_row = [&](std::uint32_t y) {
        const float source_y = (static_cast<float>(y) + 0.5f) * 0.5f - 0.5f;
        const int y0_raw = static_cast<int>(std::floor(source_y));
        const int y1_raw = y0_raw + 1;
        const std::uint32_t y0 = static_cast<std::uint32_t>(
            std::clamp(y0_raw, 0, static_cast<int>(kModelInputHeight) - 1));
        const std::uint32_t y1 = static_cast<std::uint32_t>(
            std::clamp(y1_raw, 0, static_cast<int>(kModelInputHeight) - 1));
        const float wy = source_y - std::floor(source_y);

        for (std::uint32_t x = 0; x < kLunaSrTargetWidth; ++x) {
            const float source_x = (static_cast<float>(x) + 0.5f) * 0.5f - 0.5f;
            const int x0_raw = static_cast<int>(std::floor(source_x));
            const int x1_raw = x0_raw + 1;
            const std::uint32_t x0 = static_cast<std::uint32_t>(
                std::clamp(x0_raw, 0, static_cast<int>(kModelInputWidth) - 1));
            const std::uint32_t x1 = static_cast<std::uint32_t>(
                std::clamp(x1_raw, 0, static_cast<int>(kModelInputWidth) - 1));
            const float wx = source_x - std::floor(source_x);

            const std::size_t p00 = (static_cast<std::size_t>(y0) * kModelInputWidth + x0) * 4;
            const std::size_t p10 = (static_cast<std::size_t>(y0) * kModelInputWidth + x1) * 4;
            const std::size_t p01 = (static_cast<std::size_t>(y1) * kModelInputWidth + x0) * 4;
            const std::size_t p11 = (static_cast<std::size_t>(y1) * kModelInputWidth + x1) * 4;
            const std::size_t out = (static_cast<std::size_t>(y) * kLunaSrTargetWidth + x) * 4;
            const auto interpolate = [&](std::size_t channel) {
                const float top = input[p00 + channel] * (1.0f - wx) +
                                  input[p10 + channel] * wx;
                const float bottom = input[p01 + channel] * (1.0f - wx) +
                                     input[p11 + channel] * wx;
                return top * (1.0f - wy) + bottom * wy;
            };
            const float alpha_byte = std::clamp(interpolate(3), 0.0f, 255.0f);
            const float alpha = alpha_byte / 255.0f;
            const std::size_t output_pixel = static_cast<std::size_t>(y) *
                                             kLunaSrTargetWidth + x;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const std::size_t residual_channel = 2 - channel;  // BGRA -> RGB NCHW
                const float detail = residual[residual_channel * target_pixels + output_pixel];
                const float corrected = std::clamp(interpolate(channel) / 255.0f + detail,
                                                   0.0f,
                                                   1.0f);
                output[out + channel] = static_cast<std::uint8_t>(
                    std::clamp(std::lround(corrected * alpha * 255.0f), 0l, 255l));
            }
            output[out + 3] = static_cast<std::uint8_t>(
                std::clamp(std::lround(alpha_byte), 0l, 255l));
        }
    };
#ifdef _MSC_VER
    concurrency::parallel_for<std::uint32_t>(0, kLunaSrTargetHeight, compose_row);
#else
    for (std::uint32_t y = 0; y < kLunaSrTargetHeight; ++y) {
        compose_row(y);
    }
#endif
    return output;
}

bool pass_process_benchmark_gate(
    const winrt::Windows::AI::MachineLearning::LearningModelSession& session) {
    auto& gate = process_benchmark_gate();
    {
        std::unique_lock<std::mutex> lock(gate.mutex);
        if (gate.state == LunaSrBenchmarkState::Passed) {
            return true;
        }
        if (gate.state == LunaSrBenchmarkState::Failed) {
            return false;
        }
        if (gate.state == LunaSrBenchmarkState::Running) {
            gate.changed.wait(lock, [&gate]() {
                return gate.state != LunaSrBenchmarkState::Running;
            });
            return gate.state == LunaSrBenchmarkState::Passed;
        }
        gate.state = LunaSrBenchmarkState::Running;
        gate.detail = "Benchmarking fixed 960x540 RGB x2 inference.";
    }

    double fps = 0.0;
    std::string detail;
    bool passed = false;
    try {
        namespace ml = winrt::Windows::AI::MachineLearning;
        const std::vector<std::int64_t> input_shape{1, 3, kModelInputHeight, kModelInputWidth};
        const std::vector<std::int64_t> output_shape{1, 3, kLunaSrTargetHeight, kLunaSrTargetWidth};
        std::vector<float> input(static_cast<std::size_t>(kModelInputWidth) *
                                 kModelInputHeight * 3,
                                 0.5f);
        const ml::TensorFloat input_tensor =
            ml::TensorFloat::CreateFromArray(input_shape, input);
        const ml::TensorFloat output_tensor = ml::TensorFloat::Create(output_shape);
        ml::LearningModelBinding binding(session);
        binding.Bind(L"rgb_lr", input_tensor);
        binding.Bind(L"rgb_residual_x2", output_tensor);
        for (int frame = 0; frame < kBenchmarkWarmupFrames; ++frame) {
            session.Evaluate(binding, L"tenriff-lunasr-benchmark-warmup");
        }
        const auto started = std::chrono::steady_clock::now();
        for (int frame = 0; frame < kBenchmarkTimedFrames; ++frame) {
            session.Evaluate(binding, L"tenriff-lunasr-benchmark");
        }
        const auto finished = std::chrono::steady_clock::now();
        const double elapsed_seconds =
            std::chrono::duration<double>(finished - started).count();
        if (elapsed_seconds > 0.0) {
            fps = static_cast<double>(kBenchmarkTimedFrames) / elapsed_seconds;
        }
        passed = LunaSrBackgroundUpscaler::meets_performance_gate(fps);
        detail = passed
                     ? "Fixed 960x540 RGB x2 inference passed the 35 FPS gate."
                     : "Fixed 960x540 RGB x2 inference is below 35 FPS; LunaSR is disabled.";
    } catch (const winrt::hresult_error& error) {
        detail = "WinML benchmark failed with HRESULT 0x" +
                 std::to_string(static_cast<std::uint32_t>(error.code())) +
                 "; LunaSR is disabled.";
    } catch (const std::exception& error) {
        detail = std::string("WinML benchmark failed: ") + error.what() +
                 "; LunaSR is disabled.";
    }

    {
        std::lock_guard<std::mutex> lock(gate.mutex);
        gate.fps = fps;
        gate.detail = detail;
        gate.state = passed ? LunaSrBenchmarkState::Passed : LunaSrBenchmarkState::Failed;
    }
    gate.changed.notify_all();
    std::cerr << "[LunaSR] benchmark=" << fps << " FPS, required="
              << kLunaSrMinimumBenchmarkFps << " FPS: " << detail << '\n';
    return passed;
}
#endif

}  // namespace

struct LunaSrBackgroundUpscaler::Impl {
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    bool stop = false;
    std::string requested_path;
    std::string pending_path;
    bool pending_is_bgra = false;
    std::uint32_t pending_width = 0;
    std::uint32_t pending_height = 0;
    std::vector<std::uint8_t> pending_bgra;
    std::uint64_t request_id = 0;
    std::uint64_t pending_id = 0;
    std::shared_ptr<const LunaSrBackgroundFrame> ready;
    std::unordered_map<std::string, std::shared_ptr<const LunaSrBackgroundFrame>> cache;
    std::deque<std::string> cache_lru;

    Impl() : worker([this]() { run(); }) {}

    ~Impl() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        wake.notify_all();
        if (worker.joinable()) {
            worker.join();
        }
    }

    void touch_cache_locked(const std::string& path) {
        cache_lru.erase(std::remove(cache_lru.begin(), cache_lru.end(), path), cache_lru.end());
        cache_lru.push_back(path);
        while (cache_lru.size() > kCachedFrameLimit) {
            cache.erase(cache_lru.front());
            cache_lru.pop_front();
        }
    }

    void run() {
#if defined(TENRIFF_ENABLE_LUNASR) && defined(_WIN32)
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            Microsoft::WRL::ComPtr<IWICImagingFactory> wic_factory;
            if (FAILED(CoCreateInstance(CLSID_WICImagingFactory,
                                        nullptr,
                                        CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&wic_factory))) ||
                !wic_factory) {
                std::cerr << "[LunaSR] WIC initialization failed; using native background scaling.\n";
                return;
            }

            const auto model_path = find_model_path();
            if (!model_path.has_value()) {
                std::cerr << "[LunaSR] WinML model not found; using native background scaling.\n";
                return;
            }

            namespace ml = winrt::Windows::AI::MachineLearning;
            const ml::LearningModel model = ml::LearningModel::LoadFromFilePath(model_path->c_str());
            ml::LearningModelSession session{nullptr};
            try {
                session = ml::LearningModelSession(
                    model,
                    ml::LearningModelDevice(ml::LearningModelDeviceKind::DirectXHighPerformance));
            } catch (const winrt::hresult_error&) {
                session = ml::LearningModelSession(
                    model,
                    ml::LearningModelDevice(ml::LearningModelDeviceKind::DirectX));
            }

            if (!pass_process_benchmark_gate(session)) {
                return;
            }

            const std::vector<std::int64_t> input_shape{1, 3, kModelInputHeight, kModelInputWidth};
            const std::vector<std::int64_t> output_shape{1, 3, kLunaSrTargetHeight, kLunaSrTargetWidth};

            while (true) {
                std::string path;
                std::uint64_t id = 0;
                bool is_bgra = false;
                std::uint32_t source_width = 0;
                std::uint32_t source_height = 0;
                std::vector<std::uint8_t> source_bgra;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    wake.wait(lock, [this]() { return stop || !pending_path.empty(); });
                    if (stop) {
                        return;
                    }
                    path = std::move(pending_path);
                    pending_path.clear();
                    id = pending_id;
                    is_bgra = pending_is_bgra;
                    source_width = pending_width;
                    source_height = pending_height;
                    source_bgra = std::move(pending_bgra);
                    pending_is_bgra = false;
                    pending_width = 0;
                    pending_height = 0;
                }

                std::vector<std::uint8_t> low_bgra;
                const bool decoded = is_bgra
                                         ? resize_cover_bgra(source_bgra,
                                                             source_width,
                                                             source_height,
                                                             low_bgra)
                                         : decode_cover_frame(wic_factory.Get(), path, low_bgra);
                if (!decoded) {
                    std::cerr << "[LunaSR] Could not decode background; keeping native bitmap: "
                              << path << '\n';
                    continue;
                }

                std::vector<float> rgb = bgra_to_nchw_rgb(low_bgra);
                if (rgb.empty()) {
                    continue;
                }

                const ml::TensorFloat input_tensor =
                    ml::TensorFloat::CreateFromArray(input_shape, rgb);
                const ml::TensorFloat output_tensor =
                    ml::TensorFloat::Create(output_shape);
                ml::LearningModelBinding binding(session);
                binding.Bind(L"rgb_lr", input_tensor);
                binding.Bind(L"rgb_residual_x2", output_tensor);
                session.Evaluate(binding, L"tenriff-lunasr-background");

                std::vector<float> residual(static_cast<std::size_t>(kLunaSrTargetWidth) *
                                            kLunaSrTargetHeight * 3);
                const auto residual_view = output_tensor.GetAsVectorView();
                if (residual_view.GetMany(0, residual) != residual.size()) {
                    std::cerr << "[LunaSR] WinML returned an incomplete frame; keeping native bitmap.\n";
                    continue;
                }

                std::vector<std::uint8_t> output =
                    compose_lunasr_bgra(low_bgra, residual);
                if (output.empty()) {
                    continue;
                }

                auto frame = std::make_shared<LunaSrBackgroundFrame>();
                frame->source_path = path;
                frame->request_id = id;
                frame->width = kLunaSrTargetWidth;
                frame->height = kLunaSrTargetHeight;
                frame->bgra = std::move(output);

                {
                    std::lock_guard<std::mutex> lock(mutex);
                    if (!is_bgra) {
                        cache[path] = frame;
                        touch_cache_locked(path);
                    }
                    if (id == request_id && path == requested_path) {
                        ready = std::move(frame);
                    }
                }
            }
        } catch (const winrt::hresult_error& error) {
            std::wcerr << L"[LunaSR] WinML disabled after error 0x" << std::hex
                       << static_cast<std::uint32_t>(error.code()) << std::dec
                       << L": " << error.message().c_str()
                       << L". Native background scaling remains active.\n";
        } catch (const std::exception& error) {
            std::cerr << "[LunaSR] Upscaler disabled: " << error.what()
                      << ". Native background scaling remains active.\n";
        }
#else
        std::unique_lock<std::mutex> lock(mutex);
        wake.wait(lock, [this]() { return stop; });
#endif
    }
};

LunaSrBackgroundUpscaler::LunaSrBackgroundUpscaler()
    : impl_(std::make_unique<Impl>()) {}

LunaSrBackgroundUpscaler::~LunaSrBackgroundUpscaler() = default;

void LunaSrBackgroundUpscaler::request(std::string path) {
    if (!impl_ || path.empty()) {
        return;
    }
    if (benchmark_status_snapshot().state == LunaSrBenchmarkState::Failed) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (path == impl_->requested_path) {
        return;
    }
    impl_->requested_path = std::move(path);
    impl_->pending_is_bgra = false;
    impl_->pending_width = 0;
    impl_->pending_height = 0;
    impl_->pending_bgra.clear();
    ++impl_->request_id;
    const auto cached = impl_->cache.find(impl_->requested_path);
    if (cached != impl_->cache.end()) {
        impl_->ready = cached->second;
        impl_->touch_cache_locked(impl_->requested_path);
        impl_->pending_path.clear();
        return;
    }
    impl_->pending_path = impl_->requested_path;
    impl_->pending_id = impl_->request_id;
    impl_->ready.reset();
    impl_->wake.notify_one();
}

void LunaSrBackgroundUpscaler::request_bgra(std::string source_key,
                                             std::uint32_t width,
                                             std::uint32_t height,
                                             const std::vector<std::uint8_t>& bgra) {
    const std::size_t expected = static_cast<std::size_t>(width) * height * 4;
    if (!impl_ || source_key.empty() || width == 0 || height == 0 || bgra.size() != expected) {
        return;
    }
    if (benchmark_status_snapshot().state == LunaSrBenchmarkState::Failed) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (source_key == impl_->requested_path) {
        return;
    }
    impl_->requested_path = std::move(source_key);
    impl_->pending_path = impl_->requested_path;
    impl_->pending_is_bgra = true;
    impl_->pending_width = width;
    impl_->pending_height = height;
    impl_->pending_bgra = bgra;
    impl_->pending_id = ++impl_->request_id;
    impl_->ready.reset();
    impl_->wake.notify_one();
}

std::shared_ptr<const LunaSrBackgroundFrame> LunaSrBackgroundUpscaler::take_ready() {
    if (!impl_) {
        return {};
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return std::exchange(impl_->ready, {});
}

void LunaSrBackgroundUpscaler::clear() {
    if (!impl_) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->requested_path.clear();
    impl_->pending_path.clear();
    impl_->pending_is_bgra = false;
    impl_->pending_width = 0;
    impl_->pending_height = 0;
    impl_->pending_bgra.clear();
    impl_->ready.reset();
    ++impl_->request_id;
}

bool LunaSrBackgroundUpscaler::should_upscale(std::uint32_t width,
                                              std::uint32_t height,
                                              std::string_view mode) {
    return lower_ascii(mode) == "lunasr" && width > 0 && height > 0 &&
           (width < kLunaSrTargetWidth || height < kLunaSrTargetHeight);
}

bool LunaSrBackgroundUpscaler::meets_performance_gate(double fps) {
    return std::isfinite(fps) && fps >= kLunaSrMinimumBenchmarkFps;
}

LunaSrBenchmarkStatus LunaSrBackgroundUpscaler::benchmark_status() {
    return benchmark_status_snapshot();
}

}  // namespace tenriff::render
