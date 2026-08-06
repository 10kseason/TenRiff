#include "render/BgaImageLoader.h"

#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "util/Utf8Compat.h"
#endif

namespace tenriff::render {

namespace {

#ifdef _WIN32
struct ThreadComInit {
    HRESULT hr = E_FAIL;
    ThreadComInit() : hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
    ~ThreadComInit() {
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }
    [[nodiscard]] bool ok() const { return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE; }
};

std::shared_ptr<BgaImageFrame> decode_image(IWICImagingFactory* factory,
                                            const std::string& path,
                                            std::uint64_t request_id) {
    auto result = std::make_shared<BgaImageFrame>();
    result->source_path = path;
    result->request_id = request_id;
    if (!factory || path.empty()) {
        return result;
    }

    const std::wstring wide_path = util::path_from_utf8_lossy(path).native();
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(wide_path.c_str(), nullptr, GENERIC_READ,
                                                   WICDecodeMetadataCacheOnLoad, &decoder)) ||
        !decoder) {
        return result;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(decoder->GetFrame(0, &frame)) || !frame ||
        FAILED(factory->CreateFormatConverter(&converter)) || !converter ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                                     WICBitmapDitherTypeNone, nullptr, 0.0f,
                                     WICBitmapPaletteTypeCustom))) {
        return result;
    }

    UINT width = 0;
    UINT height = 0;
    if (FAILED(converter->GetSize(&width, &height)) || width == 0 || height == 0) {
        return result;
    }
    constexpr std::size_t kMaxDecodedImageBytes = 256ull * 1024ull * 1024ull;
    const std::size_t pixel_count = static_cast<std::size_t>(width) * height;
    if (pixel_count > kMaxDecodedImageBytes / 4) {
        return result;
    }

    const UINT stride = width * 4;
    result->bgra.resize(pixel_count * 4);
    if (FAILED(converter->CopyPixels(nullptr, stride,
                                     static_cast<UINT>(result->bgra.size()),
                                     result->bgra.data()))) {
        result->bgra.clear();
        return result;
    }
    result->width = width;
    result->height = height;
    return result;
}
#endif

}  // namespace

struct BgaImageLoader::Impl {
    std::mutex mutex;
    std::condition_variable wake;
    bool stop = false;
    std::uint64_t request_id = 0;
    std::uint64_t pending_id = 0;
    std::string requested_path;
    std::string pending_path;
    std::shared_ptr<const BgaImageFrame> ready;
    std::thread worker;

    Impl() : worker([this]() { run(); }) {}

    ~Impl() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
            pending_path.clear();
        }
        wake.notify_one();
        if (worker.joinable()) {
            worker.join();
        }
    }

    void run() {
#ifdef _WIN32
        ThreadComInit com;
        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        if (!com.ok() || FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                                  CLSCTX_INPROC_SERVER,
                                                  IID_PPV_ARGS(&factory))) ||
            !factory) {
            return;
        }
#endif
        for (;;) {
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

#ifdef _WIN32
            std::shared_ptr<const BgaImageFrame> decoded =
                decode_image(factory.Get(), path, id);
#else
            auto unsupported = std::make_shared<BgaImageFrame>();
            unsupported->source_path = path;
            unsupported->request_id = id;
            std::shared_ptr<const BgaImageFrame> decoded = std::move(unsupported);
#endif
            std::lock_guard<std::mutex> lock(mutex);
            if (id == request_id && path == requested_path) {
                ready = std::move(decoded);
            }
        }
    }
};

BgaImageLoader::BgaImageLoader() : impl_(std::make_unique<Impl>()) {}
BgaImageLoader::~BgaImageLoader() = default;

void BgaImageLoader::request(std::string path) {
    if (!impl_ || path.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (path == impl_->requested_path) {
        return;
    }
    impl_->requested_path = std::move(path);
    ++impl_->request_id;
    impl_->pending_id = impl_->request_id;
    impl_->pending_path = impl_->requested_path;
    impl_->ready.reset();
    impl_->wake.notify_one();
}

std::shared_ptr<const BgaImageFrame> BgaImageLoader::take_ready() {
    if (!impl_) {
        return {};
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return std::move(impl_->ready);
}

void BgaImageLoader::clear() {
    if (!impl_) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    ++impl_->request_id;
    impl_->pending_path.clear();
    impl_->requested_path.clear();
    impl_->ready.reset();
}

}  // namespace tenriff::render
