#pragma once
#include <atomic>
#include <future>
#include <memory>
#include <string>
#include "app/SitesLeaderboardClient.h"

namespace tenriff::app {
enum class IrEligibility { Unknown, Scanning, Checked, BranchUnknown, LongKeysound, SilentNotes, Blocked, Unavailable, PlayExcluded };
[[nodiscard]] bool ir_is_excluded(IrEligibility value);
[[nodiscard]] bool ir_chart_blocked(std::string_view hash, std::string_view title);
[[nodiscard]] SitesUploadStatusText ir_status_text(IrEligibility value);
struct IrEligibilityRequest {
    std::string key, title, replay_path, replay_sha256;
    gameplay::ReplayFile replay;
    bool selection_preview = true;
};
// One selected pattern/result only. No persistent eligibility claims. Every
// visit/config revision starts fresh. No full-library or periodic audio scans.
class IrEligibilityPreview {
public:
    using Scan = IrEligibility (*)(const IrEligibilityRequest&, const std::shared_ptr<std::atomic<bool>>&);
    explicit IrEligibilityPreview(Scan scan = nullptr) : scan_(scan) {}
    ~IrEligibilityPreview();
    bool service(IrEligibilityRequest request, int64_t now_ns);
    [[nodiscard]] IrEligibility state_for(std::string_view key) const;
    [[nodiscard]] const std::string& request_key() const { return request_.key; }
    void shutdown();
private:
    struct Completion { std::string key; IrEligibility state = IrEligibility::Unavailable; };
    IrEligibilityRequest request_;
    IrEligibility state_ = IrEligibility::Unknown;
    int64_t due_ns_ = 0;
    bool pending_ = false;
    Scan scan_;
    std::future<Completion> future_;
    std::shared_ptr<std::atomic<bool>> cancel_;
};
[[nodiscard]] IrEligibility inspect_ir_eligibility(const IrEligibilityRequest& request,
    const std::shared_ptr<std::atomic<bool>>& cancel);
} // namespace tenriff::app
