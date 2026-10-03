#include "app/menu/settings/KeymapSettingsView.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "config/KeycodeMap.h"

namespace tenriff::app::menu::settings {
namespace {

std::string localized(ui::Language language, std::string_view english, std::string_view korean) {
    return ui::text(language, english, korean);
}

std::string key_mode_label(std::string_view mode) {
    std::string label(mode);
    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('a') &&
            ch <= static_cast<unsigned char>('z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('a') +
                                     static_cast<unsigned char>('A'));
        }
        return static_cast<char>(ch);
    });
    return label;
}

std::string binding_name(
    const std::unordered_map<std::string, std::string>& bindings,
    std::string_view lane,
    ui::Language language) {
    const auto binding = bindings.find(std::string(lane));
    if (binding == bindings.end() || binding->second.empty()) {
        return localized(language, "Unassigned", "미할당");
    }
    return binding->second;
}

}  // namespace

KeymapSettingsViewModel KeymapSettingsView::build(
    const KeymapSettingsController& controller,
    const config::Keymap& working_keymap,
    std::optional<int> selected_chart_key_count,
    std::string_view runtime_key_mode,
    std::string backend_status,
    std::int64_t now_ns,
    ui::Language language) {
    config::KeymapManager manager;
    const auto bindings = manager.bindings_for_mode(
        working_keymap, std::string(controller.edit_mode()));
    const auto secondary = manager.secondary_bindings_for_mode(working_keymap, controller.edit_mode());
    KeymapSettingsViewModel view;
    view.footer_reserved_lines = 6;
    view.rows.reserve(controller.lane_ids().size() + 4);

    if (controller.status_visible(now_ns)) {
        view.footer_notes.emplace_back(controller.status_message());
    }
    view.footer_notes.push_back(std::move(backend_status));
    if (selected_chart_key_count.has_value()) {
        const std::string selected_mode = resolve_keymap_edit_mode_for_menu(
            selected_chart_key_count, runtime_key_mode);
        if (selected_mode != controller.edit_mode()) {
            view.footer_notes.push_back(
                localized(language, "Selected chart uses ", "선택한 차트 키 모드: ") +
                key_mode_label(selected_mode) +
                localized(language, " / editing ", " / 현재 편집: ") +
                key_mode_label(controller.edit_mode()));
        } else {
            view.footer_notes.push_back(
                localized(language,
                          "Selected chart key mode: ",
                          "선택한 차트 키 모드: ") +
                key_mode_label(selected_mode));
        }
    }

    view.rows.push_back(KeymapViewRow{
        localized(language, "Key Mode", "키 모드"),
        key_mode_label(controller.edit_mode()),
        controller.selected_row() == 0,
        std::nullopt});
    for (std::size_t index = 0; index < controller.lane_ids().size(); ++index) {
        const std::string& lane = controller.lane_ids()[index];
        std::string value = binding_name(bindings, lane, language);
        if (controller.capture_active() && !controller.secondary_selected() &&
            static_cast<int>(index) + 1 == controller.selected_row()) {
            value += localized(language, " [waiting]", " [대기 중]");
        }
        std::string secondary_value = binding_name(secondary, lane, language);
        if (controller.capture_active() && controller.secondary_selected() &&
            static_cast<int>(index) + 1 == controller.selected_row())
            secondary_value += localized(language, " [waiting]", " [대기 중]");
        view.rows.push_back(KeymapViewRow{
            "Key " + std::to_string(index + 1),
            std::move(value),
            static_cast<int>(index) + 1 == controller.selected_row(),
            std::nullopt,
            std::move(secondary_value)});
    }

    if (controller.capture_active()) {
        const std::int64_t remaining_ns = std::max<std::int64_t>(
            0, controller.capture_deadline_ns() - now_ns);
        view.footer_notes.push_back(
            localized(language, "Capture timeout: ", "입력 대기 시간: ") +
            std::to_string(remaining_ns / 1'000'000) + "ms");
        view.footer_notes.push_back(localized(
            language,
            "Press a key. Delete clears a secondary binding or cancels primary capture.",
            "키를 누르세요. Delete는 보조 키를 해제하거나 기본 키 입력 대기를 취소합니다."));
        view.footer_notes.push_back(localized(
            language,
            "Duplicate lane bindings are allowed.",
            "같은 키를 여러 레인에 중복으로 배치할 수 있습니다."));
    }

    view.rows.push_back(KeymapViewRow{
        localized(language, "Reset", "초기화"), "", controller.selected_row() == static_cast<int>(controller.lane_ids().size()) + 1, KeymapActionId::Reset});
    view.rows.push_back(KeymapViewRow{
        localized(language, "NKRO Test", "NKRO Test"), "", controller.selected_row() == static_cast<int>(controller.lane_ids().size()) + 2, KeymapActionId::NkroTest});
    view.rows.push_back(KeymapViewRow{
        controller.capture_active() ? localized(language, "Cancel", "취소") : localized(language, "Back", "뒤로"),
        "", controller.selected_row() == static_cast<int>(controller.lane_ids().size()) + 3, KeymapActionId::Back});
    view.footer_notes.push_back(localized(
        language,
        "Left/Right on Key Mode selects a 4K-10K, 12K, 14K, or 16K layout.",
        "키 모드에서 좌우 키를 누르면 4K~10K, 12K, 14K 또는 16K 레이아웃을 고릅니다."));
    view.footer_notes.push_back(localized(
        language,
        "Click a primary or secondary key to bind. Left/Right selects its slot; Enter saves immediately.",
        "기본 키나 보조 키를 클릭해 할당하세요. 좌우로 항목을 고르고 Enter로 입력하면 즉시 저장됩니다."));
    view.footer_notes.push_back(localized(language,
        "BMS original mode uses the chart's actual key count. Secondary keys work in every layout, including 5K.",
        "BMS 원본 모드는 차트의 실제 키 수에 맞춥니다. 5K를 포함한 모든 모드에서 보조 키를 지정할 수 있습니다."));
    return view;
}

KeymapSettingsViewModel KeymapSettingsView::build_nkro_test(
    const KeymapSettingsController& controller,
    const config::Keymap& working_keymap,
    const std::unordered_set<std::uint32_t>& pressed_keys,
    std::string backend_status,
    ui::Language language) {
    config::KeymapManager manager;
    const auto bindings = manager.bindings_for_mode(
        working_keymap, std::string(controller.edit_mode()));
    const auto secondary = manager.secondary_bindings_for_mode(working_keymap, controller.edit_mode());
    KeymapSettingsViewModel view;
    view.footer_reserved_lines = 2;
    view.rows.reserve(controller.lane_ids().size() + 1);
    view.footer_notes.push_back(localized(
        language,
        "Left: fewer keys / Right: more keys (4K-16K). Hold multiple mapped keys to test simultaneous input. Saved bindings stay unchanged.",
        "← 키 수 줄이기 / → 키 수 늘리기 (4K~16K). 표시된 키를 함께 눌러 동시 입력을 확인하세요. 저장된 키 배치는 유지됩니다."));
    view.footer_notes.push_back(std::move(backend_status));

    std::size_t index = 0;
    for (const std::string& lane : controller.lane_ids()) {
        const std::string key_name = binding_name(bindings, lane, language);
        bool is_down = false;
        if (const auto keycode = config::KeycodeMap::to_keycode(key_name);
            keycode.has_value()) {
            is_down = pressed_keys.find(*keycode) != pressed_keys.end();
        }
        const auto secondary_name = binding_name(secondary, lane, language);
        if (const auto keycode = config::KeycodeMap::to_keycode(secondary_name))
            is_down = is_down || pressed_keys.find(*keycode) != pressed_keys.end();
        view.rows.push_back(KeymapViewRow{
            "Key " + std::to_string(++index),
            key_name + (is_down
                ? localized(language, " [DOWN]", " [눌림]")
                : ""),
            is_down,
            std::nullopt,
            secondary_name});
    }
    view.rows.push_back(KeymapViewRow{
        localized(language, "Back", "뒤로"),
        "",
        true,
        KeymapActionId::Back});
    return view;
}

}  // namespace tenriff::app::menu::settings
