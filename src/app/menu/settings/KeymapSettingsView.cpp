#include "app/menu/settings/KeymapSettingsView.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "config/KeycodeMap.h"

namespace tenriff::app::menu::settings {
namespace {

std::string localized(bool use_korean, std::string_view english, std::string_view korean) {
    return std::string(use_korean ? korean : english);
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
    bool use_korean) {
    const auto binding = bindings.find(std::string(lane));
    if (binding == bindings.end() || binding->second.empty()) {
        return localized(use_korean, "Unassigned", "미할당");
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
    bool use_korean) {
    config::KeymapManager manager;
    const auto bindings = manager.bindings_for_mode(
        working_keymap, std::string(controller.edit_mode()));
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
                localized(use_korean, "Selected chart uses ", "선택한 차트 키 모드: ") +
                key_mode_label(selected_mode) +
                localized(use_korean, " / editing ", " / 현재 편집: ") +
                key_mode_label(controller.edit_mode()));
        } else {
            view.footer_notes.push_back(
                localized(use_korean,
                          "Selected chart key mode: ",
                          "선택한 차트 키 모드: ") +
                key_mode_label(selected_mode));
        }
    }

    view.rows.push_back(KeymapViewRow{
        localized(use_korean, "Key Mode", "키 모드"),
        key_mode_label(controller.edit_mode()),
        controller.selected_row() == 0,
        std::nullopt});
    for (std::size_t index = 0; index < controller.lane_ids().size(); ++index) {
        const std::string& lane = controller.lane_ids()[index];
        std::string value = binding_name(bindings, lane, use_korean);
        if (controller.capture_active() &&
            static_cast<int>(index) + 1 == controller.selected_row()) {
            value += localized(use_korean, " [waiting]", " [대기 중]");
        }
        view.rows.push_back(KeymapViewRow{
            lane,
            std::move(value),
            static_cast<int>(index) + 1 == controller.selected_row(),
            std::nullopt});
    }

    if (controller.capture_active()) {
        const std::int64_t remaining_ns = std::max<std::int64_t>(
            0, controller.capture_deadline_ns() - now_ns);
        view.footer_notes.push_back(
            localized(use_korean, "Capture timeout: ", "입력 대기 시간: ") +
            std::to_string(remaining_ns / 1'000'000) + "ms");
        view.footer_notes.push_back(localized(
            use_korean,
            "Press any keyboard key. Delete cancels capture.",
            "아무 키나 누르세요. Delete 키로 입력 대기를 취소합니다."));
        view.footer_notes.push_back(localized(
            use_korean,
            "Duplicate lane bindings are allowed.",
            "같은 키를 여러 레인에 중복으로 배치할 수 있습니다."));
    }

    view.rows.push_back(KeymapViewRow{
        localized(use_korean, "Reset", "초기화"), "", false, KeymapActionId::Reset});
    view.rows.push_back(KeymapViewRow{
        "NKRO Test", "", false, KeymapActionId::NkroTest});
    view.rows.push_back(KeymapViewRow{
        localized(use_korean, "Back", "뒤로"), "", false, KeymapActionId::Back});
    view.footer_notes.push_back(localized(
        use_korean,
        "Left/Right on Key Mode selects a 4K-10K, 12K, 14K, or 16K layout.",
        "키 모드에서 좌우 키를 누르면 4K~10K, 12K, 14K 또는 16K 레이아웃을 고릅니다."));
    view.footer_notes.push_back(localized(
        use_korean,
        "Enter binds the selected lane and saves immediately. Reset also saves immediately.",
        "Enter로 선택 레인에 키를 할당하면 즉시 저장됩니다. 초기화도 바로 저장됩니다."));
    return view;
}

KeymapSettingsViewModel KeymapSettingsView::build_nkro_test(
    const KeymapSettingsController& controller,
    const config::Keymap& working_keymap,
    const std::unordered_set<std::uint32_t>& pressed_keys,
    std::string backend_status,
    bool use_korean) {
    config::KeymapManager manager;
    const auto bindings = manager.bindings_for_mode(
        working_keymap, std::string(controller.edit_mode()));
    KeymapSettingsViewModel view;
    view.footer_reserved_lines = 2;
    view.rows.reserve(controller.lane_ids().size() + 1);
    view.footer_notes.push_back(localized(
        use_korean,
        "NKRO Test (press multiple keys)",
        "NKRO 테스트 (여러 키를 동시에 눌러보세요)"));
    view.footer_notes.push_back(std::move(backend_status));

    for (const std::string& lane : controller.lane_ids()) {
        const std::string key_name = binding_name(bindings, lane, use_korean);
        bool is_down = false;
        if (const auto keycode = config::KeycodeMap::to_keycode(key_name);
            keycode.has_value()) {
            is_down = pressed_keys.find(*keycode) != pressed_keys.end();
        }
        view.rows.push_back(KeymapViewRow{
            lane,
            key_name + (is_down
                ? localized(use_korean, " [DOWN]", " [눌림]")
                : ""),
            is_down,
            std::nullopt});
    }
    view.rows.push_back(KeymapViewRow{
        localized(use_korean, "Back", "뒤로"),
        "",
        true,
        KeymapActionId::Back});
    return view;
}

}  // namespace tenriff::app::menu::settings
