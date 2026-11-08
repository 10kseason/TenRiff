#include "app/IrEligibility.h"
#include "app/ChartFileHash.h"
#include "app/ModeManager.h"
#include "app/ReplayVerifier.h"
#include <algorithm>
#include <chrono>
#include <cctype>

namespace tenriff::app {
bool ir_is_excluded(IrEligibility value) {
    return value == IrEligibility::LongKeysound || value == IrEligibility::SilentNotes ||
        value == IrEligibility::Blocked || value == IrEligibility::Unavailable || value == IrEligibility::PlayExcluded;
}
bool ir_chart_blocked(std::string_view hash, std::string_view title) {
    if (hash == "777459221a46b1d97b904100561258ae78f54fbe2233bfb25f2191905e7cf282") return true;
    while (!title.empty() && std::isspace(static_cast<unsigned char>(title.front()))) title.remove_prefix(1);
    while (!title.empty() && std::isspace(static_cast<unsigned char>(title.back()))) title.remove_suffix(1);
    std::string normalized(title);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized == "speed of link";
}
SitesUploadStatusText ir_status_text(IrEligibility value) {
    switch (value) {
    case IrEligibility::Scanning: return {"IR checking...", "IR 검사 중…"};
    case IrEligibility::BranchUnknown: return {"IR check required after the authored branch is selected", "IR 확인 필요 · 플레이 분기 선택 후 검사"};
    case IrEligibility::LongKeysound: return {"A keysound is at least 45 seconds", "45초 이상 키음"};
    case IrEligibility::SilentNotes: return {"At least 85% silent notes", "무키음 노트 85% 이상"};
    case IrEligibility::Blocked: return {"Blocked chart", "등록 제외 곡"};
    case IrEligibility::Unavailable: return {"Audio scan unavailable", "키음 검사 불가"};
    case IrEligibility::PlayExcluded: return {"Current play conditions", "현재 플레이 조건"};
    case IrEligibility::Unknown: return {"IR check required", "IR 확인 필요"};
    default: return {"", ""}; // Checked audio is not a promise of server eligibility.
    }
}
IrEligibility inspect_ir_eligibility(const IrEligibilityRequest& request,
    const std::shared_ptr<std::atomic<bool>>& cancel) {
    if (cancel->load()) return IrEligibility::Unknown;
    auto replay = request.replay;
    if (!request.replay_path.empty()) {
        const auto path = std::filesystem::u8path(request.replay_path);
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size > 32u*1024u*1024u) return IrEligibility::Unavailable;
        const auto digest = hash_chart_file(path);
        if (!digest.valid() || (!request.replay_sha256.empty() && digest.sha256 != request.replay_sha256)) return IrEligibility::Unavailable;
        const auto loaded = gameplay::load_replay_json(request.replay_path);
        if (!loaded.success()) return IrEligibility::Unavailable;
        replay = *loaded.replay;
    }
    if (ir_chart_blocked(replay.chart_sha256, request.title)) return IrEligibility::Blocked;
    if (replay.chart_path.empty()) return IrEligibility::Unavailable;
    // Guest playback is still analysed locally. Consent/account/network state
    // is not a property of the pattern; definitive play exclusions are.
    if (!request.selection_preview && (replay.pause_used || replay.aborted ||
        replay.replay_format_version != gameplay::kReplayFormatVersion ||
        !is_sites_supported_replay_ruleset(replay.ruleset_id, replay.mods))) return IrEligibility::PlayExcluded;
    const auto mode = gameplay::parse_key_mode(replay.mode.key_mode);
    if (!mode || *mode != gameplay::KeyMode::Auto || replay.mode.autoplay_enabled ||
        replay.mode.practice_no_fail_enabled || replay.mode.one_miss_fail_enabled ||
        !replay.mode.course_gauge.empty() || mode_mod_adds_notes(replay.mods) || mode_mod_auto_scratch(replay.mods) ||
        (mode_mod_long_note_mix(replay.mods) && replay.mode.long_note_mix_version != 1)) return IrEligibility::PlayExcluded;
    if (replay.chart_sha256.empty()) {
        const auto digest = hash_chart_file(std::filesystem::u8path(replay.chart_path));
        if (!digest.valid()) return IrEligibility::Unavailable;
        replay.chart_sha256 = digest.sha256;
    }
    if (ir_chart_blocked(replay.chart_sha256, request.title)) return IrEligibility::Blocked;
    SitesKeysoundEvidence evidence;
    std::string error;
    const bool success = prepare_sites_keysound_evidence(replay, inspect_keysound_audio, evidence, error,
        [cancel]() { return cancel->load(std::memory_order_acquire); }, request.selection_preview);
    if (cancel->load()) return IrEligibility::Unknown;
    if (request.selection_preview && evidence.authored_random_branch) return IrEligibility::BranchUnknown;
    if (!success) return IrEligibility::Unavailable;
    if (evidence.max_duration_seconds >= 45.0) return IrEligibility::LongKeysound;
    if (evidence.playable_note_count == 0) return IrEligibility::Unavailable;
    if (evidence.silent_note_count*100 >= evidence.playable_note_count*85) return IrEligibility::SilentNotes;
    return IrEligibility::Checked;
}
IrEligibilityPreview::~IrEligibilityPreview() { shutdown(); }
IrEligibility IrEligibilityPreview::state_for(std::string_view key) const {
    return !key.empty() && key == request_.key ? state_ : IrEligibility::Unknown;
}
bool IrEligibilityPreview::service(IrEligibilityRequest request, int64_t now_ns) {
    bool changed = false;
    if (request.key != request_.key) {
        if (cancel_) cancel_->store(true);
        request_ = std::move(request);
        state_ = request_.key.empty() ? IrEligibility::Unknown : IrEligibility::Scanning;
        pending_ = !request_.key.empty(); due_ns_ = now_ns + 150'000'000LL;
        changed = true;
    }
    if (future_.valid() && future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        Completion completed;
        try { completed = future_.get(); } catch (...) { completed.key=request_.key; }
        const bool cancelled = cancel_ && cancel_->load(); cancel_.reset();
        if (!cancelled && completed.key == request_.key && !request_.key.empty()) {
            state_ = completed.state; changed = true;
        }
    }
    if (pending_ && !future_.valid() && now_ns >= due_ns_) {
        pending_=false;
        cancel_=std::make_shared<std::atomic<bool>>(false);
        const auto task=request_; const auto cancel=cancel_; const auto scan=scan_ ? scan_ : inspect_ir_eligibility;
        try { future_=std::async(std::launch::async,[task,cancel,scan]() { return Completion{task.key,scan(task,cancel)}; }); }
        catch (...) { state_=IrEligibility::Unavailable; changed=true; }
    }
    return changed;
}
void IrEligibilityPreview::shutdown() {
    if(cancel_) cancel_->store(true);
    if(future_.valid()) { future_.wait(); try { static_cast<void>(future_.get()); } catch (...) {} }
    cancel_.reset(); request_={}; pending_=false; state_=IrEligibility::Unknown;
}
} // namespace tenriff::app
