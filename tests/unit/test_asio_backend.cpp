#include "doctest/doctest.h"
#include "audio/AsioBackend.h"
#include "audio/AsioDriverAbi.h"
#include "audio/AudioThread.h"
#include "../support/MockAsioDriver.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <future>
#include <limits>
#include <memory>
#include <thread>

namespace {
using namespace tenriff::audio;
using namespace tenriff::tests;
auto silent = [](float*, uint32_t, int64_t, int64_t) {};
}

TEST_CASE("ASIO buffer negotiation honors fixed stepped and power-of-two driver sizes") {
    CHECK(asio_choose_buffer(128, 64, 1024, 256, 0) == 256);
    CHECK(asio_choose_buffer(180, 64, 1024, 256, -1) == 128);
    CHECK(asio_choose_buffer(192, 64, 1024, 256, -1) == 256);
    CHECK(asio_choose_buffer(200, 96, 512, 256, 32) == 192);
    CHECK(asio_choose_buffer(1, 96, 512, 256, 32) == 96);
    CHECK(asio_choose_buffer(128, 0, 1024, 256, -1) == 0);
    CHECK(asio_choose_buffer(128, 64, 1000000, 256, -1) == 0);
    CHECK(asio_choose_buffer(128, 64, 512, 1024, 0) == 0);
}

TEST_CASE("ASIO converts stereo float to planar device PCM with clipping endian and valid-bit handling") {
    const float input[] = {-2.f, 1.f, 0.5f, -0.5f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
    std::array<uint8_t, 24> out{};
    REQUIRE(asio_encode_channel(out.data(), input, 3, 0, 16));
    CHECK(out[0] == 0x00); CHECK(out[1] == 0x80); CHECK(out[2] == 0x00); CHECK(out[3] == 0x40);
    CHECK(out[4] == 0); CHECK(out[5] == 0);
    REQUIRE(asio_encode_channel(out.data(), input, 3, 1, 1));
    CHECK(out[0] == 0x7f); CHECK(out[1] == 0xff); CHECK(out[2] == 0xff);
    CHECK(out[3] == 0xc0); CHECK(out[4] == 0); CHECK(out[5] == 0);
    REQUIRE(asio_encode_channel(out.data(), input, 3, 1, 27));
    CHECK(out[0] == 0xff); CHECK(out[1] == 0xff); CHECK(out[2] == 0x7f); CHECK(out[3] == 0);
    CHECK(out[4] == 0); CHECK(out[5] == 0); CHECK(out[6] == 0xc0); CHECK(out[7] == 0xff);
    REQUIRE(asio_encode_channel(out.data(), input, 3, 0, 19));
    float value = 0; std::memcpy(&value, out.data() + 4, sizeof(value)); CHECK(value == 0.5f);
    REQUIRE(asio_encode_channel(out.data(), input, 3, 1, 4));
    CHECK(out[0] == 0x3f); CHECK(out[1] == 0xf0);
    CHECK_FALSE(asio_encode_channel(out.data(), input, 3, 0, 32));
    CHECK_FALSE(asio_encode_channel(out.data(), input, 3, 2, 16));
    for (const int type : {0,1,2,3,4,8,9,10,11,16,17,18,19,20,24,25,26,27}) CHECK(asio_sample_bytes(type) > 0);
}

TEST_CASE("ASIO native protocol negotiates stereo buffers renders clocked callbacks and restarts") {
    auto state = std::make_shared<MockState>();
    AsioBackend backend(mock_factory(state));
    AudioConfig config; config.backend = AudioBackend::ASIO; config.sample_rate = 48000;
    std::vector<int64_t> writes, playbacks;
    auto render = [&](float* output, uint32_t frames, int64_t write, int64_t playback) {
        writes.push_back(write); playbacks.push_back(playback);
        for (uint32_t i = 0; i < frames; ++i) { output[i*2] = 0.25f; output[i*2+1] = -0.5f; }
    };
    REQUIRE(backend.initialize(config, render) == AudioResult::Success);
    CHECK(state->initialized); CHECK(state->created); CHECK(backend.sample_rate() == 48000); CHECK(backend.buffer_frames() == 128);
    CHECK(state->owner != std::this_thread::get_id());
    REQUIRE(backend.start() == AudioResult::Success);
    state->tick(0); state->tick(1, true); state->tick(0); state->tick(1, true);
    CHECK((writes == std::vector<int64_t>{0,128,256,384}));
    CHECK((playbacks == std::vector<int64_t>{0,0,0,128}));
    CHECK(backend.playback_samples() == 128);
    CHECK(state->output[0][0][1] == 0x20); CHECK(state->output[1][0][1] == 0xc0);
    backend.stop(); CHECK_FALSE(backend.is_playing()); CHECK(state->stops == 1);
    REQUIRE(backend.start() == AudioResult::Success);
    state->tick(); CHECK(writes.back() == 0); CHECK(state->starts == 2);
    backend.shutdown(); CHECK(state->stops == 2); CHECK(state->disposals == 1); CHECK(state->releases == 1); CHECK(state->correct_owner);
}

TEST_CASE("ASIO reset rate and buffer changes silence callbacks and require reopening") {
    for (const int selector : {3,4,5,6,0}) {
        auto state = std::make_shared<MockState>();
        AsioBackend backend(mock_factory(state));
        int called = 0;
        REQUIRE(backend.initialize(AudioConfig{}, [&](float* out, uint32_t n, int64_t, int64_t) { ++called; std::fill(out,out+n*2,1.f); }) == AudioResult::Success);
        REQUIRE(backend.start() == AudioResult::Success);
        state->tick(); REQUIRE(called == 1);
        if (selector) CHECK(state->callbacks.message(selector, 0, nullptr, nullptr) == 1);
        else state->callbacks.rate_changed(96000);
        CHECK(backend.has_runtime_error()); CHECK_FALSE(backend.is_playing()); CHECK_FALSE(backend.error_message().empty());
        state->tick(); CHECK(called == 1); CHECK(state->output[0][0][1] == 0);
        backend.stop(); CHECK(backend.start() != AudioResult::Success);
        REQUIRE(backend.initialize(AudioConfig{}, silent) == AudioResult::Success);
        CHECK_FALSE(backend.has_runtime_error());
    }
}

TEST_CASE("ASIO init and start failures release native resources and cannot select WASAPI") {
    for (const int failure : {0,1,2,3,4}) {
        auto state = std::make_shared<MockState>();
        if (failure == 0) state->outputs = 1;
        if (failure == 1) state->format = 32;
        if (failure == 2) state->fail_create = true;
        if (failure == 3) state->latency = -1;
        if (failure == 4) state->fail_start = true;
        AsioBackend backend(mock_factory(state));
        auto result = backend.initialize(AudioConfig{}, silent);
        if (failure == 4) { REQUIRE(result == AudioResult::Success); CHECK(backend.start() != AudioResult::Success); backend.shutdown(); }
        else CHECK(result != AudioResult::Success);
        CHECK_FALSE(backend.is_playing()); CHECK(state->releases == 1); CHECK_FALSE(backend.error_message().empty());
    }
    AudioThread thread; AudioConfig bad; bad.backend = AudioBackend::ASIO; bad.asio_driver = "{00000000-0000-0000-0000-000000000000}";
    CHECK(thread.initialize(bad, silent) == AudioResult::DeviceNotFound);
    CHECK(thread.start() != AudioResult::Success); CHECK_FALSE(thread.is_running()); CHECK_FALSE(thread.error_message().empty());
}

TEST_CASE("ASIO invalid configuration and competing streams fail without leaking callback ownership") {
    auto state = std::make_shared<MockState>();
    AsioBackend first(mock_factory(state));
    AudioConfig config; config.sample_rate = 0;
    CHECK(first.initialize(config,silent) == AudioResult::FormatNotSupported);
    CHECK_FALSE(state->initialized);
    config.sample_rate = 96000;
    CHECK(first.initialize(config,silent) == AudioResult::FormatNotSupported); CHECK(state->releases == 1);
    REQUIRE(first.initialize(AudioConfig{},silent) == AudioResult::Success);
    auto second_state = std::make_shared<MockState>();
    AsioBackend second(mock_factory(second_state));
    CHECK(second.initialize(AudioConfig{},silent) == AudioResult::DeviceInUse);
    first.shutdown();
    CHECK(second.initialize(AudioConfig{},silent) == AudioResult::Success);
}

TEST_CASE("ASIO teardown waits for an in-flight native callback and silences its final output") {
    auto state = std::make_shared<MockState>();
    AsioBackend backend(mock_factory(state));
    std::promise<void> entered, release;
    auto released = release.get_future();
    REQUIRE(backend.initialize(AudioConfig{}, [&](float* out, uint32_t count, int64_t, int64_t) {
        entered.set_value(); released.wait(); std::fill(out, out + count * 2, 1.f);
    }) == AudioResult::Success);
    REQUIRE(backend.start() == AudioResult::Success);
    std::thread callback([&] { state->tick(); });
    entered.get_future().wait();
    auto stop = std::async(std::launch::async, [&] { backend.stop(); });
    const bool waited = stop.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout;
    release.set_value(); callback.join(); stop.get();
    CHECK(waited); CHECK_FALSE(backend.is_playing()); CHECK(state->output[0][0][1] == 0);
    backend.shutdown(); CHECK(state->correct_owner); CHECK(state->releases == 1);
}

TEST_CASE("ASIO device sample counters cross 32-bit boundaries and backwards resets fail safely") {
    auto state = std::make_shared<MockState>(); state->position = 0xfffffff0ull;
    AsioBackend backend(mock_factory(state));
    int calls = 0; int64_t last = -1;
    REQUIRE(backend.initialize(AudioConfig{}, [&](float*, uint32_t, int64_t write, int64_t) { ++calls; last = write; }) == AudioResult::Success);
    REQUIRE(backend.start() == AudioResult::Success);
    state->tick(); state->tick(1, true); CHECK(last == 128);
    state->position = 0; state->tick(); CHECK(calls == 2); CHECK(backend.has_runtime_error());
    CHECK_FALSE(backend.is_playing());
}

TEST_CASE("ASIO first valid device position joins the existing fallback clock without a false reset") {
    for (const bool time_info : {false, true}) {
        auto state = std::make_shared<MockState>();
        state->unavailable_positions = 3;
        AsioBackend backend(mock_factory(state));
        std::vector<int64_t> writes;
        REQUIRE(backend.initialize(AudioConfig{}, [&](float*, uint32_t, int64_t write, int64_t) { writes.push_back(write); }) == AudioResult::Success);
        REQUIRE(backend.start() == AudioResult::Success);
        state->tick(); state->tick(); state->tick();
        // Some drivers start reporting their clock only now, even from a value
        // smaller than the stream's existing fallback cursor.
        state->position = time_info ? 0 : 100000;
        const uint64_t origin = state->position;
        state->tick(1, time_info); state->tick(); state->tick(1, time_info);
        CHECK((writes == std::vector<int64_t>{0,128,256,384,512,640}));
        CHECK(backend.playback_samples() == 384);
        CHECK_FALSE(backend.has_runtime_error());
        CHECK(backend.is_playing());
        state->position = origin;
        state->tick();
        CHECK(backend.has_runtime_error());
        CHECK_FALSE(backend.is_playing());
        CHECK(writes.size() == 6);
    }
}

TEST_CASE("ASIO fixed-rate drivers skip redundant setters but mismatches retain exact negotiation errors") {
    auto fixed = std::make_shared<MockState>();
    fixed->rate = 48000; fixed->set_rate_result = -1000;
    AsioBackend backend(mock_factory(fixed));
    AudioConfig config; config.sample_rate = 48000;
    REQUIRE(backend.initialize(config, silent) == AudioResult::Success);
    CHECK(fixed->set_rate_calls == 0); CHECK(backend.sample_rate() == 48000);
    backend.shutdown();
    config.sample_rate = 44100;
    CHECK(backend.initialize(config, silent) == AudioResult::FormatNotSupported);
    CHECK(fixed->set_rate_calls == 1);
    const auto error = backend.error_message();
    CHECK(error.find("requested=44100 current=48000 get_rate(before)=0 can_rate=0 set_rate=-1000 actual=48000 get_rate(after)=0") != std::string::npos);
    auto unclocked = std::make_shared<MockState>(); unclocked->rate = 0; unclocked->set_rate_result = -1000;
    AsioBackend unavailable(mock_factory(unclocked));
    CHECK(unavailable.initialize(config, silent) == AudioResult::FormatNotSupported);
    CHECK(unavailable.error_message().find("current=0") != std::string::npos);
    CHECK(unavailable.error_message().find("set_rate=-1000 actual=0") != std::string::npos);
    CHECK_FALSE(unavailable.is_initialized());
    auto inaccurate = std::make_shared<MockState>(); inaccurate->rate = 48000; inaccurate->ignore_set_rate = true;
    AsioBackend wrong_clock(mock_factory(inaccurate));
    CHECK(wrong_clock.initialize(config, silent) == AudioResult::FormatNotSupported);
    CHECK(wrong_clock.error_message().find("set_rate=0 actual=48000") != std::string::npos);
}
