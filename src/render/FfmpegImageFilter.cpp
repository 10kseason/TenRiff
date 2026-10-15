#include "render/FfmpegImageFilter.h"
#include "render/OnnxUpscaleImage.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace tenriff::render {
#ifdef _WIN32
namespace {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { reset(); }
    void reset() {
        if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
        value = INVALID_HANDLE_VALUE;
    }
    bool valid() const { return value && value != INVALID_HANDLE_VALUE; }
};

std::wstring ffmpeg_path() {
    std::array<wchar_t, 32768> path{};
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length && length < path.size()) {
        auto local = std::filesystem::path(std::wstring(path.data(), length)).parent_path() /
                     L"tools" / L"ffmpeg-deblock" / L"ffmpeg.exe";
        std::error_code ec;
        if (std::filesystem::is_regular_file(local, ec) && !ec) return local.native();
    }
    length = SearchPathW(nullptr, L"ffmpeg.exe", nullptr,
                         static_cast<DWORD>(path.size()), path.data(), nullptr);
    return length && length < path.size() ? std::wstring(path.data(), length) : std::wstring{};
}

struct AttributeList {
    std::vector<unsigned char> storage;
    LPPROC_THREAD_ATTRIBUTE_LIST value = nullptr;
    bool initialize(HANDLE* handles, std::size_t bytes) {
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        storage.resize(size);
        value = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if (!InitializeProcThreadAttributeList(value, 1, 0, &size)) {
            value = nullptr;
            return false;
        }
        return UpdateProcThreadAttribute(value, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                         handles, bytes, nullptr, nullptr) != FALSE;
    }
    ~AttributeList() { if (value) DeleteProcThreadAttributeList(value); }
};
}  // namespace
#endif

DeblockResult deblock_bgra_ffmpeg(std::uint32_t width, std::uint32_t height,
                                std::vector<std::uint8_t>& bgra,
                                const std::function<bool()>& cancelled) {
    if (!onnx_image::valid_size(width, height) || bgra.size() != std::size_t(width) * height * 4)
        return DeblockResult::Failed;
    if (cancelled && cancelled()) return DeblockResult::Cancelled;
#ifdef _WIN32
    const auto executable = ffmpeg_path();
    if (executable.empty()) return DeblockResult::Unavailable;
    std::vector<std::uint8_t> filtered(bgra.size());
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle input_read, input_write, output_read, output_write, null_device;
    if (!CreatePipe(&input_read.value, &input_write.value, &security, 0) ||
        !CreatePipe(&output_read.value, &output_write.value, &security, 0) ||
        !SetHandleInformation(input_write.value, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(output_read.value, HANDLE_FLAG_INHERIT, 0)) return DeblockResult::Failed;
    null_device.value = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                                    OPEN_EXISTING, 0, nullptr);
    if (!null_device.valid()) return DeblockResult::Failed;

    // Restrict inherited handles: three upscaler workers may launch concurrently.
    // Otherwise another child's pipe could keep EOF open after cancellation.
    HANDLE inherited[]{input_read.value, output_write.value, null_device.value};
    AttributeList attributes;
    if (!attributes.initialize(inherited, sizeof(inherited))) return DeblockResult::Failed;
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input_read.value;
    startup.StartupInfo.hStdOutput = output_write.value;
    startup.StartupInfo.hStdError = null_device.value;
    startup.lpAttributeList = attributes.value;
    // Planar RGB avoids YUV chroma subsampling. Filter the full original once,
    // before context tiling; otherwise block detection would restart per tile.
    std::wstring command = L"\"" + executable + L"\" -hide_banner -v error -nostdin"
        L" -threads 1 -filter_threads 1 -f rawvideo -pixel_format bgra -video_size " +
        std::to_wstring(width) + L"x" + std::to_wstring(height) +
        L" -i pipe:0 -vf format=gbrp,deblock=filter=weak:block=8:planes=7,format=bgra"
        L" -frames:v 1 -an -sn -dn -threads 1 -pix_fmt bgra -f rawvideo pipe:1";
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
                        nullptr, nullptr, &startup.StartupInfo, &info)) return DeblockResult::Failed;
    Handle process, process_thread;
    process.value = info.hProcess;
    process_thread.value = info.hThread;
    input_read.reset();
    output_write.reset();
    null_device.reset();
    bool written = false, read = false;
    std::thread writer, reader;
    bool aborted = false, timed_out = false;
    try {
        // Read and write concurrently so images larger than a pipe buffer cannot deadlock.
        writer = std::thread([&]() {
            std::size_t offset = 0;
            while (offset < bgra.size()) {
                DWORD count = 0;
                if (!WriteFile(input_write.value, bgra.data() + offset,
                    static_cast<DWORD>(std::min<std::size_t>(65536, bgra.size() - offset)),
                    &count, nullptr) || count == 0) break;
                offset += count;
            }
            written = offset == bgra.size();
            input_write.reset();
        });
        reader = std::thread([&]() {
            std::size_t offset = 0;
            while (offset < filtered.size()) {
                DWORD count = 0;
                if (!ReadFile(output_read.value, filtered.data() + offset,
                    static_cast<DWORD>(std::min<std::size_t>(65536, filtered.size() - offset)),
                    &count, nullptr) || count == 0) break;
                offset += count;
            }
            unsigned char extra = 0;
            DWORD count = 0;
            ReadFile(output_read.value, &extra, 1, &count, nullptr);
            read = offset == filtered.size() && count == 0;
        });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (WaitForSingleObject(process.value, 25) == WAIT_TIMEOUT) {
            aborted = cancelled && cancelled();
            timed_out = std::chrono::steady_clock::now() >= deadline;
            if (aborted || timed_out) break;
        }
    } catch (...) {
        timed_out = true;
    }
    if (aborted || timed_out) TerminateProcess(process.value, 1);
    WaitForSingleObject(process.value, INFINITE);
    if (writer.joinable()) writer.join();
    if (reader.joinable()) reader.join();
    if (aborted || (cancelled && cancelled())) return DeblockResult::Cancelled;
    DWORD exit_code = 1;
    if (timed_out || !written || !read || !GetExitCodeProcess(process.value, &exit_code) || exit_code)
        return DeblockResult::Failed;
    // FFmpeg operates only on RGB; source transparency is not a compression artifact.
    for (std::size_t i = 3; i < filtered.size(); i += 4) filtered[i] = bgra[i];
    bgra.swap(filtered);
    return DeblockResult::Completed;
#else
    return DeblockResult::Unavailable;
#endif
}
}  // namespace tenriff::render
