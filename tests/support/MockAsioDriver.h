#pragma once
#include "audio/AsioBackend.h"
#include "audio/AsioDriverAbi.h"
#include <array>
#include <cstring>
#include <memory>
#include <thread>

namespace tenriff::tests {
using namespace tenriff::audio;
struct MockState {
    asio::Callbacks callbacks{};
    std::array<std::array<std::array<uint8_t, 4096>, 2>, 2> output{};
    uint64_t position = 100000;
    int32_t format = 16, outputs = 2, latency = 256;
    double rate = 44100;
    uint32_t frames = 0;
    int unavailable_positions = 0;
    asio::Result set_rate_result = 0;
    int set_rate_calls = 0;
    bool ignore_set_rate = false;
    bool initialized = false, created = false, started = false, fail_create = false, fail_start = false;
    int starts = 0, stops = 0, disposals = 0, releases = 0;
    std::thread::id owner;
    bool correct_owner = true;
    void check_owner() { if (owner != std::this_thread::get_id()) correct_owner = false; }
    void tick(int buffer = 0, bool with_time = false) {
        if (with_time) {
            asio::Time time{};
            time.info.flags = 2;
            time.info.sample_position = {uint32_t(position >> 32), uint32_t(position)};
            callbacks.time_switch(&time, buffer, 1);
        } else callbacks.buffer_switch(buffer, 1);
        position += frames;
    }
};
struct MockDriver final : asio::Driver {
    std::shared_ptr<MockState> state;
    explicit MockDriver(std::shared_ptr<MockState> s) : state(std::move(s)) { state->owner = std::this_thread::get_id(); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { state->check_owner(); ++state->releases; delete this; return 0; }
    uint32_t __thiscall initialize(void*) override { state->check_owner(); state->initialized = true; return 1; }
    void __thiscall driver_name(char*) override {}
    int32_t __thiscall driver_version() override { return 1; }
    void __thiscall error_text(char* text) override { std::strcpy(text, "mock error"); }
    asio::Result __thiscall start() override { state->check_owner(); ++state->starts; state->started = !state->fail_start; return state->fail_start ? -999 : 0; }
    asio::Result __thiscall stop() override { state->check_owner(); ++state->stops; state->started = false; return 0; }
    asio::Result __thiscall channels(int32_t* in, int32_t* out) override { *in = 8; *out = state->outputs; return 0; }
    asio::Result __thiscall latencies(int32_t* in, int32_t* out) override { *in = 128; *out = state->latency; return 0; }
    asio::Result __thiscall buffer_sizes(int32_t* min, int32_t* max, int32_t* pref, int32_t* gran) override { *min = 64; *max = 512; *pref = 256; *gran = -1; return 0; }
    asio::Result __thiscall can_rate(double rate) override { return rate == 44100 || rate == 48000 ? 0 : -997; }
    asio::Result __thiscall get_rate(double* rate) override { *rate = state->rate; return 0; }
    asio::Result __thiscall set_rate(double rate) override {
        ++state->set_rate_calls;
        if (asio::succeeded(state->set_rate_result) && !state->ignore_set_rate) state->rate = rate;
        return state->set_rate_result;
    }
    asio::Result __thiscall clocks(asio::ClockSource*, int32_t*) override { return -1000; }
    asio::Result __thiscall set_clock(int32_t) override { return -1000; }
    asio::Result __thiscall position(asio::Counter* position, asio::Counter*) override {
        if (state->unavailable_positions > 0) { --state->unavailable_positions; return -1000; }
        *position = {uint32_t(state->position >> 32), uint32_t(state->position)};
        return 0;
    }
    asio::Result __thiscall channel_info(asio::ChannelInfo* info) override { info->sample_type = state->format; return 0; }
    asio::Result __thiscall create_buffers(asio::BufferInfo* buffers, int32_t count, int32_t frames, asio::Callbacks* callbacks) override {
        state->check_owner();
        if (state->fail_create) return -999;
        if (count != 2 || buffers[0].input || buffers[1].input || buffers[0].channel != 0 || buffers[1].channel != 1) return -998;
        state->callbacks = *callbacks; state->frames = frames; state->created = true;
        for (int channel = 0; channel < 2; ++channel) for (int buffer = 0; buffer < 2; ++buffer)
            buffers[channel].buffers[buffer] = state->output[channel][buffer].data();
        return 0;
    }
    asio::Result __thiscall dispose_buffers() override { state->check_owner(); ++state->disposals; state->created = false; return 0; }
    asio::Result __thiscall control_panel() override { return -1000; }
    asio::Result __thiscall future(int32_t, void*) override { return -1000; }
    asio::Result __thiscall output_ready() override { return 0; }
};
inline AsioBackend::DriverFactory mock_factory(const std::shared_ptr<MockState>& state) {
    return [state](const std::string&) { return new MockDriver(state); };
}
} // namespace tenriff::tests
