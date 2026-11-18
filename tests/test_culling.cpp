#include "TestHelper.h"
#include "Core/Culling.h"
#include <cmath>
#include <limits>

namespace {

float Unit(test::Rng& rng) {
    return (float)(rng.Next() % 1000000u) / 1000000.0f;
}

struct Clip { float x, y, z, w; };

Clip ToClip(const Matrix& m, Vector3 p) {
    return {
        m.m0 * p.x + m.m4 * p.y + m.m8 * p.z + m.m12,
        m.m1 * p.x + m.m5 * p.y + m.m9 * p.z + m.m13,
        m.m2 * p.x + m.m6 * p.y + m.m10 * p.z + m.m14,
        m.m3 * p.x + m.m7 * p.y + m.m11 * p.z + m.m15,
    };
}

float Margin(const Clip& c) {
    return fminf(fminf(fminf(c.w - fabsf(c.x), c.w - fabsf(c.y)), c.w - fabsf(c.z)), c.w);
}

BoundingBox Tiny(Vector3 p) {
    const float h = 0.001f;
    return { { p.x - h, p.y - h, p.z - h }, { p.x + h, p.y + h, p.z + h } };
}

void CompareAgainstProjection(const Matrix& viewProj, uint64_t seed, float range) {
    Culling::Frustum f = Culling::FromViewProjection(viewProj);
    REQUIRE(f.valid);
    test::Rng rng(seed);
    int inside = 0, outside = 0;
    for (int i = 0; i < 20000; i++) {
        Vector3 p = { (Unit(rng) - 0.5f) * range, (Unit(rng) - 0.5f) * range, (Unit(rng) - 0.5f) * range };
        Clip c = ToClip(viewProj, p);
        float margin = Margin(c);
        if (fabsf(margin) < 0.5f) continue;
        bool truth = margin > 0.0f;
        bool result = Culling::Intersects(f, Tiny(p));
        if (truth) inside++; else outside++;
        if (truth != result) {
            CHECK_MSG(false, "mismatch at point " + std::to_string(p.x) + "," + std::to_string(p.y) + "," + std::to_string(p.z) +
                             " truth=" + std::to_string(truth));
            return;
        }
    }
    CHECK(inside > 200);
    CHECK(outside > 200);
}

}

TEST(perspective_matches_projection) {
    Matrix view = MatrixLookAt({ 100, 50, -300 }, { 20, 10, 400 }, { 0, 1, 0 });
    Matrix proj = MatrixPerspective(60.0 * DEG2RAD, 16.0 / 9.0, 5.0, 3000.0);
    CompareAgainstProjection(MatrixMultiply(view, proj), 11, 4000.0f);
}

TEST(orthographic_matches_projection) {
    Matrix view = MatrixLookAt({ 0, 800, 0 }, { 0, 0, 0 }, { 0, 0, 1 });
    Matrix proj = MatrixOrtho(-600.0, 600.0, -400.0, 400.0, 1.0, 1500.0);
    CompareAgainstProjection(MatrixMultiply(view, proj), 12, 1800.0f);
}

TEST(rotated_camera_matches_projection) {
    Matrix view = MatrixLookAt({ -500, 300, 250 }, { 700, -100, -300 }, { 0, 1, 0 });
    Matrix proj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 2.0, 2500.0);
    CompareAgainstProjection(MatrixMultiply(view, proj), 13, 4000.0f);
}

TEST(large_box_straddling_frustum_is_visible) {
    Matrix view = MatrixLookAt({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 });
    Matrix proj = MatrixPerspective(60.0 * DEG2RAD, 1.0, 5.0, 1000.0);
    Culling::Frustum f = Culling::FromViewProjection(MatrixMultiply(view, proj));
    BoundingBox huge = { { -1.0e5f, -1.0e5f, -1.0e5f }, { 1.0e5f, 1.0e5f, 1.0e5f } };
    CHECK(Culling::Intersects(f, huge));
    BoundingBox behind = { { -10, -10, -500 }, { 10, 10, -100 } };
    CHECK(!Culling::Intersects(f, behind));
    BoundingBox beyondFar = { { -10, -10, 2000 }, { 10, 10, 3000 } };
    CHECK(!Culling::Intersects(f, beyondFar));
    BoundingBox ahead = { { -10, -10, 100 }, { 10, 10, 200 } };
    CHECK(Culling::Intersects(f, ahead));
}

TEST(invalid_matrices_never_cull) {
    Matrix zero{};
    Culling::Frustum f = Culling::FromViewProjection(zero);
    CHECK(!f.valid);
    BoundingBox anywhere = { { 1.0e6f, 1.0e6f, 1.0e6f }, { 1.0e6f + 1, 1.0e6f + 1, 1.0e6f + 1 } };
    CHECK(Culling::Intersects(f, anywhere));

    Matrix nan = MatrixIdentity();
    nan.m0 = std::numeric_limits<float>::quiet_NaN();
    CHECK(!Culling::FromViewProjection(nan).valid);

    Matrix inf = MatrixIdentity();
    inf.m15 = std::numeric_limits<float>::infinity();
    CHECK(!Culling::FromViewProjection(inf).valid);
}

TEST(transform_box_rotation_and_scale) {
    BoundingBox box = { { -1, -2, -3 }, { 1, 2, 3 } };
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(2, 2, 2), MatrixRotateY(90.0f * DEG2RAD)), MatrixTranslate(10, 20, 30));
    BoundingBox out = Culling::TransformBox(box, m);
    CHECK(fabsf(out.min.x - (10 - 6)) < 1e-3f);
    CHECK(fabsf(out.max.x - (10 + 6)) < 1e-3f);
    CHECK(fabsf(out.min.y - (20 - 4)) < 1e-3f);
    CHECK(fabsf(out.max.y - (20 + 4)) < 1e-3f);
    CHECK(fabsf(out.min.z - (30 - 2)) < 1e-3f);
    CHECK(fabsf(out.max.z - (30 + 2)) < 1e-3f);

    Matrix mirror = MatrixScale(-1, 1, 1);
    BoundingBox flipped = Culling::TransformBox(box, mirror);
    CHECK(Culling::IsValidBox(flipped));
    CHECK(flipped.min.x < flipped.max.x);
}

TEST(distance_to_box) {
    BoundingBox box = { { 0, 0, 0 }, { 10, 10, 10 } };
    CHECK(Culling::DistanceSquaredToBox({ 5, 5, 5 }, box) == 0.0f);
    CHECK(fabsf(Culling::DistanceSquaredToBox({ 13, 5, 5 }, box) - 9.0f) < 1e-4f);
    CHECK(fabsf(Culling::DistanceSquaredToBox({ 13, 14, 5 }, box) - 25.0f) < 1e-4f);
    CHECK(!Culling::BeyondDistance({ 1000, 0, 0 }, box, 0.0f));
    CHECK(!Culling::BeyondDistance({ 1000, 0, 0 }, box, -5.0f));
    CHECK(Culling::BeyondDistance({ 1000, 0, 0 }, box, 500.0f));
    CHECK(!Culling::BeyondDistance({ 20, 0, 0 }, box, 500.0f));
    CHECK(!Culling::BeyondDistance({ 15, 5, 5 }, box, 5.0f));
    CHECK(Culling::BeyondDistance({ 16, 5, 5 }, box, 5.0f));
}

TEST(box_validation) {
    CHECK(Culling::IsValidBox({ { 0, 0, 0 }, { 1, 1, 1 } }));
    CHECK(!Culling::IsValidBox({ { 1, 0, 0 }, { 0, 1, 1 } }));
    CHECK(!Culling::IsValidBox({ { std::numeric_limits<float>::quiet_NaN(), 0, 0 }, { 1, 1, 1 } }));
    CHECK(!Culling::IsValidBox({ { 0, 0, 0 }, { std::numeric_limits<float>::infinity(), 1, 1 } }));
}

TEST_MAIN()
