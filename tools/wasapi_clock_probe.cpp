// Opt-in diagnostic only: silent shared rendering, no capture, no setting changes.
// GetPosition's clock/QPC pair is documented here (QPC is already in 100 ns units):
// https://learn.microsoft.com/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition
// https://learn.microsoft.com/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getfrequency
// https://learn.microsoft.com/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getcurrentpadding
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT hr, const char* operation) {
    if (FAILED(hr)) {
        char hex[16]{};
        sprintf_s(hex, "%08lx", static_cast<unsigned long>(hr));
        throw std::runtime_error(std::string(operation) + " failed 0x" + hex);
    }
}
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value) CloseHandle(value); }
};
struct Format {
    WAVEFORMATEX* value = nullptr;
    ~Format() { if (value) CoTaskMemFree(value); }
};
struct Stream {
    ComPtr<IAudioClient> client;
    bool started = false;
    ~Stream() { if (started) client->Stop(); }
};
LONGLONG qpc_frequency = 0;
long double qpc100ns() {
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return static_cast<long double>(value.QuadPart) * 10000000.0L / qpc_frequency;
}
struct ClockPair {
    UINT64 position = 0, qpc = 0;
    HRESULT status = E_FAIL;
};
ClockPair position(IAudioClock* clock) {
    ClockPair pair;
    pair.status = clock->GetPosition(&pair.position, &pair.qpc);
    check(pair.status, "IAudioClock::GetPosition");
    return pair;
}
long double at_qpc(const ClockPair& pair, UINT64 frequency, long double qpc) {
    return static_cast<long double>(pair.position) / frequency +
        (qpc - static_cast<long double>(pair.qpc)) / 10000000.0L;
}
struct Row {
    unsigned block = 0, requested_delay_ms = 0, index = 0;
    UINT32 padding = 0, frames = 0;
    UINT64 write_cursor = 0;
    ClockPair before, after;
    long double padding_begin = 0, padding_end = 0, callback_qpc = 0;
    long double buffer_begin = 0, buffer_end = 0, legacy_error_ms = 0, before_error_ms = 0;
    long double clock_pair_residual_ms = 0;
};
double percentile(std::vector<double> values, double quantile) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    const double index = quantile * (values.size() - 1);
    const auto lower = static_cast<std::size_t>(index);
    const auto upper = std::min(lower + 1, values.size() - 1);
    return values[lower] + (values[upper] - values[lower]) * (index - lower);
}
void stats(std::ostream& out, const std::vector<double>& values) {
    out << "{\"p50\":" << percentile(values, .5) << ",\"p95\":" << percentile(values, .95)
        << ",\"p99\":" << percentile(values, .99) << ",\"min\":" << percentile(values, 0)
        << ",\"max\":" << percentile(values, 1) << '}';
}
int run(const std::filesystem::path& directory, unsigned count) {
    if (std::filesystem::exists(directory)) throw std::runtime_error("Output directory must not exist");
    std::filesystem::create_directories(directory);
    LARGE_INTEGER frequency{};
    if (!QueryPerformanceFrequency(&frequency)) throw std::runtime_error("QPC unavailable");
    qpc_frequency = frequency.QuadPart;
    ComPtr<IMMDeviceEnumerator> enumerator;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator)), "Create enumerator");
    ComPtr<IMMDevice> endpoint;
    check(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &endpoint), "Default render endpoint");
    Stream stream;
    check(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(stream.client.GetAddressOf())), "Activate client");
    Format format;
    check(stream.client->GetMixFormat(&format.value), "GetMixFormat");
    const unsigned sample_rate = format.value->nSamplesPerSec;
    check(stream.client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        0, 0, format.value, nullptr), "Initialize silent shared stream");
    Handle event{CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    if (!event.value) throw std::runtime_error("Create event failed");
    check(stream.client->SetEventHandle(event.value), "SetEventHandle");
    ComPtr<IAudioRenderClient> render;
    ComPtr<IAudioClock> clock;
    check(stream.client->GetService(IID_PPV_ARGS(&render)), "Get render service");
    check(stream.client->GetService(IID_PPV_ARGS(&clock)), "Get clock service");
    UINT32 buffer_frames = 0;
    UINT64 clock_frequency = 0;
    check(stream.client->GetBufferSize(&buffer_frames), "GetBufferSize");
    check(clock->GetFrequency(&clock_frequency), "GetFrequency");
    if (!sample_rate || !clock_frequency) throw std::runtime_error("Invalid clock frequency");
    // High-resolution waitable timer changes no system-wide timer resolution.
    // It models a preemption between the two legacy timestamp observations.
    Handle delay_timer{CreateWaitableTimerExW(nullptr, nullptr, 0x00000002, TIMER_ALL_ACCESS)};
    if (!delay_timer.value) throw std::runtime_error("High-resolution waitable timer unavailable");
    BYTE* buffer = nullptr;
    check(render->GetBuffer(buffer_frames, &buffer), "Prime GetBuffer");
    check(render->ReleaseBuffer(buffer_frames, AUDCLNT_BUFFERFLAGS_SILENT), "Prime ReleaseBuffer");
    UINT64 written = buffer_frames;
    check(stream.client->Start(), "Start");
    stream.started = true;
    // Interleave two rounds; no files, allocations, formatting or device-name
    // queries occur while servicing the silent render stream.
    const unsigned delays[] = {0, 1, 3, 0, 1, 3};
    std::vector<Row> rows;
    rows.reserve(6 * count);
    for (unsigned block = 0; block < 6; ++block) {
        unsigned accepted = 0, warmup = 50, attempts = 0;
        while (accepted < count) {
            if (++attempts > count * 5 + 500) throw std::runtime_error("Too few valid callbacks");
            if (WaitForSingleObject(event.value, 1000) != WAIT_OBJECT_0)
                throw std::runtime_error("Audio event timeout");
            Row row;
            row.block = block; row.requested_delay_ms = delays[block]; row.index = accepted;
            row.padding_begin = qpc100ns();
            check(stream.client->GetCurrentPadding(&row.padding), "GetCurrentPadding");
            row.padding_end = qpc100ns();
            if (row.padding >= buffer_frames) continue;
            row.frames = buffer_frames - row.padding;
            row.write_cursor = written;
            row.before = position(clock.Get());
            if (!warmup && row.requested_delay_ms) {
                LARGE_INTEGER duration{};
                duration.QuadPart = -static_cast<LONGLONG>(row.requested_delay_ms) * 10000;
                if (!SetWaitableTimer(delay_timer.value, &duration, 0, nullptr, nullptr, FALSE) ||
                    WaitForSingleObject(delay_timer.value, 1000) != WAIT_OBJECT_0)
                    throw std::runtime_error("Delay timer failed");
            }
            row.buffer_begin = qpc100ns();
            check(render->GetBuffer(row.frames, &buffer), "GetBuffer");
            row.buffer_end = qpc100ns();
            // Equivalent location to GameSession's QPC read at callback entry.
            row.callback_qpc = qpc100ns();
            row.after = position(clock.Get());
            check(render->ReleaseBuffer(row.frames, AUDCLNT_BUFFERFLAGS_SILENT), "ReleaseBuffer");
            written += row.frames;
            if (warmup) { --warmup; continue; }
            const long double legacy_seconds = static_cast<long double>(row.write_cursor - row.padding) / sample_rate;
            row.legacy_error_ms = (legacy_seconds - at_qpc(row.after, clock_frequency, row.callback_qpc)) * 1000;
            row.before_error_ms = (legacy_seconds - at_qpc(row.before, clock_frequency, row.padding_end)) * 1000;
            row.clock_pair_residual_ms = (at_qpc(row.before, clock_frequency, row.callback_qpc) -
                at_qpc(row.after, clock_frequency, row.callback_qpc)) * 1000;
            rows.push_back(row);
            ++accepted;
        }
    }
    check(stream.client->Stop(), "Stop");
    stream.started = false;
    std::ofstream csv(directory / "samples.csv");
    csv << std::setprecision(12) << "block,requested_delay_ms,index,padding,frames,write_cursor,padding_duration_ms,padding_to_callback_ms,get_buffer_ms,legacy_minus_paired_ms,legacy_at_padding_minus_paired_ms,clock_pair_extrapolation_residual_ms,clock_before_status,clock_after_status,clock_before_position,clock_before_qpc_100ns,clock_after_position,clock_after_qpc_100ns,callback_qpc_100ns\n";
    for (const auto& row : rows) {
        csv << row.block << ',' << row.requested_delay_ms << ',' << row.index << ',' << row.padding << ',' << row.frames << ',' << row.write_cursor << ','
            << (row.padding_end-row.padding_begin)/10000 << ',' << (row.callback_qpc-row.padding_end)/10000 << ',' << (row.buffer_end-row.buffer_begin)/10000 << ','
            << row.legacy_error_ms << ',' << row.before_error_ms << ',' << row.clock_pair_residual_ms << ',' << row.before.status << ',' << row.after.status << ','
            << row.before.position << ',' << row.before.qpc << ',' << row.after.position << ',' << row.after.qpc << ',' << row.callback_qpc << '\n';
    }
    std::ofstream json(directory / "summary.json");
    json << std::setprecision(10) << "{\"scope\":\"silent WASAPI shared default render endpoint; no capture or settings writes\",\"sample_rate\":" << sample_rate
        << ",\"channels\":" << format.value->nChannels << ",\"buffer_frames\":" << buffer_frames
        << ",\"clock_frequency\":" << clock_frequency << ",\"qpc_frequency\":" << qpc_frequency << ",\"samples_per_block\":" << count << ",\"blocks\":[";
    for (unsigned block = 0; block < 6; ++block) {
        std::vector<double> legacy, at_padding, delay, residual, buffer_duration;
        unsigned inaccurate = 0, empty_padding = 0;
        for (const auto& row : rows) if (row.block == block) {
            if (row.before.status != S_OK || row.after.status != S_OK) { ++inaccurate; continue; }
            legacy.push_back(static_cast<double>(row.legacy_error_ms));
            at_padding.push_back(static_cast<double>(row.before_error_ms));
            delay.push_back(static_cast<double>((row.callback_qpc-row.padding_end)/10000));
            residual.push_back(static_cast<double>(row.clock_pair_residual_ms));
            buffer_duration.push_back(static_cast<double>((row.buffer_end-row.buffer_begin)/10000));
            if (!row.padding) ++empty_padding;
        }
        if (block) json << ',';
        json << "{\"block\":" << block << ",\"requested_delay_ms\":" << delays[block] << ",\"accurate_pairs\":" << legacy.size()
            << ",\"inaccurate_pairs\":" << inaccurate << ",\"empty_padding_observations\":" << empty_padding << ",\"legacy_minus_paired_ms\":";
        stats(json, legacy); json << ",\"legacy_at_padding_minus_paired_ms\":"; stats(json, at_padding);
        json << ",\"padding_to_callback_ms\":"; stats(json, delay);
        json << ",\"clock_pair_extrapolation_residual_ms\":"; stats(json, residual);
        json << ",\"get_buffer_ms\":"; stats(json, buffer_duration); json << '}';
    }
    json << "]}\n";
    if (!csv || !json) throw std::runtime_error("Writing evidence failed");
    std::cout << "Silent shared clock probe completed: " << rows.size() << " paired observations\n";
    return 0;
}
}  // namespace
int wmain(int argc, wchar_t** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: wasapi_clock_probe NEW_OUTPUT_DIRECTORY [SAMPLES_PER_BLOCK=300]\n";
        return 2;
    }
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized)) { std::cerr << "COM initialization failed\n"; return 2; }
    int result = 0;
    try {
        const unsigned count = argc == 3 ? std::stoul(argv[2]) : 300;
        if (count < 100 || count > 1000) throw std::runtime_error("Samples must be 100..1000");
        result = run(argv[1], count);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; result = 1;
    }
    CoUninitialize();
    return result;
}
