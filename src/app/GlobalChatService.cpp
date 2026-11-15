#include "app/GlobalChatService.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <random>
#include <utility>

#include "config/SimpleJson.h"

namespace tenriff::app {
namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
using Json = config::JsonValue;
using Object = config::JsonObject;
constexpr std::int64_t kMaxExactInteger = 9007199254740991LL;

void clear_sensitive(std::string& value) {
    volatile char* data = value.empty() ? nullptr : value.data();
    for (std::size_t i = 0; i < value.size(); ++i) data[i] = 0;
    value.clear();
}
bool token_valid(std::string_view value, std::string_view prefix) {
    return value.size() == prefix.size() + 64 && value.substr(0, prefix.size()) == prefix &&
        std::all_of(value.begin() + static_cast<std::ptrdiff_t>(prefix.size()), value.end(),
            [](char ch) { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); });
}
const Json& field(const Json& value, const char* key) {
    static const Json empty;
    const auto* object = value.as_object();
    if (!object) return empty;
    const auto it = object->find(key);
    return it == object->end() ? empty : it->second;
}
bool integer(const Json& value, std::int64_t minimum, std::int64_t& output) {
    const double n = value.as_number(-1);
    if (!value.is_number() || !std::isfinite(n) || n < minimum ||
        n > static_cast<double>(kMaxExactInteger) || std::floor(n) != n) return false;
    output = static_cast<std::int64_t>(n); return true;
}
bool protocol(const Json& value) {
    return field(value, "protocol").is_number() && field(value, "protocol").as_number() == 2 &&
        field(value, "score_version").as_string() == "ex-v1";
}
bool whitespace(std::uint32_t cp) {
    return cp == 0x20 || cp == 0xa0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200a) ||
        cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000 || cp == 0xfeff;
}
// Validate before displaying server text or sending input: replacement decoding
// could conceal invalid/control bytes, and must not change the identity of text.
bool valid_text(std::string_view text, std::size_t limit) {
    if (text.empty() || text.size() > limit) return false;
    bool visible = false;
    for (std::size_t i = 0; i < text.size();) {
        auto cp = static_cast<unsigned char>(text[i++]);
        std::uint32_t value = cp, minimum = 0;
        unsigned count = 0;
        if (cp >= 0xc2 && cp <= 0xdf) { value = cp & 0x1f; count = 1; minimum = 0x80; }
        else if (cp >= 0xe0 && cp <= 0xef) { value = cp & 0x0f; count = 2; minimum = 0x800; }
        else if (cp >= 0xf0 && cp <= 0xf4) { value = cp & 7; count = 3; minimum = 0x10000; }
        else if (cp >= 0x80) return false;
        if (i + count > text.size()) return false;
        for (unsigned j = 0; j < count; ++j) {
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80) return false;
            value = (value << 6) | (next & 0x3f);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ||
            value < 0x20 || value == 0x7f) return false;
        visible |= !whitespace(value);
    }
    return visible;
}
std::string request_id() {
    std::array<unsigned char, 16> data{};
    try { std::random_device random; for (auto& byte : data) byte = static_cast<unsigned char>(random()); }
    catch (...) { return {}; }
    data[6] = static_cast<unsigned char>((data[6] & 15) | 0x40);
    data[8] = static_cast<unsigned char>((data[8] & 63) | 0x80);
    constexpr char hex[] = "0123456789abcdef";
    std::string id;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) id += '-';
        id += hex[data[i] >> 4]; id += hex[data[i] & 15];
    }
    return id;
}
bool parse_messages(const Json& reply, std::int64_t after,
                    std::vector<RankedGlobalChatMessage>& messages) {
    std::int64_t received = 0, server = 0;
    const auto* list = field(reply, "messages").as_array();
    if (!protocol(reply) || !integer(field(reply, "server_received_ms"), 1, received) ||
        !integer(field(reply, "server_time_ms"), received, server) || !list || list->size() > 100) return false;
    for (const auto& item : *list) {
        RankedGlobalChatMessage message;
        std::int64_t created = 0;
        if (!item.is_object() || !integer(field(item, "id"), after + 1, message.id) ||
            !integer(field(item, "created_at_ms"), 1, created) || created > server ||
            !field(item, "name").is_string() || !field(item, "text").is_string()) return false;
        message.username = field(item, "name").as_string();
        message.text = field(item, "text").as_string();
        if (!valid_text(message.username, 96) || !valid_text(message.text, 256)) return false;
        after = message.id;
        messages.push_back(std::move(message));
    }
    return true;
}
}  // namespace

GlobalChatService::GlobalChatService(network::HostedHttpTransport transport)
    : transport_(transport ? std::move(transport) : network::make_hosted_http_transport()) {}
GlobalChatService::~GlobalChatService() { shutdown(); }

void GlobalChatService::configure(std::string upload_key) {
    std::lock_guard lock(mutex_);
    if (stopping_ || (configured_ && !blocked_ && upload_key_ == upload_key)) {
        clear_sensitive(upload_key); return;
    }
    clear_sensitive(upload_key_);
    configured_ = token_valid(upload_key, "tr_");
    if (configured_) upload_key_ = std::move(upload_key);
    clear_sensitive(upload_key);
    blocked_ = false; online_ = false; sending_ = false;
    error_ = configured_ ? "" : "Connect your account to use global chat.";
    pending_messages_.clear(); messages_.clear(); last_message_id_ = 0;
    ++generation_; ++revision_;
    if (configured_ && !worker_.joinable()) worker_ = std::thread([this] { worker_main(); });
    wake_.notify_all();
}
void GlobalChatService::clear() {
    std::lock_guard lock(mutex_);
    configured_ = online_ = sending_ = blocked_ = false;
    clear_sensitive(upload_key_); error_.clear();
    pending_messages_.clear(); messages_.clear(); last_message_id_ = 0;
    ++generation_; ++revision_; wake_.notify_all();
}
bool GlobalChatService::send(std::string text) {
    std::lock_guard lock(mutex_);
    if (!configured_ || !online_ || blocked_ || stopping_ ||
        !valid_text(text, 256) || pending_messages_.size() >= 8) return false;
    pending_messages_.push_back(std::move(text)); sending_ = true;
    ++revision_; wake_.notify_all(); return true;
}
GlobalChatSnapshot GlobalChatService::snapshot() const {
    std::lock_guard lock(mutex_);
    return {configured_, online_, sending_, error_, messages_, revision_};
}
void GlobalChatService::shutdown() {
    { std::lock_guard lock(mutex_); stopping_ = true; ++generation_; wake_.notify_all(); }
    if (worker_.joinable()) worker_.join();
    clear();
}

void GlobalChatService::worker_main() {
    std::uint64_t active_generation = 0;
    std::string session;
    auto renew_at = Clock::time_point{}, session_deadline = Clock::time_point{};
    auto next_poll = Clock::time_point{}, next_send = Clock::time_point{};
    const auto current = [&](std::uint64_t generation) {
        std::lock_guard lock(mutex_);
        return !stopping_ && configured_ && !blocked_ && generation_ == generation;
    };
    const auto fail = [&](std::uint64_t generation, std::string error, bool terminal) {
        std::lock_guard lock(mutex_);
        if (stopping_ || generation_ != generation) return;
        online_ = false; sending_ = false; blocked_ = terminal;
        pending_messages_.clear(); error_ = std::move(error); ++revision_;
    };
    const auto request = [&](std::string_view path, std::string_view token, const std::string& body,
                             Json& reply, unsigned& status) {
        network::HostedHttpResponse response;
        bool ok = false;
        try { ok = transport_("POST", path, token, body, response); } catch (...) { ok = false; }
        status = response.status;
        if (!ok || response.body.size() > 64 * 1024) return false;
        auto parsed = config::parse_json(response.body);
        if (!parsed.success() || !parsed.root->is_object()) return false;
        reply = std::move(*parsed.root); return true;
    };
    for (;;) {
        std::string key;
        std::string outgoing;
        std::int64_t after_id = 0;
        {
            std::unique_lock lock(mutex_);
            for (;;) {
                if (stopping_) { clear_sensitive(session); return; }
                if (!configured_ || blocked_) {
                    clear_sensitive(session);
                    wake_.wait(lock, [this] { return stopping_ || (configured_ && !blocked_); });
                    continue;
                }
                if (generation_ != active_generation) {
                    active_generation = generation_; clear_sensitive(session);
                    renew_at = next_poll = next_send = Clock::time_point{};
                }
                const auto now = Clock::now();
                if (session.empty() || now >= renew_at) { key = upload_key_; break; }
                if ((!pending_messages_.empty() && now >= next_send) || now >= next_poll) {
                    after_id = last_message_id_;
                    if (!pending_messages_.empty() && now >= next_send) {
                        outgoing = std::move(pending_messages_.front()); pending_messages_.pop_front();
                    }
                    break;
                }
                const auto deadline = std::min({next_poll, renew_at,
                    pending_messages_.empty() ? next_poll : next_send});
                // An unpredicated wait also notices newly queued sends.
                wake_.wait_until(lock, deadline);
            }
        }
        if (!key.empty()) {
            const auto before = Clock::now();
            Json reply; unsigned status = 0;
            const bool ok = request("/api/chat/session", key,
                "{\"protocol\":2,\"score_version\":\"ex-v1\",\"purpose\":\"chat\"}", reply, status);
            clear_sensitive(key);
            if (!current(active_generation)) continue;
            std::int64_t expires = 0, server = 0;
            std::string token = field(reply, "session_token").as_string();
            if (!ok || status != 200 || !protocol(reply) || !token_valid(token, "mp_") ||
                !integer(field(reply, "server_time_ms"), 1, server) ||
                !integer(field(reply, "expires_at_ms"), server, expires) ||
                expires - server <= 1000 || expires - server > 30000) {
                clear_sensitive(token);
                fail(active_generation, "Global chat authentication failed. Reconnect your account.", true); continue;
            }
            clear_sensitive(session); session = std::move(token);
            session_deadline = before + std::chrono::milliseconds(expires - server);
            renew_at = std::min(Clock::now() + 20s, session_deadline - 1s);
            next_poll = Clock::time_point{};
            continue;
        }
        const auto id = request_id();
        if (id.empty() || Clock::now() >= session_deadline) {
            fail(active_generation, "Global chat session expired. Reconnect your account.", true); continue;
        }
        Object payload{{"protocol", Json(2.0)}, {"score_version", Json("ex-v1")},
            {"request_id", Json(id)}, {"action", Json(outgoing.empty() ? "poll" : "send")},
            {"after_id", Json(static_cast<double>(after_id))}};
        if (!outgoing.empty()) payload["text"] = Json(outgoing);
        const auto body = config::json_stringify(Json(std::move(payload)), 0, 17);
        Json reply; unsigned status = 0;
        bool ok = request("/api/chat", session, body, reply, status);
        // Retry only uncertain transport delivery, with the same idempotency ID.
        if (!ok && current(active_generation) && Clock::now() < session_deadline)
            ok = request("/api/chat", session, body, reply, status);
        if (!current(active_generation)) continue;
        next_poll = Clock::now() + 1s;
        if (!outgoing.empty()) next_send = Clock::now() + 1s;
        if (!ok || status != 200) {
            const bool terminal = (!ok && status == 200) || status == 401 || status == 403 || status == 404 || status == 426;
            fail(active_generation, terminal ? "Global chat is unavailable. Reconnect your account." :
                "Global chat request failed. Unconfirmed messages are not sent again.", terminal);
            continue;
        }
        std::vector<RankedGlobalChatMessage> incoming;
        if (!parse_messages(reply, after_id, incoming)) {
            fail(active_generation, "Invalid global chat response. Reconnect your account.", true); continue;
        }
        {
            std::lock_guard lock(mutex_);
            if (stopping_ || generation_ != active_generation) continue;
            online_ = true; sending_ = !pending_messages_.empty(); error_.clear();
            for (auto& message : incoming) {
                last_message_id_ = message.id; messages_.push_back(std::move(message));
            }
            if (messages_.size() > 100) messages_.erase(messages_.begin(), messages_.end() - 100);
            ++revision_;
        }
    }
}
}  // namespace tenriff::app
