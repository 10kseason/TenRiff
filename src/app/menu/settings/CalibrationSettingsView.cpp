#include "app/menu/settings/CalibrationSettingsView.h"

#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace tenriff::app::menu::settings {
namespace {

std::string localized(bool use_korean, std::string_view english, std::string_view korean) {
    return std::string(use_korean ? korean : english);
}

std::string format_signed_offset_ms(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(1);
    if (value >= 0.0) {
        stream << '+';
    }
    stream << value << " ms";
    return stream.str();
}

CalibrationSettingsRowModel make_row(
    CalibrationSettingId id,
    SettingsRowKind kind,
    std::string label,
    std::string value,
    const CalibrationSettingsController& controller,
    bool activatable,
    bool adjustable) {
    CalibrationSettingsRowModel row;
    row.id = id;
    row.kind = kind;
    row.label = std::move(label);
    row.value = std::move(value);
    row.selected = controller.selected_id() == id;
    row.activatable = activatable;
    row.adjustable = adjustable;
    row.numeric_range = calibration_setting_numeric_range(id);
    return row;
}

}  // namespace

CalibrationSettingsViewModel CalibrationSettingsView::build(
    const CalibrationSettingsController& controller,
    const config::RuntimeConfig& runtime,
    bool use_korean) {
    CalibrationSettingsViewModel view;
    view.rows.reserve(kCalibrationSettingOrder.size());
    view.notes.reserve(5);

    view.rows.push_back(make_row(
        CalibrationSettingId::AdjustmentStep,
        SettingsRowKind::Choice,
        localized(use_korean, "Adjustment Step", "조정 단위"),
        std::to_string(controller.adjustment_step_ms()) + " ms",
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        CalibrationSettingId::InputOffset,
        SettingsRowKind::Numeric,
        localized(use_korean, "Input Offset", "입력 오프셋"),
        format_signed_offset_ms(runtime.input_offset_ms),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        CalibrationSettingId::VisualOffset,
        SettingsRowKind::Numeric,
        localized(use_korean, "Visual Latency", "비주얼 레이턴시"),
        format_signed_offset_ms(runtime.visual_offset_ms),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        CalibrationSettingId::SoundOffset,
        SettingsRowKind::Numeric,
        localized(use_korean, "Sound Offset", "사운드 오프셋"),
        format_signed_offset_ms(runtime.sound_offset_ms),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        CalibrationSettingId::ResetOffsets,
        SettingsRowKind::Action,
        localized(use_korean, "Reset Offsets", "오프셋 초기화"),
        "",
        controller,
        true,
        false));
    view.rows.push_back(make_row(
        CalibrationSettingId::Back,
        SettingsRowKind::Action,
        localized(use_korean, "Back", "뒤로"),
        "",
        controller,
        true,
        false));

    view.notes.push_back(localized(
        use_korean,
        "Step 1: use Input Offset to match your actual key hit timing to the judgement windows.",
        "1단계: 입력 오프셋으로 실제 타건 타이밍을 판정창에 맞추세요."));
    view.notes.push_back(localized(
        use_korean,
        "Step 2: use Visual Latency only for what you see. Positive values draw notes earlier.",
        "2단계: 비주얼 레이턴시는 화면만 조정합니다. 양수일수록 노트가 더 일찍 보입니다."));
    view.notes.push_back(localized(
        use_korean,
        "Step 3: use Sound Offset for chart BGM/autoplay audio. Positive values delay sound.",
        "3단계: 사운드 오프셋으로 차트 BGM/자동재생 오디오를 조정합니다. 양수일수록 소리가 늦어집니다."));
    view.notes.push_back(localized(
        use_korean,
        "Use a familiar chart, retry quickly from Result, and compare fast/slow feedback until both feel centered.",
        "익숙한 차트를 고른 뒤 결과 화면에서 빠르게 재시작하면서 빠름/느림 피드백이 중앙에 모일 때까지 조정하세요."));
    view.notes.push_back(localized(
        use_korean,
        "Changes save immediately so the next launch uses the same calibration.",
        "변경은 즉시 저장되므로 다음 플레이에도 같은 보정값이 적용됩니다."));
    return view;
}

}  // namespace tenriff::app::menu::settings
