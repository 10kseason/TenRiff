#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "chart/NoteLaneMapping.h"

namespace tenriff::chart {

struct BmsParserOptions {
    bool tolerant = false;
    bool retain_wav_bmp = true;
    bool retain_unknown_headers = true;
    bool retain_nonessential_commands = true;
    // Keep only unique note-lane channels plus index headers. This preserves
    // menu key-mode inference without retaining measure payloads.
    bool metadata_only = false;
    // BMS control-flow commands use this deterministic seed. Keeping the seed
    // explicit makes repeated chart loads and replay playback select the same
    // #RANDOM/#SWITCH branches.
    uint32_t random_seed = 0;
    // Version 1 reproduces historical replay parsing. Version 2 also accepts
    // control-number comments and bounded missing-ENDIF recovery in authored BMS.
    int control_flow_version = 2;
    // Upload-only branch identity. Ordinary gameplay does not retain another
    // chart-text copy. Captures active directives after control expansion.
    bool capture_active_text = false;
    // Older native replays and explicit conversions reserve the empty SP
    // scratch lane. New native play recognizes scratch-free seven-key charts.
    bool preserve_empty_sp_scratch = false;
};

enum class BmsParseSeverity {
    Info,
    Warning,
    Error,
};

struct BmsParseMessage {
    BmsParseSeverity severity;
    std::size_t line;  // 1-based line number
    std::string text;
};

struct BmsMeasureCommand {
    int measure = 0;
    std::string channel;
    std::string data;
};

struct BmsChart {
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> wav;
    std::unordered_map<std::string, std::string> bmp;
    std::unordered_map<std::string, double> bpm;
    std::unordered_map<std::string, double> stop;
    std::unordered_map<std::string, double> scroll;
    double base_bpm = 0.0;
    // Preserve the chart extent even when indexing discards BGM/media payloads.
    int last_measure_index = 0;
    // Includes directives in inactive branches; session setup uses this to keep
    // authored branch randomness independent of the lane-randomization seed.
    bool has_random_directives = false;
    // Includes visible/LN scratch objects and scratch mines, but not 00 rows.
    // Retained separately because metadata-only parsing discards payloads.
    bool has_sp_scratch_objects = false;
    int declared_key_count = 0;
    std::string layout_label;
    std::vector<BmsMeasureCommand> commands;
    NoteLaneMapping lane_mapping = NoteLaneMapping::TenKeyDualPlayerDefault();
};

struct BmsParseResult {
    BmsChart chart;
    std::vector<BmsParseMessage> messages;
    // BOM/encoding-normalized, semicolon-comment-stripped active # directives,
    // outer whitespace removed, source order retained, each ending in LF.
    std::string active_text;

    bool success() const;
};

class BmsParser {
public:
    BmsParseResult parse(std::string_view content, const BmsParserOptions& options = {}) const;
    BmsParseResult parseFile(const std::string& path, const BmsParserOptions& options = {}) const;
};

}  // namespace tenriff::chart
