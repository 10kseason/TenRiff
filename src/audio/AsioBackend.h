#pragma once
#include "audio/AudioConfig.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tenriff::audio {
namespace asio { struct Driver; }
struct AsioDriverInfo { std::string id; std::string name; };

// Lifecycle and COM calls are serialized on a dedicated control thread. ASIO
// driver callbacks render directly; no polling or WASAPI wrapper is involved.
class AsioBackend {
public:
    using Callback = std::function<void(float*, uint32_t, int64_t, int64_t)>;
    using DriverFactory = std::function<asio::Driver*(const std::string&)>;
    AsioBackend();
    // Injection boundary for deterministic native-protocol tests; takes one COM reference.
    explicit AsioBackend(DriverFactory factory);
    ~AsioBackend();
    AsioBackend(const AsioBackend&) = delete;
    AsioBackend& operator=(const AsioBackend&) = delete;
    static std::vector<AsioDriverInfo> enumerate_drivers();
    AudioResult initialize(const AudioConfig& config, Callback callback);
    AudioResult start();
    void stop();
    void shutdown();
    bool is_initialized() const;
    bool is_playing() const;
    bool has_runtime_error() const;
    std::string error_message() const;
    uint32_t sample_rate() const;
    uint32_t buffer_frames() const;
    int64_t playback_samples() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Shared by driver negotiation and tests. Unsupported DSD types return zero/false.
uint32_t asio_sample_bytes(int32_t type);
bool asio_encode_channel(void* destination, const float* stereo, uint32_t frames, uint32_t channel, int32_t type);
uint32_t asio_choose_buffer(uint32_t requested, int32_t minimum, int32_t maximum, int32_t preferred, int32_t granularity);
} // namespace tenriff::audio
