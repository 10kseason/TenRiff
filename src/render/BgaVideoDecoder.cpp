#include "render/BgaVideoDecoder.h"

#include "render/BgaVideoFfmpeg.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cctype>

#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include "util/Utf8Compat.h"
#endif

namespace tenriff::render {

namespace {

constexpr std::int64_t kHundredNsPerSecond = 10'000'000;
constexpr std::int64_t kRequestQuantum100ns = kHundredNsPerSecond / 30;

std::string lower_ascii(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

std::int64_t quantize_position(double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0) {
        return 0;
    }
    const double raw = std::min(seconds, 24.0 * 60.0 * 60.0) *
                       static_cast<double>(kHundredNsPerSecond);
    const auto timestamp = static_cast<std::int64_t>(std::llround(raw));
    return (timestamp / kRequestQuantum100ns) * kRequestQuantum100ns;
}

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

bool copy_rgb32_sample(IMFSample* sample,
                       std::uint32_t width,
                       std::uint32_t height,
                       LONG stride,
                       std::vector<std::uint8_t>& out_bgra) {
    if (!sample || width == 0 || height == 0) {
        return false;
    }
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    if (FAILED(sample->ConvertToContiguousBuffer(&buffer)) || !buffer) {
        return false;
    }

    BYTE* bytes = nullptr;
    DWORD max_length = 0;
    DWORD current_length = 0;
    if (FAILED(buffer->Lock(&bytes, &max_length, &current_length)) || !bytes) {
        return false;
    }

    const std::size_t row_bytes = static_cast<std::size_t>(width) * 4;
    const std::size_t absolute_stride = static_cast<std::size_t>(std::abs(stride));
    const bool enough_data = absolute_stride >= row_bytes &&
                             current_length >= absolute_stride * height;
    if (enough_data) {
        out_bgra.resize(row_bytes * height);
        for (std::uint32_t y = 0; y < height; ++y) {
            const std::uint32_t source_y = stride < 0 ? height - 1 - y : y;
            const BYTE* source = bytes + static_cast<std::size_t>(source_y) * absolute_stride;
            std::uint8_t* destination = out_bgra.data() + static_cast<std::size_t>(y) * row_bytes;
            std::copy_n(source, row_bytes, destination);
            for (std::uint32_t x = 0; x < width; ++x) {
                destination[static_cast<std::size_t>(x) * 4 + 3] = 255;
            }
        }
    }
    buffer->Unlock();
    return enough_data;
}

class MfVideoReader {
public:
    void close() {
        reader_.Reset();
        path_.clear();
        width_ = 0;
        height_ = 0;
        stride_ = 0;
        last_timestamp_ = -1;
    }

    bool open(std::string_view path) {
        close();

        Microsoft::WRL::ComPtr<IMFAttributes> attributes;
        if (FAILED(MFCreateAttributes(&attributes, 2)) || !attributes) {
            return false;
        }
        attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, FALSE);

        const std::wstring wide_path = util::path_from_utf8_lossy(path).native();
        if (FAILED(MFCreateSourceReaderFromURL(wide_path.c_str(), attributes.Get(), &reader_)) ||
            !reader_) {
            return false;
        }
        reader_->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
        reader_->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);

        Microsoft::WRL::ComPtr<IMFMediaType> output_type;
        if (FAILED(MFCreateMediaType(&output_type)) || !output_type ||
            FAILED(output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) ||
            FAILED(output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32)) ||
            FAILED(reader_->SetCurrentMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, output_type.Get()))) {
            reader_.Reset();
            return false;
        }

        Microsoft::WRL::ComPtr<IMFMediaType> current_type;
        UINT32 width = 0;
        UINT32 height = 0;
        if (FAILED(reader_->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                                &current_type)) ||
            !current_type ||
            FAILED(MFGetAttributeSize(current_type.Get(), MF_MT_FRAME_SIZE, &width, &height)) ||
            width == 0 || height == 0) {
            reader_.Reset();
            return false;
        }

        LONG stride = 0;
        UINT32 stride_value = 0;
        if (SUCCEEDED(current_type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride_value))) {
            stride = static_cast<LONG>(stride_value);
        } else {
            stride = static_cast<LONG>(width * 4);
        }
        path_ = std::string(path);
        width_ = width;
        height_ = height;
        stride_ = stride;
        return true;
    }

    bool seek(std::int64_t timestamp_100ns) {
        if (!reader_) {
            return false;
        }
        PROPVARIANT position;
        PropVariantInit(&position);
        position.vt = VT_I8;
        position.hVal.QuadPart = std::max<std::int64_t>(0, timestamp_100ns);
        const HRESULT hr = reader_->SetCurrentPosition(GUID_NULL, position);
        PropVariantClear(&position);
        if (SUCCEEDED(hr)) {
            last_timestamp_ = -1;
        }
        return SUCCEEDED(hr);
    }

    std::shared_ptr<BgaVideoFrame> read_at(std::int64_t target_100ns) {
        if (!reader_) {
            return {};
        }
        if (last_timestamp_ >= 0 && target_100ns + kRequestQuantum100ns < last_timestamp_ &&
            !seek(target_100ns)) {
            return {};
        }
        if (last_timestamp_ < 0 && target_100ns > kHundredNsPerSecond / 2 &&
            !seek(target_100ns)) {
            return {};
        }

        std::shared_ptr<BgaVideoFrame> latest;
        for (int reads = 0; reads < 240; ++reads) {
            DWORD stream_index = 0;
            DWORD flags = 0;
            LONGLONG timestamp = 0;
            Microsoft::WRL::ComPtr<IMFSample> sample;
            const HRESULT hr = reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                                   0,
                                                   &stream_index,
                                                   &flags,
                                                   &timestamp,
                                                   &sample);
            if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR) != 0) {
                return latest;
            }
            if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
                return latest;
            }
            if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0) {
                Microsoft::WRL::ComPtr<IMFMediaType> current_type;
                UINT32 width = 0;
                UINT32 height = 0;
                if (SUCCEEDED(reader_->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                                            &current_type)) &&
                    current_type &&
                    SUCCEEDED(MFGetAttributeSize(current_type.Get(), MF_MT_FRAME_SIZE,
                                                 &width, &height)) &&
                    width > 0 && height > 0) {
                    width_ = width;
                    height_ = height;
                    UINT32 stride_value = width * 4;
                    current_type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride_value);
                    stride_ = static_cast<LONG>(stride_value);
                }
            }
            if (!sample) {
                continue;
            }

            auto frame = std::make_shared<BgaVideoFrame>();
            frame->source_path = path_;
            frame->timestamp_100ns = timestamp;
            frame->width = width_;
            frame->height = height_;
            if (!copy_rgb32_sample(sample.Get(), width_, height_, stride_, frame->bgra)) {
                return latest;
            }
            last_timestamp_ = timestamp;
            latest = std::move(frame);
            if (timestamp >= target_100ns) {
                return latest;
            }
        }
        return latest;
    }

    [[nodiscard]] const std::string& path() const { return path_; }

private:
    Microsoft::WRL::ComPtr<IMFSourceReader> reader_;
    std::string path_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    LONG stride_ = 0;
    std::int64_t last_timestamp_ = -1;
};
#endif

}  // namespace

struct BgaVideoDecoder::Impl {
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    bool stop = false;
    bool has_pending = false;
    std::string requested_path;
    std::int64_t requested_timestamp_100ns = -1;
    std::string pending_path;
    std::int64_t pending_timestamp_100ns = 0;
    std::uint64_t request_id = 0;
    std::uint64_t pending_id = 0;
    std::shared_ptr<const BgaVideoFrame> ready;

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

    void run() {
#ifdef _WIN32
        ThreadComInit com;
        if (!com.ok()) {
            std::cerr << "[BGA Video] COM initialization failed; video BGA is disabled.\n";
            return;
        }
        const HRESULT mf_hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
        if (FAILED(mf_hr)) {
            std::cerr << "[BGA Video] Media Foundation startup failed; video BGA is disabled.\n";
            return;
        }
        MfVideoReader reader;
        BgaVideoFfmpegReader ffmpeg_reader;
        std::string failed_path;
        auto failed_retry_after = std::chrono::steady_clock::time_point::min();
        while (true) {
            std::string path;
            std::int64_t timestamp_100ns = 0;
            std::uint64_t id = 0;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [this]() { return stop || has_pending; });
                if (stop) {
                    break;
                }
                path = pending_path;
                timestamp_100ns = pending_timestamp_100ns;
                id = pending_id;
                has_pending = false;
            }

            if (reader.path() != path && ffmpeg_reader.path() != path) {
                if (failed_path == path &&
                    std::chrono::steady_clock::now() < failed_retry_after) {
                    continue;
                }
                if (reader.open(path)) {
                    ffmpeg_reader.close();
                    failed_path.clear();
                } else if (ffmpeg_reader.open(path)) {
                    failed_path.clear();
                    std::cerr << "[BGA Video] Media Foundation unavailable; using FFmpeg fallback: "
                              << path << '\n';
                } else {
                    failed_path = path;
                    failed_retry_after = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                    std::cerr << "[BGA Video] No decoder could open: " << path << '\n';
                    continue;
                }
            }
            auto frame = reader.path() == path
                ? reader.read_at(timestamp_100ns)
                : ffmpeg_reader.read_at(timestamp_100ns);
            // Media Foundation can accept a container and then fail on its codec.
            // Retry the same frame through FFmpeg before treating it as unavailable.
            if ((!frame || frame->bgra.empty()) && reader.path() == path) {
                reader.close();
                if (ffmpeg_reader.open(path)) {
                    frame = ffmpeg_reader.read_at(timestamp_100ns);
                    std::cerr << "[BGA Video] Media Foundation decode failed; using FFmpeg fallback: "
                              << path << '\n';
                }
            }
            if (!frame || frame->bgra.empty()) {
                continue;
            }
            frame->request_id = id;
            {
                std::lock_guard<std::mutex> lock(mutex);
                // Rendering requests arrive faster than decoding. Requiring the
                // completed request to still be the newest one starves playback
                // forever on a decoder that cannot sustain the render frame rate.
                // A same-path frame is safe to present while the latest position
                // remains queued; path changes and clear() still reject stale BGA.
                if (path == requested_path) {
                    ready = std::move(frame);
                }
            }
        }
        MFShutdown();
#else
        std::unique_lock<std::mutex> lock(mutex);
        wake.wait(lock, [this]() { return stop; });
#endif
    }
};

BgaVideoDecoder::BgaVideoDecoder() : impl_(std::make_unique<Impl>()) {}
BgaVideoDecoder::~BgaVideoDecoder() = default;

void BgaVideoDecoder::request(std::string path, double position_seconds) {
    if (!impl_ || path.empty() || !is_supported_video_path(path)) {
        return;
    }
    const std::int64_t timestamp_100ns = quantize_position(position_seconds);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (path == impl_->requested_path && timestamp_100ns == impl_->requested_timestamp_100ns) {
        return;
    }
    impl_->requested_path = std::move(path);
    impl_->requested_timestamp_100ns = timestamp_100ns;
    impl_->pending_path = impl_->requested_path;
    impl_->pending_timestamp_100ns = timestamp_100ns;
    impl_->pending_id = ++impl_->request_id;
    impl_->has_pending = true;
    impl_->wake.notify_one();
}

std::shared_ptr<const BgaVideoFrame> BgaVideoDecoder::take_ready() {
    if (!impl_) {
        return {};
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return std::exchange(impl_->ready, {});
}

void BgaVideoDecoder::clear() {
    if (!impl_) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->requested_path.clear();
    impl_->requested_timestamp_100ns = -1;
    impl_->pending_path.clear();
    impl_->has_pending = false;
    impl_->ready.reset();
    ++impl_->request_id;
}

bool BgaVideoDecoder::is_supported_video_path(std::string_view path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos ||
        (slash != std::string_view::npos && dot < slash)) {
        return false;
    }
    const std::string extension = lower_ascii(path.substr(dot));
    return extension == ".mpg" || extension == ".mpeg" || extension == ".mp4" ||
           extension == ".m4v" || extension == ".wmv" || extension == ".avi" ||
           extension == ".mov" || extension == ".webm" || extension == ".mkv" ||
           extension == ".flv";
}

}  // namespace tenriff::render
