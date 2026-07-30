#include "app/DifficultyTableLink.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <utility>

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

#include "app/DifficultyTable.h"
#include "config/SimpleJson.h"

namespace tenriff::app {

namespace {

constexpr std::size_t kMaxHtmlBytes = 4u * 1024u * 1024u;
constexpr std::size_t kMaxJsonBytes = 128u * 1024u * 1024u;

std::string trim_copy(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1u])) != 0) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('A') && ch <= static_cast<unsigned char>('Z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('A') + static_cast<unsigned char>('a'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

bool is_http_url(std::string_view value) {
    const std::string lower = lower_ascii(trim_copy(value));
    return lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0;
}

std::string strip_fragment(std::string value) {
    const std::size_t fragment = value.find('#');
    if (fragment != std::string::npos) {
        value.erase(fragment);
    }
    return value;
}

std::string html_entity_decode(std::string value) {
    const std::array<std::pair<std::string_view, std::string_view>, 5> entities{{
        {"&amp;", "&"},
        {"&quot;", "\""},
        {"&#39;", "'"},
        {"&lt;", "<"},
        {"&gt;", ">"},
    }};
    for (const auto& [encoded, decoded] : entities) {
        std::size_t offset = 0;
        while ((offset = value.find(encoded, offset)) != std::string::npos) {
            value.replace(offset, encoded.size(), decoded);
            offset += decoded.size();
        }
    }
    return value;
}

std::unordered_map<std::string, std::string> parse_html_attributes(std::string_view tag) {
    std::unordered_map<std::string, std::string> attributes;
    std::size_t cursor = 0;
    while (cursor < tag.size() && tag[cursor] != '<') {
        ++cursor;
    }
    while (cursor < tag.size() && tag[cursor] != ' ' && tag[cursor] != '\t' &&
           tag[cursor] != '\r' && tag[cursor] != '\n') {
        ++cursor;
    }
    while (cursor < tag.size()) {
        while (cursor < tag.size() &&
               (std::isspace(static_cast<unsigned char>(tag[cursor])) != 0 ||
                tag[cursor] == '/' || tag[cursor] == '>')) {
            ++cursor;
        }
        const std::size_t name_begin = cursor;
        while (cursor < tag.size()) {
            const unsigned char ch = static_cast<unsigned char>(tag[cursor]);
            if (std::isalnum(ch) == 0 && ch != '-' && ch != '_' && ch != ':') {
                break;
            }
            ++cursor;
        }
        if (cursor == name_begin) {
            ++cursor;
            continue;
        }
        std::string name = lower_ascii(std::string(tag.substr(name_begin, cursor - name_begin)));
        while (cursor < tag.size() && std::isspace(static_cast<unsigned char>(tag[cursor])) != 0) {
            ++cursor;
        }
        if (cursor >= tag.size() || tag[cursor] != '=') {
            attributes.emplace(std::move(name), std::string{});
            continue;
        }
        ++cursor;
        while (cursor < tag.size() && std::isspace(static_cast<unsigned char>(tag[cursor])) != 0) {
            ++cursor;
        }
        std::string value;
        if (cursor < tag.size() && (tag[cursor] == '\"' || tag[cursor] == '\'')) {
            const char quote = tag[cursor++];
            const std::size_t value_begin = cursor;
            while (cursor < tag.size() && tag[cursor] != quote) {
                ++cursor;
            }
            value.assign(tag.substr(value_begin, cursor - value_begin));
            if (cursor < tag.size()) {
                ++cursor;
            }
        } else {
            const std::size_t value_begin = cursor;
            while (cursor < tag.size() &&
                   std::isspace(static_cast<unsigned char>(tag[cursor])) == 0 &&
                   tag[cursor] != '>') {
                ++cursor;
            }
            value.assign(tag.substr(value_begin, cursor - value_begin));
        }
        attributes.insert_or_assign(std::move(name), html_entity_decode(trim_copy(value)));
    }
    return attributes;
}

struct ParsedHttpUrl {
    std::string scheme;
    std::string authority;
    std::string path;
    std::string query;
};

std::optional<ParsedHttpUrl> parse_http_url(std::string_view raw_url) {
    const std::string url = strip_fragment(trim_copy(raw_url));
    const std::size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        return std::nullopt;
    }
    ParsedHttpUrl parsed;
    parsed.scheme = lower_ascii(url.substr(0, scheme_end));
    if (parsed.scheme != "http" && parsed.scheme != "https") {
        return std::nullopt;
    }
    const std::size_t authority_begin = scheme_end + 3u;
    const std::size_t path_begin = url.find_first_of("/?", authority_begin);
    parsed.authority = url.substr(
        authority_begin,
        path_begin == std::string::npos ? std::string::npos : path_begin - authority_begin);
    if (parsed.authority.empty() || parsed.authority.find('@') != std::string::npos) {
        return std::nullopt;
    }
    if (path_begin == std::string::npos) {
        parsed.path = "/";
        return parsed;
    }
    if (url[path_begin] == '?') {
        parsed.path = "/";
        parsed.query = url.substr(path_begin);
        return parsed;
    }
    const std::size_t query_begin = url.find('?', path_begin);
    parsed.path = url.substr(
        path_begin,
        query_begin == std::string::npos ? std::string::npos : query_begin - path_begin);
    parsed.query = query_begin == std::string::npos ? std::string{} : url.substr(query_begin);
    return parsed;
}

std::string normalize_url_path(std::string_view raw_path) {
    std::vector<std::string> segments;
    std::size_t cursor = 0;
    while (cursor <= raw_path.size()) {
        const std::size_t slash = raw_path.find('/', cursor);
        const std::size_t end = slash == std::string_view::npos ? raw_path.size() : slash;
        const std::string segment(raw_path.substr(cursor, end - cursor));
        if (segment == "..") {
            if (!segments.empty()) {
                segments.pop_back();
            }
        } else if (!segment.empty() && segment != ".") {
            segments.push_back(segment);
        }
        if (slash == std::string_view::npos) {
            break;
        }
        cursor = slash + 1u;
    }
    std::string normalized = "/";
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i != 0) {
            normalized.push_back('/');
        }
        normalized += segments[i];
    }
    if (!raw_path.empty() && raw_path.back() == '/' && normalized.back() != '/') {
        normalized.push_back('/');
    }
    return normalized;
}

std::optional<std::string> json_string_field(const config::JsonObject& object, std::string_view key) {
    const auto it = object.find(std::string(key));
    if (it == object.end() || !it->second.is_string()) {
        return std::nullopt;
    }
    return trim_copy(it->second.as_string());
}

std::string cache_key_for_url(std::string_view url) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char ch : url) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

bool write_cache_file(const std::filesystem::path& path, std::string_view content, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        error = "Could not create difficulty-table cache directory: " + ec.message();
        return false;
    }
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "Could not write difficulty-table cache file.";
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!out) {
            error = "Could not write difficulty-table cache file completely.";
            return false;
        }
    }
    std::filesystem::copy_file(
        temporary, path, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::remove(temporary);
    if (ec) {
        error = "Could not replace difficulty-table cache file: " + ec.message();
        return false;
    }
    return true;
}

#ifdef _WIN32
struct WinHttpHandle {
    HINTERNET value = nullptr;
    ~WinHttpHandle() {
        if (value) {
            WinHttpCloseHandle(value);
        }
    }
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET handle) : value(handle) {}
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
};

std::wstring utf8_to_wide(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    const int count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8,
                            MB_ERR_INVALID_CHARS,
                            value.data(),
                            static_cast<int>(value.size()),
                            result.data(),
                            count) != count) {
        return {};
    }
    return result;
}

std::string wide_to_utf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int count = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(count), '\0');
    if (WideCharToMultiByte(CP_UTF8,
                            0,
                            value.data(),
                            static_cast<int>(value.size()),
                            result.data(),
                            count,
                            nullptr,
                            nullptr) != count) {
        return {};
    }
    return result;
}

std::string winhttp_final_url(HINTERNET request, std::string_view fallback) {
    DWORD bytes = 0;
    WinHttpQueryOption(request, WINHTTP_OPTION_URL, nullptr, &bytes);
    if (bytes < sizeof(wchar_t)) {
        return std::string(fallback);
    }
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t));
    if (!WinHttpQueryOption(request, WINHTTP_OPTION_URL, buffer.data(), &bytes)) {
        return std::string(fallback);
    }
    const std::size_t length =
        bytes / sizeof(wchar_t) - (buffer[bytes / sizeof(wchar_t) - 1u] == L'\0' ? 1u : 0u);
    const std::string converted = wide_to_utf8(std::wstring_view(buffer.data(), length));
    return converted.empty() ? std::string(fallback) : converted;
}
#endif

DifficultyTableHttpResponse default_fetch(std::string_view raw_url, std::size_t max_bytes) {
    DifficultyTableHttpResponse response;
    const std::string url = strip_fragment(trim_copy(raw_url));
    response.final_url = url;
    if (!is_http_url(url)) {
        response.error = "Difficulty-table link must use http:// or https://.";
        return response;
    }
#ifndef _WIN32
    response.error = "Difficulty-table link fetching is available only on Windows.";
    return response;
#else
    const std::wstring wide_url = utf8_to_wide(url);
    if (wide_url.empty()) {
        response.error = "Difficulty-table URL is not valid UTF-8.";
        return response;
    }

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &components)) {
        response.error = "Could not parse difficulty-table URL.";
        return response;
    }
    if (components.nScheme != INTERNET_SCHEME_HTTP &&
        components.nScheme != INTERNET_SCHEME_HTTPS) {
        response.error = "Difficulty-table link must use http:// or https://.";
        return response;
    }

    const std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path;
    if (components.dwUrlPathLength > 0) {
        path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    } else {
        path = L"/";
    }
    if (components.dwExtraInfoLength > 0) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }

    WinHttpHandle session(WinHttpOpen(L"TenRiff/1.2 DifficultyTable",
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS,
                                      0));
    if (!session.value) {
        response.error = "Could not initialize HTTP client.";
        return response;
    }
    WinHttpSetTimeouts(session.value, 5000, 5000, 10000, 30000);

    WinHttpHandle connection(WinHttpConnect(
        session.value, host.c_str(), components.nPort, 0));
    if (!connection.value) {
        response.error = "Could not connect to difficulty-table host.";
        return response;
    }

    const DWORD request_flags =
        components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    const wchar_t* accept_types[] = {L"text/html", L"application/json", L"*/*", nullptr};
    WinHttpHandle request(WinHttpOpenRequest(connection.value,
                                             L"GET",
                                             path.c_str(),
                                             nullptr,
                                             WINHTTP_NO_REFERER,
                                             accept_types,
                                             request_flags));
    if (!request.value) {
        response.error = "Could not create difficulty-table HTTP request.";
        return response;
    }
#ifdef WINHTTP_OPTION_DECOMPRESSION
    DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(
        request.value, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));
#endif
    if (!WinHttpSendRequest(request.value,
                            WINHTTP_NO_ADDITIONAL_HEADERS,
                            0,
                            WINHTTP_NO_REQUEST_DATA,
                            0,
                            0,
                            0) ||
        !WinHttpReceiveResponse(request.value, nullptr)) {
        response.error = "Difficulty-table HTTP request failed.";
        return response;
    }

    DWORD status_code = 0;
    DWORD status_bytes = sizeof(status_code);
    if (!WinHttpQueryHeaders(request.value,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,
                             &status_code,
                             &status_bytes,
                             WINHTTP_NO_HEADER_INDEX)) {
        response.error = "Could not read difficulty-table HTTP status.";
        return response;
    }
    response.status_code = static_cast<int>(status_code);
    response.final_url = winhttp_final_url(request.value, url);

    std::array<char, 64u * 1024u> buffer{};
    while (true) {
        DWORD read = 0;
        if (!WinHttpReadData(
                request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
            response.error = "Could not read difficulty-table HTTP response.";
            return response;
        }
        if (read == 0) {
            break;
        }
        if (static_cast<std::size_t>(read) > max_bytes ||
            response.body.size() > max_bytes - static_cast<std::size_t>(read)) {
            response.body.clear();
            response.error = "Difficulty-table download exceeds its safety size limit.";
            return response;
        }
        response.body.append(buffer.data(), static_cast<std::size_t>(read));
    }
    return response;
#endif
}

bool fetch_document(const DifficultyTableFetchFunction& fetch,
                    std::string_view url,
                    std::size_t max_bytes,
                    DifficultyTableHttpResponse& response,
                    std::string& error) {
    response = fetch(url, max_bytes);
    if (!response.error.empty()) {
        error = response.error;
        return false;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        error = "Difficulty-table server returned HTTP " +
                std::to_string(response.status_code) + ".";
        return false;
    }
    if (response.body.size() > max_bytes) {
        error = "Difficulty-table download exceeds its safety size limit.";
        return false;
    }
    if (response.final_url.empty()) {
        response.final_url = std::string(url);
    }
    if (!is_http_url(response.final_url)) {
        error = "Difficulty-table redirect resolved outside HTTP(S).";
        return false;
    }
    return true;
}

config::JsonParseResult parse_downloaded_json(std::string body) {
    if (body.size() >= 3u &&
        static_cast<unsigned char>(body[0]) == 0xefu &&
        static_cast<unsigned char>(body[1]) == 0xbbu &&
        static_cast<unsigned char>(body[2]) == 0xbfu) {
        body.erase(0, 3u);
    }
    return config::parse_json(body);
}

}  // namespace

std::optional<std::string> extract_bmstable_header_url(std::string_view html) {
    const std::string lower = lower_ascii(std::string(html));
    std::size_t cursor = 0;
    while ((cursor = lower.find("<meta", cursor)) != std::string::npos) {
        const std::size_t tag_end = lower.find('>', cursor + 5u);
        if (tag_end == std::string::npos) {
            return std::nullopt;
        }
        const auto attributes = parse_html_attributes(
            html.substr(cursor, tag_end - cursor + 1u));
        const auto name = attributes.find("name");
        const auto content = attributes.find("content");
        if (name != attributes.end() && content != attributes.end() &&
            lower_ascii(trim_copy(name->second)) == "bmstable") {
            const std::string value = trim_copy(content->second);
            if (!value.empty()) {
                return value;
            }
        }
        cursor = tag_end + 1u;
    }
    return std::nullopt;
}

std::optional<std::string> resolve_difficulty_table_url(
    std::string_view base_url,
    std::string_view reference) {
    const auto base = parse_http_url(base_url);
    if (!base.has_value()) {
        return std::nullopt;
    }
    std::string ref = strip_fragment(trim_copy(reference));
    if (ref.empty()) {
        return base->scheme + "://" + base->authority + base->path + base->query;
    }
    if (is_http_url(ref)) {
        const auto parsed = parse_http_url(ref);
        return parsed.has_value() ? std::optional<std::string>(ref) : std::nullopt;
    }
    if (ref.rfind("//", 0) == 0) {
        const std::string absolute = base->scheme + ":" + ref;
        return parse_http_url(absolute).has_value()
                   ? std::optional<std::string>(absolute)
                   : std::nullopt;
    }
    if (ref.front() == '?') {
        return base->scheme + "://" + base->authority + base->path + ref;
    }

    std::string query;
    const std::size_t query_begin = ref.find('?');
    if (query_begin != std::string::npos) {
        query = ref.substr(query_begin);
        ref.erase(query_begin);
    }
    std::string combined_path;
    if (!ref.empty() && ref.front() == '/') {
        combined_path = ref;
    } else {
        const std::size_t slash = base->path.rfind('/');
        combined_path =
            (slash == std::string::npos ? std::string("/") : base->path.substr(0, slash + 1u)) +
            ref;
    }
    return base->scheme + "://" + base->authority +
           normalize_url_path(combined_path) + query;
}

DifficultyTableLinkImportResult import_difficulty_table_link(
    std::string_view raw_source_url,
    const std::filesystem::path& cache_root,
    DifficultyTableFetchFunction fetch) {
    DifficultyTableLinkImportResult result;
    const std::string source_url = strip_fragment(trim_copy(raw_source_url));
    if (!parse_http_url(source_url).has_value()) {
        result.error = "Difficulty-table link must use a valid http:// or https:// URL.";
        return result;
    }
    if (cache_root.empty()) {
        result.error = "Difficulty-table cache path is empty.";
        return result;
    }
    if (!fetch) {
        fetch = default_fetch;
    }

    DifficultyTableHttpResponse source_response;
    if (!fetch_document(fetch, source_url, kMaxHtmlBytes, source_response, result.error)) {
        return result;
    }

    std::string header_url = source_response.final_url;
    config::JsonParseResult header_json = parse_downloaded_json(source_response.body);
    const config::JsonObject* header_object =
        header_json.root.has_value() ? header_json.root->as_object() : nullptr;
    DifficultyTableHttpResponse header_response = source_response;
    if (!header_object) {
        if (header_json.root.has_value() && header_json.root->is_array()) {
            result.error =
                "Difficulty-table data-array URLs do not include a table name or symbol; use the HTML page or header JSON link.";
            return result;
        }
        const auto header_reference = extract_bmstable_header_url(source_response.body);
        if (!header_reference.has_value()) {
            result.error =
                "The HTML page does not contain a BMSTable meta header link.";
            return result;
        }
        const auto resolved_header =
            resolve_difficulty_table_url(source_response.final_url, *header_reference);
        if (!resolved_header.has_value()) {
            result.error = "Could not resolve the BMSTable header URL.";
            return result;
        }
        header_url = *resolved_header;
        if (!fetch_document(fetch, header_url, kMaxHtmlBytes, header_response, result.error)) {
            return result;
        }
        header_json = parse_downloaded_json(header_response.body);
        header_object =
            header_json.root.has_value() ? header_json.root->as_object() : nullptr;
        if (!header_object) {
            result.error = "BMSTable header link did not return a JSON object.";
            return result;
        }
    }

    const auto name = json_string_field(*header_object, "name");
    const auto symbol = json_string_field(*header_object, "symbol");
    const auto data_reference = json_string_field(*header_object, "data_url");
    if (!name.has_value() || name->empty() ||
        !symbol.has_value() || symbol->empty() ||
        !data_reference.has_value() || data_reference->empty()) {
        result.error =
            "Difficulty-table header requires non-empty name, symbol, and data_url strings.";
        return result;
    }
    const auto data_url =
        resolve_difficulty_table_url(header_response.final_url, *data_reference);
    if (!data_url.has_value()) {
        result.error = "Could not resolve the difficulty-table data URL.";
        return result;
    }

    DifficultyTableHttpResponse data_response;
    if (!fetch_document(fetch, *data_url, kMaxJsonBytes, data_response, result.error)) {
        return result;
    }
    config::JsonParseResult data_json = parse_downloaded_json(data_response.body);
    if (!data_json.success() || !data_json.root->is_array()) {
        result.error = "Difficulty-table data URL did not return a standard JSON array.";
        return result;
    }

    config::JsonObject cached_header = *header_object;
    cached_header.insert_or_assign("data_url", config::JsonValue{"data.json"});
    const std::string cached_header_json =
        config::json_stringify(config::JsonValue{std::move(cached_header)}, 2);
    const std::string cached_data_json =
        config::json_stringify(*data_json.root, 0);
    const std::filesystem::path table_cache =
        cache_root / cache_key_for_url(source_url);
    const std::filesystem::path header_path = table_cache / "header.json";
    const std::filesystem::path data_path = table_cache / "data.json";
    if (!write_cache_file(data_path, cached_data_json, result.error) ||
        !write_cache_file(header_path, cached_header_json, result.error)) {
        return result;
    }

    const DifficultyTableLoadResult loaded = load_difficulty_table(header_path);
    if (!loaded.success()) {
        result.error = "Cached difficulty table failed validation: " + loaded.error;
        return result;
    }
    result.cached_header_path = header_path.u8string();
    result.source_url = source_url;
    result.table_name = loaded.table.name();
    result.warnings = loaded.warnings;
    return result;
}

}  // namespace tenriff::app
