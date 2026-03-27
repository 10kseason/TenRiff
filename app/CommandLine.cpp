#include "app/CommandLine.h"

#include <cstdlib>

namespace tenriff::app {

namespace {

bool parse_double(const char* text, double& out) {
    if (!text) {
        return false;
    }
    char* end = nullptr;
    out = std::strtod(text, &end);
    return end && *end == '\0';
}

}  // namespace

CommandLineParseResult CommandLine::parse(int argc, char** argv) {
    CommandLineParseResult result;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--songs" && i + 1 < argc) {
            result.options.songs_path = argv[++i];
            continue;
        }
        if (arg == "--profile" && i + 1 < argc) {
            result.options.profile = argv[++i];
            continue;
        }
        if (arg == "--chart" && i + 1 < argc) {
            result.options.chart_path = argv[++i];
            continue;
        }
        if (arg == "--replay" && i + 1 < argc) {
            result.options.replay_path = argv[++i];
            continue;
        }
        if (arg == "--rate" && i + 1 < argc) {
            double value = 0.0;
            if (parse_double(argv[i + 1], value)) {
                result.options.rate = value;
                result.options.has_rate = true;
                ++i;
                continue;
            }
        }
        if (arg == "--hispeed" && i + 1 < argc) {
            double value = 0.0;
            if (parse_double(argv[i + 1], value)) {
                result.options.hispeed = value;
                result.options.has_hispeed = true;
                ++i;
                continue;
            }
        }
        if (arg == "--gauge" && i + 1 < argc) {
            result.options.gauge = argv[++i];
            result.options.has_gauge = true;
            continue;
        }
        if (arg == "--debug") {
            result.options.debug = true;
            continue;
        }
        if (arg == "--no-vsync") {
            result.options.no_vsync = true;
            continue;
        }

        result.warnings.push_back("Unknown argument: " + arg);
    }

    return result;
}

}  // namespace tenriff::app
