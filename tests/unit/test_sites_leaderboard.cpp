#include "doctest/doctest.h"
#include <chrono>
#include <atomic>
#include <future>
#include <fstream>
#include <iterator>
#include <iostream>
#include <limits>
#include <map>
#include "app/ChartFileHash.h"
#include "chart/BmsParser.h"
#include "app/SitesLeaderboardClient.h"
#include "app/SitesUploadResponse.h"
#include "app/ReplayVerifier.h"
#include "config/SimpleJson.h"
using namespace tenriff;
TEST_CASE("Sites upload failures distinguish compatibility authentication and retry without reflecting remote bodies") {
    const std::string untrusted = R"({"error":"private-response-marker","accepted":true})";
    struct Case { unsigned status; bool retry; const char* reason; };
    for (const auto test : {Case{400, false, "ruleset"}, Case{401, false, "connection key"},
                           Case{403, false, "Access"}, Case{404, false, "endpoint"},
                           Case{429, true, "rate limited"}, Case{503, true, "temporarily unavailable"},
                           Case{302, false, "did not accept"}}) {
        bool retryable = !test.retry;
        std::string error = "previous error";
        CHECK_FALSE(app::parse_sites_upload_response(test.status, untrusted, retryable, error));
        CHECK(retryable == test.retry);
        CHECK(error.find(std::to_string(test.status)) != std::string::npos);
        CHECK(error.find(test.reason) != std::string::npos);
        CHECK(error.find("private-response-marker") == std::string::npos);
        if (test.status == 400) CHECK(error.find("connection key") == std::string::npos);
    }
}

TEST_CASE("Sites accepts only acknowledged 200 and 201 responses and clears stale retry state") {
    for (const unsigned status : {200u, 201u}) {
        bool retryable = true;
        std::string error = "previous error";
        CHECK(app::parse_sites_upload_response(status, R"({"accepted":true,"duplicate":true})", retryable, error));
        CHECK_FALSE(retryable);
        CHECK(error.empty());
        for (const auto body : {"invalid", "{}", "[]", R"({"accepted":false})", R"({"accepted":"true"})"}) {
            CHECK_FALSE(app::parse_sites_upload_response(status, body, retryable, error));
            CHECK_FALSE(retryable);
            CHECK(error == "Sites did not acknowledge the record.");
        }
    }
}

TEST_CASE("Sites future clock errors use only the allowlisted code and never reflect server text") {
    bool retryable = true;
    std::string error;
    REQUIRE_FALSE(app::parse_sites_upload_response(400,
        R"({"error_code":"future_play_time","error":"private-response-marker"})", retryable, error));
    CHECK_FALSE(retryable);
    CHECK(error.find("PC clock") != std::string::npos);
    CHECK(error.find("private-response-marker") == std::string::npos);
    REQUIRE_FALSE(app::parse_sites_upload_response(400,
        R"({"error_code":"private-response-marker","error":"future_play_time"})", retryable, error));
    CHECK(error.find("PC clock") == std::string::npos);
    CHECK(error.find("private-response-marker") == std::string::npos);
}

TEST_CASE("Sites late asynchronous messages cannot replace a different result status") {
    const app::SitesUploadMessage completed{std::string(64, 'a'), "Sites community record uploaded."};
    CHECK(completed.applies_to_result(std::string(64, 'a')));
    CHECK_FALSE(completed.applies_to_result(std::string(64, 'b')));
    CHECK_FALSE(completed.applies_to_result(""));
    CHECK_FALSE(app::SitesUploadMessage{}.applies_to_result(""));
    CHECK(app::sites_upload_status_text(completed.text).korean == "웹 기록 등록 완료");
    CHECK(app::sites_upload_status_text(
        "Sites upload skipped: older or unsupported judgement timing remains local.").english ==
        "Older or unsupported judgement timing stays in local records.");
    for (const unsigned status : {400u, 401u, 403u, 404u, 429u}) {
        bool retryable = false;
        std::string error;
        REQUIRE_FALSE(app::parse_sites_upload_response(status, R"({"error":"private-response-marker"})", retryable, error));
        const auto summary = app::sites_upload_status_text(error);
        CHECK(summary.english.find(std::to_string(status)) != std::string_view::npos);
        CHECK(summary.korean.find(std::to_string(status)) != std::string_view::npos);
        CHECK(summary.english.find("private-response-marker") == std::string_view::npos);
    }
}

namespace {
bool json_values_equal(const config::JsonValue& lhs, const config::JsonValue& rhs) {
    if (lhs.value.index() != rhs.value.index()) return false;
    if (lhs.is_object()) {
        const auto& left = *lhs.as_object();
        const auto& right = *rhs.as_object();
        if (left.size() != right.size()) return false;
        for (const auto& field : left) {
            const auto found = right.find(field.first);
            if (found == right.end() || !json_values_equal(field.second, found->second)) return false;
        }
        return true;
    }
    if (lhs.is_array()) {
        const auto& left = *lhs.as_array();
        const auto& right = *rhs.as_array();
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i)
            if (!json_values_equal(left[i], right[i])) return false;
        return true;
    }
    if (lhs.is_string()) return lhs.as_string() == rhs.as_string();
    if (lhs.is_number()) return lhs.as_number() == rhs.as_number();
    if (lhs.is_bool()) return lhs.as_bool() == rhs.as_bool();
    return lhs.is_null();
}

gameplay::ReplayFile score_fixture() {
    gameplay::ReplayFile replay;
    replay.record_play_mode = "online";
    replay.ruleset_id = std::string(app::kCanonicalReplayRulesetId);
    replay.chart_format = "bms";
    replay.bms_rank = 3;
    replay.chart_sha256 = std::string(64, 'b');
    replay.created_utc = "20260913_000000Z";
    replay.trace.lane_count = 10;
    replay.final_score = 9900;
    replay.stats.total_notes = 100;
    replay.stats.counts.pg = 98;
    replay.stats.counts.gr = 2;
    replay.stats.max_combo = 100;
    replay.stats.detail_score = 496;
    replay.stats.detailed_accuracy_points = 98.7654321;
    replay.stats.detailed_accuracy_weight = 100.0;
    return replay;
}

struct SitesChartFixture {
    std::filesystem::path directory = std::filesystem::temp_directory_path() /
        ("tenriff_sites_branch_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path path = directory / "branch.bms";
    SitesChartFixture() { std::filesystem::create_directories(directory); }
    ~SitesChartFixture() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        std::filesystem::remove(directory, ec);
    }
    gameplay::ReplayFile write(std::string_view content) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        REQUIRE(output.good());
        output.close();
        auto replay = score_fixture();
        replay.chart_path = path.u8string();
        replay.chart_sha256 = app::sha256_bytes(content);
        replay.mode.bms_control_flow_version = 2;
        replay.mode.bms_random_seed = 0u;
        return replay;
    }
};
}  // namespace

TEST_CASE("Sites never promotes guest historical or unknown origins when online is later enabled") {
    auto replay = score_fixture();
    app::SitesLeaderboardService service;
    service.set_online_enabled(true);
    for (const std::string origin : {"", "guest", "unknown", "ONLINE"}) {
        replay.record_play_mode = origin;
        std::string payload = "must clear", error;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
        CHECK(payload.empty());
        CHECK(error.find("remain local permanently") != std::string::npos);
    }
    replay.record_play_mode = "online";
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    CHECK(payload.find("record_play_mode") == std::string::npos);
}

namespace tenriff::app {
struct SitesLeaderboardServiceTestAccess {
    struct Transport {
        std::atomic_int calls{0};
        std::promise<void> entered;
        std::shared_future<void> release;
    };
    inline static Transport* transport = nullptr;
    static bool submit(const std::string&, const std::string&, const std::string&, bool&, std::string&) {
        auto& active = *transport;
        if (active.calls.fetch_add(1) == 0) active.entered.set_value();
        if (active.release.valid()) active.release.wait_for(std::chrono::seconds(3));
        return true;
    }
    static bool attempt(SitesLeaderboardService& service, std::uint64_t generation) {
        bool retryable = true;
        std::string error;
        return service.submit_if_current(generation, {}, "synthetic payload", retryable, error);
    }
    static void queue_worker_record(SitesLeaderboardService& service, const std::filesystem::path& path,
                                    const std::string& hash) {
        std::lock_guard<std::mutex> lock(service.mutex_);
        service.pending_.push_back({path, hash, "Fixture", "CLEAR", {}});
        if (!service.worker_.joinable()) service.worker_ = std::thread(&SitesLeaderboardService::worker_main, &service);
        service.wake_.notify_all();
    }
    static void check_permission_barrier() {
        Transport state;
        transport = &state;
        SitesLeaderboardService service(submit);
        // No connection file or credentials are read while guest mode owns the service.
        CHECK(service.enqueue({}, {}, {}, {}) == SitesLeaderboardService::EnqueueResult::Disabled);
        CHECK_FALSE(attempt(service, 0));
        CHECK(state.calls == 0);
        service.set_online_enabled(true);
        std::promise<void> release;
        state.release = release.get_future().share();
        auto entered = state.entered.get_future();
        auto submission = std::async(std::launch::async, [&] { return attempt(service, 0); });
        CHECK(entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
        // Queued work is discarded even if online mode is enabled again later.
        {
            std::lock_guard<std::mutex> lock(service.mutex_);
            service.pending_.push_back({});
        }
        std::promise<void> changing;
        auto changing_started = changing.get_future();
        auto transition = std::async(std::launch::async, [&] {
            changing.set_value();
            service.set_online_enabled(false);
        });
        changing_started.wait();
        CHECK(transition.wait_for(std::chrono::milliseconds(10)) == std::future_status::timeout);
        release.set_value();
        CHECK(submission.get());
        transition.get();
        CHECK(service.pending_.empty());
        CHECK_FALSE(attempt(service, service.generation_));
        CHECK(service.enqueue({}, {}, {}, {}) == SitesLeaderboardService::EnqueueResult::Disabled);
        service.set_online_enabled(true);
        CHECK_FALSE(attempt(service, 0));
        CHECK(state.calls == 1);
        CHECK(attempt(service, service.generation_));
        CHECK(state.calls == 2);
        service.shutdown();
        transport = nullptr;
    }
};
}  // namespace tenriff::app

TEST_CASE("Sites guest transition fences in-flight submissions and invalidates every pending generation") {
    app::SitesLeaderboardServiceTestAccess::check_permission_barrier();
}

TEST_CASE("Sites worker rejects persisted guest history before loading connection credentials") {
    using Access = app::SitesLeaderboardServiceTestAccess;
    SitesChartFixture fixture;
    const auto path = fixture.directory / "replay.json";
    auto replay = score_fixture();
    replay.replay_format_version = 0;
    Access::Transport transport;
    Access::transport = &transport;
    app::SitesLeaderboardService service(Access::submit);
    service.set_online_enabled(true);
    for (const std::string origin : {"guest", ""}) {
        replay.record_play_mode = origin;
        REQUIRE(gameplay::save_replay_json(path.u8string(), replay).success());
        const auto hashes = app::hash_chart_file(path);
        REQUIRE(hashes.valid());
        Access::queue_worker_record(service, path, hashes.sha256);
        std::vector<app::SitesUploadMessage> messages;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        do {
            messages = service.take_messages();
            if (!messages.empty()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        CHECK(messages.size() == 1);
        if (!messages.empty()) CHECK(messages.front().text == "Sites upload skipped: guest or historical records remain local permanently.");
        CHECK(transport.calls == 0);
        service.set_online_enabled(false);
        service.set_online_enabled(true);
    }
    service.shutdown();
    Access::transport = nullptr;
    std::filesystem::remove(path);
}

TEST_CASE("Sites worker rejects persisted older and unsupported timing before chart or connection reads") {
    using Access = app::SitesLeaderboardServiceTestAccess;
    SitesChartFixture fixture;
    const auto path = fixture.directory / "replay.json";
    Access::Transport transport;
    Access::transport = &transport;
    app::SitesLeaderboardService service(Access::submit);
    service.set_online_enabled(true);
    struct Case { std::string_view ruleset; std::vector<std::string> mods; };
    const Case cases[] = {
        {app::kLegacyReplayRulesetId, {}}, {app::kPreviousReplayRulesetId, {}},
        {app::kRuleset3ReplayRulesetId, {}}, {app::kRuleset4ReplayRulesetId, {}},
        {app::kRuleset5ReplayRulesetId, {}}, {app::kRuleset6ReplayRulesetId, {"judge_ex"}},
        {app::kRuleset7ReplayRulesetId, {}}, {app::kRuleset8ReplayRulesetId, {"judge_ex"}},
        {"", {}}, {"custom-ruleset-9", {}}, {"custom-ruleset-10", {"judge_ex"}},
        {app::kCanonicalReplayRulesetId, {"Judge Easy"}},
        {app::kCanonicalReplayRulesetId, {"judge_hard", "judge_easy"}},
        {app::kExReplayRulesetId, {"judge_ex", "judge_hard"}},
    };
    for (const auto& test : cases) {
        auto replay = score_fixture();
        replay.ruleset_id = test.ruleset;
        replay.mods = test.mods;
        replay.sample_rate = replay.trace.sample_rate = 48000;
        replay.trace.duration_samples = 48000;
        if (test.ruleset.empty()) replay.replay_format_version = 0;
        replay.chart_path = (fixture.directory / "missing-branch.bms").u8string();
        replay.mode.bms_control_flow_version = 2;
        replay.mode.bms_random_seed = 0u;
        REQUIRE(gameplay::save_replay_json(path.u8string(), replay).success());
        const auto hashes = app::hash_chart_file(path);
        REQUIRE(hashes.valid());
        Access::queue_worker_record(service, path, hashes.sha256);
        std::vector<app::SitesUploadMessage> messages;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        do {
            messages = service.take_messages();
            if (!messages.empty()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        CHECK(messages.size() == 1);
        if (!messages.empty()) CHECK(messages.front().text ==
            "Sites upload skipped: older or unsupported judgement timing remains local.");
        CHECK(transport.calls == 0);
    }
    service.shutdown();
    Access::transport = nullptr;
    std::filesystem::remove(path);
}

#ifdef _WIN32
TEST_CASE("Ranked rejects guest and historical replay files before any HTTP request") {
    SitesChartFixture fixture;
    const auto path = fixture.directory / "replay.json";
    auto replay = score_fixture();
    replay.replay_format_version = 0;
    app::RankedPlayAuthorization authorization{"synthetic-token", "synthetic-id", "synthetic-nonce", "fixture"};
    for (const std::string origin : {"", "guest", "unknown"}) {
        replay.record_play_mode = origin;
        REQUIRE(gameplay::save_replay_json(path.u8string(), replay).success());
        std::string receipt, error;
        CHECK_FALSE(app::submit_ranked_replay("not-a-server", authorization, path, receipt, error));
        CHECK(receipt.empty());
        // This exact preflight failure precedes URL handling and credential use.
        CHECK(error == "Ranked upload skipped: guest or historical records remain local permanently.");
    }
    std::filesystem::remove(path);
}

TEST_CASE("Ranked rejects fixed timing identities before URL handling even for online osu records") {
    SitesChartFixture fixture;
    const auto path = fixture.directory / "replay.json";
    auto replay = score_fixture();
    replay.replay_format_version = 0;
    replay.chart_format = "osu";
    replay.bms_rank.reset();
    app::RankedPlayAuthorization authorization{"synthetic-token", "synthetic-id", "synthetic-nonce", "fixture"};
    for (const auto id : {app::kCanonicalReplayRulesetId, app::kExReplayRulesetId,
                          std::string_view("custom-ruleset-9"), std::string_view("custom-ruleset-10")}) {
        replay.ruleset_id = id;
        REQUIRE(gameplay::save_replay_json(path.u8string(), replay).success());
        std::string receipt, error;
        CHECK_FALSE(app::submit_ranked_replay("not-a-server", authorization, path, receipt, error));
        CHECK(receipt.empty());
        CHECK(error.find("timing baseline") != std::string::npos);
    }
    std::filesystem::remove(path);
}
#endif

TEST_CASE("Sites payload preserves all four native metrics without display rounding") {
    const auto replay = score_fixture();
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", payload, error));
    const auto json = config::parse_json(payload);
    REQUIRE(json.success());
    const auto* object = json.root->as_object();
    REQUIRE(object != nullptr);
    CHECK(object->at("schema_version").as_number() == 1);
    CHECK(object->at("ruleset_id").as_string() == app::kCanonicalReplayRulesetId);
    CHECK(object->at("score").as_number() == replay.final_score);
    CHECK(object->at("detail_score").as_number() == replay.stats.detail_score);
    CHECK(object->at("accuracy").as_number() == replay.stats.accuracy_percent());
    CHECK(object->at("detailed_accuracy").as_number() == replay.stats.detailed_accuracy_percent());
    CHECK(object->at("accuracy").as_number() != object->at("detailed_accuracy").as_number());
    CHECK(payload.find("\"detail_score\":496,") != std::string::npos);
    CHECK(object->at("timing_profile").as_string() == "bms-easy");
}

TEST_CASE("1.8.9.4 Sites optional chart author adds metadata without changing replay or score identity") {
    const auto replay = score_fixture();
    std::string old_payload, payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", old_payload, error));
    const std::string author = u8"譜面作者 \"패터너\"";
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", payload, error, author));
    // Public synthetic fixture for native/web payload-contract checks.
    std::cout << "[chart-author-payload] " << payload << '\n';
    const auto old_json = config::parse_json(old_payload);
    const auto new_json = config::parse_json(payload);
    REQUIRE(old_json.success());
    REQUIRE(new_json.success());
    const auto& old_fields = *old_json.root->as_object();
    auto new_fields = *new_json.root->as_object();
    REQUIRE(new_fields.count("chart_author") == 1);
    CHECK(new_fields.at("chart_author").as_string() == author);
    CHECK(old_fields.count("chart_author") == 0);
    new_fields.erase("chart_author");
    // Object iteration order is not a wire contract; every existing field's
    // type and exact value (including binary64 metrics and array order) is.
    CHECK(json_values_equal(config::JsonValue{new_fields}, config::JsonValue{old_fields}));
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", payload, error, ""));
    CHECK(payload == old_payload);
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", payload, error, std::string(480, 'a')));
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "FULLCOMBO", payload, error, std::string(481, 'a')));
    CHECK(payload.empty());
    CHECK_FALSE(error.empty());
}

TEST_CASE("Sites payload preserves active judge modifiers without relabeling normal records") {
    for (const std::string mod : {"", "judge_hard"}) {
        auto replay = score_fixture();
        if (!mod.empty()) replay.mods = {mod};
        std::string payload, error;
        REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
        const auto json = config::parse_json(payload);
        REQUIRE(json.success());
        const auto* root = json.root->as_object();
        REQUIRE(root);
        const auto* mods = root->at("mods").as_array();
        REQUIRE(mods);
        CHECK(mods->size() == (mod.empty() ? 0u : 1u));
        if (!mod.empty()) CHECK(mods->front().as_string() == mod);
        CHECK(root->at("ruleset_id").as_string() == app::kCanonicalReplayRulesetId);
        CHECK(root->at("timing_profile").as_string() == "bms-easy");
    }
}

TEST_CASE("Sites keeps musical LN Mix versions out of unversioned legacy boards") {
    auto replay = score_fixture();
    replay.mods = {"ln_mix_30"};
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    for (int version : {0, 2, 3}) {
        replay.mode.long_note_mix_version = version;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
        CHECK(payload.empty());
        CHECK(error.find("separate leaderboard condition") != std::string::npos);
        CHECK(error.find("LN Mix v" + std::to_string(version)) != std::string::npos);
        CHECK(app::sites_upload_status_text(error).english == "Long LN Mix uses local records only.");
    }
    replay.mods.clear();
    replay.mode.long_note_mix_version = 2;
    CHECK(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
}

TEST_CASE("Sites BMS policy metadata preserves legacy payload and accepts distinct supported conditions") {
    for (int timing : {1, 2}) for (int gauge : {1, 2}) for (int control : {1, 2}) {
        auto replay = score_fixture();
        replay.mode.bms_timing_version = timing;
        replay.mode.bms_gauge_version = gauge;
        replay.mode.bms_control_flow_version = control;
        if (control == 2) replay.mode.bms_random_seed = 0u;
        const std::string branch = control == 2 ? std::string(64, 'a') : "";
        std::string payload, error;
        REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error, {}, branch));
        const auto json = config::parse_json(payload);
        REQUIRE(json.success());
        const auto& fields = *json.root->as_object();
        const bool nonlegacy = timing != 1 || gauge != 1 || control != 1;
        CHECK(fields.count("bms_timing_version") == (nonlegacy ? 1u : 0u));
        CHECK(fields.count("bms_gauge_version") == (nonlegacy ? 1u : 0u));
        CHECK(fields.count("bms_control_flow_version") == (nonlegacy ? 1u : 0u));
        CHECK(fields.count("bms_random_seed") == (control == 2 ? 1u : 0u));
        CHECK(fields.count("bms_branch_sha256") == (control == 2 ? 1u : 0u));
        if (nonlegacy) {
            CHECK(fields.at("bms_timing_version").as_number() == timing);
            CHECK(fields.at("bms_gauge_version").as_number() == gauge);
            CHECK(fields.at("bms_control_flow_version").as_number() == control);
        }
        if (control == 2) {
            CHECK(fields.at("bms_random_seed").as_number() == 0.0);
            CHECK(fields.at("bms_branch_sha256").as_string() == branch);
        }
        if (timing == 2 && gauge == 2 && control == 2) {
            std::cout << "[bms-policy-payload] " << payload << '\n';
            replay.mode.bms_random_seed = 0xffffffffu;
            REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error, {}, branch));
            CHECK(payload.find("\"bms_random_seed\":4294967295") != std::string::npos);
        }
    }
}

TEST_CASE("Sites rejects unknown and inconsistent BMS policy seed and identity metadata") {
    for (int which = 0; which < 3; ++which) for (int version : {-1, 0, 3, 999}) {
        auto replay = score_fixture();
        if (which == 0) replay.mode.bms_timing_version = version;
        if (which == 1) replay.mode.bms_gauge_version = version;
        if (which == 2) replay.mode.bms_control_flow_version = version;
        std::string payload = "old", error;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
        CHECK(payload.empty());
        CHECK(error.find("metadata") != std::string::npos);
    }
    for (int scenario = 0; scenario < 8; ++scenario) {
        auto replay = score_fixture();
        std::string branch;
        if (scenario == 0) replay.mode.bms_random_seed = 0u;
        if (scenario == 1) branch = std::string(64, 'a');
        if (scenario >= 2 && scenario <= 5) {
            replay.mode.bms_control_flow_version = 2;
            if (scenario != 2) replay.mode.bms_random_seed = 0u;
            if (scenario == 2) branch = std::string(64, 'a');
            if (scenario == 4) branch = std::string(64, 'A');
            if (scenario == 5) branch = std::string(63, 'a');
        }
        if (scenario >= 6) {
            replay.chart_format = "osu";
            if (scenario == 6) replay.mode.bms_gauge_version = 2;
            if (scenario == 7) replay.mode.bms_timing_version = 2;
        }
        std::string payload = "old", error;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error, {}, branch));
        CHECK(payload.empty());
        CHECK(error.find("metadata") != std::string::npos);
    }
}

TEST_CASE("Sites BMS branch identity groups selected content instead of independent seed values") {
    SitesChartFixture fixture;
    const std::string content = "#TITLE Branch fixture\n#BPM 120\n#RANDOM 2\n"
        "#IF 1\n#00111:01\n#ELSE\n#00112:01\n#ENDIF\n#ENDRANDOM\n";
    auto replay = fixture.write(content);
    std::map<std::string, int> hashes;
    for (uint32_t seed = 0; seed < 32; ++seed) {
        replay.mode.bms_random_seed = seed;
        replay.mode.random_seed = static_cast<int>(seed + 1000u);
        std::string hash, error;
        REQUIRE(app::prepare_sites_bms_branch_identity(replay, hash, error));
        REQUIRE(hash.size() == 64u);
        ++hashes[hash];
        chart::BmsParserOptions options;
        options.random_seed = seed;
        options.capture_active_text = true;
        const auto parsed = chart::BmsParser{}.parse(content, options);
        CHECK(hash == app::sha256_bytes(std::string(app::kSitesBmsBranchIdentityDomain) + parsed.active_text));
        CHECK(hash != app::sha256_bytes(parsed.active_text));
        replay.mode.random_seed = -1;
        std::string repeated;
        REQUIRE(app::prepare_sites_bms_branch_identity(replay, repeated, error));
        CHECK(repeated == hash);
    }
    REQUIRE(hashes.size() == 2u);
    for (const auto& hash : hashes) CHECK(hash.second > 1);
    replay.mode.bms_random_seed = 0xffffffffu;
    std::string hash, error;
    REQUIRE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    CHECK(hashes.count(hash) == 1u);

    // Different control alternatives containing identical active directives
    // (even differently commented or indented) belong to the same board.
    replay = fixture.write("#BPM 120\n#RANDOM 2\n#IF 1\n #00111:01 ; first\n"
        "#ELSE\n\t#00111:01 ; second\n\n#ENDIF\n#ENDRANDOM\n");
    std::string first;
    for (uint32_t seed = 0; seed < 32; ++seed) {
        replay.mode.bms_random_seed = seed;
        REQUIRE(app::prepare_sites_bms_branch_identity(replay, hash, error));
        if (first.empty()) first = hash;
        CHECK(hash == first);
    }
}

TEST_CASE("Sites BMS branch identity requires exact bounded original chart evidence") {
    SitesChartFixture fixture;
    const std::string content = "#BPM 120\n#RANDOM 1\n#IF 1\n#00111:01\n#ENDIF\n";
    auto replay = fixture.write(content);
    std::string hash, error;
    REQUIRE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    auto changed = fixture.write(content + "; changed original bytes\n");
    CHECK_FALSE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    CHECK(hash.empty());
    CHECK(error.find("no longer matches") != std::string::npos);
    CHECK(error.find(fixture.path.u8string()) == std::string::npos);
    CHECK(app::sites_upload_status_text(error).korean.find("원본 BMS") != std::string_view::npos);
    replay = changed;
    REQUIRE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    replay.chart_path += ".missing";
    CHECK_FALSE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    CHECK(hash.empty());
    replay = fixture.write("#BPM 120\n#00111:01\n");
    CHECK_FALSE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    CHECK(error.find("metadata") != std::string::npos);
    replay = fixture.write("#BPM invalid\n#RANDOM 1\n#00111:01\n");
    CHECK_FALSE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    replay = fixture.write(content);
    std::filesystem::resize_file(fixture.path, 32u * 1024u * 1024u + 1u);
    CHECK_FALSE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    CHECK(hash.empty());
    CHECK(error.find("too large") != std::string::npos);
}

TEST_CASE("Sites BMS legacy and nonbranch policies require no chart read but malformed policies fail") {
    auto replay = score_fixture();
    replay.chart_path = "definitely_missing_sites_branch.bms";
    std::string hash = "old", error = "old";
    REQUIRE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    CHECK(hash.empty());
    CHECK(error.empty());
    replay.mode.bms_timing_version = 2;
    replay.mode.bms_gauge_version = 2;
    REQUIRE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    CHECK(hash.empty());
    replay.mode.bms_control_flow_version = 2;
    CHECK_FALSE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    CHECK(error.find("metadata") != std::string::npos);
    replay.mode.bms_random_seed = 0u;
    replay.chart_format = "osu";
    CHECK_FALSE(app::prepare_sites_bms_branch_identity(replay, hash, error));
    CHECK(error.find("metadata") != std::string::npos);
}

TEST_CASE("Sites R9 R10 preserve empty POOR accuracy and distinct fixed timing payload identities") {
    for (const bool ex : {false, true}) {
        auto replay = score_fixture();
        replay.ruleset_id = std::string(ex ? app::kExReplayRulesetId : app::kCanonicalReplayRulesetId);
        replay.mods = ex ? std::vector<std::string>{"judge_ex"} : std::vector<std::string>{};
        replay.stats.counts.pr = 3;
        replay.stats.accuracy_points = 99.6;
        replay.stats.accuracy_weight = 100.0;
        replay.mode.bms_timing_version = 2;
        replay.mode.bms_gauge_version = 2;
        replay.mode.bms_control_flow_version = 2;
        replay.mode.bms_random_seed = 99550;
        std::string payload, error;
        REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Empty POOR Fixture", "CLEAR",
                                           payload, error, {}, std::string(64, 'd')));
        const auto parsed = config::parse_json(payload);
        REQUIRE(parsed.success());
        const auto* object = parsed.root->as_object();
        REQUIRE(object);
        CHECK(object->at("ruleset_id").as_string() == replay.ruleset_id);
        CHECK(object->at("accuracy").as_number() == doctest::Approx(99.6));
        CHECK(object->at("bms_control_flow_version").as_number() == 2.0);
        std::cout << (ex ? "[fixed-r10-payload] " : "[fixed-r9-payload] ") << payload << '\n';
        replay.mods = ex ? std::vector<std::string>{} : std::vector<std::string>{"judge_ex"};
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Empty POOR Fixture", "CLEAR",
                                               payload, error, {}, std::string(64, 'd')));
    }
}

TEST_CASE("Sites accepts only the fixed Normal Hard and EX policies for new uploads") {
    const char* profiles[] = {"bms-veryhard", "bms-hard", "bms-normal", "bms-easy"};
    for (const std::string mod : {"", "judge_hard", "judge_ex"}) {
        auto replay = score_fixture();
        replay.ruleset_id = std::string(mod == "judge_ex" ? app::kExReplayRulesetId : app::kCanonicalReplayRulesetId);
        if (!mod.empty()) replay.mods = {mod};
        CHECK(app::is_sites_supported_replay_ruleset(replay.ruleset_id, replay.mods));
        for (int rank : {0, 1, 2, 3}) {
            replay.bms_rank = rank;
            std::string payload, error;
            REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixed baseline", "CLEAR", payload, error));
            const auto json = config::parse_json(payload);
            REQUIRE(json.success());
            CHECK(json.root->as_object()->at("ruleset_id").as_string() == replay.ruleset_id);
            CHECK(json.root->as_object()->at("timing_profile").as_string() == profiles[rank]);
            CHECK(error.empty());
        }
        replay.chart_format = "osu";
        replay.bms_rank.reset();
        std::string payload, error;
        REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixed osu baseline", "CLEAR", payload, error));
        const auto json = config::parse_json(payload);
        REQUIRE(json.success());
        CHECK(json.root->as_object()->at("ruleset_id").as_string() == replay.ruleset_id);
        CHECK(json.root->as_object()->at("timing_profile").as_string() == "osu-fixed");
    }
}

TEST_CASE("Sites keeps every released older ruleset local without removing replay support") {
    for (const auto ruleset : {app::kLegacyReplayRulesetId, app::kPreviousReplayRulesetId,
            app::kRuleset3ReplayRulesetId, app::kRuleset4ReplayRulesetId, app::kRuleset5ReplayRulesetId,
            app::kRuleset6ReplayRulesetId, app::kRuleset7ReplayRulesetId, app::kRuleset8ReplayRulesetId}) {
        auto replay = score_fixture();
        replay.ruleset_id = ruleset;
        if (ruleset == app::kRuleset6ReplayRulesetId || ruleset == app::kRuleset8ReplayRulesetId)
            replay.mods = {"judge_ex"};
        CHECK(app::is_supported_canonical_replay_ruleset(ruleset));
        CHECK_FALSE(app::is_sites_supported_replay_ruleset(ruleset, replay.mods));
        std::string payload = "stale payload", error;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Older online record", "CLEAR", payload, error));
        CHECK(payload.empty());
        CHECK(error == "Sites upload skipped: older or unsupported judgement timing remains local.");
        CHECK(replay.ruleset_id == ruleset);
    }
}

TEST_CASE("Sites rejects unmarked custom hidden Easy and ambiguous judge claims") {
    for (const std::string ruleset : {"", "unknown", "custom-ruleset-9", "custom-ruleset-10"}) {
        auto replay = score_fixture();
        replay.ruleset_id = ruleset;
        std::string payload = "stale payload", error;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Nonstandard", "CLEAR", payload, error));
        CHECK(payload.empty());
    }
    for (const std::vector<std::string> mods : {
            std::vector<std::string>{"judge_easy"}, {"Judge Easy"}, {"JUDGE-EASY"},
            {"Judge Hard"}, {"JUDGE-HARD"}, {"judgehard"}, {"Judge EX"}, {"JUDGE-EX"},
            {"judge_easy", "judge_hard"}, {"judge_hard", "judge_easy"},
            {"judge_ex", "judge_hard"}, {"judge_hard", "judge_ex"},
            {"judge_hard", "Judge Hard"}, {"judge_ex", "Judge EX"}, {"unknown_mod"}}) {
        for (const auto ruleset : {app::kCanonicalReplayRulesetId, app::kExReplayRulesetId}) {
            auto replay = score_fixture();
            replay.ruleset_id = ruleset;
            replay.mods = mods;
            std::string payload = "stale payload", error;
            CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Nonstandard", "CLEAR", payload, error));
            CHECK(payload.empty());
            CHECK(error == "Sites upload skipped: older or unsupported judgement timing remains local.");
        }
    }
    CHECK(app::is_sites_supported_replay_ruleset(app::kCanonicalReplayRulesetId, {"judge_hard"}));
    CHECK(app::is_sites_supported_replay_ruleset(app::kExReplayRulesetId, {"judge_ex"}));
    CHECK(app::is_current_canonical_replay_ruleset(app::kCanonicalReplayRulesetId, {"Judge Hard"}));
    CHECK(app::is_current_canonical_replay_ruleset(app::kExReplayRulesetId, {"Judge EX"}));
}

TEST_CASE("Sites fixed generation preserves assisted course pause and evidence exclusions") {
    for (const bool ex : {false, true}) {
        for (int scenario = 0; scenario < 9; ++scenario) {
            auto replay = score_fixture();
            if (ex) {
                replay.ruleset_id = app::kExReplayRulesetId;
                replay.mods = {"judge_ex"};
            }
            switch (scenario) {
                case 0: replay.aborted = true; break;
                case 1: replay.pause_used = true; break;
                case 2: replay.mode.autoplay_enabled = true; break;
                case 3: replay.mode.practice_no_fail_enabled = true; break;
                case 4: replay.mode.one_miss_fail_enabled = true; break;
                case 5: replay.mode.course_gauge = "course"; break;
                case 6: replay.mods.push_back("note_add_10"); break;
                case 7: replay.mods.push_back("auto_scratch"); break;
                case 8: replay.replay_format_version = 0; break;
            }
            std::string payload = "stale payload", error;
            CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Excluded", "CLEAR", payload, error));
            CHECK(payload.empty());
            CHECK(error == "Sites upload skipped: this play is not eligible for the community leaderboard.");
        }
    }
}

TEST_CASE("Sites EX payload requires its own R10 policy and excludes mixed judge modes") {
    auto replay = score_fixture();
    replay.ruleset_id = std::string(app::kExReplayRulesetId);
    replay.mods = {"judge_ex"};
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "EX Fixture", "CLEAR", payload, error));
    const auto json = config::parse_json(payload);
    REQUIRE(json.success());
    const auto* root = json.root->as_object();
    REQUIRE(root);
    CHECK(root->at("ruleset_id").as_string() == app::kExReplayRulesetId);
    CHECK(root->at("mods").as_array()->front().as_string() == "judge_ex");
    CHECK(root->at("timing_profile").as_string() == "bms-easy");
    // Synthetic public fixture: exercise the native serializer in web schema QA.
    std::cout << "[judge-ex-payload] " << payload << '\n';
    replay.ruleset_id = std::string(app::kCanonicalReplayRulesetId);
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "EX Fixture", "CLEAR", payload, error));
    replay.ruleset_id = std::string(app::kExReplayRulesetId);
    for (const std::vector<std::string> mods : {std::vector<std::string>{},
            std::vector<std::string>{"judge_hard"}, std::vector<std::string>{"judge_ex", "judge_hard"},
            std::vector<std::string>{"judge_ex", "judge_easy"}}) {
        replay.mods = mods;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "EX Fixture", "CLEAR", payload, error));
        CHECK(payload.empty());
    }
}

TEST_CASE("Sites fixed baseline payload retains every authored BMS rank and osu timing profile") {
    const char* profiles[] = {"bms-veryhard", "bms-hard", "bms-normal", "bms-easy"};
    for (int rank = 0; rank <= 3; ++rank) {
        auto replay = score_fixture();
        replay.bms_rank = rank;
        std::string payload, error;
        REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
        const auto json = config::parse_json(payload);
        REQUIRE(json.success());
        REQUIRE(json.root->as_object());
        CHECK(json.root->as_object()->at("timing_profile").as_string() == profiles[rank]);
    }
    auto replay = score_fixture();
    replay.chart_format = "osu";
    replay.bms_rank.reset();
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    const auto json = config::parse_json(payload);
    REQUIRE(json.success());
    CHECK(json.root->as_object()->at("timing_profile").as_string() == "osu-fixed");
}

TEST_CASE("Sites fixed baseline payload fails closed on missing or invalid timing metadata") {
    const auto rejects = [](const gameplay::ReplayFile& replay) {
        std::string payload = "stale payload", error;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
        CHECK(payload.empty());
        CHECK_FALSE(error.empty());
    };
    auto replay = score_fixture();
    replay.bms_rank.reset();
    rejects(replay);
    for (const int rank : {-1, 4, (std::numeric_limits<int>::max)()}) {
        replay = score_fixture();
        replay.bms_rank = rank;
        rejects(replay);
    }
    for (const std::string format : {"", "unknown", "BMS", "bme"}) {
        replay = score_fixture();
        replay.chart_format = format;
        rejects(replay);
    }
    replay = score_fixture();
    replay.chart_format = "osu";
    replay.bms_rank = -1;
    rejects(replay);
    for (const auto ruleset : {app::kLegacyReplayRulesetId, app::kPreviousReplayRulesetId, app::kRuleset3ReplayRulesetId, app::kRuleset4ReplayRulesetId}) {
        replay = score_fixture();
        replay.ruleset_id = std::string(ruleset);
        rejects(replay);
    }
}

TEST_CASE("Sites payload rejects malformed detailed metrics instead of uploading a fallback percentage") {
    const double invalid_numbers[] = {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -1.0
    };
    for (const auto bad : invalid_numbers) {
        for (int field = 0; field < 4; ++field) {
            auto replay = score_fixture();
            double* values[] = {&replay.stats.accuracy_points, &replay.stats.accuracy_weight,
                               &replay.stats.detailed_accuracy_points, &replay.stats.detailed_accuracy_weight};
            *values[field] = bad;
            std::string payload = "stale", error;
            CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
            CHECK(payload.empty());
            CHECK_FALSE(error.empty());
        }
    }
    for (const int64_t bad : {-1LL, 1'000'000'001LL}) {
        auto replay = score_fixture();
        replay.stats.detail_score = bad;
        std::string payload, error;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    }
    auto replay = score_fixture();
    replay.stats.counts.pg = std::numeric_limits<int>::max();
    replay.stats.counts.gr = std::numeric_limits<int>::max();
    std::string payload, error;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    replay = score_fixture();
    replay.stats.counts.pg = -1;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    replay = score_fixture();
    replay.stats.detailed_accuracy_points = 101.0;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    replay = score_fixture();
    replay.stats.accuracy_points = 1.0;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
}

TEST_CASE("Sites connection accepts only an HTTPS Site origin and header-safe upload key") {
    app::SitesLeaderboardConnection connection;
    std::string error;
    const std::string token = "tr_" + std::string(64, 'a');
    const auto config = [&](const std::string& origin) {
        return "{\"schema_version\":1,\"site_url\":\"" + origin + "\",\"upload_token\":\"" + token + "\"}";
    };
    CHECK(app::parse_sites_leaderboard_connection(config("https://tenriff.test.chatgpt.site"), connection, error));
    CHECK_FALSE(app::parse_sites_leaderboard_connection(config("http://tenriff.test.chatgpt.site"), connection, error));
    CHECK_FALSE(app::parse_sites_leaderboard_connection(config("https://tenriff.test.chatgpt.site.evil.test"), connection, error));
    CHECK_FALSE(app::parse_sites_leaderboard_connection(config("https://tenriff.test.chatgpt.site/path"), connection, error));
    CHECK_FALSE(app::parse_sites_leaderboard_connection(config("https://user@tenriff.test.chatgpt.site"), connection, error));
    CHECK_FALSE(app::parse_sites_leaderboard_connection("{}", connection, error));
}
TEST_CASE("Sites score payload omits private evidence and rejects assisted or noncanonical play") {
    gameplay::ReplayFile replay;
    replay.record_play_mode = "online";
    replay.ruleset_id = std::string(app::kCanonicalReplayRulesetId);
    replay.chart_format = "bms";
    replay.bms_rank = 3;
    replay.chart_sha256 = std::string(64, 'b');
    replay.chart_path = "C:/private/music/song.bms";
    replay.created_utc = "20260913_000000Z";
    replay.trace.lane_count = 10;
    replay.final_score = 950000;
    replay.stats.total_notes = 100;
    replay.stats.counts.pg = 100;
    replay.stats.max_combo = 100;
    std::string payload, error;
    REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "Song \"Title\"", "FULLCOMBO", payload, error));
    REQUIRE(config::parse_json(payload).success());
    CHECK(payload.find("2026-09-13T00:00:00Z") != std::string::npos);
    CHECK(payload.find("private") == std::string::npos);
    CHECK(payload.find("online_verified") == std::string::npos);
    CHECK(payload.find("\"key_mode\":\"10K\"") != std::string::npos);
    replay.mode.autoplay_enabled = true;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Song", "CLEAR", payload, error));
    replay.mode.autoplay_enabled = false;
    replay.pause_used = true;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Song", "CLEAR", payload, error));
    replay.pause_used = false;
    replay.ruleset_id = "custom";
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Song", "CLEAR", payload, error));
}

#ifdef _WIN32
TEST_CASE("Sites in-game import protects keys and keeps a valid connection after rejected input") {
    const auto directory = std::filesystem::temp_directory_path() /
        ("tenriff_sites_import_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            for (const auto* file : {"sites-leaderboard.json", "sites-leaderboard.dpapi", "sites-leaderboard.dpapi.tmp"})
                std::filesystem::remove(path / file, ec);
            std::filesystem::remove(path, ec);
        }
    } cleanup{directory};
    std::filesystem::create_directories(directory);
    const std::string first_key = "tr_" + std::string(64, 'a');
    const std::string second_key = "tr_" + std::string(64, 'b');
    const std::string json = "{\"schema_version\":1,\"site_url\":\"https://tenriff.test.chatgpt.site\",\"upload_token\":\"" + first_key + "\"}";
    std::ofstream(directory / "sites-leaderboard.json") << json;
    app::SitesLeaderboardConnection connection;
    std::string error;
    bool missing = false;
    REQUIRE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK_FALSE(missing);
    CHECK(connection.upload_token == first_key);
    REQUIRE(app::install_sites_leaderboard_connection(directory, json, error));
    CHECK_FALSE(std::filesystem::exists(directory / "sites-leaderboard.json"));
    std::ifstream stored(directory / "sites-leaderboard.dpapi", std::ios::binary);
    const std::string encrypted((std::istreambuf_iterator<char>(stored)), {});
    stored.close();
    CHECK(encrypted.find(first_key) == std::string::npos);
    REQUIRE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK(connection.upload_token == first_key);
    CHECK_FALSE(app::install_sites_leaderboard_connection(directory, "{invalid}", error));
    REQUIRE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK(connection.upload_token == first_key);
    REQUIRE(app::install_sites_leaderboard_connection(directory, second_key, error));
    REQUIRE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK(connection.upload_token == second_key);
    CHECK(connection.site_url == app::kSitesLeaderboardUrl);
    REQUIRE(app::clear_sites_leaderboard_connection(directory, error));
    REQUIRE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK(missing);
    CHECK(connection.upload_token.empty());
}
TEST_CASE("Sites damaged protected connection never falls back to an older plaintext key") {
    const auto directory = std::filesystem::temp_directory_path() /
        ("tenriff_sites_invalid_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    std::ofstream(directory / "sites-leaderboard.dpapi", std::ios::binary) << "damaged";
    std::ofstream(directory / "sites-leaderboard.json") <<
        "{\"schema_version\":1,\"site_url\":\"https://test.chatgpt.site\",\"upload_token\":\"tr_" + std::string(64, 'c') + "\"}";
    app::SitesLeaderboardConnection connection;
    std::string error;
    bool missing = false;
    CHECK_FALSE(app::load_sites_leaderboard_connection(directory, connection, missing, error));
    CHECK_FALSE(error.empty());
    CHECK(connection.upload_token.empty());
    CHECK(app::clear_sites_leaderboard_connection(directory, error));
    std::error_code ec;
    std::filesystem::remove(directory, ec);
}
#endif

TEST_CASE("Sites rejects Auto Scratch even if canonical ruleset metadata is forged") {
    auto replay = score_fixture();
    replay.mods = {"auto_scratch"};
    std::string payload, error;
    CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Fixture", "CLEAR", payload, error));
    CHECK(payload.empty());
}

TEST_CASE("Sites accepts existing 16K while retaining the previous 4K through 14K range") {
    for (int lanes : {4,5,6,7,8,9,10,11,12,13,14,16}) {
        auto replay = score_fixture();
        replay.trace.lane_count = lanes;
        replay.mode.key_conversion_algorithm = "nk2";
        replay.mode.key_conversion_nk2_preset = "source_ln_e_v1";
        std::string payload, error;
        REQUIRE(app::build_sites_score_json(replay, std::string(64, 'c'), "NK2 E", "CLEAR", payload, error));
        CHECK(payload.find("\"key_mode\":\"" + std::to_string(lanes) + "K\"") != std::string::npos);
    }
    for (int lanes : {0,3,15,17,18}) {
        auto replay = score_fixture(); replay.trace.lane_count = lanes;
        std::string payload, error;
        CHECK_FALSE(app::build_sites_score_json(replay, std::string(64, 'c'), "Invalid", "CLEAR", payload, error));
        CHECK(payload.empty());
    }
}
