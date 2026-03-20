#include "audio/WasapiBackend.h"

// Windows-specific includes.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

#include <cstring>

namespace tenriff::audio {

namespace {

// WASAPI format constants.
constexpr WORD kChannels = 2;
constexpr WORD kBitsPerSample = 32;  // float
constexpr WORD kBlockAlign = kChannels * (kBitsPerSample / 8);

// Reference time units (100-nanosecond intervals).
constexpr REFERENCE_TIME kReftimesPerSec = 10'000'000;
constexpr REFERENCE_TIME kReftimesPerMs = 10'000;

// Helper to safely release COM objects.
template <typename T>
void safe_release(T** ptr) {
    if (*ptr) {
        (*ptr)->Release();
        *ptr = nullptr;
    }
}

// Convert our AudioResult from HRESULT.
AudioResult from_hresult(HRESULT hr) {
    if (SUCCEEDED(hr)) return AudioResult::Success;
    
    switch (hr) {
        case AUDCLNT_E_DEVICE_IN_USE:
            return AudioResult::DeviceInUse;
        case AUDCLNT_E_UNSUPPORTED_FORMAT:
            return AudioResult::FormatNotSupported;
        case AUDCLNT_E_DEVICE_INVALIDATED:
            return AudioResult::DeviceNotFound;
        case AUDCLNT_E_BUFFER_ERROR:
        case AUDCLNT_E_BUFFER_SIZE_ERROR:
        case AUDCLNT_E_BUFFER_TOO_LARGE:
            return AudioResult::BufferError;
        default:
            return AudioResult::Unknown;
    }
}

bool is_stereo_float_format(const WAVEFORMATEX* format) {
    if (!format || format->nChannels != kChannels || format->nSamplesPerSec == 0) {
        return false;
    }

    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return format->wBitsPerSample == kBitsPerSample;
    }

    if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
        format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
        return false;
    }

    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    return extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT &&
           extensible->Format.wBitsPerSample == kBitsPerSample;
}

}  // namespace

WasapiBackend::WasapiBackend() = default;

WasapiBackend::~WasapiBackend() {
    shutdown();
}

AudioResult WasapiBackend::initialize(const AudioConfig& config) {
    if (is_initialized_) {
        shutdown();
    }

    config_ = config;
    actual_sample_rate_ = 0;
    device_mix_sample_rate_ = 0;
    buffer_frames_ = 0;
    com_initialized_ = false;

    // Initialize COM for this thread.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return AudioResult::InitializationFailed;
    }
    com_initialized_ = SUCCEEDED(hr);

    // Get the default audio device.
    auto result = initialize_device(config);
    if (result != AudioResult::Success) {
        shutdown();
        return result;
    }

    // Initialize the audio client.
    result = initialize_client(config);
    if (result != AudioResult::Success) {
        shutdown();
        return result;
    }

    is_initialized_ = true;
    return AudioResult::Success;
}

AudioResult WasapiBackend::initialize_device(const AudioConfig& /*config*/) {
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator)
    );

    if (FAILED(hr) || !enumerator) {
        return AudioResult::InitializationFailed;
    }

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    enumerator->Release();

    if (FAILED(hr) || !device) {
        return AudioResult::DeviceNotFound;
    }

    device_ = device;
    return AudioResult::Success;
}

AudioResult WasapiBackend::initialize_client(const AudioConfig& config) {
    auto* device = static_cast<IMMDevice*>(device_);
    if (!device) {
        return AudioResult::InitializationFailed;
    }

    IAudioClient* audio_client = nullptr;
    HRESULT hr = device->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void**>(&audio_client)
    );

    if (FAILED(hr) || !audio_client) {
        return AudioResult::InitializationFailed;
    }

    audio_client_ = audio_client;

    // Define our desired format: 32-bit float, stereo.
    WAVEFORMATEXTENSIBLE wfx = {};
    wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfx.Format.nChannels = kChannels;
    wfx.Format.nSamplesPerSec = config.sample_rate;
    wfx.Format.wBitsPerSample = kBitsPerSample;
    wfx.Format.nBlockAlign = kBlockAlign;
    wfx.Format.nAvgBytesPerSec = config.sample_rate * kBlockAlign;
    wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfx.Samples.wValidBitsPerSample = kBitsPerSample;
    wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    const auto buffer_duration_for_rate = [&](uint32_t sample_rate) -> REFERENCE_TIME {
        if (sample_rate == 0) {
            return 0;
        }
        return static_cast<REFERENCE_TIME>(config.frames_per_buffer * config.periods) *
               kReftimesPerSec / sample_rate;
    };
    const REFERENCE_TIME requested_buffer_duration = buffer_duration_for_rate(config.sample_rate);

    // Try exclusive mode first if requested.
    const DWORD stream_flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    bool exclusive_success = false;

    if (config.exclusive_mode) {
        hr = audio_client->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            stream_flags,
            requested_buffer_duration,
            requested_buffer_duration,  // periodicity for exclusive mode
            reinterpret_cast<const WAVEFORMATEX*>(&wfx),
            nullptr
        );

        if (SUCCEEDED(hr)) {
            exclusive_success = true;
            is_exclusive_ = true;
        }
    }

    // Fall back to shared mode if exclusive failed or not requested.
    if (!exclusive_success) {
        WAVEFORMATEX* mix_format = nullptr;
        const HRESULT mix_hr = audio_client->GetMixFormat(&mix_format);
        if (SUCCEEDED(mix_hr) && mix_format) {
            device_mix_sample_rate_ = mix_format->nSamplesPerSec;
        }

        const DWORD shared_stream_flags =
            stream_flags | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        hr = audio_client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            shared_stream_flags,
            0,  // shared event-driven streams must use the engine period
            0,
            reinterpret_cast<const WAVEFORMATEX*>(&wfx),
            nullptr
        );

        if (SUCCEEDED(hr)) {
            actual_sample_rate_ = config.sample_rate;
        } else if (mix_format && is_stereo_float_format(mix_format)) {
            hr = audio_client->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                stream_flags,
                0,
                0,
                mix_format,
                nullptr
            );
            if (SUCCEEDED(hr)) {
                actual_sample_rate_ = mix_format->nSamplesPerSec;
            }
        }

        is_exclusive_ = false;
        if (mix_format) {
            CoTaskMemFree(mix_format);
        }
    } else {
        actual_sample_rate_ = config.sample_rate;
        device_mix_sample_rate_ = config.sample_rate;
    }

    if (FAILED(hr)) {
        return from_hresult(hr);
    }

    // Create event for buffer notifications.
    HANDLE event_handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_handle) {
        return AudioResult::InitializationFailed;
    }
    event_handle_ = event_handle;

    hr = audio_client->SetEventHandle(event_handle);
    if (FAILED(hr)) {
        CloseHandle(event_handle);
        event_handle_ = nullptr;
        return AudioResult::InitializationFailed;
    }

    // Get the actual buffer size.
    UINT32 buffer_size = 0;
    hr = audio_client->GetBufferSize(&buffer_size);
    if (FAILED(hr)) {
        return AudioResult::InitializationFailed;
    }
    buffer_frames_ = buffer_size;

    // Get the render client.
    IAudioRenderClient* render_client = nullptr;
    hr = audio_client->GetService(
        __uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&render_client)
    );

    if (FAILED(hr) || !render_client) {
        return AudioResult::InitializationFailed;
    }

    render_client_ = render_client;
    return AudioResult::Success;
}

AudioResult WasapiBackend::start() {
    if (!is_initialized_) {
        return AudioResult::InitializationFailed;
    }

    auto* audio_client = static_cast<IAudioClient*>(audio_client_);
    HRESULT hr = audio_client->Start();
    
    if (FAILED(hr)) {
        return from_hresult(hr);
    }

    is_playing_.store(true, std::memory_order_release);
    total_samples_played_.store(0, std::memory_order_release);
    return AudioResult::Success;
}

void WasapiBackend::stop() {
    is_playing_.store(false, std::memory_order_release);

    if (audio_client_) {
        auto* audio_client = static_cast<IAudioClient*>(audio_client_);
        audio_client->Stop();
        audio_client->Reset();
    }
}

void WasapiBackend::shutdown() {
    stop();
    release_resources();
    if (com_initialized_) {
        CoUninitialize();
        com_initialized_ = false;
    }
    is_initialized_ = false;
    is_exclusive_ = false;
    actual_sample_rate_ = 0;
    device_mix_sample_rate_ = 0;
    buffer_frames_ = 0;
}

void WasapiBackend::release_resources() {
    if (render_client_) {
        static_cast<IAudioRenderClient*>(render_client_)->Release();
        render_client_ = nullptr;
    }

    if (audio_client_) {
        static_cast<IAudioClient*>(audio_client_)->Release();
        audio_client_ = nullptr;
    }

    if (device_) {
        static_cast<IMMDevice*>(device_)->Release();
        device_ = nullptr;
    }

    if (event_handle_) {
        CloseHandle(static_cast<HANDLE>(event_handle_));
        event_handle_ = nullptr;
    }
}

uint32_t WasapiBackend::get_padding() const {
    if (!audio_client_) return 0;

    auto* audio_client = static_cast<IAudioClient*>(audio_client_);
    UINT32 padding = 0;
    HRESULT hr = audio_client->GetCurrentPadding(&padding);
    
    return SUCCEEDED(hr) ? padding : 0;
}

AudioResult WasapiBackend::get_buffer(uint32_t frames_requested,
                                       float** buffer,
                                       uint32_t* frames_available) {
    if (!render_client_ || !buffer || !frames_available) {
        return AudioResult::BufferError;
    }

    auto* render_client = static_cast<IAudioRenderClient*>(render_client_);
    BYTE* data = nullptr;
    HRESULT hr = render_client->GetBuffer(frames_requested, &data);

    if (FAILED(hr)) {
        *buffer = nullptr;
        *frames_available = 0;
        return from_hresult(hr);
    }

    *buffer = reinterpret_cast<float*>(data);
    *frames_available = frames_requested;
    return AudioResult::Success;
}

AudioResult WasapiBackend::release_buffer(uint32_t frames_written) {
    if (!render_client_) {
        return AudioResult::BufferError;
    }

    auto* render_client = static_cast<IAudioRenderClient*>(render_client_);
    HRESULT hr = render_client->ReleaseBuffer(frames_written, 0);

    return from_hresult(hr);
}

void* WasapiBackend::get_event_handle() const {
    return event_handle_;
}

}  // namespace tenriff::audio
