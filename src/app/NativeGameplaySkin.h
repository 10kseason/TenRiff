#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
namespace tenriff::app {
struct NativeGameplayLayer { float x=0,y=0,width=0,height=0; std::uint32_t color=0xFFFFFF; float mix=1,alpha=1,radius=0; };
// Presentation only. Never consulted by the engine, audio thread or judgement logic.
struct NativeGameplaySkinStyle {
 std::unordered_map<std::string,float> metrics, motion;
 std::unordered_map<std::string,std::array<float,4>> colors, rects;
 std::unordered_map<std::string,std::string> fonts;
 // Presence matters: an explicit empty sprite draws transparent, without fallback.
 std::unordered_map<std::string,std::vector<NativeGameplayLayer>> sprites;
};
struct NativeGameplayNumberSlot { const char* key; float value, minimum, maximum; };
inline constexpr std::array<NativeGameplayNumberSlot,21> kNativeGameplayMetrics{{
 {"key_min_height",48.0f,8.0f,400.0f},
 {"key_max_height",180.0f,8.0f,400.0f},
 {"key_inset",3.0f,0.0f,32.0f},
 {"key_inset_ratio",0.07f,0.0f,0.45f},
 {"key_bottom_gap",3.0f,0.0f,64.0f},
 {"key_face_gap",7.0f,0.0f,64.0f},
 {"judgement_line_width",2.0f,0.0f,32.0f},
 {"judgement_glow_height",12.0f,0.0f,128.0f},
 {"lane_divider_opacity",0.28f,0.0f,1.0f},
 {"hold_edge_mix",0.68f,0.0f,1.0f},
 {"hold_core_mix",0.62f,0.0f,1.0f},
 {"timing_half_width",124.0f,16.0f,600.0f},
 {"timing_height",8.0f,1.0f,64.0f},
 {"timing_range_ms",80.0f,1.0f,1000.0f},
 {"title_font_size",30.0f,8.0f,144.0f},
 {"score_font_size",30.0f,8.0f,144.0f},
 {"body_font_size",18.0f,8.0f,144.0f},
 {"timing_font_size",18.0f,8.0f,144.0f},
 {"judgement_font_size",52.0f,8.0f,144.0f},
 {"combo_font_size",42.0f,8.0f,144.0f},
 {"key_label_font_size",16.0f,8.0f,144.0f},
}};
inline constexpr std::array<NativeGameplayNumberSlot,11> kNativeGameplayMotion{{
 {"press_response",100.0f,1.0f,500.0f},
 {"release_response",24.0f,1.0f,500.0f},
 {"press_depth",7.0f,0.0f,32.0f},
 {"pressed_light",0.7f,0.0f,2.0f},
 {"hit_light",0.45f,0.0f,2.0f},
 {"burst_rise",60.0f,0.0f,500.0f},
 {"burst_height",24.0f,0.0f,200.0f},
 {"combo_duration_ms",150.0f,1.0f,2000.0f},
 {"combo_scale",1.16f,1.0f,2.0f},
 {"combo_lift",-5.0f,-100.0f,100.0f},
 {"judgement_duration_ms",220.0f,1.0f,2000.0f},
}};
inline constexpr std::array<std::string_view,28> kNativeGameplayColors{{"key_well","key_led","chassis","judgement_line","judgement_glow","hold_edge","hold_core","hold_shadow","burst_core","burst_trail","title","score","body","combo","judgement","timing","key_label","gauge_hard","gauge_normal","gauge_easy","judgement_pg","judgement_gr","judgement_gd","judgement_bd","judgement_pr","timing_fast","timing_slow","gauge_ex_hard"}};
inline constexpr std::array<std::string_view,12> kNativeGameplayRects{{"title","artist","speed","score","stats","combo","judgement","timing","timing_label","gauge","progress","key_label"}};
inline constexpr std::array<std::string_view,7> kNativeGameplayFonts{{"title","score","body","timing","judgement","combo","key_label"}};
inline constexpr std::array<std::string_view,5> kNativeGameplaySprites{{"key_idle","key_pressed","note","hold_head","hold_tail"}};
} // namespace tenriff::app
