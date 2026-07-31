#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

// Minimal chart model owned by the nK2 module. TenRiff translates its
// sample-domain GameplayChart into this millisecond-domain representation at
// the adapter boundary, so nK2 does not depend on another converter project.
namespace keyconv {

enum class NoteType {
  Tap,
  Hold,
};

struct Note {
  std::string id;
  int time = 0;
  int lane = 0;
  NoteType type = NoteType::Tap;
  std::optional<int> endTime;
  std::string raw;
  std::optional<int> sourceLane;
};

struct TimingPoint {
  int time = 0;
  double beatLength = 0.0;
  std::optional<int> meter;
  std::optional<int> sampleSet;
  std::optional<int> sampleIndex;
  std::optional<int> volume;
  std::optional<bool> uninherited;
  std::optional<int> effects;
  std::string rawLine;
};

struct RawChartData {
  std::map<std::string, std::vector<std::string>> sections;
  std::vector<std::string> sectionOrder;
  std::vector<std::string> preamble;
  std::vector<std::string> originalLines;
};

struct ChartMeta {
  std::optional<std::string> title;
  std::optional<std::string> artist;
  std::optional<std::string> creator;
  std::optional<std::string> version;
  int sourceKeyCount = 0;
  std::optional<int> targetKeyCount;
  std::string format = "tenriff";
  std::optional<int> mode;
};

struct Chart {
  ChartMeta meta;
  std::vector<TimingPoint> timingPoints;
  std::vector<Note> notes;
  RawChartData raw;
  std::vector<std::string> warnings;
};

} // namespace keyconv
