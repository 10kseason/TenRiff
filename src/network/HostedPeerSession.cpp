#include "network/HostedPeerSession.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <random>
#include <thread>

#include "config/SimpleJson.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#endif

namespace tenriff::network {
namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
using Json = config::JsonValue;
using Object = config::JsonObject;
constexpr std::size_t kMaxResponse = 64 * 1024;
constexpr double kMaxExactInteger = 9007199254740991.0;

void erase_secret(std::string& value) {
    // Volatile stores keep this best-effort clearing from being optimized out.
    volatile char* bytes = value.empty() ? nullptr : value.data();
    for (std::size_t i = 0; i < value.size(); ++i) bytes[i] = 0;
    value.clear();
}

bool token_valid(std::string_view value, std::string_view prefix) {
    return value.size() == prefix.size() + 64 && value.substr(0, prefix.size()) == prefix &&
        std::all_of(value.begin() + static_cast<std::ptrdiff_t>(prefix.size()), value.end(),
                    [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

bool room_id_valid(std::string_view value) {
    if (value.size() != 36) return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (value[i] != '-') return false; }
        else if (!((value[i] >= '0' && value[i] <= '9') ||
                   (value[i] >= 'a' && value[i] <= 'f'))) return false;
    }
    return true;
}

std::string request_id() {
    std::array<unsigned char, 16> bytes{};
#ifdef _WIN32
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return {};
#else
    try { std::random_device random; for (auto& byte : bytes) byte = static_cast<unsigned char>(random()); }
    catch (...) { return {}; }
#endif
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) result += '-';
        result += digits[bytes[i] >> 4]; result += digits[bytes[i] & 15];
    }
    return result;
}

const Json& field(const Json& value, const char* key) {
    static const Json empty;
    const auto* object = value.as_object();
    if (!object) return empty;
    const auto found = object->find(key);
    return found == object->end() ? empty : found->second;
}

bool integer(const Json& value, uint64_t min, uint64_t max, uint64_t& out) {
    if (!value.is_number()) return false;
    const double number = value.as_number();
    if (!std::isfinite(number) || number < static_cast<double>(min) ||
        number > static_cast<double>(max) || number > kMaxExactInteger || std::floor(number) != number) return false;
    out = static_cast<uint64_t>(number); return true;
}
bool number(const Json& value, const char* key, uint64_t min, uint64_t max, uint64_t& out) {
    return integer(field(value, key), min, max, out);
}
bool decimal(const Json& value, uint64_t& out) {
    if (!value.is_string()) return false;
    const auto text = value.as_string();
    if (text.empty() || text.size() > 20 || (text.size() > 1 && text.front() == '0')) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}
bool boolean(const Json& value, const char* key, bool& out) {
    const auto& f = field(value, key);
    if (!f.is_bool()) return false;
    out = f.as_bool(); return true;
}
bool text_field(const Json& value, const char* key, std::size_t limit, std::string& out) {
    const auto& f = field(value, key);
    if (!f.is_string()) return false;
    out = f.as_string();
    return out.size() <= limit && out.find('\0') == std::string::npos;
}
bool read_chart(const Json& value, PeerChartInfo& chart) {
    chart = {};
    if (value.is_null()) return true;
    return value.is_object() && decimal(field(value, "hash"), chart.fingerprint.hash) &&
        chart.fingerprint.hash != 0 && decimal(field(value, "size"), chart.fingerprint.size) &&
        text_field(value, "name", 1024, chart.name);
}
Json chart_json(const ChartFingerprint& fp, std::string name) {
    return Json(Object{{"hash", Json(std::to_string(fp.hash))}, {"size", Json(std::to_string(fp.size))},
                       {"name", Json(std::move(name))}});
}
Json score_json(const PeerScore& score) {
    return Json(Object{{"score", Json(static_cast<double>(score.score))},
        {"current_sample", Json(static_cast<double>(score.current_sample))},
        {"combo", Json(static_cast<double>(score.combo))}, {"max_combo", Json(static_cast<double>(score.max_combo))},
        {"perfect", Json(static_cast<double>(score.perfect))}, {"great", Json(static_cast<double>(score.great))},
        {"good", Json(static_cast<double>(score.good))}, {"bad", Json(static_cast<double>(score.bad))},
        {"poor", Json(static_cast<double>(score.poor))}, {"gauge_milli", Json(static_cast<double>(score.gauge_milli))},
        {"finished", Json(score.finished)}, {"game_over", Json(score.game_over)}, {"aborted", Json(score.aborted)}});
}
bool ex_protocol(const Json& value) {
    uint64_t protocol = 0;
    return number(value, "protocol", 2, 2, protocol) &&
        field(value, "score_version").is_string() && field(value, "score_version").as_string() == "ex-v1";
}
bool hosted_ex_score_is_sane(const PeerScore& score) {
    // Reuse the legacy count/sample/gauge bounds without changing TCP v9's
    // normalized-score contract. Hosted v2 derives its score only from counts.
    auto bounds = score;
    bounds.score = 0;
    return peer_score_claim_is_sane(bounds) &&
        score.score == static_cast<int64_t>(score.perfect) * 2 + score.great;
}
bool read_score(const Json& value, PeerScore& score) {
    uint64_t v = 0;
    if (!value.is_object() || !number(value, "score", 0, 20'000'000, v)) return false;
    score.score = static_cast<int64_t>(v);
    const auto& sample = field(value, "current_sample");
    const double sample_number = sample.as_number();
    if (!sample.is_number() || !std::isfinite(sample_number) || std::floor(sample_number) != sample_number ||
        sample_number < -10000000000000.0 || sample_number > 10000000000000.0) return false;
    score.current_sample = static_cast<int64_t>(sample_number);
    const std::pair<const char*, int*> counts[] = {{"combo", &score.combo}, {"max_combo", &score.max_combo},
        {"perfect", &score.perfect}, {"great", &score.great}, {"good", &score.good},
        {"bad", &score.bad}, {"poor", &score.poor}, {"gauge_milli", &score.gauge_milli}};
    for (const auto& item : counts) {
        if (!number(value, item.first, 0, 10'000'000, v)) return false;
        *item.second = static_cast<int>(v);
    }
    return boolean(value, "finished", score.finished) && boolean(value, "game_over", score.game_over) &&
        boolean(value, "aborted", score.aborted) && hosted_ex_score_is_sane(score);
}

#ifdef _WIN32
struct InternetHandle {
    HINTERNET value = nullptr;
    explicit InternetHandle(HINTERNET handle) : value(handle) {}
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
    InternetHandle(const InternetHandle&) = delete;
};
#endif

bool production_transport(std::string_view method, std::string_view path,
                          std::string_view bearer, std::string_view body, HostedHttpResponse& response) {
    response = {};
    if ((method != "POST" && method != "GET") || path.size() > 128 ||
        path.substr(0, 5) != "/api/" || body.size() > 16 * 1024 ||
        !(token_valid(bearer, "tr_") || token_valid(bearer, "mp_"))) return false;
#ifdef _WIN32
    InternetHandle session(WinHttpOpen(L"TenRiff/hosted-v2-ex", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                       WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value || !WinHttpSetTimeouts(session.value, 2000, 2000, 2000, 2000)) return false;
    InternetHandle connection(WinHttpConnect(session.value,
        L"tenriff-multiplayer.lastestarcorp.chatgpt.site", INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection.value) return false;
    const std::wstring wide_method(method.begin(), method.end()), wide_path(path.begin(), path.end());
    InternetHandle request(WinHttpOpenRequest(connection.value, wide_method.c_str(), wide_path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request.value) return false;
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy))) return false;
    std::wstring headers = L"Accept: application/json\r\nContent-Type: application/json\r\nAuthorization: Bearer ";
    headers.append(bearer.begin(), bearer.end()); headers += L"\r\n";
    const bool sent = WinHttpSendRequest(request.value, headers.c_str(), static_cast<DWORD>(headers.size()),
        body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
        static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) != FALSE;
    SecureZeroMemory(headers.data(), headers.size() * sizeof(wchar_t));
    if (!sent || !WinHttpReceiveResponse(request.value, nullptr)) return false;
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) return false;
    response.status = status;
    const auto deadline = Clock::now() + 5s;
    for (;;) {
        DWORD available = 0;
        if (Clock::now() >= deadline || !WinHttpQueryDataAvailable(request.value, &available)) return false;
        if (available == 0) return true;
        if (available > kMaxResponse - response.body.size()) return false;
        const auto offset = response.body.size();
        response.body.resize(offset + available);
        DWORD received = 0;
        if (!WinHttpReadData(request.value, response.body.data() + offset, available, &received)) return false;
        response.body.resize(offset + received);
        if (received == 0) return true;
    }
#else
    return false;
#endif
}
}  // namespace

struct HostedPeerSession::Impl {
    explicit Impl(HostedHttpTransport sender) : transport(sender ? std::move(sender) : production_transport) {}
    HostedHttpTransport transport;
    std::mutex lifecycle;
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    bool stopping = false;
    bool running = false;
    std::string upload_key;
    PeerSessionSnapshot current;
    std::vector<HostedRoomInfo> available_rooms;
    std::string room_id;
    uint64_t seen_launch = 0;
    std::optional<uint64_t> pending_launch;
    std::optional<Clock::time_point> begin_at;
    bool begin_consumed = false;
    bool final_queued = false;
    bool local_chart_invalidated = false;
    std::optional<ChartFingerprint> pending_chart;
    bool reset_queued = false;
    struct Command { std::string action, path, id; Object payload; };
    std::deque<Command> commands;
    std::optional<PeerScore> pending_score;

    void touch() { ++current.revision; wake.notify_all(); }
    void fail(std::string message) {
        std::lock_guard lock(mutex);
        if (stopping) return; // Intentional revocation wins over a late HTTP/parse failure.
        current.state = PeerSessionState::Failed;
        current.status_detail = std::move(message);
        current.round_active = false;
        current.can_start = false;
        pending_launch.reset(); begin_at.reset(); commands.clear(); pending_score.reset();
        touch();
    }
    bool enqueue_locked(std::string action, Object payload = {}, std::string path = {}) {
        if (!running || stopping || commands.size() >= 64) return false;
        auto id = request_id();
        if (id.empty()) return false;
        payload["protocol"] = Json(2.0);
        payload["score_version"] = Json("ex-v1");
        payload["request_id"] = Json(id);
        if (action != "create") {
            payload["action"] = Json(action);
            payload["nonce"] = Json(std::to_string(current.result_round_nonce));
            payload["rate_revision"] = Json(static_cast<double>(current.rate_revision));
        }
        commands.push_back({std::move(action), std::move(path), std::move(id), std::move(payload)});
        wake.notify_all(); return true;
    }
    bool active_locked() const { return running && !stopping && current.state == PeerSessionState::Connected; }

    bool request(std::string_view method, std::string_view path, std::string_view token,
                 std::string_view body, Json& result, unsigned& status,
                 Clock::time_point& before, Clock::time_point& after) {
        HostedHttpResponse response;
        before = Clock::now();
        bool success = false;
        try { success = transport(method, path, token, body, response); } catch (...) { success = false; }
        after = Clock::now(); status = response.status;
        if (!success || response.body.size() > kMaxResponse) return false;
        auto parsed = config::parse_json(response.body);
        if (!parsed.success() || !parsed.root->is_object()) return false;
        result = std::move(*parsed.root);
        return true;
    }

    static int64_t current_rtt_ms(Clock::time_point before, Clock::time_point after) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count();
    }

    bool parse_participants(const Json& value, std::vector<PeerParticipantSnapshot>& target,
                            uint8_t local_id, uint8_t leader_id) {
        const auto* array = value.as_array();
        if (!array || array->size() > 8) return false;
        std::array<bool, 9> ids{};
        for (const auto& p : *array) {
            PeerParticipantSnapshot item;
            uint64_t id = 0;
            if (!p.is_object() || !number(p, "player_id", 1, 8, id) || ids[id] ||
                !text_field(p, "name", 256, item.name) || item.name.empty() ||
                !boolean(p, "ready", item.ready) || !boolean(p, "loaded", item.loaded) ||
                !boolean(p, "round_reset", item.round_reset) || !read_chart(field(p, "chart"), item.chart)) return false;
            ids[id] = true; item.player_id = static_cast<uint8_t>(id);
            item.local = id == local_id; item.leader = id == leader_id;
            const auto& score = field(p, "score");
            item.has_score = !score.is_null();
            if (item.has_score && !read_score(score, item.latest_score)) return false;
            target.push_back(std::move(item));
        }
        return true;
    }

    bool accept_room(const Json& envelope, Clock::time_point before, Clock::time_point after) {
        uint64_t local = 0, leader = 0, server_ms = 0, received_ms = 0, version = 0, rate = 0, rate_revision = 0;
        const auto& room = field(envelope, "room");
        PeerSessionSnapshot next;
        std::string id, phase;
        uint64_t nonce = 0, start_ms = 0;
        if (!ex_protocol(envelope) ||
            !number(envelope, "server_time_ms", 1, 9007199254740991ULL, server_ms) ||
            !number(envelope, "server_received_ms", 1, 9007199254740991ULL, received_ms) ||
            received_ms > server_ms ||
            !number(envelope, "local_player_id", 1, 8, local) || !room.is_object() ||
            !text_field(room, "id", 36, id) || !room_id_valid(id) ||
            !number(room, "revision", 0, 9007199254740991ULL, version) ||
            !number(room, "leader_id", 1, 8, leader) ||
            !text_field(room, "phase", 16, phase) ||
            (phase != "lobby" && phase != "loading" && phase != "playing" && phase != "results") ||
            !decimal(field(room, "nonce"), nonce) || nonce > 9007199254740991ULL ||
            !number(room, "rate_milli", 500, 2000, rate) || !peer_rate_milli_is_valid(static_cast<uint32_t>(rate)) ||
            !number(room, "rate_revision", 1, 9007199254740991ULL, rate_revision) ||
            !number(room, "begin_at_ms", 0, 9007199254740991ULL, start_ms) ||
            !read_chart(field(room, "selected_chart"), next.selected_chart) ||
            !parse_participants(field(room, "participants"), next.participants,
                               static_cast<uint8_t>(local), static_cast<uint8_t>(leader)) ||
            !parse_participants(field(room, "round_participants"), next.round_participants,
                               static_cast<uint8_t>(local), static_cast<uint8_t>(leader))) return false;
        bool have_local = false, have_leader = false;
        next.remote_ready = true; next.remote_loaded = true; next.remote_round_reset = true;
        for (const auto& p : next.participants) {
            have_leader |= p.leader;
            if (p.local) {
                have_local = true; next.local_chart = p.chart; next.local_ready = p.ready;
                next.local_loaded = p.loaded; next.local_round_reset = p.round_reset;
            } else {
                next.remote_ready &= p.ready; next.remote_loaded &= p.loaded; next.remote_round_reset &= p.round_reset;
            }
        }
        if (!have_local || !have_leader || (phase != "lobby" && nonce == 0)) return false;
        const auto* chats = field(room, "chat").as_array();
        if (!chats || chats->size() > 32) return false;
        for (const auto& chat : *chats) {
            uint64_t player = 0; PeerChatEntry entry;
            if (!number(chat, "player_id", 1, 8, player) || !text_field(chat, "text", 256, entry.text)) return false;
            entry.player_id = static_cast<uint8_t>(player); next.chat_messages.push_back(std::move(entry));
        }
        next.local_player_id = static_cast<uint8_t>(local); next.leader_player_id = static_cast<uint8_t>(leader);
        next.local_is_leader = local == leader;
        next.role = next.local_is_leader ? PeerRole::Host : PeerRole::Joiner;
        next.state = PeerSessionState::Connected;
        next.participant_count = next.participants.size();
        next.rate_milli = static_cast<uint32_t>(rate); next.round_rate_milli = next.rate_milli;
        next.rate_revision = rate_revision; next.result_round_nonce = nonce;
        next.round_active = phase != "lobby";
        next.remote_chart = next.selected_chart;
        next.remote_library_ready = true; // Selection uses local matching, never a fabricated common list.
        next.can_start = phase == "lobby" && next.local_is_leader && next.participant_count >= 2 &&
                         next.local_ready && next.remote_ready && next.selected_chart.fingerprint.valid();
        next.estimated_rtt_ms = static_cast<uint32_t>(std::min<int64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count(), 60000));
        if (server_ms - received_ms > static_cast<uint64_t>(current_rtt_ms(before, after)) + 5) return false;
        next.all_remote_finished = false;
        bool remote_seen = false, remote_finished = true;
        const auto& result_players = next.round_participants.empty() ? next.participants : next.round_participants;
        for (const auto& p : result_players) if (!p.local) {
            remote_seen = true; remote_finished &= p.has_score && p.latest_score.finished;
            if (p.has_score && (!next.has_remote_score || p.latest_score.score > next.latest_remote_score.score)) {
                next.has_remote_score = true; next.latest_remote_score = p.latest_score; next.peer_name = p.name;
            }
        }
        next.all_remote_finished = remote_seen && remote_finished;
        if (next.participant_count < 2) next.remote_ready = next.remote_loaded = false;
        next.status_detail = "Connected to the official multiplayer room.";

        std::lock_guard lock(mutex);
        if (stopping) return true;
        if (!room_id.empty() && room_id != id) return false;
        room_id = id;
        next.rate_change_pending = current.rate_change_pending && next.rate_revision <= current.rate_revision;
        if (pending_chart && pending_chart->hash == next.local_chart.fingerprint.hash &&
            pending_chart->size == next.local_chart.fingerprint.size) pending_chart.reset();
        // A server snapshot cannot restore a chart acknowledgment that this PC
        // invalidated after a file disappeared or local matching was canceled.
        if (local_chart_invalidated) {
            next.local_chart = {}; next.local_ready = false; next.can_start = false;
            for (auto& p : next.participants) if (p.local) { p.chart = {}; p.ready = false; }
        }
        next.revision = current.revision + 1;
        current = std::move(next);
        if (phase == "loading" && nonce != seen_launch) {
            seen_launch = nonce; pending_launch = current.selected_chart.fingerprint.hash; begin_at.reset(); begin_consumed = false; final_queued = false;
        }
        if (phase == "lobby") {
            pending_launch.reset(); begin_at.reset(); begin_consumed = false; final_queued = false;
            reset_queued = false;
        } else if (phase == "playing" && !begin_consumed && !begin_at) {
            // Match request midpoints on both clocks. Subtracting server-side
            // processing time prevents unequal D1 latency from shifting starts.
            // Local scheduling uses only steady_clock, never local wall time.
            if (current.estimated_rtt_ms > 1500 || start_ms <= server_ms || start_ms - server_ms > 10000) return false;
            const double server_midpoint = static_cast<double>(received_ms) +
                static_cast<double>(server_ms - received_ms) / 2.0;
            const auto remaining = std::chrono::duration<double, std::milli>(static_cast<double>(start_ms) - server_midpoint);
            const auto point = before + (after - before) / 2 + std::chrono::duration_cast<Clock::duration>(remaining);
            if (point <= after) return false;
            begin_at = point;
        }
        wake.notify_all(); return true;
    }

    bool accept_rooms(const Json& envelope) {
        const auto* array = field(envelope, "rooms").as_array();
        if (!ex_protocol(envelope) || !array || array->size() > 64) return false;
        std::vector<HostedRoomInfo> next;
        for (const auto& room : *array) {
            HostedRoomInfo item; uint64_t count = 0, max = 0;
            if (!text_field(room, "id", 36, item.id) || !room_id_valid(item.id) ||
                !text_field(room, "name", 256, item.name) ||
                !number(room, "player_count", 0, 8, count) || !number(room, "max_players", 8, 8, max) ||
                !boolean(room, "accepting_players", item.accepting_players)) return false;
            item.player_count = static_cast<std::size_t>(count); item.max_players = static_cast<std::size_t>(max);
            next.push_back(std::move(item));
        }
        std::lock_guard lock(mutex);
        if (stopping) return true;
        available_rooms = std::move(next);
        if (current.state != PeerSessionState::Starting && current.state != PeerSessionState::Connecting) {
            current.state = PeerSessionState::Idle;
            current.status_detail = "Official rooms are ready.";
        }
        touch(); return true;
    }

    void run() {
        std::string session_token;
        auto renew_at = Clock::time_point{}, session_deadline = Clock::time_point{};
        auto next_poll = Clock::now(), next_score = Clock::now();
        bool fatal = false;
        for (;;) {
            { std::lock_guard lock(mutex); if (stopping) break; }
            auto now = Clock::now();
            if (session_token.empty() || now >= renew_at) {
                std::string key;
                { std::lock_guard lock(mutex); key = upload_key; }
                Json reply; unsigned status = 0; Clock::time_point before, after;
                const bool ok = request("POST", "/api/session", key, "{\"protocol\":2,\"score_version\":\"ex-v1\"}", reply, status, before, after);
                erase_secret(key);
                { std::lock_guard lock(mutex); if (stopping) break; }
                uint64_t expires = 0, server = 0; std::string token;
                if (!ok || status != 200 || !ex_protocol(reply) ||
                    !text_field(reply, "session_token", 67, token) || !token_valid(token, "mp_") ||
                    !number(reply, "expires_at_ms", 1, 9007199254740991ULL, expires) ||
                    !number(reply, "server_time_ms", 1, 9007199254740991ULL, server) ||
                    expires <= server + 1000 || expires - server > 30000) {
                    erase_secret(token); fail("Leaderboard connection could not be verified. Reconnect with F10."); fatal = true; break;
                }
                erase_secret(session_token); session_token = std::move(token);
                session_deadline = before + std::chrono::milliseconds(expires - server);
                renew_at = std::min(after + 20s, session_deadline - 1s);
            }
            Command command;
            bool have_command = false, listing = false;
            std::string id;
            {
                std::unique_lock lock(mutex);
                if (stopping) break;
                if (Clock::now() >= session_deadline) { lock.unlock(); fail("Multiplayer session expired."); fatal = true; break; }
                id = room_id;
                if (!commands.empty()) { command = std::move(commands.front()); commands.pop_front(); have_command = true; }
                else if (!id.empty() && pending_score && Clock::now() >= next_score) {
                    const auto score = *pending_score; pending_score.reset();
                    if (!enqueue_locked("score", {{"score", score_json(score)}})) {
                        lock.unlock(); fail("Multiplayer command queue is full."); fatal = true; break;
                    }
                    next_score = Clock::now() + 500ms; continue;
                } else if (Clock::now() >= next_poll) {
                    if (id.empty()) listing = true;
                    else {
                        if (!enqueue_locked("poll")) { lock.unlock(); fail("Could not poll multiplayer room."); fatal = true; break; }
                        next_poll = Clock::now() + 500ms; continue;
                    }
                } else {
                    wake.wait_until(lock, std::min({next_poll, renew_at,
                        pending_score ? next_score : next_poll}), [this] { return stopping || !commands.empty(); });
                    continue;
                }
            }
            const std::string path = listing ? "/api/rooms" : !command.path.empty() ? command.path : "/api/rooms/" + id;
            const auto body = listing ? std::string{} : config::json_stringify(Json(command.payload), 0, 17);
            Json reply; unsigned status = 0; Clock::time_point before, after;
            bool ok = request(listing ? "GET" : "POST", path, session_token, body, reply, status, before, after);
            // Retry an uncertain delivery once with the exact same request ID.
            // Server deduplication must prevent repeated create/launch/reset.
            if (!ok && !listing) {
                bool stop = false; { std::lock_guard lock(mutex); stop = stopping; }
                if (!stop && Clock::now() < session_deadline)
                    ok = request("POST", path, session_token, body, reply, status, before, after);
            }
            { std::lock_guard lock(mutex); if (stopping) break; }
            if (!ok || status == 401 || status == 403 || status >= 500 || status == 404) {
                fail(status == 401 || status == 403 ? "Multiplayer authorization expired. Reconnect with F10." :
                     "Official multiplayer server is unavailable."); fatal = true; break;
            }
            if (status < 200 || status >= 300) {
                std::lock_guard lock(mutex);
                current.status_detail = "Multiplayer action rejected (HTTP " + std::to_string(status) + ").";
                current.rate_change_pending = false;
                if (command.action == "chart") pending_chart.reset();
                if (command.action == "reset") reset_queued = false;
                if (command.action == "create" || command.action == "join") current.state = PeerSessionState::Idle;
                touch(); next_poll = Clock::now() + (listing ? 3s : status == 429 ? 1s : 0s); continue;
            }
            if (listing ? !accept_rooms(reply) : !accept_room(reply, before, after)) {
                fail("Invalid or late multiplayer server response; disconnected."); fatal = true; break;
            }
            next_poll = Clock::now() + (listing ? 3s : 500ms);
        }
        std::string leaving_room;
        uint64_t nonce = 0, revision = 1;
        { std::lock_guard lock(mutex); leaving_room = room_id; nonce = current.result_round_nonce; revision = current.rate_revision; }
        if (!leaving_room.empty() && !session_token.empty() && Clock::now() < session_deadline) {
            const auto id = request_id();
            if (!id.empty()) {
                Json ignored; unsigned status = 0; Clock::time_point before, after;
                const auto body = config::json_stringify(Json(Object{{"protocol", Json(2.0)}, {"score_version", Json("ex-v1")}, {"action", Json("leave")},
                    {"request_id", Json(id)}, {"nonce", Json(std::to_string(nonce))}, {"rate_revision", Json(static_cast<double>(revision))}}), 0, 17);
                (void)request("POST", "/api/rooms/" + leaving_room, session_token, body, ignored, status, before, after);
            }
        }
        erase_secret(session_token);
        std::lock_guard lock(mutex);
        running = false; commands.clear(); pending_score.reset(); room_id.clear();
        pending_launch.reset(); begin_at.reset();
        if ((!fatal || stopping) && current.state != PeerSessionState::Failed) { current.state = PeerSessionState::Disconnected; current.status_detail = "Disconnected from multiplayer."; }
        current.can_start = false; current.round_active = false; touch();
    }
};

HostedPeerSession::HostedPeerSession(HostedHttpTransport transport) : impl_(std::make_unique<Impl>(std::move(transport))) {}
HostedPeerSession::~HostedPeerSession() { disconnect(); erase_secret(impl_->upload_key); }
bool HostedPeerSession::configure(std::string upload_token) {
    disconnect();
    std::lock_guard lock(impl_->mutex);
    erase_secret(impl_->upload_key); impl_->current = {}; impl_->available_rooms.clear();
    if (!token_valid(upload_token, "tr_")) { erase_secret(upload_token); return false; }
    impl_->upload_key = std::move(upload_token); return true;
}
bool HostedPeerSession::browse() {
    std::lock_guard lifecycle(impl_->lifecycle);
    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->running) return !impl_->stopping;
        if (impl_->upload_key.empty()) return false;
    }
    if (impl_->worker.joinable()) impl_->worker.join();
    std::lock_guard lock(impl_->mutex);
    impl_->stopping = false; impl_->running = true; impl_->current = {};
    impl_->current.status_detail = "Verifying leaderboard connection...";
    impl_->room_id.clear(); impl_->commands.clear(); impl_->seen_launch = 0;
    impl_->local_chart_invalidated = false; impl_->pending_chart.reset(); impl_->reset_queued = false;
    try { impl_->worker = std::thread([this] { impl_->run(); }); }
    catch (...) { impl_->running = false; return false; }
    return true;
}
bool HostedPeerSession::host() {
    if (!browse()) return false;
    std::lock_guard lock(impl_->mutex);
    if (!impl_->room_id.empty() || impl_->current.state == PeerSessionState::Starting || impl_->current.state == PeerSessionState::Connecting) return false;
    if (!impl_->enqueue_locked("create", {}, "/api/rooms")) return false;
    impl_->current.state = PeerSessionState::Starting; impl_->touch(); return true;
}
bool HostedPeerSession::join(std::string room_id) {
    if (!room_id_valid(room_id) || !browse()) return false;
    std::lock_guard lock(impl_->mutex);
    if (!impl_->room_id.empty() || impl_->current.state == PeerSessionState::Starting || impl_->current.state == PeerSessionState::Connecting) return false;
    if (!impl_->enqueue_locked("join", {}, "/api/rooms/" + room_id)) return false;
    impl_->current.state = PeerSessionState::Connecting; impl_->touch(); return true;
}
void HostedPeerSession::request_stop() {
    std::lock_guard lock(impl_->mutex);
    impl_->stopping = true;
    impl_->commands.clear(); impl_->pending_score.reset(); impl_->pending_launch.reset();
    impl_->begin_at.reset(); impl_->current.can_start = false; impl_->current.round_active = false;
    if (impl_->running && impl_->current.state != PeerSessionState::Failed) {
        impl_->current.state = PeerSessionState::Closing;
        impl_->current.status_detail = "Leaving the official multiplayer room...";
    }
    impl_->touch();
}
bool HostedPeerSession::running() const {
    std::lock_guard lock(impl_->mutex); return impl_->running;
}
void HostedPeerSession::disconnect() {
    std::lock_guard lifecycle(impl_->lifecycle);
    request_stop();
    if (impl_->worker.joinable()) impl_->worker.join();
}
PeerSessionSnapshot HostedPeerSession::snapshot() const { std::lock_guard lock(impl_->mutex); return impl_->current; }
std::vector<HostedRoomInfo> HostedPeerSession::rooms() const { std::lock_guard lock(impl_->mutex); return impl_->available_rooms; }
bool HostedPeerSession::set_local_chart(const ChartFingerprint& fp, std::string name) {
    if (!fp.valid() || name.size() > 1024) return false;
    std::lock_guard lock(impl_->mutex);
    if (!impl_->active_locked() || impl_->current.round_active) return false;
    if (!impl_->current.local_is_leader && (fp.hash != impl_->current.selected_chart.fingerprint.hash || fp.size != impl_->current.selected_chart.fingerprint.size)) return false;
    if (impl_->pending_chart && impl_->pending_chart->hash == fp.hash && impl_->pending_chart->size == fp.size) return true;
    if (!impl_->enqueue_locked("chart", {{"chart", chart_json(fp, std::move(name))}})) return false;
    impl_->pending_chart = fp;
    impl_->local_chart_invalidated = false;
    return true;
}
void HostedPeerSession::clear_local_chart() {
    std::lock_guard lock(impl_->mutex);
    impl_->local_chart_invalidated = true; impl_->pending_chart.reset();
    impl_->current.local_chart = {}; impl_->current.local_ready = false;
    impl_->current.can_start = false;
    if (impl_->active_locked() && !impl_->current.round_active)
        (void)impl_->enqueue_locked("chart", {{"chart", Json()}});
    impl_->touch();
}
bool HostedPeerSession::set_ready(bool ready) {
    std::lock_guard lock(impl_->mutex);
    const auto& s = impl_->current;
    if (!impl_->active_locked() || s.round_active || s.rate_change_pending ||
        (ready && (!s.local_chart.fingerprint.valid() || s.local_chart.fingerprint.hash != s.selected_chart.fingerprint.hash ||
                   s.local_chart.fingerprint.size != s.selected_chart.fingerprint.size))) return false;
    return impl_->enqueue_locked("ready", {{"ready", Json(ready)}});
}
bool HostedPeerSession::set_rate(uint32_t rate) {
    std::lock_guard lock(impl_->mutex);
    if (!impl_->active_locked() || !impl_->current.local_is_leader || impl_->current.round_active ||
        impl_->current.rate_change_pending || !peer_rate_milli_is_valid(rate)) return false;
    if (!impl_->enqueue_locked("rate", {{"rate_milli", Json(static_cast<double>(rate))}})) return false;
    impl_->current.rate_change_pending = true; impl_->touch(); return true;
}
bool HostedPeerSession::send_chat(std::string text) {
    if (text.empty() || text.size() > 256 || text.find('\0') != std::string::npos) return false;
    std::lock_guard lock(impl_->mutex);
    return impl_->active_locked() && impl_->enqueue_locked("chat", {{"text", Json(std::move(text))}});
}
bool HostedPeerSession::send_launch() {
    std::lock_guard lock(impl_->mutex);
    return impl_->active_locked() && impl_->current.can_start && impl_->enqueue_locked("launch");
}
std::optional<uint64_t> HostedPeerSession::poll_launch() {
    std::lock_guard lock(impl_->mutex);
    if (!impl_->active_locked() || !impl_->current.round_active) {
        impl_->pending_launch.reset(); return std::nullopt;
    }
    auto result = impl_->pending_launch; impl_->pending_launch.reset(); return result;
}
bool HostedPeerSession::mark_loaded() {
    std::lock_guard lock(impl_->mutex);
    return impl_->active_locked() && impl_->current.round_active && impl_->enqueue_locked("loaded");
}
bool HostedPeerSession::wait_for_peer_loaded(std::chrono::milliseconds timeout) {
    std::unique_lock lock(impl_->mutex);
    impl_->wake.wait_for(lock, timeout, [this] { return !impl_->active_locked() ||
        (impl_->current.local_loaded && impl_->current.remote_loaded); });
    return impl_->active_locked() && impl_->current.local_loaded && impl_->current.remote_loaded;
}
bool HostedPeerSession::send_begin() {
    std::lock_guard lock(impl_->mutex);
    return impl_->active_locked() && impl_->current.local_is_leader && impl_->current.local_loaded &&
        impl_->current.remote_loaded && impl_->enqueue_locked("begin");
}
bool HostedPeerSession::wait_for_begin(std::chrono::milliseconds timeout, uint32_t& delay) {
    std::unique_lock lock(impl_->mutex);
    impl_->wake.wait_for(lock, timeout, [this] { return !impl_->active_locked() || impl_->begin_at.has_value(); });
    if (!impl_->active_locked() || !impl_->begin_at || impl_->begin_consumed) return false;
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(*impl_->begin_at - Clock::now()).count();
    if (remaining <= 0 || remaining > 10000) {
        impl_->current.state = PeerSessionState::Failed;
        impl_->current.status_detail = "Synchronized start was missed; reconnect to the room.";
        impl_->stopping = true; impl_->touch(); return false;
    }
    delay = static_cast<uint32_t>(remaining); impl_->begin_consumed = true; return true;
}
bool HostedPeerSession::publish_score(const PeerScore& score, bool final) {
    auto value = score; value.finished = final || score.finished;
    if (!hosted_ex_score_is_sane(value)) return false;
    std::lock_guard lock(impl_->mutex);
    if (!impl_->active_locked() || !impl_->current.round_active || impl_->final_queued) return false;
    if (value.finished) {
        if (!impl_->enqueue_locked("score", {{"score", score_json(value)}})) return false;
        impl_->pending_score.reset(); impl_->final_queued = true;
    } else { impl_->pending_score = value; impl_->wake.notify_all(); }
    return true;
}
void HostedPeerSession::reset_round() {
    std::lock_guard lock(impl_->mutex);
    if (impl_->active_locked() && impl_->current.round_active && !impl_->reset_queued && impl_->enqueue_locked("reset"))
        impl_->reset_queued = true;
}

}  // namespace tenriff::network
