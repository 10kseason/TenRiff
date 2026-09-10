#include "audio/AsioBackend.h"
#include "audio/AsioDriverAbi.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <future>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>

namespace tenriff::audio {
namespace {
std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
    return result;
}
std::wstring registry_string(HKEY key, const wchar_t* name) {
    DWORD bytes = 0;
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS || bytes > 65536) return {};
    std::wstring value(bytes / sizeof(wchar_t) + 1, L'\0');
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, value.data(), &bytes) != ERROR_SUCCESS) return {};
    value.resize(wcslen(value.c_str()));
    return value;
}
asio::Driver* open_driver(const std::string& id) {
    // IDs originate in the registry/UI, never a DLL path or search-path load.
    if (id.size() != 38) return nullptr;
    std::wstring wide(id.begin(), id.end());
    CLSID clsid{};
    if (FAILED(CLSIDFromString(wide.c_str(), &clsid))) return nullptr;
    asio::Driver* result = nullptr;
    if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, clsid, reinterpret_cast<void**>(&result)))) return nullptr;
    return result;
}
uint64_t counter_value(asio::Counter value) { return (uint64_t(value.high) << 32) | value.low; }
}

std::vector<AsioDriverInfo> AsioBackend::enumerate_drivers() {
    std::vector<AsioDriverInfo> result;
    HKEY root = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ASIO", 0, KEY_READ | KEY_WOW64_64KEY, &root) != ERROR_SUCCESS) return result;
    for (DWORD index = 0;; ++index) {
        wchar_t name[256]{};
        DWORD length = 256;
        const auto status = RegEnumKeyExW(root, index, name, &length, nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS) continue;
        HKEY key = nullptr;
        if (RegOpenKeyExW(root, name, 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) continue;
        auto id = registry_string(key, L"CLSID");
        auto description = registry_string(key, L"Description");
        RegCloseKey(key);
        CLSID clsid{};
        if (id.empty() || FAILED(CLSIDFromString(id.c_str(), &clsid))) continue;
        wchar_t canonical[40]{};
        StringFromGUID2(clsid, canonical, 40);
        const std::string canonical_id = utf8(canonical);
        if (std::none_of(result.begin(), result.end(), [&](const auto& entry) { return entry.id == canonical_id; }))
            result.push_back({canonical_id, utf8(description.empty() ? std::wstring(name, length) : description)});
    }
    RegCloseKey(root);
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return result;
}

uint32_t asio_sample_bytes(int32_t type) {
    if (type == 0 || type == 16) return 2;
    if (type == 1 || type == 17) return 3;
    if (type == 4 || type == 20) return 8;
    if (type == 2 || type == 3 || type == 18 || type == 19 ||
        (type >= 8 && type <= 11) || (type >= 24 && type <= 27)) return 4;
    return 0;
}

bool asio_encode_channel(void* destination, const float* stereo, uint32_t frames, uint32_t channel, int32_t type) {
    const auto bytes = asio_sample_bytes(type);
    if (!destination || !stereo || !bytes || channel > 1) return false;
    auto* out = static_cast<uint8_t*>(destination);
    const bool big_endian = type < 16;
    const int32_t base = type >= 16 ? type - 16 : type;
    for (uint32_t frame = 0; frame < frames; ++frame) {
        double value = stereo[frame * 2 + channel];
        value = std::isfinite(value) ? std::clamp(value, -1.0, 1.0) : 0.0;
        uint64_t encoded = 0;
        if (base == 3) {
            const float sample = static_cast<float>(value);
            uint32_t bits = 0;
            std::memcpy(&bits, &sample, sizeof(bits));
            encoded = bits;
        } else if (base == 4) {
            std::memcpy(&encoded, &value, sizeof(encoded));
        } else {
            int bits = base == 0 ? 16 : base == 1 ? 24 : 32;
            if (base >= 8 && base <= 11) bits = base == 8 ? 16 : base == 9 ? 18 : base == 10 ? 20 : 24;
            const int64_t scale = int64_t(1) << (bits - 1);
            const int64_t quantized = std::clamp<int64_t>(static_cast<int64_t>(std::llround(value * scale)), -scale, scale - 1);
            // ASIO Int32{MSB,LSB}{16,18,20,24} stores a sign-extended,
            // right-aligned integer in a 32-bit container (not left shifted).
            encoded = static_cast<uint64_t>(quantized);
        }
        for (uint32_t byte = 0; byte < bytes; ++byte)
            out[frame * bytes + byte] = static_cast<uint8_t>(encoded >> (8 * (big_endian ? bytes - byte - 1 : byte)));
    }
    return true;
}

uint32_t asio_choose_buffer(uint32_t requested, int32_t minimum, int32_t maximum, int32_t preferred, int32_t granularity) {
    constexpr int32_t limit = 65536;
    if (minimum <= 0 || maximum < minimum || maximum > limit || preferred < minimum || preferred > maximum || granularity < -1) return 0;
    // granularity=0 means the driver only promises its preferred size.
    if (granularity == 0) return static_cast<uint32_t>(preferred);
    uint32_t best = 0;
    uint64_t distance = (std::numeric_limits<uint64_t>::max)();
    for (int32_t candidate = minimum; candidate <= maximum; ++candidate) {
        if (granularity == -1 ? (candidate & (candidate - 1)) != 0 : (candidate - minimum) % granularity != 0) continue;
        const uint64_t delta = candidate > requested ? uint64_t(candidate) - requested : uint64_t(requested) - candidate;
        if (delta < distance || (delta == distance && candidate > static_cast<int32_t>(best))) {
            best = static_cast<uint32_t>(candidate); distance = delta;
        }
    }
    return best;
}

struct AsioBackend::Impl {
    DriverFactory factory;
    std::thread control;
    std::mutex command_mutex;
    std::mutex invocation_mutex;
    std::condition_variable command_ready;
    std::function<void()> command;
    bool quit = false;
    std::mutex error_mutex;
    std::string error;
    asio::Driver* driver = nullptr;
    std::array<asio::BufferInfo, 2> buffers{};
    std::array<int32_t, 2> types{};
    asio::Callbacks callbacks{};
    Callback mixer;
    std::vector<float> interleaved;
    std::atomic<bool> initialized{false}, playing{false};
    std::atomic<uint32_t> rate{0}, frames{0};
    std::atomic<int64_t> playback{0};
    std::atomic<int> fault{0};
    std::atomic<unsigned> active_calls{0};
    std::atomic_flag processing = ATOMIC_FLAG_INIT;
    bool buffers_created = false, driver_started = false, ready_supported = false;
    int32_t latency = 0;
    int64_t cursor = 0;
    uint64_t position_origin = 0;
    int64_t position_origin_cursor = 0;
    bool have_origin = false;
    static std::mutex routing_mutex;
    static Impl* active;

    explicit Impl(DriverFactory f) : factory(std::move(f)) {
      control = std::thread([this] {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        for (;;) {
            std::function<void()> next;
            {
                std::unique_lock lock(command_mutex);
                command_ready.wait_for(lock, std::chrono::milliseconds(10), [&] { return quit || bool(command); });
                if (quit) break;
                next = std::move(command);
            }
            if (next) next();
            // Several native drivers own message windows on their init thread.
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
        }
        if (SUCCEEDED(hr)) CoUninitialize();
      });
    }
    ~Impl() {
        invoke([&] { release(); return AudioResult::Success; });
        { std::lock_guard lock(command_mutex); quit = true; }
        command_ready.notify_one();
        control.join();
    }
    AudioResult invoke(std::function<AudioResult()> action) {
        std::lock_guard serial(invocation_mutex);
        auto task = std::make_shared<std::packaged_task<AudioResult()>>(std::move(action));
        auto result = task->get_future();
        { std::lock_guard lock(command_mutex); command = [task] { (*task)(); }; }
        command_ready.notify_one();
        return result.get();
    }
    AudioResult fail(AudioResult result, const std::string& message) {
        { std::lock_guard lock(error_mutex); error = message; }
        return result;
    }
    AudioResult driver_failure(AudioResult result, const char* action) {
        char detail[124]{};
        if (driver) driver->error_text(detail);
        detail[123] = '\0';
        return fail(result, std::string("ASIO ") + action + " failed" + (detail[0] ? std::string(": ") + detail : "."));
    }
    void stop_driver() {
        playing.store(false, std::memory_order_release);
        if (driver && driver_started) {
            driver->stop(); driver_started = false;
        }
        // stop() must cease driver callbacks before returning; the count also
        // protects an already entered callback while teardown is requested.
        while (active_calls.load(std::memory_order_acquire)) std::this_thread::yield();
    }
    void release() {
        initialized.store(false, std::memory_order_release);
        stop_driver();
        if (driver && buffers_created) { driver->dispose_buffers(); buffers_created = false; }
        { std::lock_guard lock(routing_mutex); if (active == this) active = nullptr; }
        while (active_calls.load(std::memory_order_acquire)) std::this_thread::yield();
        if (driver) { driver->Release(); driver = nullptr; }
        interleaved.clear(); mixer = nullptr; rate = 0; frames = 0; playback = 0;
        buffers = {}; types = {};
    }
    AudioResult setup(const AudioConfig& config, Callback callback) {
        release(); fault = 0;
        { std::lock_guard lock(error_mutex); error.clear(); }
        if (!callback || config.sample_rate < 8000 || config.sample_rate > 384000 || config.frames_per_buffer == 0 || config.frames_per_buffer > 65536)
            return fail(AudioResult::FormatNotSupported, "ASIO sample rate or buffer size is invalid.");
        std::string id = config.asio_driver;
        if (!factory) {
            const auto installed = AsioBackend::enumerate_drivers();
            if (id.empty() && !installed.empty()) id = installed.front().id;
            if (id.empty() || std::none_of(installed.begin(), installed.end(), [&](const auto& entry) { return _stricmp(entry.id.c_str(), id.c_str()) == 0; }))
                return fail(AudioResult::DeviceNotFound, "Selected x64 ASIO driver is not installed.");
        }
        {
            std::lock_guard lock(routing_mutex);
            if (active) return fail(AudioResult::DeviceInUse, "Another ASIO stream is already open.");
            active = this;
        }
        driver = factory ? factory(id) : open_driver(id);
        if (!driver) return fail(AudioResult::DeviceNotFound, "ASIO driver could not be loaded. Check its x64 installation.");
        if (!driver->initialize(GetDesktopWindow())) return driver_failure(AudioResult::InitializationFailed, "driver initialization");
        int32_t inputs = 0, outputs = 0;
        if (!asio::succeeded(driver->channels(&inputs, &outputs)) || outputs < 2)
            return fail(AudioResult::FormatNotSupported, "ASIO needs at least two output channels.");
        double current_rate = 0;
        const auto current_result = driver->get_rate(&current_rate);
        const auto can_result = driver->can_rate(config.sample_rate);
        const auto matches_requested = [&](double value) {
            return std::isfinite(value) && std::abs(value - config.sample_rate) <= 0.5;
        };
        // Fixed-rate drivers may reject setSampleRate even when their active
        // clock already has the requested rate. A valid exact readback needs no
        // setter; changing rates still requires support, success and readback.
        if (!asio::succeeded(current_result) || !matches_requested(current_rate)) {
            asio::Result set_result = -1000;
            bool set_attempted = false;
            if (asio::succeeded(can_result)) {
                set_attempted = true;
                set_result = driver->set_rate(config.sample_rate);
            }
            double actual_rate = 0;
            const auto actual_result = driver->get_rate(&actual_rate);
            if (!set_attempted || !asio::succeeded(set_result) || !asio::succeeded(actual_result) || !matches_requested(actual_rate)) {
                std::ostringstream detail;
                detail << "ASIO sample-rate negotiation failed: requested=" << config.sample_rate
                       << " current=" << current_rate << " get_rate(before)=" << current_result
                       << " can_rate=" << can_result << " set_rate=";
                if (set_attempted) detail << set_result;
                else detail << "not attempted";
                detail << " actual=" << actual_rate << " get_rate(after)=" << actual_result << '.';
                char driver_text[124]{}; driver->error_text(driver_text); driver_text[123] = '\0';
                if (driver_text[0]) detail << ' ' << driver_text;
                return fail(AudioResult::FormatNotSupported, detail.str());
            }
        }
        int32_t minimum = 0, maximum = 0, preferred = 0, granularity = 0;
        if (!asio::succeeded(driver->buffer_sizes(&minimum, &maximum, &preferred, &granularity)))
            return driver_failure(AudioResult::BufferError, "buffer-size query");
        const auto chosen = asio_choose_buffer(config.frames_per_buffer, minimum, maximum, preferred, granularity);
        if (!chosen) return fail(AudioResult::BufferError, "ASIO driver reported invalid buffer sizes.");
        frames = chosen; rate = config.sample_rate;
        for (int32_t channel = 0; channel < 2; ++channel) {
            asio::ChannelInfo info{}; info.channel = channel;
            if (!asio::succeeded(driver->channel_info(&info)) || !asio_sample_bytes(info.sample_type))
                return fail(AudioResult::FormatNotSupported, "ASIO output format is unsupported (PCM/float stereo required).");
            types[channel] = info.sample_type; buffers[channel].channel = channel;
        }
        callbacks = {&switch_buffer, &rate_changed, &message, &switch_time};
        interleaved.assign(size_t(chosen) * 2, 0.0f); mixer = std::move(callback);
        if (!asio::succeeded(driver->create_buffers(buffers.data(), 2, static_cast<int32_t>(chosen), &callbacks)))
            return driver_failure(AudioResult::BufferError, "buffer creation");
        buffers_created = true;
        for (int channel = 0; channel < 2; ++channel) for (int buffer = 0; buffer < 2; ++buffer) {
            if (!buffers[channel].buffers[buffer]) return fail(AudioResult::BufferError, "ASIO driver returned a null output buffer.");
            std::memset(buffers[channel].buffers[buffer], 0, size_t(chosen) * asio_sample_bytes(types[channel]));
        }
        int32_t input_latency = 0;
        if (!asio::succeeded(driver->latencies(&input_latency, &latency)) || latency < 0 || latency > static_cast<int32_t>(config.sample_rate * 10))
            return fail(AudioResult::InitializationFailed, "ASIO driver reported an invalid output latency.");
        ready_supported = asio::succeeded(driver->output_ready());
        fault = 0; initialized = true;
        return AudioResult::Success;
    }
    AudioResult begin() {
        if (!initialized || !driver) return AudioResult::InitializationFailed;
        if (driver_started) return playing ? AudioResult::Success : AudioResult::BufferError;
        if (fault) return fail(AudioResult::InitializationFailed, "ASIO configuration changed. Reopen the audio stream.");
        cursor = 0; have_origin = false; playback = 0;
        playing = true;
        if (!asio::succeeded(driver->start())) {
            playing = false;
            // A driver may have partially enabled its callbacks before failing.
            driver->stop();
            while (active_calls.load(std::memory_order_acquire)) std::this_thread::yield();
            return driver_failure(AudioResult::InitializationFailed, "start");
        }
        driver_started = true;
        return AudioResult::Success;
    }
    static Impl* enter() {
        std::lock_guard lock(routing_mutex);
        if (active) active->active_calls.fetch_add(1, std::memory_order_acq_rel);
        return active;
    }
    static void leave(Impl* self) { self->active_calls.fetch_sub(1, std::memory_order_release); }
    void render(int32_t index, const asio::Time* time) noexcept {
        if (index < 0 || index > 1) { fault = 5; playing = false; return; }
        if (processing.test_and_set(std::memory_order_acquire)) { fault = 5; playing = false; return; }
        const uint32_t count = frames.load();
        if (count && buffers_created) {
            std::fill(interleaved.begin(), interleaved.end(), 0.0f);
            if (playing.load(std::memory_order_acquire) && !fault.load()) {
                if (time && (time->info.flags & 4u) &&
                    (!std::isfinite(time->info.sample_rate) || std::abs(time->info.sample_rate - rate.load()) > 0.5)) {
                    fault = 2; playing = false;
                }
                asio::Counter pos{}, stamp{};
                bool valid = time && (time->info.flags & 2u);
                if (valid) pos = time->info.sample_position;
                else valid = asio::succeeded(driver->position(&pos, &stamp));
                if (valid) {
                    const uint64_t absolute = counter_value(pos);
                    if (!have_origin) {
                        // Position reporting can become available after several
                        // fallback callbacks. Anchor it to that same stream cursor
                        // instead of restarting the device timeline at zero.
                        position_origin = absolute;
                        position_origin_cursor = cursor;
                        have_origin = true;
                    }
                    if (absolute < position_origin ||
                        absolute - position_origin > uint64_t((std::numeric_limits<int64_t>::max)() - count - position_origin_cursor)) {
                        fault = 4; playing = false;
                    } else {
                        const int64_t relative = position_origin_cursor + static_cast<int64_t>(absolute - position_origin);
                        // A reset/backwards device clock must never rewind chart judgement.
                        if (relative + count < cursor) { fault = 4; playing = false; }
                        else cursor = (std::max)(cursor, relative);
                    }
                }
                const int64_t audible = (std::max)(int64_t(0), cursor - latency);
                playback.store(audible, std::memory_order_release);
                if (playing && !fault) {
                    try { mixer(interleaved.data(), count, cursor, audible); }
                    catch (...) { fault = 6; playing = false; std::fill(interleaved.begin(), interleaved.end(), 0.0f); }
                    cursor += count;
                }
            }
            // Reset/stop may arrive on another driver thread during the mixer.
            if (!playing.load(std::memory_order_acquire) || fault.load())
                std::fill(interleaved.begin(), interleaved.end(), 0.0f);
            for (uint32_t channel = 0; channel < 2; ++channel)
                asio_encode_channel(buffers[channel].buffers[index], interleaved.data(), count, channel, types[channel]);
            if (ready_supported) driver->output_ready();
        }
        processing.clear(std::memory_order_release);
    }
    static void __cdecl switch_buffer(int32_t index, int32_t) {
        if (auto* self = enter()) { self->render(index, nullptr); leave(self); }
    }
    static asio::Time* __cdecl switch_time(asio::Time* time, int32_t index, int32_t) {
        if (auto* self = enter()) { self->render(index, time); leave(self); }
        return nullptr;
    }
    static void __cdecl rate_changed(double new_rate) {
        if (auto* self = enter()) {
            if (self->initialized && (!std::isfinite(new_rate) || std::abs(new_rate - self->rate.load()) > 0.5)) { self->fault = 2; self->playing = false; }
            leave(self);
        }
    }
    static int32_t __cdecl message(int32_t selector, int32_t value, void*, double*) {
        if (selector == 1) return value == 2 || value == 3 || value == 4 || value == 5 || value == 6 || value == 7;
        if (selector == 2) return 2;
        if (selector == 7) return 1;
        if (selector == 3 || selector == 4 || selector == 5 || selector == 6) {
            if (auto* self = enter()) { if (self->initialized) { self->fault = selector == 4 ? 3 : selector == 5 ? 4 : 1; self->playing = false; } leave(self); }
            return 1;
        }
        return 0;
    }
};
std::mutex AsioBackend::Impl::routing_mutex;
AsioBackend::Impl* AsioBackend::Impl::active = nullptr;

AsioBackend::AsioBackend() : AsioBackend(DriverFactory{}) {}
AsioBackend::AsioBackend(DriverFactory factory) : impl_(std::make_unique<Impl>(std::move(factory))) {}
AsioBackend::~AsioBackend() = default;
AudioResult AsioBackend::initialize(const AudioConfig& config, Callback callback) {
    return impl_->invoke([&, callback = std::move(callback)]() mutable {
        const auto result = impl_->setup(config, std::move(callback));
        if (result != AudioResult::Success) impl_->release();
        return result;
    });
}
AudioResult AsioBackend::start() { return impl_->invoke([&] { return impl_->begin(); }); }
void AsioBackend::stop() { impl_->invoke([&] { impl_->stop_driver(); return AudioResult::Success; }); }
void AsioBackend::shutdown() { impl_->invoke([&] { impl_->release(); return AudioResult::Success; }); }
bool AsioBackend::is_initialized() const { return impl_->initialized.load(); }
bool AsioBackend::is_playing() const { return impl_->playing.load(); }
bool AsioBackend::has_runtime_error() const { return impl_->fault.load() != 0; }
std::string AsioBackend::error_message() const {
    switch (impl_->fault.load()) {
    case 1: return "ASIO driver requested a reset or latency change. Reopen audio settings.";
    case 2: return "ASIO sample rate changed during playback. Reopen audio settings.";
    case 3: return "ASIO buffer size changed during playback. Reopen audio settings.";
    case 4: return "ASIO device clock was reset. Reopen audio settings.";
    case 5: return "ASIO driver supplied an invalid or overlapping callback.";
    case 6: return "ASIO audio processing failed.";
    default: { std::lock_guard lock(impl_->error_mutex); return impl_->error; }
    }
}
uint32_t AsioBackend::sample_rate() const { return impl_->rate.load(); }
uint32_t AsioBackend::buffer_frames() const { return impl_->frames.load(); }
int64_t AsioBackend::playback_samples() const { return impl_->playback.load(); }
} // namespace tenriff::audio
