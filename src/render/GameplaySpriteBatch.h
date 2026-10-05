#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d2d1_1.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace tenriff::render {

// Render-thread-owned, append-only replacement for compatible DrawBitmap calls.
// A single atlas preserves painter order across lanes and sprite types.
class GameplaySpriteBatch {
public:
    static constexpr unsigned kAtlasDimension = 2048;
    static constexpr std::size_t kMaxBitmaps = 128;
    static constexpr std::size_t kMaxSprites = 1024;
    // Atlas plus the maximum source pixels kept alive by strong references.
    static constexpr std::size_t kMaxBytes = 32u * 1024u * 1024u;

    struct Statistics {
        std::uint64_t cache_hits = 0;
        std::uint64_t cache_misses = 0;
        std::uint64_t rejected = 0;
        std::uint64_t queued = 0;
        std::uint64_t submissions = 0;
        std::uint64_t draw_calls = 0;
        std::uint64_t fallback_sprites = 0;
        std::size_t cache_entries = 0;
        std::size_t cache_bytes = 0;
    };

    GameplaySpriteBatch();
    ~GameplaySpriteBatch();
    GameplaySpriteBatch(const GameplaySpriteBatch&) = delete;
    GameplaySpriteBatch& operator=(const GameplaySpriteBatch&) = delete;

    // Requires ID2D1DeviceContext3. Older contexts retain ordinary D2D drawing.
    // Call reset on device/skin invalidation; source pixels must be immutable
    // while cached (in-place bitmap uploads require reset before reuse).
    bool initialize(ID2D1DeviceContext* context);
    void reset();
    void begin_frame();

    // Supports premultiplied BGRA8, aliased geometry, source-over blending,
    // DIPs, and positive axis-aligned transforms. Destination geometry is never
    // snapped. An explicit source must map exactly to the full bitmap in pixels;
    // custom crops fall back because DrawBitmap and sprite source-clamp differ
    // at crop edges. Linear sprite sampling may differ by one 8-bit color level.
    // False leaves earlier queued sprites intact: flush before caller fallback.
    bool enqueue(ID2D1Bitmap* bitmap, const D2D1_RECT_F& destination,
                 float opacity, const D2D1_RECT_F* source = nullptr);

    // Call within BeginDraw/EndDraw on the initialized context. Caller must flush
    // before any intervening drawing or clip/layer/target change. No D2D Flush or
    // EndDraw is introduced. API allocation failure falls back in original order.
    bool flush(ID2D1DeviceContext* context);
    bool empty() const;
    Statistics statistics() const;
    long last_error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tenriff::render
#endif
