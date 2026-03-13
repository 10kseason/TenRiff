#pragma once

#include <cstdint>

namespace tenriff::timing {

class HighResClock {
public:
    [[nodiscard]] static int64_t now_ns();
};

}  // namespace tenriff::timing
