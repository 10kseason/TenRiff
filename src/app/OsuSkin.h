#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tenriff::app {

struct OsuManiaSkinDefinition {
    bool found = false;
    int keys = 0;
    std::vector<std::string> note_images;
    std::vector<std::string> hold_head_images;
    std::vector<std::string> hold_body_images;
    std::vector<std::string> hold_tail_images;
    std::vector<std::string> key_images;
    std::vector<std::string> key_pressed_images;
    std::vector<float> lane_divider_widths;
};

[[nodiscard]] std::string find_default_osu_skin_test_root();
[[nodiscard]] bool is_osu_skin_directory(std::string_view path_utf8);
[[nodiscard]] std::vector<std::string> list_osu_skin_names(std::string_view root_utf8);
[[nodiscard]] OsuManiaSkinDefinition resolve_osu_mania_skin(std::string_view root_utf8,
                                                            std::string_view skin_name,
                                                            int keys);

}  // namespace tenriff::app
