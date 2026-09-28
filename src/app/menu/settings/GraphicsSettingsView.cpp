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

std::string localized(ui::Language language, std::string_view english, std::string_view korean) {
    return ui::text(language, english, korean);
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string on_off(bool enabled, ui::Language language) {
    return localized(language, enabled ? "On" : "Off", enabled ? "켜짐" : "꺼짐");
}

std::string display_mode_label(std::string value, ui::Language language) {
    value = to_lower_ascii(std::move(value));
    if (value == "windowed") {
        return localized(language, "Windowed", "창 모드");
    }
    if (value == "fullscreen") {
        return localized(language, "Exclusive Fullscreen", "독점 전체 화면");
    }
    return localized(language, "Borderless", "테두리 없음");
}

std::string resolution_label(std::string value, ui::Language language) {
    value = to_lower_ascii(std::move(value));
    if (value == "720p") return "1280x720";
    if (value == "1080p") return "1920x1080";
    if (value == "qhd") return "2560x1440";
    return localized(language, "Monitor Native", "모니터 기본");
}

std::string model_label(
    std::string_view model_path,
    ui::Language language) {
    if (model_path.empty()) {
        return localized(language, "Select...", "선택...");
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
    ui::Language language) {
    GraphicsSettingsViewModel view;
    view.rows.reserve(kGraphicsSettingOrder.size());
    view.notes.reserve(11);

    const std::string refresh_hz_label =
        runtime.graphics.refresh_hz == kGraphicsRefreshHzUnlimited
            ? localized(language, "Unlimited (1500 FPS max)", "무제한 (최대 1500 FPS)")
            : localized(language, "Match Display", "디스플레이에 맞춤");
    view.rows.push_back(make_row(
        GraphicsSettingId::Display, SettingsRowKind::Choice,
        localized(language, "Display", "표시 모드"),
        display_mode_label(runtime.graphics.display_mode, language),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::Resolution, SettingsRowKind::Choice,
        localized(language, "Resolution", "해상도"),
        resolution_label(runtime.graphics.resolution, language),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::RefreshHz, SettingsRowKind::Choice,
        localized(language, "Refresh Hz", "주사율"), refresh_hz_label,
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::VSync, SettingsRowKind::Toggle, "VSync",
        on_off(runtime.graphics.vsync, language), controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::PerformanceHud, SettingsRowKind::Toggle,
        localized(language, "Performance HUD", "성능 HUD"),
        on_off(runtime.graphics.performance_overlay, language),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::Bga, SettingsRowKind::Toggle,
        localized(language, "BGA", "BGA 표시"),
        on_off(runtime.graphics.bga_enabled, language), controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::BgaBehindNotes, SettingsRowKind::Toggle,
        localized(language, "BGA Behind Notes", "기어 뒤 BGA"),
        localized(language,
                  runtime.skin.black_playfield_enabled ? "Blocked" : "Visible",
                  runtime.skin.black_playfield_enabled ? "가림" : "표시"),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::BgaUpscaler, SettingsRowKind::Toggle,
        localized(language, "BGA Upscaler", "BGA 업스케일러"),
        on_off(config::normalize_background_upscale_mode(
                   runtime.graphics.background_upscale_mode) == "onnx",
               language),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::OnnxModel, SettingsRowKind::Action,
        localized(language, "ONNX Model", "ONNX 모델"),
        model_label(runtime.graphics.background_upscale_model_path, language),
        controller, true, false));
    view.rows.push_back(make_row(
        GraphicsSettingId::PreferLowPowerDirectX, SettingsRowKind::Toggle,
        localized(language,
                  "Low-Power DirectX (Experimental)",
                  "저전력 DirectX (실험)"),
        on_off(runtime.graphics.background_upscale_prefer_npu, language),
        controller, false, true));
    view.rows.push_back(make_row(
        GraphicsSettingId::Back, SettingsRowKind::Action,
        localized(language, "Back", "뒤로"), "", controller, true, false));

    const std::string normalized_display = to_lower_ascii(runtime.graphics.display_mode);
    if (normalized_display == "fullscreen") {
        view.notes.push_back(localized(
            language,
            "Discord's current voice overlay does not work in Exclusive Fullscreen. Switch Display to Borderless or Windowed.",
            "현재 Discord 음성 오버레이는 독점 전체 화면에서 동작하지 않습니다. 표시 모드를 테두리 없음 또는 창 모드로 바꾸세요."));
    } else {
        view.notes.push_back(localized(
            language,
            "Discord voice overlay: pin Voice at bottom-left and keep Performance HUD off to avoid covering gameplay information.",
            "Discord 음성 오버레이는 Voice를 좌하단에 고정하고 성능 HUD를 끄면 게임 정보와 가장 덜 겹칩니다."));
    }
    view.notes.push_back(localized(language,
        "Performance HUD shows frame graph, AVG ms/FPS, 0.1%/0.01% lows, and max FPS.",
        "성능 HUD는 프레임 그래프, 평균 ms/FPS, 0.1%/0.01% low, 최대 FPS를 표시합니다."));
    view.notes.push_back(localized(language,
        "BGA OFF suppresses gameplay image/video backgrounds and disables their decoder/upscaler work. Song Select background previews remain visible.",
        "BGA를 끄면 게임플레이 이미지/영상 배경과 디코더/업스케일러 작업이 비활성화됩니다. 선곡 배경 미리보기는 유지됩니다."));
    view.notes.push_back(localized(language,
        "Resolution cycles 720p, 1080p, QHD, or the current monitor native size. Refresh Hz is Match Display or Unlimited (1500 FPS max).",
        "해상도는 720p, 1080p, QHD, 모니터 기본 크기를 순환합니다. 주사율은 디스플레이에 맞춤 또는 무제한(최대 1500 FPS)입니다."));
    view.notes.push_back(localized(language,
        "BGA Behind Notes blocks bright backgrounds only under the playfield while keeping BGA visible outside it.",
        "기어 뒤 BGA를 가리면 바깥 BGA는 유지하면서 노트 영역 아래의 밝은 배경만 차단합니다."));
    view.notes.push_back(localized(language,
        "Menu rendering stays capped at 300 FPS. Unlimited caps gameplay rendering at 1500 FPS when VSync is off.",
        "메뉴 렌더링은 300 FPS로 유지됩니다. 무제한은 VSync가 꺼진 게임플레이 렌더링을 1500 FPS로 제한합니다."));
    view.notes.push_back(localized(language,
        "The ONNX upscaler has no automatic performance benchmark. It is intended for high-spec systems and may cause stutter or heavy accelerator load.",
        "ONNX 업스케일러는 자동 성능 벤치마크 없이 실행됩니다. 고사양 시스템용이며 끊김이나 높은 가속기 부하가 생길 수 있습니다."));
    view.notes.push_back(localized(language,
        "Selecting or dropping an .onnx file changes only the model path. Turn BGA Upscaler ON separately and confirm the warning. Failures keep native scaling.",
        "ONNX 파일 선택·드롭은 모델 경로만 바꿉니다. BGA 업스케일러를 별도로 켜고 경고를 확인하세요. 실패 시 원본 확대를 유지합니다."));
    view.notes.push_back(localized(language,
        "FP32/FP16 model I/O and float-boundary INT8 QDQ metadata are detected automatically. Low-Power DirectX requests DirectXMinPower; it does not select or verify an NPU.",
        "FP32/FP16 모델 입출력과 float 경계 INT8 QDQ 메타데이터를 자동 감지합니다. 저전력 DirectX는 DirectXMinPower 요청이며 NPU를 명시 선택하거나 검증하지 않습니다."));
    view.notes.push_back(localized(language,
        "Language and menu font size are in Profile Setup. Visual Latency is in Skin Settings.",
        "언어와 메뉴 글자 크기는 프로필 설정에, 비주얼 레이턴시는 스킨 설정에 있습니다."));
    view.notes.push_back(localized(language,
        "Display, Resolution, Refresh Hz, and VSync apply immediately. Back saves and returns.",
        "표시 모드, 해상도, 주사율, VSync는 즉시 적용됩니다. 뒤로 가면 저장 후 돌아갑니다."));
    return view;
}

OnnxUpscalerConfirmViewModel GraphicsSettingsView::build_onnx_confirmation(
    const GraphicsSettingsController& controller,
    ui::Language language) {
    OnnxUpscalerConfirmViewModel view;
    view.rows.reserve(kOnnxUpscalerConfirmOrder.size());
    view.notes.reserve(3);
    view.rows.push_back(make_confirmation_row(
        OnnxUpscalerConfirmId::Enable,
        localized(language, "Yes, enable ONNX", "예, ONNX 켜기"),
        localized(language, "High-spec mode", "고사양 모드"),
        controller));
    view.rows.push_back(make_confirmation_row(
        OnnxUpscalerConfirmId::KeepNative,
        localized(language, "No, keep native", "아니오, 원본 유지"),
        localized(language, "Recommended default", "권장 기본값"),
        controller));
    view.notes.push_back(localized(language,
        "This feature runs the selected ONNX model without a benchmark cutoff.",
        "이 기능은 선택한 ONNX 모델을 벤치마크 차단 없이 실행합니다."));
    view.notes.push_back(localized(language,
        "A high-spec GPU/accelerator is recommended. Slow models can reduce menu or gameplay smoothness.",
        "고사양 GPU/가속기를 권장합니다. 느린 모델은 메뉴나 플레이 화면을 끊기게 할 수 있습니다."));
    view.notes.push_back(localized(language,
        "Yes applies now. No, Esc, or Backspace leaves the upscaler OFF.",
        "예를 누르면 바로 적용합니다. 아니오, Esc, Backspace는 업스케일러를 끈 상태로 둡니다."));
    return view;
}

}  // namespace tenriff::app::menu::settings
