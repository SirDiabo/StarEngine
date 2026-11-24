#include "TestHelper.h"
#include "Core/MapRange.h"
#include <cmath>
#include <limits>

TEST(unclamped_maps_linearly_and_extrapolates) {
    CHECK(std::fabs(MapRangeUnclamped(5.0f, 0.0f, 10.0f, 0.0f, 100.0f) - 50.0f) < 1e-4f);
    CHECK(std::fabs(MapRangeUnclamped(0.0f, 0.0f, 10.0f, 20.0f, 40.0f) - 20.0f) < 1e-4f);
    CHECK(std::fabs(MapRangeUnclamped(10.0f, 0.0f, 10.0f, 20.0f, 40.0f) - 40.0f) < 1e-4f);
    CHECK(std::fabs(MapRangeUnclamped(20.0f, 0.0f, 10.0f, 0.0f, 100.0f) - 200.0f) < 1e-3f);
    CHECK(std::fabs(MapRangeUnclamped(-10.0f, 0.0f, 10.0f, 0.0f, 100.0f) + 100.0f) < 1e-3f);
}

TEST(clamped_stays_inside_the_output_range) {
    CHECK(std::fabs(MapRangeClamped(20.0f, 0.0f, 10.0f, 0.0f, 100.0f) - 100.0f) < 1e-4f);
    CHECK(std::fabs(MapRangeClamped(-10.0f, 0.0f, 10.0f, 0.0f, 100.0f) - 0.0f) < 1e-4f);
    CHECK(std::fabs(MapRangeClamped(5.0f, 0.0f, 10.0f, 0.0f, 100.0f) - 50.0f) < 1e-4f);
    test::Rng rng(5);
    for (int i = 0; i < 5000; i++) {
        float v = (float)(rng.Next() % 200001) / 100.0f - 1000.0f;
        float outA = (float)(rng.Next() % 2001) / 10.0f - 100.0f;
        float outB = (float)(rng.Next() % 2001) / 10.0f - 100.0f;
        float r = MapRangeClamped(v, -50.0f, 50.0f, outA, outB);
        float lo = std::fmin(outA, outB), hi = std::fmax(outA, outB);
        CHECK(r >= lo - 1e-3f && r <= hi + 1e-3f);
    }
}

TEST(reversed_ranges_work) {
    CHECK(std::fabs(MapRangeUnclamped(2.0f, 10.0f, 0.0f, 0.0f, 100.0f) - 80.0f) < 1e-4f);
    CHECK(std::fabs(MapRangeClamped(2.0f, 10.0f, 0.0f, 0.0f, 100.0f) - 80.0f) < 1e-4f);
    CHECK(std::fabs(MapRangeClamped(20.0f, 10.0f, 0.0f, 0.0f, 100.0f) - 0.0f) < 1e-4f);
    CHECK(std::fabs(MapRangeClamped(5.0f, 0.0f, 10.0f, 100.0f, 0.0f) - 50.0f) < 1e-4f);
    CHECK(std::fabs(MapRangeClamped(20.0f, 0.0f, 10.0f, 100.0f, 0.0f) - 0.0f) < 1e-4f);
}

TEST(zero_width_input_range_returns_the_first_output_value) {
    CHECK(MapRangeUnclamped(5.0f, 3.0f, 3.0f, 7.0f, 9.0f) == 7.0f);
    CHECK(MapRangeClamped(5.0f, 3.0f, 3.0f, 7.0f, 9.0f) == 7.0f);
}

TEST(non_finite_and_extreme_inputs_never_produce_nan) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const float big = std::numeric_limits<float>::max();
    CHECK(MapRangeUnclamped(nan, 0, 10, 0, 100) == 0.0f);
    CHECK(MapRangeClamped(nan, 0, 10, 5, 100) == 5.0f);
    CHECK(MapRangeUnclamped(inf, 0, 10, 0, 100) == 0.0f);
    CHECK(MapRangeClamped(inf, 0, 10, 0, 100) == 100.0f);
    CHECK(MapRangeClamped(-inf, 0, 10, 0, 100) == 0.0f);
    CHECK(std::isfinite(MapRangeUnclamped(big, 0, 1, 0, big)));
    CHECK(std::isfinite(MapRangeClamped(big, -big, big, -big, big)));
    CHECK(MapRangeClamped(big, -big, big, -big, big) == big);
    CHECK(std::fabs(MapRangeClamped(0.0f, -big, big, -big, big)) < big * 1e-6f);
    CHECK(std::isfinite(MapRangeUnclamped(1.0f, 0.0f, 1e-30f, 0.0f, big)));
}

TEST_MAIN()
