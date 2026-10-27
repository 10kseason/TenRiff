#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/SongIndex.h"

namespace tenriff::app {

struct Lr2CourseDefinition {
    std::string title;
    int key_count = 0;
    int type = 0;
    std::vector<std::string> chart_md5;
};

struct Lr2CourseLoadResult {
    std::vector<Lr2CourseDefinition> courses;
    std::vector<std::string> warnings;
    std::string error;

    [[nodiscard]] bool success() const { return error.empty(); }
};

struct Lr2CourseSaveResult {
    std::string error;

    [[nodiscard]] bool success() const { return error.empty(); }
};

struct Lr2CourseMatch {
    std::vector<std::size_t> song_indices;
    std::vector<std::string> missing_md5;
    // One slot per authored stage, including missing and repeated hashes.
    std::vector<std::optional<std::size_t>> stage_song_indices;

    [[nodiscard]] bool playable() const {
        return !song_indices.empty() && missing_md5.empty();
    }
};

[[nodiscard]] Lr2CourseLoadResult parse_lr2_course_text(std::string_view content);
[[nodiscard]] Lr2CourseLoadResult load_lr2_course_file(std::string_view path);
[[nodiscard]] std::string serialize_lr2_course_text(const Lr2CourseDefinition& course);
[[nodiscard]] Lr2CourseSaveResult save_lr2_course_file(std::string_view path,
                                                       const Lr2CourseDefinition& course);
[[nodiscard]] Lr2CourseMatch match_lr2_course(const Lr2CourseDefinition& course,
                                              const std::vector<SongEntry>& songs);

[[nodiscard]] std::vector<Lr2CourseMatch> match_lr2_courses(
    const std::vector<Lr2CourseDefinition>& courses, const std::vector<SongEntry>& songs);

}  // namespace tenriff::app
