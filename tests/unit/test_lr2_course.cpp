#include "doctest/doctest.h"

#include <vector>

#include "app/Lr2Course.h"

using tenriff::app::SongEntry;
using tenriff::app::Lr2CourseDefinition;
using tenriff::app::Lr2CourseLoadResult;
using tenriff::app::match_lr2_course;
using tenriff::app::parse_lr2_course_text;
using tenriff::app::serialize_lr2_course_text;

TEST_CASE("LR2 course parser preserves course and chart order") {
    const auto result = parse_lr2_course_text(
        "<?xml version=\"1.0\" encoding=\"shift_jis\"?>"
        "<courselist>"
        "<course><title>FIRST &amp; FAST</title><line>7</line>"
        "<hash>00000000000000000000000000005190"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB</hash><type>0</type></course>"
        "<course><title>SECOND</title><line>10</line>"
        "<hash>11110000001000000000000000005190"
        "cccccccccccccccccccccccccccccccc</hash><type>1</type></course>"
        "</courselist>");

    REQUIRE(result.success());
    REQUIRE(result.courses.size() == 2u);
    CHECK(result.courses[0].title == "FIRST & FAST");
    CHECK(result.courses[0].key_count == 7);
    CHECK(result.courses[0].type == 0);
    REQUIRE(result.courses[0].chart_md5.size() == 2u);
    CHECK(result.courses[0].chart_md5[0] == "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    CHECK(result.courses[0].chart_md5[1] == "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    CHECK(result.courses[1].key_count == 10);
    CHECK(result.courses[1].type == 1);
}

TEST_CASE("LR2 course parser rejects malformed hash blocks without losing valid courses") {
    const auto result = parse_lr2_course_text(
        "<courselist>"
        "<course><title>BROKEN</title><hash>not-a-course-hash</hash></course>"
        "<course><title>VALID</title><hash>00000000000000000000000000005190"
        "0123456789abcdef0123456789abcdef</hash></course>"
        "</courselist>");

    REQUIRE(result.success());
    REQUIRE(result.courses.size() == 1u);
    CHECK(result.courses[0].title == "VALID");
    CHECK(result.warnings.size() == 1u);
}

TEST_CASE("LR2 course matching keeps repeated stages and reports every missing chart") {
    SongEntry first;
    first.path = "first.bms";
    first.md5 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    SongEntry second;
    second.path = "second.bms";
    second.md5 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

    const auto parsed = parse_lr2_course_text(
        "<course><title>ORDER</title><hash>00000000000000000000000000005190"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "cccccccccccccccccccccccccccccccc</hash></course>");
    REQUIRE(parsed.success());

    const auto match = match_lr2_course(parsed.courses.front(), std::vector<SongEntry>{first, second});
    REQUIRE(match.song_indices.size() == 3u);
    CHECK(match.song_indices[0] == 1u);
    CHECK(match.song_indices[1] == 0u);
    CHECK(match.song_indices[2] == 1u);
    REQUIRE(match.missing_md5.size() == 1u);
    CHECK(match.missing_md5[0] == "cccccccccccccccccccccccccccccccc");
    CHECK_FALSE(match.playable());
}

TEST_CASE("LR2 course serializer round trips title order repeats and key count") {
    Lr2CourseDefinition course;
    course.title = "A & B <Dan>";
    course.key_count = 7;
    course.type = 0;
    course.chart_md5 = {
        "0123456789abcdef0123456789abcdef",
        "fedcba9876543210fedcba9876543210",
        "0123456789abcdef0123456789abcdef",
    };

    const std::string serialized = serialize_lr2_course_text(course);
    CHECK(serialized.find("A &amp; B &lt;Dan&gt;") != std::string::npos);
    const Lr2CourseLoadResult parsed = parse_lr2_course_text(serialized);
    REQUIRE(parsed.success());
    REQUIRE(parsed.courses.size() == 1u);
    CHECK(parsed.courses[0].title == course.title);
    CHECK(parsed.courses[0].key_count == 7);
    CHECK(parsed.courses[0].chart_md5 == course.chart_md5);
}
