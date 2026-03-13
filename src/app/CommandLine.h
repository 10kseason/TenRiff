#pragma once

#include <string>
#include <vector>

namespace tenriff::app {

struct CommandLineOptions {
    std::string songs_path = "songs";
    std::string profile = "default";
    std::string chart_path;
    bool has_rate = false;
    bool has_hispeed = false;
    bool has_gauge = false;
    bool has_autoshift = false;
    double rate = 1.0;
    double hispeed = 3.0;
    std::string gauge;
    bool autoshift = true;
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
