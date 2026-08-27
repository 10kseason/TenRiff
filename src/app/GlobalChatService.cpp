#include "app/GlobalChatService.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace tenriff::app {
namespace {

void clear_sensitive(std::string& value) {
    std::fill(value.begin(), value.end(), '\0');
    value.clear();
}

}  // namespace

GlobalChatService::~GlobalChatService() {
    shutdown();
}

void GlobalChatService::ensure_worker() {
    if (!worker_.joinable()) worker_ = std::thread([this] { worker_main(); });
}

void GlobalChatService::configure(std::string base_url, std::string bearer_token) {
    std::lock_guard<std::mutex> lock(mutex_);
    ensure_worker();
    base_url_ = std::move(base_url);
    clear_sensitive(bearer_token_);
    bearer_token_ = std::move(bearer_token);
    configured_ = !base_url_.empty() && !bearer_token_.empty();
    online_ = false;
    error_.clear();
    pending_messages_.clear();
    messages_.clear();
    room_error_.clear();
    rooms_.clear();
    last_message_id_ = 0;
    ++revision_;
    wake_.notify_all();
}

void GlobalChatService::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    configured_ = false;
    online_ = false;
    sending_ = false;
    base_url_.clear();
    clear_sensitive(bearer_token_);
    error_.clear();
    pending_messages_.clear();
    messages_.clear();
    room_error_.clear();
    rooms_.clear();
    last_message_id_ = 0;
    ++revision_;
    wake_.notify_all();
}

bool GlobalChatService::send(std::string text) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!configured_ || text.empty() || text.size() > 256 ||
        pending_messages_.size() >= 8) {
        return false;
    }
    pending_messages_.push_back(std::move(text));
    sending_ = true;
    ++revision_;
    wake_.notify_all();
    return true;
}

GlobalChatSnapshot GlobalChatService::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    GlobalChatSnapshot output;
    output.configured = configured_;
    output.online = online_;
    output.sending = sending_;
    output.error = error_;
    output.messages = messages_;
    output.room_error = room_error_;
    output.rooms = rooms_;
    output.revision = revision_;
    return output;
}

void GlobalChatService::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        wake_.notify_all();
    }
    if (worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    clear_sensitive(bearer_token_);
    pending_messages_.clear();
    configured_ = false;
}

void GlobalChatService::worker_main() {
    auto next_poll = std::chrono::steady_clock::now();
    for (;;) {
        std::string base_url;
        std::string token;
        std::string outgoing;
        std::int64_t after_id = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait_until(lock, next_poll, [this] {
                return stopping_ ||
                       (configured_ && !pending_messages_.empty());
            });
            if (stopping_) return;
            if (!configured_) {
                next_poll = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                continue;
            }
            base_url = base_url_;
            token = bearer_token_;
            after_id = last_message_id_;
            if (!pending_messages_.empty()) {
                outgoing = std::move(pending_messages_.front());
                pending_messages_.pop_front();
            }
        }

        std::string send_error;
        const bool sent = outgoing.empty() ||
                          send_ranked_global_chat(base_url, token, outgoing, send_error);
        std::vector<RankedGlobalChatMessage> incoming;
        std::string fetch_error;
        const bool fetched = sent && fetch_ranked_global_chat(
                                      base_url, token, after_id, incoming, fetch_error);
        std::vector<RankedMultiplayerRoom> rooms;
        std::string room_error;
        const bool rooms_fetched = sent && fetch_ranked_multiplayer_rooms(
                                             base_url, token, rooms, room_error);
        clear_sensitive(token);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (base_url != base_url_) continue;
            sending_ = !pending_messages_.empty();
            if (!sent) {
                error_ = std::move(send_error);
                online_ = false;
            } else if (!fetched) {
                error_ = std::move(fetch_error);
                online_ = false;
            } else {
                error_.clear();
                online_ = true;
                for (auto& message : incoming) {
                    if (message.id <= last_message_id_) continue;
                    last_message_id_ = message.id;
                    messages_.push_back(std::move(message));
                }
                if (messages_.size() > 100) {
                    messages_.erase(messages_.begin(),
                                    messages_.begin() +
                                        static_cast<std::ptrdiff_t>(messages_.size() - 100));
                }
            }
            if (rooms_fetched) {
                room_error_.clear();
                rooms_ = std::move(rooms);
            } else {
                room_error_ = std::move(room_error);
                rooms_.clear();
            }
            ++revision_;
        }
        next_poll = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    }
}

}  // namespace tenriff::app
