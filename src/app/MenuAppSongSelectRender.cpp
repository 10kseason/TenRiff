#include "app/MenuApp.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSongSelectUtils.h"
#include "app/MenuRecordUtils.h"
#include "app/MenuSongUtils.h"
#include "timing/HighResClock.h"

namespace tenriff::app {

namespace {

constexpr int kSongSelectVisibleCardCount = 5;

std::string song_sort_detail_label(MenuApp::SongSortMode mode, bool korean) {
    switch (mode) {
        case MenuApp::SongSortMode::DifficultyDesc: return korean ? "LV 내림" : "LV DESC";
        case MenuApp::SongSortMode::TitleAsc: return korean ? "가-힣" : "A-Z";
        case MenuApp::SongSortMode::TitleDesc: return korean ? "힣-가" : "Z-A";
        case MenuApp::SongSortMode::ArtistAsc: return korean ? "아티스트 가-힣" : "ART A-Z";
        case MenuApp::SongSortMode::ArtistDesc: return korean ? "아티스트 힣-가" : "ART Z-A";
        case MenuApp::SongSortMode::DifficultyAsc:
        default: return korean ? "LV 오름" : "LV ASC";
    }
}

std::string song_group_detail_label(MenuApp::SongGroupMode mode, bool korean) {
    switch (mode) {
        case MenuApp::SongGroupMode::Artist: return korean ? "아티스트" : "ARTIST";
        case MenuApp::SongGroupMode::Level: return korean ? "레벨" : "LEVEL";
        case MenuApp::SongGroupMode::Folder: return korean ? "폴더" : "FOLDER";
        case MenuApp::SongGroupMode::None:
        default: return korean ? "없음" : "NONE";
    }
}

std::string song_group_section_label(MenuApp::SongGroupMode mode, const SongEntry& entry, bool korean) {
    using namespace menu_song_select;

    switch (mode) {
        case MenuApp::SongGroupMode::Artist: {
            const std::string artist = safe_ui_text(entry.artist);
            return !artist.empty() ? artist : (korean ? "아티스트 미상" : "UNKNOWN ARTIST");
        }
        case MenuApp::SongGroupMode::Level:
            if (const std::string label = song_difficulty_label(entry); !label.empty()) {
                return label;
            }
            return "LV ?";
        case MenuApp::SongGroupMode::Folder: {
            const std::string folder = song_group_folder_label(entry);
            return !folder.empty() ? folder : (korean ? "루트" : "ROOT");
        }
        case MenuApp::SongGroupMode::None:
        default:
            return {};
    }
}

}  // namespace

void MenuApp::populate_song_select_render_data(render::MenuRenderData& render,
                                               const std::string& current_track,
                                               const MenuApp::BestResultRecord& current_best,
                                               const MenuApp::LocalPlayRecord* selected_record) {
    using namespace menu_song_select;

    // Song Select, Sources, and Records intentionally share one render payload so the
    // renderer can switch views without a second screen-specific data model.
    render.kind = render::MenuScreenKind::SongSelect;
    render.song_select.profile = options_.profile;
    render.song_select.track = current_track;
    render.song_select.song_count = static_cast<int>(visible_song_count());
    render.song_select.source_count = static_cast<int>(config_.ui.recent_song_sources.size());
    render.song_select.record_count = static_cast<int>(current_song_record_indices_.size());
    render.song_select.showing_sources = (song_select_view_ == SongSelectView::Sources);
    render.song_select.showing_records = (song_select_view_ == SongSelectView::Records);
    const bool korean = ui_uses_korean();
    const auto key_filter_summary = [&]() {
        return song_key_filter_ <= 0 ? ui_text("All Keys", "전체 키") : key_mode_label(std::to_string(song_key_filter_) + "k");
    };
    const auto browser_summary = [&]() {
        std::vector<std::string> parts;
        if (!song_search_query_.empty()) {
            parts.push_back(ui_text("Q ", "검색 ") + safe_ui_text(song_search_query_));
        }
        if (song_level_min_filter_ > 0 || song_level_max_filter_ > 0) {
            if (song_level_min_filter_ <= 0 && song_level_max_filter_ > 0) {
                parts.push_back(ui_text("LV <= ", "LV <= ") + std::to_string(song_level_max_filter_));
            } else if (song_level_min_filter_ > 0 && song_level_max_filter_ <= 0) {
                parts.push_back("LV " + std::to_string(song_level_min_filter_) + "+");
            } else {
                parts.push_back("LV " + std::to_string(song_level_min_filter_) + "-" +
                                std::to_string(song_level_max_filter_));
            }
        }
        if (to_lower_ascii(config_.ui.song_collection_filter) != "all") {
            parts.push_back(ui_text("COL ", "컬렉션 ") + song_collection_filter_label());
        }
        if (!config_.ui.difficulty_table_path.empty()) {
            parts.push_back(ui_text("TABLE ", "난이도표 ") +
                            safe_ui_text(
                                config_.ui.difficulty_table_url.empty()
                                    ? filename_only(config_.ui.difficulty_table_path)
                                    : config_.ui.difficulty_table_url,
                                config_.ui.difficulty_table_url.empty() ? "JSON" : "LINK"));
        }
        if (parts.empty()) {
            return ui_text("NO FILTER", "필터 없음");
        }
        std::string joined;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                joined += " / ";
            }
            joined += parts[i];
        }
        if (!joined.empty()) {
            joined += " / ";
        }
        joined += song_sort_detail_label(song_sort_mode_, korean);
        joined += " / ";
        joined += song_group_detail_label(song_group_mode_, korean);
        return joined;
    };
    render.song_select.high_score =
        render.song_select.showing_sources ? 0 :
        (render.song_select.showing_records && selected_record ? selected_record->score :
         (current_best.has_value ? current_best.best_score : 0));
    render.song_select.current_source_name =
        safe_ui_text(menu_songs::song_source_display_name(songs_path_), ui_text("Songs", "곡"));
    render.song_select.current_source_path = safe_ui_text_or_placeholder(songs_path_, "<invalid path>");
    render.song_select.index_profile_label = ui_song_index_profile_label(config_.mode.song_index_profile);
    render.song_select.background_upscale_prefer_npu = config_.graphics.background_upscale_prefer_npu;
    render.song_select.group_summary = song_group_detail_label(song_group_mode_, korean);
    render.song_select.browser_summary = browser_summary();
    const std::string input_backend_label = current_input_backend_status_label();
    if (!input_backend_label.empty()) {
        if (!render.song_select.browser_summary.empty()) {
            render.song_select.browser_summary += " / ";
        }
        render.song_select.browser_summary += input_backend_label;
    }
    render.song_select.sort_summary = song_sort_detail_label(song_sort_mode_, korean);
    render.song_select.primary_hint =
        multiplayer_selecting_chart_
            ? ui_text("UP/DOWN or wheel  MOVE     ENTER / dbl-click  SELECT FOR LOBBY",
                      "위/아래 또는 휠  이동     ENTER / 더블클릭  로비 곡 선택")
            : (render.song_select.showing_sources
            ? ui_text("UP/DOWN or wheel  MOVE     ENTER / dbl-click  OPEN SOURCE     PGUP/PGDN  PAGE",
                      "위/아래 또는 휠  이동     ENTER / 더블클릭  소스 열기     PGUP/PGDN  페이지")
            : (render.song_select.showing_records
                   ? ui_text("UP/DOWN or wheel  MOVE     ENTER / dbl-click  OPEN RESULT     PGUP/PGDN  PAGE",
                             "위/아래 또는 휠  이동     ENTER / 더블클릭  결과 열기     PGUP/PGDN  페이지")
                   : ui_text("UP/DOWN or wheel  MOVE     ENTER / dbl-click  PLAY     PGUP/PGDN  PAGE",
                             "위/아래 또는 휠  이동     ENTER / 더블클릭  플레이     PGUP/PGDN  페이지")));
    render.song_select.secondary_hint =
        multiplayer_selecting_chart_
            ? ui_text("ESC / BACKSPACE  BACK TO MULTIPLAYER LOBBY",
                      "ESC / BACKSPACE  멀티플레이 로비로 돌아가기")
            : (render.song_select.showing_sources
            ? ui_text("LEFT/RIGHT NAV     BACKSPACE BACK     F2 FOLDER     -/+ RATE     F5 REINDEX     F1 HELP",
                      "좌/우 탐색     BACKSPACE 뒤로     F2 폴더     -/+ 배속     F5 재인덱스     F1 도움말")
            : (render.song_select.showing_records
                   ? ui_text("LEFT/RIGHT  NAV FOCUS     BACKSPACE  BACK     ESC  TITLE     F1  HELP",
                             "좌/우  탐색 전환     BACKSPACE  뒤로     ESC  타이틀     F1  도움말")
                   : ui_text("LEFT/RIGHT NAV     ENTER SEARCH     F2 FOLDER     -/+ RATE     F5 REINDEX     F1 HELP",
                             "좌/우 탐색     ENTER 검색     F2 폴더     -/+ 배속     F5 재인덱스     F1 도움말")));

    const std::string source_detail =
        std::to_string(render.song_select.source_count) + " " +
        ui_text(render.song_select.source_count == 1 ? "ROOT" : "ROOTS", "소스");
    const std::string group_detail = render.song_select.group_summary;
    const std::string browser_detail = render.song_select.browser_summary;
    const std::string records_detail =
        (render.song_select.record_count > 0)
            ? (std::to_string(render.song_select.record_count) + " " + ui_text("PLAYS", "플레이"))
            : ui_text("NO PLAYS", "플레이 없음");

    render.song_select.indexing = song_indexer_.is_running();
    if (render.song_select.indexing) {
        const auto progress = song_indexer_.progress();
        switch (progress.stage) {
            case SongIndexProgressStage::ScanningFiles:
                render.song_select.indexing_stage = ui_text("SCANNING FILES", "파일 스캔");
                break;
            case SongIndexProgressStage::BuildingMetadata:
                render.song_select.indexing_stage = ui_text("BUILDING METADATA", "메타데이터 생성");
                break;
            case SongIndexProgressStage::SavingCache:
                render.song_select.indexing_stage = ui_text("WRITING CACHE", "캐시 저장");
                break;
            default:
                render.song_select.indexing_stage = ui_text("INDEXING", "인덱싱");
                break;
        }
        render.song_select.indexing_processed = std::max(0, progress.processed);
        render.song_select.indexing_total = progress.total;
        if (progress.total > 0) {
            const int processed = std::max(0, std::min(progress.processed, progress.total));
            const int percent = static_cast<int>(std::llround(
                100.0 * static_cast<double>(processed) / static_cast<double>(progress.total)));
            render.song_select.indexing_percent = percent;

            const int remaining = progress.total - processed;
            const int64_t now_ns = timing::HighResClock::now_ns();
            const int64_t elapsed_ns = (progress.started_ns > 0 && now_ns > progress.started_ns)
                                           ? (now_ns - progress.started_ns)
                                           : 0;
            if (remaining > 0 && processed > 0 && elapsed_ns > 0) {
                const double elapsed_sec = static_cast<double>(elapsed_ns) / 1'000'000'000.0;
                const double eta_sec =
                    elapsed_sec * static_cast<double>(remaining) / static_cast<double>(processed);
                render.song_select.indexing_eta =
                    format_eta_seconds(static_cast<int64_t>(std::llround(eta_sec)));
            }
        } else if (progress.total == 0) {
            render.song_select.indexing_percent = 0;
            render.song_select.indexing_eta = "0s";
        } else {
            render.song_select.indexing_percent = -1;
            render.song_select.indexing_eta.clear();
        }
    }

    render.song_select.left_nav = {
        render::MenuButtonData{ui_text("SONGS", "곡 목록"), "S", song_select_nav_cursor_ == 0,
                               render.song_select.showing_sources || render.song_select.showing_records
                                   ? (std::to_string(render.song_select.song_count) + " " + ui_text("CHARTS", "차트"))
                                   : ui_text("ACTIVE", "활성")},
        render::MenuButtonData{ui_text("SOURCES", "소스"), "D", song_select_nav_cursor_ == 1,
                               config_.ui.recent_song_sources.empty()
                                   ? ui_text("ADD FOLDER", "폴더 추가")
                                   : (render.song_select.showing_sources ? ui_text("ACTIVE", "활성") : source_detail)},
        render::MenuButtonData{ui_text("SEARCH", "검색"), "F", song_select_nav_cursor_ == 2,
                               song_search_query_.empty()
                                   ? (song_select_search_active_
                                          ? ui_text("<typing>", "<입력 중>")
                                          : ui_text("<type to search>", "<검색어 입력>"))
                                   : (song_select_search_active_
                                          ? ui_text("Q ", "검색 ") + safe_ui_text(song_search_query_) + "  " +
                                                ui_text("(typing)", "(입력 중)")
                                          : ui_text("Q ", "검색 ") + safe_ui_text(song_search_query_))},
        render::MenuButtonData{ui_text("FILTER", "필터"), "=", song_select_nav_cursor_ == 3, browser_detail},
        render::MenuButtonData{ui_text("RECORDS", "기록"), "R", song_select_nav_cursor_ == 4,
                               render.song_select.showing_records ? ui_text("ACTIVE", "활성") : records_detail},
        render::MenuButtonData{ui_text("OPTIONS", "옵션"), "O", song_select_nav_cursor_ == 5,
                               format_multiplier(config_.speed.rate) + " / HS " +
                                   format_decimal(config_.speed.hi_speed)},
    };

    if (render.song_select.showing_sources) {
        const int total_sources = static_cast<int>(config_.ui.recent_song_sources.size());
        if (total_sources > 0) {
            selected_source_ = clamp_int(selected_source_, 0, total_sources - 1);
            constexpr int visible = kSongSelectVisibleCardCount;
            int start = std::max(0, selected_source_ - (visible / 2));
            const int max_start = std::max(0, total_sources - visible);
            start = std::min(start, max_start);
            const int end = std::min(total_sources, start + visible);
            render.song_select.list_total_count = total_sources;
            render.song_select.list_visible_count = end - start;
            render.song_select.list_window_start = start;
            render.song_select.list_selected_index = selected_source_;

            for (int i = start; i < end; ++i) {
                const std::string& source_path = config_.ui.recent_song_sources[static_cast<std::size_t>(i)];
                render::SongCardData card;
                card.title = safe_ui_text(menu_songs::song_source_display_name(source_path), "<invalid title>");
                card.artist = safe_ui_text_or_placeholder(source_path, "<invalid artist>");
                const auto count_it =
                    source_song_counts_.find(menu_songs::normalize_path_key(path_from_utf8(source_path)));
                card.level = (count_it != source_song_counts_.end()) ? count_it->second : 0;
                card.song_index = i;
                card.selected = (i == selected_source_);
                card.detail = safe_ui_text((menu_songs::normalize_path_key(path_from_utf8(source_path)) ==
                                            menu_songs::normalize_path_key(path_from_utf8(songs_path_)))
                                               ? ui_text("CURRENT SOURCE", "현재 소스")
                                               : ui_text("RECENT SOURCE", "최근 소스"));
                render.song_select.songs.push_back(std::move(card));
            }

            const std::string& selected_source =
                config_.ui.recent_song_sources[static_cast<std::size_t>(selected_source_)];
            render.song_select.selected_source_name =
                safe_ui_text(menu_songs::song_source_display_name(selected_source), ui_text("Songs", "곡"));
            render.song_select.selected_source_path =
                safe_ui_text_or_placeholder(selected_source, "<invalid path>");
            const auto count_it =
                source_song_counts_.find(menu_songs::normalize_path_key(path_from_utf8(selected_source)));
            render.song_select.selected_source_song_count =
                (count_it != source_song_counts_.end()) ? count_it->second : -1;
            render.song_select.selected_source_active =
                menu_songs::normalize_path_key(path_from_utf8(selected_source)) ==
                menu_songs::normalize_path_key(path_from_utf8(songs_path_));
        }
        if (total_sources == 0) {
            render.song_select.empty_title = ui_text("NO SONG SOURCES", "불러온 곡 소스 없음");
            render.song_select.empty_message =
                ui_text("Use SOURCES > Add Songs Folder, press F2, or drop a folder onto the window.",
                        "SOURCES > 폴더 추가를 사용하거나 F2를 누르거나, 폴더를 창에 드래그 앤 드롭하세요.");
        }
    } else if (render.song_select.showing_records) {
        const int total = static_cast<int>(current_song_record_indices_.size());
        if (total > 0) {
            constexpr int visible = kSongSelectVisibleCardCount;
            selected_record_ = clamp_int(selected_record_, 0, total - 1);
            int start = std::max(0, selected_record_ - (visible / 2));
            const int max_start = std::max(0, total - visible);
            start = std::min(start, max_start);
            const int end = std::min(total, start + visible);
            render.song_select.list_total_count = total;
            render.song_select.list_visible_count = end - start;
            render.song_select.list_window_start = start;
            render.song_select.list_selected_index = selected_record_;

            for (int i = start; i < end; ++i) {
                const LocalPlayRecord& record =
                    local_play_records_[current_song_record_indices_[static_cast<std::size_t>(i)]];
                render::SongCardData card;
                card.title = menu_records::compact_timestamp_label(record.created_utc);
                card.artist = record.player_name + "  " + record.clear_status + "  " +
                              record.rank + "  " + ui_text("SCORE ", "점수 ") +
                              format_int_with_commas(record.score);
                card.detail = record.replay_path.empty()
                                  ? ui_text("RESULT ONLY", "결과만 있음")
                                  : ui_text("REPLAY ", "리플레이 ") + filename_only(record.replay_path);
                card.song_index = i;
                card.selected = (i == selected_record_);
                render.song_select.songs.push_back(std::move(card));
            }
        }
        if (total == 0) {
            render.song_select.empty_title = ui_text("NO LOCAL RECORDS", "로컬 기록 없음");
            render.song_select.empty_message =
                ui_text("Play a chart first. Saved results and replays will appear here.",
                        "먼저 차트를 플레이하세요. 저장된 결과와 리플레이가 여기에 표시됩니다.");
        }
    } else {
        const int total = static_cast<int>(visible_song_count());
        if (total > 0) {
            constexpr int visible = kSongSelectVisibleCardCount;
            int start = std::max(0, selected_song_ - (visible / 2));
            const int max_start = std::max(0, total - visible);
            start = std::min(start, max_start);
            const int end = std::min(total, start + visible);
            render.song_select.list_total_count = total;
            render.song_select.list_visible_count = end - start;
            render.song_select.list_window_start = start;
            render.song_select.list_selected_index = selected_song_;
            std::string last_group_label;

            for (int i = start; i < end; ++i) {
                const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(i));
                if (!entry) {
                    continue;
                }
                render::SongCardData card;
                if (song_group_mode_ != SongGroupMode::None) {
                    const std::string group_label = song_group_section_label(song_group_mode_, *entry, korean);
                    if (render.song_select.songs.empty() || group_label != last_group_label) {
                        card.group_label = group_label;
                    }
                    last_group_label = group_label;
                }
                card.title = song_title_for_ui(*entry);
                card.artist = song_artist_for_ui(*entry);
                card.detail = safe_ui_text_or_placeholder(song_detail_label(*entry), "<invalid detail>");
                card.background_path = song_background_preview_path_for_entry(*entry);
                const BestResultRecord best = best_result_for_song_entry(*entry);
                card.lamp = best.has_value ? best.clear_status : std::string{};
                card.level_label = song_difficulty_label(*entry);
                card.level = entry->level;
                card.rating = entry->rating;
                card.song_index = i;
                card.selected = (i == selected_song_);
                const std::string song_key =
                    menu_songs::normalize_path_key(path_from_utf8(song_absolute_path(*entry)));
                card.favorite =
                    !song_key.empty() &&
                    song_membership_contains(favorite_song_keys_, song_key);
                render.song_select.songs.push_back(std::move(card));
            }
        }
        if (total == 0) {
            if (multiplayer_selecting_chart_) {
                render.song_select.empty_title = ui_text("NO SHARED CHARTS", "공통 보유 차트 없음");
                render.song_select.empty_message =
                    ui_text("Only byte-identical BMS charts owned by every connected player are shown. Change song sources or reindex to refresh the shared list.",
                            "현재 접속자 전원이 가진 바이트 단위 동일 BMS만 표시합니다. 곡 소스를 바꾸거나 재인덱스해 공통 목록을 갱신하세요.");
            } else if (!song_search_query_.empty() || song_key_filter_ > 0 || song_level_min_filter_ > 0 ||
                song_level_max_filter_ > 0) {
                render.song_select.empty_title = ui_text("NO CHARTS MATCH", "일치하는 차트 없음");
                render.song_select.empty_message =
                    ui_text("Clear the current search/filter or switch the active source to see more charts.",
                            "현재 검색/필터를 지우거나 활성 소스를 바꿔 더 많은 차트를 보세요.");
            } else if (render.song_select.indexing) {
                render.song_select.empty_title = ui_text("BUILDING LIBRARY", "라이브러리 생성 중");
                render.song_select.empty_message =
                    ui_text("The current source is still indexing. You can keep browsing while scan progress updates above.",
                            "현재 소스를 아직 인덱싱 중입니다. 위 진행 상황이 갱신되는 동안 계속 둘러볼 수 있습니다.");
            } else {
                render.song_select.empty_title = ui_text("NO CHARTS INDEXED", "인덱싱된 차트 없음");
                render.song_select.empty_message =
                    ui_text("Use SOURCES > Add Songs Folder, press F2, or drop a songs folder. Then use F5 to reindex.",
                            "SOURCES > 폴더 추가를 사용하거나 F2를 누르거나 곡 폴더를 드래그 앤 드롭한 뒤 F5로 재인덱스하세요.");
            }
        }
    }

    if (!render.song_select.showing_sources) {
        if (const SongEntry* entry = (selected_song_ >= 0)
                                         ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                         : nullptr) {
            render.song_select.selected_song_title = song_title_for_ui(*entry);
            render.song_select.selected_song_artist = song_artist_for_ui(*entry);
            render.song_select.selected_song_detail = safe_ui_text_or_placeholder(song_detail_label(*entry), "-");
            render.song_select.selected_song_background_path = selected_song_background_preview_path();
            render.song_select.background_upscale_mode = config_.graphics.background_upscale_mode;
            render.song_select.background_upscale_model_path =
                config_.graphics.background_upscale_model_path;
            render.song_select.selected_song_lamp =
                current_best.has_value ? current_best.clear_status : ui_text("NO PLAY", "기록 없음");
            render.song_select.selected_song_favorite = selected_song_is_favorite();
            render.song_select.selected_song_collection_filter = song_collection_filter_label();
            if (!config_.mode.ghost_battle_enabled) {
                render.song_select.selected_song_ghost_status = ui_text("OFF", "끔");
            } else if (!best_replay_path_for_selected_song().empty()) {
                render.song_select.selected_song_ghost_status = ui_text("READY", "준비됨");
            } else {
                render.song_select.selected_song_ghost_status = ui_text("NONE", "없음");
            }
        }
    }

    if (render.song_select.showing_records) {
        if (selected_record) {
            render.song_select.rank = selected_record->rank;
            render.song_select.best_score = selected_record->score;
            render.song_select.max_combo = selected_record->max_combo;
            render.song_select.perfect = selected_record->perfect;
            render.song_select.great = selected_record->great;
            render.song_select.good = selected_record->good;
            render.song_select.bad = selected_record->bad;
            render.song_select.poor = selected_record->poor;
            render.song_select.accuracy = selected_record->accuracy;
            render.song_select.selected_record_created_utc =
                menu_records::compact_timestamp_label(selected_record->created_utc);
            render.song_select.selected_record_status =
                selected_record->player_name + " / " + selected_record->clear_status;
            render.song_select.selected_record_replay_file = filename_only(selected_record->replay_path);
            if (const ReplaySummary* replay = replay_summary_for_path(selected_record->replay_path)) {
                render.song_select.selected_record_replay_lane_count = replay->lane_count;
                render.song_select.selected_record_replay_event_count = replay->event_count;
                if (!replay->exists) {
                    render.song_select.selected_record_replay_detail = ui_text("Replay missing", "리플레이 파일 없음");
                } else if (!replay->error.empty()) {
                    render.song_select.selected_record_replay_detail = ui_text("Replay parse warning", "리플레이 파싱 경고");
                } else {
                    render.song_select.selected_record_replay_detail =
                        format_multiplier(replay->rate) + " / " +
                        format_signed_offset_ms(replay->input_offset_ms);
                }
            } else {
                render.song_select.selected_record_replay_detail = ui_text("No replay", "리플레이 없음");
            }
        }
    } else if (!render.song_select.showing_sources) {
        render.song_select.rank = current_best.has_value ? current_best.rank : "--";
        render.song_select.best_score = current_best.has_value ? current_best.best_score : 0;
        render.song_select.max_combo = current_best.has_value ? current_best.max_combo : 0;
        render.song_select.perfect = current_best.has_value ? current_best.perfect : 0;
        render.song_select.great = current_best.has_value ? current_best.great : 0;
        render.song_select.good = current_best.has_value ? current_best.good : 0;
        render.song_select.bad = current_best.has_value ? current_best.bad : 0;
        render.song_select.poor = current_best.has_value ? current_best.poor : 0;
    }
}

void MenuApp::populate_song_browser_render_data(render::MenuRenderData& render) {
    using namespace menu_song_select;

    render.kind = render::MenuScreenKind::GenericList;

    append_menu_row(render.generic,
                    ui_text("Sort", "정렬"),
                    song_sort_detail_label(song_sort_mode_, ui_uses_korean()),
                    settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow,
                    0,
                    false,
                    true);
    append_menu_row(render.generic,
                    ui_text("Group", "그룹"),
                    song_group_detail_label(song_group_mode_, ui_uses_korean()),
                    settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow,
                    1,
                    false,
                    true);
    append_menu_row(render.generic,
                    ui_text("Key Filter", "키 필터"),
                    (song_key_filter_ <= 0 ? ui_text("All Keys", "전체 키") : key_mode_label(std::to_string(song_key_filter_) + "k")),
                    settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow,
                    2,
                    false,
                    true);
    append_menu_row(render.generic,
                    ui_text("Difficulty Min", "난이도 최소"),
                    song_level_min_filter_ > 0 ? std::to_string(song_level_min_filter_) : ui_text("Any", "제한 없음"),
                    settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow,
                    3,
                    false,
                    true);
    append_menu_row(render.generic,
                    ui_text("Difficulty Max", "난이도 최대"),
                    song_level_max_filter_ > 0 ? std::to_string(song_level_max_filter_) : ui_text("Any", "제한 없음"),
                    settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow,
                    4,
                    false,
                    true);
    append_menu_row(render.generic,
                    ui_text("Difficulty Table", "난이도표"),
                    config_.ui.difficulty_table_path.empty()
                        ? ui_text("Native LV", "기본 LV")
                        : safe_ui_text(
                              config_.ui.difficulty_table_url.empty()
                                  ? filename_only(config_.ui.difficulty_table_path)
                                  : config_.ui.difficulty_table_url,
                              config_.ui.difficulty_table_url.empty() ? "JSON" : "LINK"),
                    settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow,
                    5,
                    false,
                    true);
    append_menu_row(render.generic,
                    ui_text("Collection Filter", "컬렉션 필터"),
                    song_collection_filter_label(),
                    settings_cursor_ == 6,
                    render::MenuHitTargetKind::SettingsRow,
                    6,
                    false,
                    true);
    const std::string named_collection = current_named_song_collection();
    const bool toggles_collection = !named_collection.empty();
    append_menu_row(render.generic,
                    toggles_collection ? ui_text("Toggle Collection Item", "컬렉션 곡 토글")
                                       : ui_text("Toggle Favorite", "페이보릿 토글"),
                    toggles_collection
                        ? ui_text(selected_song_is_in_collection(named_collection) ? "In Collection" : "Not In Collection",
                                  selected_song_is_in_collection(named_collection) ? "컬렉션에 포함됨" : "컬렉션에 없음")
                        : ui_text(selected_song_is_favorite() ? "Favorite" : "Not Favorite",
                                  selected_song_is_favorite() ? "페이보릿" : "페이보릿 아님"),
                    settings_cursor_ == 7,
                    render::MenuHitTargetKind::SettingsRow,
                    7,
                    true,
                    false);
    append_menu_row(render.generic,
                    ui_text("New Collection", "새 컬렉션"),
                    current_named_song_collection().empty() ? ui_text("Create", "생성") : current_named_song_collection(),
                    settings_cursor_ == 8,
                    render::MenuHitTargetKind::SettingsRow,
                    8,
                    true,
                    false);
    append_menu_row(render.generic,
                    ui_text("Clear Filters", "필터 지우기"),
                    "",
                    settings_cursor_ == 9,
                    render::MenuHitTargetKind::SettingsRow,
                    9,
                    true,
                    false);
    append_menu_row(render.generic,
                    ui_text("Back", "뒤로"),
                    "",
                    settings_cursor_ == 10,
                    render::MenuHitTargetKind::SettingsRow,
                    10,
                    true,
                    false);

    render.generic.notes.push_back(ui_text("Search lives on Song Select. Use the SEARCH item there for title, artist, and path matches.",
                                           "검색은 Song Select에 있습니다. 제목, 아티스트, 경로 검색은 SEARCH 항목을 사용하세요."));
    render.generic.notes.push_back(ui_text("Sort and Group moved here so the left rail only separates Songs, Sources, Search, Filter, Records, and Options.",
                                           "왼쪽 레일이 곡/소스/검색/필터/기록/옵션만 남도록 정렬과 그룹을 이 화면으로 옮겼습니다."));
    render.generic.notes.push_back(ui_text(
        "Difficulty Table: copy an http(s) BMSTable page/header link and press Enter, or press Right to select local JSON. Left clears it. Matching levels drive badges, grouping, sorting, and numeric filters.",
        "난이도표: http(s) BMSTable 페이지/헤더 링크를 복사하고 Enter를 누르거나, 오른쪽 키로 로컬 JSON을 고릅니다. 왼쪽 키는 해제입니다. 일치 레벨은 배지·그룹·정렬·숫자 필터에 반영됩니다."));
    render.generic.notes.push_back(ui_text("Collection Filter cycles All, Favorites, and any named collections you created.",
                                           "컬렉션 필터는 전체, 페이보릿, 생성한 컬렉션들을 순환합니다."));
    render.generic.notes.push_back(ui_text("Toggle Favorite works from All/Favorites. When a named collection is selected it toggles membership in that collection.",
                                           "전체/페이보릿에서는 페이보릿을 토글하고, 이름 있는 컬렉션이 선택된 상태에서는 그 컬렉션 포함 여부를 토글합니다."));
    render.generic.notes.push_back(ui_text("Key Filter supports All plus 4K-10K, 12K, 14K, and 16K.",
                                           "키 필터는 전체, 4K~10K, 12K, 14K, 16K를 지원합니다."));
}

}  // namespace tenriff::app
