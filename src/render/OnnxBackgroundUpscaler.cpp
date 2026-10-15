#include "render/OnnxBackgroundUpscaler.h"
#include "render/OnnxUpscaleImage.h"
#include "render/FfmpegImageFilter.h"

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
#include <stdexcept>
#include <thread>
#include <type_traits>
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

constexpr std::uint32_t kModelInputWidth = onnx_image::kInputWidth;
constexpr std::uint32_t kModelInputHeight = onnx_image::kInputHeight;
constexpr std::size_t kCachedFrameLimit = 3;
constexpr std::size_t kCachedByteLimit = 96u * 1024u * 1024u;

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

bool decode_native_frame(IWICImagingFactory* factory, std::string_view path,
                         std::uint32_t& width, std::uint32_t& height,
                         std::vector<std::uint8_t>& bgra) {
    if (!factory || path.empty()) return false;
    const auto wide_path = util::path_from_utf8_lossy(path).native();
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(wide_path.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, &decoder)) || !decoder) return false;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame)) || !frame ||
        FAILED(frame->GetSize(&width, &height)) || !onnx_image::valid_size(width, height)) return false;
    // Decode at authored dimensions. Display fitting and cropping happen only
    // when the renderer draws the completed native-resolution x2 bitmap.
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter)) || !converter ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return false;
    bgra.resize(std::size_t(width) * height * 4);
    return SUCCEEDED(converter->CopyPixels(nullptr, width * 4,
                                           static_cast<UINT>(bgra.size()), bgra.data()));
}

std::vector<float> bgra_to_nchw_rgb(const std::vector<std::uint8_t>& bgra,
                                   std::uint32_t width, std::uint32_t height) {
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
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
    bool bgra_in_flight = false;
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
        std::size_t bytes = 0;
        for (const auto& item : cache) bytes += item.second->bgra.size();
        // Native x2 images are no longer uniformly FHD; bound retained bytes too.
        while (cache_lru.size() > kCachedFrameLimit || bytes > kCachedByteLimit) {
            bytes -= cache.at(cache_lru.front())->bgra.size();
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
            const auto find_tensor_descriptor =
                [](const auto& features, const wchar_t* expected_name) {
                    for (const auto& feature : features) {
                        if (feature.Name() != expected_name) {
                            continue;
                        }
                        const auto tensor = feature.template try_as<ml::TensorFeatureDescriptor>();
                        if (!tensor) {
                            throw std::runtime_error("ONNX feature is not a tensor");
                        }
                        return tensor;
                    }
                    throw std::runtime_error("ONNX model is missing a required tensor");
                };
            const auto input_descriptor =
                find_tensor_descriptor(model.InputFeatures(), L"rgb_lr");
            const auto output_descriptor =
                find_tensor_descriptor(model.OutputFeatures(), L"rgb_residual_x2");
            const auto descriptor_shape = [](const auto& descriptor) {
                const auto shape = descriptor.Shape();
                return std::vector<std::int64_t>(shape.begin(), shape.end());
            };
            const auto model_shape = onnx_image::model_shape(descriptor_shape(input_descriptor),
                                                             descriptor_shape(output_descriptor));
            if (model_shape == onnx_image::ModelShape::Unsupported)
                throw std::runtime_error("ONNX model requires NCHW batch 1 RGB residual-x2 with fixed 960x540 or dynamic H/W boundaries");
            const bool dynamic_spatial = model_shape == onnx_image::ModelShape::Dynamic;
            const ml::TensorKind input_kind = input_descriptor.TensorKind();
            const ml::TensorKind output_kind = output_descriptor.TensorKind();
            const auto tensor_kind_label = [](ml::TensorKind kind) {
                switch (kind) {
                    case ml::TensorKind::Float:
                        return "FP32";
                    case ml::TensorKind::Float16:
                        return "FP16";
                    case ml::TensorKind::Int8:
                        return "INT8";
                    case ml::TensorKind::UInt8:
                        return "UINT8";
                    default:
                        return "UNSUPPORTED";
                }
            };
            const auto is_supported_float_kind = [](ml::TensorKind kind) {
                return kind == ml::TensorKind::Float || kind == ml::TensorKind::Float16;
            };
            if (!is_supported_float_kind(input_kind) ||
                !is_supported_float_kind(output_kind)) {
                throw std::runtime_error(std::string("ONNX boundary detected as rgb_lr=") +
                                         tensor_kind_label(input_kind) +
                                         ", rgb_residual_x2=" +
                                         tensor_kind_label(output_kind) +
                                         "; raw INT8/UINT8 boundaries need scale and zero-point "
                                         "metadata, so use an INT8 QDQ model with FP32/FP16 boundaries");
            }
            std::string quantization;
            const auto metadata = model.Metadata();
            if (metadata.HasKey(L"tenriff.quantization")) {
                quantization = winrt::to_string(metadata.Lookup(L"tenriff.quantization"));
            }
            const std::string quantization_lower = lower_ascii(quantization);
            const bool internal_int8 =
                quantization_lower.find("int8") != std::string::npos ||
                quantization_lower.find("s8") != std::string::npos ||
                quantization_lower.find("u8") != std::string::npos ||
                quantization_lower.find("qdq") != std::string::npos;
            std::cerr << "[ONNX Upscaler] Model contract accepted: rgb_lr="
                      << tensor_kind_label(input_kind)
                      << ", rgb_residual_x2=" << tensor_kind_label(output_kind)
                      << (dynamic_spatial ? ", spatial=dynamic original size" : ", spatial=fixed 960x540 tiles")
                      << (internal_int8
                              ? ", internal quantization=INT8/QDQ (" + quantization + ")"
                              : "")
                      << ".\n";
            ml::LearningModelSession session{nullptr};
            bool using_low_power_preference = false;
            const auto make_high_performance_session = [&]() {
                try {
                    auto high_performance = ml::LearningModelSession(
                        model,
                        ml::LearningModelDevice(ml::LearningModelDeviceKind::DirectXHighPerformance));
                    std::cerr << "[ONNX Upscaler] Using high-performance DirectX GPU session.\n";
                    return high_performance;
                } catch (const winrt::hresult_error&) {
                    auto directx = ml::LearningModelSession(
                        model,
                        ml::LearningModelDevice(ml::LearningModelDeviceKind::DirectX));
                    std::cerr << "[ONNX Upscaler] High-performance GPU session unavailable; "
                                 "using the default DirectX device.\n";
                    return directx;
                }
            };
            if (prefer_npu) {
                try {
                    // Legacy WinML cannot name or verify an NPU. DirectXMinPower requests the
                    // lowest-power compatible DirectX ML device, so this path must not be logged
                    // as proof of NPU execution. A deterministic GPU fallback remains below.
                    session = ml::LearningModelSession(
                        model,
                        ml::LearningModelDevice(ml::LearningModelDeviceKind::DirectXMinPower));
                    using_low_power_preference = true;
                    std::cerr << "[ONNX Upscaler] Requested Windows DirectXMinPower; "
                                 "this is not an explicit or verified NPU selection.\n";
                } catch (const winrt::hresult_error&) {
                    std::cerr << "[ONNX Upscaler] DirectXMinPower session unavailable; "
                                 "falling back to high-performance DirectX.\n";
                }
            }
            if (!session) {
                session = make_high_performance_session();
            }

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
                    bgra_in_flight = is_bgra;
                }
                const auto finish_bgra_request = [&]() {
                    if (!is_bgra) {
                        return;
                    }
                    std::lock_guard<std::mutex> lock(mutex);
                    bgra_in_flight = false;
                };

                const bool decoded = is_bgra
                    ? onnx_image::valid_size(source_width, source_height)
                    : decode_native_frame(wic_factory.Get(), path, source_width, source_height, source_bgra);
                if (!decoded) {
                    std::cerr << "[ONNX Upscaler] Source cannot be decoded at native size (max 4096 per axis); keeping native bitmap.\n";
                    finish_bgra_request();
                    continue;
                }
                const auto deblocked = deblock_bgra_ffmpeg(source_width, source_height, source_bgra, [&]() {
                    std::lock_guard<std::mutex> lock(mutex);
                    return stop || id != request_id;
                });
                if (deblocked != DeblockResult::Completed) {
                    if (deblocked != DeblockResult::Cancelled)
                        std::cerr << "[ONNX Upscaler] FFmpeg deblock unavailable or failed; keeping native bitmap.\n";
                    finish_bgra_request();
                    continue;
                }
                std::vector<std::uint8_t> output(std::size_t(source_width * 2) * (source_height * 2) * 4);
                const auto input_width = dynamic_spatial ? source_width : kModelInputWidth;
                const auto input_height = dynamic_spatial ? source_height : kModelInputHeight;
                const std::vector<std::int64_t> input_shape{1, 3, input_height, input_width};
                const std::vector<std::int64_t> output_shape{1, 3, input_height * 2, input_width * 2};
                const auto tiles = dynamic_spatial
                    ? std::vector<onnx_image::Tile>{{0, 0, source_width, source_height, 0, 0}}
                    : onnx_image::plan_tiles(source_width, source_height);
                bool completed = true;
                for (const auto& tile : tiles) {
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        if (stop || id != request_id) { completed = false; break; }
                    }
                    // Dynamic models see exactly the decoded/filtered original, with no
                    // padding, tiling or duplicate full-image BGRA allocation.
                    const auto low_bgra = dynamic_spatial ? std::vector<std::uint8_t>{}
                        : onnx_image::extract_tile(source_bgra, source_width, source_height, tile);
                    std::vector<float> rgb = bgra_to_nchw_rgb(dynamic_spatial ? source_bgra : low_bgra,
                                                             input_width, input_height);
                    if (rgb.empty()) { completed = false; break; }

                    const auto evaluate_residual = [&](const ml::LearningModelSession& active_session) {
                        const auto bind_and_evaluate =
                            [&](const auto& input_tensor, const auto& output_tensor) {
                                ml::LearningModelBinding binding(active_session);
                                binding.Bind(L"rgb_lr", input_tensor);
                                binding.Bind(L"rgb_residual_x2", output_tensor);
                                const auto result = active_session.Evaluate(
                                    binding,
                                    L"tenriff-onnx-upscaler-background");
                                // A dynamic descriptor cannot promise an x2 result. Validate
                                // the evaluated tensor before indexing residual pixels.
                                const auto evaluated = result.Outputs().Lookup(L"rgb_residual_x2")
                                    .template as<std::decay_t<decltype(output_tensor)>>();
                                const auto evaluated_shape = evaluated.Shape();
                                if (!onnx_image::valid_residual_shape(
                                        std::vector<std::int64_t>(evaluated_shape.begin(), evaluated_shape.end()),
                                        input_width, input_height))
                                    throw std::runtime_error("ONNX evaluated output must be batch 1 RGB at exactly x2 input dimensions");
                                std::vector<float> residual(std::size_t(input_width) * input_height * 12);
                                const auto residual_view = evaluated.GetAsVectorView();
                                if (residual_view.Size() != residual.size() ||
                                    residual_view.GetMany(0, residual) != residual.size()) {
                                    residual.clear();
                                }
                                return residual;
                            };

                        if (input_kind == ml::TensorKind::Float16) {
                            const auto input_tensor =
                                ml::TensorFloat16Bit::CreateFromArray(input_shape, rgb);
                            if (output_kind == ml::TensorKind::Float16) {
                                const auto output_tensor =
                                    ml::TensorFloat16Bit::Create(output_shape);
                                return bind_and_evaluate(input_tensor, output_tensor);
                            }
                            const auto output_tensor = ml::TensorFloat::Create(output_shape);
                            return bind_and_evaluate(input_tensor, output_tensor);
                        }

                        const auto input_tensor =
                            ml::TensorFloat::CreateFromArray(input_shape, rgb);
                        if (output_kind == ml::TensorKind::Float16) {
                            const auto output_tensor =
                                ml::TensorFloat16Bit::Create(output_shape);
                            return bind_and_evaluate(input_tensor, output_tensor);
                        }
                        const auto output_tensor = ml::TensorFloat::Create(output_shape);
                        return bind_and_evaluate(input_tensor, output_tensor);
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
                    if (!onnx_image::compose_tile(source_bgra, source_width, source_height,
                                                  tile, residual, output, input_width, input_height)) {
                        std::cerr << "[ONNX Upscaler] Invalid residual tile; keeping native bitmap.\n";
                        completed = false;
                        break;
                    }
                }
                if (!completed) { finish_bgra_request(); continue; }

                auto frame = std::make_shared<OnnxUpscaleFrame>();
                frame->source_path = path;
                frame->request_id = id;
                frame->width = source_width * 2;
                frame->height = source_height * 2;
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
                finish_bgra_request();
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

bool OnnxBackgroundUpscaler::request_bgra(std::string source_key,
                                           std::uint32_t width,
                                           std::uint32_t height,
                                           const std::vector<std::uint8_t>& bgra) {
    if (!onnx_image::valid_size(width, height)) return false;
    const std::size_t expected = static_cast<std::size_t>(width) * height * 4;
    if (!impl_ || source_key.empty() || bgra.size() != expected) {
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (source_key == impl_->requested_path || impl_->bgra_in_flight ||
        !impl_->pending_path.empty() || impl_->ready) {
        return false;
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
    return true;
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
    return lower_ascii(mode) == "onnx" && onnx_image::valid_size(width, height);
}

bool OnnxBackgroundUpscaler::should_upscale_realtime_video(std::string_view) {
    // The worker returns frames asynchronously. Applying a completed video frame after
    // newer native frames have already been presented makes the BGA jump backward in time.
    // Keep real-time video native until a timestamp-aware ordered output queue exists.
    return false;
}

}  // namespace tenriff::render
