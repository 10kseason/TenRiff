#pragma once

// Independently authored binary-interoperability declarations, not the Steinberg
// SDK. Only externally fixed vtable slots, numeric selectors and packed layouts
// are represented here. Reference: OpenMPT ASIO::Modern ASIOCore.hpp and
// ASIOConfig.hpp (v0.12.11). No SDK implementation or dependency is included.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <unknwn.h>
#include <cstdint>

namespace tenriff::audio::asio {
using Result = int32_t;
constexpr Result ok = 0;
constexpr Result success = 0x3f4847a0;
constexpr bool succeeded(Result r) { return r == ok || r == success; }

#pragma pack(push, 4)
struct Counter { uint32_t high = 0, low = 0; };
struct TimeInfo {
    double speed = 0;
    Counter system_time, sample_position;
    double sample_rate = 0;
    uint32_t flags = 0;
    char reserved[12]{};
};
struct TimeCode { double speed = 0; Counter samples; uint32_t flags = 0; char reserved[64]{}; };
struct Time { int32_t reserved[4]{}; TimeInfo info; TimeCode code; };
struct Callbacks {
    void (__cdecl* buffer_switch)(int32_t, int32_t) = nullptr;
    void (__cdecl* rate_changed)(double) = nullptr;
    int32_t (__cdecl* message)(int32_t, int32_t, void*, double*) = nullptr;
    Time* (__cdecl* time_switch)(Time*, int32_t, int32_t) = nullptr;
};
struct ChannelInfo {
    int32_t channel = 0, input = 0, active = 0, group = 0, sample_type = 0;
    char name[32]{};
};
struct BufferInfo { int32_t input = 0, channel = 0; void* buffers[2]{}; };
struct ClockSource { int32_t index, channel, group, current; char name[32]; };
#pragma pack(pop)
static_assert(sizeof(Counter) == 8 && sizeof(TimeInfo) == 48 && sizeof(Time) == 148);
static_assert(sizeof(ChannelInfo) == 52 && sizeof(BufferInfo) == 8 + 2 * sizeof(void*));

// No virtual destructor: these exact slots follow IUnknown. Release owns disposal.
struct Driver : IUnknown {
    virtual uint32_t __thiscall initialize(void* window) = 0;
    virtual void __thiscall driver_name(char* name) = 0;
    virtual int32_t __thiscall driver_version() = 0;
    virtual void __thiscall error_text(char* text) = 0;
    virtual Result __thiscall start() = 0;
    virtual Result __thiscall stop() = 0;
    virtual Result __thiscall channels(int32_t* inputs, int32_t* outputs) = 0;
    virtual Result __thiscall latencies(int32_t* input, int32_t* output) = 0;
    virtual Result __thiscall buffer_sizes(int32_t* minimum, int32_t* maximum, int32_t* preferred, int32_t* granularity) = 0;
    virtual Result __thiscall can_rate(double rate) = 0;
    virtual Result __thiscall get_rate(double* rate) = 0;
    virtual Result __thiscall set_rate(double rate) = 0;
    virtual Result __thiscall clocks(ClockSource* clocks, int32_t* count) = 0;
    virtual Result __thiscall set_clock(int32_t index) = 0;
    virtual Result __thiscall position(Counter* samples, Counter* timestamp) = 0;
    virtual Result __thiscall channel_info(ChannelInfo* channel) = 0;
    virtual Result __thiscall create_buffers(BufferInfo* buffers, int32_t count, int32_t frames, Callbacks* callbacks) = 0;
    virtual Result __thiscall dispose_buffers() = 0;
    virtual Result __thiscall control_panel() = 0;
    virtual Result __thiscall future(int32_t selector, void* data) = 0;
    virtual Result __thiscall output_ready() = 0;
};
} // namespace tenriff::audio::asio
