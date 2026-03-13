#pragma once

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace tenriff::app {

struct ProcessMemorySnapshot {
    std::uint64_t working_set_bytes = 0;
    std::uint64_t private_bytes = 0;
};

struct SystemMemorySnapshot {
    std::uint64_t available_bytes = 0;
    std::uint64_t total_bytes = 0;
};

inline ProcessMemorySnapshot query_process_memory_snapshot() {
    ProcessMemorySnapshot snapshot;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             static_cast<DWORD>(sizeof(counters))) != 0) {
        snapshot.working_set_bytes = static_cast<std::uint64_t>(counters.WorkingSetSize);
        snapshot.private_bytes = static_cast<std::uint64_t>(counters.PrivateUsage);
    }
#endif
    return snapshot;
}

inline SystemMemorySnapshot query_system_memory_snapshot() {
    SystemMemorySnapshot snapshot;
#ifdef _WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status) != 0) {
        snapshot.available_bytes = static_cast<std::uint64_t>(status.ullAvailPhys);
        snapshot.total_bytes = static_cast<std::uint64_t>(status.ullTotalPhys);
    }
#endif
    return snapshot;
}

inline std::string format_memory_bytes(std::uint64_t bytes) {
    static constexpr const char* kUnits[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    std::size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1 < (sizeof(kUnits) / sizeof(kUnits[0]))) {
        value /= 1024.0;
        ++unit_index;
    }

    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(unit_index == 0 ? 0 : 1);
    stream << value << ' ' << kUnits[unit_index];
    return stream.str();
}

inline void log_memory_phase(std::string_view owner,
                             std::string_view phase,
                             const ProcessMemorySnapshot& snapshot,
                             std::string_view detail = {}) {
    std::cerr << "[mem][" << owner << "] " << phase << " working_set="
              << format_memory_bytes(snapshot.working_set_bytes)
              << " private=" << format_memory_bytes(snapshot.private_bytes);
    if (!detail.empty()) {
        std::cerr << ' ' << detail;
    }
    std::cerr << std::endl;
}

}  // namespace tenriff::app
