#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace tenriff::render {
// Missing historical evidence must not turn a legacy normalized score into EX.
inline std::string ex_score_text(const std::optional<int64_t>& score) {
    return score ? std::to_string(*score) : "--";
}
} // namespace tenriff::render
