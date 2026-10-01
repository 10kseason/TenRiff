#include "app/menu/settings/CalibrationSettingsView.h"

#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace tenriff::app::menu::settings {
namespace {

std::string localized(ui::Language language, std::string_view english, std::string_view korean) {
    return ui::text(language, english, korean);
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
    ui::Language language) {
    CalibrationSettingsViewModel view;
    view.rows.reserve(kCalibrationSettingOrder.size());
    view.notes.reserve(5);

    view.rows.push_back(make_row(
        CalibrationSettingId::AdjustmentStep,
        SettingsRowKind::Choice,
        localized(language, "Adjustment Step", "조정 단위"),
        std::to_string(controller.adjustment_step_ms()) + " ms",
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        CalibrationSettingId::InputOffset,
        SettingsRowKind::Numeric,
        localized(language, "Input Offset", "입력 오프셋"),
        format_signed_offset_ms(runtime.input_offset_ms),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        CalibrationSettingId::VisualOffset,
        SettingsRowKind::Numeric,
        localized(language, "Visual Latency", "비주얼 레이턴시"),
        format_signed_offset_ms(runtime.visual_offset_ms),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        CalibrationSettingId::SoundOffset,
        SettingsRowKind::Numeric,
        localized(language, "Sound Offset", "사운드 오프셋"),
        format_signed_offset_ms(runtime.sound_offset_ms),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        CalibrationSettingId::ResetOffsets,
        SettingsRowKind::Action,
        localized(language, "Reset Offsets", "오프셋 초기화"),
        "",
        controller,
        true,
        false));
    view.rows.push_back(make_row(
        CalibrationSettingId::Back,
        SettingsRowKind::Action,
        localized(language, "Back", "뒤로"),
        "",
        controller,
        true,
        false));

    for (auto& row : view.rows) {
        if (row.id == CalibrationSettingId::AdjustmentStep)
            row.category = localized(language, "Adjustment", "조정");
        else if (row.id == CalibrationSettingId::ResetOffsets || row.id == CalibrationSettingId::Back)
            row.category = localized(language, "Actions", "작업");
        else
            row.category = localized(language, "Timing", "타이밍");
    }

    view.notes.push_back(localized(
        language,
        "Step 1: use Input Offset to match your actual key hit timing to the judgement windows.",
        "1단계: 입력 오프셋으로 실제 타건 타이밍을 판정창에 맞추세요."));
    view.notes.push_back(localized(
        language,
        "Step 2: use Visual Latency only for what you see. Positive values draw notes earlier.",
        "2단계: 비주얼 레이턴시는 화면만 조정합니다. 양수일수록 노트가 더 일찍 보입니다."));
    view.notes.push_back(localized(
        language,
        "Step 3: use Sound Offset for chart BGM/autoplay audio. Positive values delay sound.",
        "3단계: 사운드 오프셋으로 차트 BGM/자동재생 오디오를 조정합니다. 양수일수록 소리가 늦어집니다."));
    view.notes.push_back(localized(
        language,
        "Use a familiar chart, retry quickly from Result, and compare fast/slow feedback until both feel centered.",
        "익숙한 차트를 고른 뒤 결과 화면에서 빠르게 재시작하면서 빠름/느림 피드백이 중앙에 모일 때까지 조정하세요."));
    view.notes.push_back(localized(
        language,
        "Changes save immediately so the next launch uses the same calibration.",
        "변경은 즉시 저장되므로 다음 플레이에도 같은 보정값이 적용됩니다."));
    return view;
}

}  // namespace tenriff::app::menu::settings
