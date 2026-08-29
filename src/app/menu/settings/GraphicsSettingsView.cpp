#include "app/menu/settings/GraphicsSettingsView.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include "app/GraphicsTiming.h"
#include "util/Utf8Compat.h"

namespace tenriff::app::menu::settings {
namespace {

std::string localized(bool use_korean, std::string_view english, std::string_view korean) {
    return std::string(use_korean ? korean : english);
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string on_off(bool enabled, bool use_korean) {
    return localized(use_korean, enabled ? "On" : "Off", enabled ? "켜짐" : "꺼짐");
}

std::string display_mode_label(std::string value, bool use_korean) {
    value = to_lower_ascii(std::move(value));
    if (value == "windowed") {
        return localized(use_korean, "Windowed", "창 모드");
    }
    if (value == "fullscreen") {
        return localized(use_korean, "Exclusive Fullscreen", "독점 전체 화면");
    }
    return localized(use_korean, "Borderless", "테두리 없음");
}

std::string resolution_label(std::string value, bool use_korean) {
    value = to_lower_ascii(std::move(value));
    if (value == "720p") return "1280x720";
    if (value == "1080p") return "1920x1080";
    if (value == "qhd") return "2560x1440";
    return localized(use_korean, "Monitor Native", "모니터 기본");
}

std::string model_label(
    std::string_view model_path,
    bool use_korean) {
    if (model_path.empty()) {
        return localized(use_korean, "Select...", "선택...");
    }
    try {
        const std::string filename =
            util::path_from_utf8_lossy(model_path).filename().u8string();
        return util::sanitize_ui_text(
            filename.empty() ? std::string(model_path) : filename);
    } catch (...) {
        return util::sanitize_ui_text(model_path);
    }
}

GraphicsSettingsRowModel make_row(
    GraphicsSettingId id,
    SettingsRowKind kind,
    std::string label,
    std::string value,
    const GraphicsSettingsController& controller,
    bool activatable,
    bool adjustable) {
    GraphicsSettingsRowModel row;
    row.id = id;
    row.kind = kind;
    row.label = std::move(label);
    row.value = std::move(value);
    row.selected = controller.selected_id() == id;
    row.activatable = activatable;
    row.adjustable = adjustable;
    return row;
}

OnnxUpscalerConfirmRowModel make_confirmation_row(
    OnnxUpscalerConfirmId id,
    std::string label,
    std::string value,
    const GraphicsSettingsController& controller) {
    OnnxUpscalerConfirmRowModel row;
    row.id = id;
    row.kind = SettingsRowKind::Action;
    row.label = std::move(label);
    row.value = std::move(value);
    row.selected = controller.selected_confirmation_id() == id;
    row.activatable = true;
    return row;
}

}  // namespace

GraphicsSettingsViewModel GraphicsSettingsView::build(
    const GraphicsSettingsController& controller,
    const config::RuntimeConfig& runtime,
    bool use_korean) {
    GraphicsSettingsViewModel view;
    view.rows.reserve(kGraphicsSettingOrder.size());
    view.notes.reserve(11);

    const std::string refresh_hz_label =
        runtime.graphics.refresh_hz == kGraphicsRefreshHzUnlimited
            ? localized(use_korean, "Unlimited (1500 FPS max)", "무제한 (최대 1500 FPS)")
            : localized(use_korean, "Match Display", "디스플레이에 맞춤");
    view.rows.push_back(make_row(
        GraphicsSettingId::Display, SettingsRowKind::Choice,
        localized(use_korean, "Display", "표시 모드"),
        display_mode_label(runtime.graphics.display_mode, use_korean),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::Resolution, SettingsRowKind::Choice,
        localized(use_korean, "Resolution", "해상도"),
        resolution_label(runtime.graphics.resolution, use_korean),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::RefreshHz, SettingsRowKind::Choice,
        localized(use_korean, "Refresh Hz", "주사율"), refresh_hz_label,
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::VSync, SettingsRowKind::Toggle, "VSync",
        on_off(runtime.graphics.vsync, use_korean), controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::PerformanceHud, SettingsRowKind::Toggle,
        localized(use_korean, "Performance HUD", "성능 HUD"),
        on_off(runtime.graphics.performance_overlay, use_korean),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::Bga, SettingsRowKind::Toggle,
        localized(use_korean, "BGA", "BGA 표시"),
        on_off(runtime.graphics.bga_enabled, use_korean), controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::BgaBehindNotes, SettingsRowKind::Toggle,
        localized(use_korean, "BGA Behind Notes", "기어 뒤 BGA"),
        localized(use_korean,
                  runtime.skin.black_playfield_enabled ? "Blocked" : "Visible",
                  runtime.skin.black_playfield_enabled ? "가림" : "표시"),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::BgaUpscaler, SettingsRowKind::Toggle,
        localized(use_korean, "BGA Upscaler", "BGA 업스케일러"),
        on_off(config::normalize_background_upscale_mode(
                   runtime.graphics.background_upscale_mode) == "onnx",
               use_korean),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::OnnxModel, SettingsRowKind::Action,
        localized(use_korean, "ONNX Model", "ONNX 모델"),
        model_label(runtime.graphics.background_upscale_model_path, use_korean),
        controller, true, false));
    view.rows.push_back(make_row(
        GraphicsSettingId::PreferLowPowerDirectX, SettingsRowKind::Toggle,
        localized(use_korean,
                  "Low-Power DirectX (Experimental)",
                  "저전력 DirectX (실험)"),
        on_off(runtime.graphics.background_upscale_prefer_npu, use_korean),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::Language, SettingsRowKind::Choice,
        localized(use_korean, "Language", "언어"),
        config::normalize_ui_language_token(runtime.ui.language) == "ko"
            ? localized(use_korean, "Korean", "한국어")
            : localized(use_korean, "English", "영어"),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::Back, SettingsRowKind::Action,
        localized(use_korean, "Back", "뒤로"), "", controller, true, false));

    const std::string normalized_display = to_lower_ascii(runtime.graphics.display_mode);
    if (normalized_display == "fullscreen") {
        view.notes.push_back(localized(
            use_korean,
            "Discord's current voice overlay does not work in Exclusive Fullscreen. Switch Display to Borderless or Windowed.",
            "현재 Discord 음성 오버레이는 독점 전체 화면에서 동작하지 않습니다. 표시 모드를 테두리 없음 또는 창 모드로 바꾸세요."));
    } else {
        view.notes.push_back(localized(
            use_korean,
            "Discord voice overlay: pin Voice at bottom-left and keep Performance HUD off to avoid covering gameplay information.",
            "Discord 음성 오버레이는 Voice를 좌하단에 고정하고 성능 HUD를 끄면 게임 정보와 가장 덜 겹칩니다."));
    }
    view.notes.push_back(localized(use_korean,
        "Performance HUD shows frame graph, AVG ms/FPS, 0.1%/0.01% lows, and max FPS.",
        "성능 HUD는 프레임 그래프, 평균 ms/FPS, 0.1%/0.01% low, 최대 FPS를 표시합니다."));
    view.notes.push_back(localized(use_korean,
        "BGA OFF suppresses gameplay image/video backgrounds and disables their decoder/upscaler work. Song Select background previews remain visible.",
        "BGA를 끄면 게임플레이 이미지/영상 배경과 디코더/업스케일러 작업이 비활성화됩니다. 선곡 배경 미리보기는 유지됩니다."));
    view.notes.push_back(localized(use_korean,
        "Resolution cycles 720p, 1080p, QHD, or the current monitor native size. Refresh Hz is Match Display or Unlimited (1500 FPS max).",
        "해상도는 720p, 1080p, QHD, 모니터 기본 크기를 순환합니다. 주사율은 디스플레이에 맞춤 또는 무제한(최대 1500 FPS)입니다."));
    view.notes.push_back(localized(use_korean,
        "BGA Behind Notes blocks bright backgrounds only under the playfield while keeping BGA visible outside it.",
        "기어 뒤 BGA를 가리면 바깥 BGA는 유지하면서 노트 영역 아래의 밝은 배경만 차단합니다."));
    view.notes.push_back(localized(use_korean,
        "Menu rendering stays capped at 300 FPS. Unlimited caps gameplay rendering at 1500 FPS when VSync is off.",
        "메뉴 렌더링은 300 FPS로 유지됩니다. 무제한은 VSync가 꺼진 게임플레이 렌더링을 1500 FPS로 제한합니다."));
    view.notes.push_back(localized(use_korean,
        "The ONNX upscaler has no automatic performance benchmark. It is intended for high-spec systems and may cause stutter or heavy accelerator load.",
        "ONNX 업스케일러는 자동 성능 벤치마크 없이 실행됩니다. 고사양 시스템용이며 끊김이나 높은 가속기 부하가 생길 수 있습니다."));
    view.notes.push_back(localized(use_korean,
        "Selecting or dropping an .onnx file changes only the model path. Turn BGA Upscaler ON separately and confirm the warning. Failures keep native scaling.",
        "ONNX 파일 선택·드롭은 모델 경로만 바꿉니다. BGA 업스케일러를 별도로 켜고 경고를 확인하세요. 실패 시 원본 확대를 유지합니다."));
    view.notes.push_back(localized(use_korean,
        "FP32/FP16 model I/O and float-boundary INT8 QDQ metadata are detected automatically. Low-Power DirectX requests DirectXMinPower; it does not select or verify an NPU.",
        "FP32/FP16 모델 입출력과 float 경계 INT8 QDQ 메타데이터를 자동 감지합니다. 저전력 DirectX는 DirectXMinPower 요청이며 NPU를 명시 선택하거나 검증하지 않습니다."));
    view.notes.push_back(localized(use_korean,
        "Language changes the menu UI immediately. Visual Latency is available in Skin Settings.",
        "언어는 메뉴 UI에 즉시 반영됩니다. 비주얼 레이턴시는 스킨 설정에서 조정합니다."));
    view.notes.push_back(localized(use_korean,
        "Display, Resolution, Refresh Hz, and VSync apply immediately. Back saves and returns.",
        "표시 모드, 해상도, 주사율, VSync는 즉시 적용됩니다. 뒤로 가면 저장 후 돌아갑니다."));
    return view;
}

OnnxUpscalerConfirmViewModel GraphicsSettingsView::build_onnx_confirmation(
    const GraphicsSettingsController& controller,
    bool use_korean) {
    OnnxUpscalerConfirmViewModel view;
    view.rows.reserve(kOnnxUpscalerConfirmOrder.size());
    view.notes.reserve(3);
    view.rows.push_back(make_confirmation_row(
        OnnxUpscalerConfirmId::Enable,
        localized(use_korean, "Yes, enable ONNX", "예, ONNX 켜기"),
        localized(use_korean, "High-spec mode", "고사양 모드"),
        controller));
    view.rows.push_back(make_confirmation_row(
        OnnxUpscalerConfirmId::KeepNative,
        localized(use_korean, "No, keep native", "아니오, 원본 유지"),
        localized(use_korean, "Recommended default", "권장 기본값"),
        controller));
    view.notes.push_back(localized(use_korean,
        "This feature runs the selected ONNX model without a benchmark cutoff.",
        "이 기능은 선택한 ONNX 모델을 벤치마크 차단 없이 실행합니다."));
    view.notes.push_back(localized(use_korean,
        "A high-spec GPU/accelerator is recommended. Slow models can reduce menu or gameplay smoothness.",
        "고사양 GPU/가속기를 권장합니다. 느린 모델은 메뉴나 플레이 화면을 끊기게 할 수 있습니다."));
    view.notes.push_back(localized(use_korean,
        "Yes applies now. No, Esc, or Backspace leaves the upscaler OFF.",
        "예를 누르면 바로 적용합니다. 아니오, Esc, Backspace는 업스케일러를 끈 상태로 둡니다."));
    return view;
}

}  // namespace tenriff::app::menu::settings
