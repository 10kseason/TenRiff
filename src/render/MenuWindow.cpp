#include "render/MenuWindow.h"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <d2d1_1.h>
#include <d2d1helper.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dwrite.h>
#include <dxgi1_5.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <objbase.h>

#include "app/GraphicsTiming.h"
#include "app/OsuSkin.h"
#include "render/GameplayMotion.h"
#include "timing/HighResClock.h"
#include "util/Utf8Compat.h"

namespace tenriff::render {

namespace {

constexpr float kBaseWidth = 1920.0f;
constexpr float kBaseHeight = 1080.0f;
constexpr wchar_t kWindowClassName[] = L"TenRiffMenuWindow";
constexpr float kGameplayFieldLeft = 470.0f;
constexpr float kGameplayFieldRight = 1450.0f;
constexpr float kGameplayFieldTop = 0.0f;
constexpr float kGameplayFieldBottom = kBaseHeight;
constexpr float kGameplayGaugeLeft = 1510.0f;
constexpr float kGameplayGaugeTop = 210.0f;
constexpr float kGameplayGaugeBottom = 910.0f;
constexpr float kGameplayGaugeWidth = 46.0f;
constexpr double kGameplayJudgementLineMin = 0.55;
constexpr double kGameplayJudgementLineMax = 0.86;
constexpr double kGameplayJudgementLineDefault = 0.82;
constexpr double kGameplayNoteWidthScaleMin = 0.50;
constexpr double kGameplayNoteWidthScaleMax = 1.40;
constexpr double kGameplayNoteHeightScaleMin = 0.50;
constexpr double kGameplayNoteHeightScaleMax = 4.00;
constexpr double kGameplayLaneDividerWidthScaleMin = 0.00;
constexpr double kGameplayLaneDividerWidthScaleMax = 2.00;
constexpr double kGameplayLaneDividerWidthScaleDefault = 1.00;
constexpr double kGameplayHoldBodyWidthScaleMin = 0.50;
constexpr double kGameplayHoldBodyWidthScaleMax = 1.20;
constexpr double kGameplayHoldBodyWidthScaleDefault = 0.60;
constexpr double kGameplayNoteHeightScaleDefault = 1.80;
constexpr double kGameplayComboPositionMin = 0.10;
constexpr double kGameplayComboPositionMax = 0.78;
constexpr double kGameplayComboPositionDefault = 0.24;
constexpr float kGameplayLaneDividerBaseWidth = 1.0f;
constexpr float kGameplayLaneDividerWidthMaxPx = 16.0f;

struct MenuSceneConstants {
    float resolution[2]{};
    float time_sec = 0.0f;
    float scene_kind = 0.0f;
    float primary_color[4]{};
    float secondary_color[4]{};
};

constexpr char kMenuSceneShaderSource[] = R"(
cbuffer SceneCB : register(b0)
{
    float2 gResolution;
    float gTime;
    float gSceneKind;
    float4 gPrimaryColor;
    float4 gSecondaryColor;
};

struct VSOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOut vs_main(uint vertex_id : SV_VertexID)
{
    float2 pos = (vertex_id == 0) ? float2(-1.0, -1.0) :
                 ((vertex_id == 1) ? float2(-1.0, 3.0) : float2(3.0, -1.0));
    VSOut output;
    output.position = float4(pos, 0.0, 1.0);
    output.uv = pos * 0.5 + 0.5;
    return output;
}

float hash21(float2 p)
{
    p = frac(p * float2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return frac(p.x * p.y);
}

float projectGlow(float2 uv,
                  float3 worldPos,
                  float3 camPos,
                  float3 camForward,
                  float3 camRight,
                  float3 camUp,
                  float aspect,
                  float size)
{
    float3 rel = worldPos - camPos;
    float3 view = float3(dot(rel, camRight), dot(rel, camUp), dot(rel, camForward));
    if (view.z <= 0.1)
    {
        return 0.0;
    }
    float2 screen = float2(view.x / (view.z * aspect), view.y / view.z) * 1.18;
    float d = length(uv - screen);
    return exp(-d * d / max(0.0001, size * size));
}

float starfield(float3 rd)
{
    float2 p = rd.xy / max(0.18, rd.z + 1.35);
    p *= 56.0;
    float2 cell = floor(p);
    float2 f = frac(p) - 0.5;
    float h = hash21(cell);
    float radius = 0.10 + 0.18 * frac(h * 13.7);
    float star = 1.0 - smoothstep(0.0, radius, length(f));
    return star * step(0.988, h);
}

float4 ps_main(VSOut input) : SV_Target
{
    float2 uv = input.uv * 2.0 - 1.0;
    float aspect = max(0.001, gResolution.x / max(1.0, gResolution.y));
    uv.x *= aspect;
    float t = gTime;
    float scene = saturate(gSceneKind);

    float3 camPos = lerp(float3(0.0, 1.05, -6.2), float3(0.0, 1.18, -7.4), scene);
    camPos.x += sin(t * 0.11 + scene * 1.7) * 0.30;
    camPos.y += sin(t * 0.17 + scene * 0.4) * 0.05;
    float3 target = lerp(float3(0.0, -0.20, 8.0), float3(0.0, -0.32, 11.2), scene);
    target.x += sin(t * 0.09) * (0.28 - scene * 0.08);

    float3 forward = normalize(target - camPos);
    float3 right = normalize(cross(float3(0.0, 1.0, 0.0), forward));
    float3 up = normalize(cross(forward, right));
    float3 rd = normalize(forward + uv.x * right * 0.88 + uv.y * up * 0.72);

    float skyH = saturate(rd.y * 0.5 + 0.5);
    float3 titleTop = float3(0.03, 0.05, 0.09);
    float3 songTop = float3(0.015, 0.016, 0.030);
    float3 titleHorizon = float3(0.055, 0.090, 0.120);
    float3 songHorizon = float3(0.040, 0.044, 0.072);
    float3 topSky = lerp(titleTop, songTop, scene) + gPrimaryColor.rgb * lerp(0.10, 0.04, scene);
    float3 horizonSky = lerp(titleHorizon, songHorizon, scene) +
                        gSecondaryColor.rgb * lerp(0.12, 0.05, scene);
    float3 color = lerp(horizonSky, topSky, pow(skyH, lerp(1.15, 0.85, scene)));

    float stars = starfield(rd);
    color += stars * lerp(0.55, 0.35, scene) * (0.65 * gSecondaryColor.rgb + 0.35 * gPrimaryColor.rgb);

    float horizonGlow = exp(-abs(rd.y + 0.01 + scene * 0.005) * lerp(18.0, 22.0, scene));
    color += horizonGlow * (gPrimaryColor.rgb * lerp(0.12, 0.06, scene) +
                            gSecondaryColor.rgb * lerp(0.04, 0.025, scene));
    float horizonLine = exp(-abs(rd.y + 0.012) * lerp(160.0, 260.0, scene));
    color += horizonLine * (gPrimaryColor.rgb * lerp(0.10, 0.08, scene) +
                            gSecondaryColor.rgb * lerp(0.05, 0.04, scene));

    float planeY = -1.20 + scene * 0.10;
    if (rd.y < -0.02)
    {
        float tPlane = (planeY - camPos.y) / rd.y;
        if (tPlane > 0.0)
        {
            float3 hit = camPos + rd * tPlane;
            float distFade = saturate(1.0 - tPlane * 0.030);
            float2 gridUv = hit.xz;
            float gridScaleX = lerp(0.26, 0.18, scene);
            float gridScaleZ = lerp(0.10, 0.074, scene);
            float2 cell = abs(frac(gridUv * float2(gridScaleX, gridScaleZ)) - 0.5);
            float gridMinor = exp(-min(cell.x, cell.y) * lerp(92.0, 138.0, scene));
            float2 majorCell = abs(frac(gridUv * float2(gridScaleX * 0.25, gridScaleZ * 0.20)) - 0.5);
            float gridMajor = exp(-min(majorCell.x, majorCell.y) * lerp(34.0, 58.0, scene));
            float scan = 0.5 + 0.5 * sin(hit.z * 0.12 - t * (0.55 - scene * 0.15));
            float nebulaNoise = 0.5 + 0.5 * sin(hit.x * 0.025 + t * 0.08) * sin(hit.z * 0.018 - t * 0.05);
            float floorGlow = exp(-length(hit.xz * float2(0.014, 0.008)) * lerp(1.0, 0.78, scene));
            float3 floorColor = lerp(float3(0.015, 0.022, 0.032), float3(0.016, 0.018, 0.032), scene);
            floorColor += gridMinor * (gPrimaryColor.rgb * lerp(0.18 + 0.08 * scan, 0.028 + 0.014 * scan, scene));
            floorColor += gridMajor * (gSecondaryColor.rgb * lerp(0.18, 0.040, scene) +
                                       gPrimaryColor.rgb * lerp(0.08, 0.018, scene));
            floorColor += floorGlow * (gPrimaryColor.rgb * lerp(0.06, 0.020, scene));
            floorColor += floorGlow * nebulaNoise * (gSecondaryColor.rgb * lerp(0.00, 0.055, scene));
            color = lerp(color, floorColor, distFade);
        }
    }

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float fi = (float)i;
        float phase = fi * 1.37 + scene * 0.7;
        float radius = lerp(3.6, 6.6, scene) + fi * 1.2;
        float3 lightPos = float3(
            sin(t * (0.08 + fi * 0.01) + phase) * radius,
            0.55 + sin(t * (0.21 + fi * 0.04) + phase * 2.0) * 0.34,
            lerp(7.0, 9.5, scene) + fi * 4.8 + cos(t * 0.10 + phase) * 1.1);
        float glow = projectGlow(uv, lightPos, camPos, forward, right, up, aspect, 0.06 + fi * 0.01);
        float beam = projectGlow(uv, float3(lightPos.x, -0.85, lightPos.z),
                                 camPos, forward, right, up, aspect, 0.022);
        float3 lightColor = lerp(gPrimaryColor.rgb, gSecondaryColor.rgb, frac(0.27 * fi + scene * 0.31));
        color += lightColor * (glow * lerp(0.42 + 0.12 * fi, 0.16 + 0.04 * fi, scene) +
                               beam * lerp(0.16, 0.035, scene));
    }

    float sweep = exp(-abs(uv.y + 0.08 + sin(t * 0.23 + scene) * 0.12) * (16.0 + scene * 6.0));
    color += gPrimaryColor.rgb * sweep * lerp(0.05, 0.022, scene);

    if (scene > 0.5)
    {
        float bottomDust = saturate(1.0 - input.uv.y * 1.8);
        float cloud = 0.5 + 0.5 * sin(uv.x * 2.8 + t * 0.06) * sin(uv.y * 7.0 - t * 0.03);
        color += bottomDust * cloud * (gSecondaryColor.rgb * 0.030 + gPrimaryColor.rgb * 0.018);
    }

    float vignette = saturate(1.08 - dot(input.uv - 0.5, input.uv - 0.5) * 1.55);
    color *= vignette;
    color = pow(saturate(color), lerp(0.95, 1.04, scene));
    return float4(color, 1.0);
}
)";

double clamp_gameplay_judgement_line(double value) {
    if (!std::isfinite(value)) {
        return kGameplayJudgementLineDefault;
    }
    return std::clamp(value, kGameplayJudgementLineMin, kGameplayJudgementLineMax);
}

float clamp_gameplay_note_width_scale(double value) {
    if (!std::isfinite(value)) {
        return 1.0f;
    }
    return static_cast<float>(std::clamp(value, kGameplayNoteWidthScaleMin, kGameplayNoteWidthScaleMax));
}

std::string normalize_gameplay_skin_source(std::string_view value) {
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized == "osu" ? "osu" : "native";
}

std::wstring gameplay_feedback_overlay_text(std::string_view feedback) {
    if (feedback == "PG") {
        return L"P GREAT";
    }
    if (feedback == "GR") {
        return L"GREAT";
    }
    if (feedback == "G") {
        return L"GOOD";
    }
    if (feedback == "BAD") {
        return L"BAD";
    }
    if (feedback.empty()) {
        return {};
    }
    return std::wstring(feedback.begin(), feedback.end());
}

D2D1_COLOR_F gameplay_feedback_color(std::string_view feedback) {
    if (feedback == "PG") {
        return D2D1::ColorF(0x5EE5A7);
    }
    if (feedback == "GR") {
        return D2D1::ColorF(0x6EE7F2);
    }
    if (feedback == "G") {
        return D2D1::ColorF(0xFAE36E);
    }
    if (feedback == "BAD") {
        return D2D1::ColorF(0xFF9F43);
    }
    return D2D1::ColorF(0xE8ECF1);
}

float clamp_gameplay_note_height_scale(double value) {
    if (!std::isfinite(value)) {
        return static_cast<float>(kGameplayNoteHeightScaleDefault);
    }
    return static_cast<float>(std::clamp(value, kGameplayNoteHeightScaleMin, kGameplayNoteHeightScaleMax));
}

float clamp_gameplay_lane_divider_width_scale(double value) {
    if (!std::isfinite(value)) {
        return static_cast<float>(kGameplayLaneDividerWidthScaleDefault);
    }
    return static_cast<float>(std::clamp(
        value,
        kGameplayLaneDividerWidthScaleMin,
        kGameplayLaneDividerWidthScaleMax));
}

float gameplay_note_head_half_height(double note_height_scale) {
    return 11.0f * clamp_gameplay_note_height_scale(note_height_scale);
}

float gameplay_note_tail_half_height(double note_height_scale) {
    return 9.0f * clamp_gameplay_note_height_scale(note_height_scale);
}

float clamp_gameplay_hold_body_width_scale(double value) {
    if (!std::isfinite(value)) {
        return static_cast<float>(kGameplayHoldBodyWidthScaleDefault);
    }
    return static_cast<float>(std::clamp(value, kGameplayHoldBodyWidthScaleMin, kGameplayHoldBodyWidthScaleMax));
}

double clamp_gameplay_combo_position(double value) {
    if (!std::isfinite(value)) {
        return kGameplayComboPositionDefault;
    }
    return std::clamp(value, kGameplayComboPositionMin, kGameplayComboPositionMax);
}

std::size_t resolve_gameplay_lane_divider_widths(
    int lane_count,
    double lane_divider_width_scale,
    std::size_t imported_count,
    const std::array<float, kGameplayHudMaxLanes>& imported_widths,
    std::array<float, kGameplayHudMaxLanes>& out_widths) {
    out_widths.fill(0.0f);
    if (lane_count <= 1) {
        return 0;
    }

    const std::size_t divider_count = static_cast<std::size_t>(lane_count - 1);
    const float scale = clamp_gameplay_lane_divider_width_scale(lane_divider_width_scale);
    const float fallback_width =
        std::clamp(kGameplayLaneDividerBaseWidth * scale, 0.0f, kGameplayLaneDividerWidthMaxPx);
    const bool use_imported = imported_count == divider_count;
    for (std::size_t divider = 0; divider < divider_count; ++divider) {
        const float raw_width = use_imported ? imported_widths[divider] : kGameplayLaneDividerBaseWidth;
        out_widths[divider] =
            std::clamp(raw_width * scale, 0.0f, kGameplayLaneDividerWidthMaxPx);
        if (!use_imported) {
            out_widths[divider] = fallback_width;
        }
    }
    return divider_count;
}

std::string normalize_gameplay_note_shape(std::string_view value) {
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (normalized == "circle") {
        return "circle";
    }
    return "rect";
}

float gameplay_hold_body_cap_inset(std::string_view note_shape, float half_height) {
    return normalize_gameplay_note_shape(note_shape) == "circle" ? 0.0f : half_height;
}

float gameplay_note_draw_width(float lane_width, double note_width_scale) {
    const float safe_lane_width = std::max(24.0f, lane_width);
    const float base_note_width = std::max(16.0f, safe_lane_width - 16.0f);
    return std::clamp(base_note_width * clamp_gameplay_note_width_scale(note_width_scale),
                      16.0f,
                      std::max(16.0f, safe_lane_width - 4.0f));
}

struct GameplayFieldLayout {
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float lane_width = 0.0f;
    float note_width = 16.0f;
};

GameplayFieldLayout build_gameplay_field_layout(float bounds_left,
                                                float bounds_right,
                                                float top,
                                                float bottom,
                                                int lane_count,
                                                double note_width_scale) {
    const int clamped_lane_count = std::max(1, lane_count);
    const float bounds_width = std::max(160.0f, bounds_right - bounds_left);
    const float base_lane_width = bounds_width / static_cast<float>(clamped_lane_count);
    const float default_note_width = gameplay_note_draw_width(base_lane_width, 1.0);
    const float desired_note_width = gameplay_note_draw_width(base_lane_width, note_width_scale);
    const float shrink_ratio =
        (default_note_width > 1.0f) ? std::clamp(desired_note_width / default_note_width, 0.35f, 1.0f) : 1.0f;
    const float field_width = bounds_width * shrink_ratio;
    const float center_x = (bounds_left + bounds_right) * 0.5f;

    GameplayFieldLayout layout;
    layout.left = center_x - field_width * 0.5f;
    layout.right = center_x + field_width * 0.5f;
    layout.top = top;
    layout.bottom = bottom;
    layout.width = field_width;
    layout.height = std::max(1.0f, bottom - top);
    layout.lane_width = field_width / static_cast<float>(clamped_lane_count);
    layout.note_width = std::min(desired_note_width, std::max(16.0f, layout.lane_width - 4.0f));
    return layout;
}

D2D1_RECT_F gameplay_judgement_line_rect(const GameplayFieldLayout& field_layout,
                                         float hit_line_y,
                                         double note_height_scale) {
    const float half_h = gameplay_note_head_half_height(note_height_scale);
    return D2D1::RectF(field_layout.left,
                       std::max(field_layout.top + 2.0f, hit_line_y - half_h),
                       field_layout.right,
                       std::min(field_layout.bottom - 2.0f, hit_line_y + half_h));
}

float gameplay_field_y(float field_top, float field_height, double normalized_y) {
    return field_top + field_height * static_cast<float>(normalized_y);
}

D2D1_RECT_F centered_bitmap_source_rect(const D2D1_SIZE_F& bitmap_size,
                                        const D2D1_RECT_F& target_rect) {
    D2D1_RECT_F source_rect = D2D1::RectF(0.0f, 0.0f, bitmap_size.width, bitmap_size.height);
    const float target_width = std::max(1.0f, target_rect.right - target_rect.left);
    const float target_height = std::max(1.0f, target_rect.bottom - target_rect.top);
    const float target_aspect = target_width / target_height;
    const float bitmap_aspect = bitmap_size.width / std::max(1.0f, bitmap_size.height);
    if (bitmap_aspect > target_aspect) {
        const float cropped_width = bitmap_size.height * target_aspect;
        const float offset = (bitmap_size.width - cropped_width) * 0.5f;
        source_rect.left = offset;
        source_rect.right = offset + cropped_width;
    } else if (bitmap_aspect < target_aspect) {
        const float cropped_height = bitmap_size.width / target_aspect;
        const float offset = (bitmap_size.height - cropped_height) * 0.5f;
        source_rect.top = offset;
        source_rect.bottom = offset + cropped_height;
    }
    return source_rect;
}

D2D1_RECT_F full_bitmap_source_rect(const D2D1_SIZE_F& bitmap_size) {
    return D2D1::RectF(0.0f, 0.0f, bitmap_size.width, bitmap_size.height);
}

const D2D1_RECT_F* bitmap_source_rect_or_null(const D2D1_RECT_F& source_rect) {
    if (source_rect.right <= source_rect.left || source_rect.bottom <= source_rect.top) {
        return nullptr;
    }
    return &source_rect;
}

D2D1_SIZE_F bitmap_source_size(ID2D1Bitmap* bitmap, const D2D1_RECT_F* source_rect) {
    if (!bitmap) {
        return D2D1::SizeF(0.0f, 0.0f);
    }
    if (source_rect && source_rect->right > source_rect->left && source_rect->bottom > source_rect->top) {
        return D2D1::SizeF(source_rect->right - source_rect->left,
                           source_rect->bottom - source_rect->top);
    }
    return bitmap->GetSize();
}

D2D1_RECT_F fit_rect_preserve_aspect(const D2D1_RECT_F& bounds, const D2D1_SIZE_F& source_size) {
    const float bounds_width = std::max(0.0f, bounds.right - bounds.left);
    const float bounds_height = std::max(0.0f, bounds.bottom - bounds.top);
    if (bounds_width <= 0.0f || bounds_height <= 0.0f ||
        source_size.width <= 0.0f || source_size.height <= 0.0f) {
        return bounds;
    }

    const float scale = std::min(bounds_width / source_size.width, bounds_height / source_size.height);
    const float draw_width = std::max(1.0f, source_size.width * scale);
    const float draw_height = std::max(1.0f, source_size.height * scale);
    const float left = bounds.left + (bounds_width - draw_width) * 0.5f;
    const float top = bounds.top + (bounds_height - draw_height) * 0.5f;
    return D2D1::RectF(left, top, left + draw_width, top + draw_height);
}

D2D1_RECT_F inscribed_circle_bitmap_rect(const D2D1_RECT_F& bounds) {
    const float bounds_width = std::max(0.0f, bounds.right - bounds.left);
    const float bounds_height = std::max(0.0f, bounds.bottom - bounds.top);
    const float diameter = std::max(2.0f, std::min(bounds_width, bounds_height) - 2.0f);
    const float radius = diameter * 0.5f;
    const float center_x = (bounds.left + bounds.right) * 0.5f;
    const float center_y = (bounds.top + bounds.bottom) * 0.5f;
    return D2D1::RectF(center_x - radius, center_y - radius, center_x + radius, center_y + radius);
}

D2D1_RECT_F gameplay_note_bitmap_dest_rect(const D2D1_RECT_F& note_rect,
                                           ID2D1Bitmap* bitmap,
                                           const D2D1_RECT_F* source_rect,
                                           std::string_view note_shape,
                                           bool preserve_aspect_ratio) {
    const std::string normalized_shape = normalize_gameplay_note_shape(note_shape);
    const D2D1_RECT_F base_rect =
        normalized_shape == "circle" ? inscribed_circle_bitmap_rect(note_rect) : note_rect;
    if (!preserve_aspect_ratio || !bitmap) {
        return base_rect;
    }
    return fit_rect_preserve_aspect(base_rect, bitmap_source_size(bitmap, source_rect));
}

bool create_composited_gameplay_note_bitmap(ID2D1DeviceContext* d2d_context,
                                            ID2D1Factory1* d2d_factory,
                                            ID2D1Bitmap* source_bitmap,
                                            const D2D1_RECT_F* source_rect,
                                            const D2D1_SIZE_F& target_size,
                                            bool clip_circle,
                                            bool preserve_aspect_ratio,
                                            bool draw_border,
                                            const D2D1_COLOR_F& border_color,
                                            Microsoft::WRL::ComPtr<ID2D1Bitmap>& out_bitmap,
                                            D2D1_RECT_F& out_source_rect) {
    if (!d2d_context || !d2d_factory || !source_bitmap ||
        target_size.width <= 0.0f || target_size.height <= 0.0f) {
        return false;
    }

    Microsoft::WRL::ComPtr<ID2D1BitmapRenderTarget> render_target;
    const HRESULT create_hr = d2d_context->CreateCompatibleRenderTarget(target_size, &render_target);
    if (FAILED(create_hr) || !render_target) {
        return false;
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> border_brush;
    if (draw_border &&
        FAILED(render_target->CreateSolidColorBrush(border_color, &border_brush))) {
        return false;
    }

    const D2D1_RECT_F full_rect = D2D1::RectF(0.0f, 0.0f, target_size.width, target_size.height);
    const D2D1_RECT_F content_rect = D2D1::RectF(1.0f, 1.0f, target_size.width - 1.0f, target_size.height - 1.0f);
    const D2D1_RECT_F draw_rect = preserve_aspect_ratio
        ? fit_rect_preserve_aspect(content_rect, bitmap_source_size(source_bitmap, source_rect))
        : content_rect;

    render_target->BeginDraw();
    render_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    if (clip_circle) {
        const D2D1_RECT_F circle_rect = inscribed_circle_bitmap_rect(full_rect);
        const D2D1_ELLIPSE ellipse = D2D1::Ellipse(
            D2D1::Point2F((circle_rect.left + circle_rect.right) * 0.5f,
                          (circle_rect.top + circle_rect.bottom) * 0.5f),
            (circle_rect.right - circle_rect.left) * 0.5f,
            (circle_rect.bottom - circle_rect.top) * 0.5f);
        Microsoft::WRL::ComPtr<ID2D1EllipseGeometry> clip_geometry;
        Microsoft::WRL::ComPtr<ID2D1Layer> clip_layer;
        const bool has_clip_layer =
            SUCCEEDED(d2d_factory->CreateEllipseGeometry(ellipse, &clip_geometry)) &&
            clip_geometry &&
            SUCCEEDED(render_target->CreateLayer(&clip_layer)) &&
            clip_layer;
        if (has_clip_layer) {
            render_target->PushLayer(
                D2D1::LayerParameters(full_rect, clip_geometry.Get(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE),
                clip_layer.Get());
        }
        render_target->DrawBitmap(source_bitmap, draw_rect, 1.0f,
                                  D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source_rect);
        if (has_clip_layer) {
            render_target->PopLayer();
        }
        if (draw_border && border_brush) {
            render_target->DrawEllipse(ellipse, border_brush.Get(), 1.3f);
        }
    } else {
        render_target->DrawBitmap(source_bitmap, draw_rect, 1.0f,
                                  D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source_rect);
    }

    const HRESULT end_hr = render_target->EndDraw();
    if (FAILED(end_hr)) {
        return false;
    }

    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    const HRESULT bitmap_hr = render_target->GetBitmap(&bitmap);
    if (FAILED(bitmap_hr) || !bitmap) {
        return false;
    }

    out_source_rect = full_bitmap_source_rect(bitmap->GetSize());
    out_bitmap = std::move(bitmap);
    return true;
}

bool compute_bitmap_alpha_source_rect(IWICBitmapSource* bitmap_source, D2D1_RECT_F& out_source_rect) {
    if (!bitmap_source) {
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    if (FAILED(bitmap_source->GetSize(&width, &height)) || width == 0 || height == 0) {
        return false;
    }

    const UINT stride = width * 4;
    std::vector<BYTE> pixels(static_cast<std::size_t>(stride) * static_cast<std::size_t>(height), 0);
    if (FAILED(bitmap_source->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()))) {
        return false;
    }

    UINT min_x = width;
    UINT min_y = height;
    UINT max_x = 0;
    UINT max_y = 0;
    bool found_opaque_pixel = false;
    for (UINT y = 0; y < height; ++y) {
        const BYTE* row = pixels.data() + static_cast<std::size_t>(y) * stride;
        for (UINT x = 0; x < width; ++x) {
            const BYTE alpha = row[static_cast<std::size_t>(x) * 4 + 3];
            if (alpha == 0) {
                continue;
            }
            found_opaque_pixel = true;
            min_x = std::min(min_x, x);
            min_y = std::min(min_y, y);
            max_x = std::max(max_x, x);
            max_y = std::max(max_y, y);
        }
    }

    if (!found_opaque_pixel) {
        out_source_rect = D2D1::RectF(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        return true;
    }

    out_source_rect = D2D1::RectF(static_cast<float>(min_x),
                                  static_cast<float>(min_y),
                                  static_cast<float>(max_x + 1),
                                  static_cast<float>(max_y + 1));
    return true;
}

bool load_bitmap_from_utf8_path(IWICImagingFactory* wic_factory,
                                ID2D1DeviceContext* d2d_context,
                                std::string_view path,
                                Microsoft::WRL::ComPtr<ID2D1Bitmap>& out_bitmap,
                                D2D1_RECT_F* out_source_rect = nullptr,
                                bool trim_transparent_alpha = false) {
    if (!wic_factory || !d2d_context || path.empty()) {
        return false;
    }

    const std::wstring wide_path = util::path_from_utf8_lossy(path).native();
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    const HRESULT decoder_hr = wic_factory->CreateDecoderFromFilename(
        wide_path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(decoder_hr) || !decoder) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame)) || !frame) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(wic_factory->CreateFormatConverter(&converter)) || !converter) {
        return false;
    }
    const HRESULT convert_hr = converter->Initialize(frame.Get(),
                                                     GUID_WICPixelFormat32bppPBGRA,
                                                     WICBitmapDitherTypeNone,
                                                     nullptr,
                                                     0.0f,
                                                     WICBitmapPaletteTypeCustom);
    if (FAILED(convert_hr)) {
        return false;
    }

    D2D1_RECT_F source_rect = D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    UINT width = 0;
    UINT height = 0;
    if (SUCCEEDED(converter->GetSize(&width, &height))) {
        source_rect = D2D1::RectF(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    }
    if (trim_transparent_alpha) {
        static_cast<void>(compute_bitmap_alpha_source_rect(converter.Get(), source_rect));
    }

    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    const D2D1_BITMAP_PROPERTIES properties = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    const HRESULT bitmap_hr = d2d_context->CreateBitmapFromWicBitmap(converter.Get(), &properties, &bitmap);
    if (FAILED(bitmap_hr) || !bitmap) {
        return false;
    }

    out_bitmap = std::move(bitmap);
    if (out_source_rect) {
        *out_source_rect = source_rect;
    }
    return true;
}

float gameplay_osu_gear_top(const GameplayFieldLayout& field_layout,
                            float hit_line_y,
                            double note_height_scale) {
    const D2D1_RECT_F judgement_rect =
        gameplay_judgement_line_rect(field_layout, hit_line_y, note_height_scale);
    const float judgement_mid = judgement_rect.top + (judgement_rect.bottom - judgement_rect.top) * 0.5f;
    return std::clamp(judgement_mid, field_layout.top + 2.0f, field_layout.bottom - 4.0f);
}

D2D1_RECT_F gameplay_key_bitmap_rect(const GameplayFieldLayout& field_layout,
                                     float hit_line_y,
                                     double note_height_scale,
                                     float lane_center,
                                     float lane_width,
                                     float note_width,
                                     const D2D1_SIZE_F& bitmap_size,
                                     bool osu_gear_layout) {
    const float safe_bitmap_width = std::max(1.0f, bitmap_size.width);
    const float safe_bitmap_height = std::max(1.0f, bitmap_size.height);
    if (osu_gear_layout) {
        const float gear_top = gameplay_osu_gear_top(field_layout, hit_line_y, note_height_scale);
        const float gear_bottom = std::max(gear_top + 4.0f, field_layout.bottom - 2.0f);
        const float lane_left = lane_center - lane_width * 0.5f;
        const float lane_right = lane_center + lane_width * 0.5f;
        return D2D1::RectF(lane_left + 2.0f,
                           gear_top,
                           lane_right - 2.0f,
                           gear_bottom);
    }

    const float receptor_width = note_width * 1.04f;
    const float receptor_height =
        std::clamp(receptor_width * (safe_bitmap_height / safe_bitmap_width), 22.0f, 96.0f);
    const float receptor_bottom = std::min(field_layout.bottom - 8.0f, hit_line_y + receptor_height * 0.22f);
    return D2D1::RectF(lane_center - receptor_width * 0.5f,
                       receptor_bottom - receptor_height,
                       lane_center + receptor_width * 0.5f,
                       receptor_bottom);
}

struct MonitorDisplayInfo {
    RECT rect{0, 0, 0, 0};
    RECT work_rect{0, 0, 0, 0};
    UINT width = 0;
    UINT height = 0;
    UINT refresh_hz = 60;
};

MonitorDisplayInfo query_monitor_display_info(HWND hwnd) {
    POINT point{0, 0};
    if (hwnd && IsWindow(hwnd)) {
        RECT window_rect{};
        if (GetWindowRect(hwnd, &window_rect)) {
            point.x = (window_rect.left + window_rect.right) / 2;
            point.y = (window_rect.top + window_rect.bottom) / 2;
        }
    } else if (!GetCursorPos(&point)) {
        point = {0, 0};
    }

    MonitorDisplayInfo info{};
    HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (GetMonitorInfoW(monitor, &monitor_info)) {
        info.rect = monitor_info.rcMonitor;
        info.work_rect = monitor_info.rcWork;
        info.width = static_cast<UINT>(monitor_info.rcMonitor.right - monitor_info.rcMonitor.left);
        info.height = static_cast<UINT>(monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top);

        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (EnumDisplaySettingsW(monitor_info.szDevice, ENUM_CURRENT_SETTINGS, &mode) &&
            mode.dmDisplayFrequency > 0) {
            info.refresh_hz = static_cast<UINT>(mode.dmDisplayFrequency);
        }
    }

    if (info.width == 0) {
        info.rect = RECT{0, 0, 1280, 720};
        info.work_rect = info.rect;
        info.width = 1280;
        info.height = 720;
    }
    return info;
}

DWORD window_style_for_display_mode(std::string_view display_mode) {
    if (display_mode == "windowed") {
        return WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    }
    return WS_POPUP;
}

DWORD window_ex_style_for_display_mode(std::string_view display_mode) {
    static_cast<void>(display_mode);
    return WS_EX_APPWINDOW;
}

SIZE window_size_for_client_area(UINT client_width, UINT client_height, DWORD style, DWORD ex_style) {
    RECT rect{0, 0, static_cast<LONG>(client_width), static_cast<LONG>(client_height)};
    if (AdjustWindowRectEx(&rect, style, FALSE, ex_style) == 0) {
        return SIZE{static_cast<LONG>(client_width), static_cast<LONG>(client_height)};
    }
    return SIZE{rect.right - rect.left, rect.bottom - rect.top};
}

void resolve_window_bounds(const MenuWindowConfig& config,
                           const MonitorDisplayInfo& monitor,
                           UINT& out_width,
                           UINT& out_height,
                           int& out_x,
                           int& out_y) {
    const UINT desired_width =
        (config.width > 0) ? static_cast<UINT>(config.width) : monitor.width;
    const UINT desired_height =
        (config.height > 0) ? static_cast<UINT>(config.height) : monitor.height;

    out_width = (monitor.width > 0) ? std::min(desired_width, monitor.width) : desired_width;
    out_height = (monitor.height > 0) ? std::min(desired_height, monitor.height) : desired_height;
    if (out_width == 0) {
        out_width = 1280;
    }
    if (out_height == 0) {
        out_height = 720;
    }

    if (config.display_mode == "fullscreen") {
        out_x = monitor.rect.left;
        out_y = monitor.rect.top;
        return;
    }

    const RECT placement_rect = (config.display_mode == "windowed") ? monitor.work_rect : monitor.rect;
    const DWORD style = window_style_for_display_mode(config.display_mode);
    const DWORD ex_style = window_ex_style_for_display_mode(config.display_mode);
    const SIZE window_size = window_size_for_client_area(out_width, out_height, style, ex_style);
    const int placement_width = placement_rect.right - placement_rect.left;
    const int placement_height = placement_rect.bottom - placement_rect.top;
    out_x = placement_rect.left + std::max<int>(0, (placement_width - static_cast<int>(window_size.cx)) / 2);
    out_y = placement_rect.top + std::max<int>(0, (placement_height - static_cast<int>(window_size.cy)) / 2);
}

DXGI_MODE_DESC build_target_mode_desc(const MenuWindowConfig& config, UINT width, UINT height) {
    DXGI_MODE_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.RefreshRate.Numerator = static_cast<UINT>(std::clamp(config.refresh_hz, 60, 1050));
    desc.RefreshRate.Denominator = 1;
    desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    return desc;
}

void apply_fullscreen_target(IDXGISwapChain* swap_chain,
                             const MenuWindowConfig& config,
                             UINT width,
                             UINT height) {
    if (!swap_chain || width == 0 || height == 0) {
        return;
    }
    const DXGI_MODE_DESC target = build_target_mode_desc(config, width, height);
    const HRESULT hr = swap_chain->ResizeTarget(&target);
    if (FAILED(hr)) {
        std::cerr << "[MenuWindow] ResizeTarget failed hr=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
    }
}

bool gameplay_lane_uses_white_note(int lane) {
    switch (lane) {
    case 1:
    case 3:
    case 5:
    case 6:
    case 8:
    case 10:
        return true;
    default:
        return false;
    }
}

std::wstring to_wide(const std::string& text) {
    return util::wide_from_utf8_lossy(util::sanitize_ui_text(text));
}

void warn_invalid_ui_text_once(std::string_view context, std::string_view raw_text) {
    if (raw_text.empty()) {
        return;
    }

    static std::mutex warned_mutex;
    static std::unordered_set<std::string> warned_keys;
    const std::string key =
        std::string(context) + ":" + std::to_string(std::hash<std::string_view>{}(raw_text));

    std::lock_guard<std::mutex> lock(warned_mutex);
    if (!warned_keys.insert(key).second) {
        return;
    }
    std::cerr << "[warn] Replaced invalid UI text while drawing " << context << "." << std::endl;
}

std::wstring to_wide_with_placeholder(std::string_view text,
                                      std::string_view placeholder,
                                      std::string_view context) {
    const std::string cleaned = util::sanitize_ui_text(text);
    if (!cleaned.empty()) {
        return util::wide_from_utf8_lossy(cleaned);
    }
    if (text.empty()) {
        return {};
    }
    warn_invalid_ui_text_once(context, text);
    return util::wide_from_utf8_lossy(std::string(placeholder));
}

std::string wide_to_utf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                    nullptr, 0, nullptr, nullptr);
    if (count <= 0) {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        utf8.data(), count, nullptr, nullptr);
    return utf8;
}

bool show_fatal_error(const std::string& message) {
    const std::wstring wide = to_wide(message);
    MessageBoxW(nullptr,
                wide.empty() ? L"TenRiff failed to initialize the menu renderer." : wide.c_str(),
                L"TenRiff - Renderer Error",
                MB_OK | MB_ICONERROR);
    return false;
}

bool line_is_selected(const std::string& line) {
    return line.size() >= 2 && line[0] == '>' && line[1] == ' ';
}

bool line_has_prefix(const std::string& line) {
    if (line.size() < 2) {
        return false;
    }
    if (line[0] == '>' && line[1] == ' ') {
        return true;
    }
    return line[0] == ' ' && line[1] == ' ';
}

std::string strip_prefix(const std::string& line) {
    if (line_has_prefix(line)) {
        return line.substr(2);
    }
    return line;
}

std::string format_int_with_commas(int64_t value) {
    bool negative = value < 0;
    uint64_t abs_value = 0;
    if (negative) {
        abs_value = static_cast<uint64_t>(-(value + 1)) + 1;
    } else {
        abs_value = static_cast<uint64_t>(value);
    }
    std::string digits = std::to_string(abs_value);
    std::string out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count == 3) {
            out.push_back(',');
            count = 0;
        }
        out.push_back(*it);
        ++count;
    }
    if (negative) {
        out.push_back('-');
    }
    std::reverse(out.begin(), out.end());
    return out;
}

std::string format_decimal(double value, int precision = 2) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(precision);
    stream << value;
    return stream.str();
}

std::string format_signed_ms(double value, int precision = 2) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(precision);
    if (value >= 0.0) {
        stream << '+';
    }
    stream << value << "ms";
    return stream.str();
}

uint32_t fnv1a_32(std::string_view value) {
    uint32_t hash = 2166136261u;
    for (unsigned char ch : value) {
        hash ^= ch;
        hash *= 16777619u;
    }
    return hash;
}

D2D1_COLOR_F jacket_color(std::string_view title) {
    const uint32_t hash = fnv1a_32(title);
    const float r = 0.35f + 0.45f * static_cast<float>((hash >> 16) & 0xFF) / 255.0f;
    const float g = 0.35f + 0.45f * static_cast<float>((hash >> 8) & 0xFF) / 255.0f;
    const float b = 0.35f + 0.45f * static_cast<float>(hash & 0xFF) / 255.0f;
    return D2D1::ColorF(r, g, b, 1.0f);
}

uint32_t blend_rgb(uint32_t a, uint32_t b, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const auto lerp_channel = [clamped](uint32_t lhs, uint32_t rhs, int shift) -> uint32_t {
        const float l = static_cast<float>((lhs >> shift) & 0xFF);
        const float r = static_cast<float>((rhs >> shift) & 0xFF);
        return static_cast<uint32_t>(std::clamp(std::lround(l + (r - l) * clamped), 0l, 255l));
    };
    return (lerp_channel(a, b, 16) << 16) | (lerp_channel(a, b, 8) << 8) | lerp_channel(a, b, 0);
}

D2D1_COLOR_F color_from_rgb(uint32_t rgb, float alpha = 1.0f) {
    const float r = static_cast<float>((rgb >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((rgb >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(rgb & 0xFF) / 255.0f;
    return D2D1::ColorF(r, g, b, alpha);
}

float unit_hash_01(uint32_t seed) {
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777215.0f;
}

double pulse_wave_01(int64_t now_ns, double period_sec, double phase = 0.0) {
    if (period_sec <= 0.0) {
        return 0.5;
    }
    constexpr double kTau = 6.28318530717958647692;
    const double seconds = static_cast<double>(now_ns) / 1'000'000'000.0;
    return 0.5 + 0.5 * std::sin((seconds / period_sec + phase) * kTau);
}

D2D1_COLOR_F gameplay_note_fill_color(uint32_t rgb) {
    return color_from_rgb(rgb, 0.97f);
}

D2D1_COLOR_F gameplay_note_border_color(uint32_t rgb) {
    return color_from_rgb(blend_rgb(rgb, 0xFFFFFF, 0.55f), 0.98f);
}

D2D1_COLOR_F gameplay_note_hold_color(uint32_t rgb) {
    return color_from_rgb(blend_rgb(rgb, 0xFFFFFF, 0.18f), 0.34f);
}

D2D1_COLOR_F gameplay_lane_preview_fill(uint32_t rgb, bool selected) {
    return color_from_rgb(blend_rgb(rgb, 0xFFFFFF, selected ? 0.10f : 0.04f), selected ? 0.32f : 0.22f);
}

void draw_note_primitive(ID2D1RenderTarget* target,
                         const D2D1_RECT_F& rect,
                         ID2D1Brush* fill,
                         ID2D1Brush* border,
                         float border_width,
                         std::string_view note_shape,
                         bool draw_border) {
    if (!target || !fill) {
        return;
    }

    const std::string normalized_shape = normalize_gameplay_note_shape(note_shape);
    const D2D1_ANTIALIAS_MODE saved_antialias = target->GetAntialiasMode();
    target->SetAntialiasMode(normalized_shape == "circle"
                                 ? D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
                                 : D2D1_ANTIALIAS_MODE_ALIASED);

    if (normalized_shape == "circle") {
        const float diameter = std::max(2.0f, std::min(rect.right - rect.left, rect.bottom - rect.top) - 2.0f);
        const float radius = diameter * 0.5f;
        const D2D1_ELLIPSE ellipse = D2D1::Ellipse(
            D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f),
            radius,
            radius);
        target->FillEllipse(ellipse, fill);
        if (draw_border && border) {
            target->DrawEllipse(ellipse, border, border_width);
        }
    } else {
        target->FillRectangle(rect, fill);
        if (draw_border && border) {
            target->DrawRectangle(rect, border, border_width);
        }
    }

    target->SetAntialiasMode(saved_antialias);
}

void set_brush_points(ID2D1LinearGradientBrush* brush, const D2D1_RECT_F& rect) {
    if (!brush) {
        return;
    }
    brush->SetStartPoint(D2D1::Point2F(rect.left, rect.top));
    brush->SetEndPoint(D2D1::Point2F(rect.right, rect.bottom));
}

bool menu_scene_enabled(MenuScreenKind kind) {
    return kind == MenuScreenKind::TitleMenu || kind == MenuScreenKind::SongSelect;
}

LRESULT CALLBACK menu_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* window = static_cast<MenuWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    auto* window = reinterpret_cast<MenuWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CLOSE:
            if (window) {
                window->request_close();
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_SETCURSOR:
            if (window && window->cursor_hidden()) {
                SetCursor(nullptr);
                return TRUE;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (window && wparam != SIZE_MINIMIZED) {
                window->queue_resize(LOWORD(lparam), HIWORD(lparam));
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (window) {
                window->on_mouse_button_down(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                return 0;
            }
            break;
        case WM_LBUTTONDBLCLK:
            if (window) {
                window->on_mouse_click(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), true);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (window) {
                window->on_mouse_click(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), false);
                return 0;
            }
            break;
        case WM_RBUTTONUP:
            if (window) {
                window->on_mouse_secondary_click(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                return 0;
            }
            break;
        case WM_MOUSEMOVE:
            if (window) {
                window->on_mouse_move(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
            if (window) {
                window->on_mouse_wheel(GET_WHEEL_DELTA_WPARAM(wparam));
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            return 0;
        case WM_DROPFILES:
            if (window) {
                HDROP drop = reinterpret_cast<HDROP>(wparam);
                const UINT count = DragQueryFileW(drop, 0xFFFFFFFFu, nullptr, 0);
                for (UINT i = 0; i < count; ++i) {
                    const UINT chars = DragQueryFileW(drop, i, nullptr, 0);
                    if (chars == 0) {
                        continue;
                    }
                    std::wstring path(static_cast<std::size_t>(chars) + 1, L'\0');
                    DragQueryFileW(drop, i, path.data(), chars + 1);
                    path.resize(static_cast<std::size_t>(chars));
                    window->on_file_drop(wide_to_utf8(path));
                }
                DragFinish(drop);
                return 0;
            }
            break;
        case WM_CONTEXTMENU:
            return 0;
        default:
            break;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void configure_low_latency_presentation(IDXGIDevice* dxgi_device, IDXGISwapChain1* swap_chain) {
    if (swap_chain) {
        Microsoft::WRL::ComPtr<IDXGISwapChain2> swap_chain2;
        if (SUCCEEDED(swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain2))) && swap_chain2) {
            const HRESULT hr = swap_chain2->SetMaximumFrameLatency(1);
            if (FAILED(hr)) {
                std::cerr << "[MenuWindow] IDXGISwapChain2::SetMaximumFrameLatency(1) failed hr=0x"
                          << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            }
            return;
        }
    }

    if (dxgi_device) {
        Microsoft::WRL::ComPtr<IDXGIDevice1> dxgi_device1;
        if (SUCCEEDED(dxgi_device->QueryInterface(IID_PPV_ARGS(&dxgi_device1))) && dxgi_device1) {
            const HRESULT hr = dxgi_device1->SetMaximumFrameLatency(1);
            if (FAILED(hr)) {
                std::cerr << "[MenuWindow] IDXGIDevice1::SetMaximumFrameLatency(1) failed hr=0x"
                          << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            }
        }
    }
}

}  // namespace

struct MenuWindow::D2DResources {
    struct SongCardPreviewBitmapEntry {
        bool attempted = false;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    };

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> menu_scene_target_view;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> menu_scene_vertex_shader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> menu_scene_pixel_shader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> menu_scene_constant_buffer;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2d_factory;
    Microsoft::WRL::ComPtr<ID2D1Device> d2d_device;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2d_context;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_target;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_factory;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> title_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> option_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> body_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> mono_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> logo_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> menu_button_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> menu_icon_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> header_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> song_title_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> song_artist_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> hud_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> rank_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> stats_label_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> stats_value_format;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> text_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> accent_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> judgement_line_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> muted_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> card_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> panel_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> footer_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> button_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> button_selected_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> button_border_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> note_fill_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> note_border_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> note_hold_brush;
    std::array<Microsoft::WRL::ComPtr<ID2D1Bitmap>, kGameplayHudMaxLanes> lane_note_head_bitmaps{};
    std::array<D2D1_RECT_F, kGameplayHudMaxLanes> lane_note_head_source_rects{};
    std::array<Microsoft::WRL::ComPtr<ID2D1Bitmap>, kGameplayHudMaxLanes> lane_note_hold_head_bitmaps{};
    std::array<D2D1_RECT_F, kGameplayHudMaxLanes> lane_note_hold_head_source_rects{};
    std::array<Microsoft::WRL::ComPtr<ID2D1Bitmap>, kGameplayHudMaxLanes> lane_note_hold_body_bitmaps{};
    std::array<D2D1_RECT_F, kGameplayHudMaxLanes> lane_note_hold_body_source_rects{};
    std::array<Microsoft::WRL::ComPtr<ID2D1Bitmap>, kGameplayHudMaxLanes> lane_note_tail_bitmaps{};
    std::array<D2D1_RECT_F, kGameplayHudMaxLanes> lane_note_tail_source_rects{};
    std::array<Microsoft::WRL::ComPtr<ID2D1Bitmap>, kGameplayHudMaxLanes> lane_key_idle_bitmaps{};
    std::array<D2D1_RECT_F, kGameplayHudMaxLanes> lane_key_idle_source_rects{};
    std::array<Microsoft::WRL::ComPtr<ID2D1Bitmap>, kGameplayHudMaxLanes> lane_key_pressed_bitmaps{};
    std::array<D2D1_RECT_F, kGameplayHudMaxLanes> lane_key_pressed_source_rects{};
    Microsoft::WRL::ComPtr<ID2D1Bitmap> song_select_preview_bitmap;
    std::unordered_map<std::string, SongCardPreviewBitmapEntry> song_card_preview_bitmaps{};
    std::deque<std::string> song_card_preview_lru{};
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> glow_stops;
    Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> glow_brush;
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> logo_stops;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> logo_brush;
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> play_stops;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> play_brush;
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> edit_stops;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> edit_brush;
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> options_stops;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> options_brush;
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> exit_stops;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> exit_brush;
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> bg_stops;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> bg_brush;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> performance_graph_geometry;
    Microsoft::WRL::ComPtr<ID2D1CommandList> gameplay_static_command_list;
};

MenuWindow::MenuWindow() : d2d_(std::make_unique<D2DResources>()) {}

MenuWindow::~MenuWindow() {
    shutdown();
}

void MenuWindow::set_config(const MenuWindowConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    pending_config_ = config;
    config_dirty_ = true;
}

bool MenuWindow::fail_fatal(std::string_view message) {
    fatal_error_.store(true, std::memory_order_release);
    should_close_.store(true, std::memory_order_release);
    show_fatal_error(std::string(message));
    return false;
}

void MenuWindow::render(const MenuRenderData& data) {
    apply_pending_config();
    update_cursor_visibility(data.kind == MenuScreenKind::GameplayHud);
    static int skip_log_count = 0;
    const bool initialized = initialized_.load(std::memory_order_acquire);
    const bool should_close = should_close_.load(std::memory_order_acquire);
    if (!initialized || should_close) {
        if (skip_log_count++ < 5) {
            std::cerr << "[MenuWindow::render] skipped: initialized_=" << initialized
                      << ", should_close_=" << should_close << std::endl;
        }
        return;
    }
    skip_log_count = 0;
    pump_messages();
    if (should_close_.load(std::memory_order_acquire)) {
        return;
    }

    const HWND hwnd = static_cast<HWND>(hwnd_);
    const bool window_minimized = hwnd && IsIconic(hwnd);
    const bool window_in_foreground = is_input_foreground();
    const bool fullscreen_requested = config_.display_mode == "fullscreen";
    if (d2d_ && d2d_->swap_chain && fullscreen_requested) {
        if ((window_minimized || !window_in_foreground) && fullscreen_) {
            const HRESULT fs_hr = d2d_->swap_chain->SetFullscreenState(FALSE, nullptr);
            if (FAILED(fs_hr)) {
                std::cerr << "[MenuWindow::render] SetFullscreenState(FALSE) during background transition failed hr=0x"
                          << std::hex << static_cast<unsigned long>(fs_hr) << std::dec << std::endl;
            }
            fullscreen_ = false;
            fullscreen_restore_pending_ = true;
        } else if (!window_minimized && window_in_foreground &&
                   fullscreen_restore_pending_ && !fullscreen_) {
            const HRESULT fs_hr = d2d_->swap_chain->SetFullscreenState(TRUE, nullptr);
            if (SUCCEEDED(fs_hr)) {
                fullscreen_ = true;
                fullscreen_restore_pending_ = false;
                apply_fullscreen_target(d2d_->swap_chain.Get(), config_, width_, height_);
            } else {
                std::cerr << "[MenuWindow::render] SetFullscreenState(TRUE) during foreground restore failed hr=0x"
                          << std::hex << static_cast<unsigned long>(fs_hr) << std::dec << std::endl;
            }
        }
    } else {
        fullscreen_restore_pending_ = false;
    }

    if (window_minimized || (fullscreen_requested && !window_in_foreground)) {
        return;
    }
    draw(data);
}

void MenuWindow::request_close() {
    should_close_.store(true, std::memory_order_release);
}

void MenuWindow::queue_resize(unsigned int width, unsigned int height) {
    if (width == 0 || height == 0) {
        return;
    }
    pending_width_ = width;
    pending_height_ = height;
    resize_pending_ = true;
}

std::optional<MenuClickEvent> MenuWindow::poll_click_event() {
    std::lock_guard<std::mutex> lock(click_events_mutex_);
    if (click_events_.empty()) {
        return std::nullopt;
    }
    MenuClickEvent event = click_events_.front();
    click_events_.pop_front();
    return event;
}

void MenuWindow::push_click_event(MenuClickEvent event) {
    const bool is_virtual_event = event.kind == MenuHitTargetKind::MouseWheel || event.kind == MenuHitTargetKind::FileDrop;
    if (event.kind == MenuHitTargetKind::None) {
        return;
    }
    if (!is_virtual_event && event.index < 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(click_events_mutex_);
    if (click_events_.size() >= 64) {
        click_events_.pop_front();
    }
    click_events_.push_back(std::move(event));
}

void MenuWindow::update_cursor_visibility(bool hidden) {
    if (cursor_hidden_ == hidden) {
        return;
    }
    cursor_hidden_ = hidden;

    if (!hwnd_) {
        return;
    }
    const HWND hwnd = static_cast<HWND>(hwnd_);
    if (!IsWindow(hwnd)) {
        return;
    }

    if (cursor_hidden_) {
        SetCursor(nullptr);
    } else {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
}

void MenuWindow::invalidate_gameplay_note_sprite_cache() {
    gameplay_note_sprite_cache_ = {};
    if (!d2d_) {
        return;
    }
    for (auto& bitmap : d2d_->lane_note_head_bitmaps) {
        bitmap.Reset();
    }
    for (auto& bitmap : d2d_->lane_note_hold_head_bitmaps) {
        bitmap.Reset();
    }
    for (auto& bitmap : d2d_->lane_note_hold_body_bitmaps) {
        bitmap.Reset();
    }
    for (auto& bitmap : d2d_->lane_note_tail_bitmaps) {
        bitmap.Reset();
    }
    for (auto& bitmap : d2d_->lane_key_idle_bitmaps) {
        bitmap.Reset();
    }
    for (auto& bitmap : d2d_->lane_key_pressed_bitmaps) {
        bitmap.Reset();
    }
    d2d_->lane_note_head_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
    d2d_->lane_note_hold_head_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
    d2d_->lane_note_hold_body_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
    d2d_->lane_note_tail_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
    d2d_->lane_key_idle_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
    d2d_->lane_key_pressed_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
}

bool MenuWindow::ensure_gameplay_note_sprites(const GameplayHudData& data) {
    if (!d2d_ || !d2d_->d2d_context) {
        return false;
    }
    const int lane_count = std::clamp(data.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
    const bool note_border_enabled = data.note_border_enabled;
    const std::string note_shape = normalize_gameplay_note_shape(data.note_shape);
    const bool preserve_note_image_aspect_ratio = data.preserve_note_image_aspect_ratio;
    const std::string skin_source = normalize_gameplay_skin_source(data.skin_source);
    const bool use_osu_skin =
        skin_source == "osu" && !data.osu_skin_root.empty() && !data.osu_skin_name.empty();
    bool cache_valid = gameplay_note_sprite_cache_.lane_count == lane_count &&
                       gameplay_note_sprite_cache_.note_border_enabled == note_border_enabled &&
                       gameplay_note_sprite_cache_.note_shape == note_shape &&
                       gameplay_note_sprite_cache_.preserve_note_image_aspect_ratio ==
                           preserve_note_image_aspect_ratio &&
                       gameplay_note_sprite_cache_.skin_source == skin_source;
    if (use_osu_skin) {
        cache_valid = cache_valid &&
                      gameplay_note_sprite_cache_.osu_skin_root == data.osu_skin_root &&
                      gameplay_note_sprite_cache_.osu_skin_name == data.osu_skin_name;
    }
    for (int lane = 0; lane < lane_count && cache_valid; ++lane) {
        uint32_t color = 0xF6F8FF;
        if (static_cast<std::size_t>(lane) < data.lane_color_count) {
            color = data.lane_colors[static_cast<std::size_t>(lane)];
        } else if (!gameplay_lane_uses_white_note(lane + 1)) {
            color = 0x4F80FF;
        }
        if (!d2d_->lane_note_head_bitmaps[static_cast<std::size_t>(lane)] ||
            !d2d_->lane_note_hold_head_bitmaps[static_cast<std::size_t>(lane)] ||
            !d2d_->lane_note_tail_bitmaps[static_cast<std::size_t>(lane)] ||
            gameplay_note_sprite_cache_.lane_colors[static_cast<std::size_t>(lane)] != color) {
            cache_valid = false;
        }
    }
    if (cache_valid) {
        return true;
    }

    invalidate_gameplay_note_sprite_cache();
    auto* ctx = d2d_->d2d_context.Get();
    auto create_note_bitmap = [&](float width,
                                  float height,
                                  const D2D1_COLOR_F& fill_color,
                                  const D2D1_COLOR_F& border_color,
                                  Microsoft::WRL::ComPtr<ID2D1Bitmap>& out_bitmap,
                                  D2D1_RECT_F& out_source_rect) -> bool {
        Microsoft::WRL::ComPtr<ID2D1BitmapRenderTarget> render_target;
        const HRESULT create_hr = ctx->CreateCompatibleRenderTarget(
            D2D1::SizeF(width, height), &render_target);
        if (FAILED(create_hr) || !render_target) {
            return false;
        }

        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> fill_brush;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> border_brush;
        if (FAILED(render_target->CreateSolidColorBrush(fill_color, &fill_brush)) ||
            FAILED(render_target->CreateSolidColorBrush(border_color, &border_brush))) {
            return false;
        }

        render_target->BeginDraw();
        render_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

        const D2D1_RECT_F rect = D2D1::RectF(1.0f, 1.0f, width - 1.0f, height - 1.0f);
        draw_note_primitive(render_target.Get(), rect, fill_brush.Get(), border_brush.Get(), 1.3f,
                            note_shape, note_border_enabled);

        const HRESULT end_hr = render_target->EndDraw();
        if (FAILED(end_hr)) {
            return false;
        }

        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
        const HRESULT bitmap_hr = render_target->GetBitmap(&bitmap);
        if (FAILED(bitmap_hr) || !bitmap) {
            return false;
        }

        out_source_rect = full_bitmap_source_rect(bitmap->GetSize());
        out_bitmap = std::move(bitmap);
        return true;
    };

    if (!d2d_->note_fill_brush || !d2d_->note_border_brush) {
        return false;
    }

    gameplay_note_sprite_cache_ = {};
    gameplay_note_sprite_cache_.lane_count = lane_count;
    gameplay_note_sprite_cache_.note_border_enabled = note_border_enabled;
    gameplay_note_sprite_cache_.note_shape = note_shape;
    gameplay_note_sprite_cache_.preserve_note_image_aspect_ratio = preserve_note_image_aspect_ratio;
    gameplay_note_sprite_cache_.skin_source = skin_source;
    gameplay_note_sprite_cache_.osu_skin_root = data.osu_skin_root;
    gameplay_note_sprite_cache_.osu_skin_name = data.osu_skin_name;
    if (use_osu_skin) {
        const auto skin = app::resolve_osu_mania_skin(data.osu_skin_root, data.osu_skin_name, lane_count);
        bool loaded_any_external = false;
        if (skin.found) {
            gameplay_note_sprite_cache_.lane_divider_width_count =
                std::min(skin.lane_divider_widths.size(), gameplay_note_sprite_cache_.lane_divider_widths.size());
            for (std::size_t divider = 0; divider < gameplay_note_sprite_cache_.lane_divider_width_count; ++divider) {
                gameplay_note_sprite_cache_.lane_divider_widths[divider] = skin.lane_divider_widths[divider];
            }
            for (int lane = 0; lane < lane_count; ++lane) {
                const std::size_t lane_index = static_cast<std::size_t>(lane);
                if (lane_index < skin.note_images.size()) {
                    loaded_any_external |= load_bitmap_from_utf8_path(d2d_->wic_factory.Get(),
                                                                      ctx,
                                                                      skin.note_images[lane_index],
                                                                      d2d_->lane_note_head_bitmaps[lane_index],
                                                                      &d2d_->lane_note_head_source_rects[lane_index],
                                                                      true);
                }
                if (lane_index < skin.hold_head_images.size()) {
                    loaded_any_external |= load_bitmap_from_utf8_path(d2d_->wic_factory.Get(),
                                                                      ctx,
                                                                      skin.hold_head_images[lane_index],
                                                                      d2d_->lane_note_hold_head_bitmaps[lane_index],
                                                                      &d2d_->lane_note_hold_head_source_rects[lane_index],
                                                                      true);
                }
                if (lane_index < skin.hold_body_images.size()) {
                    loaded_any_external |= load_bitmap_from_utf8_path(d2d_->wic_factory.Get(),
                                                                      ctx,
                                                                      skin.hold_body_images[lane_index],
                                                                      d2d_->lane_note_hold_body_bitmaps[lane_index],
                                                                      &d2d_->lane_note_hold_body_source_rects[lane_index],
                                                                      true);
                }
                if (lane_index < skin.hold_tail_images.size()) {
                    loaded_any_external |= load_bitmap_from_utf8_path(d2d_->wic_factory.Get(),
                                                                      ctx,
                                                                      skin.hold_tail_images[lane_index],
                                                                      d2d_->lane_note_tail_bitmaps[lane_index],
                                                                      &d2d_->lane_note_tail_source_rects[lane_index],
                                                                      true);
                }
                if (lane_index < skin.key_images.size()) {
                    loaded_any_external |= load_bitmap_from_utf8_path(d2d_->wic_factory.Get(),
                                                                      ctx,
                                                                      skin.key_images[lane_index],
                                                                      d2d_->lane_key_idle_bitmaps[lane_index],
                                                                      &d2d_->lane_key_idle_source_rects[lane_index],
                                                                      true);
                }
                if (lane_index < skin.key_pressed_images.size()) {
                    loaded_any_external |= load_bitmap_from_utf8_path(d2d_->wic_factory.Get(),
                                                                      ctx,
                                                                      skin.key_pressed_images[lane_index],
                                                                      d2d_->lane_key_pressed_bitmaps[lane_index],
                                                                      &d2d_->lane_key_pressed_source_rects[lane_index],
                                                                      true);
                }
            }
        }
        gameplay_note_sprite_cache_.using_osu_skin_assets = loaded_any_external;
    }
    for (int lane = 0; lane < lane_count; ++lane) {
        uint32_t color = 0xF6F8FF;
        if (static_cast<std::size_t>(lane) < data.lane_color_count) {
            color = data.lane_colors[static_cast<std::size_t>(lane)];
        } else if (!gameplay_lane_uses_white_note(lane + 1)) {
            color = 0x4F80FF;
        }
        gameplay_note_sprite_cache_.lane_colors[static_cast<std::size_t>(lane)] = color;
        const float head_bitmap_width = 96.0f;
        const float head_bitmap_height = note_shape == "circle" ? 96.0f : 24.0f;
        const float tail_bitmap_width = 92.0f;
        const float tail_bitmap_height = note_shape == "circle" ? 92.0f : 20.0f;

        auto style_imported_circle_bitmap =
            [&](Microsoft::WRL::ComPtr<ID2D1Bitmap>& bitmap,
                D2D1_RECT_F& source_rect,
                float bitmap_width,
                float bitmap_height) -> bool {
                if (!use_osu_skin || note_shape != "circle" || !bitmap) {
                    return true;
                }

                Microsoft::WRL::ComPtr<ID2D1Bitmap> styled_bitmap;
                D2D1_RECT_F styled_source_rect = D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
                if (!create_composited_gameplay_note_bitmap(
                        ctx,
                        d2d_->d2d_factory.Get(),
                        bitmap.Get(),
                        bitmap_source_rect_or_null(source_rect),
                        D2D1::SizeF(bitmap_width, bitmap_height),
                        true,
                        preserve_note_image_aspect_ratio,
                        note_border_enabled,
                        gameplay_note_border_color(color),
                        styled_bitmap,
                        styled_source_rect)) {
                    return false;
                }
                bitmap = std::move(styled_bitmap);
                source_rect = styled_source_rect;
                return true;
            };

        if (!style_imported_circle_bitmap(
                d2d_->lane_note_head_bitmaps[static_cast<std::size_t>(lane)],
                d2d_->lane_note_head_source_rects[static_cast<std::size_t>(lane)],
                head_bitmap_width,
                head_bitmap_height) ||
            !style_imported_circle_bitmap(
                d2d_->lane_note_hold_head_bitmaps[static_cast<std::size_t>(lane)],
                d2d_->lane_note_hold_head_source_rects[static_cast<std::size_t>(lane)],
                head_bitmap_width,
                head_bitmap_height) ||
            !style_imported_circle_bitmap(
                d2d_->lane_note_tail_bitmaps[static_cast<std::size_t>(lane)],
                d2d_->lane_note_tail_source_rects[static_cast<std::size_t>(lane)],
                tail_bitmap_width,
                tail_bitmap_height)) {
            invalidate_gameplay_note_sprite_cache();
            return false;
        }

        if (!d2d_->lane_note_head_bitmaps[static_cast<std::size_t>(lane)] &&
            !create_note_bitmap(head_bitmap_width, head_bitmap_height,
                                gameplay_note_fill_color(color),
                                gameplay_note_border_color(color),
                                d2d_->lane_note_head_bitmaps[static_cast<std::size_t>(lane)],
                                d2d_->lane_note_head_source_rects[static_cast<std::size_t>(lane)])) {
            invalidate_gameplay_note_sprite_cache();
            return false;
        }
        if (!d2d_->lane_note_hold_head_bitmaps[static_cast<std::size_t>(lane)] &&
            !create_note_bitmap(head_bitmap_width, head_bitmap_height,
                                gameplay_note_fill_color(color),
                                gameplay_note_border_color(color),
                                d2d_->lane_note_hold_head_bitmaps[static_cast<std::size_t>(lane)],
                                d2d_->lane_note_hold_head_source_rects[static_cast<std::size_t>(lane)])) {
            invalidate_gameplay_note_sprite_cache();
            return false;
        }
        if (!d2d_->lane_note_tail_bitmaps[static_cast<std::size_t>(lane)] &&
            !create_note_bitmap(tail_bitmap_width, tail_bitmap_height,
                                gameplay_note_fill_color(color),
                                gameplay_note_border_color(color),
                                d2d_->lane_note_tail_bitmaps[static_cast<std::size_t>(lane)],
                                d2d_->lane_note_tail_source_rects[static_cast<std::size_t>(lane)])) {
            invalidate_gameplay_note_sprite_cache();
            return false;
        }
        if (d2d_->lane_note_hold_body_bitmaps[static_cast<std::size_t>(lane)]) {
            // Imported hold-body bitmaps already set a trimmed source rect during load.
        } else {
            d2d_->lane_note_hold_body_source_rects[static_cast<std::size_t>(lane)] =
                D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
        }
        if (d2d_->lane_key_idle_bitmaps[static_cast<std::size_t>(lane)]) {
            // Imported key bitmaps already set a trimmed source rect during load.
        } else {
            d2d_->lane_key_idle_source_rects[static_cast<std::size_t>(lane)] =
                D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
        }
        if (d2d_->lane_key_pressed_bitmaps[static_cast<std::size_t>(lane)]) {
            // Imported pressed-key bitmaps already set a trimmed source rect during load.
        } else {
            d2d_->lane_key_pressed_source_rects[static_cast<std::size_t>(lane)] =
                D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    return true;
}

void MenuWindow::invalidate_song_select_preview_cache() {
    song_select_preview_cache_ = {};
    if (d2d_) {
        d2d_->song_select_preview_bitmap.Reset();
    }
}

void MenuWindow::clear_song_card_preview_cache() {
    if (!d2d_) {
        return;
    }
    d2d_->song_card_preview_bitmaps.clear();
    d2d_->song_card_preview_lru.clear();
}

bool MenuWindow::ensure_song_select_preview_bitmap(const SongSelectData& data) {
    if (!d2d_ || !d2d_->d2d_context || !d2d_->wic_factory) {
        invalidate_song_select_preview_cache();
        return false;
    }

    const std::string path = data.selected_song_background_path;
    if (path.empty()) {
        invalidate_song_select_preview_cache();
        return false;
    }

    if (song_select_preview_cache_.attempted && song_select_preview_cache_.path == path) {
        return d2d_->song_select_preview_bitmap.Get() != nullptr;
    }

    invalidate_song_select_preview_cache();
    song_select_preview_cache_.path = path;
    song_select_preview_cache_.attempted = true;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    if (!load_bitmap_from_utf8_path(d2d_->wic_factory.Get(), d2d_->d2d_context.Get(), path, bitmap)) {
        return false;
    }

    d2d_->song_select_preview_bitmap = std::move(bitmap);
    return true;
}

void MenuWindow::invalidate_gameplay_static_cache() {
    gameplay_static_cache_ = {};
    if (d2d_) {
        d2d_->gameplay_static_command_list.Reset();
    }
}

bool MenuWindow::ensure_gameplay_static_cache(const GameplayHudData& data) {
    const int lane_count = std::clamp(data.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
    const double judgement_line_position = clamp_gameplay_judgement_line(data.judgement_line_position);
    const float note_width_scale = clamp_gameplay_note_width_scale(data.note_width_scale);
    const float note_height_scale = clamp_gameplay_note_height_scale(data.note_height_scale);
    const float lane_divider_width_scale =
        clamp_gameplay_lane_divider_width_scale(data.lane_divider_width_scale);
    std::array<float, kGameplayHudMaxLanes> lane_divider_widths{};
    const std::size_t lane_divider_width_count = resolve_gameplay_lane_divider_widths(
        lane_count,
        data.lane_divider_width_scale,
        gameplay_note_sprite_cache_.lane_divider_width_count,
        gameplay_note_sprite_cache_.lane_divider_widths,
        lane_divider_widths);
    if (!d2d_ || !d2d_->d2d_context || !d2d_->d2d_target) {
        return false;
    }
    bool divider_widths_match = gameplay_static_cache_.lane_divider_width_count == lane_divider_width_count;
    for (std::size_t divider = 0; divider < lane_divider_width_count && divider_widths_match; ++divider) {
        divider_widths_match =
            std::abs(gameplay_static_cache_.lane_divider_widths[divider] - lane_divider_widths[divider]) < 1e-6f;
    }
    if (d2d_->gameplay_static_command_list &&
        gameplay_static_cache_.lane_count == lane_count &&
        std::abs(gameplay_static_cache_.judgement_line_position - judgement_line_position) < 1e-6 &&
        std::abs(gameplay_static_cache_.note_width_scale - note_width_scale) < 1e-6 &&
        std::abs(gameplay_static_cache_.note_height_scale - note_height_scale) < 1e-6 &&
        std::abs(gameplay_static_cache_.lane_divider_width_scale - lane_divider_width_scale) < 1e-6 &&
        divider_widths_match) {
        return true;
    }

    Microsoft::WRL::ComPtr<ID2D1CommandList> command_list;
    const HRESULT create_hr = d2d_->d2d_context->CreateCommandList(&command_list);
    if (FAILED(create_hr)) {
        return false;
    }

    auto* ctx = d2d_->d2d_context.Get();
    ctx->SetTarget(command_list.Get());
    ctx->BeginDraw();
    ctx->SetTransform(D2D1::Matrix3x2F::Identity());
    const D2D1_ANTIALIAS_MODE saved_antialias = ctx->GetAntialiasMode();
    ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

    const GameplayFieldLayout field_layout = build_gameplay_field_layout(
        kGameplayFieldLeft, kGameplayFieldRight, kGameplayFieldTop, kGameplayFieldBottom, lane_count, note_width_scale);
    const float field_height = field_layout.height;
    const D2D1_RECT_F field_rect =
        D2D1::RectF(field_layout.left, field_layout.top, field_layout.right, field_layout.bottom);
    const D2D1_ROUNDED_RECT field_rr = D2D1::RoundedRect(field_rect, 16.0f, 16.0f);
    if (d2d_->panel_brush) {
        ctx->FillRoundedRectangle(field_rr, d2d_->panel_brush.Get());
    }
    if (d2d_->button_border_brush) {
        ctx->DrawRoundedRectangle(field_rr, d2d_->button_border_brush.Get(), 1.4f);
    }

    const float lane_width = field_layout.lane_width;
    for (int lane = 0; lane < lane_count; ++lane) {
        const float x0 = field_layout.left + static_cast<float>(lane) * lane_width;
        const float x1 = x0 + lane_width;
        const D2D1_RECT_F lane_rect =
            D2D1::RectF(x0 + 2.0f, field_layout.top + 2.0f, x1 - 2.0f, field_layout.bottom - 2.0f);
        if (d2d_->card_brush) {
            d2d_->card_brush->SetOpacity((lane % 2 == 0) ? 0.38f : 0.28f);
            ctx->FillRectangle(lane_rect, d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(1.0f);
        }
        if (d2d_->button_border_brush && lane + 1 < lane_count) {
            const float divider_width = lane_divider_widths[static_cast<std::size_t>(lane)];
            if (divider_width <= 0.01f) {
                continue;
            }
            ctx->DrawLine(D2D1::Point2F(x1, field_layout.top), D2D1::Point2F(x1, field_layout.bottom),
                          d2d_->button_border_brush.Get(), divider_width);
        }
    }

    if (d2d_->judgement_line_brush) {
        const float hit_line_y = gameplay_field_y(field_layout.top, field_height, judgement_line_position);
        const D2D1_RECT_F hit_line_rect = gameplay_judgement_line_rect(field_layout, hit_line_y, note_height_scale);
        ctx->FillRectangle(hit_line_rect, d2d_->judgement_line_brush.Get());
        ctx->DrawLine(D2D1::Point2F(field_layout.left, hit_line_y), D2D1::Point2F(field_layout.right, hit_line_y),
                      d2d_->judgement_line_brush.Get(), 1.6f);
    }

    const D2D1_RECT_F gauge_frame = D2D1::RectF(kGameplayGaugeLeft,
                                                kGameplayGaugeTop,
                                                kGameplayGaugeLeft + kGameplayGaugeWidth,
                                                kGameplayGaugeBottom);
    if (d2d_->button_border_brush) {
        ctx->DrawRoundedRectangle(D2D1::RoundedRect(gauge_frame, 10.0f, 10.0f),
                                  d2d_->button_border_brush.Get(), 1.5f);
    }
    if (d2d_->card_brush) {
        d2d_->card_brush->SetOpacity(0.45f);
        ctx->FillRoundedRectangle(D2D1::RoundedRect(gauge_frame, 10.0f, 10.0f), d2d_->card_brush.Get());
        d2d_->card_brush->SetOpacity(1.0f);
    }

    ctx->SetAntialiasMode(saved_antialias);
    const HRESULT end_hr = ctx->EndDraw();
    ctx->SetTarget(d2d_->d2d_target.Get());
    if (FAILED(end_hr) || FAILED(command_list->Close())) {
        return false;
    }

    d2d_->gameplay_static_command_list = std::move(command_list);
    gameplay_static_cache_.lane_count = lane_count;
    gameplay_static_cache_.judgement_line_position = judgement_line_position;
    gameplay_static_cache_.note_width_scale = note_width_scale;
    gameplay_static_cache_.note_height_scale = note_height_scale;
    gameplay_static_cache_.lane_divider_width_scale = lane_divider_width_scale;
    gameplay_static_cache_.lane_divider_width_count = lane_divider_width_count;
    gameplay_static_cache_.lane_divider_widths = lane_divider_widths;
    return true;
}

bool MenuWindow::is_input_foreground() const {
    if (!hwnd_) {
        return false;
    }

    const HWND hwnd = static_cast<HWND>(hwnd_);
    const HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return false;
    }
    if (foreground == hwnd) {
        return true;
    }
    return GetAncestor(foreground, GA_ROOT) == hwnd;
}

void MenuWindow::clear_song_scrollbar_state() {
    song_scrollbar_state_ = {};
    song_scroll_drag_active_ = false;
    song_scroll_drag_offset_y_ = 0.0f;
    song_scroll_drag_selected_offset_ = 0;
    song_scroll_drag_last_index_ = -1;
    if (hwnd_) {
        const HWND hwnd = static_cast<HWND>(hwnd_);
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
    }
}

bool MenuWindow::translate_window_point(int window_x, int window_y, float* out_x, float* out_y) const {
    if (!out_x || !out_y || scale_ <= 0.0f) {
        return false;
    }

    const float x = (static_cast<float>(window_x) - offset_x_) / scale_;
    const float y = (static_cast<float>(window_y) - offset_y_) / scale_;
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }

    *out_x = x;
    *out_y = y;
    return true;
}

int MenuWindow::song_scrollbar_target_index(float y, float drag_offset, int selected_offset) const {
    if (!song_scrollbar_state_.visible || song_scrollbar_state_.total_count <= 0) {
        return -1;
    }

    const float track_top = song_scrollbar_state_.top;
    const float track_bottom = song_scrollbar_state_.bottom;
    const float thumb_height =
        std::max(1.0f, song_scrollbar_state_.thumb_bottom - song_scrollbar_state_.thumb_top);
    const float max_thumb_top = std::max(track_top, track_bottom - thumb_height);
    const float desired_thumb_top = std::clamp(y - drag_offset, track_top, max_thumb_top);
    const int scrollable_count = std::max(0, song_scrollbar_state_.total_count - song_scrollbar_state_.visible_count);
    const float travel = std::max(0.0f, max_thumb_top - track_top);
    const double ratio = (travel > 0.0f) ? static_cast<double>(desired_thumb_top - track_top) / static_cast<double>(travel)
                                         : 0.0;
    const int target_window_start =
        (scrollable_count > 0)
            ? static_cast<int>(std::llround(std::clamp(ratio, 0.0, 1.0) * static_cast<double>(scrollable_count)))
            : 0;
    const int clamped_selected_offset =
        std::clamp(selected_offset, 0, std::max(0, song_scrollbar_state_.visible_count - 1));
    return std::clamp(target_window_start + clamped_selected_offset, 0, song_scrollbar_state_.total_count - 1);
}

void MenuWindow::on_mouse_button_down(int window_x, int window_y) {
    if (!is_input_foreground()) {
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    if (!translate_window_point(window_x, window_y, &x, &y)) {
        return;
    }

    if (!song_scrollbar_state_.visible || song_scrollbar_state_.total_count <= song_scrollbar_state_.visible_count) {
        return;
    }
    if (x < song_scrollbar_state_.left || x > song_scrollbar_state_.right ||
        y < song_scrollbar_state_.top || y > song_scrollbar_state_.bottom) {
        return;
    }

    const bool inside_thumb =
        (y >= song_scrollbar_state_.thumb_top && y <= song_scrollbar_state_.thumb_bottom);
    const float thumb_height =
        std::max(1.0f, song_scrollbar_state_.thumb_bottom - song_scrollbar_state_.thumb_top);

    if (inside_thumb) {
        song_scroll_drag_active_ = true;
        song_scroll_drag_offset_y_ = std::clamp(y - song_scrollbar_state_.thumb_top, 0.0f, thumb_height);
        song_scroll_drag_selected_offset_ =
            std::clamp(song_scrollbar_state_.selected_index - song_scrollbar_state_.window_start,
                       0,
                       std::max(0, song_scrollbar_state_.visible_count - 1));
        song_scroll_drag_last_index_ = song_scrollbar_state_.selected_index;
        if (hwnd_) {
            SetCapture(static_cast<HWND>(hwnd_));
        }
        return;
    }

    const int target_index = song_scrollbar_target_index(
        y, thumb_height * 0.5f, std::max(0, song_scrollbar_state_.visible_count / 2));
    if (target_index >= 0) {
        push_click_event(MenuClickEvent{MenuHitTargetKind::SongScrollbar, target_index});
    }
}

void MenuWindow::on_mouse_click(int window_x, int window_y, bool double_click) {
    if (!is_input_foreground()) {
        return;
    }
    if (song_scroll_drag_active_) {
        song_scroll_drag_active_ = false;
        song_scroll_drag_offset_y_ = 0.0f;
        song_scroll_drag_selected_offset_ = 0;
        song_scroll_drag_last_index_ = -1;
        if (hwnd_ && GetCapture() == static_cast<HWND>(hwnd_)) {
            ReleaseCapture();
        }
        return;
    }
    if (!double_click && suppress_next_left_button_up_) {
        suppress_next_left_button_up_ = false;
        return;
    }
    float x = 0.0f;
    float y = 0.0f;
    if (!translate_window_point(window_x, window_y, &x, &y)) {
        return;
    }

    for (auto it = hit_regions_.rbegin(); it != hit_regions_.rend(); ++it) {
        const HitRegion& region = *it;
        if (x < region.left || x > region.right || y < region.top || y > region.bottom) {
            continue;
        }

        if (double_click) {
            suppress_next_left_button_up_ = true;
        }
        push_click_event(MenuClickEvent{region.kind, region.index, region.part, double_click});
        return;
    }
}

void MenuWindow::on_mouse_secondary_click(int window_x, int window_y) {
    if (!is_input_foreground()) {
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    if (!translate_window_point(window_x, window_y, &x, &y)) {
        return;
    }

    for (auto it = hit_regions_.rbegin(); it != hit_regions_.rend(); ++it) {
        const HitRegion& region = *it;
        if (x < region.left || x > region.right || y < region.top || y > region.bottom) {
            continue;
        }
        if (region.kind != MenuHitTargetKind::TitleButton &&
            region.kind != MenuHitTargetKind::OptionsItem &&
            region.kind != MenuHitTargetKind::SongNavButton) {
            return;
        }
        push_click_event(MenuClickEvent{region.kind, region.index, MenuHitPart::Decrement, false});
        return;
    }
}

void MenuWindow::on_mouse_move(int window_x, int window_y) {
    if (!song_scroll_drag_active_) {
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    if (!translate_window_point(window_x, window_y, &x, &y)) {
        return;
    }
    if (x < song_scrollbar_state_.left - 32.0f || x > song_scrollbar_state_.right + 32.0f) {
        return;
    }

    const int target_index =
        song_scrollbar_target_index(y, song_scroll_drag_offset_y_, song_scroll_drag_selected_offset_);
    if (target_index >= 0 && target_index != song_scroll_drag_last_index_) {
        song_scroll_drag_last_index_ = target_index;
        push_click_event(MenuClickEvent{MenuHitTargetKind::SongScrollbar, target_index});
    }
}

void MenuWindow::on_mouse_wheel(int wheel_delta) {
    if (!is_input_foreground()) {
        return;
    }
    if (wheel_delta == 0) {
        return;
    }

    int steps = wheel_delta / WHEEL_DELTA;
    if (steps == 0) {
        steps = (wheel_delta > 0) ? 1 : -1;
    }
    MenuClickEvent event;
    event.kind = MenuHitTargetKind::MouseWheel;
    event.wheel_steps = steps;
    push_click_event(std::move(event));
}

void MenuWindow::on_file_drop(std::string path) {
    if (path.empty()) {
        return;
    }
    MenuClickEvent event;
    event.kind = MenuHitTargetKind::FileDrop;
    event.path = std::move(path);
    push_click_event(std::move(event));
}

void MenuWindow::shutdown() {
    clear_song_scrollbar_state();
    update_cursor_visibility(false);
    destroy_window();
    invalidate_gameplay_note_sprite_cache();
    invalidate_song_select_preview_cache();
    clear_song_card_preview_cache();
    invalidate_gameplay_static_cache();

    if (d2d_) {
        d2d_->exit_brush.Reset();
        d2d_->exit_stops.Reset();
        d2d_->options_brush.Reset();
        d2d_->options_stops.Reset();
        d2d_->edit_brush.Reset();
        d2d_->edit_stops.Reset();
        d2d_->play_brush.Reset();
        d2d_->play_stops.Reset();
        d2d_->logo_brush.Reset();
        d2d_->logo_stops.Reset();
        d2d_->glow_brush.Reset();
        d2d_->glow_stops.Reset();
        d2d_->bg_brush.Reset();
        d2d_->bg_stops.Reset();
        d2d_->gameplay_static_command_list.Reset();
        for (auto& bitmap : d2d_->lane_note_head_bitmaps) {
            bitmap.Reset();
        }
        d2d_->lane_note_head_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
        for (auto& bitmap : d2d_->lane_note_hold_head_bitmaps) {
            bitmap.Reset();
        }
        d2d_->lane_note_hold_head_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
        for (auto& bitmap : d2d_->lane_note_hold_body_bitmaps) {
            bitmap.Reset();
        }
        d2d_->lane_note_hold_body_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
        for (auto& bitmap : d2d_->lane_note_tail_bitmaps) {
            bitmap.Reset();
        }
        d2d_->lane_note_tail_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
        for (auto& bitmap : d2d_->lane_key_idle_bitmaps) {
            bitmap.Reset();
        }
        d2d_->lane_key_idle_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
        for (auto& bitmap : d2d_->lane_key_pressed_bitmaps) {
            bitmap.Reset();
        }
        d2d_->lane_key_pressed_source_rects.fill(D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f));
        d2d_->note_hold_brush.Reset();
        d2d_->note_border_brush.Reset();
        d2d_->note_fill_brush.Reset();
        d2d_->button_border_brush.Reset();
        d2d_->button_selected_brush.Reset();
        d2d_->button_brush.Reset();
        d2d_->footer_brush.Reset();
        d2d_->panel_brush.Reset();
        d2d_->card_brush.Reset();
        d2d_->muted_brush.Reset();
        d2d_->judgement_line_brush.Reset();
        d2d_->accent_brush.Reset();
        d2d_->text_brush.Reset();
        d2d_->stats_value_format.Reset();
        d2d_->stats_label_format.Reset();
        d2d_->rank_format.Reset();
        d2d_->hud_format.Reset();
        d2d_->song_artist_format.Reset();
        d2d_->song_title_format.Reset();
        d2d_->header_format.Reset();
        d2d_->menu_icon_format.Reset();
        d2d_->menu_button_format.Reset();
        d2d_->logo_format.Reset();
        d2d_->mono_format.Reset();
        d2d_->body_format.Reset();
        d2d_->option_format.Reset();
        d2d_->title_format.Reset();
        d2d_->dwrite_factory.Reset();
        d2d_->wic_factory.Reset();
        d2d_->d2d_target.Reset();
        d2d_->d2d_context.Reset();
        d2d_->d2d_device.Reset();
        d2d_->d2d_factory.Reset();
        d2d_->swap_chain.Reset();
        d2d_->context.Reset();
        d2d_->device.Reset();
    }

    if (com_initialized_) {
        CoUninitialize();
        com_initialized_ = false;
    }

    initialized_.store(false, std::memory_order_release);
    suppress_next_left_button_up_ = false;
    hit_regions_.clear();
    {
        std::lock_guard<std::mutex> lock(click_events_mutex_);
        click_events_.clear();
    }
}

bool MenuWindow::initialize(const MenuWindowConfig& config) {
    if (initialized_.load(std::memory_order_acquire)) {
        return true;
    }

    config_ = config;
    should_close_.store(false, std::memory_order_release);
    suppress_next_left_button_up_ = false;
    hit_regions_.clear();
    {
        std::lock_guard<std::mutex> lock(click_events_mutex_);
        click_events_.clear();
    }

    if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        com_initialized_ = true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = menu_window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    ATOM class_atom = RegisterClassExW(&wc);
    if (!class_atom) {
        const DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            return fail_fatal("Failed to register menu window class.");
        }
    }

    const DWORD style = window_style_for_display_mode(config.display_mode);
    const DWORD ex_style = window_ex_style_for_display_mode(config.display_mode);
    const MonitorDisplayInfo monitor = query_monitor_display_info(nullptr);
    int x = monitor.rect.left;
    int y = monitor.rect.top;
    resolve_window_bounds(config, monitor, width_, height_, x, y);
    const SIZE window_size = window_size_for_client_area(width_, height_, style, ex_style);

    const std::wstring title = to_wide(config.title);
    HWND hwnd = CreateWindowExW(
        ex_style,
        kWindowClassName,
        title.c_str(),
        style,
        x,
        y,
        window_size.cx,
        window_size.cy,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        this);

    if (!hwnd) {
        return fail_fatal("Failed to create menu window.");
    }

    hwnd_ = hwnd;
    std::cerr << "[MenuWindow::initialize] window created: hwnd=" << hwnd 
              << " x=" << x << " y=" << y << " w=" << width_ << " h=" << height_ << std::endl;
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, window_size.cx, window_size.cy,
                 SWP_SHOWWINDOW);
    SetWindowPos(hwnd, HWND_NOTOPMOST, x, y, window_size.cx, window_size.cy,
                 SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    BringWindowToTop(hwnd);
    SetFocus(hwnd);
    DragAcceptFiles(hwnd, TRUE);

    FLASHWINFO flash{};
    flash.cbSize = sizeof(flash);
    flash.hwnd = hwnd;
    flash.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
    flash.uCount = 3;
    FlashWindowEx(&flash);

    RECT window_rect{};
    GetWindowRect(hwnd, &window_rect);
    std::cerr << "[MenuWindow::initialize] visible=" << (IsWindowVisible(hwnd) ? 1 : 0)
              << " iconic=" << (IsIconic(hwnd) ? 1 : 0)
              << " rect=(" << window_rect.left << "," << window_rect.top << ","
              << window_rect.right << "," << window_rect.bottom << ")"
              << " foreground=" << GetForegroundWindow() << std::endl;

    UINT device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL level_out = D3D_FEATURE_LEVEL_11_0;

    const HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags,
                                         levels, static_cast<UINT>(sizeof(levels) / sizeof(levels[0])),
                                         D3D11_SDK_VERSION, &d2d_->device, &level_out, &d2d_->context);
    if (FAILED(hr)) {
        return fail_fatal("Failed to create D3D11 hardware device. TenRiff now requires GPU rendering (software fallback disabled).");
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    if (FAILED(d2d_->device.As(&dxgi_device))) {
        return fail_fatal("Failed to query IDXGIDevice from D3D11 device.");
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_device->GetAdapter(&adapter))) {
        return fail_fatal("Failed to query IDXGIAdapter from DXGI device.");
    }
    DXGI_ADAPTER_DESC adapter_desc{};
    if (SUCCEEDED(adapter->GetDesc(&adapter_desc))) {
        std::wcerr << L"[MenuWindow::initialize] rendering on GPU adapter: "
                   << adapter_desc.Description << std::endl;
    }

    // Try IDXGIFactory5 for tearing support, fall back to IDXGIFactory2
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory2;
    Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory2)))) {
        return fail_fatal("Failed to obtain DXGI factory.");
    }

    BOOL allow_tearing = FALSE;
    swap_chain_flags_ = 0;
    if (SUCCEEDED(factory2.As(&factory5)) && factory5) {
        if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                   &allow_tearing, sizeof(allow_tearing))) &&
            allow_tearing) {
            swap_chain_flags_ |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }
    }

    DXGI_SWAP_CHAIN_DESC1 swap_desc{};
    swap_desc.Width = width_;
    swap_desc.Height = height_;
    swap_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_desc.SampleDesc.Count = 1;
    swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount = 2;
    swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swap_desc.Flags = swap_chain_flags_;

    if (FAILED(factory2->CreateSwapChainForHwnd(d2d_->device.Get(), hwnd, &swap_desc,
                                               nullptr, nullptr, &d2d_->swap_chain))) {
        return fail_fatal("Failed to create DXGI swap chain.");
    }
    configure_low_latency_presentation(dxgi_device.Get(), d2d_->swap_chain.Get());

    fullscreen_ = false;
    fullscreen_restore_pending_ = false;
    if (config.display_mode == "fullscreen") {
        const HRESULT fs_hr = d2d_->swap_chain->SetFullscreenState(TRUE, nullptr);
        if (FAILED(fs_hr)) {
            std::cerr << "[MenuWindow::initialize] SetFullscreenState(TRUE) failed hr=0x"
                      << std::hex << static_cast<unsigned long>(fs_hr) << std::dec << std::endl;
            fullscreen_restore_pending_ = true;
        } else {
            fullscreen_ = true;
            apply_fullscreen_target(d2d_->swap_chain.Get(), config, width_, height_);
        }
    }

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                 IID_PPV_ARGS(&d2d_->d2d_factory)))) {
        return fail_fatal("Failed to create Direct2D factory.");
    }

    if (FAILED(d2d_->d2d_factory->CreateDevice(dxgi_device.Get(), &d2d_->d2d_device))) {
        return fail_fatal("Failed to create Direct2D device.");
    }

    if (FAILED(d2d_->d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                                     &d2d_->d2d_context))) {
        return fail_fatal("Failed to create Direct2D device context.");
    }

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                   __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(d2d_->dwrite_factory.GetAddressOf())))) {
        return fail_fatal("Failed to create DirectWrite factory.");
    }

    const HRESULT wic_hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                            IID_PPV_ARGS(&d2d_->wic_factory));
    if (FAILED(wic_hr)) {
        std::cerr << "[MenuWindow::initialize] WIC factory unavailable hr=0x"
                  << std::hex << static_cast<unsigned long>(wic_hr) << std::dec << std::endl;
    }

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           32.0f, L"en-us", &d2d_->title_format);
    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           24.0f, L"en-us", &d2d_->option_format);
    if (d2d_->option_format) {
        d2d_->option_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        d2d_->option_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_REGULAR,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           20.0f, L"en-us", &d2d_->body_format);
    d2d_->dwrite_factory->CreateTextFormat(L"Consolas", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           19.0f, L"en-us", &d2d_->mono_format);

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           110.0f, L"en-us", &d2d_->logo_format);
    if (d2d_->logo_format) {
        d2d_->logo_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        d2d_->logo_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           54.0f, L"en-us", &d2d_->menu_button_format);
    if (d2d_->menu_button_format) {
        d2d_->menu_button_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        d2d_->menu_button_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    d2d_->dwrite_factory->CreateTextFormat(L"Segoe UI Symbol", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           52.0f, L"en-us", &d2d_->menu_icon_format);
    if (d2d_->menu_icon_format) {
        d2d_->menu_icon_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        d2d_->menu_icon_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           72.0f, L"en-us", &d2d_->header_format);
    if (d2d_->header_format) {
        d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        d2d_->header_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           34.0f, L"en-us", &d2d_->song_title_format);
    if (d2d_->song_title_format) {
        d2d_->song_title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        d2d_->song_title_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_REGULAR,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           22.0f, L"en-us", &d2d_->song_artist_format);
    if (d2d_->song_artist_format) {
        d2d_->song_artist_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        d2d_->song_artist_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_REGULAR,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           20.0f, L"en-us", &d2d_->hud_format);
    if (d2d_->hud_format) {
        d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        d2d_->hud_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           96.0f, L"en-us", &d2d_->rank_format);
    if (d2d_->rank_format) {
        d2d_->rank_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        d2d_->rank_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           20.0f, L"en-us", &d2d_->stats_label_format);
    if (d2d_->stats_label_format) {
        d2d_->stats_label_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        d2d_->stats_label_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           24.0f, L"en-us", &d2d_->stats_value_format);
    if (d2d_->stats_value_format) {
        d2d_->stats_value_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        d2d_->stats_value_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    update_layout();
    if (!recreate_targets()) {
        return fail_fatal("Failed to create Direct2D render target from swap-chain surface.");
    }
    update_brushes();

    initialized_.store(true, std::memory_order_release);
    return true;
}

void MenuWindow::destroy_window() {
    if (!hwnd_) {
        return;
    }
    if (!IsWindow(static_cast<HWND>(hwnd_))) {
        hwnd_ = nullptr;
        return;
    }

    if (fullscreen_ && d2d_ && d2d_->swap_chain) {
        d2d_->swap_chain->SetFullscreenState(FALSE, nullptr);
        fullscreen_ = false;
    }
    fullscreen_restore_pending_ = false;

    DestroyWindow(static_cast<HWND>(hwnd_));
    hwnd_ = nullptr;
}

void MenuWindow::pump_messages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            should_close_.store(true, std::memory_order_release);
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void MenuWindow::draw(const MenuRenderData& data) {
    static int draw_count = 0;
    if (draw_count++ < 5) {
        std::cerr << "[MenuWindow::draw] called, d2d_=" << (d2d_ ? "yes" : "no") 
                  << ", d2d_context=" << (d2d_ && d2d_->d2d_context ? "yes" : "no")
                  << ", swap_chain=" << (d2d_ && d2d_->swap_chain ? "yes" : "no")
                  << ", target=" << (d2d_ && d2d_->d2d_target ? "yes" : "no") << std::endl;
    }
    if (!d2d_ || !d2d_->d2d_context || !d2d_->swap_chain) {
        return;
    }

    if (resize_pending_) {
        resize_pending_ = false;
        resize_swap_chain(pending_width_, pending_height_);
    }

    if (!d2d_->d2d_target) {
        if (!recreate_targets()) {
            fail_fatal("Failed to recover Direct2D render target.");
            shutdown();
            return;
        }
    }
    if (d2d_->d2d_target) {
        d2d_->d2d_context->SetTarget(d2d_->d2d_target.Get());
    } else {
        fail_fatal("Render target is unavailable.");
        shutdown();
        return;
    }

    auto* ctx = d2d_->d2d_context.Get();
    const int64_t render_now_ns = timing::HighResClock::now_ns();
    const bool has_menu_scene = render_menu_scene(data.kind, render_now_ns);
    if (data.kind == MenuScreenKind::GameplayHud) {
        if (!ensure_gameplay_note_sprites(data.gameplay)) {
            invalidate_gameplay_note_sprite_cache();
        }
        if (!ensure_gameplay_static_cache(data.gameplay)) {
            invalidate_gameplay_static_cache();
        }
    }
    ctx->BeginDraw();

    ctx->SetTransform(D2D1::Matrix3x2F::Identity());
    if (has_menu_scene && d2d_->bg_brush) {
        const float saved_opacity = d2d_->bg_brush->GetOpacity();
        d2d_->bg_brush->SetOpacity(data.kind == MenuScreenKind::TitleMenu ? 0.16f : 0.18f);
        ctx->FillRectangle(D2D1::RectF(0.0f, 0.0f, static_cast<float>(width_),
                                       static_cast<float>(height_)),
                           d2d_->bg_brush.Get());
        d2d_->bg_brush->SetOpacity(saved_opacity);
    } else if (d2d_->bg_brush) {
        ctx->FillRectangle(D2D1::RectF(0.0f, 0.0f, static_cast<float>(width_),
                                       static_cast<float>(height_)),
                           d2d_->bg_brush.Get());
    } else {
        ctx->Clear(D2D1::ColorF(0.05f, 0.05f, 0.06f, 1.0f));
    }

    const D2D1_MATRIX_3X2_F transform =
        D2D1::Matrix3x2F(scale_, 0.0f, 0.0f, scale_, offset_x_, offset_y_);
    ctx->SetTransform(transform);

    if (d2d_->glow_brush) {
        const float saved_opacity = d2d_->glow_brush->GetOpacity();
        if (has_menu_scene) {
            d2d_->glow_brush->SetOpacity(data.kind == MenuScreenKind::TitleMenu ? 0.26f : 0.14f);
        }
        ctx->FillRectangle(D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight), d2d_->glow_brush.Get());
        d2d_->glow_brush->SetOpacity(saved_opacity);
    }

    hit_regions_.clear();
    if (data.kind != MenuScreenKind::SongSelect) {
        clear_song_scrollbar_state();
    }
    auto register_hit = [this](const D2D1_RECT_F& rect,
                               MenuHitTargetKind kind,
                               int index,
                               MenuHitPart part = MenuHitPart::Activate) {
        if (kind == MenuHitTargetKind::None || index < 0) {
            return;
        }
        hit_regions_.push_back(HitRegion{kind, index, part, rect.left, rect.top, rect.right, rect.bottom});
    };

    auto draw_text_clipped = [&](const std::wstring& text,
                                 IDWriteTextFormat* format,
                                 const D2D1_RECT_F& rect,
                                 ID2D1Brush* brush) {
        if (text.empty() || !format || !brush) {
            return;
        }
        ctx->DrawText(text.c_str(), static_cast<UINT32>(text.size()),
                      format, rect, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP, DWRITE_MEASURING_MODE_NATURAL);
    };

    auto inset_rect = [](const D2D1_RECT_F& rect, float dx, float dy) {
        return D2D1::RectF(rect.left + dx, rect.top + dy, rect.right - dx, rect.bottom - dy);
    };

    auto offset_rect = [](const D2D1_RECT_F& rect, float dx, float dy) {
        return D2D1::RectF(rect.left + dx, rect.top + dy, rect.right + dx, rect.bottom + dy);
    };

    auto draw_song_select_glow_border = [&](const D2D1_ROUNDED_RECT& rr, float opacity, float width) {
        if (!d2d_->accent_brush || opacity <= 0.0f || width <= 0.0f) {
            return;
        }
        const float saved_opacity = d2d_->accent_brush->GetOpacity();
        d2d_->accent_brush->SetOpacity(opacity);
        ctx->DrawRoundedRectangle(rr, d2d_->accent_brush.Get(), width);
        d2d_->accent_brush->SetOpacity(saved_opacity);
    };

    auto draw_glass_panel = [&](const D2D1_RECT_F& rect,
                                float radius,
                                float fill_opacity,
                                float glow_strength,
                                bool strong_edge,
                                float shadow_offset = 9.0f) {
        const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, radius, radius);
        const D2D1_RECT_F shadow_rect = offset_rect(rect, 0.0f, shadow_offset);
        const D2D1_ROUNDED_RECT shadow_rr = D2D1::RoundedRect(shadow_rect, radius, radius);

        if (d2d_->footer_brush) {
            const float saved_opacity = d2d_->footer_brush->GetOpacity();
            d2d_->footer_brush->SetOpacity(std::clamp(0.10f + glow_strength * 0.10f, 0.08f, 0.24f));
            ctx->FillRoundedRectangle(shadow_rr, d2d_->footer_brush.Get());
            d2d_->footer_brush->SetOpacity(saved_opacity);
        }

        if (d2d_->panel_brush) {
            const float saved_opacity = d2d_->panel_brush->GetOpacity();
            d2d_->panel_brush->SetOpacity(fill_opacity);
            ctx->FillRoundedRectangle(rr, d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(saved_opacity);
        }

        if (d2d_->card_brush) {
            const D2D1_RECT_F sheen_rect =
                D2D1::RectF(rect.left + 8.0f, rect.top + 8.0f, rect.right - 8.0f,
                            std::min(rect.bottom - 8.0f, rect.top + std::max(30.0f, (rect.bottom - rect.top) * 0.28f)));
            const D2D1_ROUNDED_RECT sheen_rr =
                D2D1::RoundedRect(sheen_rect, std::max(4.0f, radius - 8.0f), std::max(4.0f, radius - 8.0f));
            const float saved_opacity = d2d_->card_brush->GetOpacity();
            d2d_->card_brush->SetOpacity(std::clamp(0.22f + glow_strength * 0.04f, 0.16f, 0.28f));
            ctx->FillRoundedRectangle(sheen_rr, d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(saved_opacity);
        }

        if (d2d_->text_brush) {
            const D2D1_RECT_F top_glint =
                D2D1::RectF(rect.left + 18.0f, rect.top + 14.0f, rect.right - 18.0f, rect.top + 16.0f);
            const float saved_opacity = d2d_->text_brush->GetOpacity();
            d2d_->text_brush->SetOpacity(std::clamp(0.03f + glow_strength * 0.06f, 0.03f, 0.10f));
            ctx->FillRectangle(top_glint, d2d_->text_brush.Get());
            d2d_->text_brush->SetOpacity(saved_opacity);
        }

        if (d2d_->button_border_brush) {
            const float saved_opacity = d2d_->button_border_brush->GetOpacity();
            d2d_->button_border_brush->SetOpacity(strong_edge ? 0.85f : 0.60f);
            ctx->DrawRoundedRectangle(rr, d2d_->button_border_brush.Get(), strong_edge ? 1.6f : 1.2f);
            const D2D1_RECT_F inner_rect = inset_rect(rect, 3.0f, 3.0f);
            const D2D1_ROUNDED_RECT inner_rr =
                D2D1::RoundedRect(inner_rect, std::max(4.0f, radius - 3.0f), std::max(4.0f, radius - 3.0f));
            d2d_->button_border_brush->SetOpacity(strong_edge ? 0.28f : 0.20f);
            ctx->DrawRoundedRectangle(inner_rr, d2d_->button_border_brush.Get(), 0.9f);
            d2d_->button_border_brush->SetOpacity(saved_opacity);
        }

        draw_song_select_glow_border(rr, 0.05f + glow_strength * 0.06f, 10.0f);
        draw_song_select_glow_border(rr, 0.10f + glow_strength * 0.12f, 4.0f);
        draw_song_select_glow_border(rr, strong_edge ? 0.36f + glow_strength * 0.16f : 0.14f + glow_strength * 0.10f,
                                     strong_edge ? 1.8f : 1.2f);
    };

    auto draw_song_select_horizon = [&](float y,
                                        float left,
                                        float right,
                                        float bright_width,
                                        float base_alpha,
                                        float bright_alpha) {
        if (d2d_->button_border_brush) {
            const float saved_opacity = d2d_->button_border_brush->GetOpacity();
            d2d_->button_border_brush->SetOpacity(base_alpha);
            ctx->DrawLine(D2D1::Point2F(left, y), D2D1::Point2F(right, y), d2d_->button_border_brush.Get(), 1.2f);
            d2d_->button_border_brush->SetOpacity(saved_opacity);
        }
        if (d2d_->accent_brush) {
            const float center = (left + right) * 0.5f;
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetOpacity(bright_alpha * 0.45f);
            ctx->DrawLine(D2D1::Point2F(center - bright_width * 0.5f, y - 1.0f),
                          D2D1::Point2F(center + bright_width * 0.5f, y - 1.0f),
                          d2d_->accent_brush.Get(),
                          4.0f);
            d2d_->accent_brush->SetOpacity(bright_alpha);
            ctx->DrawLine(D2D1::Point2F(center - bright_width * 0.5f, y),
                          D2D1::Point2F(center + bright_width * 0.5f, y),
                          d2d_->accent_brush.Get(),
                          1.8f);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }
    };

    auto draw_song_select_stardust = [&](const D2D1_RECT_F& rect, int count, uint32_t seed_base, float base_alpha) {
        if ((!d2d_->accent_brush && !d2d_->text_brush) || count <= 0) {
            return;
        }
        const float width = rect.right - rect.left;
        const float height = rect.bottom - rect.top;
        for (int i = 0; i < count; ++i) {
            const uint32_t seed = seed_base + static_cast<uint32_t>(i) * 97u;
            const float x = rect.left + unit_hash_01(seed) * width;
            const float y = rect.top + unit_hash_01(seed ^ 0x68bc21ebu) * height;
            const float radius = 0.7f + unit_hash_01(seed ^ 0x13579bdfu) * 2.1f;
            const float alpha =
                base_alpha * (0.30f + unit_hash_01(seed ^ 0x24a5f123u) * 0.70f) *
                static_cast<float>(0.80 + 0.20 * pulse_wave_01(render_now_ns, 5.0, static_cast<double>(i) * 0.09));
            ID2D1SolidColorBrush* brush =
                (i % 3 == 0 && d2d_->text_brush) ? d2d_->text_brush.Get() :
                (d2d_->accent_brush ? d2d_->accent_brush.Get() : d2d_->text_brush.Get());
            if (!brush) {
                continue;
            }
            const float saved_opacity = brush->GetOpacity();
            brush->SetOpacity(alpha);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius), brush);
            brush->SetOpacity(saved_opacity);
        }
    };

    auto draw_footer = [&](std::string_view profile, int64_t high_score, std::string_view track, bool song_select_style = false) {
        const float margin = song_select_style ? 26.0f : 80.0f;
        const float bar_height = song_select_style ? 78.0f : 84.0f;
        const float bar_bottom = kBaseHeight - (song_select_style ? 20.0f : 24.0f);
        const float bar_top = bar_bottom - bar_height;
        const D2D1_RECT_F rect = D2D1::RectF(margin, bar_top, kBaseWidth - margin, bar_bottom);
        const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 18.0f, 18.0f);
        if (song_select_style) {
            draw_glass_panel(rect, 18.0f, 0.82f, 0.72f, true, 6.0f);
            draw_song_select_horizon(rect.top - 2.0f, rect.left + 6.0f, rect.right - 6.0f, 460.0f, 0.18f, 0.34f);
        } else {
            if (d2d_->footer_brush) {
                ctx->FillRoundedRectangle(rr, d2d_->footer_brush.Get());
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(rr, d2d_->button_border_brush.Get(), 1.5f);
            }
        }

        const std::string profile_text = std::string("PROFILE: ") +
                                         (profile.empty() ? std::string("PLAYER01") : std::string(profile));
        const std::string track_text = std::string("TRACK: ") + (track.empty() ? std::string("-") : std::string(track));
        const std::string score_text = std::string("HIGH SCORE  ") + format_int_with_commas(high_score);

        const std::wstring profile_w = to_wide(profile_text);
        const std::wstring track_w = to_wide(track_text);
        const std::wstring score_w = to_wide(score_text);

        if (!d2d_->hud_format || !d2d_->text_brush) {
            return;
        }

        const float text_pad = 22.0f;
        const D2D1_RECT_F left_rect = D2D1::RectF(rect.left + text_pad, rect.top, rect.left + 520.0f, rect.bottom);
        const D2D1_RECT_F center_rect =
            D2D1::RectF(rect.left + 520.0f, rect.top, rect.right - 520.0f, rect.bottom);
        const D2D1_RECT_F right_rect = D2D1::RectF(rect.right - 520.0f, rect.top, rect.right - text_pad, rect.bottom);
        const D2D1_RECT_F clip_rect =
            D2D1::RectF(rect.left + 8.0f, rect.top + 6.0f, rect.right - 8.0f, rect.bottom - 6.0f);

        ctx->PushAxisAlignedClip(clip_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        draw_text_clipped(profile_w, d2d_->hud_format.Get(), left_rect, d2d_->text_brush.Get());

        d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        draw_text_clipped(score_w, d2d_->hud_format.Get(), center_rect, d2d_->text_brush.Get());

        d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        draw_text_clipped(track_w, d2d_->hud_format.Get(), right_rect, d2d_->text_brush.Get());

        d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        ctx->PopAxisAlignedClip();
    };

    auto draw_performance_overlay = [&]() {
        if (!data.performance.visible) {
            return;
        }

        const bool compact_overlay = data.kind == MenuScreenKind::SongSelect;
        const bool gameplay_metrics_visible =
            data.performance.gameplay_metrics_visible && !compact_overlay;
        const D2D1_RECT_F panel_rect = compact_overlay
                                           ? D2D1::RectF(kBaseWidth - 438.0f, 22.0f, kBaseWidth - 24.0f, 208.0f)
                                           : D2D1::RectF(kBaseWidth - 470.0f,
                                                         44.0f,
                                                         kBaseWidth - 44.0f,
                                                         gameplay_metrics_visible ? 592.0f : 432.0f);
        const D2D1_ROUNDED_RECT panel_rr = D2D1::RoundedRect(panel_rect, 20.0f, 20.0f);
        if (d2d_->panel_brush) {
            d2d_->panel_brush->SetOpacity(0.88f);
            ctx->FillRoundedRectangle(panel_rr, d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(1.0f);
        }
        if (d2d_->button_border_brush) {
            ctx->DrawRoundedRectangle(panel_rr, d2d_->button_border_brush.Get(), 1.4f);
        }

        const std::wstring title_w = L"FRAME PACING";
        if (d2d_->body_format && d2d_->text_brush) {
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            ctx->DrawText(title_w.c_str(), static_cast<UINT32>(title_w.size()),
                          d2d_->body_format.Get(),
                          D2D1::RectF(panel_rect.left + 24.0f, panel_rect.top + 16.0f,
                                      panel_rect.right - 140.0f, panel_rect.top + 48.0f),
                          d2d_->text_brush.Get());
        }

        const bool graph_changed =
            performance_overlay_cache_.graph_revision != data.performance.graph_revision ||
            performance_overlay_cache_.compact_layout != compact_overlay;
        const bool metrics_changed =
            graph_changed || performance_overlay_cache_.metrics_revision != data.performance.metrics_revision;
        const bool gameplay_metrics_changed =
            performance_overlay_cache_.gameplay_metrics_visible != gameplay_metrics_visible ||
            performance_overlay_cache_.gameplay_metrics_revision != data.performance.gameplay_metrics_revision;

        const D2D1_RECT_F graph_rect = compact_overlay
                                           ? D2D1::RectF(panel_rect.left + 20.0f, panel_rect.top + 52.0f,
                                                         panel_rect.right - 20.0f, panel_rect.top + 116.0f)
                                           : D2D1::RectF(panel_rect.left + 24.0f, panel_rect.top + 58.0f,
                                                         panel_rect.right - 24.0f, panel_rect.top + 198.0f);
        const D2D1_ROUNDED_RECT graph_rr = D2D1::RoundedRect(graph_rect, 14.0f, 14.0f);

        if (graph_changed) {
            performance_overlay_cache_.graph_ceiling_ms = 16.67f;
            d2d_->performance_graph_geometry.Reset();

            if (data.performance.valid && data.performance.graph_sample_count > 0) {
                float max_frame_ms = 0.0f;
                for (std::size_t i = 0; i < data.performance.graph_sample_count; ++i) {
                    max_frame_ms = std::max(max_frame_ms, data.performance.frame_times_ms[i]);
                }
                performance_overlay_cache_.graph_ceiling_ms =
                    std::max(16.67f, std::ceil(std::max(2.0f, max_frame_ms) / 2.0f) * 2.0f);

                if (data.performance.graph_sample_count >= 2 && d2d_->d2d_factory) {
                    Microsoft::WRL::ComPtr<ID2D1PathGeometry> graph_geometry;
                    if (SUCCEEDED(d2d_->d2d_factory->CreatePathGeometry(&graph_geometry))) {
                        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
                        if (SUCCEEDED(graph_geometry->Open(&sink))) {
                            const float graph_width = graph_rect.right - graph_rect.left - 20.0f;
                            const float graph_height = graph_rect.bottom - graph_rect.top - 18.0f;
                            const float step =
                                graph_width / static_cast<float>(data.performance.graph_sample_count - 1);

                            auto point_for_sample = [&](std::size_t index) {
                                const float sample_ms = std::clamp(
                                    data.performance.frame_times_ms[index], 0.0f, performance_overlay_cache_.graph_ceiling_ms);
                                const float x = graph_rect.left + 10.0f + step * static_cast<float>(index);
                                const float y =
                                    graph_rect.bottom - 8.0f - (sample_ms / performance_overlay_cache_.graph_ceiling_ms) * graph_height;
                                return D2D1::Point2F(x, y);
                            };

                            sink->BeginFigure(point_for_sample(0), D2D1_FIGURE_BEGIN_HOLLOW);
                            for (std::size_t i = 1; i < data.performance.graph_sample_count; ++i) {
                                sink->AddLine(point_for_sample(i));
                            }
                            sink->EndFigure(D2D1_FIGURE_END_OPEN);
                            if (SUCCEEDED(sink->Close())) {
                                d2d_->performance_graph_geometry = std::move(graph_geometry);
                            }
                        }
                    }
                }
            }

            performance_overlay_cache_.graph_revision = data.performance.graph_revision;
            performance_overlay_cache_.compact_layout = compact_overlay;
        }

        if (metrics_changed || performance_overlay_cache_.sample_text.empty()) {
            performance_overlay_cache_.avg_line_ratio = static_cast<float>(std::clamp(
                data.performance.average_frame_ms / static_cast<double>(performance_overlay_cache_.graph_ceiling_ms), 0.0, 1.0));
            performance_overlay_cache_.sample_text =
                to_wide(std::string("SAMPLES ") + std::to_string(data.performance.sample_count));
            performance_overlay_cache_.top_label_text =
                to_wide(format_decimal(performance_overlay_cache_.graph_ceiling_ms, 2) + " ms");
            performance_overlay_cache_.avg_label_text =
                to_wide("AVG " + format_decimal(data.performance.average_frame_ms, 2));

            const double row_values[] = {
                data.performance.average_frame_ms,
                data.performance.average_fps,
                data.performance.max_fps,
                data.performance.fps_0_1_low,
                data.performance.fps_0_01_low,
            };

            for (int i = 0; i < 5; ++i) {
                performance_overlay_cache_.value_texts[static_cast<std::size_t>(i)] =
                    to_wide(format_decimal(row_values[i], (i == 0) ? 2 : 1));
            }
            performance_overlay_cache_.metrics_revision = data.performance.metrics_revision;
        }
        if (gameplay_metrics_changed) {
            const double gameplay_values[] = {
                data.performance.gameplay_audio_age_ms,
                data.performance.gameplay_hud_delta_ms,
                data.performance.gameplay_extrapolated_ms,
                data.performance.gameplay_buffer_ms,
            };
            for (int i = 0; i < 4; ++i) {
                performance_overlay_cache_.gameplay_value_texts[static_cast<std::size_t>(i)] =
                    to_wide(format_decimal(gameplay_values[i], 2));
            }
            performance_overlay_cache_.gameplay_metrics_visible = gameplay_metrics_visible;
            performance_overlay_cache_.gameplay_metrics_revision = data.performance.gameplay_metrics_revision;
        }

        if (d2d_->mono_format && d2d_->muted_brush) {
            d2d_->mono_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            ctx->DrawText(performance_overlay_cache_.sample_text.c_str(),
                          static_cast<UINT32>(performance_overlay_cache_.sample_text.size()),
                          d2d_->mono_format.Get(),
                          D2D1::RectF(panel_rect.left + 160.0f, panel_rect.top + 16.0f,
                                      panel_rect.right - 24.0f, panel_rect.top + 48.0f),
                          d2d_->muted_brush.Get());
            d2d_->mono_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
        if (d2d_->card_brush) {
            d2d_->card_brush->SetOpacity(0.80f);
            ctx->FillRoundedRectangle(graph_rr, d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(1.0f);
        }
        if (d2d_->button_border_brush) {
            ctx->DrawRoundedRectangle(graph_rr, d2d_->button_border_brush.Get(), 1.1f);
        }

        if (!data.performance.valid || data.performance.graph_sample_count == 0) {
            const std::wstring waiting_w = L"Collecting frame samples...";
            if (d2d_->hud_format && d2d_->muted_brush) {
                d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawText(waiting_w.c_str(), static_cast<UINT32>(waiting_w.size()),
                              d2d_->hud_format.Get(), graph_rect, d2d_->muted_brush.Get());
                d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            return;
        }

        const float avg_line_y = graph_rect.bottom - performance_overlay_cache_.avg_line_ratio *
                                                       (graph_rect.bottom - graph_rect.top);

        if (d2d_->muted_brush) {
            for (int i = 0; i < 4; ++i) {
                const float t = static_cast<float>(i) / 3.0f;
                const float y = graph_rect.bottom - t * (graph_rect.bottom - graph_rect.top);
                d2d_->muted_brush->SetOpacity((i == 0 || i == 3) ? 0.18f : 0.10f);
                ctx->DrawLine(D2D1::Point2F(graph_rect.left + 10.0f, y),
                              D2D1::Point2F(graph_rect.right - 10.0f, y),
                              d2d_->muted_brush.Get(), 1.0f);
            }
            d2d_->muted_brush->SetOpacity(1.0f);
        }
        if (d2d_->button_selected_brush) {
            const float original_opacity = d2d_->button_selected_brush->GetOpacity();
            d2d_->button_selected_brush->SetOpacity(0.55f);
            ctx->DrawLine(D2D1::Point2F(graph_rect.left + 10.0f, avg_line_y),
                          D2D1::Point2F(graph_rect.right - 10.0f, avg_line_y),
                          d2d_->button_selected_brush.Get(), 1.2f);
            d2d_->button_selected_brush->SetOpacity(original_opacity);
        }

        if (d2d_->mono_format && d2d_->muted_brush) {
            ctx->DrawText(performance_overlay_cache_.top_label_text.c_str(),
                          static_cast<UINT32>(performance_overlay_cache_.top_label_text.size()),
                          d2d_->mono_format.Get(),
                          D2D1::RectF(graph_rect.left + 12.0f, graph_rect.top + 6.0f,
                                      graph_rect.left + 130.0f, graph_rect.top + 28.0f),
                          d2d_->muted_brush.Get());
            ctx->DrawText(performance_overlay_cache_.avg_label_text.c_str(),
                          static_cast<UINT32>(performance_overlay_cache_.avg_label_text.size()),
                          d2d_->mono_format.Get(),
                          D2D1::RectF(graph_rect.right - 160.0f, avg_line_y - 18.0f,
                                      graph_rect.right - 12.0f, avg_line_y + 12.0f),
                          d2d_->muted_brush.Get());
        }

        if (d2d_->performance_graph_geometry.Get() && d2d_->accent_brush) {
            ctx->DrawGeometry(d2d_->performance_graph_geometry.Get(), d2d_->accent_brush.Get(), 1.8f);
        }

        if (compact_overlay) {
            const float stats_top = graph_rect.bottom + 8.0f;
            const float column_gap = 18.0f;
            const float column_width = (panel_rect.right - panel_rect.left - 40.0f - column_gap) * 0.5f;
            const float row_height = 22.0f;

            auto draw_metric_cell = [&](const wchar_t* label, const std::wstring& value_text,
                                        const D2D1_POINT_2F& origin) {
                const D2D1_RECT_F cell_rect =
                    D2D1::RectF(origin.x, origin.y, origin.x + column_width, origin.y + row_height);
                const D2D1_RECT_F label_rect =
                    D2D1::RectF(cell_rect.left, cell_rect.top, cell_rect.right - 72.0f, cell_rect.bottom);
                const D2D1_RECT_F value_rect =
                    D2D1::RectF(cell_rect.right - 78.0f, cell_rect.top, cell_rect.right, cell_rect.bottom);
                if (d2d_->body_format && d2d_->muted_brush) {
                    ctx->DrawText(label, static_cast<UINT32>(wcslen(label)),
                                  d2d_->body_format.Get(), label_rect, d2d_->muted_brush.Get());
                }
                if (d2d_->mono_format && d2d_->text_brush) {
                    d2d_->mono_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                    ctx->DrawText(value_text.c_str(), static_cast<UINT32>(value_text.size()),
                                  d2d_->mono_format.Get(), value_rect, d2d_->text_brush.Get());
                    d2d_->mono_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                }
            };

            const float left_x = panel_rect.left + 20.0f;
            const float right_x = left_x + column_width + column_gap;
            draw_metric_cell(L"AVG MS", performance_overlay_cache_.value_texts[0], D2D1::Point2F(left_x, stats_top));
            draw_metric_cell(L"AVG FPS", performance_overlay_cache_.value_texts[1], D2D1::Point2F(right_x, stats_top));
            draw_metric_cell(L"MAX FPS", performance_overlay_cache_.value_texts[2],
                             D2D1::Point2F(left_x, stats_top + row_height));
            draw_metric_cell(L"0.1% FPS", performance_overlay_cache_.value_texts[3],
                             D2D1::Point2F(right_x, stats_top + row_height));
            draw_metric_cell(L"0.01% FPS", performance_overlay_cache_.value_texts[4],
                             D2D1::Point2F(left_x, stats_top + row_height * 2.0f));
            return;
        }

        constexpr const wchar_t* kPerfRowLabels[] = {
            L"AVG MS",
            L"AVG FPS",
            L"MAX FPS",
            L"0.1% FPS",
            L"0.01% FPS",
        };
        constexpr const wchar_t* kGameplayTimingLabels[] = {
            L"AUDIO AGE",
            L"HUD DELTA",
            L"EXTRAP",
            L"BUFFER",
        };

        const float stats_top = graph_rect.bottom + 18.0f;
        const float row_height = 33.0f;
        for (int i = 0; i < 5; ++i) {
            const float top = stats_top + row_height * static_cast<float>(i);
            const D2D1_RECT_F label_rect =
                D2D1::RectF(panel_rect.left + 24.0f, top, panel_rect.left + 170.0f, top + row_height);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(panel_rect.left + 180.0f, top, panel_rect.right - 24.0f, top + row_height);
            if (d2d_->stats_label_format && d2d_->muted_brush) {
                ctx->DrawText(kPerfRowLabels[i], static_cast<UINT32>(wcslen(kPerfRowLabels[i])),
                              d2d_->stats_label_format.Get(), label_rect, d2d_->muted_brush.Get());
            }
            if (d2d_->stats_value_format && d2d_->text_brush) {
                const std::wstring& value_text = performance_overlay_cache_.value_texts[static_cast<std::size_t>(i)];
                ctx->DrawText(value_text.c_str(), static_cast<UINT32>(value_text.size()),
                              d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
            }
        }

        if (!gameplay_metrics_visible) {
            return;
        }

        const float section_top = stats_top + row_height * 5.0f + 18.0f;
        if (d2d_->button_border_brush) {
            d2d_->button_border_brush->SetOpacity(0.55f);
            ctx->DrawLine(D2D1::Point2F(panel_rect.left + 24.0f, section_top - 8.0f),
                          D2D1::Point2F(panel_rect.right - 24.0f, section_top - 8.0f),
                          d2d_->button_border_brush.Get(),
                          1.0f);
            d2d_->button_border_brush->SetOpacity(1.0f);
        }
        if (d2d_->body_format && d2d_->text_brush) {
            const std::wstring gameplay_title_w = L"GAMEPLAY TIMING";
            ctx->DrawText(gameplay_title_w.c_str(),
                          static_cast<UINT32>(gameplay_title_w.size()),
                          d2d_->body_format.Get(),
                          D2D1::RectF(panel_rect.left + 24.0f, section_top, panel_rect.right - 24.0f, section_top + 28.0f),
                          d2d_->text_brush.Get());
        }

        const float gameplay_rows_top = section_top + 30.0f;
        for (int i = 0; i < 4; ++i) {
            const float top = gameplay_rows_top + row_height * static_cast<float>(i);
            const D2D1_RECT_F label_rect =
                D2D1::RectF(panel_rect.left + 24.0f, top, panel_rect.left + 170.0f, top + row_height);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(panel_rect.left + 180.0f, top, panel_rect.right - 24.0f, top + row_height);
            if (d2d_->stats_label_format && d2d_->muted_brush) {
                ctx->DrawText(kGameplayTimingLabels[i],
                              static_cast<UINT32>(wcslen(kGameplayTimingLabels[i])),
                              d2d_->stats_label_format.Get(),
                              label_rect,
                              d2d_->muted_brush.Get());
            }
            if (d2d_->stats_value_format && d2d_->text_brush) {
                const std::wstring& value_text =
                    performance_overlay_cache_.gameplay_value_texts[static_cast<std::size_t>(i)];
                ctx->DrawText(value_text.c_str(),
                              static_cast<UINT32>(value_text.size()),
                              d2d_->stats_value_format.Get(),
                              value_rect,
                              d2d_->text_brush.Get());
            }
        }
    };

    auto draw_generic_list = [&]() {
        const float left = 80.0f;
        const float top = 140.0f;
        const float right = kBaseWidth - 80.0f;
        const float bottom = kBaseHeight - 120.0f;

        if (d2d_->card_brush) {
            D2D1_ROUNDED_RECT card =
                D2D1::RoundedRect(D2D1::RectF(left, top, right, bottom), 18.0f, 18.0f);
            ctx->FillRoundedRectangle(card, d2d_->card_brush.Get());
        }

        std::string header = "TenRiff";
        if (!data.generic.heading.empty()) {
            header += " / " + data.generic.heading;
        } else if (!data.screen_title.empty()) {
            header += " / " + data.screen_title;
        }
        const std::wstring header_wide = to_wide(header);
        D2D1_RECT_F header_rect = D2D1::RectF(left, 48.0f, right, 120.0f);
        if (d2d_->title_format && d2d_->accent_brush) {
            ctx->DrawText(header_wide.c_str(), static_cast<UINT32>(header_wide.size()),
                          d2d_->title_format.Get(), header_rect, d2d_->accent_brush.Get());
        }

        const bool has_skin_preview = data.generic.skin_preview.visible;
        const float preview_gap = has_skin_preview ? 28.0f : 0.0f;
        const float preview_width = has_skin_preview ? 620.0f : 0.0f;

        auto draw_skin_preview_panel = [&](const SkinPreviewData& preview, const D2D1_RECT_F& rect) {
            const D2D1_ROUNDED_RECT panel_rr = D2D1::RoundedRect(rect, 18.0f, 18.0f);
            if (d2d_->panel_brush) {
                d2d_->panel_brush->SetOpacity(0.90f);
                ctx->FillRoundedRectangle(panel_rr, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(panel_rr, d2d_->button_border_brush.Get(), 1.2f);
            }

            const std::wstring title_w = L"LIVE PREVIEW";
            const std::wstring mode_w =
                to_wide(preview.mode_label + " / Lane " + std::to_string(std::max(1, preview.selected_lane)));
            const std::wstring color_w = to_wide("Color: " + preview.selected_color_label);
            if (d2d_->title_format && d2d_->text_brush) {
                ctx->DrawText(title_w.c_str(), static_cast<UINT32>(title_w.size()),
                              d2d_->title_format.Get(),
                              D2D1::RectF(rect.left + 24.0f, rect.top + 18.0f, rect.right - 24.0f, rect.top + 60.0f),
                              d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                ctx->DrawText(mode_w.c_str(), static_cast<UINT32>(mode_w.size()),
                              d2d_->body_format.Get(),
                              D2D1::RectF(rect.left + 24.0f, rect.top + 56.0f, rect.right - 24.0f, rect.top + 88.0f),
                              d2d_->muted_brush.Get());
                ctx->DrawText(color_w.c_str(), static_cast<UINT32>(color_w.size()),
                              d2d_->body_format.Get(),
                              D2D1::RectF(rect.left + 24.0f, rect.top + 86.0f, rect.right - 24.0f, rect.top + 118.0f),
                              d2d_->muted_brush.Get());
            }

            const float preview_bounds_left = rect.left + 28.0f;
            const float preview_bounds_right = rect.right - 28.0f;
            const float field_top = rect.top + 132.0f;
            const float field_bottom = rect.bottom - 152.0f;
            const int lane_count = std::clamp(preview.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
            const GameplayFieldLayout field_layout = build_gameplay_field_layout(
                preview_bounds_left,
                preview_bounds_right,
                field_top,
                field_bottom,
                lane_count,
                preview.note_width_scale);
            const float field_height = field_layout.height;
            const float field_left = field_layout.left;
            const float field_right = field_layout.right;
            const float lane_width = field_layout.lane_width;
            const float hit_line_y =
                gameplay_field_y(field_layout.top,
                                 field_height,
                                 clamp_gameplay_judgement_line(preview.judgement_line_position));
            const float note_width = field_layout.note_width;
            const float head_half_h = gameplay_note_head_half_height(preview.note_height_scale);
            const float tail_half_h = gameplay_note_tail_half_height(preview.note_height_scale);
            std::array<float, kGameplayHudMaxLanes> preview_lane_divider_widths{};
            const std::array<float, kGameplayHudMaxLanes> no_imported_lane_divider_widths{};
            const std::size_t preview_lane_divider_width_count = resolve_gameplay_lane_divider_widths(
                lane_count,
                preview.lane_divider_width_scale,
                0,
                no_imported_lane_divider_widths,
                preview_lane_divider_widths);
            const float hold_body_width_scale =
                clamp_gameplay_hold_body_width_scale(preview.hold_body_width_scale);
            const double combo_position = clamp_gameplay_combo_position(preview.combo_position);
            const std::string note_shape = normalize_gameplay_note_shape(preview.note_shape);
            const bool note_border_enabled = preview.note_border_enabled;

            const D2D1_ROUNDED_RECT field_rr =
                D2D1::RoundedRect(D2D1::RectF(field_left, field_layout.top, field_right, field_layout.bottom),
                                  14.0f,
                                  14.0f);
            if (d2d_->card_brush) {
                ctx->FillRoundedRectangle(field_rr, d2d_->card_brush.Get());
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(field_rr, d2d_->button_border_brush.Get(), 1.1f);
            }

            for (int lane = 0; lane < lane_count; ++lane) {
                const float x0 = field_left + static_cast<float>(lane) * lane_width;
                const float x1 = x0 + lane_width;
                const uint32_t rgb = preview.lane_colors[static_cast<std::size_t>(lane)];
                if (d2d_->note_fill_brush) {
                    d2d_->note_fill_brush->SetColor(gameplay_lane_preview_fill(rgb, lane + 1 == preview.selected_lane));
                    ctx->FillRectangle(D2D1::RectF(x0 + 2.0f,
                                                   field_layout.top + 2.0f,
                                                   x1 - 2.0f,
                                                   field_layout.bottom - 2.0f),
                                       d2d_->note_fill_brush.Get());
                }
                if (d2d_->button_border_brush &&
                    static_cast<std::size_t>(lane) < preview_lane_divider_width_count) {
                    const float divider_width = preview_lane_divider_widths[static_cast<std::size_t>(lane)];
                    if (divider_width <= 0.01f) {
                        continue;
                    }
                    ctx->DrawLine(D2D1::Point2F(x1, field_layout.top), D2D1::Point2F(x1, field_layout.bottom),
                                  d2d_->button_border_brush.Get(), divider_width);
                }
            }

            if (d2d_->judgement_line_brush) {
                const D2D1_RECT_F hit_line_rect =
                    gameplay_judgement_line_rect(field_layout, hit_line_y, preview.note_height_scale);
                ctx->FillRectangle(hit_line_rect, d2d_->judgement_line_brush.Get());
                ctx->DrawLine(D2D1::Point2F(field_left, hit_line_y), D2D1::Point2F(field_right, hit_line_y),
                              d2d_->judgement_line_brush.Get(), 1.4f);
            }

            for (int lane = 0; lane < lane_count; ++lane) {
                const uint32_t rgb = preview.lane_colors[static_cast<std::size_t>(lane)];
                const float lane_center = field_left + (static_cast<float>(lane) + 0.5f) * lane_width;
                const float x0 = lane_center - note_width * 0.5f;
                const float x1 = lane_center + note_width * 0.5f;
                const float default_y =
                    field_layout.top + field_height * (0.16f + 0.09f * static_cast<float>((lane + 1) % 4));
                const bool draw_selected_hold_preview = (lane + 1 == preview.selected_lane) && d2d_->note_hold_brush;
                const float y = draw_selected_hold_preview ? hit_line_y : default_y;
                if (d2d_->note_fill_brush) {
                    d2d_->note_fill_brush->SetColor(gameplay_note_fill_color(rgb));
                }
                if (d2d_->note_border_brush) {
                    d2d_->note_border_brush->SetColor(gameplay_note_border_color(rgb));
                }
                if (d2d_->note_hold_brush) {
                    d2d_->note_hold_brush->SetColor(gameplay_note_hold_color(rgb));
                }

                if (draw_selected_hold_preview) {
                    const float tail_y =
                        std::max(field_layout.top + 20.0f, hit_line_y - field_height * 0.18f);
                    const float hold_half_width = std::max(4.0f, note_width * 0.5f * hold_body_width_scale);
                    const float head_body_inset = gameplay_hold_body_cap_inset(note_shape, head_half_h);
                    const D2D1_RECT_F hold_rect =
                        D2D1::RectF(lane_center - hold_half_width,
                                    tail_y,
                                    lane_center + hold_half_width,
                                    y - head_body_inset);
                    if (hold_rect.bottom > hold_rect.top) {
                        ctx->FillRectangle(hold_rect, d2d_->note_hold_brush.Get());
                    }
                }

                const D2D1_RECT_F note_rect = D2D1::RectF(x0, y - head_half_h, x1, y + head_half_h);
                if (d2d_->note_fill_brush) {
                    draw_note_primitive(ctx, note_rect, d2d_->note_fill_brush.Get(), d2d_->note_border_brush.Get(),
                                        1.2f, note_shape, note_border_enabled);
                }
            }

            if (d2d_->header_format && d2d_->accent_brush) {
                const std::wstring combo_w = L"123";
                const float combo_y = gameplay_field_y(field_layout.top, field_height, combo_position);
                const D2D1_RECT_F combo_rect =
                    D2D1::RectF(field_left, combo_y - 40.0f, field_right, combo_y + 40.0f);
                ctx->DrawText(combo_w.c_str(), static_cast<UINT32>(combo_w.size()),
                              d2d_->header_format.Get(), combo_rect, d2d_->accent_brush.Get());
            }

            const float swatch_top = rect.bottom - 96.0f;
            const float swatch_height = 54.0f;
            for (int lane = 0; lane < lane_count; ++lane) {
                const float x0 = field_left + static_cast<float>(lane) * lane_width;
                const float x1 = x0 + lane_width;
                const D2D1_RECT_F swatch_rect = D2D1::RectF(x0 + 4.0f, swatch_top, x1 - 4.0f, swatch_top + swatch_height);
                const D2D1_ROUNDED_RECT swatch_rr = D2D1::RoundedRect(swatch_rect, 10.0f, 10.0f);
                if (d2d_->note_fill_brush) {
                    d2d_->note_fill_brush->SetColor(gameplay_note_fill_color(preview.lane_colors[static_cast<std::size_t>(lane)]));
                    ctx->FillRoundedRectangle(swatch_rr, d2d_->note_fill_brush.Get());
                }
                ID2D1SolidColorBrush* border =
                    (lane + 1 == preview.selected_lane) ? d2d_->accent_brush.Get() : d2d_->button_border_brush.Get();
                if (border) {
                    ctx->DrawRoundedRectangle(swatch_rr, border, lane + 1 == preview.selected_lane ? 2.0f : 1.0f);
                }
                if (d2d_->body_format && d2d_->text_brush) {
                    const std::wstring lane_w = to_wide(std::to_string(lane + 1));
                    d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    ctx->DrawText(lane_w.c_str(), static_cast<UINT32>(lane_w.size()),
                                  d2d_->body_format.Get(), swatch_rect, d2d_->text_brush.Get());
                    d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                }
            }
        };

        if (!data.generic.rows.empty() || !data.generic.notes.empty()) {
            const float row_left = left + 24.0f;
            const float base_row_right = has_skin_preview ? (right - preview_width - preview_gap) : (right - 24.0f);
            const bool roomy_option_layout = data.generic.rows.size() <= 12;
            const float row_height = roomy_option_layout ? 54.0f : 48.0f;
            const float row_gap = roomy_option_layout ? 10.0f : 8.0f;
            const float row_step = row_height + row_gap;
            const float value_width = has_skin_preview ? (roomy_option_layout ? 270.0f : 240.0f)
                                                       : (roomy_option_layout ? 360.0f : 340.0f);
            const float action_width = roomy_option_layout ? 62.0f : 56.0f;
            const float action_gap = 10.0f;
            const float note_line_height = roomy_option_layout ? 34.0f : 28.0f;
            const float note_section_gap = data.generic.notes.empty() ? 0.0f : (roomy_option_layout ? 18.0f : 14.0f);
            const float list_top = top + 24.0f;
            const float list_bottom_limit = bottom - 20.0f;
            IDWriteTextFormat* row_format =
                roomy_option_layout && d2d_->option_format ? d2d_->option_format.Get() : d2d_->body_format.Get();

            if (has_skin_preview) {
                draw_skin_preview_panel(data.generic.skin_preview,
                                        D2D1::RectF(base_row_right + preview_gap, top + 24.0f, right - 24.0f, bottom - 24.0f));
            }

            int displayed_note_count = static_cast<int>(data.generic.notes.size());
            float notes_height = 0.0f;
            if (displayed_note_count > 0) {
                notes_height = note_section_gap + note_line_height * static_cast<float>(displayed_note_count);
            }

            float row_region_bottom = list_bottom_limit - notes_height;
            const float minimum_row_region_height = data.generic.rows.empty() ? 0.0f : row_height;
            if (minimum_row_region_height > 0.0f && row_region_bottom - list_top < minimum_row_region_height && displayed_note_count > 0) {
                displayed_note_count = std::min(displayed_note_count, 3);
                notes_height = note_section_gap + note_line_height * static_cast<float>(displayed_note_count);
                row_region_bottom = list_bottom_limit - notes_height;
            }
            if (minimum_row_region_height > 0.0f && row_region_bottom - list_top < minimum_row_region_height) {
                displayed_note_count = 0;
                notes_height = 0.0f;
                row_region_bottom = list_bottom_limit;
            }

            int selected_row_index = 0;
            for (std::size_t i = 0; i < data.generic.rows.size(); ++i) {
                if (data.generic.rows[i].selected) {
                    selected_row_index = static_cast<int>(i);
                    break;
                }
            }

            int visible_row_count = 0;
            if (!data.generic.rows.empty()) {
                visible_row_count = std::max(1, static_cast<int>(
                                                    std::floor((row_region_bottom - list_top + row_gap) / row_step)));
                visible_row_count = std::min<int>(visible_row_count, static_cast<int>(data.generic.rows.size()));
            }

            int row_window_start = 0;
            if (!data.generic.rows.empty() && visible_row_count < static_cast<int>(data.generic.rows.size())) {
                const int max_window_start = static_cast<int>(data.generic.rows.size()) - visible_row_count;
                row_window_start = std::clamp(selected_row_index - visible_row_count / 2, 0, max_window_start);
            }
            const bool show_scrollbar =
                !data.generic.rows.empty() && visible_row_count < static_cast<int>(data.generic.rows.size());
            const float scrollbar_gap = show_scrollbar ? 12.0f : 0.0f;
            const float scrollbar_width = show_scrollbar ? 10.0f : 0.0f;
            const float row_right = base_row_right - scrollbar_gap - scrollbar_width;
            float row_y = list_top;

            const int row_window_end = std::min<int>(static_cast<int>(data.generic.rows.size()),
                                                     row_window_start + visible_row_count);
            for (int row_list_index = row_window_start; row_list_index < row_window_end; ++row_list_index) {
                const auto& row = data.generic.rows[static_cast<std::size_t>(row_list_index)];
                const bool highlight = row.selected || row.activatable || row.adjustable;
                const D2D1_RECT_F row_rect = D2D1::RectF(row_left, row_y, row_right, row_y + row_height);
                const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(row_rect, 12.0f, 12.0f);
                if (highlight) {
                    ID2D1SolidColorBrush* fill =
                        row.selected ? d2d_->button_selected_brush.Get() : d2d_->button_brush.Get();
                    if (fill) {
                        ctx->FillRoundedRectangle(rr, fill);
                    }
                    ID2D1SolidColorBrush* border =
                        row.selected ? d2d_->accent_brush.Get() : d2d_->button_border_brush.Get();
                    if (border) {
                        ctx->DrawRoundedRectangle(rr, border, row.selected ? 2.0f : 1.0f);
                    }
                }

                const std::wstring label_w = to_wide(row.label);
                float label_right = row_right - value_width - 18.0f;
                if (row.adjustable) {
                    label_right = row_right - value_width - action_width * 2.0f - action_gap * 2.0f - 18.0f;
                } else if (row.value.empty()) {
                    label_right = row_right - 18.0f;
                }
                const D2D1_RECT_F label_rect =
                    D2D1::RectF(row_left + 18.0f, row_y + 8.0f, std::max(row_left + 160.0f, label_right), row_y + row_height - 8.0f);
                if (row_format && d2d_->text_brush) {
                    ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                                  row_format, label_rect, d2d_->text_brush.Get());
                }

                if (!row.value.empty() && row_format && d2d_->text_brush) {
                    const std::wstring value_w = to_wide(row.value);
                    if (row.adjustable) {
                        const float plus_left = row_right - action_width;
                        const float minus_left = plus_left - action_gap - action_width;
                        const float value_right = minus_left - action_gap;
                        const D2D1_RECT_F value_rect =
                            D2D1::RectF(std::max(label_rect.right + 12.0f, row_left + 320.0f),
                                        row_y + 8.0f,
                                        value_right,
                                        row_y + row_height - 8.0f);
                        row_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                        ctx->DrawText(value_w.c_str(), static_cast<UINT32>(value_w.size()),
                                      row_format, value_rect, d2d_->text_brush.Get());
                        row_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

                        const auto draw_action = [&](const D2D1_RECT_F& rect, wchar_t symbol, MenuHitPart part, bool enabled) {
                            const D2D1_ROUNDED_RECT action_rr = D2D1::RoundedRect(rect, 10.0f, 10.0f);
                            ID2D1SolidColorBrush* fill = enabled ? d2d_->button_brush.Get() : d2d_->card_brush.Get();
                            if (fill) {
                                ctx->FillRoundedRectangle(action_rr, fill);
                            }
                            ID2D1SolidColorBrush* border = enabled ? d2d_->button_border_brush.Get() : d2d_->muted_brush.Get();
                            if (border) {
                                ctx->DrawRoundedRectangle(action_rr, border, 1.0f);
                            }
                            if (enabled) {
                                register_hit(rect, row.target_kind, row.row_index, part);
                            }
                            if (d2d_->title_format && d2d_->text_brush) {
                                const wchar_t buffer[2] = {symbol, L'\0'};
                                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                                ctx->DrawText(buffer, 1, d2d_->title_format.Get(), rect, d2d_->text_brush.Get());
                                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                            }
                        };

                        const D2D1_RECT_F minus_rect =
                            D2D1::RectF(minus_left, row_y + 6.0f, minus_left + action_width, row_y + row_height - 6.0f);
                        const D2D1_RECT_F plus_rect =
                            D2D1::RectF(plus_left, row_y + 6.0f, plus_left + action_width, row_y + row_height - 6.0f);
                        draw_action(minus_rect, L'-', MenuHitPart::Decrement, row.decrement_enabled);
                        draw_action(plus_rect, L'+', MenuHitPart::Increment, row.increment_enabled);
                    } else {
                        const D2D1_RECT_F value_rect =
                            D2D1::RectF(std::max(label_rect.right + 12.0f, row_left + 320.0f),
                                        row_y + 8.0f,
                                        row_right - 18.0f,
                                        row_y + row_height - 8.0f);
                        row_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                        ctx->DrawText(value_w.c_str(), static_cast<UINT32>(value_w.size()),
                                      row_format, value_rect, d2d_->text_brush.Get());
                        row_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                    }
                }

                if (row.activatable) {
                    register_hit(row_rect, row.target_kind, row.row_index, MenuHitPart::Activate);
                }
                row_y += row_step;
            }

            if (show_scrollbar && d2d_->button_border_brush) {
                const float track_top = list_top + 4.0f;
                const float track_bottom = row_region_bottom - 4.0f;
                const D2D1_RECT_F track_rect =
                    D2D1::RectF(row_right + scrollbar_gap, track_top, row_right + scrollbar_gap + scrollbar_width, track_bottom);
                if (track_rect.bottom > track_rect.top) {
                    if (d2d_->card_brush) {
                        d2d_->card_brush->SetOpacity(0.70f);
                        ctx->FillRoundedRectangle(D2D1::RoundedRect(track_rect, scrollbar_width * 0.5f, scrollbar_width * 0.5f),
                                                  d2d_->card_brush.Get());
                        d2d_->card_brush->SetOpacity(1.0f);
                    }
                    ctx->DrawRoundedRectangle(D2D1::RoundedRect(track_rect, scrollbar_width * 0.5f, scrollbar_width * 0.5f),
                                              d2d_->button_border_brush.Get(), 1.0f);

                    const float track_height = track_rect.bottom - track_rect.top;
                    const float total_rows = static_cast<float>(data.generic.rows.size());
                    const float visible_rows = static_cast<float>(visible_row_count);
                    const float thumb_height =
                        std::min(track_height, std::max(44.0f, track_height * (visible_rows / total_rows)));
                    const int max_window_start = static_cast<int>(data.generic.rows.size()) - visible_row_count;
                    const float scroll_ratio = (max_window_start <= 0)
                                                   ? 0.0f
                                                   : static_cast<float>(row_window_start) /
                                                         static_cast<float>(max_window_start);
                    const float thumb_top = track_rect.top + (track_height - thumb_height) * scroll_ratio;
                    const D2D1_RECT_F thumb_rect =
                        D2D1::RectF(track_rect.left + 1.0f, thumb_top, track_rect.right - 1.0f, thumb_top + thumb_height);
                    if (d2d_->accent_brush) {
                        d2d_->accent_brush->SetOpacity(0.92f);
                        ctx->FillRoundedRectangle(D2D1::RoundedRect(thumb_rect, scrollbar_width * 0.5f, scrollbar_width * 0.5f),
                                                  d2d_->accent_brush.Get());
                        d2d_->accent_brush->SetOpacity(1.0f);
                    }
                }
            }

            if (displayed_note_count > 0) {
                const float notes_top = list_bottom_limit - notes_height;
                float note_y = notes_top + note_section_gap;
                if (d2d_->button_border_brush) {
                    d2d_->button_border_brush->SetOpacity(0.35f);
                    ctx->DrawLine(D2D1::Point2F(row_left, notes_top), D2D1::Point2F(row_right, notes_top),
                                  d2d_->button_border_brush.Get(), 1.0f);
                    d2d_->button_border_brush->SetOpacity(1.0f);
                }
                for (int i = 0; i < displayed_note_count; ++i) {
                    const std::wstring note_w = to_wide(data.generic.notes[static_cast<std::size_t>(i)]);
                    const D2D1_RECT_F note_rect =
                        D2D1::RectF(row_left + 6.0f, note_y, row_right - 6.0f, note_y + 30.0f);
                    if (row_format && d2d_->muted_brush) {
                        ctx->DrawText(note_w.c_str(), static_cast<UINT32>(note_w.size()),
                                      row_format, note_rect, d2d_->muted_brush.Get());
                    }
                    note_y += note_line_height;
                }
            }
            return;
        }

        const float line_left = left + 24.0f;
        float line_y = top + 24.0f;
        const float line_height = 26.0f;
        for (const auto& line : data.lines) {
            if (line_y + line_height > bottom - 16.0f) {
                break;
            }
            const bool selected = line_is_selected(line);
            const bool is_option = line_has_prefix(line);
            const std::wstring text = to_wide(strip_prefix(line));
            D2D1_RECT_F line_rect =
                D2D1::RectF(line_left, line_y, right - 24.0f, line_y + line_height);

            if (is_option) {
                D2D1_RECT_F button_rect = D2D1::RectF(line_left - 12.0f, line_y - 4.0f,
                                                      right - 24.0f, line_y + line_height + 4.0f);
                D2D1_ROUNDED_RECT button = D2D1::RoundedRect(button_rect, 10.0f, 10.0f);
                ID2D1SolidColorBrush* fill =
                    selected ? d2d_->button_selected_brush.Get() : d2d_->button_brush.Get();
                if (fill) {
                    ctx->FillRoundedRectangle(button, fill);
                }
                ID2D1SolidColorBrush* border =
                    selected ? d2d_->accent_brush.Get() : d2d_->button_border_brush.Get();
                if (border) {
                    ctx->DrawRoundedRectangle(button, border, selected ? 2.0f : 1.0f);
                }
            }

            ID2D1SolidColorBrush* brush = d2d_->text_brush.Get();
            if (brush && d2d_->body_format) {
                ctx->DrawText(text.c_str(), static_cast<UINT32>(text.size()),
                              d2d_->body_format.Get(), line_rect, brush);
            }
            line_y += line_height + 8.0f;
        }
    };

    auto draw_title_menu = [&]() {
        const double tick = static_cast<double>(GetTickCount64()) / 1000.0;
        const float logo_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 5.4, 0.18));
        const float button_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 4.2, 0.46));
        const float bar_base_y = 150.0f;
        const int bar_count = 18;
        const float bar_w = 18.0f;
        const float bar_gap = 12.0f;
        const float total_w = bar_count * bar_w + (bar_count - 1) * bar_gap;
        const float start_x = (kBaseWidth - total_w) * 0.5f;

        if (d2d_->accent_brush) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
            for (int i = 0; i < bar_count; ++i) {
                const double phase = tick * 2.2 + static_cast<double>(i) * 0.45;
                const float height = 18.0f + 66.0f * static_cast<float>(0.5 + 0.5 * std::sin(phase));
                const float x0 = start_x + static_cast<float>(i) * (bar_w + bar_gap);
                const D2D1_ROUNDED_RECT bar =
                    D2D1::RoundedRect(D2D1::RectF(x0, bar_base_y - height, x0 + bar_w, bar_base_y),
                                      4.0f, 4.0f);
                d2d_->accent_brush->SetOpacity(0.42f + 0.18f * static_cast<float>(std::sin(phase) * 0.5 + 0.5));
                ctx->FillRoundedRectangle(bar, d2d_->accent_brush.Get());
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        draw_song_select_horizon(344.0f, 186.0f, kBaseWidth - 186.0f, 820.0f, 0.10f, 0.28f + logo_pulse * 0.20f);
        draw_song_select_stardust(D2D1::RectF(114.0f, 96.0f, 1810.0f, 364.0f), 36, 0x491u, 0.09f);

        const D2D1_RECT_F logo_rect = D2D1::RectF(0.0f, 160.0f, kBaseWidth, 320.0f);
        const std::wstring logo_w = L"TENRIFF";
        if (d2d_->logo_format && d2d_->text_brush) {
            ID2D1Brush* brush = d2d_->logo_brush ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                                 : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
            if (d2d_->logo_brush) {
                set_brush_points(d2d_->logo_brush.Get(), logo_rect);
            }
            if (brush) {
                ctx->DrawText(logo_w.c_str(), static_cast<UINT32>(logo_w.size()),
                              d2d_->logo_format.Get(), logo_rect, brush);
            }
        }

        const float button_w = 980.0f;
        const float button_h = 120.0f;
        const float button_gap = 26.0f;
        const float button_left = (kBaseWidth - button_w) * 0.5f;
        const float button_top = 380.0f;

        for (std::size_t i = 0; i < data.title.buttons.size(); ++i) {
            const auto& button = data.title.buttons[i];
            const float y0 = button_top + static_cast<float>(i) * (button_h + button_gap);
            const D2D1_RECT_F rect = D2D1::RectF(button_left, y0, button_left + button_w, y0 + button_h);
            register_hit(rect, MenuHitTargetKind::TitleButton, static_cast<int>(i));

            ID2D1LinearGradientBrush* fill = nullptr;
            if (i == 0) {
                fill = d2d_->play_brush.Get();
            } else if (i == 1) {
                fill = d2d_->edit_brush.Get();
            } else if (i == 2) {
                fill = d2d_->options_brush.Get();
            } else if (i == 3) {
                fill = d2d_->exit_brush.Get();
            }

            draw_glass_panel(rect,
                             18.0f,
                             button.selected ? 0.84f : 0.74f,
                             button.selected ? (0.58f + button_pulse * 0.20f) : 0.22f,
                             button.selected,
                             8.0f);

            if (fill) {
                const D2D1_RECT_F accent_rect =
                    D2D1::RectF(rect.left + 18.0f, rect.top + 16.0f, rect.left + 34.0f, rect.bottom - 16.0f);
                const D2D1_ROUNDED_RECT accent_rr = D2D1::RoundedRect(accent_rect, 7.0f, 7.0f);
                const D2D1_RECT_F top_line =
                    D2D1::RectF(rect.left + 52.0f, rect.top + 14.0f, rect.right - 28.0f, rect.top + 18.0f);
                const D2D1_ROUNDED_RECT top_line_rr = D2D1::RoundedRect(top_line, 2.0f, 2.0f);
                set_brush_points(fill, rect);
                const float saved_opacity = fill->GetOpacity();
                fill->SetOpacity(button.selected ? 0.92f : 0.68f);
                ctx->FillRoundedRectangle(accent_rr, fill);
                fill->SetOpacity(button.selected ? 0.44f : 0.24f);
                ctx->FillRoundedRectangle(top_line_rr, fill);
                fill->SetOpacity(saved_opacity);
            }

            const D2D1_RECT_F icon_rect = D2D1::RectF(rect.left + 52.0f, rect.top, rect.left + 162.0f, rect.bottom);
            const D2D1_RECT_F label_rect =
                D2D1::RectF(rect.left + 172.0f, rect.top, rect.right - 20.0f, rect.bottom);

            if (d2d_->menu_icon_format && d2d_->text_brush) {
                const std::wstring icon_w = to_wide(button.icon);
                if (!icon_w.empty()) {
                    ctx->DrawText(icon_w.c_str(), static_cast<UINT32>(icon_w.size()),
                                  d2d_->menu_icon_format.Get(), icon_rect, d2d_->text_brush.Get());
                }
            }

            if (d2d_->menu_button_format && d2d_->text_brush) {
                const std::wstring label_w = to_wide(button.label);
                ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                              d2d_->menu_button_format.Get(), label_rect, d2d_->text_brush.Get());
            }
        }

        if (!data.title.guides.empty()) {
            const D2D1_RECT_F guide_rect = D2D1::RectF(1492.0f, 396.0f, 1834.0f, 820.0f);
            draw_glass_panel(guide_rect, 18.0f, 0.76f, 0.24f + logo_pulse * 0.10f, false, 8.0f);

            const D2D1_RECT_F guide_header_rect =
                D2D1::RectF(guide_rect.left + 26.0f, guide_rect.top + 18.0f, guide_rect.right - 26.0f, guide_rect.top + 64.0f);
            if (d2d_->body_format && d2d_->accent_brush) {
                const std::wstring guide_header_w = L"GUIDE";
                draw_text_clipped(guide_header_w, d2d_->body_format.Get(), guide_header_rect, d2d_->accent_brush.Get());
            }
            draw_song_select_horizon(guide_rect.top + 72.0f,
                                     guide_rect.left + 22.0f,
                                     guide_rect.right - 22.0f,
                                     170.0f,
                                     0.08f,
                                     0.18f + logo_pulse * 0.10f);

            float guide_y = guide_rect.top + 98.0f;
            for (std::size_t i = 0; i < data.title.guides.size(); ++i) {
                const D2D1_RECT_F line_rect =
                    D2D1::RectF(guide_rect.left + 26.0f, guide_y, guide_rect.right - 26.0f, guide_y + 56.0f);
                if (d2d_->body_format && d2d_->text_brush) {
                    const std::wstring line_w = to_wide(data.title.guides[i]);
                    draw_text_clipped(line_w, d2d_->body_format.Get(), line_rect, d2d_->text_brush.Get());
                }
                guide_y += 68.0f;
                if (i + 1 < data.title.guides.size() && d2d_->button_border_brush) {
                    const float saved_opacity = d2d_->button_border_brush->GetOpacity();
                    d2d_->button_border_brush->SetOpacity(0.16f);
                    ctx->DrawLine(D2D1::Point2F(guide_rect.left + 24.0f, guide_y - 10.0f),
                                  D2D1::Point2F(guide_rect.right - 24.0f, guide_y - 10.0f),
                                  d2d_->button_border_brush.Get(),
                                  1.0f);
                    d2d_->button_border_brush->SetOpacity(saved_opacity);
                }
            }
        }

        draw_footer(data.title.profile, data.title.high_score, data.title.track);
    };

    auto draw_song_select = [&]() {
        const float ambient_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 7.2, 0.12));
        const float header_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 4.8, 0.03));
        const float nav_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 3.9, 0.28));
        const float card_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 4.5, 0.51));
        const float preview_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 5.6, 0.74));

        auto draw_section_divider = [&](float y, float left, float right, float alpha) {
            if (!d2d_->button_border_brush) {
                return;
            }
            const float saved_opacity = d2d_->button_border_brush->GetOpacity();
            d2d_->button_border_brush->SetOpacity(alpha);
            ctx->DrawLine(D2D1::Point2F(left, y), D2D1::Point2F(right, y), d2d_->button_border_brush.Get(), 1.0f);
            d2d_->button_border_brush->SetOpacity(saved_opacity);
        };

        auto draw_stat_section = [&](float y, std::string_view label, float left, float right) {
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring section_w = to_wide(std::string(label));
                draw_text_clipped(section_w,
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(left, y, right, y + 22.0f),
                                  d2d_->muted_brush.Get());
            }
            if (d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.22f);
                ctx->DrawLine(D2D1::Point2F(left, y + 24.0f), D2D1::Point2F(right, y + 24.0f),
                              d2d_->accent_brush.Get(), 1.1f);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            return y + 30.0f;
        };

        auto draw_chip = [&](const D2D1_RECT_F& rect, std::string_view text, bool selected) {
            draw_glass_panel(rect,
                             11.0f,
                             selected ? 0.84f : 0.70f,
                             selected ? (0.40f + card_pulse * 0.18f) : 0.12f,
                             selected,
                             3.0f);
            if (d2d_->body_format && d2d_->text_brush) {
                const std::wstring chip_w = to_wide(std::string(text));
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_text_clipped(chip_w, d2d_->body_format.Get(), rect, d2d_->text_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        };

        if (d2d_->accent_brush) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
            d2d_->accent_brush->SetOpacity(0.020f + header_pulse * 0.012f);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kBaseWidth * 0.5f, 114.0f), 220.0f, 38.0f),
                             d2d_->accent_brush.Get());
            d2d_->accent_brush->SetOpacity(0.016f + ambient_pulse * 0.010f);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(250.0f, 950.0f), 220.0f, 70.0f),
                             d2d_->accent_brush.Get());
            d2d_->accent_brush->SetOpacity(0.012f + preview_pulse * 0.008f);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(1638.0f, 914.0f), 200.0f, 62.0f),
                             d2d_->accent_brush.Get());
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        draw_song_select_horizon(154.0f, 86.0f, kBaseWidth - 86.0f, 840.0f, 0.12f, 0.44f + header_pulse * 0.18f);
        draw_song_select_horizon(kBaseHeight - 228.0f, 18.0f, kBaseWidth - 18.0f, 720.0f, 0.10f,
                                 0.24f + ambient_pulse * 0.10f);
        draw_song_select_stardust(D2D1::RectF(66.0f, 882.0f, 904.0f, 1042.0f), 38, 0x51u, 0.12f);
        draw_song_select_stardust(D2D1::RectF(960.0f, 874.0f, 1770.0f, 1038.0f), 24, 0x251u, 0.06f);

        const D2D1_RECT_F header_rect = D2D1::RectF(0.0f, 70.0f, kBaseWidth, 170.0f);
        const std::wstring header_w = L"TENG SELECT";
        ID2D1Brush* header_brush = d2d_->logo_brush ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                                    : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
        if (d2d_->logo_brush) {
            set_brush_points(d2d_->logo_brush.Get(), header_rect);
        }
        if (d2d_->header_format && header_brush) {
            draw_text_clipped(header_w, d2d_->header_format.Get(), header_rect, header_brush);
        }

        if (data.song_select.indexing && d2d_->hud_format && d2d_->muted_brush) {
            std::string status = data.song_select.indexing_stage.empty() ? std::string("INDEXING")
                                                                         : data.song_select.indexing_stage;
            if (data.song_select.indexing_percent >= 0) {
                status += " " + std::to_string(data.song_select.indexing_percent) + "%";
            }
            if (data.song_select.indexing_total > 0) {
                status += " (" + format_int_with_commas(data.song_select.indexing_processed) + "/" +
                          format_int_with_commas(data.song_select.indexing_total) + ")";
            } else if (data.song_select.indexing_processed > 0) {
                status += " " + format_int_with_commas(data.song_select.indexing_processed);
            }
            if (!data.song_select.indexing_eta.empty()) {
                status += " ETA " + data.song_select.indexing_eta;
            }
            status += " (" + std::to_string(data.song_select.song_count) + " songs)";
            const std::wstring status_w = to_wide(status);
            const D2D1_RECT_F status_rect = D2D1::RectF(120.0f, 190.0f, 820.0f, 222.0f);
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            draw_text_clipped(status_w, d2d_->hud_format.Get(), status_rect, d2d_->muted_brush.Get());

            const D2D1_RECT_F progress_track = D2D1::RectF(120.0f, 228.0f, 610.0f, 246.0f);
            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.72f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(progress_track, 8.0f, 8.0f), d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(progress_track, 8.0f, 8.0f),
                                          d2d_->button_border_brush.Get(), 1.0f);
            }
            if (d2d_->accent_brush && data.song_select.indexing_percent >= 0) {
                const float fill_ratio =
                    std::clamp(static_cast<float>(data.song_select.indexing_percent) / 100.0f, 0.0f, 1.0f);
                const D2D1_RECT_F progress_fill =
                    D2D1::RectF(progress_track.left + 3.0f,
                                progress_track.top + 3.0f,
                                progress_track.left + 3.0f +
                                    (progress_track.right - progress_track.left - 6.0f) * fill_ratio,
                                progress_track.bottom - 3.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(progress_fill, 6.0f, 6.0f), d2d_->accent_brush.Get());
            }
        }

        const float nav_left = 120.0f;
        const float nav_width = 290.0f;
        const float nav_top = 236.0f;
        const float nav_height = 74.0f;
        const float nav_gap = 14.0f;
        for (std::size_t i = 0; i < data.song_select.left_nav.size(); ++i) {
            const auto& item = data.song_select.left_nav[i];
            const float y0 = nav_top + static_cast<float>(i) * (nav_height + nav_gap);
            const D2D1_RECT_F rect = D2D1::RectF(nav_left, y0, nav_left + nav_width, y0 + nav_height);
            register_hit(rect, MenuHitTargetKind::SongNavButton, static_cast<int>(i));
            if (item.selected && d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.10f + nav_pulse * 0.06f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(offset_rect(rect, 0.0f, 1.0f), 16.0f, 16.0f),
                                          d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            draw_glass_panel(rect,
                             14.0f,
                             item.selected ? 0.84f : 0.62f,
                             item.selected ? (0.54f + nav_pulse * 0.16f) : 0.08f,
                             item.selected,
                             3.5f);
            if (item.selected && d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.84f);
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(rect.left + 8.0f, rect.top + 12.0f, rect.left + 12.0f, rect.bottom - 12.0f),
                                      2.0f, 2.0f),
                    d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }

            const D2D1_RECT_F icon_rect = D2D1::RectF(rect.left + 16.0f, rect.top, rect.left + 78.0f, rect.bottom);
            const bool has_detail = !item.detail.empty();
            const D2D1_RECT_F label_rect = has_detail
                                               ? D2D1::RectF(rect.left + 82.0f, rect.top + 6.0f, rect.right - 18.0f,
                                                             rect.top + 40.0f)
                                               : D2D1::RectF(rect.left + 82.0f, rect.top, rect.right - 18.0f, rect.bottom);
            const D2D1_RECT_F detail_rect =
                D2D1::RectF(rect.left + 82.0f, rect.top + 34.0f, rect.right - 18.0f, rect.bottom - 6.0f);

            if (d2d_->menu_icon_format && d2d_->text_brush) {
                const std::wstring icon_w = to_wide(item.icon);
                if (!icon_w.empty()) {
                    draw_text_clipped(icon_w, d2d_->menu_icon_format.Get(), icon_rect, d2d_->text_brush.Get());
                }
            }
            if (d2d_->title_format && d2d_->text_brush) {
                const std::wstring label_w = to_wide(item.label);
                draw_text_clipped(label_w, d2d_->title_format.Get(), label_rect, d2d_->text_brush.Get());
            }
            if (has_detail && d2d_->body_format) {
                ID2D1SolidColorBrush* detail_brush = item.selected ? d2d_->text_brush.Get() : d2d_->muted_brush.Get();
                if (detail_brush) {
                    const std::wstring detail_w = to_wide(item.detail);
                    draw_text_clipped(detail_w, d2d_->body_format.Get(), detail_rect, detail_brush);
                }
            }
        }

        const D2D1_RECT_F list_rect = D2D1::RectF(450.0f, 220.0f, 1270.0f, 912.0f);
        draw_glass_panel(list_rect, 18.0f, 0.84f, 0.54f + ambient_pulse * 0.14f, true, 8.0f);
        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring list_header_w = data.song_select.showing_sources
                                                   ? L"Sources"
                                                   : (data.song_select.showing_records
                                                          ? L"Records"
                                                          : L"Songs");
            const D2D1_RECT_F list_header_rect =
                D2D1::RectF(list_rect.left + 28.0f, list_rect.top + 18.0f, list_rect.right - 220.0f, list_rect.top + 58.0f);
            draw_text_clipped(list_header_w, d2d_->title_format.Get(), list_header_rect, d2d_->text_brush.Get());
        }
        if (d2d_->body_format && d2d_->muted_brush) {
            const std::wstring list_detail_w = data.song_select.showing_sources
                                                   ? to_wide("RECENT ROOTS")
                                                   : (data.song_select.showing_records
                                                          ? to_wide(std::to_string(data.song_select.record_count) + " PLAYS")
                                                          : to_wide_with_placeholder(data.song_select.current_source_name,
                                                                                    "<invalid source>",
                                                                                    "song-select-header"));
            const D2D1_RECT_F list_detail_rect =
                D2D1::RectF(list_rect.left + 30.0f, list_rect.top + 52.0f, list_rect.right - 220.0f, list_rect.top + 76.0f);
            draw_text_clipped(list_detail_w, d2d_->body_format.Get(), list_detail_rect, d2d_->muted_brush.Get());
        }
        if (d2d_->hud_format && d2d_->text_brush) {
            const std::string count_text =
                data.song_select.showing_sources
                    ? (std::to_string(data.song_select.source_count) + " ROOTS")
                    : (data.song_select.showing_records
                           ? (std::to_string(data.song_select.record_count) + " ENTRIES")
                           : (std::to_string(data.song_select.song_count) + " CHARTS"));
            const D2D1_RECT_F count_rect =
                D2D1::RectF(list_rect.right - 220.0f, list_rect.top + 24.0f, list_rect.right - 24.0f, list_rect.top + 54.0f);
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            draw_text_clipped(to_wide(count_text), d2d_->hud_format.Get(), count_rect, d2d_->text_brush.Get());
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
        draw_section_divider(list_rect.top + 72.0f, list_rect.left + 24.0f, list_rect.right - 24.0f, 0.30f);

        const float card_left = list_rect.left + 28.0f;
        const float card_right = list_rect.right - 28.0f;
        const float card_top = list_rect.top + 70.0f;
        const float card_h = 110.0f;
        const float card_gap = 18.0f;
        constexpr std::size_t kSongCardPreviewBitmapCacheLimit = 24;
        auto ensure_song_card_preview_bitmap = [&](std::string_view path) -> ID2D1Bitmap* {
            if (!d2d_ || !d2d_->d2d_context || !d2d_->wic_factory || path.empty()) {
                return nullptr;
            }

            const std::string key(path);
            auto touch_lru = [&](const std::string& touched_key) {
                auto& lru = d2d_->song_card_preview_lru;
                lru.erase(std::remove(lru.begin(), lru.end(), touched_key), lru.end());
                lru.push_back(touched_key);
            };

            auto it = d2d_->song_card_preview_bitmaps.find(key);
            if (it != d2d_->song_card_preview_bitmaps.end()) {
                touch_lru(key);
                return it->second.bitmap.Get();
            }

            D2DResources::SongCardPreviewBitmapEntry entry;
            entry.attempted = true;
            load_bitmap_from_utf8_path(d2d_->wic_factory.Get(), d2d_->d2d_context.Get(), key, entry.bitmap);
            auto [inserted_it, inserted] =
                d2d_->song_card_preview_bitmaps.emplace(key, std::move(entry));
            (void)inserted;
            touch_lru(key);
            while (d2d_->song_card_preview_lru.size() > kSongCardPreviewBitmapCacheLimit) {
                const std::string evict_key = d2d_->song_card_preview_lru.front();
                d2d_->song_card_preview_lru.pop_front();
                d2d_->song_card_preview_bitmaps.erase(evict_key);
            }
            return inserted_it->second.bitmap.Get();
        };

        for (std::size_t i = 0; i < data.song_select.songs.size(); ++i) {
            const auto& song = data.song_select.songs[i];
            const float y0 = card_top + static_cast<float>(i) * (card_h + card_gap);
            const D2D1_RECT_F card = D2D1::RectF(card_left, y0, card_right, y0 + card_h);
            register_hit(card, MenuHitTargetKind::SongCard, song.song_index);
            if (song.selected && d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.08f + card_pulse * 0.05f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(offset_rect(card, 0.0f, 1.0f), 16.0f, 16.0f),
                                          d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            draw_glass_panel(card,
                             14.0f,
                             song.selected ? 0.86f : 0.66f,
                             song.selected ? (0.56f + card_pulse * 0.16f) : 0.10f,
                             song.selected,
                             4.0f);
            if (song.selected && d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.82f);
                ctx->FillRectangle(D2D1::RectF(card.left + 18.0f, card.bottom - 6.0f, card.right - 18.0f, card.bottom - 4.0f),
                                   d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }

            const D2D1_RECT_F jacket =
                D2D1::RectF(card.left + 18.0f, card.top + 12.0f, card.left + 158.0f, card.bottom - 12.0f);
            const D2D1_ROUNDED_RECT jacket_rr = D2D1::RoundedRect(jacket, 10.0f, 10.0f);
            if (ID2D1Bitmap* jacket_bitmap = ensure_song_card_preview_bitmap(song.background_path)) {
                const D2D1_RECT_F source_rect =
                    centered_bitmap_source_rect(jacket_bitmap->GetSize(), jacket);
                ctx->PushAxisAlignedClip(jacket, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                ctx->DrawBitmap(jacket_bitmap,
                                jacket,
                                song.selected ? 0.98f : 0.92f,
                                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                &source_rect);
                ctx->PopAxisAlignedClip();
                if (d2d_->accent_brush) {
                    const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                    const float saved_opacity = d2d_->accent_brush->GetOpacity();
                    d2d_->accent_brush->SetColor(D2D1::ColorF(0x081018));
                    d2d_->accent_brush->SetOpacity(song.selected ? 0.08f : 0.16f);
                    ctx->FillRoundedRectangle(jacket_rr, d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetColor(D2D1::ColorF(blend_rgb(0xD9E8F5, 0xFFFFFF, 0.35f), 0.20f));
                    d2d_->accent_brush->SetOpacity(0.20f);
                    ctx->FillRoundedRectangle(
                        D2D1::RoundedRect(D2D1::RectF(jacket.left + 8.0f, jacket.top + 8.0f, jacket.right - 8.0f, jacket.top + 26.0f), 8.0f, 8.0f),
                        d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetColor(saved_color);
                    d2d_->accent_brush->SetOpacity(saved_opacity);
                }
            } else {
                if (d2d_->accent_brush) {
                    const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                    const float saved_opacity = d2d_->accent_brush->GetOpacity();
                    const D2D1_COLOR_F color = jacket_color(song.title);
                    d2d_->accent_brush->SetColor(color);
                    d2d_->accent_brush->SetOpacity(0.58f);
                    ctx->FillRoundedRectangle(jacket_rr, d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetColor(D2D1::ColorF(blend_rgb(0xD9E8F5, 0xFFFFFF, 0.35f), 0.20f));
                    d2d_->accent_brush->SetOpacity(0.22f);
                    ctx->FillRoundedRectangle(
                        D2D1::RoundedRect(D2D1::RectF(jacket.left + 8.0f, jacket.top + 8.0f, jacket.right - 8.0f, jacket.top + 26.0f), 8.0f, 8.0f),
                        d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetColor(saved_color);
                    d2d_->accent_brush->SetOpacity(saved_opacity);
                }
                if (d2d_->text_brush) {
                    const float saved_opacity = d2d_->text_brush->GetOpacity();
                    d2d_->text_brush->SetOpacity(0.10f);
                    ctx->DrawLine(D2D1::Point2F(jacket.left + 12.0f, jacket.top + 20.0f),
                                  D2D1::Point2F(jacket.right - 12.0f, jacket.top + 20.0f),
                                  d2d_->text_brush.Get(),
                                  1.0f);
                    ctx->DrawLine(D2D1::Point2F(jacket.left + 12.0f, jacket.bottom - 18.0f),
                                  D2D1::Point2F(jacket.right - 18.0f, jacket.top + 24.0f),
                                  d2d_->text_brush.Get(),
                                  1.2f);
                    d2d_->text_brush->SetOpacity(saved_opacity);
                }
            }
            if (d2d_->button_border_brush) {
                const float saved_opacity = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(song.selected ? 0.75f : 0.48f);
                ctx->DrawRoundedRectangle(jacket_rr, d2d_->button_border_brush.Get(), 1.2f);
                d2d_->button_border_brush->SetOpacity(saved_opacity);
            }

            const D2D1_RECT_F title_rect =
                D2D1::RectF(jacket.right + 18.0f, card.top + 18.0f, card.right - 180.0f, card.top + 62.0f);
            const D2D1_RECT_F artist_rect = data.song_select.showing_sources
                                                ? D2D1::RectF(jacket.right + 18.0f, card.top + 54.0f,
                                                              card.right - 180.0f, card.top + 88.0f)
                                                : D2D1::RectF(jacket.right + 18.0f, card.top + 62.0f,
                                                              card.right - 180.0f, card.bottom - 18.0f);
            const D2D1_RECT_F detail_rect =
                D2D1::RectF(jacket.right + 18.0f, card.top + 82.0f, card.right - 180.0f, card.bottom - 12.0f);

            if (d2d_->song_title_format && d2d_->text_brush) {
                const std::wstring title_w =
                    to_wide_with_placeholder(song.title, "<invalid title>",
                                             "song-card-title:" + std::to_string(song.song_index));
                draw_text_clipped(title_w, d2d_->song_title_format.Get(), title_rect, d2d_->text_brush.Get());
            }
            if (d2d_->song_artist_format && d2d_->muted_brush) {
                const std::wstring artist_w =
                    to_wide_with_placeholder(song.artist, "<invalid artist>",
                                             "song-card-artist:" + std::to_string(song.song_index));
                draw_text_clipped(artist_w, d2d_->song_artist_format.Get(), artist_rect, d2d_->muted_brush.Get());
            }

            if (data.song_select.showing_sources || data.song_select.showing_records) {
                if (!song.detail.empty() && d2d_->body_format) {
                    ID2D1SolidColorBrush* detail_brush = song.selected ? d2d_->text_brush.Get() : d2d_->muted_brush.Get();
                    if (detail_brush) {
                        const std::wstring detail_w =
                            to_wide_with_placeholder(song.detail, "<invalid detail>",
                                                     "song-card-detail:" + std::to_string(song.song_index));
                        draw_text_clipped(detail_w, d2d_->body_format.Get(), detail_rect, detail_brush);
                    }
                }
                if (data.song_select.showing_sources && d2d_->body_format && d2d_->text_brush) {
                    const std::string count_label =
                        (song.level > 0) ? (std::to_string(song.level) + " SONGS") : std::string("OPEN");
                    draw_chip(D2D1::RectF(card.right - 170.0f, card.top + 18.0f, card.right - 18.0f, card.top + 52.0f),
                              count_label,
                              song.selected);
                }
            } else if ((song.level > 0 || song.rating > 0.0) && d2d_->body_format && d2d_->text_brush) {
                std::ostringstream level_stream;
                if (song.level > 0) {
                    level_stream << "LV " << song.level;
                }
                if (song.rating > 0.0) {
                    if (!level_stream.str().empty()) {
                        level_stream << "  ";
                    }
                    level_stream << std::fixed << std::setprecision(2) << "CR " << song.rating;
                }
                const std::string level_text = level_stream.str();
                draw_chip(D2D1::RectF(card.right - 170.0f, card.top + 18.0f, card.right - 18.0f, card.top + 52.0f),
                          level_text,
                          song.selected);
            }
        }

        if (data.song_select.songs.empty() && d2d_->title_format && d2d_->body_format) {
            const D2D1_RECT_F empty_rect =
                D2D1::RectF(list_rect.left + 34.0f, list_rect.top + 146.0f, list_rect.right - 34.0f, list_rect.bottom - 80.0f);
            if (d2d_->text_brush) {
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_text_clipped(
                    to_wide(data.song_select.empty_title.empty() ? std::string("NO ITEMS") : data.song_select.empty_title),
                    d2d_->title_format.Get(),
                    D2D1::RectF(empty_rect.left, empty_rect.top, empty_rect.right, empty_rect.top + 48.0f),
                    d2d_->text_brush.Get());
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            if (d2d_->muted_brush) {
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_text_clipped(
                    to_wide(data.song_select.empty_message.empty() ? std::string("Try another source or filter.")
                                                                   : data.song_select.empty_message),
                    d2d_->body_format.Get(),
                    D2D1::RectF(empty_rect.left + 40.0f, empty_rect.top + 62.0f, empty_rect.right - 40.0f,
                                empty_rect.top + 132.0f),
                    d2d_->muted_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        }

        if (data.song_select.list_total_count > data.song_select.list_visible_count &&
            data.song_select.list_visible_count > 0) {
            const D2D1_RECT_F track_rect =
                D2D1::RectF(list_rect.right - 14.0f, card_top + 4.0f, list_rect.right - 9.0f, list_rect.bottom - 26.0f);
            const float track_height = track_rect.bottom - track_rect.top;
            const float thumb_height = std::max(
                58.0f,
                track_height * (static_cast<float>(data.song_select.list_visible_count) /
                                static_cast<float>(data.song_select.list_total_count)));
            const int scrollable_count =
                std::max(0, data.song_select.list_total_count - data.song_select.list_visible_count);
            const float thumb_travel = std::max(0.0f, track_height - thumb_height);
            const float thumb_top =
                track_rect.top +
                ((scrollable_count > 0)
                     ? (thumb_travel * (static_cast<float>(data.song_select.list_window_start) /
                                        static_cast<float>(scrollable_count)))
                     : 0.0f);
            const D2D1_RECT_F thumb_rect =
                D2D1::RectF(track_rect.left, thumb_top, track_rect.right, thumb_top + thumb_height);

            if (d2d_->button_border_brush) {
                const float saved_opacity = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(0.36f);
                ctx->DrawLine(D2D1::Point2F((track_rect.left + track_rect.right) * 0.5f, track_rect.top),
                              D2D1::Point2F((track_rect.left + track_rect.right) * 0.5f, track_rect.bottom),
                              d2d_->button_border_brush.Get(),
                              1.2f);
                d2d_->button_border_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(song_scroll_drag_active_ ? 0.98f : 0.82f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(thumb_rect, 6.0f, 6.0f), d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(0.22f);
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(thumb_rect, 6.0f, 6.0f), d2d_->accent_brush.Get(), 4.0f);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }

            song_scrollbar_state_.visible = true;
            song_scrollbar_state_.left = track_rect.left - 8.0f;
            song_scrollbar_state_.top = track_rect.top;
            song_scrollbar_state_.right = track_rect.right + 8.0f;
            song_scrollbar_state_.bottom = track_rect.bottom;
            song_scrollbar_state_.thumb_top = thumb_rect.top;
            song_scrollbar_state_.thumb_bottom = thumb_rect.bottom;
            song_scrollbar_state_.total_count = data.song_select.list_total_count;
            song_scrollbar_state_.visible_count = data.song_select.list_visible_count;
            song_scrollbar_state_.window_start = data.song_select.list_window_start;
            song_scrollbar_state_.selected_index = data.song_select.list_selected_index;
        } else {
            clear_song_scrollbar_state();
        }

        if (data.song_select.songs.empty() && d2d_->title_format && d2d_->muted_brush) {
            const std::wstring empty_w = data.song_select.showing_sources
                                             ? L"No song folders loaded yet. Use F2 or drag and drop a folder."
                                             : (data.song_select.showing_records
                                                    ? L"No local records saved for this chart yet."
                                                    : L"No charts matched the current search/filter.");
            const D2D1_RECT_F empty_rect =
                D2D1::RectF(list_rect.left + 40.0f, list_rect.top + 180.0f, list_rect.right - 40.0f, list_rect.top + 260.0f);
            d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            draw_text_clipped(empty_w, d2d_->title_format.Get(), empty_rect, d2d_->muted_brush.Get());
            d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        const D2D1_RECT_F right_rect = D2D1::RectF(1320.0f, 220.0f, 1800.0f, 912.0f);
        draw_glass_panel(right_rect, 18.0f, 0.84f, 0.58f + preview_pulse * 0.16f, true, 8.0f);

        const float stats_left = right_rect.left + 24.0f;
        const float stats_right = right_rect.right - 24.0f;
        float stats_y = right_rect.top + 160.0f;
        float row_h = 30.0f;
        const auto compute_row_height = [&](float top, int row_count, float bottom) {
            if (row_count <= 0) {
                return 30.0f;
            }
            const float available = std::max(0.0f, bottom - top);
            if (available <= 0.0f) {
                return 24.0f;
            }
            return std::clamp(std::floor(available / static_cast<float>(row_count)), 24.0f, 30.0f);
        };

        auto draw_stat_row = [&](std::string_view label, int64_t value) {
            if (!d2d_->stats_label_format || !d2d_->stats_value_format || !d2d_->text_brush) {
                return;
            }
            const std::wstring label_w = to_wide(std::string(label));
            const std::wstring value_w = to_wide(format_int_with_commas(value));
            const D2D1_RECT_F label_rect = D2D1::RectF(stats_left, stats_y, stats_right - 120.0f, stats_y + row_h);
            const D2D1_RECT_F value_rect = D2D1::RectF(stats_right - 120.0f, stats_y, stats_right, stats_y + row_h);
            draw_text_clipped(label_w,
                              d2d_->stats_label_format.Get(),
                              label_rect,
                              d2d_->muted_brush ? d2d_->muted_brush.Get() : d2d_->text_brush.Get());
            draw_text_clipped(value_w, d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
            stats_y += row_h;
        };

        auto draw_stat_text_row = [&](std::string_view label, std::string_view value) {
            if (!d2d_->stats_label_format || !d2d_->stats_value_format || !d2d_->text_brush) {
                return;
            }
            const std::wstring label_w = to_wide(std::string(label));
            const std::wstring value_w = to_wide(std::string(value));
            const D2D1_RECT_F label_rect = D2D1::RectF(stats_left, stats_y, stats_right - 180.0f, stats_y + row_h);
            const D2D1_RECT_F value_rect = D2D1::RectF(stats_right - 180.0f, stats_y, stats_right, stats_y + row_h);
            draw_text_clipped(label_w,
                              d2d_->stats_label_format.Get(),
                              label_rect,
                              d2d_->muted_brush ? d2d_->muted_brush.Get() : d2d_->text_brush.Get());
            draw_text_clipped(value_w, d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
            stats_y += row_h;
        };

        const D2D1_RECT_F right_clip_rect =
            D2D1::RectF(right_rect.left + 8.0f, right_rect.top + 8.0f, right_rect.right - 8.0f, right_rect.bottom - 8.0f);
        ctx->PushAxisAlignedClip(right_clip_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        const D2D1_RECT_F showcase_rect =
            D2D1::RectF(right_rect.left + 20.0f, right_rect.top + 20.0f, right_rect.right - 20.0f, right_rect.top + 236.0f);
        draw_glass_panel(showcase_rect, 18.0f, 0.80f, 0.66f + preview_pulse * 0.12f, true, 6.0f);

        if (data.song_select.showing_sources) {
            row_h = compute_row_height(stats_y, 4, right_rect.bottom - 28.0f);
            if (d2d_->song_title_format && d2d_->text_brush) {
                const std::wstring source_title_w =
                    data.song_select.selected_source_name.empty()
                        ? to_wide("No Source Selected")
                        : to_wide_with_placeholder(data.song_select.selected_source_name,
                                                   "<invalid source>",
                                                   "selected-source-name");
                const D2D1_RECT_F source_title_rect =
                    D2D1::RectF(showcase_rect.left + 18.0f, showcase_rect.top + 44.0f, showcase_rect.right - 18.0f, showcase_rect.top + 94.0f);
                draw_text_clipped(source_title_w, d2d_->song_title_format.Get(), source_title_rect, d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                draw_text_clipped(L"SOURCE STATUS",
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(showcase_rect.left + 18.0f, showcase_rect.top + 12.0f, showcase_rect.right - 18.0f, showcase_rect.top + 34.0f),
                                  d2d_->muted_brush.Get());
                const std::wstring source_path_w =
                    to_wide_with_placeholder(data.song_select.selected_source_path.empty()
                                                 ? data.song_select.current_source_path
                                                 : data.song_select.selected_source_path,
                                             "<invalid path>",
                                             "selected-source-path");
                const D2D1_RECT_F source_path_rect =
                    D2D1::RectF(showcase_rect.left + 18.0f, showcase_rect.top + 98.0f, showcase_rect.right - 18.0f, showcase_rect.top + 154.0f);
                draw_text_clipped(source_path_w, d2d_->body_format.Get(), source_path_rect, d2d_->muted_brush.Get());
            }
            draw_chip(D2D1::RectF(showcase_rect.left + 18.0f, showcase_rect.bottom - 48.0f,
                                  showcase_rect.left + 146.0f, showcase_rect.bottom - 16.0f),
                      data.song_select.selected_source_active ? "ACTIVE" : "READY",
                      data.song_select.selected_source_active);

            stats_y = draw_stat_section(showcase_rect.bottom + 20.0f, "SOURCE DATA", stats_left, stats_right);
            draw_stat_row("ROOTS", data.song_select.source_count);
            draw_stat_row("CURRENT", data.song_select.song_count);
            draw_stat_row("SELECTED",
                          data.song_select.selected_source_song_count >= 0 ? data.song_select.selected_source_song_count : 0);
            if (d2d_->stats_label_format && d2d_->stats_value_format && d2d_->text_brush) {
                const std::wstring label_w = L"STATUS";
                const std::wstring value_w = to_wide(data.song_select.selected_source_active ? "ACTIVE" : "READY");
                const D2D1_RECT_F label_rect = D2D1::RectF(stats_left, stats_y, stats_right - 120.0f, stats_y + row_h);
                const D2D1_RECT_F value_rect = D2D1::RectF(stats_right - 160.0f, stats_y, stats_right, stats_y + row_h);
                draw_text_clipped(label_w, d2d_->stats_label_format.Get(), label_rect,
                                  d2d_->muted_brush ? d2d_->muted_brush.Get() : d2d_->text_brush.Get());
                draw_text_clipped(value_w, d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
            }
        } else if (data.song_select.showing_records) {
            const std::wstring rank_w = to_wide(data.song_select.rank.empty() ? std::string("--") : data.song_select.rank);
            if (d2d_->rank_format && header_brush) {
                const D2D1_RECT_F rank_rect = D2D1::RectF(showcase_rect.left, showcase_rect.top + 4.0f,
                                                         showcase_rect.right, showcase_rect.top + 132.0f);
                draw_text_clipped(rank_w, d2d_->rank_format.Get(), rank_rect, header_brush);
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring status_w = to_wide(data.song_select.selected_record_status);
                const std::wstring time_w = to_wide(data.song_select.selected_record_created_utc);
                const D2D1_RECT_F status_rect =
                    D2D1::RectF(showcase_rect.left + 20.0f, showcase_rect.top + 134.0f, showcase_rect.right - 20.0f, showcase_rect.top + 164.0f);
                const D2D1_RECT_F time_rect =
                    D2D1::RectF(showcase_rect.left + 20.0f, showcase_rect.top + 162.0f, showcase_rect.right - 20.0f, showcase_rect.top + 190.0f);
                draw_text_clipped(status_w, d2d_->body_format.Get(), status_rect, d2d_->muted_brush.Get());
                draw_text_clipped(time_w, d2d_->body_format.Get(), time_rect, d2d_->muted_brush.Get());
            }

            stats_y = draw_stat_section(showcase_rect.bottom + 20.0f, "SESSION", stats_left, stats_right);
            row_h = compute_row_height(stats_y, 8, right_rect.bottom - 160.0f);
            draw_stat_row("SCORE", data.song_select.best_score);
            draw_stat_text_row("ACCURACY", format_decimal(data.song_select.accuracy, 2) + "%");
            draw_stat_row("MAX COMBO", data.song_select.max_combo);
            draw_stat_row("PERFECT", data.song_select.perfect);
            draw_stat_row("GREAT", data.song_select.great);
            draw_stat_row("GOOD", data.song_select.good);
            draw_stat_row("BAD", data.song_select.bad);
            draw_stat_row("MISS", data.song_select.miss);

            stats_y += 8.0f;
            stats_y = draw_stat_section(stats_y, "REPLAY", stats_left, stats_right);
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring replay_file_w =
                    to_wide(data.song_select.selected_record_replay_file.empty()
                                ? std::string("No replay file")
                                : data.song_select.selected_record_replay_file);
                const std::wstring replay_detail_w = to_wide(data.song_select.selected_record_replay_detail);
                const std::wstring replay_meta_w =
                    to_wide("LANES " + std::to_string(data.song_select.selected_record_replay_lane_count) +
                            " / EVENTS " + std::to_string(data.song_select.selected_record_replay_event_count));
                const D2D1_RECT_F replay_file_rect =
                    D2D1::RectF(stats_left, stats_y, stats_right, stats_y + 24.0f);
                const D2D1_RECT_F replay_detail_rect =
                    D2D1::RectF(stats_left, stats_y + 26.0f, stats_right, stats_y + 50.0f);
                const D2D1_RECT_F replay_meta_rect =
                    D2D1::RectF(stats_left, stats_y + 52.0f, stats_right, stats_y + 76.0f);
                draw_text_clipped(replay_file_w, d2d_->body_format.Get(), replay_file_rect, d2d_->muted_brush.Get());
                draw_text_clipped(replay_detail_w, d2d_->body_format.Get(), replay_detail_rect, d2d_->muted_brush.Get());
                draw_text_clipped(replay_meta_w, d2d_->body_format.Get(), replay_meta_rect, d2d_->muted_brush.Get());
            }
        } else {
            const D2D1_RECT_F preview_rect =
                D2D1::RectF(showcase_rect.left + 8.0f, showcase_rect.top + 8.0f, showcase_rect.right - 8.0f, showcase_rect.bottom - 8.0f);
            const D2D1_ROUNDED_RECT preview_rr = D2D1::RoundedRect(preview_rect, 14.0f, 14.0f);
            if (ensure_song_select_preview_bitmap(data.song_select) && d2d_->song_select_preview_bitmap) {
                const D2D1_RECT_F source_rect =
                    centered_bitmap_source_rect(d2d_->song_select_preview_bitmap->GetSize(), preview_rect);
                ctx->DrawBitmap(d2d_->song_select_preview_bitmap.Get(),
                                preview_rect,
                                0.96f,
                                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                &source_rect);
            } else if (d2d_->accent_brush) {
                const D2D1_COLOR_F color = jacket_color(data.song_select.selected_song_title);
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                d2d_->accent_brush->SetColor(color);
                d2d_->accent_brush->SetOpacity(0.80f);
                ctx->FillRoundedRectangle(preview_rr, d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
                d2d_->accent_brush->SetOpacity(0.12f + preview_pulse * 0.06f);
                ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F((preview_rect.left + preview_rect.right) * 0.5f,
                                                             preview_rect.bottom - 18.0f),
                                               180.0f, 54.0f),
                                 d2d_->accent_brush.Get());
                draw_song_select_stardust(preview_rect, 14, 0x901u, 0.10f);
                d2d_->accent_brush->SetOpacity(1.0f);
                d2d_->accent_brush->SetColor(saved_color);
                if (d2d_->title_format && d2d_->text_brush) {
                    const std::wstring empty_preview_w = L"NO BG PREVIEW";
                    d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    draw_text_clipped(empty_preview_w, d2d_->title_format.Get(), preview_rect, d2d_->text_brush.Get());
                    d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                }
            }
            if (d2d_->button_border_brush) {
                const float saved_opacity = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(0.72f);
                ctx->DrawRoundedRectangle(preview_rr, d2d_->button_border_brush.Get(), 1.2f);
                d2d_->button_border_brush->SetOpacity(saved_opacity);
            }

            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring preview_label_w = L"CHART PREVIEW";
                const D2D1_RECT_F preview_label_rect =
                    D2D1::RectF(preview_rect.left + 16.0f, preview_rect.top + 10.0f, preview_rect.right - 16.0f, preview_rect.top + 36.0f);
                draw_text_clipped(preview_label_w, d2d_->body_format.Get(), preview_label_rect, d2d_->muted_brush.Get());
            }

            if (d2d_->song_title_format && d2d_->text_brush) {
                const std::wstring title_w =
                    to_wide_with_placeholder(data.song_select.selected_song_title, "<invalid title>", "selected-song-title");
                const D2D1_RECT_F title_rect =
                    D2D1::RectF(right_rect.left + 24.0f, preview_rect.bottom + 20.0f, right_rect.right - 24.0f, preview_rect.bottom + 70.0f);
                draw_text_clipped(title_w, d2d_->song_title_format.Get(), title_rect, d2d_->text_brush.Get());
            }
            if (d2d_->song_artist_format && d2d_->muted_brush) {
                const std::wstring artist_w =
                    to_wide_with_placeholder(data.song_select.selected_song_artist, "<invalid artist>", "selected-song-artist");
                const D2D1_RECT_F artist_rect =
                    D2D1::RectF(right_rect.left + 24.0f, preview_rect.bottom + 66.0f, right_rect.right - 24.0f, preview_rect.bottom + 100.0f);
                draw_text_clipped(artist_w, d2d_->song_artist_format.Get(), artist_rect, d2d_->muted_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring detail_w = to_wide(data.song_select.selected_song_detail);
                const D2D1_RECT_F detail_rect =
                    D2D1::RectF(right_rect.left + 24.0f, preview_rect.bottom + 96.0f, right_rect.right - 24.0f, preview_rect.bottom + 126.0f);
                draw_text_clipped(detail_w, d2d_->body_format.Get(), detail_rect, d2d_->muted_brush.Get());
            }

            stats_y = draw_stat_section(preview_rect.bottom + 140.0f, "OVERVIEW", stats_left, stats_right);
            row_h = 26.0f;
            draw_stat_text_row("RANK", data.song_select.rank.empty() ? std::string("--") : data.song_select.rank);
            draw_stat_text_row("SORT", data.song_select.sort_summary);
            draw_stat_text_row("FILTER", data.song_select.browser_summary);
            draw_stat_text_row("SOURCE", data.song_select.current_source_name);
            draw_stat_text_row("INDEX", data.song_select.index_profile_label);
            stats_y += 6.0f;
            draw_section_divider(stats_y, stats_left, stats_right, 0.28f);
            stats_y += 12.0f;
            stats_y = draw_stat_section(stats_y, "BEST RECORD", stats_left, stats_right);
            row_h = compute_row_height(stats_y, 6, right_rect.bottom - 24.0f);
            draw_stat_row("BEST", data.song_select.best_score);
            draw_stat_row("MAX COMBO", data.song_select.max_combo);
            draw_stat_row("PERFECT", data.song_select.perfect);
            draw_stat_row("GREAT", data.song_select.great);
            draw_stat_row("GOOD", data.song_select.good);
            draw_stat_row("BAD", data.song_select.bad);
        }

        ctx->PopAxisAlignedClip();

        if (d2d_->hud_format && d2d_->muted_brush) {
            const D2D1_RECT_F primary_rect =
                D2D1::RectF(list_rect.left, right_rect.bottom + 6.0f, right_rect.right, right_rect.bottom + 34.0f);
            const D2D1_RECT_F secondary_rect =
                D2D1::RectF(list_rect.left, right_rect.bottom + 30.0f, right_rect.right, right_rect.bottom + 58.0f);
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            draw_text_clipped(to_wide(data.song_select.primary_hint),
                              d2d_->hud_format.Get(),
                              primary_rect,
                              d2d_->muted_brush.Get());
            draw_text_clipped(to_wide(data.song_select.secondary_hint),
                              d2d_->hud_format.Get(),
                              secondary_rect,
                              d2d_->muted_brush.Get());
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        draw_footer(data.song_select.profile, data.song_select.high_score, data.song_select.track, true);
    };

    auto draw_help_overlay = [&]() {
        if (!data.help_overlay.visible) {
            return;
        }

        if (d2d_->panel_brush) {
            const float saved_opacity = d2d_->panel_brush->GetOpacity();
            d2d_->panel_brush->SetOpacity(0.88f);
            ctx->FillRectangle(D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight), d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(saved_opacity);
        }

        const D2D1_RECT_F panel_rect = D2D1::RectF(260.0f, 132.0f, kBaseWidth - 260.0f, kBaseHeight - 128.0f);
        draw_glass_panel(panel_rect, 24.0f, 0.92f, 0.56f, true, 8.0f);

        if (d2d_->header_format && d2d_->text_brush) {
            const D2D1_RECT_F title_rect =
                D2D1::RectF(panel_rect.left + 34.0f, panel_rect.top + 28.0f, panel_rect.right - 34.0f, panel_rect.top + 92.0f);
            draw_text_clipped(to_wide(data.help_overlay.title), d2d_->header_format.Get(), title_rect, d2d_->text_brush.Get());
        }
        if (d2d_->accent_brush) {
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetOpacity(0.24f);
            ctx->DrawLine(D2D1::Point2F(panel_rect.left + 32.0f, panel_rect.top + 104.0f),
                          D2D1::Point2F(panel_rect.right - 32.0f, panel_rect.top + 104.0f),
                          d2d_->accent_brush.Get(),
                          1.4f);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        float line_y = panel_rect.top + 130.0f;
        for (const auto& line : data.help_overlay.lines) {
            if (line_y > panel_rect.bottom - 120.0f) {
                break;
            }
            if (d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.78f);
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(panel_rect.left + 34.0f, line_y + 10.0f, panel_rect.left + 42.0f, line_y + 18.0f),
                                      3.0f,
                                      3.0f),
                    d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->body_format && d2d_->text_brush) {
                const D2D1_RECT_F line_rect =
                    D2D1::RectF(panel_rect.left + 58.0f, line_y, panel_rect.right - 42.0f, line_y + 44.0f);
                draw_text_clipped(to_wide(line), d2d_->body_format.Get(), line_rect, d2d_->text_brush.Get());
            }
            line_y += 56.0f;
        }

        if (!data.help_overlay.footer.empty() && d2d_->body_format && d2d_->muted_brush) {
            const D2D1_RECT_F footer_rect =
                D2D1::RectF(panel_rect.left + 40.0f, panel_rect.bottom - 88.0f, panel_rect.right - 40.0f, panel_rect.bottom - 34.0f);
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            draw_text_clipped(to_wide(data.help_overlay.footer), d2d_->body_format.Get(), footer_rect, d2d_->muted_brush.Get());
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    };

    auto draw_result_screen = [&]() {
        auto draw_panel = [&](const D2D1_RECT_F& rect, bool accent_border = false) {
            const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 18.0f, 18.0f);
            if (d2d_->panel_brush) {
                ctx->FillRoundedRectangle(rr, d2d_->panel_brush.Get());
            }
            if (accent_border && d2d_->accent_brush) {
                ctx->DrawRoundedRectangle(rr, d2d_->accent_brush.Get(), 1.8f);
            } else if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(rr, d2d_->button_border_brush.Get(), 1.4f);
            }
        };

        const std::wstring header_w = L"RESULT";
        const D2D1_RECT_F header_rect = D2D1::RectF(100.0f, 60.0f, 700.0f, 150.0f);
        ID2D1Brush* header_brush = d2d_->logo_brush ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                                    : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
        if (d2d_->logo_brush) {
            set_brush_points(d2d_->logo_brush.Get(), header_rect);
        }
        if (d2d_->header_format && header_brush) {
            ctx->DrawText(header_w.c_str(), static_cast<UINT32>(header_w.size()),
                          d2d_->header_format.Get(), header_rect, header_brush);
        }

        const D2D1_RECT_F summary_rect = D2D1::RectF(100.0f, 180.0f, 730.0f, 930.0f);
        const D2D1_RECT_F gauge_rect = D2D1::RectF(770.0f, 180.0f, 1820.0f, 500.0f);
        const D2D1_RECT_F breakdown_rect = D2D1::RectF(770.0f, 530.0f, 1285.0f, 930.0f);
        const D2D1_RECT_F detail_rect = D2D1::RectF(1315.0f, 530.0f, 1820.0f, 930.0f);
        draw_panel(summary_rect, true);
        draw_panel(gauge_rect, true);
        draw_panel(breakdown_rect);
        draw_panel(detail_rect);

        const std::wstring title_w = to_wide(data.result.title.empty() ? std::string("Unknown Chart") : data.result.title);
        const std::wstring artist_w = to_wide(data.result.artist.empty() ? std::string("Unknown Artist") : data.result.artist);
        if (d2d_->song_title_format && d2d_->text_brush) {
            const D2D1_RECT_F title_rect =
                D2D1::RectF(summary_rect.left + 28.0f, summary_rect.top + 28.0f, summary_rect.right - 28.0f,
                            summary_rect.top + 90.0f);
            ctx->DrawText(title_w.c_str(), static_cast<UINT32>(title_w.size()),
                          d2d_->song_title_format.Get(), title_rect, d2d_->text_brush.Get());
        }
        if (d2d_->body_format && d2d_->muted_brush) {
            const D2D1_RECT_F artist_rect =
                D2D1::RectF(summary_rect.left + 28.0f, summary_rect.top + 92.0f, summary_rect.right - 28.0f,
                            summary_rect.top + 124.0f);
            ctx->DrawText(artist_w.c_str(), static_cast<UINT32>(artist_w.size()),
                          d2d_->body_format.Get(), artist_rect, d2d_->muted_brush.Get());
        }

        if (d2d_->rank_format && d2d_->accent_brush) {
            const std::wstring rank_w = to_wide(data.result.rank.empty() ? std::string("--") : data.result.rank);
            const D2D1_RECT_F rank_rect =
                D2D1::RectF(summary_rect.left + 20.0f, summary_rect.top + 120.0f, summary_rect.right - 20.0f,
                            summary_rect.top + 280.0f);
            ctx->DrawText(rank_w.c_str(), static_cast<UINT32>(rank_w.size()),
                          d2d_->rank_format.Get(), rank_rect, d2d_->accent_brush.Get());
        }

        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring score_w = to_wide(format_int_with_commas(data.result.score));
            const D2D1_RECT_F score_rect =
                D2D1::RectF(summary_rect.left + 28.0f, summary_rect.top + 290.0f, summary_rect.right - 28.0f,
                            summary_rect.top + 350.0f);
            d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            ctx->DrawText(score_w.c_str(), static_cast<UINT32>(score_w.size()),
                          d2d_->title_format.Get(), score_rect, d2d_->text_brush.Get());
            d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        if (d2d_->hud_format && d2d_->muted_brush) {
            const std::wstring status_w = to_wide(data.result.status + "  /  SCORE");
            const D2D1_RECT_F status_rect =
                D2D1::RectF(summary_rect.left + 28.0f, summary_rect.top + 352.0f, summary_rect.right - 28.0f,
                            summary_rect.top + 388.0f);
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            ctx->DrawText(status_w.c_str(), static_cast<UINT32>(status_w.size()),
                          d2d_->hud_format.Get(), status_rect, d2d_->muted_brush.Get());
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        const float meter_left = summary_rect.left + 42.0f;
        const float meter_right = summary_rect.right - 42.0f;
        auto draw_meter = [&](float top, const std::string& label, double value, double max_value,
                              const D2D1_COLOR_F& color) {
            const float height = 18.0f;
            const D2D1_RECT_F frame = D2D1::RectF(meter_left, top, meter_right, top + height);
            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.60f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(frame, 9.0f, 9.0f), d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(frame, 9.0f, 9.0f), d2d_->button_border_brush.Get(), 1.0f);
            }

            const float ratio = static_cast<float>(std::clamp(max_value > 0.0 ? value / max_value : 0.0, 0.0, 1.0));
            if (d2d_->accent_brush && ratio > 0.0f) {
                const auto saved = d2d_->accent_brush->GetColor();
                d2d_->accent_brush->SetColor(color);
                const D2D1_RECT_F fill =
                    D2D1::RectF(frame.left + 3.0f, frame.top + 3.0f,
                                frame.left + 3.0f + (frame.right - frame.left - 6.0f) * ratio, frame.bottom - 3.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(fill, 7.0f, 7.0f), d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(saved);
            }

            if (d2d_->body_format && d2d_->text_brush) {
                const std::wstring label_w = to_wide(label);
                const std::wstring value_w = to_wide(format_decimal(value) + (max_value <= 1.0 ? std::string("") : "%"));
                const D2D1_RECT_F label_rect = D2D1::RectF(frame.left, frame.top - 34.0f, frame.left + 220.0f, frame.top - 4.0f);
                const D2D1_RECT_F value_rect = D2D1::RectF(frame.right - 180.0f, frame.top - 34.0f, frame.right, frame.top - 4.0f);
                ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                              d2d_->body_format.Get(), label_rect, d2d_->text_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                ctx->DrawText(value_w.c_str(), static_cast<UINT32>(value_w.size()),
                              d2d_->body_format.Get(), value_rect, d2d_->text_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        };

        draw_meter(summary_rect.top + 445.0f, "Accuracy", data.result.accuracy, 100.0, D2D1::ColorF(0x6EE7F2));
        draw_meter(summary_rect.top + 530.0f, "Gauge " + data.result.gauge_label, data.result.gauge_value, 100.0,
                   data.result.status == "GAME OVER" ? D2D1::ColorF(0xFF6B6B) : D2D1::ColorF(0xFABB4B));

        auto draw_panel_row = [&](const D2D1_RECT_F& panel_rect, float y, std::string_view label, std::string_view value) {
            if (!d2d_->stats_label_format || !d2d_->stats_value_format || !d2d_->text_brush) {
                return;
            }
            const std::wstring label_w = to_wide(std::string(label));
            const std::wstring value_w = to_wide(std::string(value));
            const D2D1_RECT_F label_rect =
                D2D1::RectF(panel_rect.left + 42.0f, y, panel_rect.left + 290.0f, y + 32.0f);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(panel_rect.right - 240.0f, y, panel_rect.right - 42.0f, y + 32.0f);
            ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                          d2d_->stats_label_format.Get(), label_rect, d2d_->text_brush.Get());
            ctx->DrawText(value_w.c_str(), static_cast<UINT32>(value_w.size()),
                          d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
        };

        draw_panel_row(summary_rect, summary_rect.top + 640.0f, "Max Combo", std::to_string(data.result.max_combo));
        draw_panel_row(summary_rect, summary_rect.top + 680.0f, "Judged", std::to_string(data.result.judged_notes));
        draw_panel_row(summary_rect, summary_rect.top + 720.0f, "Total Notes", std::to_string(data.result.total_notes));
        draw_panel_row(summary_rect, summary_rect.top + 760.0f, "Gauge Shifts", std::to_string(data.result.shift_count));

        if (d2d_->body_format && d2d_->muted_brush) {
            const std::wstring detail_w =
                to_wide(data.result.replay_available ? std::string("R / Replay or Enter / Esc to return")
                                                     : std::string("ENTER or ESC to return"));
            const D2D1_RECT_F detail_rect_hint =
                D2D1::RectF(summary_rect.left + 28.0f, summary_rect.bottom - 60.0f, summary_rect.right - 28.0f,
                            summary_rect.bottom - 24.0f);
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            ctx->DrawText(detail_w.c_str(), static_cast<UINT32>(detail_w.size()),
                          d2d_->body_format.Get(), detail_rect_hint, d2d_->muted_brush.Get());
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring gauge_title_w = L"Gauge Trace";
            const D2D1_RECT_F gauge_title_rect =
                D2D1::RectF(gauge_rect.left + 28.0f, gauge_rect.top + 20.0f, gauge_rect.right - 28.0f, gauge_rect.top + 60.0f);
            ctx->DrawText(gauge_title_w.c_str(), static_cast<UINT32>(gauge_title_w.size()),
                          d2d_->title_format.Get(), gauge_title_rect, d2d_->text_brush.Get());
        }

        const D2D1_RECT_F plot_rect = D2D1::RectF(gauge_rect.left + 34.0f, gauge_rect.top + 86.0f,
                                                  gauge_rect.right - 34.0f, gauge_rect.bottom - 46.0f);
        if (d2d_->card_brush) {
            d2d_->card_brush->SetOpacity(0.42f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(plot_rect, 14.0f, 14.0f), d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(1.0f);
        }
        if (d2d_->button_border_brush) {
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(plot_rect, 14.0f, 14.0f), d2d_->button_border_brush.Get(), 1.0f);
        }

        const auto gauge_plot_y = [&](float value) {
            const float clamped = std::clamp(value, 0.0f, 1.0f);
            return plot_rect.top + clamped * (plot_rect.bottom - plot_rect.top);
        };

        if (d2d_->muted_brush && d2d_->hud_format) {
            for (int line = 0; line <= 4; ++line) {
                const float t = static_cast<float>(line) / 4.0f;
                const float y = plot_rect.top + t * (plot_rect.bottom - plot_rect.top);
                d2d_->muted_brush->SetOpacity(line == 0 ? 0.50f : 0.22f);
                ctx->DrawLine(D2D1::Point2F(plot_rect.left + 6.0f, y), D2D1::Point2F(plot_rect.right - 6.0f, y),
                              d2d_->muted_brush.Get(), line == 0 ? 1.2f : 0.8f);
                d2d_->muted_brush->SetOpacity(1.0f);
                const std::wstring label_w = to_wide(std::to_string(line * 25) + "%");
                const D2D1_RECT_F label_rect = D2D1::RectF(plot_rect.left + 10.0f, y - 16.0f, plot_rect.left + 80.0f, y + 12.0f);
                ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                              d2d_->hud_format.Get(), label_rect, d2d_->muted_brush.Get());
            }
        }

        if (!data.result.gauge_shifts.empty() && d2d_->accent_brush && d2d_->hud_format) {
            const auto saved = d2d_->accent_brush->GetColor();
            d2d_->accent_brush->SetColor(D2D1::ColorF(0xFAE36E));
            for (const auto& shift : data.result.gauge_shifts) {
                const float x = plot_rect.left + shift.position * (plot_rect.right - plot_rect.left);
                d2d_->accent_brush->SetOpacity(0.50f);
                ctx->DrawLine(D2D1::Point2F(x, plot_rect.top + 8.0f), D2D1::Point2F(x, plot_rect.bottom - 8.0f),
                              d2d_->accent_brush.Get(), 1.2f);
                d2d_->accent_brush->SetOpacity(1.0f);
                const std::wstring shift_w = to_wide(shift.label);
                const D2D1_RECT_F shift_rect = D2D1::RectF(x - 36.0f, plot_rect.top + 8.0f, x + 36.0f, plot_rect.top + 30.0f);
                d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawText(shift_w.c_str(), static_cast<UINT32>(shift_w.size()),
                              d2d_->hud_format.Get(), shift_rect, d2d_->accent_brush.Get());
                d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            d2d_->accent_brush->SetColor(saved);
        }

        if (data.result.gauge_points.size() >= 2 && d2d_->accent_brush) {
            const auto saved = d2d_->accent_brush->GetColor();
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
            for (std::size_t i = 1; i < data.result.gauge_points.size(); ++i) {
                const auto& prev = data.result.gauge_points[i - 1];
                const auto& next = data.result.gauge_points[i];
                const D2D1_POINT_2F p0 = D2D1::Point2F(plot_rect.left + prev.position * (plot_rect.right - plot_rect.left),
                                                       gauge_plot_y(prev.value));
                const D2D1_POINT_2F p1 = D2D1::Point2F(plot_rect.left + next.position * (plot_rect.right - plot_rect.left),
                                                       gauge_plot_y(next.value));
                ctx->DrawLine(p0, p1, d2d_->accent_brush.Get(), 3.2f);
            }
            const auto& tail = data.result.gauge_points.back();
            const D2D1_ELLIPSE tail_dot = D2D1::Ellipse(
                D2D1::Point2F(plot_rect.left + tail.position * (plot_rect.right - plot_rect.left),
                              gauge_plot_y(tail.value)),
                5.5f, 5.5f);
            ctx->FillEllipse(tail_dot, d2d_->accent_brush.Get());
            d2d_->accent_brush->SetColor(saved);
        } else if (d2d_->body_format && d2d_->muted_brush) {
            const std::wstring empty_w = L"No gauge history captured.";
            const D2D1_RECT_F empty_rect = D2D1::RectF(plot_rect.left, plot_rect.top + 80.0f, plot_rect.right, plot_rect.top + 120.0f);
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            ctx->DrawText(empty_w.c_str(), static_cast<UINT32>(empty_w.size()),
                          d2d_->body_format.Get(), empty_rect, d2d_->muted_brush.Get());
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        if (d2d_->body_format && d2d_->text_brush) {
            const std::wstring gauge_summary_w = to_wide(
                data.result.gauge_label + "  " + format_decimal(data.result.gauge_value) + "%");
            const D2D1_RECT_F gauge_summary_rect =
                D2D1::RectF(gauge_rect.right - 280.0f, gauge_rect.top + 22.0f, gauge_rect.right - 24.0f, gauge_rect.top + 58.0f);
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            ctx->DrawText(gauge_summary_w.c_str(), static_cast<UINT32>(gauge_summary_w.size()),
                          d2d_->body_format.Get(), gauge_summary_rect, d2d_->text_brush.Get());
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring mix_title_w = L"Judgement Mix";
            const D2D1_RECT_F mix_title_rect =
                D2D1::RectF(breakdown_rect.left + 24.0f, breakdown_rect.top + 18.0f, breakdown_rect.right - 24.0f,
                            breakdown_rect.top + 56.0f);
            ctx->DrawText(mix_title_w.c_str(), static_cast<UINT32>(mix_title_w.size()),
                          d2d_->title_format.Get(), mix_title_rect, d2d_->text_brush.Get());
        }

        struct JudgeBar {
            const char* label;
            int count;
            D2D1_COLOR_F color;
        };
        const JudgeBar bars[] = {
            {"PG", data.result.perfect, D2D1::ColorF(0x5EE5A7)},
            {"GR", data.result.great, D2D1::ColorF(0x6EE7F2)},
            {"G", data.result.good, D2D1::ColorF(0xFAE36E)},
            {"BAD", data.result.bad, D2D1::ColorF(0xFF9F43)},
        };

        const int max_bar_count = std::max({1, data.result.perfect, data.result.great, data.result.good,
                                            data.result.bad});
        float bar_y = breakdown_rect.top + 82.0f;
        for (const auto& bar : bars) {
            const D2D1_RECT_F label_rect =
                D2D1::RectF(breakdown_rect.left + 26.0f, bar_y, breakdown_rect.left + 86.0f, bar_y + 28.0f);
            const D2D1_RECT_F track_rect =
                D2D1::RectF(breakdown_rect.left + 96.0f, bar_y + 3.0f, breakdown_rect.right - 132.0f, bar_y + 21.0f);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(breakdown_rect.right - 122.0f, bar_y - 2.0f, breakdown_rect.right - 24.0f, bar_y + 26.0f);

            if (d2d_->body_format && d2d_->text_brush) {
                const std::wstring label_w = to_wide(bar.label);
                const std::wstring value_w = to_wide(std::to_string(bar.count));
                ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                              d2d_->body_format.Get(), label_rect, d2d_->text_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                ctx->DrawText(value_w.c_str(), static_cast<UINT32>(value_w.size()),
                              d2d_->body_format.Get(), value_rect, d2d_->text_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.60f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(track_rect, 8.0f, 8.0f), d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush && bar.count > 0) {
                const auto saved = d2d_->accent_brush->GetColor();
                d2d_->accent_brush->SetColor(bar.color);
                const float width = (track_rect.right - track_rect.left) * static_cast<float>(bar.count) /
                                    static_cast<float>(max_bar_count);
                const D2D1_RECT_F fill = D2D1::RectF(track_rect.left, track_rect.top,
                                                     track_rect.left + width, track_rect.bottom);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(fill, 8.0f, 8.0f), d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(saved);
            }
            bar_y += 52.0f;
        }

        draw_panel_row(breakdown_rect, breakdown_rect.top + 360.0f, "Timing Mean", format_signed_ms(data.result.mean_delta_ms));
        draw_panel_row(breakdown_rect, breakdown_rect.top + 398.0f, "Timing StdDev",
                       format_decimal(data.result.stddev_delta_ms) + "ms");
        draw_panel_row(breakdown_rect, breakdown_rect.top + 436.0f, "Warnings",
                       std::to_string(data.result.export_warning_count));

        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring session_w = L"Session Detail";
            const D2D1_RECT_F session_rect =
                D2D1::RectF(detail_rect.left + 24.0f, detail_rect.top + 18.0f, detail_rect.right - 24.0f,
                            detail_rect.top + 56.0f);
            ctx->DrawText(session_w.c_str(), static_cast<UINT32>(session_w.size()),
                          d2d_->title_format.Get(), session_rect, d2d_->text_brush.Get());
        }

        auto draw_detail_line = [&](float y, std::string_view text, bool muted = false) {
            if (!d2d_->body_format) {
                return;
            }
            ID2D1Brush* brush = muted ? static_cast<ID2D1Brush*>(d2d_->muted_brush.Get())
                                      : static_cast<ID2D1Brush*>(d2d_->text_brush.Get());
            if (!brush) {
                return;
            }
            const std::wstring line_w = to_wide(std::string(text));
            const D2D1_RECT_F line_rect =
                D2D1::RectF(detail_rect.left + 24.0f, y, detail_rect.right - 24.0f, y + 28.0f);
            ctx->DrawText(line_w.c_str(), static_cast<UINT32>(line_w.size()),
                          d2d_->body_format.Get(), line_rect, brush);
        };

        draw_detail_line(detail_rect.top + 82.0f, "Profile: " + data.result.profile, true);
        draw_detail_line(detail_rect.top + 118.0f, "Track: " + data.result.track, true);
        if (!data.result.replay_file.empty()) {
            draw_detail_line(detail_rect.top + 166.0f, "Replay");
            draw_detail_line(detail_rect.top + 194.0f, data.result.replay_file, true);
        }
        if (!data.result.result_file.empty()) {
            draw_detail_line(detail_rect.top + 242.0f, "Result File");
            draw_detail_line(detail_rect.top + 270.0f, data.result.result_file, true);
        }

        const bool show_replay_button = !data.result.replay_file.empty();
        const bool replay_available = data.result.replay_available;
        const D2D1_RECT_F back_rect =
            D2D1::RectF(detail_rect.left + 24.0f, detail_rect.bottom - 78.0f, detail_rect.right - 24.0f,
                        detail_rect.bottom - 24.0f);
        const D2D1_RECT_F replay_rect =
            D2D1::RectF(detail_rect.left + 24.0f, back_rect.top - 66.0f, detail_rect.right - 24.0f,
                        back_rect.top - 12.0f);

        float note_y = detail_rect.top + 326.0f;
        const float notes_bottom = (show_replay_button ? replay_rect.top : back_rect.top) - 18.0f;
        if (d2d_->body_format && d2d_->text_brush && note_y + 24.0f < notes_bottom) {
            const std::wstring notes_w = L"Notes";
            const D2D1_RECT_F notes_rect =
                D2D1::RectF(detail_rect.left + 24.0f, note_y, detail_rect.right - 24.0f, note_y + 28.0f);
            ctx->DrawText(notes_w.c_str(), static_cast<UINT32>(notes_w.size()),
                          d2d_->body_format.Get(), notes_rect, d2d_->text_brush.Get());
        }
        note_y += 34.0f;
        if (note_y < notes_bottom && d2d_->body_format) {
            const D2D1_RECT_F notes_clip_rect =
                D2D1::RectF(detail_rect.left + 18.0f, note_y - 4.0f, detail_rect.right - 18.0f, notes_bottom);
            const int max_lines = std::max(
                0,
                static_cast<int>(std::floor((notes_clip_rect.bottom - note_y) / 30.0f)));
            if (max_lines > 0) {
                ctx->PushAxisAlignedClip(notes_clip_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                std::size_t visible_lines = std::min<std::size_t>(data.result.notes.size(), static_cast<std::size_t>(max_lines));
                const bool truncated = data.result.notes.size() > visible_lines;
                if (truncated && visible_lines > 0) {
                    --visible_lines;
                }
                for (std::size_t i = 0; i < visible_lines; ++i) {
                    draw_detail_line(note_y, data.result.notes[i], true);
                    note_y += 30.0f;
                }
                if (truncated) {
                    const std::string remaining = "+" + std::to_string(data.result.notes.size() - visible_lines) + " more";
                    draw_detail_line(note_y, remaining, true);
                }
                ctx->PopAxisAlignedClip();
            }
        }

        if (show_replay_button) {
            if (replay_available) {
                register_hit(replay_rect, MenuHitTargetKind::SettingsRow, 1);
            }
            if (d2d_->button_selected_brush) {
                d2d_->button_selected_brush->SetOpacity(replay_available ? 0.88f : 0.32f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(replay_rect, 14.0f, 14.0f), d2d_->button_selected_brush.Get());
                d2d_->button_selected_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush) {
                d2d_->accent_brush->SetOpacity(replay_available ? 1.0f : 0.35f);
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(replay_rect, 14.0f, 14.0f), d2d_->accent_brush.Get(), 1.5f);
                d2d_->accent_brush->SetOpacity(1.0f);
            }
            if (d2d_->title_format && d2d_->text_brush) {
                const std::wstring replay_w = replay_available ? L"WATCH REPLAY" : L"REPLAY UNAVAILABLE";
                ID2D1Brush* replay_brush =
                    replay_available ? static_cast<ID2D1Brush*>(d2d_->text_brush.Get())
                                     : static_cast<ID2D1Brush*>(d2d_->muted_brush ? d2d_->muted_brush.Get()
                                                                                   : d2d_->text_brush.Get());
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawText(replay_w.c_str(), static_cast<UINT32>(replay_w.size()),
                              d2d_->title_format.Get(), replay_rect, replay_brush);
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        }

        register_hit(back_rect, MenuHitTargetKind::SettingsRow, 0);
        if (d2d_->button_selected_brush) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(back_rect, 14.0f, 14.0f), d2d_->button_selected_brush.Get());
        }
        if (d2d_->accent_brush) {
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(back_rect, 14.0f, 14.0f), d2d_->accent_brush.Get(), 1.5f);
        }
        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring back_w = L"BACK TO SONG SELECT";
            d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            ctx->DrawText(back_w.c_str(), static_cast<UINT32>(back_w.size()),
                          d2d_->title_format.Get(), back_rect, d2d_->text_brush.Get());
            d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    };

    auto draw_gameplay_hud = [&]() {
        const float header_left = 84.0f;
        const float header_top = 42.0f;
        const float header_right = kBaseWidth - 84.0f;
        const double judgement_line_position = clamp_gameplay_judgement_line(data.gameplay.judgement_line_position);
        const double combo_position = clamp_gameplay_combo_position(data.gameplay.combo_position);
        const float note_width_scale = clamp_gameplay_note_width_scale(data.gameplay.note_width_scale);
        const float note_height_scale = clamp_gameplay_note_height_scale(data.gameplay.note_height_scale);
        const float hold_body_width_scale =
            clamp_gameplay_hold_body_width_scale(data.gameplay.hold_body_width_scale);
        const bool note_border_enabled = data.gameplay.note_border_enabled;
        const std::string note_shape = normalize_gameplay_note_shape(data.gameplay.note_shape);

        const GameplayMotionDiagnostics motion_diagnostics = compute_gameplay_motion_diagnostics(
            GameplayMotionState{
                data.gameplay.current_sample,
                data.gameplay.duration_samples,
                data.gameplay.sample_rate,
                data.gameplay.audio_sample_time_ns,
                0,
                data.gameplay.audio_buffer_frames,
                data.gameplay.visual_offset_ms,
                data.gameplay.finished,
                data.gameplay.game_over,
            },
            timing::HighResClock::now_ns());
        const int64_t display_sample = motion_diagnostics.display_sample;

        auto sample_to_y = [&](int64_t sample) -> float {
            return static_cast<float>(compute_gameplay_note_y_normalized(sample,
                                                                         display_sample,
                                                                         data.gameplay.lookahead_samples,
                                                                         data.gameplay.past_samples,
                                                                         judgement_line_position));
        };

        auto draw_gameplay_header = [&]() {
            if (d2d_->title_format && d2d_->text_brush) {
                const D2D1_RECT_F title_rect =
                    D2D1::RectF(header_left, header_top, header_right * 0.60f, header_top + 52.0f);
                ctx->DrawText(gameplay_hud_cache_.title_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.title_text.size()),
                              d2d_->title_format.Get(), title_rect, d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const D2D1_RECT_F artist_rect =
                    D2D1::RectF(header_left, header_top + 46.0f, header_right * 0.60f, header_top + 84.0f);
                ctx->DrawText(gameplay_hud_cache_.artist_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.artist_text.size()),
                              d2d_->body_format.Get(), artist_rect, d2d_->muted_brush.Get());
            }
            if (d2d_->hud_format && d2d_->muted_brush) {
                const D2D1_RECT_F speed_rect =
                    D2D1::RectF(header_left, header_top + 82.0f, header_right * 0.68f, header_top + 118.0f);
                ctx->DrawText(gameplay_hud_cache_.speed_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.speed_text.size()),
                              d2d_->hud_format.Get(), speed_rect, d2d_->muted_brush.Get());
            }
            if (d2d_->title_format && d2d_->text_brush) {
                const D2D1_RECT_F score_rect =
                    D2D1::RectF(kBaseWidth - 700.0f, header_top, kBaseWidth - 90.0f, header_top + 52.0f);
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                ctx->DrawText(gameplay_hud_cache_.score_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.score_text.size()),
                              d2d_->title_format.Get(), score_rect, d2d_->text_brush.Get());
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            if (d2d_->body_format && d2d_->text_brush) {
                const D2D1_RECT_F combo_rect =
                    D2D1::RectF(kBaseWidth - 780.0f, header_top + 52.0f, kBaseWidth - 90.0f, header_top + 84.0f);
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                ctx->DrawText(gameplay_hud_cache_.combo_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.combo_text.size()),
                              d2d_->body_format.Get(), combo_rect, d2d_->text_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            if (d2d_->hud_format && d2d_->muted_brush) {
                const D2D1_RECT_F judge_stats_rect =
                    D2D1::RectF(kBaseWidth - 780.0f, header_top + 82.0f, kBaseWidth - 90.0f, header_top + 116.0f);
                d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                ctx->DrawText(gameplay_hud_cache_.judge_stats_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.judge_stats_text.size()),
                              d2d_->hud_format.Get(), judge_stats_rect, d2d_->muted_brush.Get());
                d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        };

        if (gameplay_hud_cache_.text_revision != data.gameplay.text_revision) {
            const std::string title = data.gameplay.title.empty() ? "Unknown Track" : data.gameplay.title;
            const std::string artist = data.gameplay.artist.empty() ? "Unknown Artist" : data.gameplay.artist;
            gameplay_hud_cache_.title_text = to_wide(title);
            gameplay_hud_cache_.artist_text = to_wide(artist);
            gameplay_hud_cache_.speed_text =
                to_wide("RATE x" + format_decimal(data.gameplay.rate, 2) +
                        " / HS " + format_decimal(data.gameplay.hispeed, 2) +
                        " / BPM " + std::to_string(static_cast<int>(std::llround(data.gameplay.bpm))) +
                        " / Scroll " + std::to_string(static_cast<int>(std::llround(data.gameplay.scroll_speed))));
            gameplay_hud_cache_.score_text =
                to_wide("SCORE  " + format_int_with_commas(data.gameplay.score));
            gameplay_hud_cache_.combo_text =
                to_wide("COMBO " + std::to_string(data.gameplay.combo) +
                        "   MAX " + std::to_string(data.gameplay.max_combo) +
                        "   ACC " + format_decimal(data.gameplay.accuracy, 2) + "%");
            gameplay_hud_cache_.judge_stats_text =
                to_wide("PG " + std::to_string(data.gameplay.pg) +
                        "  GR " + std::to_string(data.gameplay.gr) +
                        "  G " + std::to_string(data.gameplay.gd) +
                        "  BAD " + std::to_string(data.gameplay.bd));
            gameplay_hud_cache_.gauge_label_text = to_wide(data.gameplay.gauge_label);
            gameplay_hud_cache_.gauge_value_text =
                to_wide(std::to_string(static_cast<int>(std::llround(data.gameplay.gauge))) + "%");

            if (data.gameplay.has_feedback) {
                gameplay_hud_cache_.feedback_text = gameplay_feedback_overlay_text(data.gameplay.feedback);
            } else {
                gameplay_hud_cache_.feedback_text.clear();
            }
            gameplay_hud_cache_.text_revision = data.gameplay.text_revision;
        }

        if (data.gameplay.loading && !data.gameplay.active) {
            draw_gameplay_header();
            const D2D1_RECT_F panel_rect = D2D1::RectF(620.0f, 360.0f, 1300.0f, 620.0f);
            const D2D1_ROUNDED_RECT panel_rr = D2D1::RoundedRect(panel_rect, 24.0f, 24.0f);
            if (d2d_->panel_brush) {
                d2d_->panel_brush->SetOpacity(0.92f);
                ctx->FillRoundedRectangle(panel_rr, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(panel_rr, d2d_->button_border_brush.Get(), 1.6f);
            }

            const std::wstring loading_w = L"LOADING CHART";
            const std::wstring stage_w =
                to_wide(data.gameplay.loading_stage.empty() ? std::string("Preparing gameplay")
                                                            : data.gameplay.loading_stage);
            const std::wstring percent_w =
                to_wide(std::to_string(std::clamp(data.gameplay.loading_percent, 0, 100)) + "%");
            if (d2d_->title_format && d2d_->text_brush) {
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawText(loading_w.c_str(), static_cast<UINT32>(loading_w.size()),
                              d2d_->title_format.Get(),
                              D2D1::RectF(panel_rect.left + 32.0f, panel_rect.top + 24.0f,
                                          panel_rect.right - 32.0f, panel_rect.top + 72.0f),
                              d2d_->text_brush.Get());
                ctx->DrawText(percent_w.c_str(), static_cast<UINT32>(percent_w.size()),
                              d2d_->title_format.Get(),
                              D2D1::RectF(panel_rect.left + 32.0f, panel_rect.top + 74.0f,
                                          panel_rect.right - 32.0f, panel_rect.top + 124.0f),
                              d2d_->text_brush.Get());
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawText(stage_w.c_str(), static_cast<UINT32>(stage_w.size()),
                              d2d_->body_format.Get(),
                              D2D1::RectF(panel_rect.left + 32.0f, panel_rect.top + 134.0f,
                                          panel_rect.right - 32.0f, panel_rect.top + 174.0f),
                              d2d_->muted_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }

            const D2D1_RECT_F progress_track =
                D2D1::RectF(panel_rect.left + 74.0f, panel_rect.bottom - 82.0f,
                            panel_rect.right - 74.0f, panel_rect.bottom - 54.0f);
            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.78f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(progress_track, 10.0f, 10.0f), d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush) {
                const float fill_ratio =
                    std::clamp(static_cast<float>(data.gameplay.loading_percent) / 100.0f, 0.0f, 1.0f);
                const D2D1_RECT_F progress_fill =
                    D2D1::RectF(progress_track.left + 4.0f,
                                progress_track.top + 4.0f,
                                progress_track.left + 4.0f +
                                    (progress_track.right - progress_track.left - 8.0f) * fill_ratio,
                                progress_track.bottom - 4.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(progress_fill, 8.0f, 8.0f), d2d_->accent_brush.Get());
            }
            return;
        }

        if (data.gameplay.countdown_active) {
            if (d2d_->gameplay_static_command_list) {
                ctx->DrawImage(d2d_->gameplay_static_command_list.Get());
            }
            draw_gameplay_header();

            const D2D1_RECT_F panel_rect = D2D1::RectF(700.0f, 300.0f, 1220.0f, 760.0f);
            const D2D1_ROUNDED_RECT panel_rr = D2D1::RoundedRect(panel_rect, 28.0f, 28.0f);
            if (d2d_->panel_brush) {
                d2d_->panel_brush->SetOpacity(0.90f);
                ctx->FillRoundedRectangle(panel_rr, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush) {
                ctx->DrawRoundedRectangle(panel_rr, d2d_->accent_brush.Get(), 1.8f);
            }

            if (d2d_->body_format && d2d_->muted_brush) {
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                const std::wstring ready_w = L"GET READY";
                ctx->DrawText(ready_w.c_str(), static_cast<UINT32>(ready_w.size()),
                              d2d_->body_format.Get(),
                              D2D1::RectF(panel_rect.left + 24.0f, panel_rect.top + 30.0f,
                                          panel_rect.right - 24.0f, panel_rect.top + 76.0f),
                              d2d_->muted_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }

            ID2D1Brush* countdown_brush = d2d_->logo_brush
                                              ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                              : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
            if (d2d_->logo_brush) {
                set_brush_points(d2d_->logo_brush.Get(), panel_rect);
            }
            if (d2d_->rank_format && countdown_brush) {
                const std::wstring countdown_w =
                    to_wide(std::to_string(std::max(1, data.gameplay.countdown_value)));
                ctx->DrawText(countdown_w.c_str(), static_cast<UINT32>(countdown_w.size()),
                              d2d_->rank_format.Get(),
                              D2D1::RectF(panel_rect.left + 24.0f, panel_rect.top + 86.0f,
                                          panel_rect.right - 24.0f, panel_rect.top + 270.0f),
                              countdown_brush);
            }

            if (d2d_->title_format && d2d_->text_brush) {
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                const std::wstring start_w = L"START";
                ctx->DrawText(start_w.c_str(), static_cast<UINT32>(start_w.size()),
                              d2d_->title_format.Get(),
                              D2D1::RectF(panel_rect.left + 24.0f, panel_rect.bottom - 132.0f,
                                          panel_rect.right - 24.0f, panel_rect.bottom - 64.0f),
                              d2d_->text_brush.Get());
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }

            return;
        }

        const int lane_count = std::clamp(data.gameplay.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
        const GameplayFieldLayout field_layout = build_gameplay_field_layout(
            kGameplayFieldLeft,
            kGameplayFieldRight,
            kGameplayFieldTop,
            kGameplayFieldBottom,
            lane_count,
            note_width_scale);
        const float field_left = field_layout.left;
        const float field_right = field_layout.right;
        const float field_top = field_layout.top;
        const float field_bottom = field_layout.bottom;
        const float field_height = field_layout.height;
        const float lane_width = field_layout.lane_width;
        const float note_width = field_layout.note_width;
        const D2D1_RECT_F field_clip_rect =
            D2D1::RectF(field_left + 2.0f, field_top + 2.0f, field_right - 2.0f, field_bottom - 2.0f);
        const float hit_line_y = gameplay_field_y(field_top, field_height, judgement_line_position);
        if (d2d_->gameplay_static_command_list) {
            ctx->DrawImage(d2d_->gameplay_static_command_list.Get());
        }

        const D2D1_ANTIALIAS_MODE saved_antialias = ctx->GetAntialiasMode();
        ctx->PushAxisAlignedClip(field_clip_rect, D2D1_ANTIALIAS_MODE_ALIASED);
        ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        for (int lane = 0; lane < lane_count; ++lane) {
            const std::size_t lane_index = static_cast<std::size_t>(lane);
            ID2D1Bitmap* key_bitmap = d2d_->lane_key_idle_bitmaps[lane_index].Get();
            if (lane_index < data.gameplay.lane_activity_count &&
                data.gameplay.lane_activity[lane_index] > 0.05f &&
                d2d_->lane_key_pressed_bitmaps[lane_index]) {
                key_bitmap = d2d_->lane_key_pressed_bitmaps[lane_index].Get();
            }
            if (!key_bitmap) {
                continue;
            }
            const D2D1_SIZE_F bitmap_size = key_bitmap->GetSize();
            const float lane_center = field_left + (static_cast<float>(lane) + 0.5f) * lane_width;
            const D2D1_RECT_F receptor_rect =
                gameplay_key_bitmap_rect(field_layout,
                                         hit_line_y,
                                         note_height_scale,
                                         lane_center,
                                         lane_width,
                                         note_width,
                                         bitmap_size,
                                         gameplay_note_sprite_cache_.using_osu_skin_assets);
            const D2D1_RECT_F* key_source_rect = nullptr;
            if (lane_index < d2d_->lane_key_pressed_source_rects.size() &&
                key_bitmap == d2d_->lane_key_pressed_bitmaps[lane_index].Get()) {
                key_source_rect = bitmap_source_rect_or_null(d2d_->lane_key_pressed_source_rects[lane_index]);
            } else if (lane_index < d2d_->lane_key_idle_source_rects.size()) {
                key_source_rect = bitmap_source_rect_or_null(d2d_->lane_key_idle_source_rects[lane_index]);
            }
            ctx->DrawBitmap(key_bitmap, receptor_rect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, key_source_rect);
        }
        for (std::size_t note_index = 0; note_index < data.gameplay.note_count; ++note_index) {
            const auto& note = data.gameplay.notes[note_index];
            if (!should_render_gameplay_note(note.start_sample, note.head_visible, display_sample)) {
                continue;
            }
            const int lane = std::clamp(note.lane, 1, lane_count);
            const float lane_center = field_left + (static_cast<float>(lane) - 0.5f) * lane_width;
            const float x0 = lane_center - note_width * 0.5f;
            const float x1 = lane_center + note_width * 0.5f;
            const int64_t render_sample =
                gameplay_note_render_sample(note.start_sample, note.hold, note.head_visible, display_sample);
            const float y = gameplay_field_y(field_top, field_height, sample_to_y(render_sample));
            const float tail_y = gameplay_field_y(field_top, field_height, sample_to_y(note.tail_sample));
            const float head_half_h = gameplay_note_head_half_height(note_height_scale);
            const float tail_half_h = gameplay_note_tail_half_height(note_height_scale);
            uint32_t lane_color = 0xF6F8FF;
            if (static_cast<std::size_t>(lane - 1) < data.gameplay.lane_color_count) {
                lane_color = data.gameplay.lane_colors[static_cast<std::size_t>(lane - 1)];
            } else if (!gameplay_lane_uses_white_note(lane)) {
                lane_color = 0x4F80FF;
            }
            ID2D1SolidColorBrush* note_fill = d2d_->note_fill_brush.Get();
            ID2D1SolidColorBrush* note_border = d2d_->note_border_brush.Get();
            ID2D1SolidColorBrush* note_hold_fill = d2d_->note_hold_brush.Get();
            if (note_fill) {
                note_fill->SetColor(gameplay_note_fill_color(lane_color));
            }
            if (note_border) {
                note_border->SetColor(gameplay_note_border_color(lane_color));
            }
            if (note_hold_fill) {
                note_hold_fill->SetColor(gameplay_note_hold_color(lane_color));
            }
            ID2D1Bitmap* note_head_bitmap = d2d_->lane_note_head_bitmaps[static_cast<std::size_t>(lane - 1)].Get();
            ID2D1Bitmap* note_hold_head_bitmap =
                d2d_->lane_note_hold_head_bitmaps[static_cast<std::size_t>(lane - 1)].Get();
            ID2D1Bitmap* note_hold_body_bitmap =
                d2d_->lane_note_hold_body_bitmaps[static_cast<std::size_t>(lane - 1)].Get();

            if (note.hold && note_hold_fill) {
                const float head_body_inset = gameplay_hold_body_cap_inset(note_shape, head_half_h);
                const float body_top = std::min(y, tail_y);
                const float body_bottom = std::max(y, tail_y) - (note.head_visible ? head_body_inset : 0.0f);
                const float hold_half_width = std::max(4.0f, note_width * 0.5f * hold_body_width_scale);
                const D2D1_RECT_F hold_body =
                    D2D1::RectF(lane_center - hold_half_width, body_top, lane_center + hold_half_width, body_bottom);
                if (body_bottom > body_top) {
                    if (note_hold_body_bitmap) {
                        const D2D1_RECT_F* hold_body_source_rect =
                            bitmap_source_rect_or_null(
                                d2d_->lane_note_hold_body_source_rects[static_cast<std::size_t>(lane - 1)]);
                        ctx->DrawBitmap(note_hold_body_bitmap, hold_body, 1.0f,
                                        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                        hold_body_source_rect);
                    } else {
                        ctx->FillRectangle(hold_body, note_hold_fill);
                    }
                }
            }

            const D2D1_RECT_F note_rect = D2D1::RectF(x0, y - head_half_h, x1, y + head_half_h);
            if (note.head_visible) {
                ID2D1Bitmap* head_bitmap = note.hold && note_hold_head_bitmap ? note_hold_head_bitmap : note_head_bitmap;
                if (head_bitmap) {
                    const D2D1_RECT_F* head_source_rect =
                        note.hold
                            ? bitmap_source_rect_or_null(
                                  d2d_->lane_note_hold_head_source_rects[static_cast<std::size_t>(lane - 1)])
                            : bitmap_source_rect_or_null(
                                  d2d_->lane_note_head_source_rects[static_cast<std::size_t>(lane - 1)]);
                    const D2D1_RECT_F bitmap_rect =
                        gameplay_note_bitmap_dest_rect(note_rect,
                                                       head_bitmap,
                                                       head_source_rect,
                                                       note_shape,
                                                       data.gameplay.preserve_note_image_aspect_ratio);
                    ctx->DrawBitmap(head_bitmap, bitmap_rect, 1.0f,
                                    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, head_source_rect);
                } else {
                    if (note_fill) {
                        draw_note_primitive(ctx, note_rect, note_fill, note_border, 1.3f,
                                            note_shape, note_border_enabled);
                    }
                }
            }
        }
        ctx->PopAxisAlignedClip();
        ctx->SetAntialiasMode(saved_antialias);

        const float combo_y = gameplay_field_y(field_top, field_height, combo_position);
        const bool show_feedback_overlay =
            data.gameplay.has_feedback && !gameplay_hud_cache_.feedback_text.empty();
        if (show_feedback_overlay && d2d_->header_format && d2d_->text_brush) {
            const float feedback_center_x = (field_left + field_right) * 0.5f;
            const D2D1_RECT_F feedback_rect =
                D2D1::RectF(field_left - 24.0f, combo_y - 74.0f, field_right + 24.0f, combo_y + 6.0f);
            const bool show_timing_indicator =
                data.gameplay.feedback != "PG" &&
                std::abs(data.gameplay.feedback_delta_ms) >= 0.05;
            const bool timing_fast = show_timing_indicator && data.gameplay.feedback_delta_ms < 0.0;
            const bool timing_slow = show_timing_indicator && data.gameplay.feedback_delta_ms > 0.0;

            d2d_->text_brush->SetColor(D2D1::ColorF(0x061118, 0.78f));
            d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            const D2D1_RECT_F feedback_shadow_rect =
                D2D1::RectF(feedback_rect.left + 3.0f, feedback_rect.top + 3.0f,
                            feedback_rect.right + 3.0f, feedback_rect.bottom + 3.0f);
            ctx->DrawText(gameplay_hud_cache_.feedback_text.c_str(),
                          static_cast<UINT32>(gameplay_hud_cache_.feedback_text.size()),
                          d2d_->header_format.Get(), feedback_shadow_rect, d2d_->text_brush.Get());

            d2d_->text_brush->SetColor(gameplay_feedback_color(data.gameplay.feedback));
            ctx->DrawText(gameplay_hud_cache_.feedback_text.c_str(),
                          static_cast<UINT32>(gameplay_hud_cache_.feedback_text.size()),
                          d2d_->header_format.Get(), feedback_rect, d2d_->text_brush.Get());

            if (show_timing_indicator && d2d_->body_format) {
                constexpr wchar_t kFastIndicatorText[] = L"\uBE60\uB984";
                constexpr wchar_t kSlowIndicatorText[] = L"\uB290\uB9BC";
                const D2D1_RECT_F fast_rect =
                    D2D1::RectF(field_left + 60.0f, combo_y + 8.0f, feedback_center_x - 220.0f, combo_y + 38.0f);
                const D2D1_RECT_F slow_rect =
                    D2D1::RectF(feedback_center_x + 220.0f, combo_y + 8.0f, field_right - 60.0f, combo_y + 38.0f);

                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                d2d_->text_brush->SetColor(D2D1::ColorF(0x5DA9FF, timing_fast ? 0.82f : 0.28f));
                ctx->DrawText(kFastIndicatorText, 2, d2d_->body_format.Get(), fast_rect, d2d_->text_brush.Get());

                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                d2d_->text_brush->SetColor(D2D1::ColorF(0xFF5A6B, timing_slow ? 0.82f : 0.28f));
                ctx->DrawText(kSlowIndicatorText, 2, d2d_->body_format.Get(), slow_rect, d2d_->text_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            d2d_->text_brush->SetColor(D2D1::ColorF(0xE8ECF1));
        }

        if (data.gameplay.combo > 0 && d2d_->accent_brush &&
            (show_feedback_overlay ? (d2d_->title_format.Get() != nullptr)
                                   : (d2d_->header_format.Get() != nullptr))) {
            const std::wstring combo_overlay_w = to_wide(std::to_string(data.gameplay.combo));
            if (show_feedback_overlay) {
                const D2D1_RECT_F combo_overlay_rect =
                    D2D1::RectF(field_left, combo_y + 38.0f, field_right, combo_y + 82.0f);
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->accent_brush->SetOpacity(0.92f);
                ctx->DrawText(combo_overlay_w.c_str(), static_cast<UINT32>(combo_overlay_w.size()),
                              d2d_->title_format.Get(), combo_overlay_rect, d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(1.0f);
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            } else {
                const D2D1_RECT_F combo_overlay_rect =
                    D2D1::RectF(field_left, combo_y - 44.0f, field_right, combo_y + 44.0f);
                ctx->DrawText(combo_overlay_w.c_str(), static_cast<UINT32>(combo_overlay_w.size()),
                              d2d_->header_format.Get(), combo_overlay_rect, d2d_->accent_brush.Get());
            }
        }

        draw_gameplay_header();

        if (d2d_->accent_brush && data.gameplay.lane_activity_count > 0) {
            const std::size_t count =
                std::min(data.gameplay.lane_activity_count, static_cast<std::size_t>(lane_count));
            for (std::size_t lane = 0; lane < count; ++lane) {
                const float activity = std::clamp(data.gameplay.lane_activity[lane], 0.0f, 1.0f);
                if (activity <= 0.01f) {
                    continue;
                }
                const float lane_center = field_left + (static_cast<float>(lane) + 0.5f) * lane_width;
                const float x0 = lane_center - note_width * 0.5f;
                const float x1 = lane_center + note_width * 0.5f;
                const float glow_half_h = 8.0f + 14.0f * activity;
                const D2D1_RECT_F glow_rect =
                    D2D1::RectF(x0,
                                std::max(field_top + 2.0f, hit_line_y - glow_half_h),
                                x1,
                                std::min(field_bottom - 2.0f, hit_line_y + glow_half_h));
                d2d_->accent_brush->SetOpacity(0.12f + 0.38f * activity);
                ctx->FillRectangle(glow_rect, d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(1.0f);
            }
        }

        const float gauge_left = kGameplayGaugeLeft;
        const float gauge_top = kGameplayGaugeTop;
        const float gauge_bottom = kGameplayGaugeBottom;
        const float gauge_width = kGameplayGaugeWidth;

        const float gauge_ratio = static_cast<float>(std::clamp(data.gameplay.gauge / 100.0, 0.0, 1.0));
        const float fill_top = gauge_bottom - (gauge_bottom - gauge_top) * gauge_ratio;
        if (d2d_->accent_brush) {
            D2D1_COLOR_F gauge_color = D2D1::ColorF(0xFFB703, 0.90f);
            if (data.gameplay.gauge_label == "HARD") {
                gauge_color = D2D1::ColorF(0xFF4D6D, 0.92f);
            } else if (data.gameplay.gauge_label == "EASY") {
                gauge_color = D2D1::ColorF(0x89D185, 0.92f);
            }
            d2d_->accent_brush->SetColor(gauge_color);
            const D2D1_RECT_F fill =
                D2D1::RectF(gauge_left + 4.0f, fill_top + 4.0f, gauge_left + gauge_width - 4.0f, gauge_bottom - 4.0f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(fill, 8.0f, 8.0f), d2d_->accent_brush.Get());
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
        }

        if (d2d_->body_format && d2d_->text_brush) {
            const D2D1_RECT_F label_rect =
                D2D1::RectF(gauge_left - 90.0f, gauge_top - 38.0f, gauge_left + 140.0f, gauge_top - 8.0f);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(gauge_left - 90.0f, gauge_bottom + 10.0f, gauge_left + 140.0f, gauge_bottom + 42.0f);
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            ctx->DrawText(gameplay_hud_cache_.gauge_label_text.c_str(),
                          static_cast<UINT32>(gameplay_hud_cache_.gauge_label_text.size()),
                          d2d_->body_format.Get(), label_rect, d2d_->text_brush.Get());
            ctx->DrawText(gameplay_hud_cache_.gauge_value_text.c_str(),
                          static_cast<UINT32>(gameplay_hud_cache_.gauge_value_text.size()),
                          d2d_->body_format.Get(), value_rect, d2d_->text_brush.Get());
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        if (data.gameplay.game_over && d2d_->panel_brush) {
            const D2D1_RECT_F overlay = D2D1::RectF(field_left - 10.0f, field_top - 10.0f,
                                                    field_right + 10.0f, field_bottom + 10.0f);
            d2d_->panel_brush->SetOpacity(0.78f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(overlay, 18.0f, 18.0f), d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(1.0f);
            if (d2d_->header_format && d2d_->accent_brush) {
                const std::wstring over_w = L"GAME OVER";
                const D2D1_RECT_F over_rect =
                    D2D1::RectF(field_left, (field_top + field_bottom) * 0.5f - 50.0f, field_right,
                                (field_top + field_bottom) * 0.5f + 50.0f);
                d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawText(over_w.c_str(), static_cast<UINT32>(over_w.size()),
                              d2d_->header_format.Get(), over_rect, d2d_->accent_brush.Get());
                d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        }
    };

    switch (data.kind) {
        case MenuScreenKind::TitleMenu:
            draw_title_menu();
            break;
        case MenuScreenKind::SongSelect:
            draw_song_select();
            break;
        case MenuScreenKind::ResultScreen:
            draw_result_screen();
            break;
        case MenuScreenKind::GameplayHud:
            draw_gameplay_hud();
            break;
        case MenuScreenKind::GenericList:
        default:
            draw_generic_list();
            break;
    }

    if (!data.help_overlay.visible) {
        draw_performance_overlay();
    }
    draw_help_overlay();

    const HRESULT hr = ctx->EndDraw();
    if (FAILED(hr)) {
        std::cerr << "[MenuWindow::draw] EndDraw failed hr=0x" << std::hex
                  << static_cast<unsigned long>(hr) << std::dec << std::endl;
    }
    if (hr == D2DERR_RECREATE_TARGET || hr == D2DERR_WRONG_STATE) {
        if (!recreate_targets()) {
            fail_fatal("Failed to recreate render target after device state change.");
            shutdown();
            return;
        }
    }

    UINT present_flags = 0;
    if (app::should_allow_tearing_present(
            config_.vsync,
            fullscreen_,
            (swap_chain_flags_ & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0)) {
        present_flags = DXGI_PRESENT_ALLOW_TEARING;
    }
    const HRESULT present_hr = d2d_->swap_chain->Present(config_.vsync ? 1 : 0, present_flags);
    if (present_hr == DXGI_ERROR_DEVICE_REMOVED || present_hr == DXGI_ERROR_DEVICE_RESET) {
        std::cerr << "[MenuWindow::draw] Present failed: device removed/reset hr=0x" << std::hex
                  << static_cast<unsigned long>(present_hr) << std::dec << std::endl;
        fail_fatal("Graphics device was removed/reset. Update GPU drivers and attach logs/run.log.");
        shutdown();
        return;
    }
    if (FAILED(present_hr)) {
        const HWND hwnd = static_cast<HWND>(hwnd_);
        const bool window_minimized = hwnd && IsIconic(hwnd);
        const bool window_in_foreground = is_input_foreground();
        if (app::should_treat_present_failure_as_transient(
                static_cast<std::uint32_t>(present_hr),
                config_.display_mode == "fullscreen",
                window_in_foreground,
                window_minimized)) {
            if (config_.display_mode == "fullscreen") {
                fullscreen_ = false;
                fullscreen_restore_pending_ = true;
            }
            std::cerr << "[MenuWindow::draw] Present returned transient hr=0x" << std::hex
                      << static_cast<unsigned long>(present_hr) << std::dec << std::endl;
            return;
        }
        std::cerr << "[MenuWindow::draw] Present failed hr=0x" << std::hex
                  << static_cast<unsigned long>(present_hr) << std::dec << std::endl;
        fail_fatal("Failed to present the menu frame. Attach logs/run.log.");
        shutdown();
        return;
    }
}

void MenuWindow::apply_pending_config() {
    MenuWindowConfig pending;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (!config_dirty_) {
            return;
        }
        pending = pending_config_;
        config_dirty_ = false;
    }

    if (!initialized_.load(std::memory_order_acquire)) {
        std::cerr << "[MenuWindow::apply_pending_config] initializing window..." << std::endl;
        init_done_.store(false, std::memory_order_release);
        init_success_.store(false, std::memory_order_release);
        const bool ok = initialize(pending);
        init_success_.store(ok, std::memory_order_release);
        init_done_.store(true, std::memory_order_release);
        if (!ok) {
            std::cerr << "[MenuWindow::apply_pending_config] initialize FAILED" << std::endl;
            should_close_.store(true, std::memory_order_release);
            shutdown();
        } else {
            std::cerr << "[MenuWindow::apply_pending_config] initialize OK, initialized_="
                      << initialized_.load(std::memory_order_acquire) << std::endl;
        }
        return;
    }

    const MenuWindowConfig previous = config_;
    config_ = pending;

    if (hwnd_ && previous.title != config_.title) {
        const std::wstring title = to_wide(config_.title);
        SetWindowTextW(static_cast<HWND>(hwnd_), title.c_str());
    }

    const bool display_mode_changed = previous.display_mode != config_.display_mode;
    const bool refresh_changed = previous.refresh_hz != config_.refresh_hz;
    const bool resolution_changed = previous.width != config_.width || previous.height != config_.height;

    if (d2d_ && d2d_->swap_chain && (display_mode_changed || refresh_changed || resolution_changed)) {
        const HWND hwnd = static_cast<HWND>(hwnd_);
        const MonitorDisplayInfo monitor = query_monitor_display_info(hwnd);
        UINT next_width = width_;
        UINT next_height = height_;
        int next_x = monitor.rect.left;
        int next_y = monitor.rect.top;
        resolve_window_bounds(config_, monitor, next_width, next_height, next_x, next_y);
        const DWORD next_style = window_style_for_display_mode(config_.display_mode);
        const DWORD next_ex_style = window_ex_style_for_display_mode(config_.display_mode);
        const SIZE next_window_size = window_size_for_client_area(next_width, next_height, next_style, next_ex_style);

        const bool need_fullscreen_reset = fullscreen_ && (resolution_changed || display_mode_changed);
        if (need_fullscreen_reset || (fullscreen_ && config_.display_mode != "fullscreen")) {
            d2d_->swap_chain->SetFullscreenState(FALSE, nullptr);
            fullscreen_ = false;
            fullscreen_restore_pending_ = false;
        }

        width_ = next_width;
        height_ = next_height;
        if (hwnd) {
            if (display_mode_changed) {
                SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(next_style));
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, static_cast<LONG_PTR>(next_ex_style));
            }
            if (previous.display_mode == "windowed" && config_.display_mode == "windowed") {
                RECT current_rect{};
                if (GetWindowRect(hwnd, &current_rect)) {
                    next_x = current_rect.left;
                    next_y = current_rect.top;
                }
            }
            SetWindowPos(hwnd,
                         nullptr,
                         next_x,
                         next_y,
                         next_window_size.cx,
                         next_window_size.cy,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        }

        if (resolution_changed || display_mode_changed) {
            resize_swap_chain(next_width, next_height);
        } else {
            update_layout();
        }

        const bool want_fullscreen = (config_.display_mode == "fullscreen");
        if (want_fullscreen != fullscreen_) {
            const HRESULT fs_hr = d2d_->swap_chain->SetFullscreenState(want_fullscreen ? TRUE : FALSE, nullptr);
            if (FAILED(fs_hr)) {
                std::cerr << "[MenuWindow::apply_pending_config] SetFullscreenState("
                          << (want_fullscreen ? "TRUE" : "FALSE") << ") failed hr=0x"
                          << std::hex << static_cast<unsigned long>(fs_hr) << std::dec << std::endl;
                config_.display_mode = previous.display_mode;
                fullscreen_restore_pending_ = want_fullscreen;
            } else {
                fullscreen_ = want_fullscreen;
                fullscreen_restore_pending_ = false;
            }
        }
        if (fullscreen_) {
            apply_fullscreen_target(d2d_->swap_chain.Get(), config_, width_, height_);
        }
    }
}

void MenuWindow::update_layout() {
    const float width = static_cast<float>(width_);
    const float height = static_cast<float>(height_);
    const float scale_x = width / kBaseWidth;
    const float scale_y = height / kBaseHeight;
    scale_ = std::min(scale_x, scale_y);
    if (scale_ <= 0.0f) {
        scale_ = 1.0f;
    }
    offset_x_ = (width - kBaseWidth * scale_) * 0.5f;
    offset_y_ = (height - kBaseHeight * scale_) * 0.5f;
}

void MenuWindow::update_brushes() {
    if (!d2d_ || !d2d_->d2d_context) {
        return;
    }

    invalidate_gameplay_note_sprite_cache();
    invalidate_gameplay_static_cache();

    auto* ctx = d2d_->d2d_context.Get();
    ctx->CreateSolidColorBrush(D2D1::ColorF(0xE8ECF1), &d2d_->text_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x6EE7F2), &d2d_->accent_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0xFF4D6D, 0.38f), &d2d_->judgement_line_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x9AA3AD), &d2d_->muted_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x1F2130), &d2d_->card_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x14141C, 0.72f), &d2d_->panel_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x0B0B10, 0.75f), &d2d_->footer_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x242638), &d2d_->button_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x6EE7F2, 0.22f), &d2d_->button_selected_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x31344A), &d2d_->button_border_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0xF6F8FF, 0.97f), &d2d_->note_fill_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0xCAD8E7, 0.98f), &d2d_->note_border_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0xF6F8FF, 0.36f), &d2d_->note_hold_brush);

    {
        D2D1_GRADIENT_STOP stops[2]{};
        stops[0].position = 0.0f;
        stops[0].color = D2D1::ColorF(0x6EE7F2, 0.32f);
        stops[1].position = 1.0f;
        stops[1].color = D2D1::ColorF(0x0B0B10, 0.0f);
        if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 2, &d2d_->glow_stops))) {
            const D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props =
                D2D1::RadialGradientBrushProperties(
                    D2D1::Point2F(kBaseWidth * 0.5f, kBaseHeight * 0.55f),
                    D2D1::Point2F(0.0f, 0.0f),
                    900.0f,
                    520.0f);
            ctx->CreateRadialGradientBrush(props, d2d_->glow_stops.Get(), &d2d_->glow_brush);
        }
    }

    {
        D2D1_GRADIENT_STOP stops[2]{};
        stops[0].position = 0.0f;
        stops[0].color = D2D1::ColorF(0x6EE7F2, 1.0f);
        stops[1].position = 1.0f;
        stops[1].color = D2D1::ColorF(0xE8ECF1, 1.0f);
        if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 2, &d2d_->logo_stops))) {
            const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props =
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(0.0f, 0.0f),
                    D2D1::Point2F(kBaseWidth, 0.0f));
            ctx->CreateLinearGradientBrush(props, d2d_->logo_stops.Get(), &d2d_->logo_brush);
        }
    }

    auto create_button_gradient = [ctx](uint32_t a, uint32_t b,
                                        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection>* stops_out,
                                        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>* brush_out) {
        D2D1_GRADIENT_STOP stops[2]{};
        stops[0].position = 0.0f;
        stops[0].color = D2D1::ColorF(a, 0.95f);
        stops[1].position = 1.0f;
        stops[1].color = D2D1::ColorF(b, 0.95f);
        if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 2, stops_out->ReleaseAndGetAddressOf()))) {
            const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props =
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(0.0f, 0.0f),
                    D2D1::Point2F(1.0f, 1.0f));
            ctx->CreateLinearGradientBrush(props, stops_out->Get(), brush_out->ReleaseAndGetAddressOf());
        }
    };

    create_button_gradient(0x165CFF, 0x6EE7F2, &d2d_->play_stops, &d2d_->play_brush);
    create_button_gradient(0x7B2CFF, 0xFF60C8, &d2d_->edit_stops, &d2d_->edit_brush);
    create_button_gradient(0xFF8C1A, 0xFF4D6D, &d2d_->options_stops, &d2d_->options_brush);
    create_button_gradient(0xFF2D74, 0xB0003A, &d2d_->exit_stops, &d2d_->exit_brush);

    D2D1_GRADIENT_STOP stops[2]{};
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(0x0B0B10);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(0x14141C);

    if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 2, &d2d_->bg_stops))) {
        const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props =
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(0.0f, 0.0f),
                D2D1::Point2F(0.0f, static_cast<float>(height_)));
        ctx->CreateLinearGradientBrush(props, d2d_->bg_stops.Get(), &d2d_->bg_brush);
    }
}

void MenuWindow::invalidate_menu_scene_target() {
    if (!d2d_) {
        return;
    }
    d2d_->menu_scene_target_view.Reset();
}

bool MenuWindow::ensure_menu_scene_resources() {
    if (!d2d_ || !d2d_->device) {
        return false;
    }
    if (d2d_->menu_scene_vertex_shader && d2d_->menu_scene_pixel_shader && d2d_->menu_scene_constant_buffer) {
        return true;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> vertex_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixel_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
    constexpr UINT shader_flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

    const HRESULT vs_hr =
        D3DCompile(kMenuSceneShaderSource,
                   sizeof(kMenuSceneShaderSource) - 1,
                   "MenuScene",
                   nullptr,
                   nullptr,
                   "vs_main",
                   "vs_4_0",
                   shader_flags,
                   0,
                   &vertex_blob,
                   &error_blob);
    if (FAILED(vs_hr)) {
        if (error_blob) {
            std::cerr << "[MenuWindow] Menu-scene VS compile failed: "
                      << static_cast<const char*>(error_blob->GetBufferPointer()) << std::endl;
        }
        return false;
    }

    error_blob.Reset();
    const HRESULT ps_hr =
        D3DCompile(kMenuSceneShaderSource,
                   sizeof(kMenuSceneShaderSource) - 1,
                   "MenuScene",
                   nullptr,
                   nullptr,
                   "ps_main",
                   "ps_4_0",
                   shader_flags,
                   0,
                   &pixel_blob,
                   &error_blob);
    if (FAILED(ps_hr)) {
        if (error_blob) {
            std::cerr << "[MenuWindow] Menu-scene PS compile failed: "
                      << static_cast<const char*>(error_blob->GetBufferPointer()) << std::endl;
        }
        return false;
    }

    if (FAILED(d2d_->device->CreateVertexShader(vertex_blob->GetBufferPointer(),
                                                vertex_blob->GetBufferSize(),
                                                nullptr,
                                                d2d_->menu_scene_vertex_shader.ReleaseAndGetAddressOf()))) {
        return false;
    }
    if (FAILED(d2d_->device->CreatePixelShader(pixel_blob->GetBufferPointer(),
                                               pixel_blob->GetBufferSize(),
                                               nullptr,
                                               d2d_->menu_scene_pixel_shader.ReleaseAndGetAddressOf()))) {
        d2d_->menu_scene_vertex_shader.Reset();
        return false;
    }

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(MenuSceneConstants);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;

    if (FAILED(d2d_->device->CreateBuffer(&buffer_desc,
                                          nullptr,
                                          d2d_->menu_scene_constant_buffer.ReleaseAndGetAddressOf()))) {
        d2d_->menu_scene_vertex_shader.Reset();
        d2d_->menu_scene_pixel_shader.Reset();
        return false;
    }

    return true;
}

bool MenuWindow::render_menu_scene(MenuScreenKind kind, int64_t now_ns) {
    if (!menu_scene_enabled(kind) || !d2d_ || !d2d_->context || !d2d_->menu_scene_target_view) {
        return false;
    }
    if (!ensure_menu_scene_resources()) {
        return false;
    }

    MenuSceneConstants constants{};
    constants.resolution[0] = static_cast<float>(std::max(1u, width_));
    constants.resolution[1] = static_cast<float>(std::max(1u, height_));
    constants.time_sec = static_cast<float>(static_cast<double>(now_ns) / 1'000'000'000.0);
    constants.scene_kind = (kind == MenuScreenKind::SongSelect) ? 1.0f : 0.0f;

    if (kind == MenuScreenKind::SongSelect) {
        constants.primary_color[0] = 0.38f;
        constants.primary_color[1] = 0.84f;
        constants.primary_color[2] = 0.98f;
        constants.primary_color[3] = 1.0f;
        constants.secondary_color[0] = 0.56f;
        constants.secondary_color[1] = 0.62f;
        constants.secondary_color[2] = 0.98f;
        constants.secondary_color[3] = 1.0f;
    } else {
        constants.primary_color[0] = 0.25f;
        constants.primary_color[1] = 0.86f;
        constants.primary_color[2] = 0.93f;
        constants.primary_color[3] = 1.0f;
        constants.secondary_color[0] = 1.00f;
        constants.secondary_color[1] = 0.62f;
        constants.secondary_color[2] = 0.30f;
        constants.secondary_color[3] = 1.0f;
    }

    ID3D11DeviceContext* const context = d2d_->context.Get();
    context->UpdateSubresource(d2d_->menu_scene_constant_buffer.Get(), 0, nullptr, &constants, 0, 0);

    const float clear_color[4] = {0.015f, 0.020f, 0.032f, 1.0f};
    context->ClearRenderTargetView(d2d_->menu_scene_target_view.Get(), clear_color);

    ID3D11RenderTargetView* render_target = d2d_->menu_scene_target_view.Get();
    context->OMSetRenderTargets(1, &render_target, nullptr);

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(std::max(1u, width_));
    viewport.Height = static_cast<float>(std::max(1u, height_));
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(d2d_->menu_scene_vertex_shader.Get(), nullptr, 0);
    context->PSSetShader(d2d_->menu_scene_pixel_shader.Get(), nullptr, 0);

    ID3D11Buffer* constant_buffer = d2d_->menu_scene_constant_buffer.Get();
    context->VSSetConstantBuffers(0, 1, &constant_buffer);
    context->PSSetConstantBuffers(0, 1, &constant_buffer);
    context->Draw(3, 0);

    ID3D11Buffer* null_buffer = nullptr;
    context->VSSetConstantBuffers(0, 1, &null_buffer);
    context->PSSetConstantBuffers(0, 1, &null_buffer);
    context->VSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
    ID3D11RenderTargetView* null_target = nullptr;
    context->OMSetRenderTargets(1, &null_target, nullptr);
    return true;
}

bool MenuWindow::recreate_targets() {
    if (!d2d_ || !d2d_->swap_chain || !d2d_->d2d_context) {
        return false;
    }

    invalidate_menu_scene_target();
    invalidate_gameplay_note_sprite_cache();
    invalidate_song_select_preview_cache();
    clear_song_card_preview_cache();
    invalidate_gameplay_static_cache();
    d2d_->d2d_context->SetTarget(nullptr);
    d2d_->d2d_target.Reset();

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    const HRESULT buffer_hr = d2d_->swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(buffer_hr)) {
        std::cerr << "[MenuWindow::recreate_targets] GetBuffer failed hr=0x" << std::hex
                  << static_cast<unsigned long>(buffer_hr) << std::dec << std::endl;
        return false;
    }

    if (d2d_->device) {
        const HRESULT rtv_hr =
            d2d_->device->CreateRenderTargetView(back_buffer.Get(),
                                                 nullptr,
                                                 d2d_->menu_scene_target_view.ReleaseAndGetAddressOf());
        if (FAILED(rtv_hr)) {
            std::cerr << "[MenuWindow::recreate_targets] CreateRenderTargetView failed hr=0x" << std::hex
                      << static_cast<unsigned long>(rtv_hr) << std::dec << std::endl;
        }
    }

    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    if (FAILED(back_buffer.As(&surface))) {
        return false;
    }

    auto try_create = [&](D2D1_ALPHA_MODE alpha_mode, std::string_view label) -> bool {
        D2D1_BITMAP_PROPERTIES1 props =
            D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, alpha_mode),
                96.0f, 96.0f);

        Microsoft::WRL::ComPtr<ID2D1Bitmap1> target;
        const HRESULT hr =
            d2d_->d2d_context->CreateBitmapFromDxgiSurface(surface.Get(), &props, &target);
        if (FAILED(hr)) {
            std::cerr << "[MenuWindow::recreate_targets] CreateBitmapFromDxgiSurface(" << label
                      << ") failed hr=0x" << std::hex << static_cast<unsigned long>(hr) << std::dec
                      << std::endl;
            return false;
        }

        d2d_->d2d_target = std::move(target);
        d2d_->d2d_context->SetTarget(d2d_->d2d_target.Get());
        return true;
    };

    if (try_create(D2D1_ALPHA_MODE_IGNORE, "ignore")) {
        return true;
    }
    if (try_create(D2D1_ALPHA_MODE_PREMULTIPLIED, "premultiplied")) {
        return true;
    }
    if (try_create(D2D1_ALPHA_MODE_UNKNOWN, "unknown")) {
        return true;
    }
    return false;
}

void MenuWindow::resize_swap_chain(unsigned int width, unsigned int height) {
    if (!d2d_ || !d2d_->swap_chain || width == 0 || height == 0) {
        return;
    }

    width_ = width;
    height_ = height;
    invalidate_menu_scene_target();
    d2d_->d2d_context->SetTarget(nullptr);
    d2d_->d2d_target.Reset();
    d2d_->swap_chain->ResizeBuffers(0, width_, height_, DXGI_FORMAT_UNKNOWN, swap_chain_flags_);
    if (!recreate_targets()) {
        fail_fatal("Failed to recreate render target after resize.");
        shutdown();
        return;
    }
    update_layout();
    update_brushes();
}

}  // namespace tenriff::render

#endif  // _WIN32
