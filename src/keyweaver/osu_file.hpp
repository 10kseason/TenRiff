#pragma once

#include <optional>
#include <string>
#include <vector>

// Byte-level .osu reader/writer. Everything outside [HitObjects] is kept as the
// original lines so a converted difficulty differs from its source only where
// the key count forces it to.
namespace keyweaver::osu {

inline constexpr int kManiaMode = 3;
inline constexpr int kManiaY = 192;
inline constexpr int kHoldFlag = 128;

// Where a converted note came from, so the legality pass knows which of two
// colliding notes is the safer one to give up.
enum class Origin {
    Source,     // a source note still at its original time
    Shifted,    // a source note nK2 rolled off its original time
    Generated,  // an nK2 support note
};

struct HitObject {
    int x = 0;
    int y = kManiaY;
    int time = 0;
    int type = 1;
    int hitSound = 0;
    std::optional<int> endTime;  // present for hold notes
    std::string hitSample = "0:0:0:0:";
    std::string rawLine;
    Origin origin = Origin::Source;
};

struct Section {
    std::string name;                // header text without the brackets
    std::vector<std::string> lines;  // body lines, header excluded
};

struct File {
    bool hasBom = false;
    std::string newline = "\r\n";
    std::vector<std::string> preamble;  // lines before the first section header
    std::vector<Section> sections;

    // Values pulled out of the sections for convenience. The sections stay
    // authoritative; setField() writes changes back into them.
    int mode = 0;
    int keyCount = 0;
    std::string version;
    std::string title;
    std::string artist;
    std::string creator;

    std::vector<HitObject> hitObjects;

    Section* findSection(const std::string& name);
    const Section* findSection(const std::string& name) const;
    std::optional<std::string> getField(const std::string& section, const std::string& key) const;
    // Rewrites "key:value" in place, or appends it when the key is missing.
    void setField(const std::string& section, const std::string& key, const std::string& value);
};

struct ParseResult {
    bool ok = false;
    std::string error;
    File file;
};

// column <-> x helpers for the osu!mania playfield.
int columnToX(int column, int keyCount);
int xToColumn(int x, int keyCount);

ParseResult parseFile(const std::string& bytes);
std::string serializeFile(const File& file);

std::string formatHitObject(const HitObject& object);

}  // namespace keyweaver::osu
