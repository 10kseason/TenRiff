#include "doctest/doctest.h"
#include "render/NativeMenuMotion.h"

#include <limits>

using tenriff::render::NativeMenuMotion;

TEST_CASE("native menu transitions reset across screens and gameplay") {
    NativeMenuMotion motion;
    motion.begin(1, 0, true, false);
    CHECK(motion.entrance() == 0);
    motion.begin(1, 500'000'000, true, false);
    CHECK(motion.entrance() == 1);
    motion.begin(2, 600'000'000, true, false);
    CHECK(motion.entrance() == 0);
    motion.begin(3, 700'000'000, false, false);
    CHECK_FALSE(motion.moving());
    CHECK(motion.seconds() == 0);
    motion.begin(2, 800'000'000, true, false);
    CHECK(motion.entrance() == 0);
}

TEST_CASE("native menu reduced motion freezes decoration and completes entry") {
    NativeMenuMotion motion;
    motion.begin(1, 2'000'000'000, true, true);
    CHECK(motion.entrance(0.2f) == 1);
    CHECK(motion.seconds() == 0);
    CHECK_FALSE(motion.moving());
    CHECK(motion.focus(0, 123, true) == 1);
    CHECK(motion.focus(0, 123, false) == 0);
}

TEST_CASE("native menu focus remains bounded during rapid navigation and long pauses") {
    NativeMenuMotion motion;
    motion.begin(1, 0, true, false);
    CHECK(motion.focus(1, 12, false) == 0);
    motion.begin(1, 16'000'000, true, false);
    const float focused = motion.focus(1, 12, true);
    CHECK(focused > 0);
    CHECK(focused < 1);
    motion.begin(1, 32'000'000, true, false);
    CHECK(motion.focus(1, 12, false) < focused);
    CHECK(motion.focus(1, 13, false) == 0); // A recycled row cannot retain another song's glow.
    motion.begin(1, 99'000'000'000LL, true, false);
    const float resumed = motion.focus(1, 13, true);
    CHECK(resumed > 0);
    CHECK(resumed < 1);
    motion.begin(1, 1, true, false); // Counter rollback or a fresh preview session.
    CHECK(motion.entrance() == 0);
    CHECK(motion.focus(1, 13, false) == 0);
}

TEST_CASE("native menu zero speed immediately settles visible content and focus") {
    NativeMenuMotion motion;
    motion.begin(1, 0, true, false, 1);
    CHECK(motion.entrance() == 0);
    motion.begin(1, 16'000'000, true, false, 0);
    CHECK_FALSE(motion.moving());
    CHECK(motion.seconds() == 0);
    CHECK(motion.entrance(0.2f, 0.8f) == 1);
    CHECK(motion.focus(1, 123, true) == 1);
    CHECK(motion.focus(1, 123, false) == 0);
}

TEST_CASE("native menu speed and entry duration independently scale presentation time") {
    NativeMenuMotion normal, fast, longer;
    normal.begin(1, 0, true, false);
    fast.begin(1, 0, true, false, 2);
    longer.begin(1, 0, true, false, 1, 0.84f);
    normal.begin(1, 100'000'000, true, false);
    fast.begin(1, 50'000'000, true, false, 2);
    longer.begin(1, 200'000'000, true, false, 1, 0.84f);
    CHECK(fast.seconds() == doctest::Approx(normal.seconds()));
    CHECK(fast.entrance() == doctest::Approx(normal.entrance()));
    CHECK(longer.entrance() == doctest::Approx(normal.entrance()));
    CHECK(longer.seconds() > normal.seconds());
}

TEST_CASE("native menu nonfinite timing overrides fall back to normal speed and entry") {
    NativeMenuMotion normal;
    normal.begin(1, 0, true, false);
    normal.begin(1, 100'000'000, true, false);
    for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity()}) {
        NativeMenuMotion motion;
        motion.begin(1, 0, true, false, invalid, invalid);
        motion.begin(1, 100'000'000, true, false, invalid, invalid);
        CHECK(motion.moving());
        CHECK(motion.seconds() == doctest::Approx(normal.seconds()));
        CHECK(motion.entrance() == doctest::Approx(normal.entrance()));
    }
}
