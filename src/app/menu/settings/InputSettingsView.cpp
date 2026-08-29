#include "app/menu/settings/InputSettingsView.h"

#include <cmath>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace tenriff::app::menu::settings {
namespace {

std::string localized(bool use_korean, std::string_view english, std::string_view korean) {
    return std::string(use_korean ? korean : english);
}

std::string format_decimal(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(std::abs(value - std::round(value)) < 0.0001 ? 0 : 1);
    stream << value;
    return stream.str();
}

InputSettingsRowModel make_row(
    InputSettingId id,
    SettingsRowKind kind,
    std::string label,
    std::string value,
    const InputSettingsController& controller,
    bool activatable,
    bool adjustable) {
    InputSettingsRowModel row;
    row.id = id;
    row.kind = kind;
    row.label = std::move(label);
    row.value = std::move(value);
    row.selected = controller.selected_id() == id;
    row.activatable = activatable;
    row.adjustable = adjustable;
    return row;
}

}  // namespace

InputSettingsViewModel InputSettingsView::build(
    const InputSettingsController& controller,
    const config::RuntimeConfig& runtime,
    bool is_polling_fallback_latched,
    bool use_korean) {
    InputSettingsViewModel view;
    view.rows.reserve(kInputSettingOrder.size());
    view.notes.reserve(6);

    std::string backend = runtime.input.rawinput ? "RawInput" : "Polling";
    if (runtime.input.rawinput && is_polling_fallback_latched) {
        backend += " (active: Polling)";
    }
    view.rows.push_back(make_row(
        InputSettingId::Backend,
        SettingsRowKind::Choice,
        localized(use_korean, "Backend", "입력 백엔드"),
        std::move(backend),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        InputSettingId::PollingHz,
        SettingsRowKind::Choice,
        localized(use_korean, "Polling Hz", "폴링 Hz"),
        std::to_string(runtime.input.polling_hz),
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        InputSettingId::Debounce,
        SettingsRowKind::Choice,
        localized(use_korean, "Debounce", "디바운스"),
        format_decimal(runtime.input.debounce_ms) + " ms",
        controller,
        false,
        true));
    view.rows.push_back(make_row(
        InputSettingId::Back,
        SettingsRowKind::Action,
        localized(use_korean, "Back", "뒤로"),
        "",
        controller,
        true,
        false));

    view.notes.push_back(localized(
        use_korean,
        "Backend selects RawInput or Polling for the saved profile.",
        "입력 백엔드는 프로필에 저장할 RawInput 또는 Polling을 선택합니다."));
    view.notes.push_back(localized(
        use_korean,
        "After a confirmed RawInput failure, this app run stays on Polling so menu and note input keep working.",
        "RawInput 고장이 확인되면 이번 실행 동안 Polling을 유지해 메뉴와 노트 입력이 계속 작동합니다."));
    view.notes.push_back(localized(
        use_korean,
        "Left selects Polling. Right selects or retries RawInput and clears the runtime fallback.",
        "왼쪽은 Polling을 선택하고, 오른쪽은 런타임 대체를 해제해 RawInput을 선택하거나 다시 시도합니다."));
    view.notes.push_back(localized(
        use_korean,
        "During gameplay, RawInput also uses Polling Hz as an always-on bound-key backup.",
        "플레이 중 RawInput은 폴링 Hz 주기로 노트 키를 항상 보조 감시합니다."));
    view.notes.push_back(localized(
        use_korean,
        "Debounce filters duplicate switch chatter on a single key before gameplay sees it.",
        "디바운스는 플레이에 전달되기 전 한 키의 중복 스위치 채터링을 걸러냅니다."));
    view.notes.push_back(localized(
        use_korean,
        "Left/Right or click +/- to change. Back saves and returns.",
        "좌우 키 또는 +/- 클릭으로 변경합니다. 뒤로 가면 저장됩니다."));
    return view;
}

}  // namespace tenriff::app::menu::settings
