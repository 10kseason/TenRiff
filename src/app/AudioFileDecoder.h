#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace tenriff::app {

// Decode a supported audio file to interleaved stereo float PCM. File I/O and
// decoding must stay off the realtime audio callback.
[[nodiscard]] bool decode_audio_file_stereo_resampled(const std::string& path,
                                                      int target_sample_rate,
                                                      std::vector<float>& out,
                                                      std::string* error = nullptr,
                                                      std::size_t max_output_frames = 0);

}  // namespace tenriff::app
