#include "app/CrashLogger.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <DbgHelp.h>
#include <eh.h>
#endif

namespace tenriff::app {

namespace {

#ifdef _WIN32

std::mutex g_crash_log_mutex;
std::atomic<bool> g_crash_log_in_progress{false};

struct CrashLogGuard {
    CrashLogGuard() = default;
    CrashLogGuard(const CrashLogGuard&) = delete;
    CrashLogGuard& operator=(const CrashLogGuard&) = delete;

    ~CrashLogGuard() {
        if (active) {
            g_crash_log_in_progress.store(false);
        }
    }

    bool active = false;
};

std::string utf8_from_wide(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8,
                                             0,
                                             value.data(),
                                             static_cast<int>(value.size()),
                                             nullptr,
                                             0,
                                             nullptr,
                                             nullptr);
    if (required <= 0) {
        return {};
    }

    std::string out(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8,
                        0,
                        value.data(),
                        static_cast<int>(value.size()),
                        out.data(),
                        required,
                        nullptr,
                        nullptr);
    return out;
}

std::string current_timestamp_local() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
    localtime_s(&local_tm, &now_time);

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream stream;
    stream << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S")
           << '.'
           << std::setfill('0')
           << std::setw(3)
           << ms.count();
    return stream.str();
}

std::string timestamp_compact() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
    localtime_s(&local_tm, &now_time);

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream stream;
    stream << std::put_time(&local_tm, "%Y%m%d-%H%M%S")
           << '-'
           << std::setfill('0')
           << std::setw(3)
           << ms.count();
    return stream.str();
}

std::filesystem::path crash_log_directory() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::current_path(ec);
    if (ec || dir.empty()) {
        dir.clear();
        wchar_t buffer[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
        if (length > 0) {
            dir = fs::path(buffer).parent_path();
        }
    }
    if (dir.empty()) {
        dir = fs::temp_directory_path(ec);
    }
    dir /= "logs";
    fs::create_directories(dir, ec);
    return dir;
}

std::filesystem::path make_crash_log_path() {
    std::ostringstream name;
    name << "crash-"
         << timestamp_compact()
         << "-p" << GetCurrentProcessId()
         << "-t" << GetCurrentThreadId()
         << ".log";
    return crash_log_directory() / name.str();
}

std::string current_command_line_utf8() {
    const wchar_t* raw = GetCommandLineW();
    if (!raw) {
        return {};
    }
    return utf8_from_wide(raw);
}

std::string exception_code_name(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND: return "EXCEPTION_FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT: return "EXCEPTION_FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK: return "EXCEPTION_FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW: return "EXCEPTION_FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION: return "EXCEPTION_INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP: return "EXCEPTION_SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
        default: return "UNKNOWN_EXCEPTION_CODE";
    }
}

std::string current_exception_summary() {
    try {
        std::exception_ptr current = std::current_exception();
        if (!current) {
            return "no active C++ exception";
        }
        std::rethrow_exception(current);
    } catch (const std::exception& e) {
        return std::string("std::exception: ") + e.what();
    } catch (...) {
        return "non-std exception";
    }
}

void write_stack_trace(std::ostream& out) {
    void* frames[64] = {};
    const USHORT frame_count = CaptureStackBackTrace(0, static_cast<DWORD>(std::size(frames)), frames, nullptr);

    HANDLE process = GetCurrentProcess();
    static std::once_flag symbols_once;
    static bool symbols_ready = false;
    std::call_once(symbols_once, [&]() {
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        symbols_ready = SymInitialize(process, nullptr, TRUE) == TRUE;
    });

    out << "stack_trace:\n";
    if (frame_count == 0) {
        out << "  <unavailable>\n";
        return;
    }

    std::vector<unsigned char> symbol_buffer(sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer.data());
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    for (USHORT i = 0; i < frame_count; ++i) {
        const DWORD64 address = reinterpret_cast<DWORD64>(frames[i]);
        out << "  [" << i << "] 0x" << std::hex << std::uppercase << address << std::dec;

        DWORD64 displacement = 0;
        if (symbols_ready && SymFromAddr(process, address, &displacement, symbol) == TRUE) {
            out << " " << symbol->Name;

            IMAGEHLP_LINE64 line = {};
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD line_displacement = 0;
            if (SymGetLineFromAddr64(process, address, &line_displacement, &line) == TRUE) {
                out << " (" << line.FileName << ":" << line.LineNumber << ")";
            }
        }

        out << '\n';
    }
}

void write_common_metadata(std::ostream& out, std::string_view kind, std::string_view detail) {
    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);

    out << "timestamp_local: " << current_timestamp_local() << '\n';
    out << "pid: " << GetCurrentProcessId() << '\n';
    out << "tid: " << GetCurrentThreadId() << '\n';
    out << "kind: " << kind << '\n';
    if (!detail.empty()) {
        out << "detail: " << detail << '\n';
    }
    if (!ec) {
        out << "cwd: " << cwd.u8string() << '\n';
    }
    out << "command_line: " << current_command_line_utf8() << '\n';
}

void write_exception_record(std::ostream& out, EXCEPTION_POINTERS* exception_pointers) {
    if (!exception_pointers || !exception_pointers->ExceptionRecord) {
        return;
    }

    const EXCEPTION_RECORD* record = exception_pointers->ExceptionRecord;
    out << "exception_code: 0x"
        << std::hex << std::uppercase << record->ExceptionCode
        << std::dec
        << " (" << exception_code_name(record->ExceptionCode) << ")\n";
    out << "exception_flags: 0x"
        << std::hex << std::uppercase << record->ExceptionFlags
        << std::dec << '\n';
    out << "exception_address: 0x"
        << std::hex << std::uppercase << reinterpret_cast<std::uintptr_t>(record->ExceptionAddress)
        << std::dec << '\n';
    out << "exception_parameters: " << record->NumberParameters << '\n';
    for (DWORD i = 0; i < record->NumberParameters; ++i) {
        out << "  [" << i << "] 0x"
            << std::hex << std::uppercase << record->ExceptionInformation[i]
            << std::dec << '\n';
    }
}

void write_crash_log(std::string_view kind,
                     std::string_view detail,
                     EXCEPTION_POINTERS* exception_pointers) noexcept {
    CrashLogGuard guard;
    if (g_crash_log_in_progress.exchange(true)) {
        return;
    }
    guard.active = true;

    try {
        std::lock_guard<std::mutex> lock(g_crash_log_mutex);
        const std::filesystem::path path = make_crash_log_path();
        std::ofstream out(path, std::ios::out | std::ios::trunc);
        if (!out) {
            return;
        }

        write_common_metadata(out, kind, detail);
        write_exception_record(out, exception_pointers);
        write_stack_trace(out);
        out.flush();
    } catch (...) {
    }
}

LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS* exception_pointers) {
    write_crash_log("unhandled_seh_exception", {}, exception_pointers);
    return EXCEPTION_EXECUTE_HANDLER;
}

void purecall_handler() {
    write_crash_log("purecall", "Pure virtual function call.", nullptr);
    std::_Exit(3);
}

void invalid_parameter_handler(const wchar_t* expression,
                               const wchar_t* function_name,
                               const wchar_t* file_name,
                               unsigned int line_number,
                               std::uintptr_t) {
    std::ostringstream detail;
    detail << "Invalid parameter";
    if (function_name && *function_name != L'\0') {
        detail << " in " << utf8_from_wide(function_name);
    }
    if (expression && *expression != L'\0') {
        detail << " expression=" << utf8_from_wide(expression);
    }
    if (file_name && *file_name != L'\0') {
        detail << " file=" << utf8_from_wide(file_name);
        detail << ":" << line_number;
    }
    write_crash_log("invalid_parameter", detail.str(), nullptr);
    std::_Exit(3);
}

const char* signal_name(int signal_value) {
    switch (signal_value) {
        case SIGABRT: return "SIGABRT";
        case SIGFPE: return "SIGFPE";
        case SIGILL: return "SIGILL";
        case SIGINT: return "SIGINT";
        case SIGSEGV: return "SIGSEGV";
        case SIGTERM: return "SIGTERM";
        default: return "UNKNOWN_SIGNAL";
    }
}

void signal_handler(int signal_value) {
    write_crash_log("signal", signal_name(signal_value), nullptr);
    std::_Exit(128 + signal_value);
}

void terminate_handler() {
    write_crash_log("terminate", current_exception_summary(), nullptr);
    std::_Exit(3);
}

#endif

}  // namespace

void install_crash_handlers() noexcept {
#ifdef _WIN32
    SetUnhandledExceptionFilter(unhandled_exception_filter);
    std::set_terminate(terminate_handler);
    std::signal(SIGABRT, signal_handler);
    std::signal(SIGFPE, signal_handler);
    std::signal(SIGILL, signal_handler);
    std::signal(SIGSEGV, signal_handler);
    std::signal(SIGTERM, signal_handler);
    _set_purecall_handler(purecall_handler);
    _set_invalid_parameter_handler(invalid_parameter_handler);
#endif
}

void write_current_exception_log(std::string_view context) noexcept {
#ifdef _WIN32
    write_crash_log("caught_cpp_exception", context.empty() ? current_exception_summary()
                                                            : std::string(context) + " | " + current_exception_summary(),
                    nullptr);
#else
    (void)context;
#endif
}

}  // namespace tenriff::app
