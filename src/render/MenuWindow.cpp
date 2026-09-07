#include "render/MenuWindow.h"
#include "render/GameplayFeedbackText.h"

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
#include <chrono>
#include <cstdint>
#include <ctime>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
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
#include "app/ImportedGameplaySkin.h"
#include "config/Config.h"
#include "input/InputThread.h"
#include "render/GameplayGaugePalette.h"
#include "render/GameplayGearLayout.h"
#include "render/GameplayMotion.h"
#include "render/GameplayNativeDigitalKey.h"
#include "render/ResultPresentation.h"
#include "render/TargaImage.h"
#include "render/TextFit.h"
#include "timing/HighResClock.h"
#include "util/Utf8Compat.h"

namespace tenriff::render {

namespace {

constexpr float kBaseWidth = 1920.0f;
constexpr float kBaseHeight = 1080.0f;
constexpr wchar_t kWindowClassName[] = L"TenRiffMenuWindow";
constexpr float kGameplayFieldLeft = 470.0f;
// Ceiling for the Targa fallback so a corrupt header cannot make the loader read a
// huge file into memory. A 4096x4096 32-bit frame is well past anything LR2 ships.
constexpr std::streamoff kMaxTargaBytes = 96ll * 1024ll * 1024ll;
constexpr std::uint32_t kDxgiErrorInvalidCall = 0x887A0001u;
constexpr std::uint32_t kDxgiStatusModeChanged = 0x087A0007u;
constexpr float kGameplayFieldRight = 1450.0f;
constexpr float kGameplayFieldTop = 0.0f;
constexpr float kGameplayFieldBottom = kBaseHeight;
constexpr float kGameplayGaugeLeft = 1510.0f;
constexpr float kGameplaySplitPlayerFieldLeft = 250.0f;
constexpr float kGameplaySplitPlayerFieldRight = 810.0f;
constexpr float kGameplaySplitGhostFieldLeft = 1110.0f;
constexpr float kGameplaySplitGhostFieldRight = 1670.0f;
constexpr float kGameplaySplitPlayerGaugeLeft = 188.0f;
constexpr float kGameplaySplitGhostGaugeLeft = 1708.0f;
constexpr float kGameplayGaugeTop = 210.0f;
constexpr float kGameplayGaugeBottom = 910.0f;
constexpr float kGameplayFieldDragHandleGap = 12.0f;
constexpr float kGameplayFieldDragHandleWidth = 72.0f;
constexpr float kGameplayFieldDragHandleHeight = 42.0f;
constexpr float kGameplayFieldDragHandleTop = 16.0f;
constexpr float kGameplayFieldDragCanvasMargin = 12.0f;
constexpr float kGameplayHoldTailTaperRatio = 0.55f;
constexpr float kGameplayGaugeWidth = 46.0f;
constexpr double kGameplayJudgementLineMin = config::kJudgementLinePositionMin;
constexpr double kGameplayJudgementLineMax = config::kJudgementLinePositionMax;
constexpr double kGameplayJudgementLineDefault = config::kJudgementLinePositionDefault;
constexpr double kGameplayNoteWidthScaleMin = 0.50;
constexpr double kGameplayNoteWidthScaleMax = 1.40;
constexpr double kGameplayLaneWidthScaleMin = 0.50;
constexpr double kGameplayLaneWidthScaleMax = 1.75;
constexpr double kGameplayLaneWidthScaleDefault = 1.00;
constexpr double kGameplayNoteHeightScaleMin = 0.50;
constexpr double kGameplayNoteHeightScaleMax = 4.00;
constexpr double kGameplayLaneSpacingScaleMin = 0.00;
constexpr double kGameplayLaneSpacingScaleMax = 2.00;
constexpr double kGameplayLaneSpacingScaleDefault = 0.00;
constexpr double kGameplayLaneDividerWidthScaleMin = 0.00;
constexpr double kGameplayLaneDividerWidthScaleMax = 2.00;
constexpr double kGameplayLaneDividerWidthScaleDefault = 1.00;
constexpr double kGameplayLaneCenterGapScaleMin = 0.00;
constexpr double kGameplayLaneCenterGapScaleMax = 2.00;
constexpr double kGameplayLaneCenterGapScaleDefault = 0.00;
constexpr double kGameplayHoldBodyWidthScaleMin = 0.50;
constexpr double kGameplayHoldBodyWidthScaleMax = 1.20;
constexpr double kGameplayHoldBodyWidthScaleDefault = 0.60;
constexpr double kGameplayNoteHeightScaleDefault = 1.80;
constexpr double kGameplayComboPositionMin = 0.10;
constexpr double kGameplayComboPositionMax = 0.78;
constexpr double kGameplayComboPositionDefault = 0.24;
constexpr float kGameplayComboWideCenterGapThreshold = 1.05f;
constexpr float kGameplayLaneDividerBaseWidth = 1.0f;
constexpr float kGameplayLaneDividerWidthMaxPx = 16.0f;

const wchar_t* ui_font_family_for_token(std::string_view token) {
    if (token == "malgun") {
        return L"Malgun Gothic";
    }
    if (token == "bahnschrift") {
        return L"Bahnschrift";
    }
    if (token == "consolas") {
        return L"Consolas";
    }
    return L"Segoe UI";
}

// Menu rects a TenRiff skin may move. Slots the skin left out fall through to
// the built-in rect, so every caller passes the original literal as `fallback`.
D2D1_RECT_F skin_layout_rect(const MenuRenderData& data,
                             std::string_view slot,
                             const D2D1_RECT_F& fallback) {
    if (!data.lobby_skin.enabled || data.lobby_skin.layout_rects.empty()) {
        return fallback;
    }
    auto it = data.lobby_skin.layout_rects.end();
    if (slot.rfind("generic.", 0u) == 0u) {
        const std::string suffix(slot.substr(std::string_view("generic.").size()));
        if (!data.lobby_skin.screen_id.empty()) {
            it = data.lobby_skin.layout_rects.find(data.lobby_skin.screen_id + "." + suffix);
        }
        if (it == data.lobby_skin.layout_rects.end() &&
            !data.lobby_skin.screen_fallback_id.empty()) {
            it = data.lobby_skin.layout_rects.find(
                data.lobby_skin.screen_fallback_id + "." + suffix);
        }
    }
    if (it == data.lobby_skin.layout_rects.end()) {
        it = data.lobby_skin.layout_rects.find(std::string(slot));
    }
    if (it == data.lobby_skin.layout_rects.end()) {
        return fallback;
    }
    return D2D1::RectF(it->second[0], it->second[1], it->second[2], it->second[3]);
}

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

float clamp_gameplay_lane_width_scale(double value) {
    if (!std::isfinite(value)) {
        return static_cast<float>(kGameplayLaneWidthScaleDefault);
    }
    return static_cast<float>(std::clamp(value, kGameplayLaneWidthScaleMin, kGameplayLaneWidthScaleMax));
}

std::string normalize_gameplay_skin_source(std::string_view value) {
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (normalized == "lr2") {
        return "lr2";
    }
    if (normalized == "tenriff") {
        return "tenriff";
    }
    return "native";
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
    if (feedback == "POOR") {
        return L"POOR";
    }
    if (feedback.empty()) {
        return {};
    }
    return std::wstring(feedback.begin(), feedback.end());
}

D2D1_COLOR_F gameplay_feedback_color(std::string_view feedback) {
    return D2D1::ColorF(gameplay_judgement_rgb(feedback));
}

float clamp_gameplay_note_height_scale(double value) {
    if (!std::isfinite(value)) {
        return static_cast<float>(kGameplayNoteHeightScaleDefault);
    }
    return static_cast<float>(std::clamp(value, kGameplayNoteHeightScaleMin, kGameplayNoteHeightScaleMax));
}

float clamp_gameplay_lane_spacing_scale(double value) {
    if (!std::isfinite(value)) {
        return static_cast<float>(kGameplayLaneSpacingScaleDefault);
    }
    return static_cast<float>(std::clamp(value, kGameplayLaneSpacingScaleMin, kGameplayLaneSpacingScaleMax));
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

float clamp_gameplay_lane_center_gap_scale(double value) {
    if (!std::isfinite(value)) {
        return static_cast<float>(kGameplayLaneCenterGapScaleDefault);
    }
    return static_cast<float>(std::clamp(
        value,
        kGameplayLaneCenterGapScaleMin,
        kGameplayLaneCenterGapScaleMax));
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

float sanitize_gameplay_imported_scale_ratio(float value) {
    if (!std::isfinite(value) || value <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(value, 0.25f, 4.0f);
}

float effective_gameplay_note_art_width_ratio(float imported_ratio, bool apply_imported_ratio) {
    return apply_imported_ratio ? sanitize_gameplay_imported_scale_ratio(imported_ratio) : 1.0f;
}

float effective_gameplay_note_height_scale(double value, float imported_ratio, bool apply_imported_ratio) {
    float scale = clamp_gameplay_note_height_scale(value);
    if (apply_imported_ratio) {
        scale *= sanitize_gameplay_imported_scale_ratio(imported_ratio);
    }
    return std::clamp(scale, 0.25f, 6.0f);
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
    if (normalized == "circle" || normalized == "triangle" || normalized == "pentagon" ||
        normalized == "hexagon" || normalized == "square" || normalized == "diamond" ||
        normalized == "arrow") {
        return normalized;
    }
    return "rect";
}

// Index into the cached unit geometries. Square and circle are drawn from
// primitives instead, so they have no entry here.
std::optional<std::size_t> gameplay_note_polygon_index(std::string_view note_shape) {
    const std::string normalized = normalize_gameplay_note_shape(note_shape);
    if (normalized == "triangle") return 0;
    if (normalized == "pentagon") return 1;
    if (normalized == "hexagon") return 2;
    if (normalized == "diamond") return 3;
    if (normalized == "arrow") return 4;
    return std::nullopt;
}

bool create_unit_note_polygon_geometry(ID2D1Factory1* factory,
                                       int sides,
                                       ID2D1PathGeometry** out_geometry) {
    if (!factory || !out_geometry || sides < 3) {
        return false;
    }
    *out_geometry = nullptr;

    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(factory->CreatePathGeometry(geometry.ReleaseAndGetAddressOf())) || !geometry) {
        return false;
    }
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(sink.ReleaseAndGetAddressOf())) || !sink) {
        return false;
    }

    constexpr double kPi = 3.14159265358979323846;
    std::vector<D2D1_POINT_2F> points;
    points.reserve(static_cast<std::size_t>(sides));
    for (int i = 0; i < sides; ++i) {
        const double angle = -kPi * 0.5 + 2.0 * kPi * static_cast<double>(i) / static_cast<double>(sides);
        points.push_back(D2D1::Point2F(static_cast<float>(std::cos(angle) * 0.5),
                                      static_cast<float>(std::sin(angle) * 0.5)));
    }
    float min_x = points.front().x;
    float max_x = points.front().x;
    float min_y = points.front().y;
    float max_y = points.front().y;
    for (const auto& point : points) {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }
    const float extent_x = std::max(0.001f, max_x - min_x);
    const float center_x = (min_x + max_x) * 0.5f;
    const float center_y = (min_y + max_y) * 0.5f;
    for (auto& point : points) {
        // Normalize against the horizontal extent only. Every polygon reaches the same
        // lane width as a 100% bar while preserving its regular-polygon proportions.
        point.x = (point.x - center_x) / extent_x;
        point.y = (point.y - center_y) / extent_x;
    }
    sink->BeginFigure(points.front(), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLines(points.data() + 1, static_cast<UINT32>(points.size() - 1));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(sink->Close())) {
        return false;
    }

    *out_geometry = geometry.Detach();
    return true;
}

// An upward chevron in the same unit box the regular polygons use: one lane wide,
// centred on the origin. Lane rotation then aims it wherever the skin wants.
bool create_unit_note_arrow_geometry(ID2D1Factory1* factory, ID2D1PathGeometry** out_geometry) {
    if (!factory || !out_geometry) {
        return false;
    }
    *out_geometry = nullptr;

    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(factory->CreatePathGeometry(geometry.ReleaseAndGetAddressOf())) || !geometry) {
        return false;
    }
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(sink.ReleaseAndGetAddressOf())) || !sink) {
        return false;
    }

    constexpr float kShaftHalfWidth = 0.18f;
    constexpr float kHeadY = -0.04f;
    const D2D1_POINT_2F points[] = {
        D2D1::Point2F(0.0f, -0.5f),               // tip
        D2D1::Point2F(0.5f, kHeadY),              // right barb
        D2D1::Point2F(kShaftHalfWidth, kHeadY),   // right shoulder
        D2D1::Point2F(kShaftHalfWidth, 0.5f),     // right shaft
        D2D1::Point2F(-kShaftHalfWidth, 0.5f),    // left shaft
        D2D1::Point2F(-kShaftHalfWidth, kHeadY),  // left shoulder
        D2D1::Point2F(-0.5f, kHeadY),             // left barb
    };
    sink->BeginFigure(points[0], D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLines(points + 1, static_cast<UINT32>(std::size(points) - 1));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(sink->Close())) {
        return false;
    }

    *out_geometry = geometry.Detach();
    return true;
}

template <std::size_t Size>
ID2D1Geometry* gameplay_note_polygon_geometry(
    const std::array<Microsoft::WRL::ComPtr<ID2D1PathGeometry>, Size>& geometries,
    std::string_view note_shape) {
    const auto index = gameplay_note_polygon_index(note_shape);
    if (!index.has_value() || index.value() >= geometries.size()) {
        return nullptr;
    }
    return geometries[index.value()].Get();
}

float gameplay_hold_body_width_ratio_at_y(float position_y,
                                          float head_y,
                                          float tail_y,
                                          bool taper_enabled) {
    if (!taper_enabled) {
        return 1.0f;
    }
    const float delta = tail_y - head_y;
    if (std::abs(delta) <= 0.001f) {
        return 1.0f;
    }
    const float t = std::clamp((position_y - head_y) / delta, 0.0f, 1.0f);
    return 1.0f + (kGameplayHoldTailTaperRatio - 1.0f) * t;
}

void draw_gameplay_hold_body(ID2D1RenderTarget* target,
                             ID2D1Factory1* factory,
                             const D2D1_RECT_F& body_rect,
                             float lane_center,
                             float head_y,
                             float tail_y,
                             float base_half_width,
                             bool taper_enabled,
                             ID2D1Brush* brush) {
    if (!target || !brush || body_rect.bottom <= body_rect.top || base_half_width <= 0.0f) {
        return;
    }

    if (!taper_enabled || !factory) {
        target->FillRectangle(body_rect, brush);
        return;
    }

    const float top_half_width =
        std::max(1.0f, base_half_width * gameplay_hold_body_width_ratio_at_y(body_rect.top, head_y, tail_y, true));
    const float bottom_half_width =
        std::max(1.0f, base_half_width * gameplay_hold_body_width_ratio_at_y(body_rect.bottom, head_y, tail_y, true));

    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(factory->CreatePathGeometry(&geometry)) || !geometry) {
        target->FillRectangle(body_rect, brush);
        return;
    }

    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(&sink)) || !sink) {
        target->FillRectangle(body_rect, brush);
        return;
    }

    sink->BeginFigure(D2D1::Point2F(lane_center - top_half_width, body_rect.top), D2D1_FIGURE_BEGIN_FILLED);
    const D2D1_POINT_2F points[] = {
        D2D1::Point2F(lane_center + top_half_width, body_rect.top),
        D2D1::Point2F(lane_center + bottom_half_width, body_rect.bottom),
        D2D1::Point2F(lane_center - bottom_half_width, body_rect.bottom),
    };
    sink->AddLines(points, ARRAYSIZE(points));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(sink->Close())) {
        target->FillRectangle(body_rect, brush);
        return;
    }

    target->FillGeometry(geometry.Get(), brush);
}

struct GameplayFieldLayout {
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    int lane_count = 0;
    float lane_width = 0.0f;
    float center_gap = 0.0f;
    float center_gap_scale = 0.0f;
    int split_after_lane = 0;
    float note_width = 16.0f;
    std::array<float, kGameplayHudMaxLanes> lane_lefts{};
    std::array<float, kGameplayHudMaxLanes> lane_widths{};
    std::array<float, kGameplayHudMaxLanes> note_widths{};
    std::array<float, kGameplayHudMaxLanes> divider_gaps{};
};

struct GameplaySurfaceLayout {
    GameplayFieldLayout player_field{};
    GameplayFieldLayout ghost_field{};
    float player_gauge_left = 0.0f;
    float ghost_gauge_left = 0.0f;
    float offset_x = 0.0f;
    float min_offset_x = 0.0f;
    float max_offset_x = 0.0f;
    bool ghost_visible = false;
};

void translate_gameplay_field_layout(GameplayFieldLayout& layout, float offset_x) {
    layout.left += offset_x;
    layout.right += offset_x;
    const std::size_t lane_count = std::min(
        static_cast<std::size_t>(std::max(0, layout.lane_count)), layout.lane_lefts.size());
    for (std::size_t lane = 0; lane < lane_count; ++lane) {
        layout.lane_lefts[lane] += offset_x;
    }
}

GameplayFieldLayout build_gameplay_field_layout(float bounds_left,
                                                float bounds_right,
                                                float top,
                                                float bottom,
                                                int lane_count,
                                                double note_width_scale,
                                                double note_art_width_ratio,
                                                std::size_t lane_width_scale_count,
                                                const std::array<double, kGameplayHudMaxLanes>& lane_width_scales,
                                                std::size_t lane_spacing_scale_count,
                                                const std::array<double, kGameplayHudMaxLanes>& lane_spacing_scales,
                                                double lane_center_gap_scale = 0.0,
                                                double note_divider_gap_px = kGameplayNoteDividerGapPxDefault) {
    const int clamped_lane_count = std::max(1, lane_count);
    const float bounds_width = std::max(160.0f, bounds_right - bounds_left);
    const bool use_center_gap = clamped_lane_count == 16;
    const float center_gap_scale =
        (use_center_gap ? clamp_gameplay_lane_center_gap_scale(lane_center_gap_scale) : 0.0f);
    std::array<float, kGameplayHudMaxLanes> lane_units{};
    std::array<float, kGameplayHudMaxLanes> gap_units{};
    float total_lane_units = 0.0f;
    float total_gap_units = 0.0f;
    for (int lane = 0; lane < clamped_lane_count; ++lane) {
        float lane_unit = static_cast<float>(kGameplayLaneWidthScaleDefault);
        if (static_cast<std::size_t>(lane) < lane_width_scale_count) {
            lane_unit = clamp_gameplay_lane_width_scale(lane_width_scales[static_cast<std::size_t>(lane)]);
        }
        lane_units[static_cast<std::size_t>(lane)] = lane_unit;
        total_lane_units += lane_unit;
        if (lane + 1 >= clamped_lane_count) {
            continue;
        }
        float gap_unit = 0.0f;
        if (static_cast<std::size_t>(lane) < lane_spacing_scale_count) {
            gap_unit = clamp_gameplay_lane_spacing_scale(lane_spacing_scales[static_cast<std::size_t>(lane)]);
        }
        if (use_center_gap && lane == 7) {
            gap_unit += center_gap_scale;
        }
        gap_units[static_cast<std::size_t>(lane)] = gap_unit;
        total_gap_units += gap_unit;
    }
    const float effective_lane_units = std::max(1.0f, total_lane_units + total_gap_units);
    // Note Size scales the complete field around its center so lanes, gaps, and note art stay linked.
    const float field_width = compute_gameplay_playfield_width(bounds_width, note_width_scale);
    const float center_x = (bounds_left + bounds_right) * 0.5f;
    const float unit_width = field_width / effective_lane_units;
    const float center_gap_only = (use_center_gap ? unit_width * center_gap_scale : 0.0f);

    GameplayFieldLayout layout;
    layout.left = center_x - field_width * 0.5f;
    layout.top = top;
    layout.bottom = bottom;
    layout.height = std::max(1.0f, bottom - top);
    layout.lane_count = clamped_lane_count;
    layout.center_gap = center_gap_only;
    layout.center_gap_scale = center_gap_scale;
    layout.split_after_lane = (use_center_gap && center_gap_only > 0.0f) ? 8 : 0;
    float cursor = layout.left;
    float total_lane_width = 0.0f;
    float total_note_width = 0.0f;
    for (int lane = 0; lane < clamped_lane_count; ++lane) {
        const std::size_t index = static_cast<std::size_t>(lane);
        const float lane_width_px = unit_width * lane_units[index];
        layout.lane_lefts[index] = cursor;
        layout.lane_widths[index] = lane_width_px;
        // The divider line sits at the middle of the gap on either side. Take the
        // wider neighbouring gap so the note stays centred on its lane and every
        // lane ends up the same width.
        const float gap_before = lane > 0 ? gap_units[static_cast<std::size_t>(lane - 1)] : 0.0f;
        const float gap_after = (lane + 1 < clamped_lane_count) ? gap_units[index] : 0.0f;
        layout.note_widths[index] = compute_gameplay_note_draw_width(
            lane_width_px,
            note_width_scale,
            note_art_width_ratio,
            note_divider_gap_px,
            unit_width * std::max(gap_before, gap_after));
        total_lane_width += lane_width_px;
        total_note_width += layout.note_widths[index];
        cursor += lane_width_px;
        if (lane + 1 < clamped_lane_count) {
            layout.divider_gaps[index] = unit_width * gap_units[index];
            cursor += layout.divider_gaps[index];
        }
    }
    layout.right = cursor;
    layout.width = std::max(1.0f, layout.right - layout.left);
    layout.lane_width = total_lane_width / static_cast<float>(clamped_lane_count);
    layout.note_width = total_note_width / static_cast<float>(clamped_lane_count);
    return layout;
}

GameplaySurfaceLayout build_gameplay_surface_layout(int lane_count,
                                                    double note_width_scale,
                                                    double note_art_width_ratio,
                                                    std::size_t lane_width_scale_count,
                                                    const std::array<double, kGameplayHudMaxLanes>& lane_width_scales,
                                                    std::size_t lane_spacing_scale_count,
                                                    const std::array<double, kGameplayHudMaxLanes>& lane_spacing_scales,
                                                    bool ghost_visible,
                                                    double lane_center_gap_scale = 0.0,
                                                    double requested_offset_x = 0.0,
                                                    double note_divider_gap_px = kGameplayNoteDividerGapPxDefault) {
    GameplaySurfaceLayout layout;
    layout.ghost_visible = ghost_visible;
    if (ghost_visible) {
        layout.player_field = build_gameplay_field_layout(kGameplaySplitPlayerFieldLeft,
                                                          kGameplaySplitPlayerFieldRight,
                                                          kGameplayFieldTop,
                                                          kGameplayFieldBottom,
                                                          lane_count,
                                                          note_width_scale,
                                                          note_art_width_ratio,
                                                          lane_width_scale_count,
                                                          lane_width_scales,
                                                          lane_spacing_scale_count,
                                                          lane_spacing_scales,
                                                          lane_center_gap_scale,
                                                          note_divider_gap_px);
        layout.ghost_field = build_gameplay_field_layout(kGameplaySplitGhostFieldLeft,
                                                         kGameplaySplitGhostFieldRight,
                                                         kGameplayFieldTop,
                                                         kGameplayFieldBottom,
                                                         lane_count,
                                                         note_width_scale,
                                                         note_art_width_ratio,
                                                         lane_width_scale_count,
                                                         lane_width_scales,
                                                         lane_spacing_scale_count,
                                                         lane_spacing_scales,
                                                         lane_center_gap_scale,
                                                         note_divider_gap_px);
        layout.player_gauge_left =
            layout.player_field.left - (kGameplaySplitPlayerFieldLeft - kGameplaySplitPlayerGaugeLeft);
        layout.ghost_gauge_left =
            layout.ghost_field.right + (kGameplaySplitGhostGaugeLeft - kGameplaySplitGhostFieldRight);
    } else {
        layout.player_field = build_gameplay_field_layout(kGameplayFieldLeft,
                                                          kGameplayFieldRight,
                                                          kGameplayFieldTop,
                                                          kGameplayFieldBottom,
                                                          lane_count,
                                                          note_width_scale,
                                                          note_art_width_ratio,
                                                          lane_width_scale_count,
                                                          lane_width_scales,
                                                          lane_spacing_scale_count,
                                                          lane_spacing_scales,
                                                          lane_center_gap_scale,
                                                          note_divider_gap_px);
        layout.player_gauge_left =
            layout.player_field.right + (kGameplayGaugeLeft - kGameplayFieldRight);
    }

    float surface_left = std::min(layout.player_field.left, layout.player_gauge_left);
    float surface_right = std::max(
        layout.player_field.right + kGameplayFieldDragHandleGap + kGameplayFieldDragHandleWidth,
        layout.player_gauge_left + kGameplayGaugeWidth);
    if (layout.ghost_visible) {
        surface_left = std::min(
            surface_left, std::min(layout.ghost_field.left, layout.ghost_gauge_left));
        surface_right = std::max(
            surface_right,
            std::max(layout.ghost_field.right, layout.ghost_gauge_left + kGameplayGaugeWidth));
    }

    layout.min_offset_x = kGameplayFieldDragCanvasMargin - surface_left;
    layout.max_offset_x = kBaseWidth - kGameplayFieldDragCanvasMargin - surface_right;
    const double finite_requested =
        std::isfinite(requested_offset_x) ? requested_offset_x : config::kGameplayFieldOffsetXDefault;
    const float config_clamped_offset = static_cast<float>(std::clamp(
        finite_requested,
        config::kGameplayFieldOffsetXMin,
        config::kGameplayFieldOffsetXMax));
    layout.offset_x = layout.min_offset_x <= layout.max_offset_x
                          ? std::clamp(config_clamped_offset,
                                       layout.min_offset_x,
                                       layout.max_offset_x)
                          : (layout.min_offset_x + layout.max_offset_x) * 0.5f;
    translate_gameplay_field_layout(layout.player_field, layout.offset_x);
    layout.player_gauge_left += layout.offset_x;
    if (layout.ghost_visible) {
        translate_gameplay_field_layout(layout.ghost_field, layout.offset_x);
        layout.ghost_gauge_left += layout.offset_x;
    }
    return layout;
}

float gameplay_lane_left(const GameplayFieldLayout& field_layout, int lane_index) {
    const int clamped_lane = std::clamp(lane_index, 0, std::max(0, field_layout.lane_count - 1));
    return field_layout.lane_lefts[static_cast<std::size_t>(clamped_lane)];
}

float gameplay_lane_right(const GameplayFieldLayout& field_layout, int lane_index) {
    const int clamped_lane = std::clamp(lane_index, 0, std::max(0, field_layout.lane_count - 1));
    return gameplay_lane_left(field_layout, clamped_lane) +
           field_layout.lane_widths[static_cast<std::size_t>(clamped_lane)];
}

float gameplay_lane_width(const GameplayFieldLayout& field_layout, int lane_index) {
    const int clamped_lane = std::clamp(lane_index, 0, std::max(0, field_layout.lane_count - 1));
    return field_layout.lane_widths[static_cast<std::size_t>(clamped_lane)];
}

float gameplay_lane_center(const GameplayFieldLayout& field_layout, int lane_index) {
    return gameplay_lane_left(field_layout, lane_index) + gameplay_lane_width(field_layout, lane_index) * 0.5f;
}

float gameplay_note_width(const GameplayFieldLayout& field_layout, int lane_index) {
    const int clamped_lane = std::clamp(lane_index, 0, std::max(0, field_layout.lane_count - 1));
    return field_layout.note_widths[static_cast<std::size_t>(clamped_lane)];
}

float gameplay_lane_gap_after(const GameplayFieldLayout& field_layout, int lane_index) {
    const int clamped_lane = std::clamp(lane_index, 0, std::max(0, field_layout.lane_count - 2));
    return field_layout.divider_gaps[static_cast<std::size_t>(clamped_lane)];
}

float gameplay_lane_divider_x(const GameplayFieldLayout& field_layout, std::size_t divider_index) {
    const int divider_lane = std::clamp(static_cast<int>(divider_index), 0, std::max(0, field_layout.lane_count - 2));
    return gameplay_lane_right(field_layout, divider_lane) +
           gameplay_lane_gap_after(field_layout, divider_lane) * 0.5f;
}

bool gameplay_is_center_gap_divider(const GameplayFieldLayout& field_layout, std::size_t divider_index) {
    return field_layout.split_after_lane > 0 &&
           (static_cast<int>(divider_index) + 1) == field_layout.split_after_lane;
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

float gameplay_combo_anchor_y(const GameplayFieldLayout& field_layout,
                              double combo_position,
                              float top_safe_margin,
                              float bottom_safe_margin) {
    const float raw_y =
        gameplay_field_y(field_layout.top, field_layout.height, clamp_gameplay_combo_position(combo_position));
    const float min_y = field_layout.top + std::max(0.0f, top_safe_margin);
    const float max_y = field_layout.bottom - std::max(0.0f, bottom_safe_margin);
    if (max_y <= min_y) {
        return (field_layout.top + field_layout.bottom) * 0.5f;
    }
    return std::clamp(raw_y, min_y, max_y);
}

D2D1_RECT_F gameplay_centered_overlay_rect(const GameplayFieldLayout& field_layout,
                                           float center_y,
                                           float half_height,
                                           float horizontal_inset = 0.0f) {
    const float inset = std::max(0.0f, horizontal_inset);
    const float left = std::min(field_layout.right, field_layout.left + inset);
    const float right = std::max(left, field_layout.right - inset);
    return D2D1::RectF(left, center_y - half_height, right, center_y + half_height);
}

bool gameplay_combo_uses_center_gap_anchor(const GameplayFieldLayout& field_layout) {
    return field_layout.lane_count == 16 &&
           field_layout.split_after_lane == 8 &&
           field_layout.center_gap_scale >= kGameplayComboWideCenterGapThreshold;
}

float gameplay_combo_overlay_center_x(const GameplayFieldLayout& field_layout) {
    if (gameplay_combo_uses_center_gap_anchor(field_layout)) {
        const float left_group_right = gameplay_lane_right(field_layout, field_layout.split_after_lane - 1);
        const float right_group_left = gameplay_lane_left(field_layout, field_layout.split_after_lane);
        return (left_group_right + right_group_left) * 0.5f;
    }
    return (field_layout.left + field_layout.right) * 0.5f;
}

D2D1_RECT_F gameplay_combo_overlay_rect(const GameplayFieldLayout& field_layout,
                                         double combo_position,
                                         float half_height,
                                         float top_safe_margin,
                                         float bottom_safe_margin,
                                         float vertical_offset = 0.0f,
                                         float horizontal_inset = 0.0f,
                                         float bottom_extension = 0.0f) {
    const float anchor_y =
        gameplay_combo_anchor_y(field_layout, combo_position, top_safe_margin, bottom_safe_margin) + vertical_offset;
    const float min_center_y = field_layout.top + std::max(0.0f, half_height);
    const float max_center_y = field_layout.bottom - std::max(0.0f, half_height) -
                               std::max(0.0f, bottom_extension);
    const float center_y =
        (max_center_y <= min_center_y) ? (field_layout.top + field_layout.bottom) * 0.5f
                                       : std::clamp(anchor_y, min_center_y, max_center_y);
    if (gameplay_combo_uses_center_gap_anchor(field_layout)) {
        const float inset = std::max(0.0f, horizontal_inset);
        const float center_x = gameplay_combo_overlay_center_x(field_layout);
        const float left = std::min(field_layout.right, field_layout.left + inset);
        const float right = std::max(left, field_layout.right - inset);
        const float max_half_width = std::max(0.0f, std::min(center_x - left, right - center_x));
        if (max_half_width <= 0.0f) {
            return gameplay_centered_overlay_rect(field_layout, center_y, half_height, horizontal_inset);
        }
        const float min_width = std::max(180.0f, half_height * 6.0f);
        const float preferred_width = std::max(min_width, field_layout.center_gap + half_height * 4.0f);
        const float overlay_half_width = std::min(preferred_width * 0.5f, max_half_width);
        return D2D1::RectF(center_x - overlay_half_width,
                           center_y - half_height,
                           center_x + overlay_half_width,
                           center_y + half_height);
    }
    return gameplay_centered_overlay_rect(field_layout, center_y, half_height, horizontal_inset);
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
                                           NoteImageAspect aspect) {
    if (aspect == NoteImageAspect::Stretch || !bitmap) {
        return note_rect;
    }
    const D2D1_SIZE_F size = bitmap_source_size(bitmap, source_rect);
    if (aspect == NoteImageAspect::Contain) {
        return fit_rect_preserve_aspect(note_rect, size);
    }
    if (size.width <= 0.0f || size.height <= 0.0f) {
        return note_rect;
    }
    // Width mode: the lane sets the sprite width and the image aspect sets its
    // height, so a square arrow stays square however short the note rect is.
    const float width = note_rect.right - note_rect.left;
    const float height = width * (size.height / size.width);
    const float center_y = (note_rect.top + note_rect.bottom) * 0.5f;
    return D2D1::RectF(note_rect.left, center_y - height * 0.5f, note_rect.right,
                       center_y + height * 0.5f);
}

float gameplay_lane_sprite_rotation(const std::array<float, kGameplayHudMaxLanes>& rotations,
                                    std::size_t count,
                                    std::size_t lane) {
    return lane < count ? rotations[lane] : 0.0f;
}

// Rotation is clockwise about the destination centre, pre-multiplied onto the
// scene transform so the menu's base-space scaling still applies.
void draw_gameplay_sprite(ID2D1DeviceContext* ctx,
                          ID2D1Bitmap* bitmap,
                          const D2D1_RECT_F& dest,
                          float opacity,
                          const D2D1_RECT_F* source_rect,
                          float rotation_degrees) {
    if (rotation_degrees == 0.0f) {
        ctx->DrawBitmap(bitmap, dest, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source_rect);
        return;
    }
    D2D1_MATRIX_3X2_F saved{};
    ctx->GetTransform(&saved);
    const D2D1_POINT_2F center =
        D2D1::Point2F((dest.left + dest.right) * 0.5f, (dest.top + dest.bottom) * 0.5f);
    ctx->SetTransform(D2D1::Matrix3x2F::Rotation(rotation_degrees, center) * saved);
    ctx->DrawBitmap(bitmap, dest, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source_rect);
    ctx->SetTransform(saved);
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

Microsoft::WRL::ComPtr<IWICBitmapSource> load_targa_as_wic_bitmap(IWICImagingFactory* wic_factory,
                                                                  const std::filesystem::path& file_path) {
    Microsoft::WRL::ComPtr<IWICBitmapSource> source;
    if (!wic_factory) {
        return source;
    }
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file) {
        return source;
    }
    const std::streamoff byte_count = file.tellg();
    if (byte_count <= 0 || byte_count > kMaxTargaBytes) {
        return source;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(byte_count));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), byte_count)) {
        return source;
    }

    const TargaImage image = decode_targa(bytes.data(), bytes.size());
    if (!image.valid) {
        return source;
    }
    const UINT stride = static_cast<UINT>(image.width) * 4u;
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const HRESULT hr = wic_factory->CreateBitmapFromMemory(static_cast<UINT>(image.width),
                                                           static_cast<UINT>(image.height),
                                                           GUID_WICPixelFormat32bppPBGRA,
                                                           stride,
                                                           static_cast<UINT>(image.pixels.size()),
                                                           const_cast<BYTE*>(image.pixels.data()),
                                                           bitmap.ReleaseAndGetAddressOf());
    if (FAILED(hr) || !bitmap) {
        return source;
    }
    source = bitmap;
    return source;
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

    const std::filesystem::path file_path = util::path_from_utf8_lossy(path);
    const std::wstring wide_path = file_path.native();
    Microsoft::WRL::ComPtr<IWICBitmapSource> source;
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    const HRESULT decoder_hr = wic_factory->CreateDecoderFromFilename(
        wide_path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (SUCCEEDED(decoder_hr) && decoder) {
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        if (SUCCEEDED(decoder->GetFrame(0, &frame)) && frame) {
            source = frame;
        }
    }
    if (!source) {
        // Windows has no Targa codec, so classic LR2 themes decode nothing at all
        // through WIC. Fall back to the in-tree decoder before giving up.
        source = load_targa_as_wic_bitmap(wic_factory, file_path);
    }
    if (!source) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(wic_factory->CreateFormatConverter(&converter)) || !converter) {
        return false;
    }
    const HRESULT convert_hr = converter->Initialize(source.Get(),
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

bool load_bitmap_from_imported_asset(IWICImagingFactory* wic_factory,
                                     ID2D1DeviceContext* d2d_context,
                                     const app::ImportedSkinImageAsset& asset,
                                     Microsoft::WRL::ComPtr<ID2D1Bitmap>& out_bitmap,
                                     D2D1_RECT_F* out_source_rect = nullptr,
                                     bool trim_alpha = false) {
    if (asset.path.empty()) {
        return false;
    }
    D2D1_RECT_F loaded_source_rect = D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    const bool should_trim_alpha = trim_alpha && !asset.has_source_rect;
    if (!load_bitmap_from_utf8_path(
            wic_factory, d2d_context, asset.path, out_bitmap, &loaded_source_rect, should_trim_alpha)) {
        return false;
    }
    if (out_source_rect) {
        if (asset.has_source_rect) {
            *out_source_rect = D2D1::RectF(asset.source_x,
                                           asset.source_y,
                                           asset.source_x + asset.source_width,
                                           asset.source_y + asset.source_height);
        } else {
            *out_source_rect = loaded_source_rect;
        }
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

std::string renderer_log_timestamp_local() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
    localtime_s(&local_tm, &now_time);

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream stream;
    stream << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S")
           << '.'
           << std::setfill('0')
           << std::setw(3)
           << ms.count();
    return stream.str();
}

std::filesystem::path renderer_runtime_log_path() {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= static_cast<DWORD>(std::size(buffer))) {
        return {};
    }

    std::error_code ec;
    std::filesystem::path log_dir = std::filesystem::path(buffer).parent_path() / "logs";
    std::filesystem::create_directories(log_dir, ec);
    if (ec) {
        return {};
    }
    return log_dir / "run.log";
}

bool append_renderer_fatal_log(std::string_view message) {
    const std::filesystem::path log_path = renderer_runtime_log_path();
    if (log_path.empty()) {
        return false;
    }

    std::ofstream out(log_path, std::ios::app);
    if (!out) {
        return false;
    }

    out << "[fatal][" << renderer_log_timestamp_local() << "] " << message << '\n';
    out.flush();
    return out.good();
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

D2D1_COLOR_F gameplay_note_fill_color(uint32_t rgb, float opacity = 0.96f) {
    return color_from_rgb(rgb, std::clamp(opacity, 0.0f, 1.0f));
}

D2D1_COLOR_F gameplay_note_border_color(uint32_t rgb, float opacity = 0.78f) {
    return color_from_rgb(blend_rgb(rgb, 0xFFFFFF, 0.55f), std::clamp(opacity, 0.0f, 1.0f));
}

D2D1_COLOR_F gameplay_note_hold_color(uint32_t rgb, float opacity = 0.24f) {
    return color_from_rgb(blend_rgb(rgb, 0xFFFFFF, 0.18f), std::clamp(opacity, 0.0f, 1.0f));
}

float gameplay_native_hold_body_opacity(float base_opacity, float visual_opacity) {
    // The legacy body alpha is intentionally subtle for bitmap skins, but applying it directly to
    // the procedural material makes the narrow LN body disappear against jacket-heavy playfields.
    // Boost only the native/fallback pass; imported body bitmaps keep their authored alpha.
    constexpr float kNativeHoldBodyContrastGain = 2.10f;
    return std::min(std::clamp(visual_opacity, 0.0f, 1.0f),
                    std::clamp(base_opacity, 0.0f, 1.0f) * kNativeHoldBodyContrastGain);
}

D2D1_COLOR_F gameplay_lane_preview_fill(uint32_t rgb, bool selected, float opacity = 0.18f) {
    const float base_opacity = std::clamp(opacity, 0.0f, 0.60f);
    return color_from_rgb(blend_rgb(rgb, 0xFFFFFF, selected ? 0.12f : 0.04f),
                          std::clamp(base_opacity * (selected ? 1.35f : 1.0f), 0.0f, 0.70f));
}

void draw_note_primitive(ID2D1RenderTarget* target,
                         const D2D1_RECT_F& rect,
                         ID2D1Brush* fill,
                         ID2D1Brush* border,
                         float border_width,
                         std::string_view note_shape,
                         bool draw_border,
                         ID2D1Geometry* polygon_geometry = nullptr) {
    if (!target || !fill) {
        return;
    }

    const std::string normalized_shape = normalize_gameplay_note_shape(note_shape);
    const auto extents = gameplay_note_shape_extents(rect.right - rect.left,
                                                     rect.bottom - rect.top,
                                                     normalized_shape);
    const float center_x = (rect.left + rect.right) * 0.5f;
    const float center_y = (rect.top + rect.bottom) * 0.5f;
    const D2D1_RECT_F shape_rect = D2D1::RectF(center_x - extents.half_width,
                                               center_y - extents.half_height,
                                               center_x + extents.half_width,
                                               center_y + extents.half_height);
    const D2D1_ANTIALIAS_MODE saved_antialias = target->GetAntialiasMode();
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (normalized_shape == "circle") {
        const float diameter = std::max(2.0f, shape_rect.right - shape_rect.left - 2.0f);
        const float radius = diameter * 0.5f;
        const D2D1_ELLIPSE ellipse = D2D1::Ellipse(
            D2D1::Point2F(center_x, center_y),
            radius,
            radius);
        target->FillEllipse(ellipse, fill);
        if (draw_border && border) {
            target->DrawEllipse(ellipse, border, border_width);
        }
    } else if (polygon_geometry && gameplay_note_polygon_index(normalized_shape).has_value()) {
        const float width = std::max(2.0f, shape_rect.right - shape_rect.left);
        const float height = width;
        D2D1_MATRIX_3X2_F saved_transform{};
        target->GetTransform(&saved_transform);
        const D2D1_MATRIX_3X2_F shape_transform =
            D2D1::Matrix3x2F::Scale(width, height) *
            D2D1::Matrix3x2F::Translation(center_x, center_y) *
            saved_transform;
        target->SetTransform(shape_transform);
        target->FillGeometry(polygon_geometry, fill);
        if (draw_border && border) {
            const float unit_border_width = border_width / std::max(width, height);
            target->DrawGeometry(polygon_geometry, border, unit_border_width);
        }
        target->SetTransform(saved_transform);
    } else {
        const float width = std::max(1.0f, shape_rect.right - shape_rect.left);
        const float height = std::max(1.0f, shape_rect.bottom - shape_rect.top);
        const float radius = std::clamp(std::min(width, height) * 0.22f, 3.0f, 8.0f);
        const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(shape_rect, radius, radius);
        target->FillRoundedRectangle(rounded, fill);
        if (draw_border && border) {
            target->DrawRoundedRectangle(rounded, border, border_width);
        }
    }

    target->SetAntialiasMode(saved_antialias);
}

ID2D1Brush* configure_gameplay_material_brush(ID2D1LinearGradientBrush* material,
                                              ID2D1Brush* fallback,
                                              const D2D1_RECT_F& rect,
                                              float opacity,
                                              bool horizontal) {
    if (!material) {
        return fallback;
    }
    material->SetStartPoint(D2D1::Point2F(rect.left, rect.top));
    material->SetEndPoint(horizontal ? D2D1::Point2F(rect.right, rect.top)
                                     : D2D1::Point2F(rect.left, rect.bottom));
    material->SetOpacity(std::clamp(opacity, 0.0f, 1.0f));
    return material;
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
            if (window && window->horizontal_drag_cursor()) {
                SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                return TRUE;
            }
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
        case WM_CHAR:
            if (window) {
                window->on_text_input(static_cast<wchar_t>(wparam));
                return 0;
            }
            break;
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
            if (window) {
                window->on_mouse_capture_changed();
            }
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

    struct GameplayBackgroundBitmapEntry {
        std::size_t decoded_bytes = 0;
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
    std::wstring ui_font_family;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> title_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> option_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> body_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> mono_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> logo_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> menu_button_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> menu_icon_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> header_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> gameplay_combo_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> song_logo_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> song_nav_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> song_record_label_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> song_record_value_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> song_record_detail_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> song_title_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> song_artist_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> result_score_format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> result_metric_format;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> generic_help_layout;
    std::wstring generic_help_text;
    float generic_help_width = 0.0f;
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
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> lane_divider_brush;
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
    std::array<Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>, kGameplayHudMaxLanes>
        lane_native_note_brushes{};
    std::array<Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>, kGameplayHudMaxLanes>
        lane_native_hold_brushes{};
    std::array<Microsoft::WRL::ComPtr<ID2D1Bitmap>, kGameplayHudMaxLanes> lane_key_idle_bitmaps{};
    std::array<D2D1_RECT_F, kGameplayHudMaxLanes> lane_key_idle_source_rects{};
    std::array<Microsoft::WRL::ComPtr<ID2D1Bitmap>, kGameplayHudMaxLanes> lane_key_pressed_bitmaps{};
    std::array<D2D1_RECT_F, kGameplayHudMaxLanes> lane_key_pressed_source_rects{};
    Microsoft::WRL::ComPtr<ID2D1Bitmap> gameplay_gear_overlay_bitmap;
    D2D1_RECT_F gameplay_gear_overlay_source_rect{};
    Microsoft::WRL::ComPtr<ID2D1Bitmap> song_select_preview_bitmap;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> gameplay_background_base_bitmap;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> gameplay_background_overlay_bitmap;
    std::unordered_map<std::string, GameplayBackgroundBitmapEntry>
        gameplay_background_bitmaps{};
    std::deque<std::string> gameplay_background_lru{};
    std::size_t gameplay_background_bitmap_bytes = 0;
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
    std::array<Microsoft::WRL::ComPtr<ID2D1PathGeometry>, 2> gameplay_gauge_grid_geometries{};
    // Indexed by gameplay_note_polygon_index: triangle, pentagon, hexagon, diamond, arrow.
    std::array<Microsoft::WRL::ComPtr<ID2D1PathGeometry>, 5> gameplay_note_shape_geometries{};
};

MenuWindow::MenuWindow()
    : d2d_(std::make_unique<D2DResources>()),
      gameplay_base_video_decoder_(std::make_unique<BgaVideoDecoder>()),
      gameplay_overlay_video_decoder_(std::make_unique<BgaVideoDecoder>()),
      gameplay_base_image_loader_(std::make_unique<BgaImageLoader>()),
      gameplay_overlay_image_loader_(std::make_unique<BgaImageLoader>()),
      gameplay_background_upscaler_(std::make_unique<OnnxBackgroundUpscaler>()),
      gameplay_overlay_background_upscaler_(std::make_unique<OnnxBackgroundUpscaler>()),
      song_select_background_upscaler_(std::make_unique<OnnxBackgroundUpscaler>()) {}

MenuWindow::~MenuWindow() {
    shutdown();
}

void MenuWindow::shutdown() {
    initialized_.store(false, std::memory_order_release);
    init_success_.store(false, std::memory_order_release);
    screenshot_requested_.store(false, std::memory_order_release);
    last_present_completion_ns_.store(0, std::memory_order_release);
    update_cursor_visibility(false);
    clear_song_scrollbar_state();
    hit_regions_.clear();

    if (d2d_) {
        if (d2d_->swap_chain && fullscreen_) {
            d2d_->swap_chain->SetFullscreenState(FALSE, nullptr);
        }
        if (d2d_->d2d_context) {
            d2d_->d2d_context->SetTarget(nullptr);
        }
    }

    fullscreen_ = false;
    fullscreen_restore_pending_ = false;
    invalidate_gameplay_note_sprite_cache();
    invalidate_gameplay_background_cache();
    invalidate_song_select_preview_cache();
    clear_song_card_preview_cache();
    invalidate_gameplay_static_cache();

    destroy_window();
    d2d_ = std::make_unique<D2DResources>();

    if (com_initialized_) {
        CoUninitialize();
        com_initialized_ = false;
    }
}

void MenuWindow::destroy_window() {
    HWND hwnd = static_cast<HWND>(hwnd_);
    hwnd_ = nullptr;
    if (!hwnd) {
        return;
    }

    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
    DragAcceptFiles(hwnd, FALSE);
    if (IsWindow(hwnd)) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        DestroyWindow(hwnd);
    }
}

bool MenuWindow::initialize(const MenuWindowConfig& config) {
    shutdown();

    config_ = config;
    pending_config_ = config;
    config_dirty_ = false;
    should_close_.store(false, std::memory_order_release);
    fatal_error_.store(false, std::memory_order_release);
    screenshot_requested_.store(false, std::memory_order_release);

    const HRESULT com_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(com_hr)) {
        com_initialized_ = true;
    } else if (com_hr != RPC_E_CHANGED_MODE) {
        return fail_fatal("Failed to initialize COM for the menu renderer.");
    }

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    window_class.lpfnWndProc = menu_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClassName;
    if (RegisterClassExW(&window_class) == 0) {
        const DWORD register_error = GetLastError();
        if (register_error != ERROR_CLASS_ALREADY_EXISTS) {
            return fail_fatal("Failed to register the menu window class.");
        }
    }

    const MonitorDisplayInfo monitor = query_monitor_display_info(nullptr);
    UINT client_width = 0;
    UINT client_height = 0;
    int window_x = 0;
    int window_y = 0;
    resolve_window_bounds(config_, monitor, client_width, client_height, window_x, window_y);

    const DWORD style = window_style_for_display_mode(config_.display_mode);
    const DWORD ex_style = window_ex_style_for_display_mode(config_.display_mode);
    const SIZE window_size = window_size_for_client_area(client_width, client_height, style, ex_style);
    const std::wstring title = to_wide(config_.title);
    HWND hwnd = CreateWindowExW(ex_style,
                                kWindowClassName,
                                title.empty() ? L"TenRiff" : title.c_str(),
                                style,
                                window_x,
                                window_y,
                                window_size.cx,
                                window_size.cy,
                                nullptr,
                                nullptr,
                                instance,
                                this);
    if (!hwnd) {
        return fail_fatal("Failed to create the menu window.");
    }

    hwnd_ = hwnd;
    width_ = client_width;
    height_ = client_height;

    UINT device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    const HRESULT device_hr = D3D11CreateDevice(nullptr,
                                                D3D_DRIVER_TYPE_HARDWARE,
                                                nullptr,
                                                device_flags,
                                                feature_levels,
                                                static_cast<UINT>(std::size(feature_levels)),
                                                D3D11_SDK_VERSION,
                                                d2d_->device.ReleaseAndGetAddressOf(),
                                                &feature_level,
                                                d2d_->context.ReleaseAndGetAddressOf());
    if (FAILED(device_hr) || !d2d_->device || !d2d_->context) {
        destroy_window();
        return fail_fatal("Failed to create a Direct3D 11 hardware device.");
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    if (FAILED(d2d_->device.As(&dxgi_device)) || !dxgi_device) {
        destroy_window();
        return fail_fatal("Failed to query IDXGIDevice from the Direct3D device.");
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    if (FAILED(dxgi_device->GetAdapter(&adapter)) ||
        FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))) ||
        !factory) {
        destroy_window();
        return fail_fatal("Failed to create the DXGI swap-chain factory.");
    }

    if (adapter) {
        DXGI_ADAPTER_DESC adapter_desc{};
        if (SUCCEEDED(adapter->GetDesc(&adapter_desc))) {
            std::cerr << "[MenuWindow] Using GPU adapter: "
                      << wide_to_utf8(std::wstring(adapter_desc.Description)) << std::endl;
        }
    }

    swap_chain_flags_ = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
    BOOL allow_tearing = FALSE;
    if (SUCCEEDED(factory.As(&factory5)) && factory5) {
        factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                      &allow_tearing,
                                      sizeof(allow_tearing));
    }
    if (allow_tearing) {
        swap_chain_flags_ |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
    swap_chain_desc.Width = width_;
    swap_chain_desc.Height = height_;
    swap_chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swap_chain_desc.Flags = swap_chain_flags_;

    const HRESULT swap_chain_hr =
        factory->CreateSwapChainForHwnd(d2d_->device.Get(),
                                        hwnd,
                                        &swap_chain_desc,
                                        nullptr,
                                        nullptr,
                                        d2d_->swap_chain.ReleaseAndGetAddressOf());
    if (FAILED(swap_chain_hr) || !d2d_->swap_chain) {
        destroy_window();
        return fail_fatal("Failed to create the DXGI swap chain.");
    }

    static_cast<void>(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    configure_low_latency_presentation(dxgi_device.Get(), d2d_->swap_chain.Get());

    const D2D1_FACTORY_OPTIONS d2d_options{};
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                 __uuidof(ID2D1Factory1),
                                 &d2d_options,
                                 reinterpret_cast<void**>(d2d_->d2d_factory.ReleaseAndGetAddressOf()))) ||
        !d2d_->d2d_factory) {
        destroy_window();
        return fail_fatal("Failed to create the Direct2D factory.");
    }
    // A regular quad with its first vertex at the top is a diamond, so the shared
    // polygon builder covers it; the arrow needs its own path.
    constexpr std::array<int, 4> kNotePolygonSides{3, 5, 6, 4};
    for (std::size_t index = 0; index < kNotePolygonSides.size(); ++index) {
        static_cast<void>(create_unit_note_polygon_geometry(
            d2d_->d2d_factory.Get(),
            kNotePolygonSides[index],
            d2d_->gameplay_note_shape_geometries[index].ReleaseAndGetAddressOf()));
    }
    static_cast<void>(create_unit_note_arrow_geometry(
        d2d_->d2d_factory.Get(),
        d2d_->gameplay_note_shape_geometries[4].ReleaseAndGetAddressOf()));
    if (FAILED(d2d_->d2d_factory->CreateDevice(dxgi_device.Get(), d2d_->d2d_device.ReleaseAndGetAddressOf())) ||
        !d2d_->d2d_device ||
        FAILED(d2d_->d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                                     d2d_->d2d_context.ReleaseAndGetAddressOf())) ||
        !d2d_->d2d_context) {
        destroy_window();
        return fail_fatal("Failed to create the Direct2D device context.");
    }

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                   __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(d2d_->dwrite_factory.ReleaseAndGetAddressOf()))) ||
        !d2d_->dwrite_factory) {
        destroy_window();
        return fail_fatal("Failed to create the DirectWrite factory.");
    }
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory,
                                nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(d2d_->wic_factory.ReleaseAndGetAddressOf()))) ||
        !d2d_->wic_factory) {
        destroy_window();
        return fail_fatal("Failed to create the WIC imaging factory.");
    }

    if (!create_text_formats(L"Segoe UI")) {
        destroy_window();
        return fail_fatal("Failed to create one or more text formats.");
    }

    if (!recreate_targets()) {
        destroy_window();
        return fail_fatal("Failed to create the initial Direct2D render target.");
    }

    update_layout();
    update_brushes();

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    DragAcceptFiles(hwnd, TRUE);

    if (config_.display_mode == "fullscreen") {
        if (!enter_fullscreen_mode(width_, height_, "MenuWindow::initialize")) {
            if (fullscreen_restore_pending_) {
                std::cerr << "[MenuWindow::initialize] Initial fullscreen transition will retry on the next frame."
                          << std::endl;
            }
        }
    }

    initialized_.store(true, std::memory_order_release);
    return true;
}

void MenuWindow::set_config(const MenuWindowConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    pending_config_ = config;
    config_dirty_ = true;
}

bool MenuWindow::fail_fatal(std::string_view message) {
    fatal_error_.store(true, std::memory_order_release);
    should_close_.store(true, std::memory_order_release);
    std::string fatal_message(message);
    if (append_renderer_fatal_log(message)) {
        fatal_message += "\n\nDiagnostic log written to logs/run.log next to TenRiff.exe.";
    }
    show_fatal_error(fatal_message);
    return false;
}

void MenuWindow::invalidate_gameplay_note_sprite_cache() {
    gameplay_note_sprite_cache_ = GameplayNoteSpriteCache{};
    if (!d2d_) {
        return;
    }

    for (auto& bitmap : d2d_->lane_note_head_bitmaps) {
        bitmap.Reset();
    }
    for (auto& rect : d2d_->lane_note_head_source_rects) {
        rect = D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    }
    for (auto& bitmap : d2d_->lane_note_hold_head_bitmaps) {
        bitmap.Reset();
    }
    for (auto& rect : d2d_->lane_note_hold_head_source_rects) {
        rect = D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    }
    for (auto& bitmap : d2d_->lane_note_hold_body_bitmaps) {
        bitmap.Reset();
    }
    for (auto& rect : d2d_->lane_note_hold_body_source_rects) {
        rect = D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    }
    for (auto& bitmap : d2d_->lane_note_tail_bitmaps) {
        bitmap.Reset();
    }
    for (auto& rect : d2d_->lane_note_tail_source_rects) {
        rect = D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    }
    for (auto& brush : d2d_->lane_native_note_brushes) {
        brush.Reset();
    }
    for (auto& brush : d2d_->lane_native_hold_brushes) {
        brush.Reset();
    }
    for (auto& bitmap : d2d_->lane_key_idle_bitmaps) {
        bitmap.Reset();
    }
    for (auto& rect : d2d_->lane_key_idle_source_rects) {
        rect = D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    }
    for (auto& bitmap : d2d_->lane_key_pressed_bitmaps) {
        bitmap.Reset();
    }
    for (auto& rect : d2d_->lane_key_pressed_source_rects) {
        rect = D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    }
    d2d_->gameplay_gear_overlay_bitmap.Reset();
    d2d_->gameplay_gear_overlay_source_rect =
        D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
}

bool MenuWindow::ensure_gameplay_note_sprites(const GameplayHudData& data) {
    if (!d2d_ || !d2d_->wic_factory || !d2d_->d2d_context) {
        return false;
    }

    const int lane_count = std::clamp(data.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
    const std::string source = normalize_gameplay_skin_source(data.skin_source);
    const std::string note_shape = normalize_gameplay_note_shape(data.note_shape);
    const bool cache_matches =
        gameplay_note_sprite_cache_.lane_count == lane_count &&
        gameplay_note_sprite_cache_.note_border_enabled == data.note_border_enabled &&
        gameplay_note_sprite_cache_.note_shape == note_shape &&
        gameplay_note_sprite_cache_.note_image_aspect == data.note_image_aspect &&
        gameplay_note_sprite_cache_.skin_source == source &&
        gameplay_note_sprite_cache_.external_skin_root == data.external_skin_root &&
        gameplay_note_sprite_cache_.external_skin_name == data.external_skin_name &&
        gameplay_note_sprite_cache_.skin_revision == data.skin_revision &&
        gameplay_note_sprite_cache_.lr2_resolution_override == data.lr2_resolution_override &&
        gameplay_note_sprite_cache_.lane_colors == data.lane_colors;
    if (cache_matches) {
        return true;
    }

    invalidate_gameplay_note_sprite_cache();
    gameplay_note_sprite_cache_.lane_count = lane_count;
    gameplay_note_sprite_cache_.note_border_enabled = data.note_border_enabled;
    gameplay_note_sprite_cache_.note_shape = note_shape;
    gameplay_note_sprite_cache_.note_image_aspect = data.note_image_aspect;
    gameplay_note_sprite_cache_.skin_source = source;
    gameplay_note_sprite_cache_.external_skin_root = data.external_skin_root;
    gameplay_note_sprite_cache_.external_skin_name = data.external_skin_name;
    gameplay_note_sprite_cache_.skin_revision = data.skin_revision;
    gameplay_note_sprite_cache_.lr2_resolution_override = data.lr2_resolution_override;
    gameplay_note_sprite_cache_.lane_colors = data.lane_colors;
    gameplay_note_sprite_cache_.has_imported_note_aspect = false;
    gameplay_note_sprite_cache_.imported_note_aspect = NoteImageAspect::Stretch;
    gameplay_note_sprite_cache_.note_rotation_count = 0;
    gameplay_note_sprite_cache_.key_rotation_count = 0;
    gameplay_note_sprite_cache_.has_gear_placement = false;

    // Native and partial-import fallbacks share cached lane gradients. This keeps the material
    // upgrade inside the existing single fill pass instead of adding a highlight primitive per note.
    for (int lane = 0; lane < lane_count; ++lane) {
        const std::size_t index = static_cast<std::size_t>(lane);
        uint32_t lane_color = 0xF6F8FF;
        if (index < data.lane_color_count) {
            lane_color = data.lane_colors[index];
        } else if (!gameplay_lane_uses_white_note(lane + 1)) {
            lane_color = 0x4F80FF;
        }

        const D2D1_GRADIENT_STOP note_stops[] = {
            {0.00f, color_from_rgb(blend_rgb(lane_color, 0xFFFFFF, 0.78f))},
            {0.24f, color_from_rgb(blend_rgb(lane_color, 0xFFFFFF, 0.34f))},
            {0.62f, color_from_rgb(lane_color)},
            {1.00f, color_from_rgb(blend_rgb(lane_color, 0x07131E, 0.22f))},
        };
        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> note_stop_collection;
        if (SUCCEEDED(d2d_->d2d_context->CreateGradientStopCollection(
                note_stops, 4, note_stop_collection.ReleaseAndGetAddressOf()))) {
            const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES properties =
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, 0.0f),
                                                    D2D1::Point2F(0.0f, 1.0f));
            static_cast<void>(d2d_->d2d_context->CreateLinearGradientBrush(
                properties,
                note_stop_collection.Get(),
                d2d_->lane_native_note_brushes[index].ReleaseAndGetAddressOf()));
        }

        const D2D1_GRADIENT_STOP hold_stops[] = {
            {0.00f, color_from_rgb(blend_rgb(lane_color, 0xFFFFFF, 0.14f))},
            {0.50f, color_from_rgb(blend_rgb(lane_color, 0xFFFFFF, 0.72f))},
            {1.00f, color_from_rgb(blend_rgb(lane_color, 0xFFFFFF, 0.14f))},
        };
        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> hold_stop_collection;
        if (SUCCEEDED(d2d_->d2d_context->CreateGradientStopCollection(
                hold_stops, 3, hold_stop_collection.ReleaseAndGetAddressOf()))) {
            const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES properties =
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, 0.0f),
                                                    D2D1::Point2F(1.0f, 0.0f));
            static_cast<void>(d2d_->d2d_context->CreateLinearGradientBrush(
                properties,
                hold_stop_collection.Get(),
                d2d_->lane_native_hold_brushes[index].ReleaseAndGetAddressOf()));
        }
    }

    if (source == "native") {
        return true;
    }

    app::ImportedGameplaySkinDefinition imported;
    if (source == "tenriff") {
        if (data.resolved_tenriff_skin) {
            imported = *data.resolved_tenriff_skin;
        }
    } else {
        imported = app::resolve_imported_gameplay_skin(source,
                                                       data.external_skin_root,
                                                       data.external_skin_name,
                                                       lane_count,
                                                       data.lr2_resolution_override);
    }
    if (!imported.found) {
        return true;
    }

    gameplay_note_sprite_cache_.use_full_lane_receptor_layout = imported.use_full_lane_receptor_layout;
    gameplay_note_sprite_cache_.has_gear_placement =
        imported.gear_placement.valid && std::isfinite(imported.gear_placement.width) &&
        std::isfinite(imported.gear_placement.height) &&
        imported.gear_placement.width > 0.0f && imported.gear_placement.height > 0.0f &&
        std::isfinite(imported.gear_placement.offset_x) &&
        std::isfinite(imported.gear_placement.offset_y);
    gameplay_note_sprite_cache_.gear_placement_offset_x = imported.gear_placement.offset_x;
    gameplay_note_sprite_cache_.gear_placement_offset_y = imported.gear_placement.offset_y;
    gameplay_note_sprite_cache_.gear_placement_width = imported.gear_placement.width;
    gameplay_note_sprite_cache_.gear_placement_height = imported.gear_placement.height;
    gameplay_note_sprite_cache_.imported_note_width_ratio = imported.imported_note_width_ratio;
    gameplay_note_sprite_cache_.imported_note_height_ratio = imported.imported_note_height_ratio;
    if (!imported.note_aspect.empty()) {
        gameplay_note_sprite_cache_.has_imported_note_aspect = true;
        gameplay_note_sprite_cache_.imported_note_aspect =
            imported.note_aspect == "width"
                ? NoteImageAspect::Width
                : (imported.note_aspect == "contain" ? NoteImageAspect::Contain
                                                     : NoteImageAspect::Stretch);
    }
    gameplay_note_sprite_cache_.note_rotation_count = std::min(
        imported.note_rotations.size(), gameplay_note_sprite_cache_.note_rotations.size());
    for (std::size_t i = 0; i < gameplay_note_sprite_cache_.note_rotation_count; ++i) {
        gameplay_note_sprite_cache_.note_rotations[i] = imported.note_rotations[i];
    }
    // key_rotations defaults to note_rotations: arrow receptors point the same
    // way as the notes falling into them unless the skin says otherwise.
    const std::vector<float>& key_rotation_source =
        imported.key_rotations.empty() ? imported.note_rotations : imported.key_rotations;
    gameplay_note_sprite_cache_.key_rotation_count =
        std::min(key_rotation_source.size(), gameplay_note_sprite_cache_.key_rotations.size());
    for (std::size_t i = 0; i < gameplay_note_sprite_cache_.key_rotation_count; ++i) {
        gameplay_note_sprite_cache_.key_rotations[i] = key_rotation_source[i];
    }
    gameplay_note_sprite_cache_.lane_divider_width_count =
        std::min(imported.lane_divider_widths.size(), gameplay_note_sprite_cache_.lane_divider_widths.size());
    for (std::size_t i = 0; i < gameplay_note_sprite_cache_.lane_divider_width_count; ++i) {
        gameplay_note_sprite_cache_.lane_divider_widths[i] = imported.lane_divider_widths[i];
    }

    if (imported.column_widths.size() == static_cast<std::size_t>(lane_count)) {
        double total_width = 0.0;
        std::size_t valid_width_count = 0;
        for (float width : imported.column_widths) {
            if (std::isfinite(width) && width > 0.0f) {
                total_width += width;
                ++valid_width_count;
            }
        }
        const double average_width =
            valid_width_count > 0 ? total_width / static_cast<double>(valid_width_count) : 0.0;
        if (average_width > 0.0) {
            gameplay_note_sprite_cache_.imported_lane_width_scale_count =
                imported.column_widths.size();
            for (std::size_t lane = 0; lane < imported.column_widths.size(); ++lane) {
                gameplay_note_sprite_cache_.imported_lane_width_scales[lane] =
                    std::clamp(static_cast<double>(imported.column_widths[lane]) / average_width,
                               kGameplayLaneWidthScaleMin,
                               kGameplayLaneWidthScaleMax);
            }
            if (imported.column_spacings.size() == static_cast<std::size_t>(lane_count - 1)) {
                gameplay_note_sprite_cache_.imported_lane_spacing_scale_count =
                    imported.column_spacings.size();
                for (std::size_t gap = 0; gap < imported.column_spacings.size(); ++gap) {
                    gameplay_note_sprite_cache_.imported_lane_spacing_scales[gap] =
                        std::clamp(static_cast<double>(imported.column_spacings[gap]) / average_width,
                                   kGameplayLaneSpacingScaleMin,
                                   kGameplayLaneSpacingScaleMax);
                }
            }
        }
    }
    if (imported.has_hit_position && std::isfinite(imported.hit_position)) {
        gameplay_note_sprite_cache_.has_imported_judgement_line_position = true;
        gameplay_note_sprite_cache_.imported_judgement_line_position =
            clamp_gameplay_judgement_line(static_cast<double>(imported.hit_position) / 480.0);
    }

    auto pick_asset = [](const std::vector<app::ImportedSkinImageAsset>& assets,
                         int lane_index) -> const app::ImportedSkinImageAsset* {
        if (assets.empty() || lane_index < 0) {
            return nullptr;
        }
        if (assets.size() == 1) {
            return &assets.front();
        }
        const std::size_t index = std::min<std::size_t>(static_cast<std::size_t>(lane_index), assets.size() - 1);
        return &assets[index];
    };
    auto load_asset = [&](const std::vector<app::ImportedSkinImageAsset>& assets,
                          int lane_index,
                          Microsoft::WRL::ComPtr<ID2D1Bitmap>* bitmap_out,
                          D2D1_RECT_F* rect_out) {
        const app::ImportedSkinImageAsset* asset = pick_asset(assets, lane_index);
        if (!asset || !bitmap_out || !rect_out) {
            return;
        }
        load_bitmap_from_imported_asset(d2d_->wic_factory.Get(),
                                        d2d_->d2d_context.Get(),
                                        *asset,
                                        *bitmap_out,
                                        rect_out,
                                        true);
    };

    if (!imported.gear_overlay_image.path.empty()) {
        load_bitmap_from_imported_asset(d2d_->wic_factory.Get(),
                                        d2d_->d2d_context.Get(),
                                        imported.gear_overlay_image,
                                        d2d_->gameplay_gear_overlay_bitmap,
                                        &d2d_->gameplay_gear_overlay_source_rect,
                                        false);
    }

    for (int lane = 0; lane < lane_count; ++lane) {
        const std::size_t index = static_cast<std::size_t>(lane);
        load_asset(imported.note_images, lane, &d2d_->lane_note_head_bitmaps[index], &d2d_->lane_note_head_source_rects[index]);
        load_asset(imported.hold_head_images, lane, &d2d_->lane_note_hold_head_bitmaps[index], &d2d_->lane_note_hold_head_source_rects[index]);
        load_asset(imported.hold_body_images, lane, &d2d_->lane_note_hold_body_bitmaps[index], &d2d_->lane_note_hold_body_source_rects[index]);
        load_asset(imported.hold_tail_images, lane, &d2d_->lane_note_tail_bitmaps[index], &d2d_->lane_note_tail_source_rects[index]);
        load_asset(imported.key_images, lane, &d2d_->lane_key_idle_bitmaps[index], &d2d_->lane_key_idle_source_rects[index]);
        load_asset(imported.key_pressed_images, lane, &d2d_->lane_key_pressed_bitmaps[index], &d2d_->lane_key_pressed_source_rects[index]);
    }

    return true;
}

void MenuWindow::invalidate_gameplay_background_cache() {
    gameplay_background_cache_ = GameplayBackgroundCache{};
    if (gameplay_base_video_decoder_) {
        gameplay_base_video_decoder_->clear();
    }
    if (gameplay_overlay_video_decoder_) {
        gameplay_overlay_video_decoder_->clear();
    }
    if (gameplay_base_image_loader_) {
        gameplay_base_image_loader_->clear();
    }
    if (gameplay_overlay_image_loader_) {
        gameplay_overlay_image_loader_->clear();
    }
    if (gameplay_background_upscaler_) {
        gameplay_background_upscaler_->clear();
    }
    if (gameplay_overlay_background_upscaler_) {
        gameplay_overlay_background_upscaler_->clear();
    }
    if (d2d_) {
        d2d_->gameplay_background_base_bitmap.Reset();
        d2d_->gameplay_background_overlay_bitmap.Reset();
    }
}

void MenuWindow::set_background_upscale_model_path(std::string model_path, bool prefer_npu) {
    if (active_background_upscale_model_path_ == model_path &&
        active_background_upscale_prefer_npu_ == prefer_npu) {
        return;
    }

    active_background_upscale_model_path_ = std::move(model_path);
    active_background_upscale_prefer_npu_ = prefer_npu;
    gameplay_background_upscaler_ =
        std::make_unique<OnnxBackgroundUpscaler>(active_background_upscale_model_path_, prefer_npu);
    gameplay_overlay_background_upscaler_ =
        std::make_unique<OnnxBackgroundUpscaler>(active_background_upscale_model_path_, prefer_npu);
    song_select_background_upscaler_ =
        std::make_unique<OnnxBackgroundUpscaler>(active_background_upscale_model_path_, prefer_npu);
    gameplay_background_cache_ = GameplayBackgroundCache{};
    song_select_preview_cache_ = SongSelectPreviewCache{};
    if (d2d_) {
        d2d_->gameplay_background_base_bitmap.Reset();
        d2d_->gameplay_background_overlay_bitmap.Reset();
        d2d_->song_select_preview_bitmap.Reset();
    }
}

bool MenuWindow::ensure_gameplay_background_bitmap(const GameplayHudData& data) {
    set_background_upscale_model_path(
        data.background_upscale_mode == "onnx" ? data.background_upscale_model_path : "",
        data.background_upscale_prefer_npu);
    if (!d2d_ || !d2d_->wic_factory || !d2d_->d2d_context ||
        !gameplay_base_video_decoder_ || !gameplay_overlay_video_decoder_ ||
        !gameplay_base_image_loader_ || !gameplay_overlay_image_loader_ ||
        !gameplay_background_upscaler_ || !gameplay_overlay_background_upscaler_) {
        return false;
    }

    const std::string upscale_mode =
        data.background_upscale_mode == "onnx" ? "onnx" : "off";
    const auto touch_background_bitmap = [&](const std::string& path) {
        auto& lru = d2d_->gameplay_background_lru;
        const auto existing = std::find(lru.begin(), lru.end(), path);
        if (existing != lru.end()) {
            lru.erase(existing);
        }
        lru.push_back(path);
    };
    const auto use_cached_background_bitmap =
        [&](const std::string& path, Microsoft::WRL::ComPtr<ID2D1Bitmap>& destination) {
            const auto found = d2d_->gameplay_background_bitmaps.find(path);
            if (found == d2d_->gameplay_background_bitmaps.end() || !found->second.bitmap) {
                return false;
            }
            destination = found->second.bitmap;
            touch_background_bitmap(path);
            return true;
        };
    const bool paths_changed =
        gameplay_background_cache_.base_path != data.background_base_path ||
        gameplay_background_cache_.overlay_path != data.background_overlay_path ||
        gameplay_background_cache_.base_start_sample != data.background_base_start_sample ||
        gameplay_background_cache_.overlay_start_sample != data.background_overlay_start_sample;
    const bool mode_changed = gameplay_background_cache_.upscale_mode != upscale_mode;
    if (paths_changed || mode_changed) {
        gameplay_base_video_decoder_->clear();
        gameplay_overlay_video_decoder_->clear();
        gameplay_base_image_loader_->clear();
        gameplay_overlay_image_loader_->clear();
        gameplay_background_upscaler_->clear();
        gameplay_overlay_background_upscaler_->clear();
        gameplay_background_cache_ = GameplayBackgroundCache{};
        gameplay_background_cache_.base_path = data.background_base_path;
        gameplay_background_cache_.overlay_path = data.background_overlay_path;
        gameplay_background_cache_.base_start_sample = data.background_base_start_sample;
        gameplay_background_cache_.overlay_start_sample = data.background_overlay_start_sample;
        gameplay_background_cache_.upscale_mode = upscale_mode;

        const auto prepare_static_layer =
            [&](const std::string& path,
                BgaImageLoader* loader,
                Microsoft::WRL::ComPtr<ID2D1Bitmap>& destination) {
                if (path.empty() || BgaVideoDecoder::is_supported_video_path(path)) {
                    destination.Reset();
                    return;
                }
                if (!use_cached_background_bitmap(path, destination)) {
                    // Keep the previous frame visible while the new image is
                    // decoded. The render thread never performs file I/O here.
                    loader->request(path);
                }
            };
        prepare_static_layer(data.background_base_path,
                             gameplay_base_image_loader_.get(),
                             d2d_->gameplay_background_base_bitmap);
        prepare_static_layer(data.background_overlay_path,
                             gameplay_overlay_image_loader_.get(),
                             d2d_->gameplay_background_overlay_bitmap);
    }

    const auto upload_bgra = [&](std::uint32_t width,
                                 std::uint32_t height,
                                 const std::vector<std::uint8_t>& bgra,
                                 Microsoft::WRL::ComPtr<ID2D1Bitmap>& destination) {
        if (width == 0 || height == 0 ||
            bgra.size() != static_cast<std::size_t>(width) * height * 4) {
            return false;
        }
        const D2D1_BITMAP_PROPERTIES properties = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_PREMULTIPLIED));
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
        const HRESULT bitmap_hr = d2d_->d2d_context->CreateBitmap(
            D2D1::SizeU(width, height),
            bgra.data(),
            width * 4,
            &properties,
            bitmap.ReleaseAndGetAddressOf());
        if (FAILED(bitmap_hr) || !bitmap) {
            return false;
        }
        destination = std::move(bitmap);
        return true;
    };

    const auto cache_ready_image =
        [&](BgaImageLoader* loader,
            const std::string& active_path,
            Microsoft::WRL::ComPtr<ID2D1Bitmap>& destination) {
            const auto frame = loader ? loader->take_ready() : nullptr;
            if (!frame) {
                return;
            }
            if (frame->bgra.empty()) {
                if (frame->source_path == active_path) {
                    destination.Reset();
                }
                return;
            }

            Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
            if (!upload_bgra(frame->width, frame->height, frame->bgra, bitmap)) {
                return;
            }
            constexpr std::size_t kMaxCachedBackgroundBitmaps = 256;
            constexpr std::size_t kMaxCachedBackgroundBytes = 192ull * 1024ull * 1024ull;
            auto existing = d2d_->gameplay_background_bitmaps.find(frame->source_path);
            if (existing != d2d_->gameplay_background_bitmaps.end()) {
                d2d_->gameplay_background_bitmap_bytes -= existing->second.decoded_bytes;
                existing->second = D2DResources::GameplayBackgroundBitmapEntry{
                    frame->bgra.size(), bitmap};
            } else {
                d2d_->gameplay_background_bitmaps.emplace(
                    frame->source_path,
                    D2DResources::GameplayBackgroundBitmapEntry{frame->bgra.size(), bitmap});
            }
            d2d_->gameplay_background_bitmap_bytes += frame->bgra.size();
            touch_background_bitmap(frame->source_path);

            while ((!d2d_->gameplay_background_lru.empty()) &&
                   (d2d_->gameplay_background_bitmaps.size() > kMaxCachedBackgroundBitmaps ||
                    d2d_->gameplay_background_bitmap_bytes > kMaxCachedBackgroundBytes)) {
                const std::string oldest = d2d_->gameplay_background_lru.front();
                d2d_->gameplay_background_lru.pop_front();
                const auto victim = d2d_->gameplay_background_bitmaps.find(oldest);
                if (victim == d2d_->gameplay_background_bitmaps.end()) {
                    continue;
                }
                d2d_->gameplay_background_bitmap_bytes -= victim->second.decoded_bytes;
                d2d_->gameplay_background_bitmaps.erase(victim);
            }

            if (frame->source_path == active_path) {
                destination = std::move(bitmap);
            }
        };

    cache_ready_image(gameplay_base_image_loader_.get(),
                      gameplay_background_cache_.base_path,
                      d2d_->gameplay_background_base_bitmap);
    cache_ready_image(gameplay_overlay_image_loader_.get(),
                      gameplay_background_cache_.overlay_path,
                      d2d_->gameplay_background_overlay_bitmap);

    const auto process_video_layer = [&](const std::string& path,
                                         int64_t start_sample,
                                         BgaVideoDecoder* decoder,
                                         OnnxBackgroundUpscaler* upscaler,
                                         Microsoft::WRL::ComPtr<ID2D1Bitmap>& bitmap,
                                         std::string& requested_key,
                                         std::string& upscaled_key,
                                         bool& is_upscaled) {
        if (!decoder || !upscaler || !BgaVideoDecoder::is_supported_video_path(path)) {
            return;
        }
        const bool upscale_realtime_video =
            OnnxBackgroundUpscaler::should_upscale_realtime_video(upscale_mode);
        const double sample_rate = static_cast<double>(std::max(1, data.sample_rate));
        const double playback_rate = std::clamp(data.rate, 0.05, 4.0);
        const double position_seconds =
            std::max<int64_t>(0, data.current_sample - start_sample) / sample_rate * playback_rate;
        decoder->request(path, position_seconds);
        if (const auto frame = decoder->take_ready();
            frame && frame->source_path == path && !frame->bgra.empty()) {
            if (upload_bgra(frame->width, frame->height, frame->bgra, bitmap)) {
                is_upscaled = false;
                upscaled_key.clear();
                const std::string frame_key =
                    path + "|mf|" + std::to_string(frame->timestamp_100ns);
                if (upscale_realtime_video &&
                    OnnxBackgroundUpscaler::should_upscale(
                        frame->width, frame->height, upscale_mode)) {
                    // Keep one video inference in flight. Replacing the request id every
                    // decoded frame made every slower-than-video GPU result look stale.
                    if (upscaler->request_bgra(frame_key,
                                               frame->width,
                                               frame->height,
                                               frame->bgra)) {
                        requested_key = frame_key;
                    }
                } else {
                    requested_key.clear();
                    upscaler->clear();
                }
            }
        }
        if (!upscale_realtime_video) {
            return;
        }
        if (const auto frame = upscaler->take_ready();
            frame && frame->source_path == requested_key && !frame->bgra.empty() &&
            upload_bgra(frame->width, frame->height, frame->bgra, bitmap)) {
            is_upscaled = true;
            upscaled_key = frame->source_path;
        }
    };

    process_video_layer(gameplay_background_cache_.base_path,
                        gameplay_background_cache_.base_start_sample,
                        gameplay_base_video_decoder_.get(),
                        gameplay_background_upscaler_.get(),
                        d2d_->gameplay_background_base_bitmap,
                        gameplay_background_cache_.base_requested_key,
                        gameplay_background_cache_.base_upscaled_key,
                        gameplay_background_cache_.base_is_upscaled);
    process_video_layer(gameplay_background_cache_.overlay_path,
                        gameplay_background_cache_.overlay_start_sample,
                        gameplay_overlay_video_decoder_.get(),
                        gameplay_overlay_background_upscaler_.get(),
                        d2d_->gameplay_background_overlay_bitmap,
                        gameplay_background_cache_.overlay_requested_key,
                        gameplay_background_cache_.overlay_upscaled_key,
                        gameplay_background_cache_.overlay_is_upscaled);

    const auto process_static_layer = [&](const std::string& path,
                                          OnnxBackgroundUpscaler* upscaler,
                                          ID2D1Bitmap* bitmap,
                                          Microsoft::WRL::ComPtr<ID2D1Bitmap>& destination,
                                          std::string& requested_key,
                                          std::string& upscaled_key,
                                          bool& is_upscaled) {
        if (!upscaler || path.empty() || BgaVideoDecoder::is_supported_video_path(path)) {
            return;
        }
        if (bitmap && !is_upscaled && upscale_mode == "onnx") {
            const D2D1_SIZE_U size = bitmap->GetPixelSize();
            if (OnnxBackgroundUpscaler::should_upscale(size.width, size.height, upscale_mode) &&
                requested_key != path) {
                requested_key = path;
                upscaler->request(path);
            }
        }
        if (const auto frame = upscaler->take_ready();
            frame && frame->source_path == requested_key && !frame->bgra.empty() &&
            upload_bgra(frame->width, frame->height, frame->bgra, destination)) {
            is_upscaled = true;
            upscaled_key = frame->source_path;
        }
    };

    process_static_layer(gameplay_background_cache_.base_path,
                         gameplay_background_upscaler_.get(),
                         d2d_->gameplay_background_base_bitmap.Get(),
                         d2d_->gameplay_background_base_bitmap,
                         gameplay_background_cache_.base_requested_key,
                         gameplay_background_cache_.base_upscaled_key,
                         gameplay_background_cache_.base_is_upscaled);
    process_static_layer(gameplay_background_cache_.overlay_path,
                         gameplay_overlay_background_upscaler_.get(),
                         d2d_->gameplay_background_overlay_bitmap.Get(),
                         d2d_->gameplay_background_overlay_bitmap,
                         gameplay_background_cache_.overlay_requested_key,
                         gameplay_background_cache_.overlay_upscaled_key,
                         gameplay_background_cache_.overlay_is_upscaled);
    return true;
}
void MenuWindow::invalidate_song_select_preview_cache() {
    song_select_preview_cache_ = SongSelectPreviewCache{};
    if (song_select_background_upscaler_) {
        song_select_background_upscaler_->clear();
    }
    if (d2d_) {
        d2d_->song_select_preview_bitmap.Reset();
    }
}

bool MenuWindow::ensure_song_select_preview_bitmap(const SongSelectData& data) {
    set_background_upscale_model_path(
        data.background_upscale_mode == "onnx" ? data.background_upscale_model_path : "",
        data.background_upscale_prefer_npu);
    if (!d2d_ || !d2d_->wic_factory || !d2d_->d2d_context ||
        !song_select_background_upscaler_) {
        return false;
    }

    const std::string preview_path = data.selected_song_background_path;
    const std::string upscale_mode =
        data.background_upscale_mode == "onnx" ? "onnx" : "off";
    if (song_select_preview_cache_.path != preview_path ||
        song_select_preview_cache_.upscale_mode != upscale_mode) {
        invalidate_song_select_preview_cache();
        song_select_preview_cache_.path = preview_path;
        song_select_preview_cache_.upscale_mode = upscale_mode;
        song_select_preview_cache_.attempted = preview_path.empty();
    }

    if (upscale_mode == "onnx") {
        if (const auto frame = song_select_background_upscaler_->take_ready();
            frame && frame->source_path == preview_path && frame->width > 0 &&
            frame->height > 0 && !frame->bgra.empty()) {
            const D2D1_BITMAP_PROPERTIES properties = D2D1::BitmapProperties(
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                                  D2D1_ALPHA_MODE_PREMULTIPLIED));
            Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
            if (SUCCEEDED(d2d_->d2d_context->CreateBitmap(
                    D2D1::SizeU(frame->width, frame->height),
                    frame->bgra.data(),
                    frame->width * 4,
                    &properties,
                    bitmap.ReleaseAndGetAddressOf())) && bitmap) {
                d2d_->song_select_preview_bitmap = bitmap;
                song_select_preview_cache_.is_upscaled = true;
            }
        }
    }

    std::string desired_request;
    if (d2d_->song_select_preview_bitmap &&
        !song_select_preview_cache_.is_upscaled && upscale_mode == "onnx") {
        const D2D1_SIZE_U size = d2d_->song_select_preview_bitmap->GetPixelSize();
        if (OnnxBackgroundUpscaler::should_upscale(size.width, size.height, upscale_mode)) {
            desired_request = preview_path;
        }
    }
    if (desired_request != song_select_preview_cache_.requested_path) {
        song_select_preview_cache_.requested_path = desired_request;
        if (desired_request.empty()) {
            song_select_background_upscaler_->clear();
        } else {
            song_select_background_upscaler_->request(desired_request);
        }
    }
    return true;
}

void MenuWindow::clear_song_card_preview_cache() {
    if (!d2d_) {
        return;
    }
    d2d_->song_card_preview_bitmaps.clear();
    d2d_->song_card_preview_lru.clear();
}

void MenuWindow::update_song_select_preview_loading_state(const SongSelectData& data, int64_t now_ns) {
    if (data.showing_sources || data.showing_records) {
        song_select_preview_signature_.clear();
        song_select_preview_load_hold_until_ns_ = 0;
        return;
    }

    std::string signature;
    signature.reserve(64 + data.songs.size() * 12);
    signature += std::to_string(data.list_selected_index);
    signature.push_back('|');
    signature += std::to_string(data.list_window_start);
    signature.push_back('|');
    signature += data.selected_song_background_path;
    signature.push_back('|');
    for (const auto& song : data.songs) {
        signature += std::to_string(song.song_index);
        signature.push_back(',');
    }

    if (signature != song_select_preview_signature_) {
        song_select_preview_signature_ = std::move(signature);
        song_select_preview_load_hold_until_ns_ = now_ns + 150'000'000LL;
    }
}

bool MenuWindow::song_select_preview_loading_deferred(int64_t now_ns) const {
    return now_ns < song_select_preview_load_hold_until_ns_;
}

void MenuWindow::touch_song_card_preview_lru(std::string_view path) {
    if (!d2d_ || path.empty()) {
        return;
    }

    const std::string key(path);
    auto& lru = d2d_->song_card_preview_lru;
    lru.erase(std::remove(lru.begin(), lru.end(), key), lru.end());
    lru.push_back(key);
}

void MenuWindow::trim_song_card_preview_cache() {
    if (!d2d_) {
        return;
    }

    constexpr std::size_t kSongCardPreviewBitmapCacheLimit = 24;
    while (d2d_->song_card_preview_lru.size() > kSongCardPreviewBitmapCacheLimit) {
        const std::string evict_key = d2d_->song_card_preview_lru.front();
        d2d_->song_card_preview_lru.pop_front();
        d2d_->song_card_preview_bitmaps.erase(evict_key);
    }
}

ID2D1Bitmap* MenuWindow::find_song_card_preview_bitmap(std::string_view path) {
    if (!d2d_ || path.empty()) {
        return nullptr;
    }

    const std::string key(path);
    auto it = d2d_->song_card_preview_bitmaps.find(key);
    if (it == d2d_->song_card_preview_bitmaps.end()) {
        return nullptr;
    }

    touch_song_card_preview_lru(key);
    return it->second.bitmap.Get();
}

bool MenuWindow::load_song_card_preview_bitmap(std::string_view path) {
    if (!d2d_ || !d2d_->d2d_context || !d2d_->wic_factory || path.empty()) {
        return false;
    }

    const std::string key(path);
    auto existing = d2d_->song_card_preview_bitmaps.find(key);
    if (existing != d2d_->song_card_preview_bitmaps.end()) {
        touch_song_card_preview_lru(key);
        return static_cast<bool>(existing->second.bitmap);
    }

    D2DResources::SongCardPreviewBitmapEntry entry;
    entry.attempted = true;
    static_cast<void>(load_bitmap_from_utf8_path(
        d2d_->wic_factory.Get(), d2d_->d2d_context.Get(), key, entry.bitmap));
    auto [inserted_it, inserted] =
        d2d_->song_card_preview_bitmaps.emplace(key, std::move(entry));
    (void)inserted;
    touch_song_card_preview_lru(key);
    trim_song_card_preview_cache();
    return static_cast<bool>(inserted_it->second.bitmap);
}

bool MenuWindow::load_selected_song_preview_bitmap(const SongSelectData& data, int64_t now_ns) {
    static_cast<void>(now_ns);

    if (!ensure_song_select_preview_bitmap(data) || !d2d_ || !d2d_->wic_factory || !d2d_->d2d_context) {
        return false;
    }

    if (song_select_preview_cache_.attempted) {
        return song_select_preview_cache_.path.empty() || static_cast<bool>(d2d_->song_select_preview_bitmap);
    }
    if (song_select_preview_cache_.path.empty()) {
        song_select_preview_cache_.attempted = true;
        return true;
    }

    const int64_t load_start_ns = timing::HighResClock::now_ns();
    const bool loaded = load_bitmap_from_utf8_path(d2d_->wic_factory.Get(),
                                                   d2d_->d2d_context.Get(),
                                                   song_select_preview_cache_.path,
                                                   d2d_->song_select_preview_bitmap);
    song_select_preview_cache_.attempted = true;

    const double load_ms = static_cast<double>(timing::HighResClock::now_ns() - load_start_ns) / 1'000'000.0;
    if (load_ms >= 8.0 &&
        song_select_preview_warned_slow_paths_.emplace(song_select_preview_cache_.path).second) {
        std::cerr << "[MenuWindow] Slow Song Select preview decode " << load_ms
                  << " ms path=" << song_select_preview_cache_.path << std::endl;
    }
    if (!loaded &&
        song_select_preview_warned_decode_failures_.emplace(song_select_preview_cache_.path).second) {
        std::cerr << "[MenuWindow] Failed to decode Song Select preview path="
                  << song_select_preview_cache_.path << std::endl;
    }

    return loaded;
}

void MenuWindow::pump_song_select_preview_loads(const SongSelectData& data, int64_t now_ns) {
    if (!d2d_ || !d2d_->d2d_context || !d2d_->wic_factory) {
        return;
    }

    if (!data.profile_avatar_path.empty()) {
        const std::string avatar_key(data.profile_avatar_path);
        auto avatar_it = d2d_->song_card_preview_bitmaps.find(avatar_key);
        if (avatar_it == d2d_->song_card_preview_bitmaps.end()) {
            static_cast<void>(load_song_card_preview_bitmap(avatar_key));
            return;
        }
        touch_song_card_preview_lru(avatar_key);
    }
    if (data.showing_sources || data.showing_records ||
        song_select_preview_loading_deferred(now_ns)) {
        return;
    }

    if (!song_select_preview_cache_.attempted) {
        static_cast<void>(load_selected_song_preview_bitmap(data, now_ns));
        return;
    }

    auto load_visible_song_if_missing = [&](bool selected_only) -> bool {
        for (const auto& song : data.songs) {
            if (song.background_path.empty()) {
                continue;
            }
            if (selected_only && song.song_index != data.list_selected_index) {
                continue;
            }
            if (!selected_only && song.song_index == data.list_selected_index) {
                continue;
            }

            const std::string key(song.background_path);
            auto it = d2d_->song_card_preview_bitmaps.find(key);
            if (it != d2d_->song_card_preview_bitmaps.end()) {
                touch_song_card_preview_lru(key);
                continue;
            }

            static_cast<void>(load_song_card_preview_bitmap(key));
            return true;
        }
        return false;
    };

    if (load_visible_song_if_missing(true)) {
        return;
    }
    static_cast<void>(load_visible_song_if_missing(false));
}

void MenuWindow::invalidate_gameplay_static_cache() {
    gameplay_static_cache_ = GameplayStaticCache{};
    if (d2d_) {
        d2d_->gameplay_static_command_list.Reset();
        for (auto& geometry : d2d_->gameplay_gauge_grid_geometries) {
            geometry.Reset();
        }
    }
}

bool MenuWindow::ensure_gameplay_static_cache(const GameplayHudData& data) {
    if (!d2d_ || !d2d_->d2d_context || !d2d_->d2d_factory) {
        return false;
    }

    GameplayStaticCache desired{};
    desired.lane_count = std::clamp(data.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
    if (gameplay_field_drag_state_.has_local_override &&
        !gameplay_field_drag_state_.active &&
        std::abs(data.gameplay_field_offset_x - gameplay_field_drag_state_.offset_x) < 0.5) {
        gameplay_field_drag_state_.has_local_override = false;
    }
    desired.gameplay_field_offset_x = gameplay_field_drag_state_.has_local_override
                                         ? gameplay_field_drag_state_.offset_x
                                         : data.gameplay_field_offset_x;
    const bool use_imported_metrics = normalize_gameplay_skin_source(data.skin_source) != "native";
    desired.judgement_line_position =
        gameplay_note_sprite_cache_.has_imported_judgement_line_position
            ? gameplay_note_sprite_cache_.imported_judgement_line_position
            : clamp_gameplay_judgement_line(data.judgement_line_position);
    desired.note_width_scale = clamp_gameplay_note_width_scale(data.note_width_scale);
    desired.note_art_width_ratio = effective_gameplay_note_art_width_ratio(
        gameplay_note_sprite_cache_.imported_note_width_ratio, use_imported_metrics);
    desired.note_height_scale = effective_gameplay_note_height_scale(
        data.note_height_scale,
        gameplay_note_sprite_cache_.imported_note_height_ratio,
        use_imported_metrics);
    desired.lane_width_scale_count = std::min(data.lane_width_scale_count, desired.lane_width_scales.size());
    desired.lane_width_scales.fill(kGameplayLaneWidthScaleDefault);
    for (std::size_t i = 0; i < desired.lane_width_scale_count; ++i) {
        desired.lane_width_scales[i] = data.lane_width_scales[i];
    }
    if (gameplay_note_sprite_cache_.imported_lane_width_scale_count ==
        static_cast<std::size_t>(desired.lane_count)) {
        desired.lane_width_scale_count = static_cast<std::size_t>(desired.lane_count);
        for (std::size_t lane = 0; lane < desired.lane_width_scale_count; ++lane) {
            desired.lane_width_scales[lane] = std::clamp(
                desired.lane_width_scales[lane] *
                    gameplay_note_sprite_cache_.imported_lane_width_scales[lane],
                kGameplayLaneWidthScaleMin,
                kGameplayLaneWidthScaleMax);
        }
    }
    desired.lane_spacing_scale_count = std::min(data.lane_spacing_scale_count, desired.lane_spacing_scales.size());
    desired.lane_spacing_scales.fill(kGameplayLaneSpacingScaleDefault);
    for (std::size_t i = 0; i < desired.lane_spacing_scale_count; ++i) {
        desired.lane_spacing_scales[i] = data.lane_spacing_scales[i];
    }
    if (gameplay_note_sprite_cache_.imported_lane_spacing_scale_count ==
        static_cast<std::size_t>(std::max(0, desired.lane_count - 1))) {
        desired.lane_spacing_scale_count =
            gameplay_note_sprite_cache_.imported_lane_spacing_scale_count;
        for (std::size_t gap = 0; gap < desired.lane_spacing_scale_count; ++gap) {
            desired.lane_spacing_scales[gap] = std::clamp(
                desired.lane_spacing_scales[gap] +
                    gameplay_note_sprite_cache_.imported_lane_spacing_scales[gap],
                kGameplayLaneSpacingScaleMin,
                kGameplayLaneSpacingScaleMax);
        }
    }
    desired.lane_divider_width_scale = data.lane_divider_width_scale;
    desired.lane_center_gap_scale = data.lane_center_gap_scale;
    desired.show_lane_dividers = data.show_lane_dividers;
    desired.show_judgement_line = data.show_judgement_line;
    desired.note_divider_gap_px = data.note_divider_gap_px;
    desired.show_gear_boundary_line = data.show_gear_boundary_line;
    desired.judgement_line_glow_enabled = data.judgement_line_glow_enabled;
    desired.lane_background_opacity = std::clamp(data.lane_background_opacity, 0.0, 0.45);
    desired.black_playfield_enabled = data.black_playfield_enabled;
    desired.visual_opacity = std::clamp(data.visual_opacity, 0.20, 1.0);
    desired.ghost_visible = data.ghost_visible;
    desired.lane_color_count = std::min(data.lane_color_count, desired.lane_colors.size());
    desired.lane_colors.fill(0);
    for (std::size_t lane = 0; lane < desired.lane_color_count; ++lane) {
        desired.lane_colors[lane] = data.lane_colors[lane];
    }
    desired.lane_divider_width_count = resolve_gameplay_lane_divider_widths(
        desired.lane_count,
        data.lane_divider_width_scale,
        gameplay_note_sprite_cache_.lane_divider_width_count,
        gameplay_note_sprite_cache_.lane_divider_widths,
        desired.lane_divider_widths);

    const bool cache_matches =
        d2d_->gameplay_static_command_list &&
        gameplay_static_cache_.lane_count == desired.lane_count &&
        gameplay_static_cache_.judgement_line_position == desired.judgement_line_position &&
        gameplay_static_cache_.gameplay_field_offset_x == desired.gameplay_field_offset_x &&
        gameplay_static_cache_.note_width_scale == desired.note_width_scale &&
        gameplay_static_cache_.note_art_width_ratio == desired.note_art_width_ratio &&
        gameplay_static_cache_.note_height_scale == desired.note_height_scale &&
        gameplay_static_cache_.lane_width_scale_count == desired.lane_width_scale_count &&
        gameplay_static_cache_.lane_width_scales == desired.lane_width_scales &&
        gameplay_static_cache_.lane_spacing_scale_count == desired.lane_spacing_scale_count &&
        gameplay_static_cache_.lane_spacing_scales == desired.lane_spacing_scales &&
        gameplay_static_cache_.lane_divider_width_scale == desired.lane_divider_width_scale &&
        gameplay_static_cache_.lane_center_gap_scale == desired.lane_center_gap_scale &&
        gameplay_static_cache_.show_lane_dividers == desired.show_lane_dividers &&
        gameplay_static_cache_.show_judgement_line == desired.show_judgement_line &&
        gameplay_static_cache_.note_divider_gap_px == desired.note_divider_gap_px &&
        gameplay_static_cache_.show_gear_boundary_line == desired.show_gear_boundary_line &&
        gameplay_static_cache_.judgement_line_glow_enabled == desired.judgement_line_glow_enabled &&
        gameplay_static_cache_.lane_background_opacity == desired.lane_background_opacity &&
        gameplay_static_cache_.black_playfield_enabled == desired.black_playfield_enabled &&
        gameplay_static_cache_.visual_opacity == desired.visual_opacity &&
        gameplay_static_cache_.ghost_visible == desired.ghost_visible &&
        gameplay_static_cache_.lane_color_count == desired.lane_color_count &&
        gameplay_static_cache_.lane_colors == desired.lane_colors &&
        gameplay_static_cache_.lane_divider_width_count == desired.lane_divider_width_count &&
        gameplay_static_cache_.lane_divider_widths == desired.lane_divider_widths;
    if (cache_matches) {
        return true;
    }

    d2d_->gameplay_static_command_list.Reset();
    for (auto& geometry : d2d_->gameplay_gauge_grid_geometries) {
        geometry.Reset();
    }
    if (FAILED(d2d_->d2d_context->CreateCommandList(d2d_->gameplay_static_command_list.ReleaseAndGetAddressOf())) ||
        !d2d_->gameplay_static_command_list) {
        return false;
    }

    Microsoft::WRL::ComPtr<ID2D1Image> previous_target;
    d2d_->d2d_context->GetTarget(previous_target.ReleaseAndGetAddressOf());
    d2d_->d2d_context->SetTarget(d2d_->gameplay_static_command_list.Get());
    d2d_->d2d_context->BeginDraw();
    d2d_->d2d_context->SetTransform(D2D1::Matrix3x2F::Identity());

    const float note_width_scale = static_cast<float>(desired.note_width_scale);
    const float note_height_scale = static_cast<float>(desired.note_height_scale);
    const GameplaySurfaceLayout surface_layout =
        build_gameplay_surface_layout(
            desired.lane_count,
            note_width_scale,
            desired.note_art_width_ratio,
            desired.lane_width_scale_count,
            desired.lane_width_scales,
            desired.lane_spacing_scale_count,
            desired.lane_spacing_scales,
            desired.ghost_visible,
            desired.lane_center_gap_scale,
            desired.gameplay_field_offset_x,
            desired.note_divider_gap_px);
    auto build_gauge_grid = [&](std::size_t index, float gauge_left) {
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
        if (FAILED(d2d_->d2d_factory->CreatePathGeometry(geometry.ReleaseAndGetAddressOf())) || !geometry) {
            return;
        }
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(geometry->Open(sink.ReleaseAndGetAddressOf())) || !sink) {
            return;
        }
        for (int segment = 1; segment < 10; ++segment) {
            const float ratio = static_cast<float>(segment) / 10.0f;
            const float y = kGameplayGaugeBottom -
                            (kGameplayGaugeBottom - kGameplayGaugeTop) * ratio;
            sink->BeginFigure(D2D1::Point2F(gauge_left + 4.0f, y), D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddLine(D2D1::Point2F(gauge_left + kGameplayGaugeWidth - 4.0f, y));
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
        }
        if (SUCCEEDED(sink->Close())) {
            d2d_->gameplay_gauge_grid_geometries[index] = std::move(geometry);
        }
    };
    build_gauge_grid(0, surface_layout.player_gauge_left);
    if (surface_layout.ghost_visible) {
        build_gauge_grid(1, surface_layout.ghost_gauge_left);
    }
    auto draw_field_panel = [&](const GameplayFieldLayout& field_layout) {
        const D2D1_RECT_F field_rect =
            D2D1::RectF(field_layout.left, field_layout.top, field_layout.right, field_layout.bottom);

        if (d2d_->panel_brush) {
            d2d_->panel_brush->SetOpacity(0.84f);
            d2d_->d2d_context->FillRoundedRectangle(
                D2D1::RoundedRect(field_rect, 16.0f, 16.0f), d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(1.0f);
        }
        if (desired.black_playfield_enabled && d2d_->note_fill_brush) {
            d2d_->note_fill_brush->SetColor(D2D1::ColorF(0x000000, 1.0f));
            d2d_->d2d_context->FillRoundedRectangle(
                D2D1::RoundedRect(field_rect, 16.0f, 16.0f), d2d_->note_fill_brush.Get());
        }
        if (!desired.black_playfield_enabled && d2d_->note_fill_brush) {
            const float lane_bg_opacity = static_cast<float>(
                std::clamp(desired.lane_background_opacity * desired.visual_opacity, 0.0, 0.50));
            for (int lane = 0; lane < desired.lane_count; ++lane) {
                const float left = gameplay_lane_left(field_layout, lane);
                const float right = gameplay_lane_right(field_layout, lane);
                uint32_t lane_color = 0xF6F8FF;
                if (static_cast<std::size_t>(lane) < desired.lane_color_count) {
                    lane_color = desired.lane_colors[static_cast<std::size_t>(lane)];
                } else if (!gameplay_lane_uses_white_note(lane + 1)) {
                    lane_color = 0x4F80FF;
                }
                d2d_->note_fill_brush->SetColor(
                    color_from_rgb(blend_rgb(lane_color, 0xFFFFFF, (lane % 2 == 0) ? 0.08f : 0.02f),
                                   lane_bg_opacity * ((lane % 2 == 0) ? 1.0f : 0.72f)));
                d2d_->d2d_context->FillRoundedRectangle(
                    D2D1::RoundedRect(
                        D2D1::RectF(left + 1.0f,
                                    field_layout.top + 1.0f,
                                    right - 1.0f,
                                    field_layout.bottom - 1.0f),
                        5.0f,
                        5.0f),
                    d2d_->note_fill_brush.Get());
            }
        }
        if (d2d_->button_border_brush) {
            d2d_->d2d_context->DrawRoundedRectangle(D2D1::RoundedRect(field_rect, 16.0f, 16.0f),
                                                    d2d_->button_border_brush.Get(),
                                                    1.4f);
        }

        if (desired.show_lane_dividers && d2d_->lane_divider_brush) {
            for (std::size_t divider = 0; divider < desired.lane_divider_width_count; ++divider) {
                if (gameplay_is_center_gap_divider(field_layout, divider)) {
                    continue;
                }
                const float x = gameplay_lane_divider_x(field_layout, divider);
                const float stroke = std::max(0.0f, desired.lane_divider_widths[divider]);
                if (stroke <= 0.0f) {
                    continue;
                }
                d2d_->d2d_context->DrawLine(D2D1::Point2F(x, field_layout.top + 3.0f),
                                            D2D1::Point2F(x, field_layout.bottom - 3.0f),
                                            d2d_->lane_divider_brush.Get(),
                                            stroke);
            }
        }

        const float hit_line_y =
            gameplay_field_y(field_layout.top, field_layout.height, desired.judgement_line_position);
        if (desired.show_judgement_line && d2d_->judgement_line_brush) {
            const D2D1_RECT_F line_rect =
                gameplay_judgement_line_rect(field_layout, hit_line_y, note_height_scale);
            if (desired.judgement_line_glow_enabled) {
                const float saved_opacity = d2d_->judgement_line_brush->GetOpacity();
                const D2D1_RECT_F outer_glow =
                    D2D1::RectF(field_layout.left + 5.0f,
                                line_rect.top - 12.0f,
                                field_layout.right - 5.0f,
                                line_rect.bottom + 12.0f);
                const D2D1_RECT_F inner_glow =
                    D2D1::RectF(field_layout.left + 8.0f,
                                line_rect.top - 5.0f,
                                field_layout.right - 8.0f,
                                line_rect.bottom + 5.0f);
                d2d_->judgement_line_brush->SetOpacity(
                    static_cast<float>(0.18 * desired.visual_opacity));
                d2d_->d2d_context->FillRoundedRectangle(D2D1::RoundedRect(outer_glow, 10.0f, 10.0f),
                                                        d2d_->judgement_line_brush.Get());
                d2d_->judgement_line_brush->SetOpacity(
                    static_cast<float>(0.32 * desired.visual_opacity));
                d2d_->d2d_context->FillRoundedRectangle(D2D1::RoundedRect(inner_glow, 7.0f, 7.0f),
                                                        d2d_->judgement_line_brush.Get());
                d2d_->judgement_line_brush->SetOpacity(saved_opacity);
            }
            d2d_->d2d_context->FillRoundedRectangle(D2D1::RoundedRect(line_rect, 5.0f, 5.0f),
                                                     d2d_->judgement_line_brush.Get());
            if (d2d_->accent_brush) {
                // Keep the endpoint core fully visible while the logical line remains exactly at 0%/100%.
                const float core_y = std::clamp(hit_line_y,
                                                field_layout.top + 1.5f,
                                                field_layout.bottom - 1.5f);
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(static_cast<float>(0.92 * desired.visual_opacity));
                d2d_->d2d_context->DrawLine(D2D1::Point2F(field_layout.left + 7.0f, core_y),
                                            D2D1::Point2F(field_layout.right - 7.0f, core_y),
                                            d2d_->accent_brush.Get(),
                                            3.0f);
                d2d_->accent_brush->SetColor(D2D1::ColorF(0xF7FAFD));
                d2d_->accent_brush->SetOpacity(static_cast<float>(0.96 * desired.visual_opacity));
                d2d_->d2d_context->DrawLine(D2D1::Point2F(field_layout.left + 9.0f, core_y),
                                            D2D1::Point2F(field_layout.right - 9.0f, core_y),
                                            d2d_->accent_brush.Get(),
                                            1.0f);
                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
        }
        if (desired.show_gear_boundary_line && d2d_->accent_brush) {
            const float gear_top = gameplay_osu_gear_top(field_layout, hit_line_y, note_height_scale);
            d2d_->accent_brush->SetOpacity(0.38f);
            d2d_->d2d_context->DrawLine(D2D1::Point2F(field_layout.left + 4.0f, gear_top),
                                        D2D1::Point2F(field_layout.right - 4.0f, gear_top),
                                        d2d_->accent_brush.Get(),
                                        1.4f);
            d2d_->accent_brush->SetOpacity(1.0f);
        }
    };
    auto draw_gauge_frame = [&](float gauge_left) {
        const D2D1_RECT_F gauge_frame = D2D1::RectF(
            gauge_left, kGameplayGaugeTop, gauge_left + kGameplayGaugeWidth, kGameplayGaugeBottom);
        if (d2d_->footer_brush) {
            d2d_->footer_brush->SetOpacity(0.62f);
            d2d_->d2d_context->FillRoundedRectangle(D2D1::RoundedRect(gauge_frame, 10.0f, 10.0f),
                                                    d2d_->footer_brush.Get());
            d2d_->footer_brush->SetOpacity(1.0f);
        }
        if (d2d_->button_border_brush) {
            d2d_->d2d_context->DrawRoundedRectangle(D2D1::RoundedRect(gauge_frame, 10.0f, 10.0f),
                                                     d2d_->button_border_brush.Get(),
                                                     1.4f);
        }
    };

    draw_field_panel(surface_layout.player_field);
    draw_gauge_frame(surface_layout.player_gauge_left);
    if (surface_layout.ghost_visible) {
        draw_field_panel(surface_layout.ghost_field);
        draw_gauge_frame(surface_layout.ghost_gauge_left);
    }

    const HRESULT end_draw_hr = d2d_->d2d_context->EndDraw();
    d2d_->d2d_context->SetTarget(previous_target.Get());
    if (FAILED(end_draw_hr) || FAILED(d2d_->gameplay_static_command_list->Close())) {
        d2d_->gameplay_static_command_list.Reset();
        return false;
    }

    gameplay_static_cache_ = desired;
    return true;
}

bool MenuWindow::is_input_foreground() const {
    const HWND hwnd = static_cast<HWND>(hwnd_);
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }

    const HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return false;
    }
    if (foreground == hwnd) {
        return true;
    }

    const HWND foreground_root = GetAncestor(foreground, GA_ROOT);
    const HWND window_root = GetAncestor(hwnd, GA_ROOT);
    return foreground_root != nullptr && foreground_root == window_root;
}

void MenuWindow::clear_song_scrollbar_state() {
    song_scrollbar_state_ = SongScrollbarState{};
    song_scroll_drag_active_ = false;
    song_scroll_drag_offset_y_ = 0.0f;
    song_scroll_drag_selected_offset_ = 0;
    song_scroll_drag_last_index_ = -1;
}

bool MenuWindow::translate_window_point(int window_x, int window_y, float* out_x, float* out_y) const {
    if (scale_ <= 0.0f) {
        if (out_x) {
            *out_x = 0.0f;
        }
        if (out_y) {
            *out_y = 0.0f;
        }
        return false;
    }

    const float logical_x = (static_cast<float>(window_x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(window_y) - offset_y_) / scale_;
    if (out_x) {
        *out_x = logical_x;
    }
    if (out_y) {
        *out_y = logical_y;
    }
    return logical_x >= 0.0f && logical_x <= kBaseWidth &&
           logical_y >= 0.0f && logical_y <= kBaseHeight;
}

int MenuWindow::song_scrollbar_target_index(float y, float drag_offset, int selected_offset) const {
    if (!song_scrollbar_state_.visible || song_scrollbar_state_.total_count <= 0) {
        return -1;
    }

    const float thumb_height =
        std::max(12.0f, song_scrollbar_state_.thumb_bottom - song_scrollbar_state_.thumb_top);
    const float track_top = song_scrollbar_state_.top;
    const float track_bottom = std::max(track_top + thumb_height, song_scrollbar_state_.bottom);
    const float max_thumb_top = std::max(track_top, track_bottom - thumb_height);
    const float desired_thumb_top = std::clamp(y - drag_offset, track_top, max_thumb_top);
    const float travel = std::max(1.0f, max_thumb_top - track_top);
    const int max_window_start =
        std::max(0, song_scrollbar_state_.total_count - song_scrollbar_state_.visible_count);
    const float ratio =
        (max_window_start <= 0) ? 0.0f : ((desired_thumb_top - track_top) / travel);
    const int window_start =
        std::clamp(static_cast<int>(std::lround(ratio * static_cast<float>(max_window_start))),
                   0,
                   max_window_start);
    const int safe_selected_offset =
        std::clamp(selected_offset, 0, std::max(0, song_scrollbar_state_.visible_count - 1));
    return std::clamp(window_start + safe_selected_offset, 0, song_scrollbar_state_.total_count - 1);
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

    if (gameplay_field_drag_state_.visible &&
        x >= gameplay_field_drag_state_.left && x <= gameplay_field_drag_state_.right &&
        y >= gameplay_field_drag_state_.top && y <= gameplay_field_drag_state_.bottom) {
        gameplay_field_drag_state_.active = true;
        gameplay_field_drag_state_.hovered = true;
        gameplay_field_drag_state_.has_local_override = true;
        gameplay_field_drag_state_.drag_start_x = x;
        gameplay_field_drag_state_.drag_start_offset_x = gameplay_field_drag_state_.offset_x;
        suppress_next_left_button_up_ = true;
        update_cursor_visibility(false);
        SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
        if (hwnd_) {
            SetCapture(static_cast<HWND>(hwnd_));
        }
        return;
    }

    for (auto it = hit_regions_.rbegin(); it != hit_regions_.rend(); ++it) {
        if (it->part != MenuHitPart::SetValue ||
            x < it->left || x > it->right || y < it->top || y > it->bottom) {
            continue;
        }
        value_slider_drag_state_.active = true;
        value_slider_drag_state_.kind = it->kind;
        value_slider_drag_state_.index = it->index;
        value_slider_drag_state_.left = it->left;
        value_slider_drag_state_.right = it->right;
        const float width = it->right - it->left;
        value_slider_drag_state_.last_value =
            width > 0.0f
                ? std::clamp(static_cast<double>((x - it->left) / width), 0.0, 1.0)
                : 0.0;
        MenuClickEvent event;
        event.kind = it->kind;
        event.index = it->index;
        event.part = MenuHitPart::SetValue;
        event.value = value_slider_drag_state_.last_value;
        push_click_event(std::move(event));
        suppress_next_left_button_up_ = true;
        if (hwnd_) {
            SetCapture(static_cast<HWND>(hwnd_));
        }
        SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
        return;
    }

    if (!song_scrollbar_state_.visible ||
        x < song_scrollbar_state_.left || x > song_scrollbar_state_.right ||
        y < song_scrollbar_state_.top || y > song_scrollbar_state_.bottom) {
        return;
    }

    const float thumb_height =
        std::max(12.0f, song_scrollbar_state_.thumb_bottom - song_scrollbar_state_.thumb_top);
    const bool on_thumb =
        y >= song_scrollbar_state_.thumb_top && y <= song_scrollbar_state_.thumb_bottom;
    song_scroll_drag_active_ = true;
    song_scroll_drag_offset_y_ = on_thumb ? (y - song_scrollbar_state_.thumb_top) : (thumb_height * 0.5f);
    song_scroll_drag_selected_offset_ =
        std::clamp(song_scrollbar_state_.selected_index - song_scrollbar_state_.window_start,
                   0,
                   std::max(0, song_scrollbar_state_.visible_count - 1));
    song_scroll_drag_last_index_ = song_scrollbar_state_.selected_index;
    suppress_next_left_button_up_ = true;

    if (!on_thumb) {
        const int target_index =
            song_scrollbar_target_index(y, song_scroll_drag_offset_y_, song_scroll_drag_selected_offset_);
        if (target_index >= 0) {
            MenuClickEvent event;
            event.kind = MenuHitTargetKind::SongScrollbar;
            event.index = target_index;
            push_click_event(std::move(event));
            song_scroll_drag_last_index_ = target_index;
        }
    }

    if (hwnd_) {
        SetCapture(static_cast<HWND>(hwnd_));
    }
}

void MenuWindow::on_mouse_click(int window_x, int window_y, bool double_click) {
    if (gameplay_field_drag_state_.active) {
        float x = 0.0f;
        float y = 0.0f;
        static_cast<void>(translate_window_point(window_x, window_y, &x, &y));
        const double requested_offset = gameplay_field_drag_state_.drag_start_offset_x +
                                        static_cast<double>(x - gameplay_field_drag_state_.drag_start_x);
        gameplay_field_drag_state_.offset_x = std::clamp(
            std::round(requested_offset),
            gameplay_field_drag_state_.min_offset_x,
            gameplay_field_drag_state_.max_offset_x);
        gameplay_field_drag_state_.has_local_override = true;
        gameplay_field_drag_state_.active = false;
        gameplay_field_drag_state_.hovered =
            x >= gameplay_field_drag_state_.left && x <= gameplay_field_drag_state_.right &&
            y >= gameplay_field_drag_state_.top && y <= gameplay_field_drag_state_.bottom;

        MenuClickEvent event;
        event.kind = MenuHitTargetKind::GameplayFieldDrag;
        event.index = 0;
        event.part = MenuHitPart::Activate;
        event.value = gameplay_field_drag_state_.offset_x;
        push_click_event(std::move(event));

        if (hwnd_ && GetCapture() == static_cast<HWND>(hwnd_)) {
            ReleaseCapture();
        }
        update_cursor_visibility(
            gameplay_field_drag_state_.visible && !gameplay_field_drag_state_.hovered);
        if (gameplay_field_drag_state_.hovered) {
            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
        }
        suppress_next_left_button_up_ = false;
        return;
    }

    if (value_slider_drag_state_.active) {
        float x = 0.0f;
        float y = 0.0f;
        static_cast<void>(translate_window_point(window_x, window_y, &x, &y));
        const float width = value_slider_drag_state_.right - value_slider_drag_state_.left;
        const double value =
            width > 0.0f
                ? std::clamp(
                      static_cast<double>((x - value_slider_drag_state_.left) / width),
                      0.0, 1.0)
                : 0.0;
        if (value != value_slider_drag_state_.last_value) {
            MenuClickEvent event;
            event.kind = value_slider_drag_state_.kind;
            event.index = value_slider_drag_state_.index;
            event.part = MenuHitPart::SetValue;
            event.value = value;
            push_click_event(std::move(event));
        }
        value_slider_drag_state_ = ValueSliderDragState{};
        if (hwnd_ && GetCapture() == static_cast<HWND>(hwnd_)) {
            ReleaseCapture();
        }
        suppress_next_left_button_up_ = false;
        return;
    }

    if (song_scroll_drag_active_) {
        song_scroll_drag_active_ = false;
        if (hwnd_ && GetCapture() == static_cast<HWND>(hwnd_)) {
            ReleaseCapture();
        }
        if (!double_click && suppress_next_left_button_up_) {
            suppress_next_left_button_up_ = false;
            return;
        }
    }

    if (!is_input_foreground()) {
        suppress_next_left_button_up_ = false;
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    if (!translate_window_point(window_x, window_y, &x, &y)) {
        suppress_next_left_button_up_ = false;
        return;
    }
    if (!double_click && suppress_next_left_button_up_) {
        suppress_next_left_button_up_ = false;
        return;
    }

    const HitRegion* hit = nullptr;
    for (auto it = hit_regions_.rbegin(); it != hit_regions_.rend(); ++it) {
        if (x >= it->left && x <= it->right &&
            y >= it->top && y <= it->bottom) {
            hit = &*it;
            break;
        }
    }
    if (!hit) {
        suppress_next_left_button_up_ = false;
        return;
    }

    MenuClickEvent event;
    event.kind = hit->kind;
    event.index = hit->index;
    event.part = hit->part;
    if (hit->part == MenuHitPart::SetValue) {
        const float width = hit->right - hit->left;
        event.value = width > 0.0f
                          ? std::clamp(static_cast<double>((x - hit->left) / width), 0.0, 1.0)
                          : 0.0;
    }
    event.double_click = double_click;
    push_click_event(std::move(event));
    suppress_next_left_button_up_ = double_click;
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
        if (x < it->left || x > it->right || y < it->top || y > it->bottom) {
            continue;
        }
        if (it->kind == MenuHitTargetKind::SongQuickSetting) {
            MenuClickEvent event;
            event.kind = it->kind;
            event.index = it->index;
            event.part = MenuHitPart::Decrement;
            push_click_event(std::move(event));
        } else if ((it->kind == MenuHitTargetKind::TitleButton ||
                    it->kind == MenuHitTargetKind::SongNavButton) &&
                   it->index > 0) {
            MenuClickEvent event;
            event.kind = it->kind;
            event.index = it->index - 1;
            push_click_event(std::move(event));
        }
        return;
    }
}

void MenuWindow::on_mouse_move(int window_x, int window_y) {
    float x = 0.0f;
    float y = 0.0f;
    static_cast<void>(translate_window_point(window_x, window_y, &x, &y));

    if (gameplay_field_drag_state_.active) {
        const double requested_offset = gameplay_field_drag_state_.drag_start_offset_x +
                                        static_cast<double>(x - gameplay_field_drag_state_.drag_start_x);
        gameplay_field_drag_state_.offset_x = std::clamp(
            std::round(requested_offset),
            gameplay_field_drag_state_.min_offset_x,
            gameplay_field_drag_state_.max_offset_x);
        gameplay_field_drag_state_.has_local_override = true;
        gameplay_field_drag_state_.hovered = true;
        update_cursor_visibility(false);
        SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
        return;
    }

    if (gameplay_field_drag_state_.visible) {
        gameplay_field_drag_state_.hovered =
            x >= gameplay_field_drag_state_.left && x <= gameplay_field_drag_state_.right &&
            y >= gameplay_field_drag_state_.top && y <= gameplay_field_drag_state_.bottom;
        update_cursor_visibility(!gameplay_field_drag_state_.hovered);
        if (gameplay_field_drag_state_.hovered) {
            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
        }
        return;
    }

    if (value_slider_drag_state_.active) {
        const float width = value_slider_drag_state_.right - value_slider_drag_state_.left;
        const double value =
            width > 0.0f
                ? std::clamp(
                      static_cast<double>((x - value_slider_drag_state_.left) / width),
                      0.0, 1.0)
                : 0.0;
        if (value != value_slider_drag_state_.last_value) {
            MenuClickEvent event;
            event.kind = value_slider_drag_state_.kind;
            event.index = value_slider_drag_state_.index;
            event.part = MenuHitPart::SetValue;
            event.value = value;
            push_click_event(std::move(event));
            value_slider_drag_state_.last_value = value;
        }
        SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
        return;
    }

    if (!song_scroll_drag_active_) {
        return;
    }
    const int target_index =
        song_scrollbar_target_index(y, song_scroll_drag_offset_y_, song_scroll_drag_selected_offset_);
    if (target_index < 0 || target_index == song_scroll_drag_last_index_) {
        return;
    }

    MenuClickEvent event;
    event.kind = MenuHitTargetKind::SongScrollbar;
    event.index = target_index;
    push_click_event(std::move(event));
    song_scroll_drag_last_index_ = target_index;
}

void MenuWindow::on_mouse_capture_changed() {
    const bool lost_gameplay_drag = gameplay_field_drag_state_.active;
    const bool lost_song_drag = song_scroll_drag_active_;
    const bool lost_slider_drag = value_slider_drag_state_.active;
    if (gameplay_field_drag_state_.active) {
        gameplay_field_drag_state_.active = false;
        gameplay_field_drag_state_.hovered = false;
        gameplay_field_drag_state_.has_local_override = true;
        MenuClickEvent event;
        event.kind = MenuHitTargetKind::GameplayFieldDrag;
        event.index = 0;
        event.part = MenuHitPart::Activate;
        event.value = gameplay_field_drag_state_.offset_x;
        push_click_event(std::move(event));
    }
    value_slider_drag_state_ = ValueSliderDragState{};
    song_scroll_drag_active_ = false;
    if (lost_gameplay_drag || lost_song_drag || lost_slider_drag) {
        suppress_next_left_button_up_ = false;
    }
    if (gameplay_field_drag_state_.visible) {
        update_cursor_visibility(true);
    }
}

void MenuWindow::on_mouse_wheel(int wheel_delta) {
    if (!is_input_foreground()) {
        return;
    }

    const int steps = wheel_delta / WHEEL_DELTA;
    if (steps == 0) {
        return;
    }

    MenuClickEvent event;
    event.kind = MenuHitTargetKind::MouseWheel;
    event.index = 0;
    event.wheel_steps = steps;
    push_click_event(std::move(event));
}

void MenuWindow::on_file_drop(std::string path) {
    if (path.empty()) {
        return;
    }

    MenuClickEvent event;
    event.kind = MenuHitTargetKind::FileDrop;
    event.index = 0;
    event.path = std::move(path);
    push_click_event(std::move(event));
}

void MenuWindow::on_text_input(wchar_t character) {
    if (character < 0x20) {
        return;
    }

    std::wstring text;
    const bool high_surrogate = character >= 0xD800 && character <= 0xDBFF;
    const bool low_surrogate = character >= 0xDC00 && character <= 0xDFFF;
    if (high_surrogate) {
        pending_high_surrogate_ = character;
        return;
    }
    if (low_surrogate) {
        if (pending_high_surrogate_ == 0) {
            return;
        }
        text.push_back(pending_high_surrogate_);
        text.push_back(character);
        pending_high_surrogate_ = 0;
    } else {
        pending_high_surrogate_ = 0;
        text.push_back(character);
    }

    push_text_input(wide_to_utf8(text));
}

void MenuWindow::push_text_input(std::string text) {
    if (text.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(text_input_mutex_);
    if (text_input_events_.size() >= 64) {
        text_input_events_.pop_front();
    }
    text_input_events_.push_back(std::move(text));
}

void MenuWindow::render(const MenuRenderData& data) {
    last_present_completion_ns_.store(0, std::memory_order_release);
    apply_pending_config();
    update_cursor_visibility(data.kind == MenuScreenKind::GameplayHud &&
                             !data.gameplay.show_cursor_in_gameplay &&
                             !horizontal_drag_cursor());
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
                return;
            }
            fullscreen_ = false;
            fullscreen_restore_pending_ = true;
        } else if (!window_minimized && window_in_foreground &&
                   fullscreen_restore_pending_ && !fullscreen_) {
            if (!enter_fullscreen_mode(width_, height_, "MenuWindow::render")) {
                return;
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

void MenuWindow::request_screenshot() {
    screenshot_requested_.store(true, std::memory_order_release);
}

bool MenuWindow::save_screenshot_to_png() {
    if (!d2d_ || !d2d_->swap_chain || !d2d_->device || !d2d_->context || !d2d_->wic_factory) {
        std::cerr << "[warn] Screenshot skipped: renderer resources are unavailable." << std::endl;
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = d2d_->swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr) || !back_buffer) {
        std::cerr << "[warn] Screenshot failed: could not read swap-chain buffer hr=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
        return false;
    }

    D3D11_TEXTURE2D_DESC source_desc{};
    back_buffer->GetDesc(&source_desc);
    if (source_desc.Width == 0 || source_desc.Height == 0) {
        std::cerr << "[warn] Screenshot skipped: swap-chain buffer has zero size." << std::endl;
        return false;
    }

    D3D11_TEXTURE2D_DESC staging_desc = source_desc;
    staging_desc.BindFlags = 0;
    staging_desc.MiscFlags = 0;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture;
    hr = d2d_->device->CreateTexture2D(&staging_desc, nullptr, &staging_texture);
    if (FAILED(hr) || !staging_texture) {
        std::cerr << "[warn] Screenshot failed: could not create staging texture hr=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
        return false;
    }

    d2d_->context->CopyResource(staging_texture.Get(), back_buffer.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = d2d_->context->Map(staging_texture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr) || !mapped.pData || mapped.RowPitch == 0) {
        std::cerr << "[warn] Screenshot failed: could not map staging texture hr=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
        return false;
    }

    bool saved = false;
    do {
        const std::uint64_t buffer_size_u64 =
            static_cast<std::uint64_t>(mapped.RowPitch) * static_cast<std::uint64_t>(source_desc.Height);
        if (buffer_size_u64 == 0 ||
            buffer_size_u64 > static_cast<std::uint64_t>((std::numeric_limits<UINT>::max)())) {
            std::cerr << "[warn] Screenshot failed: mapped buffer size is invalid." << std::endl;
            break;
        }

        std::error_code ec;
        std::filesystem::path screenshot_dir = std::filesystem::current_path(ec);
        if (ec || screenshot_dir.empty()) {
            screenshot_dir = std::filesystem::temp_directory_path(ec);
            if (ec || screenshot_dir.empty()) {
                std::cerr << "[warn] Screenshot failed: could not resolve output directory." << std::endl;
                break;
            }
        }
        screenshot_dir /= "screenshots";
        std::filesystem::create_directories(screenshot_dir, ec);
        if (ec) {
            std::cerr << "[warn] Screenshot failed: could not create screenshots directory." << std::endl;
            break;
        }

        const auto now = std::chrono::system_clock::now();
        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        const int millisecond = static_cast<int>(now_ms.count() % 1000);
        const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm{};
        localtime_s(&local_tm, &now_time);

        std::wstringstream filename_stream;
        filename_stream << L"tenriff_" << std::put_time(&local_tm, L"%Y%m%d_%H%M%S")
                        << L'_' << std::setw(3) << std::setfill(L'0') << millisecond << L".png";
        const std::filesystem::path screenshot_path = screenshot_dir / filename_stream.str();

        Microsoft::WRL::ComPtr<IWICStream> stream;
        hr = d2d_->wic_factory->CreateStream(&stream);
        if (FAILED(hr) || !stream) {
            std::cerr << "[warn] Screenshot failed: could not create WIC stream hr=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            break;
        }

        hr = stream->InitializeFromFilename(screenshot_path.c_str(), GENERIC_WRITE);
        if (FAILED(hr)) {
            std::cerr << "[warn] Screenshot failed: could not open output file hr=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            break;
        }

        Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
        hr = d2d_->wic_factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
        if (FAILED(hr) || !encoder) {
            std::cerr << "[warn] Screenshot failed: could not create PNG encoder hr=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            break;
        }

        hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) {
            std::cerr << "[warn] Screenshot failed: encoder init failed hr=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            break;
        }

        Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        Microsoft::WRL::ComPtr<IPropertyBag2> property_bag;
        hr = encoder->CreateNewFrame(&frame, &property_bag);
        if (FAILED(hr) || !frame) {
            std::cerr << "[warn] Screenshot failed: frame creation failed hr=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            break;
        }

        hr = frame->Initialize(property_bag.Get());
        if (FAILED(hr)) {
            std::cerr << "[warn] Screenshot failed: frame init failed hr=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            break;
        }

        hr = frame->SetSize(source_desc.Width, source_desc.Height);
        if (FAILED(hr)) {
            std::cerr << "[warn] Screenshot failed: frame size setup failed hr=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            break;
        }

        WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
        hr = frame->SetPixelFormat(&pixel_format);
        if (FAILED(hr)) {
            std::cerr << "[warn] Screenshot failed: pixel format setup failed hr=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            break;
        }

        hr = frame->WritePixels(source_desc.Height,
                                mapped.RowPitch,
                                static_cast<UINT>(buffer_size_u64),
                                static_cast<BYTE*>(mapped.pData));
        if (FAILED(hr)) {
            std::cerr << "[warn] Screenshot failed: pixel write failed hr=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            break;
        }

        hr = frame->Commit();
        if (FAILED(hr)) {
            std::cerr << "[warn] Screenshot failed: frame commit failed hr=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            break;
        }

        hr = encoder->Commit();
        if (FAILED(hr)) {
            std::cerr << "[warn] Screenshot failed: encoder commit failed hr=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << std::endl;
            break;
        }

        std::cerr << "[info] Screenshot saved: "
                  << wide_to_utf8(screenshot_path.native()) << std::endl;
        saved = true;
    } while (false);

    d2d_->context->Unmap(staging_texture.Get(), 0);
    return saved;
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

std::optional<std::string> MenuWindow::poll_text_input() {
    std::lock_guard<std::mutex> lock(text_input_mutex_);
    if (text_input_events_.empty()) {
        return std::nullopt;
    }
    std::string text = std::move(text_input_events_.front());
    text_input_events_.pop_front();
    return text;
}


void MenuWindow::push_click_event(MenuClickEvent event) {
    if (event.kind == MenuHitTargetKind::GenericHelpPage) {
        generic_help_page_.store(std::max(0, event.index), std::memory_order_relaxed);
        return;
    }
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

bool MenuWindow::create_text_formats(const wchar_t* ui_family) {
    if (!d2d_ || !d2d_->dwrite_factory || !ui_family) {
        return false;
    }
    auto create_text_format = [this](const wchar_t* family,
                                     DWRITE_FONT_WEIGHT weight,
                                     float size,
                                     Microsoft::WRL::ComPtr<IDWriteTextFormat>* out_format) -> bool {
        Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
        const HRESULT hr = d2d_->dwrite_factory->CreateTextFormat(family,
                                                                  nullptr,
                                                                  weight,
                                                                  DWRITE_FONT_STYLE_NORMAL,
                                                                  DWRITE_FONT_STRETCH_NORMAL,
                                                                  size,
                                                                  L"",
                                                                  format.ReleaseAndGetAddressOf());
        if (FAILED(hr) || !format) {
            return false;
        }
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        *out_format = std::move(format);
        return true;
    };

    // Text roles follow the player's font choice. Logo, rank and combo stay on
    // Bahnschrift and the debug readout on Consolas: those are display numerals
    // whose layouts are built around their metrics, not UI copy.
    if (!create_text_format(ui_family, DWRITE_FONT_WEIGHT_SEMI_BOLD, 30.0f, &d2d_->title_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_NORMAL, 22.0f, &d2d_->option_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_NORMAL, 18.0f, &d2d_->body_format) ||
        !create_text_format(L"Consolas", DWRITE_FONT_WEIGHT_NORMAL, 16.0f, &d2d_->mono_format) ||
        !create_text_format(L"Bahnschrift SemiBold", DWRITE_FONT_WEIGHT_SEMI_BOLD, 82.0f, &d2d_->logo_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_SEMI_BOLD, 34.0f, &d2d_->menu_button_format) ||
        !create_text_format(L"Segoe UI Symbol", DWRITE_FONT_WEIGHT_NORMAL, 26.0f, &d2d_->menu_icon_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_SEMI_BOLD, 52.0f, &d2d_->header_format) ||
        !create_text_format(L"Bahnschrift SemiBold", DWRITE_FONT_WEIGHT_SEMI_BOLD, 42.0f,
                            &d2d_->gameplay_combo_format) ||
        !create_text_format(L"Bahnschrift SemiBold", DWRITE_FONT_WEIGHT_BOLD, 68.0f,
                            &d2d_->song_logo_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_SEMI_BOLD, 29.0f,
                            &d2d_->song_nav_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_BOLD, 22.0f,
                            &d2d_->song_record_label_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_BOLD, 24.0f,
                            &d2d_->song_record_value_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_SEMI_BOLD, 18.0f,
                            &d2d_->song_record_detail_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_SEMI_BOLD, 24.0f, &d2d_->song_title_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_NORMAL, 18.0f, &d2d_->song_artist_format) ||
        !create_text_format(L"Bahnschrift", DWRITE_FONT_WEIGHT_SEMI_BOLD, 104.0f, &d2d_->result_score_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_SEMI_BOLD, 32.0f, &d2d_->result_metric_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_NORMAL, 16.0f, &d2d_->hud_format) ||
        !create_text_format(L"Bahnschrift SemiBold", DWRITE_FONT_WEIGHT_SEMI_BOLD, 128.0f, &d2d_->rank_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_NORMAL, 16.0f, &d2d_->stats_label_format) ||
        !create_text_format(ui_family, DWRITE_FONT_WEIGHT_SEMI_BOLD, 18.0f, &d2d_->stats_value_format)) {
        return false;
    }
    d2d_->ui_font_family = ui_family;
    d2d_->generic_help_layout.Reset();
    return true;
}

#include "MenuWindow_draw.inl"

#include "MenuWindow_render_cache.inl"

}  // namespace tenriff::render

#endif  // _WIN32
