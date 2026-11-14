#include "network/HostedPeerSession.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

namespace {
using namespace tenriff::network;
using namespace std::chrono_literals;
struct Handle {
    HINTERNET value;
    explicit Handle(HINTERNET h) : value(h) {}
    ~Handle() { if (value) WinHttpCloseHandle(value); }
};

// The transport cannot access any non-loopback address. These are synthetic
// accounts understood by the task-local Worker bridge, never production keys.
HostedHttpTransport loopback_transport(uint16_t port) {
    return [port](std::string_view method, std::string_view path, std::string_view bearer,
                  std::string_view body, HostedHttpResponse& response) {
        Handle session(WinHttpOpen(L"TenRiff/hosted-local-smoke", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.value || !WinHttpSetTimeouts(session.value, 1000, 1000, 1000, 3000)) return false;
        Handle connection(WinHttpConnect(session.value, L"127.0.0.1", port, 0));
        if (!connection.value) return false;
        const std::wstring verb(method.begin(), method.end()), endpoint(path.begin(), path.end());
        Handle request(WinHttpOpenRequest(connection.value, verb.c_str(), endpoint.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
        if (!request.value) return false;
        DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects))) return false;
        std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer ";
        headers.append(bearer.begin(), bearer.end()); headers += L"\r\n";
        if (!WinHttpSendRequest(request.value, headers.c_str(), static_cast<DWORD>(headers.size()),
            body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) ||
            !WinHttpReceiveResponse(request.value, nullptr)) return false;
        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) return false;
        response = {}; response.status = status;
        for (;;) {
            char buffer[4096]; DWORD count = 0;
            if (!WinHttpReadData(request.value, buffer, sizeof(buffer), &count)) return false;
            if (!count) return true;
            if (response.body.size() + count > 65536) return false;
            response.body.append(buffer, count);
        }
    };
}

void require(bool ok, const char* label) {
    if (!ok) throw std::runtime_error(label);
    std::cout << "PASS " << label << '\n';
}
bool wait_for(const std::function<bool()>& test, std::chrono::milliseconds timeout = 12s) {
    const auto end = std::chrono::steady_clock::now() + timeout;
    do { if (test()) return true; std::this_thread::sleep_for(10ms); }
    while (std::chrono::steady_clock::now() < end);
    return test();
}
}

int main(int argc, char** argv) {
    unsigned value = 0;
    if (argc != 2) { std::cerr << "usage: tenriff_hosted_multiplayer_smoke <loopback-port>\n"; return 2; }
    const std::string argument = argv[1];
    const auto parsed = std::from_chars(argument.data(), argument.data() + argument.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != argument.data() + argument.size() || !value || value > 65535) return 2;
    HostedPeerSession alice(loopback_transport(static_cast<uint16_t>(value)));
    HostedPeerSession bob(loopback_transport(static_cast<uint16_t>(value)));
    try {
        require(alice.configure("tr_" + std::string(64, 'a')), "configure account A");
        require(bob.configure("tr_" + std::string(64, 'b')), "configure account B");
        require(alice.host(), "queue create");
        require(wait_for([&] { return alice.snapshot().state == PeerSessionState::Connected; }), "create authenticated room");
        require(bob.browse(), "browse authenticated rooms");
        require(wait_for([&] { return !bob.rooms().empty(); }), "discover room");
        require(bob.join(bob.rooms().front().id), "queue join");
        require(wait_for([&] { return alice.snapshot().participant_count == 2 && bob.snapshot().participant_count == 2; }), "two clients joined");
        require(alice.send_chat("Hello 안녕하세요"), "queue Korean and ASCII chat A to B");
        require(wait_for([&] {
            const auto messages = bob.snapshot().chat_messages;
            return !messages.empty() && messages.back().text == "Hello 안녕하세요";
        }), "deliver authenticated room chat A to B");
        require(bob.send_chat("Reply 확인"), "queue Korean and ASCII chat B to A");
        require(wait_for([&] {
            const auto messages = alice.snapshot().chat_messages;
            return messages.size() >= 2 && messages.back().text == "Reply 확인";
        }), "deliver authenticated room chat B to A");
        const ChartFingerprint chart{18446744073709551614ULL, 8000};
        require(alice.set_local_chart(chart, "Synthetic contract smoke"), "select chart");
        require(wait_for([&] { return bob.snapshot().selected_chart.fingerprint.hash == chart.hash; }), "deliver exact uint64 chart identity");
        require(bob.set_local_chart(chart, "Synthetic contract smoke"), "acknowledge local chart");
        require(wait_for([&] { return bob.snapshot().local_chart.fingerprint.hash == chart.hash; }), "chart acknowledgment accepted");
        require(alice.set_rate(1500), "change shared rate");
        require(wait_for([&] { return alice.snapshot().rate_milli == 1500 && bob.snapshot().rate_milli == 1500; }), "synchronize rate");
        require(alice.set_ready(true) && bob.set_ready(true), "both ready");
        require(wait_for([&] { return alice.snapshot().can_start; }), "all-ready barrier");
        require(alice.send_launch(), "queue launch");
        require(wait_for([&] { return alice.snapshot().round_active && bob.snapshot().round_active; }), "load barrier");
        require(alice.poll_launch() == chart.hash && bob.poll_launch() == chart.hash, "launch fingerprint contract");
        require(alice.mark_loaded() && bob.mark_loaded(), "both loaded");
        require(wait_for([&] { return alice.wait_for_peer_loaded(1ms) && bob.wait_for_peer_loaded(1ms); }), "server acknowledged all loads");
        require(alice.send_begin(), "request scheduled start");
        uint32_t alice_delay = 0, bob_delay = 0;
        std::chrono::steady_clock::time_point alice_begin, bob_begin;
        require(wait_for([&] {
            if (!alice_delay && alice.wait_for_begin(1ms, alice_delay)) alice_begin = std::chrono::steady_clock::now() + std::chrono::milliseconds(alice_delay);
            if (!bob_delay && bob.wait_for_begin(1ms, bob_delay)) bob_begin = std::chrono::steady_clock::now() + std::chrono::milliseconds(bob_delay);
            return alice_delay && bob_delay;
        }), "both clients consume server scheduled start");
        const auto skew = std::chrono::duration_cast<std::chrono::milliseconds>(alice_begin - bob_begin).count();
        require(std::abs(skew) <= 250, "local start estimates within 250ms");
        std::this_thread::sleep_until(std::max(alice_begin, bob_begin) + 100ms);
        PeerScore score;
        score.current_sample = -48000;
        score.gauge_milli = 100000;
        require(alice.publish_score(score, false), "signed intro sample accepted");
        require(wait_for([&] { return bob.snapshot().has_remote_score && bob.snapshot().latest_remote_score.current_sample == -48000; }), "signed score delivered");
        score.score = 20; score.current_sample = 48000; score.perfect = 10; score.combo = 10; score.max_combo = 10;
        require(alice.publish_score(score, true), "account A final score");
        score.score = 19; score.perfect = 9; score.great = 1;
        require(bob.publish_score(score, true), "account B final score");
        require(wait_for([&] { return alice.snapshot().all_remote_finished && bob.snapshot().all_remote_finished; }), "complete results delivered");
        alice.reset_round(); bob.reset_round();
        require(wait_for([&] { return !alice.snapshot().round_active && !bob.snapshot().round_active; }), "round reset barrier");
        require(bob.snapshot().local_is_leader, "selection leadership rotates");
        alice.disconnect();
        require(wait_for([&] { return bob.snapshot().state == PeerSessionState::Connected && bob.snapshot().participant_count == 1; }), "creator departure preserves room");
        require(bob.snapshot().round_participants.size() == 2, "finished result participants retained");
        bob.disconnect();
        std::cout << "PASS hosted multiplayer C++/Worker contract smoke\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        std::cerr << "A: " << alice.snapshot().status_detail << '\n';
        std::cerr << "B: " << bob.snapshot().status_detail << '\n';
        return 1;
    }
}
