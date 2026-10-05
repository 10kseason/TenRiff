#include "app/OnlineRecordsClient.h"

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

#include "config/SimpleJson.h"
#include "app/MainApiTlsPin.h"
#include "app/SitesLeaderboardConnection.h"
#include "util/Utf8Compat.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace tenriff::app {
namespace {

constexpr std::size_t kMaximumResponseBytes = 1024 * 1024;
constexpr std::size_t kMaximumRecords = 100;

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char byte) {
        return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a')
                                          : static_cast<char>(byte);
    });
    return value;
}

bool valid_sha256(std::string_view value) {
    return value.size() == 64 &&
           std::all_of(value.begin(), value.end(), [](unsigned char byte) {
               return (byte >= '0' && byte <= '9') ||
                      (byte >= 'a' && byte <= 'f') ||
                      (byte >= 'A' && byte <= 'F');
           });
}

const config::JsonValue* find_field(const config::JsonObject& object,
                                    const char* name) {
    const auto found = object.find(name);
    return found == object.end() ? nullptr : &found->second;
}

bool required_string(const config::JsonObject& object,
                     const char* name,
                     std::string& value) {
    const auto* field = find_field(object, name);
    if (!field || !field->is_string()) return false;
    value = field->as_string();
    return true;
}

bool required_integer(const config::JsonObject& object,
                      const char* name,
                      std::int64_t minimum,
                      std::int64_t maximum,
                      std::int64_t& value) {
    const auto* field = find_field(object, name);
    if (!field || !field->is_number()) return false;
    const double number = field->as_number();
    if (!std::isfinite(number) || std::floor(number) != number ||
        number < static_cast<double>(minimum) ||
        number > static_cast<double>(maximum)) {
        return false;
    }
    value = static_cast<std::int64_t>(number);
    return true;
}

#ifdef _WIN32
class InternetHandle {
public:
    InternetHandle() = default;
    explicit InternetHandle(HINTERNET value) : value_(value) {}
    ~InternetHandle() { reset(); }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    InternetHandle(InternetHandle&& other) noexcept
        : value_(std::exchange(other.value_, nullptr)) {}
    [[nodiscard]] HINTERNET get() const { return value_; }
    [[nodiscard]] explicit operator bool() const { return value_ != nullptr; }
    void reset() {
        if (value_) WinHttpCloseHandle(std::exchange(value_, nullptr));
    }
private:
    HINTERNET value_ = nullptr;
};

std::wstring utf8_to_wide(std::string_view value) {
    if (value.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          value.data(), static_cast<int>(value.size()),
                                          nullptr, 0);
    if (count <= 0) return {};
    std::wstring output(static_cast<std::size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            value.data(), static_cast<int>(value.size()),
                            output.data(), count) != count) {
        return {};
    }
    return output;
}

std::string winhttp_error(const char* operation) {
    const DWORD code = GetLastError();
    return std::string(operation) + " failed (WinHTTP error " +
           std::to_string(code) + ").";
}

bool fetch_json_impl(const std::string& base_url,
                               const std::string& endpoint,
                               std::string& body,
                               std::string& error) {
    const std::wstring url = utf8_to_wide(base_url);
    if (url.empty()) {
        error = "Online records server URL is empty or invalid UTF-8.";
        return false;
    }
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0,
                         &components)) {
        error = "Online records server URL is invalid.";
        return false;
    }
    if (components.nScheme != INTERNET_SCHEME_HTTP &&
        components.nScheme != INTERNET_SCHEME_HTTPS) {
        error = "Online records server URL must use HTTP or HTTPS.";
        return false;
    }
    if (components.dwHostNameLength == 0 || components.dwExtraInfoLength != 0) {
        error = "Online records server URL must not contain a query or fragment.";
        return false;
    }
    const std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    while (!path.empty() && path.back() == L'/') path.pop_back();
    path += utf8_to_wide(endpoint);

    InternetHandle session(WinHttpOpen(L"TenRiff/1.8.41 records",
                                       WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                       WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        error = winhttp_error("WinHttpOpen");
        return false;
    }
    WinHttpSetTimeouts(session.get(), 3000, 3000, 5000, 5000);
    InternetHandle connection(WinHttpConnect(session.get(), host.c_str(),
                                             components.nPort, 0));
    if (!connection) {
        error = winhttp_error("WinHttpConnect");
        return false;
    }
    const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS
                            ? WINHTTP_FLAG_SECURE
                            : 0;
    InternetHandle request(WinHttpOpenRequest(
        connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request) {
        error = winhttp_error("WinHttpOpenRequest");
        return false;
    }
    DWORD redirect_policy =
        WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY,
                          &redirect_policy,
                          sizeof(redirect_policy))) {
        error = winhttp_error("WinHttpSetOption redirect policy");
        return false;
    }
    const bool verify_main_api_pin =
        main_api_tls_pin::applies(components.nScheme, components.nPort, host);
    if (!main_api_tls_pin::prepare(request.get(), verify_main_api_pin, error)) {
        return false;
    }
    const wchar_t* headers = L"Accept: application/json\r\n";
    if (!WinHttpSendRequest(request.get(), headers, static_cast<DWORD>(-1L),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
        error = winhttp_error("Online records request");
        return false;
    }
    if (!main_api_tls_pin::verify(request.get(), verify_main_api_pin, error)) {
        return false;
    }
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(),
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                             WINHTTP_NO_HEADER_INDEX)) {
        error = winhttp_error("WinHttpQueryHeaders");
        return false;
    }
    if (status != 200) {
        error = "Online records server returned HTTP " + std::to_string(status) + ".";
        return false;
    }

    body.clear();
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available)) {
            error = winhttp_error("WinHttpQueryDataAvailable");
            return false;
        }
        if (available == 0) break;
        if (static_cast<std::size_t>(available) >
            kMaximumResponseBytes - body.size()) {
            error = "Online records response exceeds 1 MiB.";
            return false;
        }
        const std::size_t old_size = body.size();
        body.resize(old_size + available);
        DWORD received = 0;
        if (!WinHttpReadData(request.get(), body.data() + old_size, available,
                             &received)) {
            error = winhttp_error("WinHttpReadData");
            return false;
        }
        body.resize(old_size + received);
        if (body.size() > kMaximumResponseBytes) {
            error = "Online records response exceeds 1 MiB.";
            return false;
        }
    }
    return true;
}
#else
bool fetch_json_impl(const std::string&,
                               const std::string&,
                               std::string&,
                               std::string& error) {
    error = "Online records HTTP is not available on this platform build.";
    return false;
}
#endif

}  // namespace

bool parse_online_records_response(std::string_view json,
                                   std::string_view expected_chart_sha256,
                                   OnlineRecordsResponse& output,
                                   std::string& error) {
    output = {};
    error.clear();
    if (!valid_sha256(expected_chart_sha256)) {
        error = "Expected chart SHA-256 is invalid.";
        return false;
    }
    const auto parsed = config::parse_json(json);
    if (!parsed.success() || !parsed.root->is_object()) {
        error = parsed.error.empty() ? "Online records response is not a JSON object."
                                     : parsed.error;
        return false;
    }
    const auto& root = *parsed.root->as_object();
    std::int64_t schema = 0;
    if (!required_integer(root, "schema_version", 1, 1, schema) || schema != 1) {
        error = "Online records response has an unsupported schema version.";
        return false;
    }
    if (!required_string(root, "chart_sha256", output.chart_sha256) ||
        lower_ascii(output.chart_sha256) != lower_ascii(std::string(expected_chart_sha256))) {
        error = "Online records response chart hash does not match the request.";
        output = {};
        return false;
    }
    const auto* records_field = find_field(root, "records");
    const auto* records = records_field ? records_field->as_array() : nullptr;
    if (!records || records->size() > kMaximumRecords) {
        error = "Online records response contains an invalid record list.";
        output = {};
        return false;
    }
    output.chart_sha256 = lower_ascii(output.chart_sha256);
    output.records.reserve(records->size());
    for (std::size_t index = 0; index < records->size(); ++index) {
        const auto* object = (*records)[index].as_object();
        if (!object) {
            error = "Online record entry is not an object.";
            output = {};
            return false;
        }
        OnlineRecordEntry entry;
        std::int64_t rank = 0;
        std::int64_t combo = 0;
        const auto* accuracy_field = find_field(*object, "accuracy");
        if (!required_integer(*object, "rank", 1, 100, rank) ||
            !required_string(*object, "player_name", entry.player_name) ||
            !required_integer(*object, "score", 0, 1'000'000'000LL, entry.score) ||
            !accuracy_field || !accuracy_field->is_number() ||
            !std::isfinite(accuracy_field->as_number()) ||
            accuracy_field->as_number() < 0.0 || accuracy_field->as_number() > 100.0 ||
            !required_integer(*object, "max_combo", 0,
                              (std::numeric_limits<int>::max)(), combo) ||
            !required_string(*object, "clear_status", entry.clear_status) ||
            !required_string(*object, "ruleset_id", entry.ruleset_id) ||
            !required_string(*object, "verification_status",
                             entry.verification_status) ||
            !required_string(*object, "verified_at_utc", entry.verified_at_utc) ||
            entry.verification_status != "online_verified" ||
            entry.player_name.empty() || entry.player_name.size() > 64 ||
            entry.ruleset_id.empty() || entry.ruleset_id.size() > 64 ||
            rank != static_cast<std::int64_t>(index + 1) ||
            util::sanitize_ui_text(entry.player_name) != entry.player_name ||
            util::sanitize_ui_text(entry.clear_status) != entry.clear_status ||
            util::sanitize_ui_text(entry.ruleset_id) != entry.ruleset_id ||
            util::sanitize_ui_text(entry.verified_at_utc) != entry.verified_at_utc) {
            error = "Online record entry contains invalid or unverified data.";
            output = {};
            return false;
        }
        entry.rank = static_cast<int>(rank);
        entry.max_combo = static_cast<int>(combo);
        entry.accuracy = accuracy_field->as_number();
        output.records.push_back(std::move(entry));
    }
    return true;
}

bool fetch_online_records_once(const std::string& base_url,
                               const std::string& chart_sha256,
                               OnlineRecordsResponse& output,
                               std::string& error) {
    if (!valid_sha256(chart_sha256)) {
        error = "Chart SHA-256 must contain 64 hexadecimal characters.";
        output = {};
        return false;
    }
    std::string body;
    return fetch_json_impl(base_url, "/v1/leaderboards/" + lower_ascii(chart_sha256) + "?limit=50", body, error) &&
           parse_online_records_response(body, chart_sha256, output, error);
}

bool parse_sites_record_boards(std::string_view json, std::string_view title,
                               std::vector<SitesRecordBoard>& output, std::string& error) {
    output.clear();
    error.clear();
    const auto parsed = config::parse_json(json);
    const auto* root = parsed.success() ? parsed.root->as_object() : nullptr;
    const auto* field = root ? find_field(*root, "boards") : nullptr;
    const auto* boards = field ? field->as_array() : nullptr;
    auto fail = [&] { output.clear(); error = "Invalid web leaderboard board list."; return false; };
    if (json.size() > kMaximumResponseBytes || !boards || boards->size() > kMaximumRecords) return fail();
    const auto safe_string = [](const config::JsonObject& object, const char* key, std::string& value, std::size_t limit) {
        return required_string(object, key, value) && !value.empty() && value.size() <= limit &&
            util::sanitize_ui_text(value) == value;
    };
    for (const auto& value : *boards) {
        const auto* object = value.as_object();
        SitesRecordBoard board;
        std::string conditions;
        std::int64_t rate = 0;
        if (!object || !required_string(*object, "id", board.id) || !valid_sha256(board.id) ||
            !safe_string(*object, "title", board.title, 1920) ||
            !required_string(*object, "chart_sha256", board.chart_sha256) || !valid_sha256(board.chart_sha256) ||
            !safe_string(*object, "key_mode", board.key_mode, 4) ||
            !required_integer(*object, "rate_milli", 500, 2000, rate) ||
            !required_string(*object, "conditions", conditions) || conditions.size() > 8192) return fail();
        const auto condition_json = config::parse_json(conditions);
        const auto* condition = condition_json.success() ? condition_json.root->as_object() : nullptr;
        std::string gauge, random;
        if (!condition || !safe_string(*condition, "ruleset", board.ruleset_id, 64) ||
            !safe_string(*condition, "gauge", gauge, 64) || !safe_string(*condition, "random", random, 64)) return fail();
        std::string timing_label = "TIMING UNKNOWN";
        if (find_field(*condition, "timing_profile")) {
            std::string profile;
            if (!safe_string(*condition, "timing_profile", profile, 64) ||
                !std::all_of(profile.begin(), profile.end(), [](unsigned char ch) {
                    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
                })) return fail();
            // Label the base policy only: judge mods may change effective windows.
            if (profile == "bms-easy") timing_label = "BMS EASY";
            else if (profile == "bms-normal") timing_label = "BMS NORMAL";
            else if (profile == "bms-hard") timing_label = "BMS HARD";
            else if (profile == "bms-veryhard") timing_label = "BMS VERY HARD";
            else if (profile == "osu-fixed") timing_label = "osu!mania";
        }
        board.rate_milli = static_cast<int>(rate);
        board.id = lower_ascii(board.id);
        board.chart_sha256 = lower_ascii(board.chart_sha256);
        const std::string ruleset_label = board.ruleset_id == "tenriff-native-score-v2-ruleset-4" ? "RULESET 4" :
                                          board.ruleset_id == "tenriff-native-score-v2-ruleset-3" ? "RULESET 3" :
                                          board.ruleset_id == "tenriff-native-score-v2-ruleset-2" ? "RULESET 2" :
            board.ruleset_id == "tenriff-native-score-v2-ruleset-1" ? "RULESET 1" : board.ruleset_id;
        board.conditions_label = board.key_mode + " / " + std::to_string(rate / 1000) + "." +
            (rate % 1000 < 100 ? "0" : "") + std::to_string((rate % 1000) / 10) +
            "x / " + ruleset_label + " / " + timing_label + " / " + gauge + " / " + random;
        if (const auto* mods_field = find_field(*condition, "mods")) {
            const auto* mods = mods_field->as_array();
            if (!mods || mods->size() > 20) return fail();
            for (const auto& mod : *mods) {
                if (!mod.is_string() || mod.as_string().size() > 64 ||
                    util::sanitize_ui_text(mod.as_string()) != mod.as_string()) return fail();
                board.conditions_label += " / " + mod.as_string();
            }
        }
        // The site combines converted variants by title and conditions. Keep
        // its groups intact and exclude substring-only matches from this view.
        if (board.title == title) output.push_back(std::move(board));
    }
    std::sort(output.begin(), output.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return true;
}

bool parse_sites_record_rankings(std::string_view json, const SitesRecordBoard& board,
                                 std::vector<OnlineRecordEntry>& output, std::string& error) {
    output.clear();
    error.clear();
    const auto parsed = config::parse_json(json);
    const auto* root = parsed.success() ? parsed.root->as_object() : nullptr;
    const auto* board_field = root ? find_field(*root, "board") : nullptr;
    const auto* returned_board = board_field ? board_field->as_object() : nullptr;
    const auto* rankings_field = root ? find_field(*root, "rankings") : nullptr;
    const auto* rankings = rankings_field ? rankings_field->as_array() : nullptr;
    std::string id;
    auto fail = [&] { output.clear(); error = "Invalid web leaderboard rankings."; return false; };
    if (json.size() > kMaximumResponseBytes || !returned_board ||
        !required_string(*returned_board, "id", id) || id != board.id ||
        !rankings || rankings->size() > kMaximumRecords) return fail();
    int previous_rank = 0;
    for (const auto& value : *rankings) {
        const auto* object = value.as_object();
        OnlineRecordEntry record;
        std::int64_t rank = 0, combo = 0;
        const auto* accuracy = object ? find_field(*object, "accuracy") : nullptr;
        if (!object || !required_integer(*object, "rank", 1, 100, rank) ||
            rank < previous_rank || rank > static_cast<std::int64_t>(output.size() + 1) ||
            !required_string(*object, "nickname", record.player_name) || record.player_name.empty() || record.player_name.size() > 256 ||
            !required_integer(*object, "score", 0, 1'000'000'000, record.score) ||
            !required_integer(*object, "max_combo", 0, (std::numeric_limits<int>::max)(), combo) ||
            !accuracy || !accuracy->is_number() || !std::isfinite(accuracy->as_number()) ||
            accuracy->as_number() < 0 || accuracy->as_number() > 100 ||
            !required_string(*object, "clear_status", record.clear_status) || record.clear_status.size() > 64 ||
            !required_string(*object, "played_at", record.verified_at_utc) || record.verified_at_utc.size() > 64 ||
            util::sanitize_ui_text(record.player_name) != record.player_name ||
            util::sanitize_ui_text(record.clear_status) != record.clear_status ||
            util::sanitize_ui_text(record.verified_at_utc) != record.verified_at_utc) return fail();
        record.rank = previous_rank = static_cast<int>(rank);
        record.max_combo = static_cast<int>(combo);
        record.accuracy = record.detailed_accuracy = accuracy->as_number();
        if (const auto* detail = find_field(*object, "detail_score"); detail && !detail->is_null()) {
            if (!required_integer(*object, "detail_score", 0, 1'000'000'000, record.detail_score)) return fail();
            record.detail_score_available = true;
        }
        if (const auto* detail = find_field(*object, "detailed_accuracy"); detail && !detail->is_null()) {
            if (!detail->is_number() || !std::isfinite(detail->as_number()) || detail->as_number() < 0 || detail->as_number() > 100) return fail();
            record.detailed_accuracy = detail->as_number();
            record.detailed_accuracy_available = true;
        }
        record.ruleset_id = board.ruleset_id;
        record.verification_status = "sites_community";
        output.push_back(std::move(record));
    }
    return true;
}

bool fetch_sites_records_once(const std::string& hash, const std::string& title,
                              int board_index, OnlineRecordsResponse& output, std::string& error) {
    output = {};
    output.chart_sha256 = hash;
    if (title.empty() || title.size() > 1920) { error = "Chart title is unavailable."; return false; }
    std::string query;
    constexpr char hex[] = "0123456789ABCDEF";
    // Percent-encode UTF-8 bytes; never concatenate a title as URL syntax.
    for (unsigned char ch : title) {
        query += '%'; query += hex[ch >> 4]; query += hex[ch & 15];
    }
    std::string body;
    // Public, read-only endpoints: no connection key, profile or upload token.
    if (!fetch_json_impl(kSitesLeaderboardUrl, "/api/boards?q=" + query, body, error) ||
        !parse_sites_record_boards(body, title, output.boards, error)) return false;
    std::stable_partition(output.boards.begin(), output.boards.end(), [&](const auto& board) {
        return board.chart_sha256 == lower_ascii(hash);
    });
    if (output.boards.empty()) return true;
    output.board_index = std::clamp(board_index, 0, static_cast<int>(output.boards.size()) - 1);
    const auto& board = output.boards[static_cast<std::size_t>(output.board_index)];
    return fetch_json_impl(kSitesLeaderboardUrl, "/api/leaderboard?board=" + board.id, body, error) &&
        parse_sites_record_rankings(body, board, output.records, error);
}

struct OnlineRecordsService::Impl {
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    bool stopping = false;
    bool pending = false;
    std::uint64_t request_id = 0;
    std::string pending_url;
    std::string pending_hash;
    OnlineRecordsSnapshot current;
    bool pending_sites = false;
    std::string pending_title;
    int pending_board = 0;

    Impl() : worker([this] { run(); }) {}

    void run() {
        for (;;) {
            std::string url;
            std::string hash;
            std::string title;
            int board = 0;
            bool sites = false;
            std::uint64_t active_request = 0;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [this] { return stopping || pending; });
                if (stopping) return;
                pending = false;
                url = pending_url;
                hash = pending_hash;
                title = pending_title;
                board = pending_board;
                sites = pending_sites;
                active_request = request_id;
            }
            OnlineRecordsResponse response;
            std::string error;
            const bool ok = sites
                ? fetch_sites_records_once(hash, title, board, response, error)
                : fetch_online_records_once(url, hash, response, error);
            {
                std::lock_guard lock(mutex);
                if (stopping) return;
                if (active_request != request_id) continue;
                current.state = ok ? OnlineRecordsState::Ready
                                   : OnlineRecordsState::Error;
                current.chart_sha256 = hash;
                current.sites = sites;
                current.boards = std::move(response.boards);
                current.board_index = response.board_index;
                current.records = ok ? std::move(response.records)
                                     : std::vector<OnlineRecordEntry>{};
                current.error = ok ? std::string{} : std::move(error);
                ++current.revision;
            }
        }
    }
};

OnlineRecordsService::OnlineRecordsService() : impl_(std::make_unique<Impl>()) {}
OnlineRecordsService::~OnlineRecordsService() { shutdown(); }

void OnlineRecordsService::request(std::string base_url,
                                   std::string chart_sha256,
                                   bool force_refresh) {
    if (!impl_) return;
    chart_sha256 = lower_ascii(std::move(chart_sha256));
    std::lock_guard lock(impl_->mutex);
    if (impl_->stopping) return;
    if (!force_refresh && !impl_->pending_sites && impl_->current.chart_sha256 == chart_sha256 &&
        impl_->pending_url == base_url &&
        impl_->current.state != OnlineRecordsState::Idle) {
        return;
    }
    ++impl_->request_id;
    impl_->pending = true;
    impl_->pending_sites = false;
    impl_->current.sites = false;
    impl_->current.boards.clear();
    impl_->pending_url = std::move(base_url);
    impl_->pending_hash = chart_sha256;
    impl_->current.state = OnlineRecordsState::Loading;
    impl_->current.chart_sha256 = std::move(chart_sha256);
    impl_->current.records.clear();
    impl_->current.error.clear();
    ++impl_->current.revision;
    impl_->wake.notify_one();
}

void OnlineRecordsService::request_sites(std::string hash, std::string title,
                                          int board, bool force_refresh) {
    if (!impl_) return;
    hash = lower_ascii(std::move(hash));
    board = std::max(0, board);
    std::lock_guard lock(impl_->mutex);
    if (impl_->stopping) return;
    if (!force_refresh && impl_->pending_sites && impl_->pending_hash == hash &&
        impl_->pending_title == title && impl_->pending_board == board &&
        impl_->current.state != OnlineRecordsState::Idle) return;
    ++impl_->request_id;
    impl_->pending = true;
    impl_->pending_sites = true;
    impl_->pending_hash = hash;
    impl_->pending_title = std::move(title);
    impl_->pending_board = board;
    const auto revision = impl_->current.revision + 1;
    impl_->current = {};
    impl_->current.chart_sha256 = std::move(hash);
    impl_->current.sites = true;
    impl_->current.state = OnlineRecordsState::Loading;
    impl_->current.revision = revision;
    impl_->wake.notify_one();
}

OnlineRecordsSnapshot OnlineRecordsService::snapshot() const {
    if (!impl_) return {};
    std::lock_guard lock(impl_->mutex);
    return impl_->current;
}

void OnlineRecordsService::shutdown() {
    if (!impl_) return;
    {
        std::lock_guard lock(impl_->mutex);
        impl_->stopping = true;
        impl_->wake.notify_all();
    }
    if (impl_->worker.joinable()) impl_->worker.join();
    impl_.reset();
}

}  // namespace tenriff::app
