#pragma once

#include <optional>
#include <string>

namespace keyweaver_eplus {

enum class NoteType {
    Tap,
    Hold,
};

struct Note {
    std::string id;
    int time = 0;
    int lane = 0;
    NoteType type = NoteType::Tap;
    std::optional<int> endTime;
    std::string raw;
    std::optional<int> sourceLane;
};

}  // namespace keyweaver_eplus

