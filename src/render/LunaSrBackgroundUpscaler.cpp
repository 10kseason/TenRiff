#include "render/LunaSrBackgroundUpscaler.h"

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
    L"lunasr_basic_v2_dense8_b6_540p_residual_winml_public.onnx";

std::string lower_ascii(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
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

std::vector<std::uint8_t> compose_lunasr_bgra(
    const std::vector<std::uint8_t>& input,
    const std::vector<float>& residual) {
    const std::size_t expected = static_cast<std::size_t>(kModelInputWidth) *
                                 kModelInputHeight * 4;
    if (input.size() != expected ||
        residual.size() != static_cast<std::size_t>(kLunaSrTargetWidth) *
                               kLunaSrTargetHeight) {
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
            const float detail = residual[static_cast<std::size_t>(y) *
                                          kLunaSrTargetWidth + x];
            for (std::size_t channel = 0; channel < 3; ++channel) {
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
#endif

}  // namespace

struct LunaSrBackgroundUpscaler::Impl {
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    bool stop = false;
    std::string requested_path;
    std::string pending_path;
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

            const std::vector<std::int64_t> input_shape{1, 1, kModelInputHeight, kModelInputWidth};
            const std::vector<std::int64_t> output_shape{1, 1, kLunaSrTargetHeight, kLunaSrTargetWidth};

            while (true) {
                std::string path;
                std::uint64_t id = 0;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    wake.wait(lock, [this]() { return stop || !pending_path.empty(); });
                    if (stop) {
                        return;
                    }
                    path = std::move(pending_path);
                    pending_path.clear();
                    id = pending_id;
                }

                std::vector<std::uint8_t> low_bgra;
                if (!decode_cover_frame(wic_factory.Get(), path, low_bgra)) {
                    std::cerr << "[LunaSR] Could not decode background; keeping native bitmap: "
                              << path << '\n';
                    continue;
                }

                std::vector<float> luma(static_cast<std::size_t>(kModelInputWidth) *
                                        kModelInputHeight);
                for (std::size_t i = 0; i < luma.size(); ++i) {
                    const std::size_t pixel = i * 4;
                    const float blue = low_bgra[pixel] / 255.0f;
                    const float green = low_bgra[pixel + 1] / 255.0f;
                    const float red = low_bgra[pixel + 2] / 255.0f;
                    luma[i] = 0.2126f * red + 0.7152f * green + 0.0722f * blue;
                }

                const ml::TensorFloat input_tensor =
                    ml::TensorFloat::CreateFromArray(input_shape, luma);
                const ml::TensorFloat output_tensor = ml::TensorFloat::Create(output_shape);
                ml::LearningModelBinding binding(session);
                binding.Bind(L"luma_lr", input_tensor);
                binding.Bind(L"luma_residual", output_tensor);
                session.Evaluate(binding, L"tenriff-lunasr-background");

                std::vector<float> residual(static_cast<std::size_t>(kLunaSrTargetWidth) *
                                            kLunaSrTargetHeight);
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
                    cache[path] = frame;
                    touch_cache_locked(path);
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
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (path == impl_->requested_path) {
        return;
    }
    impl_->requested_path = std::move(path);
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
    impl_->ready.reset();
    ++impl_->request_id;
}

bool LunaSrBackgroundUpscaler::should_upscale(std::uint32_t width,
                                              std::uint32_t height,
                                              std::string_view mode) {
    return lower_ascii(mode) == "lunasr" && width > 0 && height > 0 &&
           (width < kLunaSrTargetWidth || height < kLunaSrTargetHeight);
}

}  // namespace tenriff::render
