#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace tenriff::app {

struct CommandLineOptions {
    std::string songs_path = "songs";
    std::string profile = "default";
    std::string chart_path;
    std::string replay_path;
    std::string ghost_replay_path;
    std::string ranked_challenge_id;
    std::string ranked_challenge_nonce;
    // Captured by MenuApp for a fresh play; there is deliberately no CLI opt-in.
    bool online_recording_enabled = false;
    // Switching to guest irrevocably revokes the current play's permission.
    std::shared_ptr<std::atomic_bool> online_recording_permission;
    bool has_rate = false;
    bool has_hispeed = false;
    bool has_gauge = false;
    double rate = 1.0;
    double hispeed = 3.0;
    std::string gauge;
    bool debug = false;
    bool no_vsync = false;
};

struct CommandLineParseResult {
    CommandLineOptions options;
    std::vector<std::string> warnings;
};

class CommandLine {
public:
    [[nodiscard]] static CommandLineParseResult parse(int argc, char** argv);
};

}  // namespace tenriff::app
