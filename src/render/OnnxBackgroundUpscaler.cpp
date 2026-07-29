#include "render/OnnxBackgroundUpscaler.h"

#include <algorithm>
#include <array>
#include <cmath>
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

#if defined(TENRIFF_ENABLE_ONNX_UPSCALER) && defined(_WIN32)
#include <winrt/Windows.AI.MachineLearning.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/base.h>
#endif

namespace tenriff::render {

namespace {

constexpr std::uint32_t kModelInputWidth = 960;
constexpr std::uint32_t kModelInputHeight = 540;
constexpr std::size_t kCachedFrameLimit = 3;

std::string lower_ascii(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

#ifdef _WIN32
std::optional<std::filesystem::path> resolve_model_path(std::string_view configured_path) {
    namespace fs = std::filesystem;
    if (configured_path.empty()) {
        return std::nullopt;
    }

    const fs::path configured = util::path_from_utf8_lossy(configured_path);
    std::error_code ec;
    if (configured.is_absolute()) {
        return fs::is_regular_file(configured, ec) && !ec
                   ? std::optional<fs::path>{configured}
                   : std::nullopt;
    }

    std::vector<fs::path> roots;
    std::array<wchar_t, 32768> module_path{};
    const DWORD length = GetModuleFileNameW(nullptr,
                                            module_path.data(),
                                            static_cast<DWORD>(module_path.size()));
    if (length > 0 && length < module_path.size()) {
        roots.push_back(fs::path(module_path.data()).parent_path());
    }
    const fs::path current = fs::current_path(ec);
    if (!ec && !current.empty()) {
        roots.push_back(current);
    }

    for (const auto& root : roots) {
        const fs::path candidate = root / configured;
        if (fs::is_regular_file(candidate, ec) && !ec) {
            return candidate;
        }
        ec.clear();
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

std::vector<std::uint8_t> compose_onnx_residual_bgra(
    const std::vector<std::uint8_t>& input,
    const std::vector<float>& residual) {
    const std::size_t expected = static_cast<std::size_t>(kModelInputWidth) *
                                 kModelInputHeight * 4;
    const std::size_t target_pixels = static_cast<std::size_t>(kOnnxUpscaleTargetWidth) *
                                      kOnnxUpscaleTargetHeight;
    if (input.size() != expected || residual.size() != target_pixels * 3) {
        return {};
    }

    std::vector<std::uint8_t> output(static_cast<std::size_t>(kOnnxUpscaleTargetWidth) *
                                     kOnnxUpscaleTargetHeight * 4);
    const auto compose_row = [&](std::uint32_t y) {
        const float source_y = (static_cast<float>(y) + 0.5f) * 0.5f - 0.5f;
        const int y0_raw = static_cast<int>(std::floor(source_y));
        const int y1_raw = y0_raw + 1;
        const std::uint32_t y0 = static_cast<std::uint32_t>(
            std::clamp(y0_raw, 0, static_cast<int>(kModelInputHeight) - 1));
        const std::uint32_t y1 = static_cast<std::uint32_t>(
            std::clamp(y1_raw, 0, static_cast<int>(kModelInputHeight) - 1));
        const float wy = source_y - std::floor(source_y);

        for (std::uint32_t x = 0; x < kOnnxUpscaleTargetWidth; ++x) {
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
            const std::size_t out = (static_cast<std::size_t>(y) * kOnnxUpscaleTargetWidth + x) * 4;
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
                                             kOnnxUpscaleTargetWidth + x;
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
    concurrency::parallel_for<std::uint32_t>(0, kOnnxUpscaleTargetHeight, compose_row);
#else
    for (std::uint32_t y = 0; y < kOnnxUpscaleTargetHeight; ++y) {
        compose_row(y);
    }
#endif
    return output;
}

#endif

}  // namespace

struct OnnxBackgroundUpscaler::Impl {
    std::string model_path;
    bool prefer_npu = true;
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
    std::shared_ptr<const OnnxUpscaleFrame> ready;
    std::unordered_map<std::string, std::shared_ptr<const OnnxUpscaleFrame>> cache;
    std::deque<std::string> cache_lru;

    explicit Impl(std::string configured_model_path, bool configured_prefer_npu)
        : model_path(std::move(configured_model_path)),
          prefer_npu(configured_prefer_npu),
          worker([this]() { run(); }) {}

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
#if defined(TENRIFF_ENABLE_ONNX_UPSCALER) && defined(_WIN32)
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            Microsoft::WRL::ComPtr<IWICImagingFactory> wic_factory;
            if (FAILED(CoCreateInstance(CLSID_WICImagingFactory,
                                        nullptr,
                                        CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&wic_factory))) ||
                !wic_factory) {
                std::cerr << "[ONNX Upscaler] WIC initialization failed; using native background scaling.\n";
                return;
            }

            const auto resolved_model_path = resolve_model_path(model_path);
            if (!resolved_model_path.has_value()) {
                std::cerr << "[ONNX Upscaler] WinML model not found; using native background scaling.\n";
                return;
            }

            namespace ml = winrt::Windows::AI::MachineLearning;
            const ml::LearningModel model = ml::LearningModel::LoadFromFilePath(resolved_model_path->c_str());
            ml::LearningModelSession session{nullptr};
            bool using_low_power_preference = false;
            const auto make_high_performance_session = [&]() {
                try {
                    return ml::LearningModelSession(
                        model,
                        ml::LearningModelDevice(ml::LearningModelDeviceKind::DirectXHighPerformance));
                } catch (const winrt::hresult_error&) {
                    return ml::LearningModelSession(
                        model,
                        ml::LearningModelDevice(ml::LearningModelDeviceKind::DirectX));
                }
            };
            if (prefer_npu) {
                try {
                    // Legacy WinML cannot name an NPU directly. DirectXMinPower asks Windows for
                    // its lowest-power compatible ML device, which can be an NPU when the OS and
                    // driver expose one. We keep a deterministic GPU fallback below.
                    session = ml::LearningModelSession(
                        model,
                        ml::LearningModelDevice(ml::LearningModelDeviceKind::DirectXMinPower));
                    using_low_power_preference = true;
                    std::cerr << "[ONNX Upscaler] Requested the Windows low-power AI device "
                                 "(NPU when available).\n";
                } catch (const winrt::hresult_error&) {
                    std::cerr << "[ONNX Upscaler] Low-power/NPU-preferred session unavailable; "
                                 "falling back to high-performance DirectX.\n";
                }
            }
            if (!session) {
                session = make_high_performance_session();
            }

            const std::vector<std::int64_t> input_shape{1, 3, kModelInputHeight, kModelInputWidth};
            const std::vector<std::int64_t> output_shape{1, 3, kOnnxUpscaleTargetHeight, kOnnxUpscaleTargetWidth};

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
                    std::cerr << "[ONNX Upscaler] Could not decode background; keeping native bitmap: "
                              << path << '\n';
                    continue;
                }

                std::vector<float> rgb = bgra_to_nchw_rgb(low_bgra);
                if (rgb.empty()) {
                    continue;
                }

                const auto evaluate_residual = [&](const ml::LearningModelSession& active_session) {
                    const ml::TensorFloat input_tensor =
                        ml::TensorFloat::CreateFromArray(input_shape, rgb);
                    const ml::TensorFloat output_tensor =
                        ml::TensorFloat::Create(output_shape);
                    ml::LearningModelBinding binding(active_session);
                    binding.Bind(L"rgb_lr", input_tensor);
                    binding.Bind(L"rgb_residual_x2", output_tensor);
                    active_session.Evaluate(binding, L"tenriff-onnx-upscaler-background");

                    std::vector<float> residual(static_cast<std::size_t>(kOnnxUpscaleTargetWidth) *
                                                kOnnxUpscaleTargetHeight * 3);
                    const auto residual_view = output_tensor.GetAsVectorView();
                    if (residual_view.GetMany(0, residual) != residual.size()) {
                        residual.clear();
                    }
                    return residual;
                };

                std::vector<float> residual;
                try {
                    residual = evaluate_residual(session);
                } catch (const winrt::hresult_error&) {
                    if (!using_low_power_preference) {
                        throw;
                    }
                    std::cerr << "[ONNX Upscaler] NPU/low-power evaluation rejected this model; "
                                 "retrying on high-performance DirectX.\n";
                    session = make_high_performance_session();
                    using_low_power_preference = false;
                    residual = evaluate_residual(session);
                }
                if (residual.empty()) {
                    std::cerr << "[ONNX Upscaler] WinML returned an incomplete frame; "
                                 "keeping native bitmap.\n";
                    continue;
                }

                std::vector<std::uint8_t> output =
                    compose_onnx_residual_bgra(low_bgra, residual);
                if (output.empty()) {
                    continue;
                }

                auto frame = std::make_shared<OnnxUpscaleFrame>();
                frame->source_path = path;
                frame->request_id = id;
                frame->width = kOnnxUpscaleTargetWidth;
                frame->height = kOnnxUpscaleTargetHeight;
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
            std::wcerr << L"[ONNX Upscaler] WinML disabled after error 0x" << std::hex
                       << static_cast<std::uint32_t>(error.code()) << std::dec
                       << L": " << error.message().c_str()
                       << L". Native background scaling remains active.\n";
        } catch (const std::exception& error) {
            std::cerr << "[ONNX Upscaler] Upscaler disabled: " << error.what()
                      << ". Native background scaling remains active.\n";
        }
#else
        std::unique_lock<std::mutex> lock(mutex);
        wake.wait(lock, [this]() { return stop; });
#endif
    }
};

OnnxBackgroundUpscaler::OnnxBackgroundUpscaler(std::string model_path, bool prefer_npu)
    : impl_(std::make_unique<Impl>(std::move(model_path), prefer_npu)) {}

OnnxBackgroundUpscaler::~OnnxBackgroundUpscaler() = default;

void OnnxBackgroundUpscaler::request(std::string path) {
    if (!impl_ || path.empty()) {
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

void OnnxBackgroundUpscaler::request_bgra(std::string source_key,
                                             std::uint32_t width,
                                             std::uint32_t height,
                                             const std::vector<std::uint8_t>& bgra) {
    const std::size_t expected = static_cast<std::size_t>(width) * height * 4;
    if (!impl_ || source_key.empty() || width == 0 || height == 0 || bgra.size() != expected) {
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

std::shared_ptr<const OnnxUpscaleFrame> OnnxBackgroundUpscaler::take_ready() {
    if (!impl_) {
        return {};
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return std::exchange(impl_->ready, {});
}

void OnnxBackgroundUpscaler::clear() {
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

bool OnnxBackgroundUpscaler::should_upscale(std::uint32_t width,
                                              std::uint32_t height,
                                              std::string_view mode) {
    return lower_ascii(mode) == "onnx" && width > 0 && height > 0 &&
           (width < kOnnxUpscaleTargetWidth || height < kOnnxUpscaleTargetHeight);
}

}  // namespace tenriff::render
