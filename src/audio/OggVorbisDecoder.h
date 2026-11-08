#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace tenriff::audio {

std::optional<int> probe_ogg_vorbis_sample_rate(const std::string& path, std::string* error = nullptr);
bool decode_ogg_vorbis_stereo(const std::string& path,
                              int* out_sample_rate,
                              std::vector<float>& out,
                              std::string* error = nullptr);
bool decode_ogg_vorbis_stereo(const std::string& path,
                              int* out_sample_rate,
                              std::vector<float>& out,
                              std::string* error,
                              std::size_t max_frames);

}  // namespace tenriff::audio
