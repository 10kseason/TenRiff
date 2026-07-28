#include "render/BgaVideoFfmpeg.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

#include "render/BgaVideoDecoder.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

#include "util/Utf8Compat.h"
#endif

namespace tenriff::render {

namespace {

constexpr std::int64_t kHundredNsPerSecond = 10'000'000;
constexpr std::int64_t kFallbackFramesPerSecond = 30;
constexpr std::int64_t kMaximumSequentialFrames = kFallbackFramesPerSecond * 10;
constexpr std::uint32_t kMaximumBmpBytes = 128u * 1024u * 1024u;

std::uint16_t read_u16_le(const std::uint8_t* value) {
    return static_cast<std::uint16_t>(value[0]) |
           (static_cast<std::uint16_t>(value[1]) << 8);
}

std::uint32_t read_u32_le(const std::uint8_t* value) {
    return static_cast<std::uint32_t>(value[0]) |
           (static_cast<std::uint32_t>(value[1]) << 8) |
           (static_cast<std::uint32_t>(value[2]) << 16) |
           (static_cast<std::uint32_t>(value[3]) << 24);
}

std::int32_t read_i32_le(const std::uint8_t* value) {
    const std::uint32_t raw = read_u32_le(value);
    std::int32_t result = 0;
    std::memcpy(&result, &raw, sizeof(result));
    return result;
}

#ifdef _WIN32
std::wstring find_ffmpeg_executable() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = SearchPathW(nullptr, L"ffmpeg.exe", nullptr,
                                     static_cast<DWORD>(buffer.size()),
                                     buffer.data(), nullptr);
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    return std::wstring(buffer.data(), length);
}

bool read_exact(HANDLE pipe, std::uint8_t* destination, std::size_t size) {
    while (size > 0) {
        const DWORD chunk = static_cast<DWORD>(
            std::min<std::size_t>(size, std::numeric_limits<DWORD>::max()));
        DWORD received = 0;
        if (!ReadFile(pipe, destination, chunk, &received, nullptr) || received == 0) {
            return false;
        }
        destination += received;
        size -= received;
    }
    return true;
}

bool read_bmp_frame(HANDLE pipe,
                    std::uint32_t& width,
                    std::uint32_t& height,
                    std::vector<std::uint8_t>& bgra) {
    std::array<std::uint8_t, 14> file_header{};
    if (!read_exact(pipe, file_header.data(), file_header.size()) ||
        file_header[0] != 'B' || file_header[1] != 'M') {
        return false;
    }

    const std::uint32_t file_size = read_u32_le(file_header.data() + 2);
    const std::uint32_t pixel_offset = read_u32_le(file_header.data() + 10);
    if (file_size < 54 || file_size > kMaximumBmpBytes || pixel_offset >= file_size) {
        return false;
    }

    std::vector<std::uint8_t> bmp(file_size);
    std::copy(file_header.begin(), file_header.end(), bmp.begin());
    if (!read_exact(pipe, bmp.data() + file_header.size(), file_size - file_header.size())) {
        return false;
    }

    const std::uint32_t dib_size = read_u32_le(bmp.data() + 14);
    const std::int32_t signed_width = read_i32_le(bmp.data() + 18);
    const std::int32_t signed_height = read_i32_le(bmp.data() + 22);
    const std::uint16_t planes = read_u16_le(bmp.data() + 26);
    const std::uint16_t bits_per_pixel = read_u16_le(bmp.data() + 28);
    const std::uint32_t compression = read_u32_le(bmp.data() + 30);
    if (dib_size < 40 || signed_width <= 0 || signed_height == 0 || planes != 1 ||
        (bits_per_pixel != 24 && bits_per_pixel != 32) ||
        (compression != 0 && compression != 3)) {
        return false;
    }

    const std::uint32_t decoded_width = static_cast<std::uint32_t>(signed_width);
    const std::uint32_t decoded_height = static_cast<std::uint32_t>(
        signed_height < 0 ? -static_cast<std::int64_t>(signed_height) : signed_height);
    if (decoded_width > 16384 || decoded_height > 16384) {
        return false;
    }

    const std::size_t source_row_bytes =
        ((static_cast<std::size_t>(decoded_width) * bits_per_pixel + 31) / 32) * 4;
    const std::size_t pixel_bytes = source_row_bytes * decoded_height;
    if (pixel_offset + pixel_bytes > bmp.size()) {
        return false;
    }

    const std::size_t destination_row_bytes = static_cast<std::size_t>(decoded_width) * 4;
    bgra.resize(destination_row_bytes * decoded_height);
    const std::size_t source_pixel_bytes = bits_per_pixel / 8;
    for (std::uint32_t y = 0; y < decoded_height; ++y) {
        const std::uint32_t source_y = signed_height > 0 ? decoded_height - 1 - y : y;
        const std::uint8_t* source = bmp.data() + pixel_offset +
                                     static_cast<std::size_t>(source_y) * source_row_bytes;
        std::uint8_t* destination = bgra.data() +
                                    static_cast<std::size_t>(y) * destination_row_bytes;
        for (std::uint32_t x = 0; x < decoded_width; ++x) {
            const std::uint8_t* pixel = source + static_cast<std::size_t>(x) * source_pixel_bytes;
            destination[static_cast<std::size_t>(x) * 4 + 0] = pixel[0];
            destination[static_cast<std::size_t>(x) * 4 + 1] = pixel[1];
            destination[static_cast<std::size_t>(x) * 4 + 2] = pixel[2];
            destination[static_cast<std::size_t>(x) * 4 + 3] = 255;
        }
    }
    width = decoded_width;
    height = decoded_height;
    return true;
}
#endif

}  // namespace

struct BgaVideoFfmpegReader::Impl {
    std::string path;
    std::int64_t next_frame_index = 0;

#ifdef _WIN32
    HANDLE process = nullptr;
    HANDLE output = INVALID_HANDLE_VALUE;
    std::wstring ffmpeg_path;

    bool start(std::int64_t first_frame_index) {
        stop();
        if (path.empty()) {
            return false;
        }
        if (ffmpeg_path.empty()) {
            ffmpeg_path = find_ffmpeg_executable();
        }
        if (ffmpeg_path.empty()) {
            return false;
        }

        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;
        HANDLE pipe_read = INVALID_HANDLE_VALUE;
        HANDLE pipe_write = INVALID_HANDLE_VALUE;
        if (!CreatePipe(&pipe_read, &pipe_write, &security, 0) ||
            !SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0)) {
            if (pipe_read != INVALID_HANDLE_VALUE) {
                CloseHandle(pipe_read);
            }
            if (pipe_write != INVALID_HANDLE_VALUE) {
                CloseHandle(pipe_write);
            }
            return false;
        }

        HANDLE null_device = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          &security, OPEN_EXISTING, 0, nullptr);
        if (null_device == INVALID_HANDLE_VALUE) {
            CloseHandle(pipe_read);
            CloseHandle(pipe_write);
            return false;
        }

        const double seek_seconds = static_cast<double>(first_frame_index) /
                                    static_cast<double>(kFallbackFramesPerSecond);
        std::wostringstream seek;
        seek << std::fixed << std::setprecision(6) << seek_seconds;
        const std::wstring input_path = util::path_from_utf8_lossy(path).native();
        std::wstring command = L"\"" + ffmpeg_path +
            L"\" -v error -nostdin -ss " + seek.str() + L" -i \"" + input_path +
            L"\" -map 0:v:0 -an -sn -dn -vf fps=30 -pix_fmt bgra -c:v bmp "
            L"-f image2pipe pipe:1";
        std::vector<wchar_t> command_buffer(command.begin(), command.end());
        command_buffer.push_back(L'\0');

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = null_device;
        startup.hStdOutput = pipe_write;
        startup.hStdError = null_device;
        PROCESS_INFORMATION process_info{};
        const BOOL launched = CreateProcessW(ffmpeg_path.c_str(), command_buffer.data(),
                                             nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                             nullptr, nullptr, &startup, &process_info);
        CloseHandle(pipe_write);
        CloseHandle(null_device);
        if (!launched) {
            CloseHandle(pipe_read);
            return false;
        }

        CloseHandle(process_info.hThread);
        process = process_info.hProcess;
        output = pipe_read;
        next_frame_index = first_frame_index;
        return true;
    }

    void stop() {
        if (output != INVALID_HANDLE_VALUE) {
            CloseHandle(output);
            output = INVALID_HANDLE_VALUE;
        }
        if (process) {
            if (WaitForSingleObject(process, 1000) == WAIT_TIMEOUT) {
                TerminateProcess(process, 1);
                WaitForSingleObject(process, 1000);
            }
            CloseHandle(process);
            process = nullptr;
        }
    }
#else
    bool start(std::int64_t) { return false; }
    void stop() {}
#endif
};

BgaVideoFfmpegReader::BgaVideoFfmpegReader() : impl_(std::make_unique<Impl>()) {}

BgaVideoFfmpegReader::~BgaVideoFfmpegReader() {
    close();
}

bool BgaVideoFfmpegReader::open(std::string_view path) {
    close();
    impl_->path = std::string(path);
    if (!impl_->start(0)) {
        impl_->path.clear();
        return false;
    }
    return true;
}

std::shared_ptr<BgaVideoFrame> BgaVideoFfmpegReader::read_at(std::int64_t target_100ns) {
#ifdef _WIN32
    if (impl_->path.empty() || impl_->output == INVALID_HANDLE_VALUE) {
        return {};
    }
    const auto target_frame = static_cast<std::int64_t>(std::llround(
        static_cast<double>(std::max<std::int64_t>(0, target_100ns)) *
        static_cast<double>(kFallbackFramesPerSecond) /
        static_cast<double>(kHundredNsPerSecond)));
    if (target_frame < impl_->next_frame_index ||
        target_frame - impl_->next_frame_index > kMaximumSequentialFrames) {
        if (!impl_->start(target_frame)) {
            return {};
        }
    }

    std::shared_ptr<BgaVideoFrame> result;
    while (impl_->next_frame_index <= target_frame) {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> bgra;
        if (!read_bmp_frame(impl_->output, width, height, bgra)) {
            return result;
        }
        auto frame = std::make_shared<BgaVideoFrame>();
        frame->source_path = impl_->path;
        frame->timestamp_100ns = impl_->next_frame_index * kHundredNsPerSecond /
                                 kFallbackFramesPerSecond;
        frame->width = width;
        frame->height = height;
        frame->bgra = std::move(bgra);
        ++impl_->next_frame_index;
        result = std::move(frame);
    }
    return result;
#else
    (void)target_100ns;
    return {};
#endif
}

void BgaVideoFfmpegReader::close() {
    if (!impl_) {
        return;
    }
    impl_->stop();
    impl_->path.clear();
    impl_->next_frame_index = 0;
}

const std::string& BgaVideoFfmpegReader::path() const {
    return impl_->path;
}

}  // namespace tenriff::render
