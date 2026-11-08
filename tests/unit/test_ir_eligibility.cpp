#include "doctest/doctest.h"
#ifdef _WIN32
#include "app/IrEligibility.h"
#include "app/ChartFileHash.h"
#include "app/ReplayVerifier.h"
#include <chrono>
#include <fstream>
#include <thread>
#ifdef _WIN32
#include "app/MenuApp.h"
#endif

using namespace tenriff::app;
namespace {
std::atomic<int> ir_scans{0};
std::atomic<bool> ir_hold{false};
IrEligibility fake_ir_scan(const IrEligibilityRequest& request, const std::shared_ptr<std::atomic<bool>>&) {
    ++ir_scans;
    // Simulate a single codec call that cannot observe cancellation mid-file.
    while (ir_hold) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return request.title == "silent" ? IrEligibility::SilentNotes : IrEligibility::Checked;
}
bool wait_ir(IrEligibilityPreview& preview, const IrEligibilityRequest& request, int64_t now) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        static_cast<void>(preview.service(request, now));
        if (preview.state_for(request.key) != IrEligibility::Scanning) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}
struct IrFixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("tenriff_ir_ui_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    IrFixture() {
        std::filesystem::create_directories(root);
        std::ofstream wav(root/"silent.wav",std::ios::binary);
        const auto le=[&](unsigned value,int bytes) { for(int i=0;i<bytes;++i) wav.put(static_cast<char>((value>>(8*i))&255)); };
        wav.write("RIFF",4); le(36+1600,4); wav.write("WAVEfmt ",8);
        le(16,4); le(1,2); le(1,2); le(8000,4); le(16000,4); le(2,2); le(16,2);
        wav.write("data",4); le(1600,4); const std::string zeros(1600,'\0'); wav.write(zeros.data(),zeros.size());
    }
    ~IrFixture() { std::error_code ec; std::filesystem::remove_all(root,ec); }
    IrEligibilityRequest request(bool random=false) const {
        const std::string text = "#TITLE Fixture\n#BPM 120\n#RANK 3\n#WAV01 silent.wav\n" +
            std::string(random ? "#RANDOM 2\n#IF 1\n#00111:01\n#ELSE\n#00112:01\n#ENDIF\n#ENDRANDOM\n" : "#00111:01\n");
        const auto path=root/"chart.bms";
        { std::ofstream out(path,std::ios::binary); out<<text; }
        IrEligibilityRequest request;
        request.key="chart";
        auto& replay=request.replay;
        replay.chart_path=path.u8string(); replay.chart_sha256=sha256_bytes(text); replay.chart_format="bms";
        replay.mode.key_mode="auto"; replay.mode.bms_timing_version=2; replay.mode.bms_gauge_version=2;
        replay.mode.bms_control_flow_version=2; replay.mode.bms_random_seed=1;
        replay.trace.sample_rate=replay.sample_rate=48000; replay.trace.lane_count=7;
        replay.trace.duration_samples=240000;
        replay.ruleset_id=std::string(kCanonicalReplayRulesetId); replay.record_play_mode="guest";
        replay.stats.total_notes=1; replay.stats.counts.pg=1; replay.stats.max_combo=1;
        replay.final_score=100; replay.created_utc="20261108_000000Z";
        return request;
    }
};
}

TEST_CASE("IR preview debounces and never repeats an unchanged audio scan") {
    ir_scans=0; ir_hold=false;
    IrEligibilityPreview preview(fake_ir_scan);
    IrEligibilityRequest request; request.key="selected"; request.title="silent";
    CHECK(preview.service(request,0));
    CHECK_FALSE(ir_is_excluded(preview.state_for(request.key)));
    CHECK_FALSE(preview.service(request,149'999'999)); CHECK(ir_scans==0);
    REQUIRE(wait_ir(preview,request,150'000'000));
    CHECK(preview.state_for(request.key)==IrEligibility::SilentNotes); CHECK(ir_scans==1);
    CHECK_FALSE(preview.service(request,3600'000'000'000LL)); CHECK(ir_scans==1);
    CHECK(preview.service({},3600'000'000'001LL));
    CHECK(preview.state_for(request.key)==IrEligibility::Unknown);
    CHECK(preview.service(request,3600'000'000'002LL));
    REQUIRE(wait_ir(preview,request,3601'000'000'000LL)); CHECK(ir_scans==2);
}

TEST_CASE("IR preview cancels without joining on selection change and discards stale results") {
    ir_scans=0; ir_hold=true;
    IrEligibilityPreview preview(fake_ir_scan);
    IrEligibilityRequest old_request; old_request.key="old"; old_request.title="silent";
    static_cast<void>(preview.service(old_request,0));
    static_cast<void>(preview.service(old_request,150'000'000));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(ir_scans==0 && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();
    REQUIRE(ir_scans==1);
    IrEligibilityRequest next; next.key="new";
    auto release=std::async(std::launch::async,[] { std::this_thread::sleep_for(std::chrono::milliseconds(250)); ir_hold=false; });
    const auto start=std::chrono::steady_clock::now();
    CHECK(preview.service(next,160'000'000));
    const auto elapsed=std::chrono::steady_clock::now()-start;
    release.wait();
    CHECK(elapsed < std::chrono::milliseconds(100));
    CHECK(preview.state_for("old")==IrEligibility::Unknown);
    CHECK(preview.state_for("new")==IrEligibility::Scanning);
    ir_hold=false;
    REQUIRE(wait_ir(preview,next,400'000'000));
    CHECK(preview.state_for("new")==IrEligibility::Checked);
    CHECK_FALSE(ir_is_excluded(preview.state_for("new")));
}

TEST_CASE("IR local scan distinguishes unknown authored branches and guest actual play exclusions") {
    IrFixture fixture;
    auto cancel=std::make_shared<std::atomic<bool>>(false);
    auto request=fixture.request();
    CHECK(inspect_ir_eligibility(request,cancel)==IrEligibility::SilentNotes);
    request=fixture.request(true);
    CHECK(inspect_ir_eligibility(request,cancel)==IrEligibility::BranchUnknown);
    request.selection_preview=false;
    CHECK(inspect_ir_eligibility(request,cancel)==IrEligibility::SilentNotes);
    request.replay.pause_used=true;
    CHECK(inspect_ir_eligibility(request,cancel)==IrEligibility::PlayExcluded);
    request.replay.pause_used=false; request.replay.aborted=true;
    CHECK(inspect_ir_eligibility(request,cancel)==IrEligibility::PlayExcluded);
    request.replay.aborted=false; request.replay.ruleset_id="unknown";
    CHECK(inspect_ir_eligibility(request,cancel)==IrEligibility::PlayExcluded);
    request.title="  Speed of Link  ";
    CHECK(inspect_ir_eligibility(request,cancel)==IrEligibility::Blocked);
    CHECK(ir_chart_blocked("777459221a46b1d97b904100561258ae78f54fbe2233bfb25f2191905e7cf282","renamed"));
    CHECK_FALSE(ir_is_excluded(IrEligibility::Unknown));
    CHECK_FALSE(ir_is_excluded(IrEligibility::BranchUnknown));
    CHECK(ir_status_text(IrEligibility::Unknown).korean=="IR 확인 필요");
}

#ifdef _WIN32
namespace tenriff::app {
struct MenuAppIrEligibilityTestAccess {
    static void check() {
        IrFixture fixture;
        auto request=fixture.request();
        auto menu=std::make_unique<MenuApp>();
        menu->config_.audio_ui.background_sound_enabled=false;
        menu->songs_path_=fixture.root.u8string();
        menu->song_select_view_=MenuApp::SongSelectView::Songs;
        SongEntry entry; entry.path="chart.bms"; entry.title="Fixture"; entry.format="bms"; entry.key_count=7;
        menu->update_song_list(SongIndex{{entry}});
        menu->replace_screen(MenuApp::Screen::SongSelect);
        const auto key=menu->song_ir_profile_key(entry);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(std::chrono::steady_clock::now()<deadline && !ir_is_excluded(menu->song_ir_state(entry))) {
            static_cast<void>(menu->service_ir_eligibility());
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        CHECK(menu->song_ir_state(entry)==IrEligibility::SilentNotes);
        render::MenuRenderData render;
        menu->populate_song_select_render_data(render,{}, {},nullptr);
        REQUIRE(render.song_select.songs.size()==1);
        CHECK(render.song_select.songs[0].ir_excluded);
        CHECK(render.song_select.selected_song_ir_excluded);
        CHECK_FALSE(render.song_select.selected_song_ir_status.empty());
        menu->config_.speed.rate=1.1;
        CHECK(menu->song_ir_profile_key(entry)!=key);
        CHECK(menu->song_ir_state(entry)==IrEligibility::Unknown);
        menu->config_.speed.rate=1.0; ++menu->song_index_revision_;
        CHECK(menu->song_ir_profile_key(entry)!=key);
        CHECK(menu->song_ir_state(entry)==IrEligibility::Unknown);
        menu->config_.mode.key_mode="7k";
        CHECK(menu->song_ir_state(entry)==IrEligibility::PlayExcluded);
        menu->config_.mode.key_mode="auto";
        request.selection_preview=false;
        const auto replay_path=fixture.root/"guest.replay.json";
        REQUIRE(gameplay::save_replay_json(replay_path.u8string(),request.replay).success());
        REQUIRE(gameplay::load_replay_json(replay_path.u8string()).success());
        menu->last_replay_path_=replay_path.u8string(); menu->last_chart_title_="Fixture";
        menu->has_result_=true; menu->last_result_=request.replay.stats;
        menu->replace_screen(MenuApp::Screen::Result);
        const auto result_key=menu->result_ir_profile_key();
        const auto result_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(std::chrono::steady_clock::now()<result_deadline && !ir_is_excluded(menu->ir_eligibility_.state_for(result_key))) {
            static_cast<void>(menu->service_ir_eligibility());
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        CHECK(menu->ir_eligibility_.state_for(result_key)==IrEligibility::SilentNotes);
        menu->config_.speed.rate=2.0; menu->config_.mode.key_mode="10k";
        CHECK(menu->result_ir_profile_key()==result_key);
        menu->populate_result_render_data(render,{});
        CHECK(render.result.ir_excluded);
        CHECK_FALSE(render.result.ir_status.empty());
        menu->last_replay_path_="another-result.replay.json";
        CHECK(menu->ir_eligibility_.state_for(menu->result_ir_profile_key())==IrEligibility::Unknown);
    }
};
}
TEST_CASE("IR menu publishes measured list detail and guest result with distinct invalidation keys") {
    tenriff::app::MenuAppIrEligibilityTestAccess::check();
}
#endif
#endif // _WIN32: menu/application audio inspection is a Windows target.
