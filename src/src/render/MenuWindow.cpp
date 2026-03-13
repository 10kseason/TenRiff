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
#include <unordered_set>

#include <d2d1_1.h>
#include <d2d1helper.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

#include <objbase.h>

#include "timing/HighResClock.h"
#include "util/Utf8Compat.h"

namespace tenriff::render {

namespace {

constexpr float kBaseWidth = 1920.0f;
constexpr float kBaseHeight = 1080.0f;
constexpr wchar_t kWindowClassName[] = L"TenRiffMenuWindow";
constexpr float kGameplayFieldLeft = 470.0f;
constexpr float kGameplayFieldRight = 1450.0f;
constexpr float kGameplayFieldTop = 140.0f;
constexpr float kGameplayFieldBottom = 980.0f;
constexpr float kGameplayGaugeLeft = 1510.0f;
constexpr float kGameplayGaugeTop = 210.0f;
constexpr float kGameplayGaugeBottom = 910.0f;
constexpr float kGameplayGaugeWidth = 46.0f;
constexpr double kGameplayYTop = 0.08;
constexpr double kGameplayYBottom = 0.90;
constexpr double kGameplayJudgementLineMin = 0.55;
constexpr double kGameplayJudgementLineMax = 0.86;
constexpr double kGameplayJudgementLineDefault = 0.82;
constexpr double kGameplayNoteWidthScaleMin = 0.50;
constexpr double kGameplayNoteWidthScaleMax = 1.40;
constexpr double kGameplayNoteHeightScaleMin = 0.50;
constexpr double kGameplayNoteHeightScaleMax = 2.00;

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

float clamp_gameplay_note_height_scale(double value) {
    if (!std::isfinite(value)) {
        return 1.0f;
    }
    return static_cast<float>(std::clamp(value, kGameplayNoteHeightScaleMin, kGameplayNoteHeightScaleMax));
}

float gameplay_note_draw_width(float lane_width, double note_width_scale) {
    const float safe_lane_width = std::max(24.0f, lane_width);
    const float base_note_width = std::max(16.0f, safe_lane_width - 16.0f);
    return std::clamp(base_note_width * clamp_gameplay_note_width_scale(note_width_scale),
                      16.0f,
                      std::max(16.0f, safe_lane_width - 4.0f));
}

float gameplay_field_y(float field_top, float field_height, double normalized_y) {
    return field_top + field_height * static_cast<float>(normalized_y);
}

struct MonitorDisplayInfo {
    RECT rect{0, 0, 0, 0};
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
        info.width = 1280;
        info.height = 720;
    }
    return info;
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

    const int monitor_width = static_cast<int>(monitor.width);
    const int monitor_height = static_cast<int>(monitor.height);
    out_x = monitor.rect.left + std::max(0, (monitor_width - static_cast<int>(out_width)) / 2);
    out_y = monitor.rect.top + std::max(0, (monitor_height - static_cast<int>(out_height)) / 2);
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

void set_brush_points(ID2D1LinearGradientBrush* brush, const D2D1_RECT_F& rect) {
    if (!brush) {
        return;
    }
    brush->SetStartPoint(D2D1::Point2F(rect.left, rect.top));
    brush->SetEndPoint(D2D1::Point2F(rect.right, rect.bottom));
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
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (window && wparam != SIZE_MINIMIZED) {
                window->queue_resize(LOWORD(lparam), HIWORD(lparam));
            }
            return 0;
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
        case WM_MOUSEWHEEL:
            if (window) {
                window->on_mouse_wheel(GET_WHEEL_DELTA_WPARAM(wparam));
                return 0;
            }
            break;
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
        default:
            break;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

struct MenuWindow::D2DResources {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2d_factory;
    Microsoft::WRL::ComPtr<ID2D1Device> d2d_device;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2d_context;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_target;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> title_format;
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
    std::array<Microsoft::WRL::ComPtr<ID2D1Bitmap>, kGameplayHudMaxLanes> lane_note_tail_bitmaps{};
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

void MenuWindow::invalidate_gameplay_note_sprite_cache() {
    gameplay_note_sprite_cache_ = {};
    if (!d2d_) {
        return;
    }
    for (auto& bitmap : d2d_->lane_note_head_bitmaps) {
        bitmap.Reset();
    }
    for (auto& bitmap : d2d_->lane_note_tail_bitmaps) {
        bitmap.Reset();
    }
}

bool MenuWindow::ensure_gameplay_note_sprites(const GameplayHudData& data) {
    if (!d2d_ || !d2d_->d2d_context) {
        return false;
    }
    const int lane_count = std::clamp(data.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
    bool cache_valid = gameplay_note_sprite_cache_.lane_count == lane_count;
    for (int lane = 0; lane < lane_count && cache_valid; ++lane) {
        uint32_t color = 0xF6F8FF;
        if (static_cast<std::size_t>(lane) < data.lane_color_count) {
            color = data.lane_colors[static_cast<std::size_t>(lane)];
        } else if (!gameplay_lane_uses_white_note(lane + 1)) {
            color = 0x4F80FF;
        }
        if (!d2d_->lane_note_head_bitmaps[static_cast<std::size_t>(lane)] ||
            !d2d_->lane_note_tail_bitmaps[static_cast<std::size_t>(lane)] ||
            gameplay_note_sprite_cache_.lane_colors[static_cast<std::size_t>(lane)] != color) {
            cache_valid = false;
        }
    }
    if (cache_valid) {
        return true;
    }

    auto* ctx = d2d_->d2d_context.Get();
    auto create_note_bitmap = [&](float width,
                                  float height,
                                  const D2D1_COLOR_F& fill_color,
                                  const D2D1_COLOR_F& border_color,
                                  Microsoft::WRL::ComPtr<ID2D1Bitmap>& out_bitmap) -> bool {
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
        render_target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

        const D2D1_RECT_F rect = D2D1::RectF(1.0f, 1.0f, width - 1.0f, height - 1.0f);
        render_target->FillRectangle(rect, fill_brush.Get());
        render_target->DrawRectangle(rect, border_brush.Get(), 1.3f);

        const HRESULT end_hr = render_target->EndDraw();
        if (FAILED(end_hr)) {
            return false;
        }

        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
        const HRESULT bitmap_hr = render_target->GetBitmap(&bitmap);
        if (FAILED(bitmap_hr) || !bitmap) {
            return false;
        }

        out_bitmap = std::move(bitmap);
        return true;
    };

    if (!d2d_->note_fill_brush || !d2d_->note_border_brush) {
        return false;
    }

    gameplay_note_sprite_cache_ = {};
    gameplay_note_sprite_cache_.lane_count = lane_count;
    for (int lane = 0; lane < lane_count; ++lane) {
        uint32_t color = 0xF6F8FF;
        if (static_cast<std::size_t>(lane) < data.lane_color_count) {
            color = data.lane_colors[static_cast<std::size_t>(lane)];
        } else if (!gameplay_lane_uses_white_note(lane + 1)) {
            color = 0x4F80FF;
        }
        gameplay_note_sprite_cache_.lane_colors[static_cast<std::size_t>(lane)] = color;

        if (!create_note_bitmap(96.0f, 24.0f,
                                gameplay_note_fill_color(color),
                                gameplay_note_border_color(color),
                                d2d_->lane_note_head_bitmaps[static_cast<std::size_t>(lane)])) {
            invalidate_gameplay_note_sprite_cache();
            return false;
        }
        if (!create_note_bitmap(92.0f, 20.0f,
                                gameplay_note_fill_color(color),
                                gameplay_note_border_color(color),
                                d2d_->lane_note_tail_bitmaps[static_cast<std::size_t>(lane)])) {
            invalidate_gameplay_note_sprite_cache();
            return false;
        }
    }

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
    if (!d2d_ || !d2d_->d2d_context || !d2d_->d2d_target) {
        return false;
    }
    if (d2d_->gameplay_static_command_list &&
        gameplay_static_cache_.lane_count == lane_count &&
        std::abs(gameplay_static_cache_.judgement_line_position - judgement_line_position) < 1e-6) {
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

    const float field_height = kGameplayFieldBottom - kGameplayFieldTop;
    const D2D1_RECT_F field_rect =
        D2D1::RectF(kGameplayFieldLeft, kGameplayFieldTop, kGameplayFieldRight, kGameplayFieldBottom);
    const D2D1_ROUNDED_RECT field_rr = D2D1::RoundedRect(field_rect, 16.0f, 16.0f);
    if (d2d_->panel_brush) {
        ctx->FillRoundedRectangle(field_rr, d2d_->panel_brush.Get());
    }
    if (d2d_->button_border_brush) {
        ctx->DrawRoundedRectangle(field_rr, d2d_->button_border_brush.Get(), 1.4f);
    }

    const float lane_width = (kGameplayFieldRight - kGameplayFieldLeft) / static_cast<float>(lane_count);
    for (int lane = 0; lane < lane_count; ++lane) {
        const float x0 = kGameplayFieldLeft + static_cast<float>(lane) * lane_width;
        const float x1 = x0 + lane_width;
        const D2D1_RECT_F lane_rect =
            D2D1::RectF(x0 + 2.0f, kGameplayFieldTop + 2.0f, x1 - 2.0f, kGameplayFieldBottom - 2.0f);
        if (d2d_->card_brush) {
            d2d_->card_brush->SetOpacity((lane % 2 == 0) ? 0.38f : 0.28f);
            ctx->FillRectangle(lane_rect, d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(1.0f);
        }
        if (d2d_->button_border_brush) {
            ctx->DrawLine(D2D1::Point2F(x1, kGameplayFieldTop), D2D1::Point2F(x1, kGameplayFieldBottom),
                          d2d_->button_border_brush.Get(), 1.0f);
        }
    }

    if (d2d_->accent_brush) {
        const float hit_line_y = gameplay_field_y(kGameplayFieldTop, field_height, judgement_line_position);
        ctx->DrawLine(D2D1::Point2F(kGameplayFieldLeft, hit_line_y), D2D1::Point2F(kGameplayFieldRight, hit_line_y),
                      d2d_->accent_brush.Get(), 2.2f);
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

void MenuWindow::on_mouse_click(int window_x, int window_y, bool double_click) {
    if (!is_input_foreground()) {
        return;
    }
    if (!double_click && suppress_next_left_button_up_) {
        suppress_next_left_button_up_ = false;
        return;
    }
    if (scale_ <= 0.0f) {
        return;
    }

    const float x = (static_cast<float>(window_x) - offset_x_) / scale_;
    const float y = (static_cast<float>(window_y) - offset_y_) / scale_;
    if (!std::isfinite(x) || !std::isfinite(y)) {
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
    destroy_window();
    invalidate_gameplay_note_sprite_cache();
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
        for (auto& bitmap : d2d_->lane_note_tail_bitmaps) {
            bitmap.Reset();
        }
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
        d2d_->title_format.Reset();
        d2d_->dwrite_factory.Reset();
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

    DWORD style = WS_POPUP;
    const MonitorDisplayInfo monitor = query_monitor_display_info(nullptr);
    int x = monitor.rect.left;
    int y = monitor.rect.top;
    resolve_window_bounds(config, monitor, width_, height_, x, y);

    const std::wstring title = to_wide(config.title);
    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClassName,
        title.c_str(),
        style,
        x,
        y,
        static_cast<int>(width_),
        static_cast<int>(height_),
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
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, static_cast<int>(width_), static_cast<int>(height_),
                 SWP_SHOWWINDOW);
    SetWindowPos(hwnd, HWND_NOTOPMOST, x, y, static_cast<int>(width_), static_cast<int>(height_),
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

    fullscreen_ = false;
    if (config.display_mode == "fullscreen") {
        const HRESULT fs_hr = d2d_->swap_chain->SetFullscreenState(TRUE, nullptr);
        if (FAILED(fs_hr)) {
            std::cerr << "[MenuWindow::initialize] SetFullscreenState(TRUE) failed hr=0x"
                      << std::hex << static_cast<unsigned long>(fs_hr) << std::dec << std::endl;
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

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           32.0f, L"en-us", &d2d_->title_format);
    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_REGULAR,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           18.0f, L"en-us", &d2d_->body_format);
    d2d_->dwrite_factory->CreateTextFormat(L"Consolas", nullptr,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           18.0f, L"en-us", &d2d_->mono_format);

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
                                           20.0f, L"en-us", &d2d_->song_artist_format);
    if (d2d_->song_artist_format) {
        d2d_->song_artist_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        d2d_->song_artist_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }

    d2d_->dwrite_factory->CreateTextFormat(L"Bahnschrift", nullptr,
                                           DWRITE_FONT_WEIGHT_REGULAR,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           18.0f, L"en-us", &d2d_->hud_format);
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
    if (d2d_->bg_brush) {
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
        ctx->FillRectangle(D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight), d2d_->glow_brush.Get());
    }

    hit_regions_.clear();
    auto register_hit = [this](const D2D1_RECT_F& rect,
                               MenuHitTargetKind kind,
                               int index,
                               MenuHitPart part = MenuHitPart::Activate) {
        if (kind == MenuHitTargetKind::None || index < 0) {
            return;
        }
        hit_regions_.push_back(HitRegion{kind, index, part, rect.left, rect.top, rect.right, rect.bottom});
    };

    auto draw_footer = [&](std::string_view profile, int64_t high_score, std::string_view track) {
        const float margin = 80.0f;
        const float bar_height = 84.0f;
        const float bar_bottom = kBaseHeight - 24.0f;
        const float bar_top = bar_bottom - bar_height;
        const D2D1_RECT_F rect = D2D1::RectF(margin, bar_top, kBaseWidth - margin, bar_bottom);
        const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 16.0f, 16.0f);
        if (d2d_->footer_brush) {
            ctx->FillRoundedRectangle(rr, d2d_->footer_brush.Get());
        }
        if (d2d_->button_border_brush) {
            ctx->DrawRoundedRectangle(rr, d2d_->button_border_brush.Get(), 1.5f);
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

        d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        ctx->DrawText(profile_w.c_str(), static_cast<UINT32>(profile_w.size()),
                      d2d_->hud_format.Get(), left_rect, d2d_->text_brush.Get());

        d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        ctx->DrawText(score_w.c_str(), static_cast<UINT32>(score_w.size()),
                      d2d_->hud_format.Get(), center_rect, d2d_->text_brush.Get());

        d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        ctx->DrawText(track_w.c_str(), static_cast<UINT32>(track_w.size()),
                      d2d_->hud_format.Get(), right_rect, d2d_->text_brush.Get());

        d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    };

    auto draw_performance_overlay = [&]() {
        if (!data.performance.visible) {
            return;
        }

        const D2D1_RECT_F panel_rect = D2D1::RectF(kBaseWidth - 470.0f, 44.0f, kBaseWidth - 44.0f, 432.0f);
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
            performance_overlay_cache_.graph_revision != data.performance.graph_revision;
        const bool metrics_changed =
            graph_changed || performance_overlay_cache_.metrics_revision != data.performance.metrics_revision;

        const D2D1_RECT_F graph_rect =
            D2D1::RectF(panel_rect.left + 24.0f, panel_rect.top + 58.0f, panel_rect.right - 24.0f, panel_rect.top + 198.0f);
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

        constexpr const wchar_t* kPerfRowLabels[] = {
            L"AVG MS",
            L"AVG FPS",
            L"MAX FPS",
            L"0.1% FPS",
            L"0.01% FPS",
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

            const float field_left = rect.left + 28.0f;
            const float field_right = rect.right - 28.0f;
            const float field_top = rect.top + 132.0f;
            const float field_bottom = rect.bottom - 152.0f;
            const float field_height = field_bottom - field_top;
            const int lane_count = std::clamp(preview.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
            const float lane_width = (field_right - field_left) / static_cast<float>(lane_count);
            const float hit_line_y =
                gameplay_field_y(field_top,
                                 field_height,
                                 clamp_gameplay_judgement_line(preview.judgement_line_position));
            const float note_width = gameplay_note_draw_width(lane_width, preview.note_width_scale);
            const float head_half_h = 11.0f * clamp_gameplay_note_height_scale(preview.note_height_scale);
            const float tail_half_h = 9.0f * clamp_gameplay_note_height_scale(preview.note_height_scale);

            const D2D1_ROUNDED_RECT field_rr =
                D2D1::RoundedRect(D2D1::RectF(field_left, field_top, field_right, field_bottom), 14.0f, 14.0f);
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
                    ctx->FillRectangle(D2D1::RectF(x0 + 2.0f, field_top + 2.0f, x1 - 2.0f, field_bottom - 2.0f),
                                       d2d_->note_fill_brush.Get());
                }
                if (d2d_->button_border_brush) {
                    ctx->DrawLine(D2D1::Point2F(x1, field_top), D2D1::Point2F(x1, field_bottom),
                                  d2d_->button_border_brush.Get(), 1.0f);
                }
            }

            if (d2d_->accent_brush) {
                ctx->DrawLine(D2D1::Point2F(field_left, hit_line_y), D2D1::Point2F(field_right, hit_line_y),
                              d2d_->accent_brush.Get(), 2.0f);
            }

            for (int lane = 0; lane < lane_count; ++lane) {
                const uint32_t rgb = preview.lane_colors[static_cast<std::size_t>(lane)];
                const float lane_center = field_left + (static_cast<float>(lane) + 0.5f) * lane_width;
                const float x0 = lane_center - note_width * 0.5f;
                const float x1 = lane_center + note_width * 0.5f;
                const float y = field_top + field_height * (0.16f + 0.09f * static_cast<float>((lane + 1) % 4));
                if (d2d_->note_fill_brush) {
                    d2d_->note_fill_brush->SetColor(gameplay_note_fill_color(rgb));
                }
                if (d2d_->note_border_brush) {
                    d2d_->note_border_brush->SetColor(gameplay_note_border_color(rgb));
                }
                if (d2d_->note_hold_brush) {
                    d2d_->note_hold_brush->SetColor(gameplay_note_hold_color(rgb));
                }

                if (lane + 1 == preview.selected_lane && d2d_->note_hold_brush) {
                    const float tail_y = std::min(field_bottom - 20.0f, hit_line_y + field_height * 0.12f);
                    const float hold_half_width = std::max(4.0f, note_width * 0.30f);
                    const D2D1_RECT_F hold_rect =
                        D2D1::RectF(lane_center - hold_half_width, y + tail_half_h, lane_center + hold_half_width, tail_y - head_half_h);
                    if (hold_rect.bottom > hold_rect.top) {
                        ctx->FillRectangle(hold_rect, d2d_->note_hold_brush.Get());
                    }
                    const D2D1_RECT_F tail_rect =
                        D2D1::RectF(x0 + 2.0f, tail_y - tail_half_h, x1 - 2.0f, tail_y + tail_half_h);
                    if (d2d_->note_fill_brush) {
                        ctx->FillRectangle(tail_rect, d2d_->note_fill_brush.Get());
                    }
                    if (d2d_->note_border_brush) {
                        ctx->DrawRectangle(tail_rect, d2d_->note_border_brush.Get(), 1.2f);
                    }
                }

                const D2D1_RECT_F note_rect = D2D1::RectF(x0, y - head_half_h, x1, y + head_half_h);
                if (d2d_->note_fill_brush) {
                    ctx->FillRectangle(note_rect, d2d_->note_fill_brush.Get());
                }
                if (d2d_->note_border_brush) {
                    ctx->DrawRectangle(note_rect, d2d_->note_border_brush.Get(), 1.2f);
                }
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
            const float row_right = has_skin_preview ? (right - preview_width - preview_gap) : (right - 24.0f);
            const float row_height = 48.0f;
            const float row_gap = 8.0f;
            const float value_width = has_skin_preview ? 240.0f : 340.0f;
            const float action_width = 56.0f;
            const float action_gap = 10.0f;
            float row_y = top + 24.0f;

            if (has_skin_preview) {
                draw_skin_preview_panel(data.generic.skin_preview,
                                        D2D1::RectF(row_right + preview_gap, top + 24.0f, right - 24.0f, bottom - 24.0f));
            }

            for (const auto& row : data.generic.rows) {
                if (row_y + row_height > bottom - 20.0f) {
                    break;
                }

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
                if (d2d_->body_format && d2d_->text_brush) {
                    ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                                  d2d_->body_format.Get(), label_rect, d2d_->text_brush.Get());
                }

                if (!row.value.empty() && d2d_->body_format && d2d_->text_brush) {
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
                        d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                        ctx->DrawText(value_w.c_str(), static_cast<UINT32>(value_w.size()),
                                      d2d_->body_format.Get(), value_rect, d2d_->text_brush.Get());
                        d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

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
                        d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                        ctx->DrawText(value_w.c_str(), static_cast<UINT32>(value_w.size()),
                                      d2d_->body_format.Get(), value_rect, d2d_->text_brush.Get());
                        d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                    }
                }

                if (row.activatable) {
                    register_hit(row_rect, row.target_kind, row.row_index, MenuHitPart::Activate);
                }
                row_y += row_height + row_gap;
            }

            for (const auto& note : data.generic.notes) {
                if (row_y + 24.0f > bottom - 12.0f) {
                    break;
                }
                const std::wstring note_w = to_wide(note);
                const D2D1_RECT_F note_rect = D2D1::RectF(row_left + 6.0f, row_y, row_right - 6.0f, row_y + 24.0f);
                if (d2d_->body_format && d2d_->muted_brush) {
                    ctx->DrawText(note_w.c_str(), static_cast<UINT32>(note_w.size()),
                                  d2d_->body_format.Get(), note_rect, d2d_->muted_brush.Get());
                }
                row_y += 28.0f;
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
        const float bar_base_y = 150.0f;
        const int bar_count = 18;
        const float bar_w = 18.0f;
        const float bar_gap = 12.0f;
        const float total_w = bar_count * bar_w + (bar_count - 1) * bar_gap;
        const float start_x = (kBaseWidth - total_w) * 0.5f;

        if (d2d_->accent_brush) {
            for (int i = 0; i < bar_count; ++i) {
                const double phase = tick * 2.2 + static_cast<double>(i) * 0.45;
                const float height = 18.0f + 66.0f * static_cast<float>(0.5 + 0.5 * std::sin(phase));
                const float x0 = start_x + static_cast<float>(i) * (bar_w + bar_gap);
                const D2D1_ROUNDED_RECT bar =
                    D2D1::RoundedRect(D2D1::RectF(x0, bar_base_y - height, x0 + bar_w, bar_base_y),
                                      4.0f, 4.0f);
                ctx->FillRoundedRectangle(bar, d2d_->accent_brush.Get());
            }
        }

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
            const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 18.0f, 18.0f);
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

            if (fill) {
                set_brush_points(fill, rect);
                ctx->FillRoundedRectangle(rr, fill);
            } else if (d2d_->button_brush) {
                ctx->FillRoundedRectangle(rr, d2d_->button_brush.Get());
            }

            ID2D1SolidColorBrush* border = button.selected ? d2d_->accent_brush.Get() : d2d_->button_border_brush.Get();
            if (border) {
                ctx->DrawRoundedRectangle(rr, border, button.selected ? 3.0f : 2.0f);
            }
            if (button.selected && d2d_->accent_brush) {
                d2d_->accent_brush->SetOpacity(0.35f);
                const D2D1_RECT_F glow_rect =
                    D2D1::RectF(rect.left - 6.0f, rect.top - 6.0f, rect.right + 6.0f, rect.bottom + 6.0f);
                const D2D1_ROUNDED_RECT glow_rr = D2D1::RoundedRect(glow_rect, 22.0f, 22.0f);
                ctx->DrawRoundedRectangle(glow_rr, d2d_->accent_brush.Get(), 6.0f);
                d2d_->accent_brush->SetOpacity(1.0f);
            }

            const D2D1_RECT_F icon_rect = D2D1::RectF(rect.left + 26.0f, rect.top, rect.left + 140.0f, rect.bottom);
            const D2D1_RECT_F label_rect =
                D2D1::RectF(rect.left + 140.0f, rect.top, rect.right - 20.0f, rect.bottom);

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

        draw_footer(data.title.profile, data.title.high_score, data.title.track);
    };

    auto draw_song_select = [&]() {
        const D2D1_RECT_F header_rect = D2D1::RectF(0.0f, 70.0f, kBaseWidth, 170.0f);
        const std::wstring header_w = L"TENG SELECT";
        ID2D1Brush* header_brush = d2d_->logo_brush ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                                    : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
        if (d2d_->logo_brush) {
            set_brush_points(d2d_->logo_brush.Get(), header_rect);
        }
        if (d2d_->header_format && header_brush) {
            ctx->DrawText(header_w.c_str(), static_cast<UINT32>(header_w.size()),
                          d2d_->header_format.Get(), header_rect, header_brush);
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
            ctx->DrawText(status_w.c_str(), static_cast<UINT32>(status_w.size()),
                          d2d_->hud_format.Get(), status_rect, d2d_->muted_brush.Get());

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
        const float nav_top = 300.0f;
        const float nav_height = 86.0f;
        const float nav_gap = 22.0f;
        for (std::size_t i = 0; i < data.song_select.left_nav.size(); ++i) {
            const auto& item = data.song_select.left_nav[i];
            const float y0 = nav_top + static_cast<float>(i) * (nav_height + nav_gap);
            const D2D1_RECT_F rect = D2D1::RectF(nav_left, y0, nav_left + nav_width, y0 + nav_height);
            const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 14.0f, 14.0f);
            register_hit(rect, MenuHitTargetKind::SongNavButton, static_cast<int>(i));
            ID2D1SolidColorBrush* fill = item.selected ? d2d_->button_selected_brush.Get() : d2d_->button_brush.Get();
            if (fill) {
                ctx->FillRoundedRectangle(rr, fill);
            }
            ID2D1SolidColorBrush* border = item.selected ? d2d_->accent_brush.Get() : d2d_->button_border_brush.Get();
            if (border) {
                ctx->DrawRoundedRectangle(rr, border, item.selected ? 2.4f : 1.6f);
            }

            const D2D1_RECT_F icon_rect = D2D1::RectF(rect.left + 16.0f, rect.top, rect.left + 78.0f, rect.bottom);
            const bool has_detail = !item.detail.empty();
            const D2D1_RECT_F label_rect = has_detail
                                               ? D2D1::RectF(rect.left + 82.0f, rect.top + 10.0f, rect.right - 18.0f,
                                                             rect.top + 50.0f)
                                               : D2D1::RectF(rect.left + 82.0f, rect.top, rect.right - 18.0f, rect.bottom);
            const D2D1_RECT_F detail_rect =
                D2D1::RectF(rect.left + 82.0f, rect.top + 44.0f, rect.right - 18.0f, rect.bottom - 8.0f);

            if (d2d_->menu_icon_format && d2d_->text_brush) {
                const std::wstring icon_w = to_wide(item.icon);
                if (!icon_w.empty()) {
                    ctx->DrawText(icon_w.c_str(), static_cast<UINT32>(icon_w.size()),
                                  d2d_->menu_icon_format.Get(), icon_rect, d2d_->text_brush.Get());
                }
            }
            if (d2d_->title_format && d2d_->text_brush) {
                const std::wstring label_w = to_wide(item.label);
                ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                              d2d_->title_format.Get(), label_rect, d2d_->text_brush.Get());
            }
            if (has_detail && d2d_->body_format) {
                ID2D1SolidColorBrush* detail_brush = item.selected ? d2d_->text_brush.Get() : d2d_->muted_brush.Get();
                if (detail_brush) {
                    const std::wstring detail_w = to_wide(item.detail);
                    ctx->DrawText(detail_w.c_str(), static_cast<UINT32>(detail_w.size()),
                                  d2d_->body_format.Get(), detail_rect, detail_brush);
                }
            }
        }

        const D2D1_RECT_F list_rect = D2D1::RectF(450.0f, 220.0f, 1270.0f, 930.0f);
        const D2D1_ROUNDED_RECT list_rr = D2D1::RoundedRect(list_rect, 18.0f, 18.0f);
        if (d2d_->panel_brush) {
            ctx->FillRoundedRectangle(list_rr, d2d_->panel_brush.Get());
        }
        if (d2d_->button_border_brush) {
            ctx->DrawRoundedRectangle(list_rr, d2d_->button_border_brush.Get(), 1.6f);
        }
        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring list_header_w = data.song_select.showing_sources
                                                   ? to_wide("RECENT SOURCES")
                                                   : (data.song_select.showing_records
                                                          ? to_wide("LOCAL RECORDS")
                                                          : to_wide_with_placeholder(data.song_select.current_source_name,
                                                                                    "<invalid source>",
                                                                                    "song-select-header"));
            const D2D1_RECT_F list_header_rect =
                D2D1::RectF(list_rect.left + 28.0f, list_rect.top + 18.0f, list_rect.right - 28.0f, list_rect.top + 60.0f);
            ctx->DrawText(list_header_w.c_str(), static_cast<UINT32>(list_header_w.size()),
                          d2d_->title_format.Get(), list_header_rect, d2d_->text_brush.Get());
        }

        const float card_left = list_rect.left + 28.0f;
        const float card_right = list_rect.right - 28.0f;
        const float card_top = list_rect.top + 70.0f;
        const float card_h = 110.0f;
        const float card_gap = 18.0f;

        for (std::size_t i = 0; i < data.song_select.songs.size(); ++i) {
            const auto& song = data.song_select.songs[i];
            const float y0 = card_top + static_cast<float>(i) * (card_h + card_gap);
            const D2D1_RECT_F card = D2D1::RectF(card_left, y0, card_right, y0 + card_h);
            const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(card, 14.0f, 14.0f);
            register_hit(card, MenuHitTargetKind::SongCard, song.song_index);
            ID2D1SolidColorBrush* fill = song.selected ? d2d_->button_selected_brush.Get() : d2d_->card_brush.Get();
            if (fill) {
                ctx->FillRoundedRectangle(rr, fill);
            }
            ID2D1SolidColorBrush* border = song.selected ? d2d_->accent_brush.Get() : d2d_->button_border_brush.Get();
            if (border) {
                ctx->DrawRoundedRectangle(rr, border, song.selected ? 2.2f : 1.4f);
            }

            const D2D1_RECT_F jacket =
                D2D1::RectF(card.left + 18.0f, card.top + 12.0f, card.left + 158.0f, card.bottom - 12.0f);
            const D2D1_ROUNDED_RECT jacket_rr = D2D1::RoundedRect(jacket, 10.0f, 10.0f);
            if (d2d_->accent_brush) {
                const D2D1_COLOR_F color = jacket_color(song.title);
                d2d_->accent_brush->SetColor(color);
                d2d_->accent_brush->SetOpacity(0.85f);
                ctx->FillRoundedRectangle(jacket_rr, d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(1.0f);
                d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(jacket_rr, d2d_->button_border_brush.Get(), 1.2f);
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
                ctx->DrawText(title_w.c_str(), static_cast<UINT32>(title_w.size()),
                              d2d_->song_title_format.Get(), title_rect, d2d_->text_brush.Get());
            }
            if (d2d_->song_artist_format && d2d_->muted_brush) {
                const std::wstring artist_w =
                    to_wide_with_placeholder(song.artist, "<invalid artist>",
                                             "song-card-artist:" + std::to_string(song.song_index));
                ctx->DrawText(artist_w.c_str(), static_cast<UINT32>(artist_w.size()),
                              d2d_->song_artist_format.Get(), artist_rect, d2d_->muted_brush.Get());
            }

            if (data.song_select.showing_sources || data.song_select.showing_records) {
                if (!song.detail.empty() && d2d_->body_format) {
                    ID2D1SolidColorBrush* detail_brush = song.selected ? d2d_->text_brush.Get() : d2d_->muted_brush.Get();
                    if (detail_brush) {
                        const std::wstring detail_w =
                            to_wide_with_placeholder(song.detail, "<invalid detail>",
                                                     "song-card-detail:" + std::to_string(song.song_index));
                        ctx->DrawText(detail_w.c_str(), static_cast<UINT32>(detail_w.size()),
                                      d2d_->body_format.Get(), detail_rect, detail_brush);
                    }
                }
                if (data.song_select.showing_sources && d2d_->body_format && d2d_->text_brush) {
                    const std::string count_label =
                        (song.level > 0) ? (std::to_string(song.level) + " SONGS") : std::string("OPEN");
                    const std::wstring count_w = to_wide(count_label);
                    const D2D1_RECT_F count_rect =
                        D2D1::RectF(card.right - 170.0f, card.top + 18.0f, card.right - 18.0f, card.bottom - 18.0f);
                    d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                    ctx->DrawText(count_w.c_str(), static_cast<UINT32>(count_w.size()),
                                  d2d_->body_format.Get(), count_rect, d2d_->text_brush.Get());
                    d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
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
                const std::wstring level_w = to_wide(level_text);
                const D2D1_RECT_F level_rect =
                    D2D1::RectF(card.right - 170.0f, card.top + 18.0f, card.right - 18.0f, card.bottom - 18.0f);
                ctx->DrawText(level_w.c_str(), static_cast<UINT32>(level_w.size()),
                              d2d_->body_format.Get(), level_rect, d2d_->text_brush.Get());
            }
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
            ctx->DrawText(empty_w.c_str(), static_cast<UINT32>(empty_w.size()),
                          d2d_->title_format.Get(), empty_rect, d2d_->muted_brush.Get());
            d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        const D2D1_RECT_F right_rect = D2D1::RectF(1320.0f, 300.0f, 1800.0f, 820.0f);
        const D2D1_ROUNDED_RECT right_rr = D2D1::RoundedRect(right_rect, 18.0f, 18.0f);
        if (d2d_->panel_brush) {
            ctx->FillRoundedRectangle(right_rr, d2d_->panel_brush.Get());
        }
        if (d2d_->accent_brush) {
            ctx->DrawRoundedRectangle(right_rr, d2d_->accent_brush.Get(), 1.8f);
        }

        const float stats_left = right_rect.left + 24.0f;
        const float stats_right = right_rect.right - 24.0f;
        float stats_y = right_rect.top + 160.0f;
        const float row_h = 34.0f;

        auto draw_stat_row = [&](std::string_view label, int64_t value) {
            if (!d2d_->stats_label_format || !d2d_->stats_value_format || !d2d_->text_brush) {
                return;
            }
            const std::wstring label_w = to_wide(std::string(label));
            const std::wstring value_w = to_wide(std::to_string(value));
            const D2D1_RECT_F label_rect = D2D1::RectF(stats_left, stats_y, stats_right - 120.0f, stats_y + row_h);
            const D2D1_RECT_F value_rect = D2D1::RectF(stats_right - 120.0f, stats_y, stats_right, stats_y + row_h);
            ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                          d2d_->stats_label_format.Get(), label_rect, d2d_->text_brush.Get());
            ctx->DrawText(value_w.c_str(), static_cast<UINT32>(value_w.size()),
                          d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
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
            ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                          d2d_->stats_label_format.Get(), label_rect, d2d_->text_brush.Get());
            ctx->DrawText(value_w.c_str(), static_cast<UINT32>(value_w.size()),
                          d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
            stats_y += row_h;
        };

        if (data.song_select.showing_sources) {
            if (d2d_->song_title_format && d2d_->text_brush) {
                const std::wstring source_title_w =
                    data.song_select.selected_source_name.empty()
                        ? to_wide("No Source Selected")
                        : to_wide_with_placeholder(data.song_select.selected_source_name,
                                                   "<invalid source>",
                                                   "selected-source-name");
                const D2D1_RECT_F source_title_rect =
                    D2D1::RectF(right_rect.left + 24.0f, right_rect.top + 24.0f, right_rect.right - 24.0f, right_rect.top + 84.0f);
                ctx->DrawText(source_title_w.c_str(), static_cast<UINT32>(source_title_w.size()),
                              d2d_->song_title_format.Get(), source_title_rect, d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring source_path_w =
                    to_wide_with_placeholder(data.song_select.selected_source_path.empty()
                                                 ? data.song_select.current_source_path
                                                 : data.song_select.selected_source_path,
                                             "<invalid path>",
                                             "selected-source-path");
                const D2D1_RECT_F source_path_rect =
                    D2D1::RectF(right_rect.left + 24.0f, right_rect.top + 96.0f, right_rect.right - 24.0f, right_rect.top + 150.0f);
                ctx->DrawText(source_path_w.c_str(), static_cast<UINT32>(source_path_w.size()),
                              d2d_->body_format.Get(), source_path_rect, d2d_->muted_brush.Get());
            }

            draw_stat_row("ROOTS", data.song_select.source_count);
            draw_stat_row("CURRENT", data.song_select.song_count);
            draw_stat_row("SELECTED",
                          data.song_select.selected_source_song_count >= 0 ? data.song_select.selected_source_song_count : 0);
            if (d2d_->stats_label_format && d2d_->stats_value_format && d2d_->text_brush) {
                const std::wstring label_w = L"STATUS";
                const std::wstring value_w = to_wide(data.song_select.selected_source_active ? "ACTIVE" : "READY");
                const D2D1_RECT_F label_rect = D2D1::RectF(stats_left, stats_y, stats_right - 120.0f, stats_y + row_h);
                const D2D1_RECT_F value_rect = D2D1::RectF(stats_right - 160.0f, stats_y, stats_right, stats_y + row_h);
                ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                              d2d_->stats_label_format.Get(), label_rect, d2d_->text_brush.Get());
                ctx->DrawText(value_w.c_str(), static_cast<UINT32>(value_w.size()),
                              d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
            }
        } else if (data.song_select.showing_records) {
            const std::wstring rank_w = to_wide(data.song_select.rank.empty() ? std::string("--") : data.song_select.rank);
            if (d2d_->rank_format && header_brush) {
                const D2D1_RECT_F rank_rect = D2D1::RectF(right_rect.left, right_rect.top + 8.0f,
                                                         right_rect.right, right_rect.top + 120.0f);
                ctx->DrawText(rank_w.c_str(), static_cast<UINT32>(rank_w.size()),
                              d2d_->rank_format.Get(), rank_rect, header_brush);
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring status_w = to_wide(data.song_select.selected_record_status);
                const std::wstring time_w = to_wide(data.song_select.selected_record_created_utc);
                const D2D1_RECT_F status_rect =
                    D2D1::RectF(right_rect.left + 24.0f, right_rect.top + 118.0f, right_rect.right - 24.0f, right_rect.top + 150.0f);
                const D2D1_RECT_F time_rect =
                    D2D1::RectF(right_rect.left + 24.0f, right_rect.top + 148.0f, right_rect.right - 24.0f, right_rect.top + 182.0f);
                ctx->DrawText(status_w.c_str(), static_cast<UINT32>(status_w.size()),
                              d2d_->body_format.Get(), status_rect, d2d_->muted_brush.Get());
                ctx->DrawText(time_w.c_str(), static_cast<UINT32>(time_w.size()),
                              d2d_->body_format.Get(), time_rect, d2d_->muted_brush.Get());
            }

            stats_y = right_rect.top + 200.0f;
            draw_stat_row("SCORE", data.song_select.best_score);
            draw_stat_text_row("ACCURACY", format_decimal(data.song_select.accuracy, 2) + "%");
            draw_stat_row("MAX COMBO", data.song_select.max_combo);
            draw_stat_row("PERFECT", data.song_select.perfect);
            draw_stat_row("GREAT", data.song_select.great);
            draw_stat_row("GOOD", data.song_select.good);
            draw_stat_row("BAD", data.song_select.bad);
            draw_stat_row("MISS", data.song_select.miss);

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
                    D2D1::RectF(right_rect.left + 24.0f, right_rect.bottom - 144.0f, right_rect.right - 24.0f, right_rect.bottom - 112.0f);
                const D2D1_RECT_F replay_detail_rect =
                    D2D1::RectF(right_rect.left + 24.0f, right_rect.bottom - 110.0f, right_rect.right - 24.0f, right_rect.bottom - 78.0f);
                const D2D1_RECT_F replay_meta_rect =
                    D2D1::RectF(right_rect.left + 24.0f, right_rect.bottom - 76.0f, right_rect.right - 24.0f, right_rect.bottom - 44.0f);
                ctx->DrawText(replay_file_w.c_str(), static_cast<UINT32>(replay_file_w.size()),
                              d2d_->body_format.Get(), replay_file_rect, d2d_->muted_brush.Get());
                ctx->DrawText(replay_detail_w.c_str(), static_cast<UINT32>(replay_detail_w.size()),
                              d2d_->body_format.Get(), replay_detail_rect, d2d_->muted_brush.Get());
                ctx->DrawText(replay_meta_w.c_str(), static_cast<UINT32>(replay_meta_w.size()),
                              d2d_->body_format.Get(), replay_meta_rect, d2d_->muted_brush.Get());
            }
        } else {
            const std::wstring rank_w = to_wide(data.song_select.rank.empty() ? std::string("--") : data.song_select.rank);
            if (d2d_->rank_format && header_brush) {
                const D2D1_RECT_F rank_rect = D2D1::RectF(right_rect.left, right_rect.top + 10.0f,
                                                         right_rect.right, right_rect.top + 140.0f);
                ctx->DrawText(rank_w.c_str(), static_cast<UINT32>(rank_w.size()),
                              d2d_->rank_format.Get(), rank_rect, header_brush);
            }

            draw_stat_text_row("SORT", data.song_select.sort_summary);
            draw_stat_text_row("FILTER", data.song_select.browser_summary);
            draw_stat_row("BEST", data.song_select.best_score);
            draw_stat_row("MAX COMBO", data.song_select.max_combo);
            draw_stat_row("PERFECT", data.song_select.perfect);
            draw_stat_row("GREAT", data.song_select.great);
            draw_stat_row("GOOD", data.song_select.good);
            draw_stat_row("BAD", data.song_select.bad);
            draw_stat_row("MISS", data.song_select.miss);
        }

        if (d2d_->hud_format && d2d_->muted_brush) {
            const std::wstring hint_w = data.song_select.showing_sources
                                            ? L"ENTER  OPEN     F2  BROWSE     ESC  BACK"
                                            : (data.song_select.showing_records
                                                   ? L"ENTER  VIEW RESULT     BACKSPACE  SONGS     ESC  BACK"
                                                   : L"ENTER  SELECT     BROWSE  SEARCH/FILTER     BACKSPACE  SOURCES");
            const D2D1_RECT_F hint_rect = D2D1::RectF(right_rect.left + 10.0f, right_rect.bottom - 60.0f,
                                                     right_rect.right - 10.0f, right_rect.bottom - 18.0f);
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            ctx->DrawText(hint_w.c_str(), static_cast<UINT32>(hint_w.size()),
                          d2d_->hud_format.Get(), hint_rect, d2d_->muted_brush.Get());
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        draw_footer(data.song_select.profile, data.song_select.high_score, data.song_select.track);
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
            const std::wstring detail_w = to_wide("ENTER or ESC to return");
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

        if (d2d_->muted_brush && d2d_->hud_format) {
            for (int line = 0; line <= 4; ++line) {
                const float t = static_cast<float>(line) / 4.0f;
                const float y = plot_rect.bottom - t * (plot_rect.bottom - plot_rect.top);
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
                                                       plot_rect.bottom - prev.value * (plot_rect.bottom - plot_rect.top));
                const D2D1_POINT_2F p1 = D2D1::Point2F(plot_rect.left + next.position * (plot_rect.right - plot_rect.left),
                                                       plot_rect.bottom - next.value * (plot_rect.bottom - plot_rect.top));
                ctx->DrawLine(p0, p1, d2d_->accent_brush.Get(), 3.2f);
            }
            const auto& tail = data.result.gauge_points.back();
            const D2D1_ELLIPSE tail_dot = D2D1::Ellipse(
                D2D1::Point2F(plot_rect.left + tail.position * (plot_rect.right - plot_rect.left),
                              plot_rect.bottom - tail.value * (plot_rect.bottom - plot_rect.top)),
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
            {"GD", data.result.good, D2D1::ColorF(0xFAE36E)},
            {"BD", data.result.bad, D2D1::ColorF(0xFF9F43)},
            {"PR", data.result.miss, D2D1::ColorF(0xFF6B6B)},
        };

        const int max_bar_count = std::max({1, data.result.perfect, data.result.great, data.result.good,
                                            data.result.bad, data.result.miss});
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

        float note_y = detail_rect.top + 326.0f;
        if (d2d_->body_format && d2d_->text_brush) {
            const std::wstring notes_w = L"Notes";
            const D2D1_RECT_F notes_rect =
                D2D1::RectF(detail_rect.left + 24.0f, note_y, detail_rect.right - 24.0f, note_y + 28.0f);
            ctx->DrawText(notes_w.c_str(), static_cast<UINT32>(notes_w.size()),
                          d2d_->body_format.Get(), notes_rect, d2d_->text_brush.Get());
        }
        note_y += 34.0f;
        for (std::size_t i = 0; i < data.result.notes.size() && i < 6; ++i) {
            draw_detail_line(note_y, data.result.notes[i], true);
            note_y += 30.0f;
        }

        const D2D1_RECT_F back_rect =
            D2D1::RectF(detail_rect.left + 24.0f, detail_rect.bottom - 78.0f, detail_rect.right - 24.0f,
                        detail_rect.bottom - 24.0f);
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
        const float note_width_scale = clamp_gameplay_note_width_scale(data.gameplay.note_width_scale);
        const float note_height_scale = clamp_gameplay_note_height_scale(data.gameplay.note_height_scale);

        int64_t display_sample = data.gameplay.current_sample;
        if (data.gameplay.sample_rate > 0 && data.gameplay.snapshot_time_ns > 0 &&
            !data.gameplay.finished && !data.gameplay.game_over) {
            const int64_t now_ns = timing::HighResClock::now_ns();
            const int64_t elapsed_ns = std::max<int64_t>(0, now_ns - data.gameplay.snapshot_time_ns);
            const double advanced_samples =
                static_cast<double>(elapsed_ns) * static_cast<double>(data.gameplay.sample_rate) / 1'000'000'000.0;
            display_sample += static_cast<int64_t>(std::llround(advanced_samples));
            display_sample += static_cast<int64_t>(std::llround(
                data.gameplay.visual_offset_ms * static_cast<double>(data.gameplay.sample_rate) / 1000.0));
            display_sample = std::max<int64_t>(0, display_sample);
            if (data.gameplay.duration_samples > 0) {
                display_sample = std::min(display_sample, data.gameplay.duration_samples);
            }
        }

        auto sample_to_y = [&](int64_t sample) -> float {
            const double delta = static_cast<double>(sample - display_sample);
            if (delta >= 0.0) {
                if (data.gameplay.lookahead_samples <= 0) {
                    return static_cast<float>(judgement_line_position);
                }
                const double t = std::clamp(delta / static_cast<double>(data.gameplay.lookahead_samples), 0.0, 1.0);
                const double y = judgement_line_position - t * (judgement_line_position - kGameplayYTop);
                return static_cast<float>(std::clamp(y, kGameplayYTop, kGameplayYBottom));
            }

            if (data.gameplay.past_samples <= 0) {
                return static_cast<float>(judgement_line_position);
            }
            const double t = std::clamp((-delta) / static_cast<double>(data.gameplay.past_samples), 0.0, 1.0);
            const double y = judgement_line_position + t * (kGameplayYBottom - judgement_line_position);
            return static_cast<float>(std::clamp(y, kGameplayYTop, kGameplayYBottom));
        };

        if (gameplay_hud_cache_.revision != data.gameplay.revision) {
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
                        "  GD " + std::to_string(data.gameplay.gd) +
                        "  BD " + std::to_string(data.gameplay.bd) +
                        "  PR " + std::to_string(data.gameplay.pr));
            gameplay_hud_cache_.gauge_label_text = to_wide(data.gameplay.gauge_label);
            gameplay_hud_cache_.gauge_value_text =
                to_wide(std::to_string(static_cast<int>(std::llround(data.gameplay.gauge))) + "%");

            std::string feedback_text;
            if (data.gameplay.has_feedback) {
                feedback_text = data.gameplay.feedback;
                feedback_text += (data.gameplay.feedback_delta_ms >= 0.0) ? "  SLOW " : "  FAST ";
                feedback_text += format_decimal(std::abs(data.gameplay.feedback_delta_ms), 1) + "ms";
            } else {
                feedback_text = "READY";
            }
            gameplay_hud_cache_.feedback_text = to_wide(feedback_text);
            gameplay_hud_cache_.revision = data.gameplay.revision;
        }

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

        if (data.gameplay.loading && !data.gameplay.active) {
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

        const float field_left = kGameplayFieldLeft;
        const float field_right = kGameplayFieldRight;
        const float field_top = kGameplayFieldTop;
        const float field_bottom = kGameplayFieldBottom;
        const float field_height = field_bottom - field_top;
        const int lane_count = std::clamp(data.gameplay.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
        const float lane_width = (field_right - field_left) / static_cast<float>(lane_count);
        const float hit_line_y = gameplay_field_y(field_top, field_height, judgement_line_position);
        if (d2d_->gameplay_static_command_list) {
            ctx->DrawImage(d2d_->gameplay_static_command_list.Get());
        }

        const D2D1_ANTIALIAS_MODE saved_antialias = ctx->GetAntialiasMode();
        ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        for (std::size_t note_index = 0; note_index < data.gameplay.note_count; ++note_index) {
            const auto& note = data.gameplay.notes[note_index];
            const int lane = std::clamp(note.lane, 1, lane_count);
            const float lane_center = field_left + (static_cast<float>(lane) - 0.5f) * lane_width;
            const float note_width = gameplay_note_draw_width(lane_width, note_width_scale);
            const float x0 = lane_center - note_width * 0.5f;
            const float x1 = lane_center + note_width * 0.5f;
            const float y = gameplay_field_y(field_top, field_height, sample_to_y(note.start_sample));
            const float tail_y = gameplay_field_y(field_top, field_height, sample_to_y(note.tail_sample));
            const float head_half_h = 11.0f * note_height_scale;
            const float tail_half_h = 9.0f * note_height_scale;
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
            ID2D1Bitmap* note_tail_bitmap = d2d_->lane_note_tail_bitmaps[static_cast<std::size_t>(lane - 1)].Get();

            if (note.hold && note_hold_fill) {
                const float body_top = std::min(y, tail_y) + tail_half_h;
                const float body_bottom = std::max(y, tail_y) - head_half_h;
                const float hold_half_width = std::max(4.0f, note_width * 0.30f);
                const D2D1_RECT_F hold_body =
                    D2D1::RectF(lane_center - hold_half_width, body_top, lane_center + hold_half_width, body_bottom);
                if (body_bottom > body_top) {
                    ctx->FillRectangle(hold_body, note_hold_fill);
                }

                const D2D1_RECT_F tail_rect =
                    D2D1::RectF(x0 + 2.0f, tail_y - tail_half_h, x1 - 2.0f, tail_y + tail_half_h);
                if (note_tail_bitmap) {
                    ctx->DrawBitmap(note_tail_bitmap, tail_rect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                } else {
                    if (note_fill) {
                        ctx->FillRectangle(tail_rect, note_fill);
                    }
                    if (note_border) {
                        ctx->DrawRectangle(tail_rect, note_border, 1.2f);
                    }
                }
            }

            const D2D1_RECT_F note_rect = D2D1::RectF(x0, y - head_half_h, x1, y + head_half_h);
            if (note_head_bitmap) {
                ctx->DrawBitmap(note_head_bitmap, note_rect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            } else {
                if (note_fill) {
                    ctx->FillRectangle(note_rect, note_fill);
                }
                if (note_border) {
                    ctx->DrawRectangle(note_rect, note_border, 1.3f);
                }
            }
        }
        ctx->SetAntialiasMode(saved_antialias);

        if (d2d_->accent_brush && data.gameplay.lane_activity_count > 0) {
            const std::size_t count =
                std::min(data.gameplay.lane_activity_count, static_cast<std::size_t>(lane_count));
            for (std::size_t lane = 0; lane < count; ++lane) {
                const float activity = std::clamp(data.gameplay.lane_activity[lane], 0.0f, 1.0f);
                if (activity <= 0.01f) {
                    continue;
                }
                const float lane_center = field_left + (static_cast<float>(lane) + 0.5f) * lane_width;
                const float note_width = gameplay_note_draw_width(lane_width, note_width_scale);
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

        if (d2d_->title_format && d2d_->text_brush) {
            D2D1_COLOR_F judge_color = D2D1::ColorF(0xE8ECF1);
            if (data.gameplay.feedback == "PG") {
                judge_color = D2D1::ColorF(0x5EE5A7);
            } else if (data.gameplay.feedback == "GR") {
                judge_color = D2D1::ColorF(0x6EE7F2);
            } else if (data.gameplay.feedback == "GD") {
                judge_color = D2D1::ColorF(0xFAE36E);
            } else if (data.gameplay.feedback == "BD" || data.gameplay.feedback == "PR") {
                judge_color = D2D1::ColorF(0xFF6B6B);
            }
            d2d_->text_brush->SetColor(judge_color);
            const D2D1_RECT_F feedback_rect =
                D2D1::RectF(field_left, field_bottom + 22.0f, field_right, field_bottom + 72.0f);
            d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            ctx->DrawText(gameplay_hud_cache_.feedback_text.c_str(),
                          static_cast<UINT32>(gameplay_hud_cache_.feedback_text.size()),
                          d2d_->title_format.Get(), feedback_rect, d2d_->text_brush.Get());
            d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            d2d_->text_brush->SetColor(D2D1::ColorF(0xE8ECF1));
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

    draw_performance_overlay();

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
    if (!config_.vsync && (swap_chain_flags_ & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)) {
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

        const bool need_fullscreen_reset = fullscreen_ && (resolution_changed || display_mode_changed);
        if (need_fullscreen_reset || (fullscreen_ && config_.display_mode != "fullscreen")) {
            d2d_->swap_chain->SetFullscreenState(FALSE, nullptr);
            fullscreen_ = false;
        }

        width_ = next_width;
        height_ = next_height;
        if (hwnd) {
            SetWindowPos(hwnd,
                         nullptr,
                         next_x,
                         next_y,
                         static_cast<int>(next_width),
                         static_cast<int>(next_height),
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
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
            } else {
                fullscreen_ = want_fullscreen;
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

bool MenuWindow::recreate_targets() {
    if (!d2d_ || !d2d_->swap_chain || !d2d_->d2d_context) {
        return false;
    }

    invalidate_gameplay_note_sprite_cache();
    invalidate_gameplay_static_cache();
    d2d_->d2d_context->SetTarget(nullptr);
    d2d_->d2d_target.Reset();

    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    const HRESULT buffer_hr = d2d_->swap_chain->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(buffer_hr)) {
        std::cerr << "[MenuWindow::recreate_targets] GetBuffer failed hr=0x" << std::hex
                  << static_cast<unsigned long>(buffer_hr) << std::dec << std::endl;
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
