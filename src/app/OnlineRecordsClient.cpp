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

bool fetch_online_records_impl(const std::string& base_url,
                               const std::string& chart_sha256,
                               OnlineRecordsResponse& output,
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
    path += L"/v1/leaderboards/";
    path += utf8_to_wide(chart_sha256);
    path += L"?limit=50";

    InternetHandle session(WinHttpOpen(L"TenRiff/1.5 online-records",
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
        WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
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

    std::string body;
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
    return parse_online_records_response(body, chart_sha256, output, error);
}
#else
bool fetch_online_records_impl(const std::string&,
                               const std::string&,
                               OnlineRecordsResponse&,
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
    return fetch_online_records_impl(base_url, lower_ascii(chart_sha256),
                                     output, error);
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

    Impl() : worker([this] { run(); }) {}

    void run() {
        for (;;) {
            std::string url;
            std::string hash;
            std::uint64_t active_request = 0;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [this] { return stopping || pending; });
                if (stopping) return;
                pending = false;
                url = pending_url;
                hash = pending_hash;
                active_request = request_id;
            }
            OnlineRecordsResponse response;
            std::string error;
            const bool ok = fetch_online_records_once(url, hash, response, error);
            {
                std::lock_guard lock(mutex);
                if (stopping) return;
                if (active_request != request_id) continue;
                current.state = ok ? OnlineRecordsState::Ready
                                   : OnlineRecordsState::Error;
                current.chart_sha256 = hash;
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
    if (!force_refresh && impl_->current.chart_sha256 == chart_sha256 &&
        impl_->pending_url == base_url &&
        impl_->current.state != OnlineRecordsState::Idle) {
        return;
    }
    ++impl_->request_id;
    impl_->pending = true;
    impl_->pending_url = std::move(base_url);
    impl_->pending_hash = chart_sha256;
    impl_->current.state = OnlineRecordsState::Loading;
    impl_->current.chart_sha256 = std::move(chart_sha256);
    impl_->current.records.clear();
    impl_->current.error.clear();
    ++impl_->current.revision;
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
