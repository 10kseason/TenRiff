#pragma once

#include <string>
#include <string_view>

#include "config/SimpleJson.h"

namespace tenriff::app {

// Keep remote error bodies out of UI/logs: an endpoint may reflect request
// headers or private data. Known HTTP outcomes have local, actionable messages.
inline bool parse_sites_upload_response(unsigned status, std::string_view body,
                                        bool& retryable, std::string& error) {
    retryable = false;
    error.clear();
    if (status != 200 && status != 201) {
        retryable = status == 429 || status >= 500;
        error = "Sites returned HTTP " + std::to_string(status) + ". ";
        switch (status) {
            case 400:
                // Only this allowlisted machine code changes the local message;
                // free-form remote error text is deliberately never reflected.
                if (const auto parsed = config::parse_json(body); parsed.success() && parsed.root->is_object()) {
                    const auto& object = *parsed.root->as_object();
                    const auto code = object.find("error_code");
                    if (code != object.end() && code->second.is_string() && code->second.as_string() == "future_play_time") {
                        error += "The play time is in the future; check the PC clock and time zone.";
                        break;
                    }
                }
                error += "The Site rejected the record format, ruleset or play conditions; check game/Site compatibility.";
                break;
            case 401:
                error += "The connection key is missing, expired or revoked; reconnect the game on the leaderboard Site.";
                break;
            case 403:
                error += "Access to the Site was denied; check its public access settings.";
                break;
            case 404:
                error += "The Site upload endpoint was not found; check the Site address and deployment.";
                break;
            case 429:
                error += "Uploads are temporarily rate limited; try again shortly.";
                break;
            default:
                error += retryable ? "The Site is temporarily unavailable; try again later."
                                   : "The Site did not accept this upload; the record remains saved locally.";
                break;
        }
        return false;
    }
    const auto parsed = config::parse_json(body);
    if (parsed.success() && parsed.root->is_object()) {
        const auto& object = *parsed.root->as_object();
        const auto accepted = object.find("accepted");
        if (accepted != object.end() && accepted->second.is_bool() && accepted->second.as_bool()) return true;
    }
    error = "Sites did not acknowledge the record.";
    return false;
}

}  // namespace tenriff::app
