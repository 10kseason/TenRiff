#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <optional>

namespace tenriff::app {

struct KeysoundAudioInfo {
    // Duration saturates at the IR exclusion boundary. No playback-rate scaling.
    double duration_seconds = 0.0;
    bool audible = false;
};
using InspectKeysoundAudio = std::optional<KeysoundAudioInfo> (*)(const std::string&);
[[nodiscard]] std::optional<KeysoundAudioInfo> inspect_keysound_audio(const std::string& path);

// Decode a supported audio file to interleaved stereo float PCM. File I/O and
// decoding must stay off the realtime audio callback.
[[nodiscard]] bool decode_audio_file_stereo_resampled(const std::string& path,
                                                      int target_sample_rate,
                                                      std::vector<float>& out,
                                                      std::string* error = nullptr,
                                                      std::size_t max_output_frames = 0);

}  // namespace tenriff::app
