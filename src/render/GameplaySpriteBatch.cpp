#include "render/GameplaySpriteBatch.h"

#ifdef _WIN32
#include <d2d1_3.h>
#include <d2d1helper.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace tenriff::render {
namespace {
using Microsoft::WRL::ComPtr;
constexpr UINT32 kMinSpritesPerBatch = 8;

bool same_matrix(const D2D1_MATRIX_3X2_F& a, const D2D1_MATRIX_3X2_F& b) {
    return a._11 == b._11 && a._12 == b._12 && a._21 == b._21 &&
           a._22 == b._22 && a._31 == b._31 && a._32 == b._32;
}

bool valid_transform(const D2D1_MATRIX_3X2_F& value) {
    return std::isfinite(value._11) && std::isfinite(value._22) &&
           std::isfinite(value._31) && std::isfinite(value._32) &&
           value._11 > 0 && value._22 > 0 && value._12 == 0 && value._21 == 0;
}

bool valid_rect(const D2D1_RECT_F& value) {
    return std::isfinite(value.left) && std::isfinite(value.top) &&
           std::isfinite(value.right) && std::isfinite(value.bottom) &&
           value.right > value.left && value.bottom > value.top;
}

bool source_pixels(ID2D1Bitmap* bitmap, const D2D1_RECT_F* source,
                   D2D1_SIZE_U size, D2D1_RECT_U& output) {
    output = D2D1::RectU(0, 0, size.width, size.height);
    if (!source) return true;
    if (!valid_rect(*source)) return false;
    float dpi_x = 0, dpi_y = 0;
    bitmap->GetDpi(&dpi_x, &dpi_y);
    if (!std::isfinite(dpi_x) || !std::isfinite(dpi_y) || dpi_x <= 0 || dpi_y <= 0)
        return false;
    // DrawBitmap's source is in DIPs; sprite-batch source rectangles are pixels.
    // Do not round a fractional crop into a different image sampling operation.
    const double values[] = {double(source->left) * dpi_x / 96.0,
        double(source->top) * dpi_y / 96.0, double(source->right) * dpi_x / 96.0,
        double(source->bottom) * dpi_y / 96.0};
    for (unsigned index = 0; index < 4; ++index) {
        const double limit = (index & 1u) ? size.height : size.width;
        if (!std::isfinite(values[index]) || values[index] < 0 ||
            values[index] > limit || std::floor(values[index]) != values[index]) return false;
    }
    output = D2D1::RectU(static_cast<UINT32>(values[0]), static_cast<UINT32>(values[1]),
                         static_cast<UINT32>(values[2]), static_cast<UINT32>(values[3]));
    // DrawBitmap's crop can sample adjacent source pixels. Clamping that crop
    // in an atlas is observably different, so keep the original path for it.
    return output.left == 0 && output.top == 0 &&
           output.right == size.width && output.bottom == size.height;
}
} // namespace

struct GameplaySpriteBatch::Impl {
    struct Entry {
        ComPtr<ID2D1Bitmap> bitmap;
        ComPtr<IUnknown> identity;
        D2D1_SIZE_U size{};
        D2D1_POINT_2U origin{};
    };
    struct Sprite {
        D2D1_RECT_F destination{};
        D2D1_RECT_U source{};
        D2D1_COLOR_F color{};
        D2D1_RECT_F original_source{};
        std::size_t entry = 0;
        float opacity = 1;
        bool has_source = false;
    };

    ComPtr<ID2D1DeviceContext> owner;
    ComPtr<ID2D1DeviceContext3> context;
    ComPtr<ID2D1SpriteBatch> batch;
    ComPtr<ID2D1Bitmap> atlas;
    std::vector<Entry> entries;
    std::vector<Sprite> sprites;
    D2D1_MATRIX_3X2_F transform{1, 0, 0, 1, 0, 0};
    float dpi_x = 96, dpi_y = 96;
    unsigned next_x = 0, next_y = 0, row_height = 0;
    std::size_t source_bytes = 0;
    Statistics stats{};
    HRESULT error = S_OK;

    std::size_t register_bitmap(ID2D1Bitmap* bitmap, D2D1_SIZE_U size) {
        // Pointer lookup is fast for the retained interface used by the renderer.
        // Canonical IUnknown below also handles another interface for that object.
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].bitmap.Get() == bitmap && entries[i].size.width == size.width &&
                entries[i].size.height == size.height) { ++stats.cache_hits; return i; }
        }
        ComPtr<IUnknown> identity;
        error = bitmap->QueryInterface(IID_PPV_ARGS(&identity));
        if (FAILED(error)) return kMaxBitmaps;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].identity.Get() == identity.Get() && entries[i].size.width == size.width &&
                entries[i].size.height == size.height) { ++stats.cache_hits; return i; }
        }
        ++stats.cache_misses;
        if (entries.size() == kMaxBitmaps) return kMaxBitmaps;
        unsigned x = next_x, y = next_y, height = row_height;
        if (x + size.width > kAtlasDimension) { x = 0; y += height; height = 0; }
        if (y + size.height > kAtlasDimension) return kMaxBitmaps;
        if (!atlas) {
            const auto properties = D2D1::BitmapProperties(
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
            error = context->CreateBitmap(D2D1::SizeU(kAtlasDimension, kAtlasDimension),
                nullptr, 0, &properties, atlas.GetAddressOf());
            if (FAILED(error)) return kMaxBitmaps;
        }
        const auto origin = D2D1::Point2U(x, y);
        // Byte-preserving GPU copy, never rasterize or rescale an existing sprite.
        error = atlas->CopyFromBitmap(&origin, bitmap, nullptr);
        if (FAILED(error)) return kMaxBitmaps;
        Entry entry;
        entry.bitmap = bitmap;
        entry.identity = std::move(identity);
        entry.size = size;
        entry.origin = origin;
        entries.push_back(std::move(entry));
        next_x = x + size.width;
        next_y = y;
        row_height = (std::max)(height, size.height);
        source_bytes += std::size_t(size.width) * size.height * 4;
        return entries.size() - 1;
    }
};

GameplaySpriteBatch::GameplaySpriteBatch() : impl_(std::make_unique<Impl>()) {}
GameplaySpriteBatch::~GameplaySpriteBatch() = default;

bool GameplaySpriteBatch::initialize(ID2D1DeviceContext* context) {
    if (impl_->owner.Get() == context && impl_->batch) return true;
    reset();
    if (!context) { impl_->error = E_INVALIDARG; return false; }
    impl_->error = context->QueryInterface(IID_PPV_ARGS(&impl_->context));
    if (FAILED(impl_->error)) return false;
    impl_->error = impl_->context->CreateSpriteBatch(&impl_->batch);
    if (FAILED(impl_->error)) { impl_->context.Reset(); return false; }
    impl_->owner = context;
    impl_->entries.reserve(kMaxBitmaps);
    impl_->sprites.reserve(kMaxSprites);
    return true;
}

void GameplaySpriteBatch::reset() { impl_ = std::make_unique<Impl>(); }
void GameplaySpriteBatch::begin_frame() { impl_->stats = {}; }

bool GameplaySpriteBatch::enqueue(ID2D1Bitmap* bitmap, const D2D1_RECT_F& destination,
                                  float opacity, const D2D1_RECT_F* source) {
    auto& state = *impl_;
    auto reject = [&] { ++state.stats.rejected; return false; };
    if (!state.batch || !bitmap || !valid_rect(destination) || !std::isfinite(opacity) ||
        opacity < 0 || opacity > 1 || state.sprites.size() == kMaxSprites) return reject();
    const auto format = bitmap->GetPixelFormat();
    const auto size = bitmap->GetPixelSize();
    if (format.format != DXGI_FORMAT_B8G8R8A8_UNORM || format.alphaMode != D2D1_ALPHA_MODE_PREMULTIPLIED ||
        !size.width || !size.height || size.width > kAtlasDimension || size.height > kAtlasDimension)
        return reject();
    D2D1_RECT_U pixels{};
    if (!source_pixels(bitmap, source, size, pixels)) return reject();
    D2D1_MATRIX_3X2_F transform{};
    state.context->GetTransform(&transform);
    float dpi_x = 0, dpi_y = 0;
    state.context->GetDpi(&dpi_x, &dpi_y);
    if (!valid_transform(transform) || !std::isfinite(dpi_x) || !std::isfinite(dpi_y) ||
        dpi_x <= 0 || dpi_y <= 0 || state.context->GetAntialiasMode() != D2D1_ANTIALIAS_MODE_ALIASED ||
        state.context->GetPrimitiveBlend() != D2D1_PRIMITIVE_BLEND_SOURCE_OVER ||
        state.context->GetUnitMode() != D2D1_UNIT_MODE_DIPS) return reject();
    if (!state.sprites.empty() && (!same_matrix(state.transform, transform) ||
        state.dpi_x != dpi_x || state.dpi_y != dpi_y)) return reject();
    const auto index = state.register_bitmap(bitmap, size);
    if (index == kMaxBitmaps) return reject();
    const auto origin = state.entries[index].origin;
    Impl::Sprite sprite;
    sprite.destination = destination;
    sprite.source = D2D1::RectU(origin.x + pixels.left, origin.y + pixels.top,
        origin.x + pixels.right, origin.y + pixels.bottom);
    // D2D applies tint alpha to the premultiplied image. Keep white RGB so the
    // opacity is not applied twice (matching DrawBitmap's opacity parameter).
    sprite.color = D2D1::ColorF(1.0f, 1.0f, 1.0f, opacity);
    sprite.original_source = source ? *source : D2D1::RectF();
    sprite.entry = index;
    sprite.opacity = opacity;
    sprite.has_source = source != nullptr;
    state.transform = transform;
    state.dpi_x = dpi_x;
    state.dpi_y = dpi_y;
    state.sprites.push_back(sprite);
    ++state.stats.queued;
    return true;
}

bool GameplaySpriteBatch::flush(ID2D1DeviceContext* context) {
    auto& state = *impl_;
    if (state.sprites.empty()) return true;
    if (!context || context != state.owner.Get()) { state.error = E_INVALIDARG; return false; }
    const UINT32 count = static_cast<UINT32>(state.sprites.size());
    // Procedural LN bodies split heads/tails into very short runs. Uploading a
    // sprite batch for each run costs more than the original bitmap commands.
    const bool use_batch = count >= kMinSpritesPerBatch;
    state.error = S_OK;
    if (use_batch) {
        const UINT32 old_count = state.batch->GetSpriteCount();
        constexpr auto stride = static_cast<UINT32>(sizeof(Impl::Sprite));
        if (count > old_count) {
            const auto* extra = state.sprites.data() + old_count;
            state.error = state.batch->AddSprites(count - old_count, &extra->destination,
                &extra->source, &extra->color, nullptr, stride, stride, stride, 0);
        }
        if (SUCCEEDED(state.error)) {
            const auto* first = state.sprites.data();
            state.error = state.batch->SetSprites(0, count, &first->destination,
                &first->source, &first->color, nullptr, stride, stride, stride, 0);
        }
    }
    // Caller controls clip/target lifetime. Preserve other context state even if
    // a fallback was requested after a transform change.
    D2D1_MATRIX_3X2_F saved{};
    context->GetTransform(&saved);
    float saved_dpi_x = 0, saved_dpi_y = 0;
    context->GetDpi(&saved_dpi_x, &saved_dpi_y);
    const auto saved_aa = context->GetAntialiasMode();
    const auto saved_blend = context->GetPrimitiveBlend();
    const auto saved_unit = context->GetUnitMode();
    const bool transform_changed = !same_matrix(saved, state.transform);
    const bool dpi_changed = saved_dpi_x != state.dpi_x || saved_dpi_y != state.dpi_y;
    const bool aa_changed = saved_aa != D2D1_ANTIALIAS_MODE_ALIASED;
    const bool blend_changed = saved_blend != D2D1_PRIMITIVE_BLEND_SOURCE_OVER;
    const bool unit_changed = saved_unit != D2D1_UNIT_MODE_DIPS;
    if (transform_changed) context->SetTransform(state.transform);
    if (dpi_changed) context->SetDpi(state.dpi_x, state.dpi_y);
    if (aa_changed) context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    if (blend_changed) context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    if (unit_changed) context->SetUnitMode(D2D1_UNIT_MODE_DIPS);
    if (use_batch && SUCCEEDED(state.error)) {
        state.context->DrawSpriteBatch(state.batch.Get(), 0, count, state.atlas.Get(),
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, D2D1_SPRITE_OPTIONS_CLAMP_TO_SOURCE_RECTANGLE);
        ++state.stats.draw_calls;
        ++state.stats.submissions;
    } else {
        for (const auto& sprite : state.sprites) {
            context->DrawBitmap(state.entries[sprite.entry].bitmap.Get(), sprite.destination,
                sprite.opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                sprite.has_source ? &sprite.original_source : nullptr);
            ++state.stats.fallback_sprites;
            ++state.stats.draw_calls;
        }
    }
    if (unit_changed) context->SetUnitMode(saved_unit);
    if (blend_changed) context->SetPrimitiveBlend(saved_blend);
    if (aa_changed) context->SetAntialiasMode(saved_aa);
    if (dpi_changed) context->SetDpi(saved_dpi_x, saved_dpi_y);
    if (transform_changed) context->SetTransform(saved);
    state.sprites.clear();
    return true;
}

bool GameplaySpriteBatch::empty() const { return impl_->sprites.empty(); }
GameplaySpriteBatch::Statistics GameplaySpriteBatch::statistics() const {
    auto result = impl_->stats;
    result.cache_entries = impl_->entries.size();
    result.cache_bytes = impl_->atlas ? std::size_t(kAtlasDimension) * kAtlasDimension * 4 + impl_->source_bytes : 0;
    return result;
}
long GameplaySpriteBatch::last_error() const { return impl_->error; }

} // namespace tenriff::render
#endif
