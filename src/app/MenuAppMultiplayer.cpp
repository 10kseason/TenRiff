#include "app/MenuApp.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "app/MenuAppSkinUtils.h"
#include "app/MenuSongUtils.h"
#include "config/KeycodeMap.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

bool peer_session_is_active(network::PeerSessionState state) {
    switch (state) {
        case network::PeerSessionState::Starting:
        case network::PeerSessionState::Listening:
        case network::PeerSessionState::Resolving:
        case network::PeerSessionState::Connecting:
        case network::PeerSessionState::Handshaking:
        case network::PeerSessionState::Connected:
        case network::PeerSessionState::Closing:
            return true;
        case network::PeerSessionState::Idle:
        case network::PeerSessionState::Disconnected:
        case network::PeerSessionState::Failed:
        default:
            return false;
    }
}

bool peer_round_ui_locked(const network::PeerSessionSnapshot& peer) {
    return peer.round_active || peer.round_transition_pending;
}

std::string peer_session_state_label(network::PeerSessionState state) {
    switch (state) {
        case network::PeerSessionState::Starting: return "STARTING";
        case network::PeerSessionState::Listening: return "LISTENING";
        case network::PeerSessionState::Resolving: return "RESOLVING";
        case network::PeerSessionState::Connecting: return "CONNECTING";
        case network::PeerSessionState::Handshaking: return "HANDSHAKING";
        case network::PeerSessionState::Connected: return "CONNECTED";
        case network::PeerSessionState::Closing: return "CLOSING";
        case network::PeerSessionState::Disconnected: return "DISCONNECTED";
        case network::PeerSessionState::Failed: return "FAILED";
        case network::PeerSessionState::Idle:
        default: return "IDLE";
    }
}

std::optional<char> multiplayer_character_from_keycode(uint32_t keycode) {
    const std::string name = config::KeycodeMap::to_name(keycode);
    if (name.size() == 1) {
        const char ch = name.front();
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            return ch;
        }
    }
    if (name == "Period" || name == "Decimal") return '.';
    if (name == "Minus" || name == "Subtract") return '-';
    return std::nullopt;
}

#ifdef _WIN32
bool current_process_is_foreground() {
    const HWND foreground = GetForegroundWindow();
    if (!foreground) return false;
    DWORD process_id = 0;
    GetWindowThreadProcessId(foreground, &process_id);
    return process_id == GetCurrentProcessId();
}
#endif

}  // namespace

void MenuApp::populate_multiplayer_render_data(render::MenuRenderData& render) {
    const network::PeerSessionSnapshot peer = peer_session_.snapshot();
    const bool active = peer_session_is_active(peer.state);
    const bool connected = peer.state == network::PeerSessionState::Connected;
    const bool chart_matches = multiplayer_chart_fingerprints_match(multiplayer_menu_);
    const bool round_active = last_game_was_multiplayer_ || peer_round_ui_locked(peer);
    const bool leader_can_choose_chart =
        !round_active && multiplayer_leader_can_choose_chart(multiplayer_menu_) &&
        peer.remote_library_ready && multiplayer_common_chart_count_ > 0;
    const bool can_start = !round_active && multiplayer_leader_can_start(multiplayer_menu_);

    const auto row_selected = [this](MultiplayerMenuRow row) {
        return multiplayer_menu_.cursor == static_cast<int>(row);
    };
    const std::string address_value =
        multiplayer_menu_.address +
        (multiplayer_menu_.edit_field == MultiplayerEditField::Address ? " _" : "");
    const std::string port_value =
        multiplayer_menu_.port_text +
        (multiplayer_menu_.edit_field == MultiplayerEditField::Port ? " _" : "");
    const network::LanDiscoveredRoom* selected_lan_room =
        multiplayer_lan_rooms_.empty()
            ? nullptr
            : &multiplayer_lan_rooms_[std::min(
                  multiplayer_lan_room_index_, multiplayer_lan_rooms_.size() - 1u)];
    std::string lan_room_value;
    if (active) {
        lan_room_value = ui_text("AVAILABLE WHILE DISCONNECTED", "연결 해제 후 사용");
    } else if (!selected_lan_room) {
        lan_room_value = ui_text("SCANNING LOCAL NETWORK...", "LAN 방 검색 중...");
    } else {
        lan_room_value = selected_lan_room->host_name + " / " +
                         std::to_string(selected_lan_room->player_count) + "/" +
                         std::to_string(selected_lan_room->max_players);
        if (!selected_lan_room->accepting_players) {
            lan_room_value += ui_text(" / BUSY", " / 참가 불가");
        }
    }
    std::string chat_value;
    if (!connected) {
        chat_value = ui_text("CONNECT FIRST", "먼저 연결");
    } else if (multiplayer_menu_.chat_input.empty()) {
        chat_value = ui_text("ENTER TO CHAT", "ENTER로 채팅");
    } else {
        chat_value = "> " + multiplayer_menu_.chat_input;
    }
    if (multiplayer_menu_.edit_field == MultiplayerEditField::Chat) {
        chat_value += " _";
    }

    append_menu_row(render.generic,
                    ui_text("Server IP / Hostname", "서버 IP / 호스트명"),
                    address_value,
                    row_selected(MultiplayerMenuRow::Address),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(MultiplayerMenuRow::Address),
                    !active,
                    false);
    append_menu_row(render.generic,
                    ui_text("TCP Port", "TCP 포트"),
                    port_value,
                    row_selected(MultiplayerMenuRow::Port),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(MultiplayerMenuRow::Port),
                    !active,
                    false);
    append_menu_row(render.generic,
                    ui_text("LAN Rooms", "LAN 방 자동 검색"),
                    lan_room_value,
                    row_selected(MultiplayerMenuRow::LanRoom),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(MultiplayerMenuRow::LanRoom),
                    !active && selected_lan_room && selected_lan_room->accepting_players,
                    false);
    append_menu_row(render.generic,
                    ui_text("Host", "호스트"),
                    peer.role == network::PeerRole::Host ? peer_session_state_label(peer.state) : "",
                    row_selected(MultiplayerMenuRow::Host),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(MultiplayerMenuRow::Host),
                    true,
                    false);
    append_menu_row(render.generic,
                    ui_text("Join by IP", "IP로 참가"),
                    peer.role == network::PeerRole::Joiner ? peer_session_state_label(peer.state) : "",
                    row_selected(MultiplayerMenuRow::Join),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(MultiplayerMenuRow::Join),
                    true,
                    false);
    std::string chart_value;
    if (multiplayer_menu_.local_is_leader) {
        if (!connected || !peer.remote_library_ready) {
            chart_value = ui_text("Syncing song libraries", "곡 목록 동기화 중");
        } else if (multiplayer_common_chart_count_ == 0) {
            chart_value = ui_text("No shared songs", "공통 보유곡 없음");
        } else {
            chart_value = multiplayer_chart_title_.empty()
                              ? ui_text("Choose Shared Song", "공통 곡 선택")
                              : multiplayer_chart_title_;
        }
    } else if (peer.selected_chart.fingerprint.valid()) {
        chart_value = peer.selected_chart.name.empty()
                          ? ui_text("Matching leader HASH", "리더 HASH 곡 검색 중")
                          : peer.selected_chart.name;
        if (chart_matches && !multiplayer_chart_path_.empty()) {
            chart_value += ui_text(" / HASH MATCH", " / HASH 일치");
        }
    } else {
        chart_value = ui_text("Waiting for leader", "리더 선곡 대기");
    }
    append_menu_row(render.generic,
                    ui_text("Battle Chart", "대전 차트"),
                    chart_value,
                    row_selected(MultiplayerMenuRow::Chart),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(MultiplayerMenuRow::Chart),
                    leader_can_choose_chart,
                    false);

    std::string ready_value = round_active
                                  ? ui_text("ROUND IN PROGRESS", "대전 종료 대기")
                                  : (multiplayer_menu_.local_ready ? ui_text("YOU READY", "나 준비")
                                                                   : ui_text("YOU WAIT", "나 대기"));
    ready_value += " / ";
    ready_value += multiplayer_menu_.peer_ready ? ui_text("ALL READY", "상대 준비")
                                                 : ui_text("PLAYERS WAIT", "상대 대기");
    append_menu_row(render.generic,
                    ui_text("Ready", "준비"),
                    ready_value,
                    row_selected(MultiplayerMenuRow::Ready),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(MultiplayerMenuRow::Ready),
                    !round_active && connected && chart_matches,
                    false);
    append_menu_row(render.generic,
                    ui_text("Start Match", "대전 시작"),
                    can_start ? ui_text("READY TO START", "시작 가능")
                              : ui_text("LEADER + ALL READY", "리더 + 전원 준비 필요"),
                    row_selected(MultiplayerMenuRow::Start),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(MultiplayerMenuRow::Start),
                    can_start,
                    false);
    append_menu_row(render.generic,
                    ui_text("Room Chat", "방 채팅"),
                    chat_value,
                    row_selected(MultiplayerMenuRow::Chat),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(MultiplayerMenuRow::Chat),
                    connected,
                    false);
    append_menu_row(render.generic,
                    ui_text("Options", "옵션"),
                    multiplayer_menu_.local_ready
                        ? ui_text("READY WILL BE CLEARED", "진입 시 준비 해제")
                        : "",
                    row_selected(MultiplayerMenuRow::Options),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(MultiplayerMenuRow::Options),
                    !round_active,
                    false);
    append_menu_row(render.generic,
                    ui_text("Back", "뒤로"),
                    "",
                    row_selected(MultiplayerMenuRow::Back),
                    render::MenuHitTargetKind::SettingsRow,
                    static_cast<int>(MultiplayerMenuRow::Back),
                    true,
                    false);

    std::string connection_line =
        ui_text("Connection: ", "연결: ") + peer_session_state_label(peer.state);
    connection_line += " / " + ui_text("Players: ", "인원: ") +
                       std::to_string(peer.participant_count) + "/8";
    render.generic.notes.push_back(std::move(connection_line));
    for (const auto& participant : peer.participants) {
        std::string player_line =
            "#" + std::to_string(participant.player_id) + " " + participant.name;
        if (participant.local) player_line += ui_text(" [YOU]", " [나]");
        if (participant.leader) player_line += ui_text(" [LEADER]", " [리더]");
        player_line += participant.ready
                           ? ui_text(" / READY", " / 준비")
                           : ui_text(" / WAIT", " / 대기");
        render.generic.notes.push_back(std::move(player_line));
    }
    if (!peer.status_detail.empty()) {
        render.generic.notes.push_back(peer.status_detail);
    }
    if (connected) {
        if (peer.remote_library_ready) {
            render.generic.notes.push_back(
                ui_text("Shared BMS charts for all players: ", "전원 공통 BMS: ") +
                std::to_string(multiplayer_common_chart_count_) + " / " +
                std::to_string(indexed_songs_.size()));
        } else {
            render.generic.notes.push_back(
                ui_text("Syncing exact BMS chart hashes for every player...", "전원의 정확한 BMS 차트 HASH 동기화 중..."));
        }
    }
    if (!multiplayer_status_message_.empty()) {
        render.generic.notes.push_back(multiplayer_status_message_);
    }
    const network::LanDiscoverySnapshot lan_snapshot = lan_discovery_.snapshot();
    if (!lan_snapshot.status_detail.empty()) {
        render.generic.notes.push_back(
            ui_text("LAN discovery: ", "LAN 검색: ") + lan_snapshot.status_detail);
    } else if (lan_snapshot.advertising) {
        render.generic.notes.push_back(ui_text(
            "This room is visible to TenRiff players on the same LAN.",
            "이 방은 같은 LAN의 TenRiff에서 자동으로 보입니다."));
    }
    if (connected && !multiplayer_menu_.local_is_leader &&
        peer.selected_chart.fingerprint.valid() && !chart_matches) {
        render.generic.notes.push_back(ui_text("Finding the leader BMS chart on this PC by exact HASH.",
                                               "리더 BMS를 이 PC에서 정확한 HASH로 찾는 중입니다."));
    } else if (connected && !multiplayer_menu_.local_chart_fingerprint) {
        render.generic.notes.push_back(ui_text("The current leader chooses a BMS chart; every other player matches it by HASH.",
                                               "현재 리더만 BMS를 고르고 나머지는 HASH로 자동 선택합니다."));
    } else if (connected && multiplayer_menu_.local_chart_fingerprint && !chart_matches) {
        render.generic.notes.push_back(ui_text("Chart mismatch: every player must resolve identical BMS bytes.",
                                               "차트 불일치: 전원이 완전히 같은 BMS 파일을 선택해야 합니다."));
    } else if (connected && chart_matches) {
        render.generic.notes.push_back(ui_text("Chart match confirmed.", "차트 일치가 확인되었습니다."));
    }
    render.generic.notes.push_back(ui_text(
        "Peer battle fixes Rate 1.00x, Gauge Shift, default judge windows, and Random/Mods/Assist off; local key-mode conversion is allowed.",
        "P2P 대전은 Rate 1.00x, Gauge Shift, 기본 판정, 랜덤/모드/어시스트 끔을 사용하며 로컬 키모드 변환은 허용합니다."));
    render.generic.notes.push_back(ui_text(
        "LAN Rooms needs no server or manual IP entry. Internet play still needs an IP and router port forwarding.",
        "LAN 방 자동 검색은 서버와 IP 입력이 필요 없습니다. 인터넷 연결은 여전히 IP와 포트 포워딩이 필요합니다."));
    render.generic.footer_reserved_lines = 6;
    render.generic.footer_notes.push_back(
        ui_text("ROOM CHAT", "방 채팅") + "  " +
        std::to_string(peer.chat_messages.size()) + "/" +
        std::to_string(network::kPeerChatHistoryLimit));
    const std::size_t chat_begin = peer.chat_messages.size() > 4
                                       ? peer.chat_messages.size() - 4
                                       : 0;
    for (std::size_t i = chat_begin; i < peer.chat_messages.size(); ++i) {
        const auto& chat = peer.chat_messages[i];
        const auto sender = std::find_if(
            peer.participants.begin(), peer.participants.end(),
            [&chat](const network::PeerParticipantSnapshot& participant) {
                return participant.player_id == chat.player_id;
            });
        std::string sender_name =
            sender == peer.participants.end()
                ? "#" + std::to_string(chat.player_id)
                : sender->name;
        if (sender != peer.participants.end() && sender->local) {
            sender_name += ui_text(" [YOU]", " [나]");
        }
        if (sender != peer.participants.end() && sender->leader) {
            sender_name += ui_text(" [LEADER]", " [리더]");
        }
        render.generic.footer_notes.push_back(sender_name + ": " + chat.text);
    }
    if (peer.chat_messages.empty()) {
        render.generic.footer_notes.push_back(
            ui_text("No room messages yet.", "아직 방 메시지가 없습니다."));
    }
    render.generic.footer_notes.push_back(ui_text(
        "Enter edits/selects/sends chat. Delete clears an edit field. Esc closes editing or leaves Multiplayer.",
        "Enter로 편집/선택/채팅 전송합니다. Delete는 편집 칸을 지웁니다. Esc는 편집을 닫거나 멀티플레이를 나갑니다."));
}

void MenuApp::handle_multiplayer_input(uint32_t keycode) {
    if (multiplayer_menu_.edit_field == MultiplayerEditField::Chat) {
        if (keycode == key_enter_) {
            const std::string message = util::sanitize_ui_text(multiplayer_menu_.chat_input);
            if (message.empty()) {
                multiplayer_status_message_ =
                    ui_text("Type a message before sending.",
                            "전송할 메시지를 먼저 입력하세요.");
            } else if (peer_session_.send_chat(message)) {
                multiplayer_menu_.chat_input.clear();
                multiplayer_status_message_.clear();
            } else {
                multiplayer_status_message_ =
                    ui_text("Room chat is unavailable because the connection closed.",
                            "연결이 종료되어 방 채팅을 사용할 수 없습니다.");
            }
            publish_snapshot();
            return;
        }
        if (keycode == key_escape_) {
            multiplayer_menu_.edit_field = MultiplayerEditField::None;
            publish_snapshot();
            return;
        }
        if (keycode == key_backspace_) {
            erase_last_multiplayer_utf8_character(multiplayer_menu_.chat_input);
            publish_snapshot();
            return;
        }
        if (keycode == key_delete_) {
            multiplayer_menu_.chat_input.clear();
            publish_snapshot();
            return;
        }
        return;
    }
    if (multiplayer_menu_.edit_field != MultiplayerEditField::None) {
        if (keycode == key_enter_) {
            multiplayer_menu_.edit_field = MultiplayerEditField::None;
            publish_snapshot();
            return;
        }
        if (keycode == key_escape_) {
            multiplayer_menu_.edit_field = MultiplayerEditField::None;
            publish_snapshot();
            return;
        }
        std::string& field = multiplayer_menu_.edit_field == MultiplayerEditField::Address
                                 ? multiplayer_menu_.address
                                 : multiplayer_menu_.port_text;
        if (keycode == key_backspace_) {
            if (!field.empty()) field.pop_back();
            publish_snapshot();
            return;
        }
        if (keycode == key_delete_) {
            field.clear();
            publish_snapshot();
            return;
        }
        const auto character = multiplayer_character_from_keycode(keycode);
        if (!character.has_value()) return;
        const bool changed = multiplayer_menu_.edit_field == MultiplayerEditField::Address
                                 ? try_append_multiplayer_address_character(field, *character)
                                 : try_append_multiplayer_port_character(field, *character);
        if (changed) publish_snapshot();
        return;
    }

    if (keycode == key_up_) {
        multiplayer_menu_.cursor = move_multiplayer_menu_cursor(multiplayer_menu_.cursor, -1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        multiplayer_menu_.cursor = move_multiplayer_menu_cursor(multiplayer_menu_.cursor, 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_escape_ || keycode == key_backspace_) {
        leave_multiplayer();
        return;
    }
    if (keycode != key_enter_ && keycode != key_left_ && keycode != key_right_) {
        return;
    }

    const MultiplayerMenuRow row = static_cast<MultiplayerMenuRow>(
        clamp_multiplayer_menu_cursor(multiplayer_menu_.cursor));
    const network::PeerSessionSnapshot peer = peer_session_.snapshot();
    const bool active = peer_session_is_active(peer.state);
    const bool round_active = last_game_was_multiplayer_ || peer_round_ui_locked(peer);

    const auto begin_network_session = [this](MultiplayerRole role,
                                               const std::string& address,
                                               uint16_t port) {
        multiplayer_status_message_.clear();
        peer_session_.set_local_library(
            build_multiplayer_chart_sha256_inventory(indexed_songs_));
        multiplayer_local_library_index_revision_ = song_index_revision_;
        multiplayer_remote_library_revision_ = 0;
        multiplayer_remote_library_ready_ = false;
        multiplayer_common_chart_count_ = 0;

        bool accepted = false;
        if (role == MultiplayerRole::Host) {
            multiplayer_menu_.role = MultiplayerRole::Host;
            reset_multiplayer_chart_match_search();
            accepted = peer_session_.host(port, profile_display_name());
        } else {
            multiplayer_menu_.role = MultiplayerRole::Join;
            peer_session_.clear_local_chart();
            multiplayer_chart_path_.clear();
            multiplayer_chart_title_.clear();
            multiplayer_chart_fingerprint_ = {};
            multiplayer_menu_.local_chart_fingerprint = 0;
            multiplayer_menu_.local_chart_size = 0;
            reset_multiplayer_chart_match_search();
            accepted = peer_session_.join(address, port, profile_display_name());
        }
        if (!accepted) {
            multiplayer_status_message_ = ui_text("Could not start the network worker.",
                                                  "네트워크 작업을 시작하지 못했습니다.");
        }
        multiplayer_last_revision_ = 0;
        publish_snapshot();
    };

    if (row == MultiplayerMenuRow::Address) {
        if (!active) multiplayer_menu_.edit_field = MultiplayerEditField::Address;
        publish_snapshot();
        return;
    }
    if (row == MultiplayerMenuRow::Port) {
        if (!active) multiplayer_menu_.edit_field = MultiplayerEditField::Port;
        publish_snapshot();
        return;
    }
    if (row == MultiplayerMenuRow::LanRoom) {
        if (active) {
            multiplayer_status_message_ = ui_text(
                "Disconnect before joining another LAN room.",
                "다른 LAN 방에 참가하려면 먼저 연결을 끊으세요.");
            publish_snapshot();
            return;
        }
        if (multiplayer_lan_rooms_.empty()) {
            multiplayer_status_message_ = ui_text(
                "Searching for TenRiff rooms on this LAN...",
                "같은 LAN의 TenRiff 방을 검색 중입니다...");
            publish_snapshot();
            return;
        }
        if (keycode == key_left_ || keycode == key_right_) {
            const std::size_t count = multiplayer_lan_rooms_.size();
            if (keycode == key_left_) {
                multiplayer_lan_room_index_ =
                    (multiplayer_lan_room_index_ + count - 1u) % count;
            } else {
                multiplayer_lan_room_index_ =
                    (multiplayer_lan_room_index_ + 1u) % count;
            }
            multiplayer_status_message_.clear();
            publish_snapshot();
            return;
        }

        multiplayer_lan_room_index_ =
            std::min(multiplayer_lan_room_index_, multiplayer_lan_rooms_.size() - 1u);
        const network::LanDiscoveredRoom& room =
            multiplayer_lan_rooms_[multiplayer_lan_room_index_];
        if (!room.accepting_players) {
            multiplayer_status_message_ = ui_text(
                "That LAN room is full or currently in a match.",
                "해당 LAN 방은 가득 찼거나 대전 중입니다.");
            publish_snapshot();
            return;
        }
        multiplayer_menu_.address = room.address;
        multiplayer_menu_.port_text = std::to_string(room.tcp_port);
        begin_network_session(MultiplayerRole::Join, room.address, room.tcp_port);
        return;
    }
    if (row == MultiplayerMenuRow::Host || row == MultiplayerMenuRow::Join) {
        if (active) {
            peer_session_.disconnect("Disconnected from multiplayer menu");
            multiplayer_status_message_ = ui_text("Disconnected.", "연결을 끊었습니다.");
            publish_snapshot();
            return;
        }
        const auto port = parse_multiplayer_port(multiplayer_menu_.port_text);
        if (!port.has_value()) {
            multiplayer_status_message_ = ui_text("Port must be between 1 and 65535.",
                                                  "포트는 1~65535 사이여야 합니다.");
            publish_snapshot();
            return;
        }
        if (row == MultiplayerMenuRow::Join &&
            !is_valid_multiplayer_address_text(multiplayer_menu_.address)) {
            multiplayer_status_message_ = ui_text("Enter a valid IPv4 address or hostname.",
                                                  "올바른 IPv4 주소 또는 호스트명을 입력하세요.");
            publish_snapshot();
            return;
        }

        if (row == MultiplayerMenuRow::Host) {
            begin_network_session(MultiplayerRole::Host, {}, *port);
        } else {
            begin_network_session(
                MultiplayerRole::Join, multiplayer_menu_.address, *port);
        }
        return;
    }
    if (row == MultiplayerMenuRow::Chart) {
        if (!multiplayer_leader_can_choose_chart(multiplayer_menu_)) {
            multiplayer_status_message_ =
                ui_text("Only the current leader chooses the BMS chart. Other players match it automatically by HASH.",
                        "현재 리더만 BMS를 선택합니다. 나머지는 HASH로 자동 선택합니다.");
            publish_snapshot();
            return;
        }
        if (!peer.remote_library_ready) {
            multiplayer_status_message_ =
                ui_text("Wait for song-library sync to finish.", "곡 목록 동기화가 끝날 때까지 기다리세요.");
            publish_snapshot();
            return;
        }
        if (multiplayer_common_chart_count_ == 0) {
            multiplayer_status_message_ =
                ui_text("No identical BMS charts exist across all connected players.",
                        "현재 접속자 전원이 가진 동일한 BMS가 없습니다.");
            publish_snapshot();
            return;
        }
        if (round_active) {
            multiplayer_status_message_ =
                ui_text("Wait for every player to leave the result before changing charts.",
                        "전원이 리절트에서 나온 뒤 차트를 변경할 수 있습니다.");
            publish_snapshot();
            return;
        }
        multiplayer_selecting_chart_ = true;
        rebuild_visible_song_list();
        song_select_view_ = SongSelectView::Songs;
        song_select_focus_ = SongSelectFocus::SongList;
        song_select_search_active_ = false;
        screen_ = Screen::SongSelect;
        publish_snapshot();
        return;
    }
    if (row == MultiplayerMenuRow::Ready) {
        if (round_active) {
            multiplayer_status_message_ = ui_text("Wait for every player to finish the current round.",
                                                  "현재 대전이 전원에게서 끝날 때까지 기다려주세요.");
        } else if (!multiplayer_ready_gate_open(multiplayer_menu_)) {
            multiplayer_status_message_ = ui_text("Connect and confirm a matching chart first.",
                                                  "먼저 연결하고 같은 차트인지 확인하세요.");
        } else if (!peer_session_.set_ready(!multiplayer_menu_.local_ready)) {
            multiplayer_status_message_ = ui_text("Ready state was rejected.", "준비 상태가 거부되었습니다.");
        } else {
            multiplayer_status_message_.clear();
        }
        publish_snapshot();
        return;
    }
    if (row == MultiplayerMenuRow::Start) {
        if (!multiplayer_leader_can_start(multiplayer_menu_) || multiplayer_chart_path_.empty()) {
            multiplayer_status_message_ = ui_text("Only the current leader can start after every player is ready.",
                                                  "전원이 준비된 뒤 현재 리더만 시작할 수 있습니다.");
            publish_snapshot();
            return;
        }
        if (!peer_session_.send_launch()) {
            multiplayer_status_message_ = ui_text("Start request was rejected.", "시작 요청이 거부되었습니다.");
            publish_snapshot();
            return;
        }
        multiplayer_status_message_ =
            ui_text("Start requested; waiting for room coordinator.",
                    "시작 요청을 보냈습니다. 방장 승인을 기다립니다.");
        publish_snapshot();
        return;
    }
    if (row == MultiplayerMenuRow::Chat) {
        if (peer.state != network::PeerSessionState::Connected) {
            multiplayer_status_message_ =
                ui_text("Connect to a room before using chat.",
                        "채팅을 사용하려면 먼저 방에 연결하세요.");
        } else {
            multiplayer_menu_.edit_field = MultiplayerEditField::Chat;
            multiplayer_status_message_.clear();
        }
        publish_snapshot();
        return;
    }
    if (row == MultiplayerMenuRow::Options) {
        open_multiplayer_options();
        return;
    }
    if (row == MultiplayerMenuRow::Back) {
        leave_multiplayer();
    }
}

void MenuApp::select_multiplayer_chart() {
    const network::PeerSessionSnapshot peer = peer_session_.snapshot();
    if (peer_round_ui_locked(peer) ||
        !multiplayer_leader_can_choose_chart(multiplayer_menu_)) {
        multiplayer_status_message_ =
            ui_text("Chart selection was canceled because only the current leader may choose.",
                    "현재 리더만 BMS를 선곡할 수 있어 곡 선택을 취소했습니다.");
        multiplayer_selecting_chart_ = false;
        rebuild_visible_song_list();
        screen_ = Screen::Multiplayer;
        publish_snapshot();
        return;
    }
    const std::string chart_path = selected_song_absolute_path();
    if (chart_path.empty()) return;

    const SongEntry* selected_entry =
        visible_song_entry(static_cast<std::size_t>(std::max(0, selected_song_)));
    if (!selected_entry || selected_entry->format != "bms") {
        multiplayer_status_message_ =
            ui_text("Multiplayer supports BMS charts only.",
                    "멀티플레이에서는 BMS 곡만 선택할 수 있습니다.");
        multiplayer_selecting_chart_ = false;
        rebuild_visible_song_list();
        screen_ = Screen::Multiplayer;
        publish_snapshot();
        return;
    }
    std::string error;
    const network::ChartFingerprint fingerprint = network::fingerprint_chart_file(chart_path, &error);
    if (!fingerprint.valid()) {
        multiplayer_status_message_ = error.empty()
                                          ? ui_text("Could not fingerprint the selected chart.",
                                                    "선택한 차트의 동일성 값을 만들지 못했습니다.")
                                          : error;
        multiplayer_selecting_chart_ = false;
        rebuild_visible_song_list();
        screen_ = Screen::Multiplayer;
        publish_snapshot();
        return;
    }

    std::string chart_title = selected_entry->title.empty()
                                  ? selected_entry->path
                                  : selected_entry->title;
    if (!peer_session_.set_local_chart(fingerprint, chart_title)) {
        multiplayer_status_message_ =
            ui_text("Chart selection expired. Wait for the current round to finish.",
                    "선곡 시간이 만료되었습니다. 현재 대전이 끝날 때까지 기다려주세요.");
        multiplayer_selecting_chart_ = false;
        rebuild_visible_song_list();
        screen_ = Screen::Multiplayer;
        publish_snapshot();
        return;
    }

    multiplayer_chart_path_ = chart_path;
    multiplayer_chart_fingerprint_ = fingerprint;
    multiplayer_chart_title_ = std::move(chart_title);
    multiplayer_menu_.local_chart_fingerprint = fingerprint.hash;
    multiplayer_menu_.local_chart_size = fingerprint.size;
    multiplayer_menu_.local_ready = false;
    multiplayer_status_message_.clear();
    multiplayer_selecting_chart_ = false;
    rebuild_visible_song_list();
    screen_ = Screen::Multiplayer;
    publish_snapshot();
}

void MenuApp::reset_multiplayer_chart_match_search() {
    multiplayer_chart_match_target_ = {};
    multiplayer_chart_match_index_revision_ = 0;
    multiplayer_chart_match_source_inputs_.clear();
    multiplayer_chart_match_sources_.clear();
    multiplayer_chart_match_source_cursor_ = 0;
    multiplayer_chart_match_candidates_.clear();
    multiplayer_chart_match_cursor_ = 0;
    multiplayer_chart_match_active_ = false;
}

void MenuApp::service_multiplayer_chart_match(const network::PeerSessionSnapshot& peer) {
    if (peer.local_is_leader ||
        peer.state != network::PeerSessionState::Connected ||
        !peer.selected_chart.fingerprint.valid() ||
        (last_game_was_multiplayer_ || peer_round_ui_locked(peer))) {
        return;
    }

    const network::ChartFingerprint target = peer.selected_chart.fingerprint;
    std::vector<std::string> source_inputs;
    source_inputs.reserve(config_.ui.recent_song_sources.size() + 1);
    source_inputs.push_back(songs_path_);
    for (const auto& source : config_.ui.recent_song_sources) {
        source_inputs.push_back(source);
    }
    const bool local_matches =
        !multiplayer_chart_path_.empty() &&
        multiplayer_chart_fingerprint_.hash == target.hash &&
        multiplayer_chart_fingerprint_.size == target.size;
    if (local_matches) {
        multiplayer_chart_match_target_ = target;
        multiplayer_chart_match_index_revision_ = song_index_revision_;
        multiplayer_chart_match_source_inputs_ = std::move(source_inputs);
        multiplayer_chart_match_sources_.clear();
        multiplayer_chart_match_source_cursor_ = multiplayer_chart_match_sources_.size();
        multiplayer_chart_match_candidates_.clear();
        multiplayer_chart_match_cursor_ = 0;
        multiplayer_chart_match_active_ = false;
        return;
    }

    const bool target_changed =
        multiplayer_chart_match_target_.hash != target.hash ||
        multiplayer_chart_match_target_.size != target.size;
    const bool index_changed =
        multiplayer_chart_match_index_revision_ != song_index_revision_;
    const bool sources_changed = multiplayer_chart_match_source_inputs_ != source_inputs;
    if (target_changed || index_changed || sources_changed) {
        if (peer.local_chart.fingerprint.valid()) {
            peer_session_.clear_local_chart();
        }
        multiplayer_chart_path_.clear();
        multiplayer_chart_fingerprint_ = {};
        multiplayer_chart_title_ = peer.selected_chart.name;
        multiplayer_menu_.local_chart_fingerprint = 0;
        multiplayer_menu_.local_chart_size = 0;

        multiplayer_chart_match_target_ = target;
        multiplayer_chart_match_index_revision_ = song_index_revision_;
        multiplayer_chart_match_source_inputs_ = std::move(source_inputs);
        multiplayer_chart_match_sources_ = multiplayer_loaded_song_sources(
            songs_path_, config_.ui.recent_song_sources);
        multiplayer_chart_match_source_cursor_ = 0;
        multiplayer_chart_match_candidates_.clear();
        multiplayer_chart_match_cursor_ = 0;
        multiplayer_chart_match_active_ = true;
        multiplayer_status_message_ =
            ui_text("Searching loaded song folders for the leader BMS HASH...",
                    "로드한 곡 폴더에서 리더 BMS HASH를 찾는 중...");
        publish_snapshot();
    }

    if (!multiplayer_chart_match_active_) {
        return;
    }

    // Keep memory bounded by materializing candidates for only one loaded
    // source at a time. Recent sources are read from existing profile caches;
    // this path never starts an index scan.
    if (multiplayer_chart_match_cursor_ >= multiplayer_chart_match_candidates_.size()) {
        multiplayer_chart_match_candidates_.clear();
        multiplayer_chart_match_cursor_ = 0;
        while (multiplayer_chart_match_source_cursor_ < multiplayer_chart_match_sources_.size()) {
            const std::string source =
                multiplayer_chart_match_sources_[multiplayer_chart_match_source_cursor_++];
            const std::string source_key =
                menu_songs::normalize_path_key(util::path_from_utf8_lossy(source));
            const std::string active_key =
                menu_songs::normalize_path_key(util::path_from_utf8_lossy(songs_path_));
            if (!source_key.empty() && source_key == active_key) {
                multiplayer_chart_match_candidates_ = build_multiplayer_chart_candidates(
                    indexed_songs_, source, peer.selected_chart.name);
            } else {
                SongIndexOptions index_options;
                index_options.difficulty_table_path = config_.ui.difficulty_table_path;
                index_options.calculate_difficulty = config_.mode.calculate_song_index_difficulty;
                index_options.profile =
                    (config::normalize_song_index_profile_token(config_.mode.song_index_profile) == "fast")
                        ? SongIndexProfile::Fast
                        : SongIndexProfile::Safe;
                auto loaded = load_multiplayer_chart_candidates_from_profile_cache(
                    profile_dir_, source, index_options, peer.selected_chart.name);
                if (!loaded.error.empty()) {
                    std::cerr << "[warn] Multiplayer chart cache skipped: "
                              << loaded.error << std::endl;
                }
                if (loaded.loaded_from_cache) {
                    multiplayer_chart_match_candidates_ = std::move(loaded.candidates);
                }
            }
            if (!multiplayer_chart_match_candidates_.empty()) {
                break;
            }
        }

        if (multiplayer_chart_match_candidates_.empty() &&
            multiplayer_chart_match_source_cursor_ >= multiplayer_chart_match_sources_.size()) {
            multiplayer_chart_match_active_ = false;
            multiplayer_status_message_ =
                ui_text("No identical chart was found in loaded song folders. Open or rescan that folder first.",
                        "로드한 곡 폴더에서 같은 차트를 찾지 못했습니다. 해당 폴더를 먼저 열거나 다시 스캔하세요.");
            publish_snapshot();
            return;
        }
    }

    constexpr std::size_t kMaxCandidatesPerTick = 64;
    constexpr auto kSearchBudget = std::chrono::milliseconds(3);
    const auto deadline = std::chrono::steady_clock::now() + kSearchBudget;
    std::size_t processed = 0;
    while (multiplayer_chart_match_cursor_ < multiplayer_chart_match_candidates_.size() &&
           processed < kMaxCandidatesPerTick &&
           (processed == 0 || std::chrono::steady_clock::now() < deadline)) {
        const MultiplayerChartSearchCandidate candidate =
            std::move(multiplayer_chart_match_candidates_[multiplayer_chart_match_cursor_++]);
        ++processed;
        const std::string chart_path = multiplayer_chart_path_for_source(
            candidate.indexed_path, candidate.source_root);
        if (chart_path.empty()) {
            continue;
        }

        std::error_code size_error;
        const auto file_size =
            std::filesystem::file_size(util::path_from_utf8_lossy(chart_path), size_error);
        if (size_error || file_size != target.size) {
            continue;
        }

        std::string fingerprint_error;
        const network::ChartFingerprint fingerprint =
            network::fingerprint_chart_file(chart_path, &fingerprint_error);
        if (fingerprint.hash != target.hash || fingerprint.size != target.size) {
            continue;
        }
        const std::string title = peer.selected_chart.name.empty()
                                      ? candidate.title
                                      : peer.selected_chart.name;
        if (!peer_session_.set_local_chart(fingerprint, title)) {
            multiplayer_status_message_ =
                ui_text("The matched BMS could not be announced to the room coordinator.",
                        "일치한 BMS를 방 코디네이터에 전달하지 못했습니다.");
            multiplayer_chart_match_active_ = false;
            publish_snapshot();
            return;
        }

        multiplayer_chart_path_ = chart_path;
        multiplayer_chart_title_ = title;
        multiplayer_chart_fingerprint_ = fingerprint;
        multiplayer_menu_.local_chart_fingerprint = fingerprint.hash;
        multiplayer_menu_.local_chart_size = fingerprint.size;
        multiplayer_menu_.local_ready = false;
        multiplayer_status_message_ =
            ui_text("Leader BMS matched automatically by HASH.",
                    "리더 BMS를 HASH로 자동 선택했습니다.");
        multiplayer_chart_match_candidates_.clear();
        multiplayer_chart_match_cursor_ = 0;
        multiplayer_chart_match_active_ = false;
        publish_snapshot();
        return;
    }

    if (multiplayer_chart_match_cursor_ >= multiplayer_chart_match_candidates_.size()) {
        multiplayer_chart_match_candidates_.clear();
        multiplayer_chart_match_cursor_ = 0;
        if (multiplayer_chart_match_source_cursor_ >= multiplayer_chart_match_sources_.size()) {
            multiplayer_chart_match_active_ = false;
            multiplayer_status_message_ =
                ui_text("No identical chart was found in loaded song folders. Open or rescan that folder first.",
                        "로드한 곡 폴더에서 같은 차트를 찾지 못했습니다. 해당 폴더를 먼저 열거나 다시 스캔하세요.");
            publish_snapshot();
        }
    }
}

void MenuApp::open_multiplayer_options() {
    const network::PeerSessionSnapshot peer = peer_session_.snapshot();
    if (last_game_was_multiplayer_ || peer_round_ui_locked(peer)) {
        multiplayer_status_message_ =
            ui_text("Options cannot be opened during an active match.",
                    "대전 중에는 옵션을 열 수 없습니다.");
        publish_snapshot();
        return;
    }
    if (peer.local_ready) {
        if (peer.state == network::PeerSessionState::Connected) {
            if (!peer_session_.set_ready(false)) {
                multiplayer_status_message_ =
                    ui_text("Could not clear Ready before opening Options.",
                            "옵션 진입 전 준비 상태를 해제하지 못했습니다.");
                publish_snapshot();
                return;
            }
        } else {
            peer_session_.reset_round();
        }
    }
    multiplayer_menu_.local_ready = false;
    submenu_return_screen_ = Screen::Multiplayer;
    screen_ = Screen::OptionsHub;
    options_cursor_ = 0;
    multiplayer_status_message_ =
        ui_text("Ready was cleared before opening Options.",
                "옵션 진입 전에 준비 상태를 해제했습니다.");
    publish_snapshot();
}

void MenuApp::service_multiplayer() {
    const network::PeerSessionSnapshot peer = peer_session_.snapshot();
    const bool peer_active = peer_session_is_active(peer.state);
    if (peer.role == network::PeerRole::Host && peer_active && peer.local_port != 0) {
        network::LanRoomAdvertisement advertisement;
        advertisement.host_name = profile_display_name();
        advertisement.tcp_port = peer.local_port;
        advertisement.player_count = static_cast<uint8_t>(
            std::min<std::size_t>(peer.participant_count, network::kPeerMaxPlayers));
        advertisement.max_players = network::kPeerMaxPlayers;
        advertisement.accepting_players =
            !peer_round_ui_locked(peer) &&
            peer.participant_count < network::kPeerMaxPlayers &&
            (peer.state == network::PeerSessionState::Listening ||
             peer.state == network::PeerSessionState::Connected);
        lan_discovery_.advertise(std::move(advertisement));
    } else if (screen_ == Screen::Multiplayer && !peer_active) {
        lan_discovery_.start_browsing();
    } else {
        lan_discovery_.stop();
    }

    const network::LanDiscoverySnapshot lan_snapshot = lan_discovery_.snapshot();
    if (lan_snapshot.revision != multiplayer_lan_discovery_revision_) {
        std::string selected_key;
        if (!multiplayer_lan_rooms_.empty()) {
            multiplayer_lan_room_index_ =
                std::min(multiplayer_lan_room_index_, multiplayer_lan_rooms_.size() - 1u);
            const auto& selected = multiplayer_lan_rooms_[multiplayer_lan_room_index_];
            selected_key = selected.address + ":" + std::to_string(selected.tcp_port);
        }
        multiplayer_lan_rooms_ = lan_snapshot.rooms;
        multiplayer_lan_room_index_ = 0;
        if (!selected_key.empty()) {
            const auto selected = std::find_if(
                multiplayer_lan_rooms_.begin(), multiplayer_lan_rooms_.end(),
                [&selected_key](const network::LanDiscoveredRoom& room) {
                    return room.address + ":" + std::to_string(room.tcp_port) == selected_key;
                });
            if (selected != multiplayer_lan_rooms_.end()) {
                multiplayer_lan_room_index_ = static_cast<std::size_t>(
                    std::distance(multiplayer_lan_rooms_.begin(), selected));
            }
        }
        multiplayer_lan_discovery_revision_ = lan_snapshot.revision;
        if (screen_ == Screen::Multiplayer) publish_snapshot();
    }

    multiplayer_menu_.connected = peer.state == network::PeerSessionState::Connected;
    multiplayer_menu_.local_ready = peer.local_ready;
    multiplayer_menu_.peer_ready = peer.remote_ready;
    multiplayer_menu_.local_player_id = peer.local_player_id;
    multiplayer_menu_.leader_player_id = peer.leader_player_id;
    multiplayer_menu_.player_count = static_cast<uint8_t>(
        std::min<std::size_t>(peer.participant_count, network::kPeerMaxPlayers));
    multiplayer_menu_.local_is_leader = peer.local_is_leader;
    multiplayer_menu_.local_chart_fingerprint = peer.local_chart.fingerprint.hash;
    multiplayer_menu_.local_chart_size = peer.local_chart.fingerprint.size;
    multiplayer_menu_.peer_chart_fingerprint = peer.selected_chart.fingerprint.hash;
    multiplayer_menu_.peer_chart_size = peer.selected_chart.fingerprint.size;
    if (peer.role == network::PeerRole::Host) multiplayer_menu_.role = MultiplayerRole::Host;
    if (peer.role == network::PeerRole::Joiner) multiplayer_menu_.role = MultiplayerRole::Join;

    const bool local_library_changed =
        peer_session_is_active(peer.state) &&
        song_index_revision_ != multiplayer_local_library_index_revision_;
    if (local_library_changed) {
        peer_session_.set_local_library(
            build_multiplayer_chart_sha256_inventory(indexed_songs_));
        multiplayer_local_library_index_revision_ = song_index_revision_;
    }
    const bool library_state_changed =
        local_library_changed ||
        peer.remote_library_ready != multiplayer_remote_library_ready_ ||
        peer.remote_library_revision != multiplayer_remote_library_revision_;
    if (library_state_changed) {
        multiplayer_remote_library_ready_ = peer.remote_library_ready;
        multiplayer_remote_library_revision_ = peer.remote_library_revision;
        multiplayer_common_chart_count_ =
            peer.remote_library_ready && peer.remote_library_sha256
                ? count_shared_multiplayer_charts(indexed_songs_, *peer.remote_library_sha256)
                : 0;
        if (multiplayer_selecting_chart_ && screen_ == Screen::SongSelect) {
            rebuild_visible_song_list();
        }
    }

    service_multiplayer_chart_match(peer);

    if (multiplayer_waiting_for_result_exit_) {
        if (peer.state != network::PeerSessionState::Connected) {
            multiplayer_waiting_for_result_exit_ = false;
            multiplayer_status_message_ =
                ui_text("Peer disconnected. Reconnect for another match.",
                        "상대 연결이 끊겼습니다. 다시 대전하려면 재연결하세요.");
        } else if (!peer.round_active) {
            multiplayer_waiting_for_result_exit_ = false;
            multiplayer_status_message_ =
                ui_text("Round complete. Ready again for a rematch.",
                        "대전이 끝났습니다. 다시 준비하면 재대전할 수 있습니다.");
        }
    }

    if (last_game_was_multiplayer_ && screen_ == Screen::Multiplayer) {
        const bool peer_finished =
            peer.all_remote_finished;
        const bool disconnected = peer.state != network::PeerSessionState::Connected;
        if (peer_finished || disconnected) {
            peer_session_.reset_round();
            const bool waiting_for_peer_exit =
                !disconnected && peer_session_.snapshot().round_active;
            last_game_was_multiplayer_ = false;
            multiplayer_waiting_for_result_exit_ = waiting_for_peer_exit;
            multiplayer_status_message_ =
                disconnected
                    ? ui_text("Peer disconnected. Reconnect for another match.",
                              "상대 연결이 끊겼습니다. 다시 대전하려면 재연결하세요.")
                    : waiting_for_peer_exit
                          ? ui_text("Waiting for the peer to leave the result screen.",
                                    "상대가 리절트 화면에서 나가기를 기다리는 중입니다.")
                    : ui_text("Round complete. Ready again for a rematch.",
                              "대전이 끝났습니다. 다시 준비하면 재대전할 수 있습니다.");
        }
    }

    // A Launch can be queued immediately before either socket closes. Gate at
    // the UI boundary as well as inside PeerSession so a stale launch cannot
    // turn a disconnected lobby into a local gameplay session.
    if (peer.state == network::PeerSessionState::Connected &&
        peer.round_active) {
        while (const std::optional<uint64_t> launch = peer_session_.poll_launch()) {
            if (*launch == multiplayer_menu_.local_chart_fingerprint &&
                !multiplayer_chart_path_.empty()) {
                multiplayer_match_active_ = true;
                last_game_was_multiplayer_ = true;
                multiplayer_waiting_for_result_exit_ = false;
                launch_gameplay(multiplayer_chart_path_, {}, GameplayLaunchKind::PeerBattle);
                return;
            }
            multiplayer_status_message_ = ui_text("Peer requested an unknown chart; start canceled.",
                                              "상대가 알 수 없는 차트를 요청해 시작을 취소했습니다.");
        }
    }

    if (peer.revision != multiplayer_last_revision_) {
        multiplayer_last_revision_ = peer.revision;
        if (screen_ == Screen::Multiplayer || screen_ == Screen::Result ||
            (screen_ == Screen::SongSelect && multiplayer_selecting_chart_)) {
            publish_snapshot();
        }
    }
}

bool MenuApp::coordinate_multiplayer_start() {
    using namespace std::chrono_literals;
    if (!peer_session_.mark_loaded()) {
        multiplayer_status_message_ = ui_text("Could not report local chart readiness.",
                                              "로컬 차트 준비 상태를 전송하지 못했습니다.");
        return false;
    }

    update_gameplay_loading_state(100, ui_text("Waiting for all player chart loads", "상대 차트 로딩 대기 중"));
    const auto timeout_at = std::chrono::steady_clock::now() + 60s;
#ifdef _WIN32
    bool escape_was_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
#endif
    auto canceled_or_disconnected = [this
#ifdef _WIN32
                                     , &escape_was_down
#endif
    ]() {
#ifdef _WIN32
        const bool escape_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        const bool fresh_escape = escape_down && !escape_was_down;
        escape_was_down = escape_down;
        if (fresh_escape && current_process_is_foreground()) return true;
#endif
        const network::PeerSessionSnapshot peer = peer_session_.snapshot();
        return peer.state != network::PeerSessionState::Connected || !peer.round_active;
    };

    const network::PeerSessionSnapshot room = peer_session_.snapshot();
    uint32_t delay_ms = 0;
    if (room.local_is_leader) {
        bool all_loaded = false;
        while (std::chrono::steady_clock::now() < timeout_at) {
            if (peer_session_.wait_for_peer_loaded(50ms)) {
                all_loaded = true;
                break;
            }
            if (canceled_or_disconnected()) break;
        }
        if (!all_loaded) {
            multiplayer_status_message_ =
                ui_text("Player load wait timed out or was canceled.",
                        "참가자 로딩 대기가 시간 초과되었거나 취소되었습니다.");
            return false;
        }
        constexpr uint32_t kRoomBeginDelayMs = 1500;
        if (!peer_session_.send_begin(kRoomBeginDelayMs)) {
            multiplayer_status_message_ =
                ui_text("Could not send synchronized room start.",
                        "동기화된 방 시작 신호를 보내지 못했습니다.");
            return false;
        }
        delay_ms = kRoomBeginDelayMs;
    } else if (room.local_player_id != 0) {
        bool begin_received = false;
        while (std::chrono::steady_clock::now() < timeout_at) {
            if (peer_session_.wait_for_begin(50ms, delay_ms)) {
                begin_received = true;
                break;
            }
            if (canceled_or_disconnected()) break;
        }
        if (!begin_received) {
            multiplayer_status_message_ =
                ui_text("Leader start wait timed out or was canceled.",
                        "리더 시작 대기가 시간 초과되었거나 취소되었습니다.");
            return false;
        }
    } else {
        multiplayer_status_message_ =
            ui_text("Multiplayer player id is unavailable.",
                    "멀티플레이 참가자 ID가 없습니다.");
        return false;
    }
    update_gameplay_loading_state(100, ui_text("Synchronized countdown starting", "동기 카운트다운 시작 중"));
    const auto begin_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
    while (std::chrono::steady_clock::now() < begin_at) {
        if (canceled_or_disconnected()) {
            multiplayer_status_message_ = ui_text("Match start was canceled.", "대전 시작이 취소되었습니다.");
            return false;
        }
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

bool MenuApp::wait_for_multiplayer_result() {
    using namespace std::chrono_literals;

    update_gameplay_loading_state(
        100,
        ui_text("Local result ready - waiting for all player results",
                "내 결과 준비 완료 - 상대 결과 대기 중"));
    const auto timeout_at = std::chrono::steady_clock::now() + std::chrono::minutes(15);
    auto next_status_update = std::chrono::steady_clock::now();
#ifdef _WIN32
    bool escape_was_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
#endif

    while (std::chrono::steady_clock::now() < timeout_at) {
        if (exit_requested_.load(std::memory_order_acquire) ||
            menu_window_.should_close()) {
            exit_requested_.store(true, std::memory_order_release);
            peer_session_.disconnect("Window closed while waiting for opponent result");
            multiplayer_status_message_ =
                ui_text("Opponent result wait ended because the window closed.",
                        "창이 닫혀 상대 결과 대기를 종료했습니다.");
            return false;
        }
        const network::PeerSessionSnapshot peer = peer_session_.snapshot();
        if (peer.all_remote_finished) {
            multiplayer_status_message_ =
                ui_text("All player results received.", "전원 결과를 모두 받았습니다.");
            return true;
        }
        if (peer.state != network::PeerSessionState::Connected) {
            multiplayer_status_message_ =
                ui_text("Opponent disconnected before the final result arrived.",
                        "최종 결과를 받기 전에 상대 연결이 끊겼습니다.");
            return false;
        }
#ifdef _WIN32
        const bool escape_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        const bool fresh_escape = escape_down && !escape_was_down;
        escape_was_down = escape_down;
        if (fresh_escape && current_process_is_foreground()) {
            peer_session_.disconnect("Canceled opponent result wait");
            multiplayer_status_message_ =
                ui_text("Opponent result wait was canceled.",
                        "상대 결과 대기를 취소했습니다.");
            return false;
        }
#endif
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_status_update) {
            std::string stage =
                ui_text("Waiting for all player results", "상대 결과 대기 중");
            if (peer.has_remote_score) {
                stage += " / " + ui_text("Claimed Score ", "미검증 점수 ") +
                         std::to_string(peer.latest_remote_score.score);
            }
            update_gameplay_loading_state(100, stage);
            next_status_update = now + 250ms;
        }
        std::this_thread::sleep_for(20ms);
    }

    peer_session_.disconnect("Opponent result wait timed out");
    multiplayer_status_message_ =
        ui_text("Opponent result wait timed out.",
                "상대 결과 대기 시간이 초과되었습니다.");
    return false;
}

void MenuApp::reset_multiplayer_for_single_player() {
    lan_discovery_.stop();
    peer_session_.disconnect("Switching to single player");
    peer_session_.clear_local_chart();
    reset_multiplayer_menu_session(multiplayer_menu_);
    multiplayer_chart_path_.clear();
    multiplayer_chart_title_.clear();
    multiplayer_chart_fingerprint_ = {};
    multiplayer_status_message_.clear();
    reset_multiplayer_chart_match_search();
    multiplayer_selecting_chart_ = false;
    multiplayer_match_active_ = false;
    last_game_was_multiplayer_ = false;
    multiplayer_waiting_for_result_exit_ = false;
    multiplayer_last_revision_ = 0;
    multiplayer_local_library_index_revision_ = 0;
    multiplayer_remote_library_revision_ = 0;
    multiplayer_remote_library_ready_ = false;
    multiplayer_common_chart_count_ = 0;
    multiplayer_lan_rooms_.clear();
    multiplayer_lan_room_index_ = 0;
    multiplayer_lan_discovery_revision_ = lan_discovery_.snapshot().revision;
}

void MenuApp::leave_multiplayer() {
    reset_multiplayer_for_single_player();
    submenu_return_screen_ = Screen::Title;
    options_cursor_ = 0;
    song_select_view_ = SongSelectView::Songs;
    song_select_focus_ = SongSelectFocus::SongList;
    screen_ = Screen::Title;
    publish_snapshot();
}

}  // namespace tenriff::app
