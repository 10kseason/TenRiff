#pragma once

#include <array>
#include <string>

#include "ui/Localization.h"

namespace tenriff::app {

// Shared by real option screens and the standalone native preview fixture.
inline std::array<std::string, 2> settings_help_tips(ui::Language language) {
    return {
        ui::text(language,
            "ALL SONG combines registered folders. With a difficulty table selected, it shows only table charts; Native LV restores all charts.",
            "ALL SONG은 등록 폴더를 합칩니다. 난이도표 선택 중에는 표에 해당하는 차트만 표시하고, Native LV로 전체를 다시 표시합니다."),
        ui::text(language,
            "Skin Settings edits the selected key mode with actual gameplay proportions. Key backdrop and hit-burst brightness are separate controls.",
            "스킨 설정은 선택한 키 모드를 실제 인게임 비율로 보여줍니다. 키 배경색과 타격 효과 밝기는 각각 조절합니다.")
    };
}

}  // namespace tenriff::app
