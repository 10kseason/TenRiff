#include "doctest/doctest.h"

#include "gameplay/OsuManiaScore.h"

using tenriff::gameplay::OsuManiaJudgement;
using tenriff::gameplay::OsuManiaScoreV1;
using tenriff::gameplay::classify_osu_mania_od8_hold;
using tenriff::gameplay::classify_osu_mania_od8_tap;
using tenriff::gameplay::initialize_osu_mania_od8_score;
using tenriff::gameplay::record_osu_mania_od8_judgement;

TEST_CASE("osu mania OD8 tap conversion follows the stable asymmetric windows") {
    CHECK(classify_osu_mania_od8_tap(16.0) == OsuManiaJudgement::Perfect);
    CHECK(classify_osu_mania_od8_tap(-40.0) == OsuManiaJudgement::Great);
    CHECK(classify_osu_mania_od8_tap(73.0) == OsuManiaJudgement::Good);
    CHECK(classify_osu_mania_od8_tap(-103.0) == OsuManiaJudgement::Ok);
    CHECK(classify_osu_mania_od8_tap(-127.0) == OsuManiaJudgement::Meh);
    CHECK(classify_osu_mania_od8_tap(103.49) == OsuManiaJudgement::Ok);
    CHECK(classify_osu_mania_od8_tap(103.50) == OsuManiaJudgement::Miss);
    CHECK(classify_osu_mania_od8_tap(-127.49) == OsuManiaJudgement::Meh);
    CHECK(classify_osu_mania_od8_tap(-127.50) == OsuManiaJudgement::Miss);
}

TEST_CASE("osu mania OD8 hold conversion combines head and tail and caps body breaks") {
    CHECK(classify_osu_mania_od8_hold(10.0, 10.0, false) == OsuManiaJudgement::Perfect);
    CHECK(classify_osu_mania_od8_hold(20.0, 30.0, false) == OsuManiaJudgement::Great);
    CHECK(classify_osu_mania_od8_hold(10.0, 0.0, true) == OsuManiaJudgement::Meh);
    CHECK(classify_osu_mania_od8_hold(103.5, 0.0, false) == OsuManiaJudgement::Miss);
    CHECK(classify_osu_mania_od8_hold(-127.5, 0.0, false) == OsuManiaJudgement::Miss);
    CHECK(classify_osu_mania_od8_hold(10.0, 104.0, false) == OsuManiaJudgement::Miss);
    CHECK(classify_osu_mania_od8_hold(10.0, 0.0, false, true) == OsuManiaJudgement::Miss);
}

TEST_CASE("osu mania ScoreV1 conversion reaches one million on an OD8 perfect run") {
    OsuManiaScoreV1 score;
    initialize_osu_mania_od8_score(score, 3);
    record_osu_mania_od8_judgement(score, OsuManiaJudgement::Perfect);
    record_osu_mania_od8_judgement(score, OsuManiaJudgement::Perfect);
    record_osu_mania_od8_judgement(score, OsuManiaJudgement::Perfect);

    CHECK(score.available);
    CHECK(score.judged_objects == 3);
    CHECK(score.counts.perfect == 3);
    CHECK(score.score == 1'000'000);
}

TEST_CASE("osu mania ScoreV1 miss resets bonus without changing the native score") {
    OsuManiaScoreV1 score;
    initialize_osu_mania_od8_score(score, 3);
    record_osu_mania_od8_judgement(score, OsuManiaJudgement::Perfect);
    record_osu_mania_od8_judgement(score, OsuManiaJudgement::Miss);
    record_osu_mania_od8_judgement(score, OsuManiaJudgement::Perfect);

    CHECK(score.judged_objects == 3);
    CHECK(score.counts.perfect == 2);
    CHECK(score.counts.miss == 1);
    CHECK(score.bonus == doctest::Approx(2.0));
    CHECK(score.score > 333'333);
    CHECK(score.score < 666'667);
}
