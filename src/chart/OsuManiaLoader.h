#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::chart {

enum class OsuParseSeverity {
    Warning,
    Error,
};

struct OsuParseMessage {
    OsuParseSeverity severity;
    std::size_t line;
    std::string text;
};

struct OsuManiaTimingPoint {
    double time_ms = 0.0;
    double beat_length = 0.0;
    bool inherited = false;
    int sample_set = 0;
    int sample_index = 0;
    int volume = 100;
};

struct OsuManiaNote {
    int column = 0;
    int64_t start_time_ms = 0;
    std::optional<int64_t> end_time_ms;
    int hit_sound = 0;
    int normal_set = 0;
    int addition_set = 0;
    int sample_index = 0;
    int sample_volume = 0;
    std::string custom_filename;
};

struct OsuManiaChart {
    int key_count = 0;
    double base_bpm = 0.0;
    double overall_difficulty = 8.0;
    int general_sample_set = 1;
    std::string title;
    std::string title_unicode;
    std::string artist;
    std::string artist_unicode;
    std::string version;
    std::string audio_filename;
    std::string background_filename;
    std::vector<OsuManiaTimingPoint> timing_points;
    std::vector<OsuManiaNote> notes;
};

struct OsuManiaParseResult {
    OsuManiaChart chart;
    std::vector<OsuParseMessage> messages;

    [[nodiscard]] bool success() const;
};

class OsuManiaLoader {
public:
    OsuManiaParseResult parse(std::string_view content) const;
};

}  // namespace tenriff::chart
