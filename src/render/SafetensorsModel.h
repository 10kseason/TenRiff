#pragma once

// Safetensors stores weights, not a graph. Convert recognized architectures in
// an optional, isolated Python process, then reuse the existing WinML boundary.
#ifdef _WIN32
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "util/Utf8Compat.h"

namespace tenriff::render::safetensors {

struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

inline std::wstring quote_argument(const std::wstring& value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (const auto ch : value) {
        if (ch == L'\\') { ++slashes; continue; }
        if (ch == L'\"') result.append(slashes * 2 + 1, L'\\');
        else result.append(slashes, L'\\');
        slashes = 0;
        result += ch;
    }
    result.append(slashes * 2, L'\\');
    return result + L'\"';
}

inline std::wstring environment(const wchar_t* name) {
    std::array<wchar_t, 32768> buffer{};
    const auto length = GetEnvironmentVariableW(name, buffer.data(), static_cast<DWORD>(buffer.size()));
    return length && length < buffer.size() ? std::wstring(buffer.data(), length) : std::wstring{};
}

inline std::filesystem::path python_executable() {
    const auto configured = environment(L"TENRIFF_UPSCALER_PYTHON");
    std::error_code error;
    if (!configured.empty()) {
        const std::filesystem::path path(configured);
        if (!path.is_absolute() || !std::filesystem::is_regular_file(path, error) || error)
            throw std::runtime_error("TENRIFF_UPSCALER_PYTHON must name an existing absolute Python executable");
        return path;
    }
    std::array<wchar_t, 32768> buffer{};
    // Search PATH explicitly, never the model directory or process current directory.
    const auto paths = environment(L"PATH");
    for (const auto* name : {L"python.exe", L"py.exe"}) {
        const auto length = SearchPathW(paths.c_str(), name, nullptr,
                                       static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
        if (length && length < buffer.size()) return std::filesystem::path(std::wstring(buffer.data(), length));
    }
    throw std::runtime_error("Safetensors needs a local Python with torch, safetensors, onnx and onnxruntime; set TENRIFF_UPSCALER_PYTHON or use ONNX");
}

inline DWORD run_converter(const std::filesystem::path& executable, const std::vector<std::wstring>& arguments,
                           const std::function<bool()>& cancelled,
                           std::chrono::milliseconds timeout = std::chrono::seconds(120)) {
    if (cancelled()) throw std::runtime_error("Safetensors conversion cancelled");
    std::wstring command = quote_argument(executable.native());
    for (const auto& argument : arguments) command += L" " + quote_argument(argument);
    Handle job;
    job.value = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        throw std::runtime_error("Safetensors process isolation unavailable");
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    // No shell, inherited handles, interactive window, user-site imports, or model code.
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &info))
        throw std::runtime_error("Safetensors Python process could not start");
    Handle process, thread;
    process.value = info.hProcess;
    thread.value = info.hThread;
    if (!AssignProcessToJobObject(job.value, process.value) || ResumeThread(thread.value) == static_cast<DWORD>(-1)) {
        TerminateProcess(process.value, 1);
        WaitForSingleObject(process.value, 1000);
        throw std::runtime_error("Safetensors process isolation failed");
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    DWORD wait = WAIT_TIMEOUT;
    while ((wait = WaitForSingleObject(process.value, 25)) == WAIT_TIMEOUT) {
        const bool stopped = cancelled();
        if (stopped || std::chrono::steady_clock::now() >= deadline) {
            TerminateJobObject(job.value, 1);
            WaitForSingleObject(process.value, 1000);
            throw std::runtime_error(stopped ? "Safetensors conversion cancelled" : "Safetensors conversion exceeded 120 seconds; use a pre-exported ONNX");
        }
    }
    DWORD code = 1;
    if (wait != WAIT_OBJECT_0 || !GetExitCodeProcess(process.value, &code))
        throw std::runtime_error("Safetensors conversion process failed");
    return code;
}

inline std::string read_bounded(const std::filesystem::path& path, std::size_t limit) {
    std::ifstream input(path, std::ios::binary);
    std::string text(limit, '\0');
    input.read(text.data(), static_cast<std::streamsize>(limit));
    text.resize(static_cast<std::size_t>(input.gcount()));
    return text;
}

inline std::filesystem::path prepare(const std::filesystem::path& source, const std::function<bool()>& cancelled) {
    // The menu owns three image workers. Serialize their optional conversion to
    // avoid three simultaneous Torch imports/exports and duplicate cache writes.
    static std::timed_mutex conversion_mutex;
    std::unique_lock<std::timed_mutex> guard(conversion_mutex, std::defer_lock);
    while (!guard.try_lock_for(std::chrono::milliseconds(25)))
        if (cancelled()) throw std::runtime_error("Safetensors conversion cancelled");
    if (cancelled()) throw std::runtime_error("Safetensors conversion cancelled");
    std::array<wchar_t, 32768> buffer{};
    const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size()) throw std::runtime_error("Safetensors converter location unavailable");
    const auto script = std::filesystem::path(std::wstring(buffer.data(), length)).parent_path() /
                        L"tools" / L"onnx_upscaler" / L"convert_safetensors.py";
    if (!std::filesystem::is_regular_file(script)) throw std::runtime_error("Safetensors converter script is missing");
    const auto local = environment(L"LOCALAPPDATA");
    if (local.empty()) throw std::runtime_error("Safetensors user cache is unavailable");
    const auto cache = std::filesystem::path(local) / L"TenRiff" / L"upscaler-cache";
    std::filesystem::create_directories(cache);
    static std::atomic<unsigned long> sequence{0};
    const auto name = L"request-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++sequence);
    struct TemporaryResults {
        std::filesystem::path result, error, temporary;
        ~TemporaryResults() {
            std::error_code ec;
            std::filesystem::remove(result, ec);
            std::filesystem::remove(error, ec);
            std::filesystem::remove(temporary, ec);
        }
    } files{cache / (name + L".txt"), cache / (name + L".error.txt"), cache / (name + L".onnx.tmp")};
    const auto executable = python_executable();
    std::vector<std::wstring> arguments;
    if (executable.filename() == L"py.exe") arguments.push_back(L"-3");
    arguments.insert(arguments.end(), {L"-I", script.native(), std::filesystem::absolute(source).native(),
        L"--cache", cache.native(), L"--result", files.result.native(), L"--error", files.error.native(),
        L"--temporary", files.temporary.native()});
    const auto code = run_converter(executable, arguments, cancelled);
    if (code != 0) {
        const auto detail = read_bounded(files.error, 2048);
        throw std::runtime_error(detail.empty() ? "Safetensors conversion failed; check Python dependencies and supported architecture" : detail);
    }
    const auto result = util::path_from_utf8_lossy(read_bounded(files.result, 32768));
    if (result.parent_path() != cache || result.extension() != L".onnx" || !std::filesystem::is_regular_file(result))
        throw std::runtime_error("Safetensors converter returned an invalid cache result");
    return result;
}
}  // namespace tenriff::render::safetensors
#endif
