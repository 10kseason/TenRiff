#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace tenriff::render {

enum class DeblockResult { Completed, Unavailable, Failed, Cancelled };

// Worker-only: one full-size RGB filter pass, with byte-exact source alpha.
// Failure leaves bgra untouched; cancellation and a deadline bound child lifetime.
DeblockResult deblock_bgra_ffmpeg(std::uint32_t width, std::uint32_t height,
                                std::vector<std::uint8_t>& bgra,
                                const std::function<bool()>& cancelled = {});

}  // namespace tenriff::render
