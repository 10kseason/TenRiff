#pragma once
#include <string>
#include <string_view>

namespace tenriff::app {
struct SitesUploadMessage {
    std::string replay_sha256;
    std::string text;

    [[nodiscard]] bool applies_to_result(std::string_view result_sha256) const {
        return !result_sha256.empty() && replay_sha256 == result_sha256;
    }
};

struct SitesUploadStatusText { std::string_view english; std::string_view korean; };

// Summaries use locally authored text only. Keep remote bodies, paths and keys
// out of the persistent account status and the result's visible status line.
inline SitesUploadStatusText sites_upload_status_text(std::string_view message) {
    if (message == "Sites community record uploaded.")
        return {"Web record registered.", "웹 기록 등록 완료"};
    if (message.find("HTTP 400") != std::string_view::npos) {
        if (message.find("PC clock") != std::string_view::npos)
            return {"Web rejected the play time (HTTP 400). Check the PC clock; record saved locally.",
                    "웹 등록 거절 (HTTP 400) · PC 날짜/시간 확인 · 로컬 저장"};
        return {"Web rejected the record (HTTP 400). Check game/Site versions and play conditions; saved locally.",
                "웹 등록 거절 (HTTP 400) · 게임/웹 버전·플레이 조건 확인 · 로컬 저장"};
    }
    if (message.find("HTTP 401") != std::string_view::npos)
        return {"Web key expired or revoked (HTTP 401). Reconnect via F10; record saved locally.",
                "웹 인증 실패 (HTTP 401) · F10에서 연결 정보 다시 등록 · 로컬 저장"};
    if (message.find("HTTP 403") != std::string_view::npos)
        return {"Web access denied (HTTP 403). Check Site access; record saved locally.",
                "웹 접근 거절 (HTTP 403) · 사이트 공개 설정 확인 · 로컬 저장"};
    if (message.find("HTTP 404") != std::string_view::npos)
        return {"Web upload endpoint missing (HTTP 404). Check the Site address; saved locally.",
                "웹 업로드 주소 없음 (HTTP 404) · F10 연결 주소 확인 · 로컬 저장"};
    if (message.find("HTTP 429") != std::string_view::npos)
        return {"Web upload rate limited (HTTP 429). Try playing again later; this record is local.",
                "웹 업로드 요청 제한 (HTTP 429) · 잠시 후 다음 플레이 · 이번 기록은 로컬 저장"};
    if (message.find("BMS branch chart evidence") != std::string_view::npos)
        return {"Web upload skipped: original BMS chart unavailable or changed; record saved locally.",
                "웹 등록 제외 · 원본 BMS 채보가 없거나 변경됨 · 로컬 저장"};
    if (message.find("LN Mix v") != std::string_view::npos)
        return {"Long LN Mix uses local records only.", "긴 LN 믹스는 로컬 기록만 저장합니다."};
    if (message.find("older or unsupported judgement timing") != std::string_view::npos)
        return {"Older or unsupported judgement timing stays in local records.",
                "이전 또는 지원하지 않는 판정 기준의 기록은 로컬에 보관됩니다."};
    if (message.find("not eligible") != std::string_view::npos)
        return {"This play is excluded from web ranking; record saved locally.",
                "웹 등록 제외 플레이 조건 · 기록은 로컬에 저장"};
    if (message.find("metadata") != std::string_view::npos || message.find("statistics") != std::string_view::npos)
        return {"Web upload skipped: invalid record information; saved locally.",
                "웹 등록 제외 · 기록 정보 확인 필요 · 로컬 저장"};
    if (message.find("replay evidence") != std::string_view::npos)
        return {"Web upload skipped: replay evidence unavailable; check local replay export.",
                "웹 등록 제외 · 리플레이 저장/검증 실패 · 로컬 기록 확인"};
    if (message.find("connection") != std::string_view::npos || message.find("Connection") != std::string_view::npos)
        return {"Web connection unavailable. Reconnect via F10; record saved locally.",
                "웹 연결 정보 확인 필요 · F10에서 다시 연결 · 로컬 저장"};
    return {"Web upload incomplete. Check the connection and Site; record saved locally.",
            "웹 전송 미완료 · 네트워크·사이트 상태 확인 · 로컬 저장"};
}
}  // namespace tenriff::app
