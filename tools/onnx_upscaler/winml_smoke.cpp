#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

#include "render/OnnxBackgroundUpscaler.h"
#include "render/OnnxUpscaleImage.h"

#include <winrt/Windows.AI.MachineLearning.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/base.h>

namespace {

bool write_smoke_bmp(const std::filesystem::path& path) {
    constexpr std::uint32_t width = 4;
    constexpr std::uint32_t height = 4;
    constexpr std::size_t header_size = 54;
    std::vector<std::uint8_t> bytes(header_size + width * height * 4, 0);
    const auto put_u16 = [&](std::size_t offset, std::uint16_t value) {
        bytes[offset] = static_cast<std::uint8_t>(value);
        bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    };
    const auto put_u32 = [&](std::size_t offset, std::uint32_t value) {
        for (std::size_t byte = 0; byte < 4; ++byte) {
            bytes[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
        }
    };

    bytes[0] = 'B';
    bytes[1] = 'M';
    put_u32(2, static_cast<std::uint32_t>(bytes.size()));
    put_u32(10, static_cast<std::uint32_t>(header_size));
    put_u32(14, 40);
    put_u32(18, width);
    put_u32(22, height);
    put_u16(26, 1);
    put_u16(28, 32);
    put_u32(34, width * height * 4);
    for (std::size_t pixel = 0; pixel < width * height; ++pixel) {
        const std::size_t offset = header_size + pixel * 4;
        bytes[offset] = static_cast<std::uint8_t>(32 + pixel * 7);
        bytes[offset + 1] = static_cast<std::uint8_t>(64 + pixel * 5);
        bytes[offset + 2] = static_cast<std::uint8_t>(96 + pixel * 3);
        bytes[offset + 3] = 255;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return output.good();
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);

        if (argc < 2) {
            std::wcerr << L"usage: onnx_upscaler_winml_smoke <model.onnx> [image output-prefix]\n";
            return 2;
        }
        const std::filesystem::path model_path(argv[1]);
        if (!std::filesystem::is_regular_file(model_path)) {
            std::wcerr << L"model not found: " << model_path.c_str() << L'\n';
            return 2;
        }

        if (argc >= 4) {
            tenriff::render::OnnxBackgroundUpscaler upscaler(model_path.u8string(), false,
                argc >= 5 ? winrt::to_string(argv[4]) : "auto");
            upscaler.request(std::filesystem::path(argv[2]).u8string());
            std::shared_ptr<const tenriff::render::OnnxUpscaleFrame> native;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            while (!native && std::chrono::steady_clock::now() < deadline) {
                native = upscaler.take_ready();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (!native) { std::cerr << "Native image restoration timed out\n"; return 7; }
            const std::filesystem::path prefix(argv[3]);
            const auto raw_path = std::filesystem::path(prefix.wstring() + L".bgra");
            const auto json_path = std::filesystem::path(prefix.wstring() + L".json");
            if (std::filesystem::exists(raw_path) || std::filesystem::exists(json_path)) return 8;
            std::ofstream raw(raw_path, std::ios::binary);
            raw.write(reinterpret_cast<const char*>(native->bgra.data()), native->bgra.size());
            std::ofstream json(json_path);
            json << "{\"width\":" << native->width << ",\"height\":" << native->height << "}\n";
            if (!raw.good() || !json.good()) return 8;
            std::cout << "Native image restored: " << native->width << 'x' << native->height << '\n';
            return 0;
        }

        namespace ml = winrt::Windows::AI::MachineLearning;
        const auto load_started = std::chrono::steady_clock::now();
        const ml::LearningModel model = ml::LearningModel::LoadFromFilePath(model_path.c_str());
        const auto find_tensor_descriptor =
            [](const auto& features, const wchar_t* expected_name) {
                for (const auto& feature : features) {
                    if (feature.Name() == expected_name) {
                        return feature.template as<ml::TensorFeatureDescriptor>();
                    }
                }
                throw std::runtime_error("required ONNX tensor is missing");
            };
        const auto input_descriptor = find_tensor_descriptor(model.InputFeatures(), L"rgb_lr");
        const auto output_descriptor = find_tensor_descriptor(model.OutputFeatures(), L"rgb_residual_x2");
        const ml::TensorKind input_kind = input_descriptor.TensorKind();
        const ml::TensorKind output_kind = output_descriptor.TensorKind();
        const auto in_shape = input_descriptor.Shape();
        const auto out_shape = output_descriptor.Shape();
        namespace onnx_image = tenriff::render::onnx_image;
        const auto model_shape = onnx_image::model_shape(
            std::vector<std::int64_t>(in_shape.begin(), in_shape.end()),
            std::vector<std::int64_t>(out_shape.begin(), out_shape.end()));
        if (model_shape == onnx_image::ModelShape::Unsupported) {
            std::cerr << "unsupported ONNX tensor shape contract\n";
            return 3;
        }
        std::string quantization;
        const auto metadata = model.Metadata();
        if (metadata.HasKey(L"tenriff.quantization")) {
            quantization = winrt::to_string(metadata.Lookup(L"tenriff.quantization"));
        }
        const auto supported_kind = [](ml::TensorKind kind) {
            return kind == ml::TensorKind::Float || kind == ml::TensorKind::Float16;
        };
        if (!supported_kind(input_kind) || !supported_kind(output_kind)) {
            std::cerr << "unsupported ONNX tensor type\n";
            return 3;
        }

        const ml::LearningModelDevice device(ml::LearningModelDeviceKind::DirectXHighPerformance);
        const ml::LearningModelSession session(model, device);
        const auto session_ready = std::chrono::steady_clock::now();

        const bool dynamic_spatial = model_shape == onnx_image::ModelShape::Dynamic;
        const std::uint32_t input_width = dynamic_spatial ? 19 : 960;
        const std::uint32_t input_height = dynamic_spatial ? 13 : 540;
        const std::vector<std::int64_t> input_shape{1, 3, input_height, input_width};
        const std::vector<std::int64_t> output_shape{1, 3, input_height * 2, input_width * 2};
        std::vector<float> input(std::size_t(input_width) * input_height * 3, 0.5f);
        const auto evaluate_started = std::chrono::steady_clock::now();
        const auto bind_and_evaluate =
            [&](const auto& input_tensor, const auto& output_tensor) {
                ml::LearningModelBinding binding(session);
                binding.Bind(L"rgb_lr", input_tensor);
                binding.Bind(L"rgb_residual_x2", output_tensor);
                const auto result = session.Evaluate(binding, L"onnx-upscaler-smoke");
                const auto evaluated = result.Outputs().Lookup(L"rgb_residual_x2")
                    .template as<std::decay_t<decltype(output_tensor)>>();
                const auto evaluated_shape = evaluated.Shape();
                if (!onnx_image::valid_residual_shape(
                        std::vector<std::int64_t>(evaluated_shape.begin(), evaluated_shape.end()),
                        input_width, input_height))
                    throw std::runtime_error("evaluated ONNX output is not RGB residual x2");
                return evaluated.GetAsVectorView();
            };
        winrt::Windows::Foundation::Collections::IVectorView<float> values{nullptr};
        if (input_kind == ml::TensorKind::Float16) {
            const auto input_tensor =
                ml::TensorFloat16Bit::CreateFromArray(input_shape, input);
            if (output_kind == ml::TensorKind::Float16) {
                const auto output_tensor = ml::TensorFloat16Bit::Create(output_shape);
                values = bind_and_evaluate(input_tensor, output_tensor);
            } else {
                const auto output_tensor = ml::TensorFloat::Create(output_shape);
                values = bind_and_evaluate(input_tensor, output_tensor);
            }
        } else {
            const auto input_tensor = ml::TensorFloat::CreateFromArray(input_shape, input);
            if (output_kind == ml::TensorKind::Float16) {
                const auto output_tensor = ml::TensorFloat16Bit::Create(output_shape);
                values = bind_and_evaluate(input_tensor, output_tensor);
            } else {
                const auto output_tensor = ml::TensorFloat::Create(output_shape);
                values = bind_and_evaluate(input_tensor, output_tensor);
            }
        }
        const auto evaluate_finished = std::chrono::steady_clock::now();

        if (values.Size() != std::size_t(input_width) * input_height * 12) {
            std::cerr << "unexpected output size: " << values.Size() << '\n';
            return 3;
        }

        float max_abs = 0.0f;
        for (const float value : values) {
            max_abs = std::max(max_abs, std::abs(value));
        }
        std::cout << "External ONNX upscaler WinML smoke passed: values=" << values.Size()
                  << " max_abs=" << max_abs
                  << " spatial=" << (dynamic_spatial ? "dynamic" : "fixed")
                  << " quantization="
                  << (quantization.empty() ? "unspecified" : quantization)
                  << " session_ms="
                  << std::chrono::duration<double, std::milli>(
                         session_ready - load_started).count()
                  << " evaluate_ms="
                  << std::chrono::duration<double, std::milli>(
                         evaluate_finished - evaluate_started).count()
                  << '\n';

        const auto unique_suffix = std::to_wstring(static_cast<long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        const std::filesystem::path image_path =
            std::filesystem::temp_directory_path() /
            (std::wstring(L"tenriff_onnx_upscaler_smoke_") + unique_suffix + L".bmp");
        const std::filesystem::path warm_image_path =
            std::filesystem::temp_directory_path() /
            (std::wstring(L"tenriff_onnx_upscaler_smoke_") + unique_suffix + L"_warm.bmp");
        if (!write_smoke_bmp(image_path) || !write_smoke_bmp(warm_image_path)) {
            std::wcerr << L"could not create WIC smoke image: " << image_path.c_str() << L'\n';
            return 4;
        }

        std::shared_ptr<const tenriff::render::OnnxUpscaleFrame> frame;
        std::shared_ptr<const tenriff::render::OnnxUpscaleFrame> warm_frame;
        std::shared_ptr<const tenriff::render::OnnxUpscaleFrame> video_frame;
        bool first_video_request_accepted = false;
        bool video_overwrite_rejected = false;
        bool next_video_request_accepted = false;
        const auto pipeline_started = std::chrono::steady_clock::now();
        auto pipeline_finished = pipeline_started;
        auto warm_started = pipeline_started;
        auto warm_finished = pipeline_started;
        {
            tenriff::render::OnnxBackgroundUpscaler upscaler(model_path.u8string());
            upscaler.request(image_path.u8string());
            const auto deadline = pipeline_started + std::chrono::seconds(20);
            while (std::chrono::steady_clock::now() < deadline) {
                frame = upscaler.take_ready();
                if (frame) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            pipeline_finished = std::chrono::steady_clock::now();
            if (frame) {
                warm_started = pipeline_finished;
                upscaler.request(warm_image_path.u8string());
                const auto warm_deadline = warm_started + std::chrono::seconds(20);
                while (std::chrono::steady_clock::now() < warm_deadline) {
                    warm_frame = upscaler.take_ready();
                    if (warm_frame) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                warm_finished = std::chrono::steady_clock::now();
            }
            if (warm_frame) {
                std::vector<std::uint8_t> video_bgra(960u * 540u * 4u, 128u);
                for (std::size_t pixel = 0; pixel < 960u * 540u; ++pixel) {
                    video_bgra[pixel * 4u + 3u] = 255u;
                }
                first_video_request_accepted =
                    upscaler.request_bgra("video-smoke|0", 960, 540, video_bgra);
                video_overwrite_rejected =
                    !upscaler.request_bgra("video-smoke|1", 960, 540, video_bgra);
                const auto video_deadline =
                    std::chrono::steady_clock::now() + std::chrono::seconds(20);
                while (std::chrono::steady_clock::now() < video_deadline) {
                    video_frame = upscaler.take_ready();
                    if (video_frame) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                if (video_frame) {
                    next_video_request_accepted =
                        upscaler.request_bgra("video-smoke|2", 960, 540, video_bgra);
                }
            }
        }
        std::error_code remove_error;
        std::filesystem::remove(image_path, remove_error);
        remove_error.clear();
        std::filesystem::remove(warm_image_path, remove_error);
        if (!frame || !warm_frame ||
            frame->width != 8 || frame->height != 8 ||
            frame->bgra.size() != static_cast<std::size_t>(frame->width) * frame->height * 4 ||
            warm_frame->bgra.size() != frame->bgra.size()) {
            std::cerr << "External ONNX upscaler WIC-to-BGRA pipeline did not preserve original x2 size\n";
            return 5;
        }
        if (!first_video_request_accepted || !video_overwrite_rejected ||
            !next_video_request_accepted || !video_frame ||
            video_frame->source_path != "video-smoke|0" ||
            video_frame->width != 1920 || video_frame->height != 1080 ||
            video_frame->bgra.size() != std::size_t(1920) * 1080 * 4) {
            std::cerr << "External ONNX upscaler video backpressure smoke failed\n";
            return 6;
        }
        std::cout << "External ONNX upscaler background pipeline passed: "
                  << frame->width << 'x' << frame->height
                  << " bgra_bytes=" << frame->bgra.size()
                  << " pipeline_ms="
                  << std::chrono::duration<double, std::milli>(
                         pipeline_finished - pipeline_started).count()
                  << " warm_frame_ms="
                  << std::chrono::duration<double, std::milli>(
                         warm_finished - warm_started).count()
                  << " video_backpressure=passed"
                  << '\n';
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::wcerr << L"WinML error 0x" << std::hex
                   << static_cast<std::uint32_t>(error.code()) << L": "
                   << error.message().c_str() << L'\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
