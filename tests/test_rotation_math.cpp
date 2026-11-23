#include "TestHelper.h"
#include "Core/RotationMath.h"
#include <cmath>
#include <limits>

namespace {

float Unit(test::Rng& rng) {
    return (float)(rng.Next() % 1000001u) / 1000000.0f;
}

float Range(test::Rng& rng, float lo, float hi) {
    return lo + (hi - lo) * Unit(rng);
}

Vector3 RandomRotation(test::Rng& rng) {
    return { Range(rng, -180.0f, 180.0f), Range(rng, -180.0f, 180.0f), Range(rng, -180.0f, 180.0f) };
}

Vector3 Scaled(const Vector3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }

float Dist(const Vector3& a, const Vector3& b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

float MatrixDistance(const Matrix& a, const Matrix& b) {
    const float* pa = &a.m0;
    const float* pb = &b.m0;
    float worst = 0.0f;
    for (int i = 0; i < 16; i++) worst = std::fmax(worst, std::fabs(pa[i] - pb[i]));
    return worst;
}

bool SameOrientation(const Vector3& a, const Vector3& b, float tolerance) {
    return MatrixDistance(ComposeRotationDegrees(a), ComposeRotationDegrees(b)) < tolerance;
}

bool Orthonormal(const RotationAxes::Basis& b, float tolerance) {
    using namespace RotationAxes;
    return std::fabs(Dot3(b.right, b.right) - 1.0f) < tolerance && std::fabs(Dot3(b.up, b.up) - 1.0f) < tolerance &&
           std::fabs(Dot3(b.forward, b.forward) - 1.0f) < tolerance && std::fabs(Dot3(b.right, b.up)) < tolerance &&
           std::fabs(Dot3(b.up, b.forward)) < tolerance && std::fabs(Dot3(b.right, b.forward)) < tolerance &&
           Dist(Cross3(b.forward, b.up), b.right) < tolerance;
}

bool InCanonicalRange(const Vector3& r) {
    return r.x >= -90.001f && r.x <= 90.001f && r.y > -180.001f && r.y <= 180.001f && r.z > -180.001f && r.z <= 180.001f;
}

}

TEST(identity_basis_is_zero_rotation) {
    Vector3 r = RotationFromBasisDegrees({ -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 });
    CHECK(Dist(r, { 0, 0, 0 }) < 1e-4f);
    CHECK(Dist(RotationForwardVector({ 0, 0, 0 }), { 0, 0, 1 }) < 1e-6f);
    CHECK(Dist(RotationUpVector({ 0, 0, 0 }), { 0, 1, 0 }) < 1e-6f);
    CHECK(Dist(RotationRightVector({ 0, 0, 0 }), { -1, 0, 0 }) < 1e-6f);
}

TEST(direction_vectors_match_matrix_columns) {
    test::Rng rng(11);
    for (int i = 0; i < 2000; i++) {
        Vector3 rot = RandomRotation(rng);
        Matrix m = ComposeRotationDegrees(rot);
        CHECK(Dist(RotationForwardVector(rot), { m.m8, m.m9, m.m10 }) < 1e-6f);
        CHECK(Dist(RotationUpVector(rot), { m.m4, m.m5, m.m6 }) < 1e-6f);
        CHECK(Dist(RotationRightVector(rot), { -m.m0, -m.m1, -m.m2 }) < 1e-6f);
    }
}

TEST(direction_vectors_are_orthonormal) {
    test::Rng rng(12);
    for (int i = 0; i < 2000; i++) {
        Vector3 rot = RandomRotation(rng);
        RotationAxes::Basis b = { RotationRightVector(rot), RotationUpVector(rot), RotationForwardVector(rot) };
        CHECK(Orthonormal(b, 1e-5f));
    }
}

TEST(basis_to_euler_round_trip_random) {
    test::Rng rng(21);
    int bad = 0;
    for (int i = 0; i < 20000; i++) {
        Vector3 rot = RandomRotation(rng);
        Vector3 back = RotationFromBasisDegrees(RotationRightVector(rot), RotationUpVector(rot), RotationForwardVector(rot));
        if (!SameOrientation(rot, back, 2e-4f) || !InCanonicalRange(back)) bad++;
    }
    CHECK_MSG(bad == 0, std::to_string(bad) + " round trips failed");
}

TEST(basis_to_euler_gimbal_cases) {
    const float pitches[] = { 90.0f, -90.0f };
    for (float pitch : pitches) {
        for (float yaw = -180.0f; yaw <= 180.0f; yaw += 30.0f) {
            for (float roll = -180.0f; roll <= 180.0f; roll += 30.0f) {
                Vector3 rot = { pitch, yaw, roll };
                Vector3 back = RotationFromBasisDegrees(RotationRightVector(rot), RotationUpVector(rot), RotationForwardVector(rot));
                CHECK_MSG(SameOrientation(rot, back, 2e-3f), "gimbal pitch " + std::to_string(pitch));
                CHECK_MSG(std::fabs(back.y) < 1e-3f, "gimbal yaw must be 0");
                CHECK(std::fabs(std::fabs(back.x) - 90.0f) < 1e-3f);
            }
        }
    }
}

TEST(basis_to_euler_near_gimbal) {
    test::Rng rng(22);
    int bad = 0;
    for (int i = 0; i < 20000; i++) {
        float offset = Range(rng, -0.2f, 0.2f);
        Vector3 rot = { (i % 2 ? 90.0f : -90.0f) + offset, Range(rng, -180.0f, 180.0f), Range(rng, -180.0f, 180.0f) };
        Vector3 back = RotationFromBasisDegrees(RotationRightVector(rot), RotationUpVector(rot), RotationForwardVector(rot));
        if (!SameOrientation(rot, back, 5e-3f)) bad++;
    }
    CHECK_MSG(bad == 0, std::to_string(bad) + " near-gimbal round trips failed");
}

TEST(make_rot_from_single_axis_keeps_axis_and_zero_roll_or_yaw) {
    test::Rng rng(31);
    for (int i = 0; i < 5000; i++) {
        Vector3 rot = RandomRotation(rng);
        float scale = Range(rng, 0.01f, 1000.0f);

        Vector3 f = MakeRotFromZ(Scaled(RotationForwardVector(rot), scale));
        CHECK(Dist(RotationForwardVector(f), RotationForwardVector(rot)) < 1e-3f);
        CHECK(std::fabs(f.z) < 1e-2f || std::fabs(f.x) > 89.99f);
        CHECK(InCanonicalRange(f));

        Vector3 r = MakeRotFromX(Scaled(RotationRightVector(rot), scale));
        CHECK(Dist(RotationRightVector(r), RotationRightVector(rot)) < 1e-3f);

        Vector3 u = MakeRotFromY(Scaled(RotationUpVector(rot), scale));
        CHECK(Dist(RotationUpVector(u), RotationUpVector(rot)) < 1e-3f);
    }
}

TEST(make_rot_from_forward_matches_look_at) {
    test::Rng rng(32);
    for (int i = 0; i < 3000; i++) {
        Vector3 dir = { Range(rng, -1, 1), Range(rng, -1, 1), Range(rng, -1, 1) };
        if (std::sqrt(RotationAxes::Dot3(dir, dir)) < 0.05f) continue;
        Vector3 a = MakeRotFromZ(dir);
        Vector3 b = LookAtRotationDegrees({ 0, 0, 0 }, dir);
        CHECK(Dist(a, b) < 1e-2f);
    }
}

TEST(make_rot_from_two_axes_round_trip) {
    test::Rng rng(41);
    int bad = 0;
    for (int i = 0; i < 10000; i++) {
        Vector3 rot = RandomRotation(rng);
        Vector3 R = Scaled(RotationRightVector(rot), Range(rng, 0.1f, 50.0f));
        Vector3 U = Scaled(RotationUpVector(rot), Range(rng, 0.1f, 50.0f));
        Vector3 F = Scaled(RotationForwardVector(rot), Range(rng, 0.1f, 50.0f));
        const Vector3 results[] = {
            MakeRotFromXY(R, U), MakeRotFromXZ(R, F), MakeRotFromYX(U, R),
            MakeRotFromYZ(U, F), MakeRotFromZX(F, R), MakeRotFromZY(F, U),
        };
        for (const Vector3& r : results) {
            if (!SameOrientation(rot, r, 1e-3f) || !InCanonicalRange(r)) bad++;
        }
    }
    CHECK_MSG(bad == 0, std::to_string(bad) + " two-axis round trips failed");
}

TEST(make_rot_from_two_axes_first_axis_is_fixed_second_is_adjusted) {
    test::Rng rng(42);
    using namespace RotationAxes;
    for (int i = 0; i < 5000; i++) {
        Vector3 a = { Range(rng, -1, 1), Range(rng, -1, 1), Range(rng, -1, 1) };
        Vector3 b = { Range(rng, -1, 1), Range(rng, -1, 1), Range(rng, -1, 1) };
        Vector3 an = a, bn = b;
        if (!Normalize3(an, 0.2f) || !Normalize3(bn, 0.2f)) continue;
        if (std::fabs(Dot3(an, bn)) > 0.98f) continue;

        Vector3 zy = MakeRotFromZY(a, b);
        CHECK(Dist(RotationForwardVector(zy), an) < 2e-3f);
        CHECK(Dot3(RotationUpVector(zy), b) > 0.0f);

        Vector3 xz = MakeRotFromXZ(a, b);
        CHECK(Dist(RotationRightVector(xz), an) < 2e-3f);
        CHECK(Dot3(RotationForwardVector(xz), b) > 0.0f);

        Vector3 yx = MakeRotFromYX(a, b);
        CHECK(Dist(RotationUpVector(yx), an) < 2e-3f);
        CHECK(Dot3(RotationRightVector(yx), b) > 0.0f);

        Vector3 xy = MakeRotFromXY(a, b);
        CHECK(Dist(RotationRightVector(xy), an) < 2e-3f);
        CHECK(Dot3(RotationUpVector(xy), b) > 0.0f);

        Vector3 yz = MakeRotFromYZ(a, b);
        CHECK(Dist(RotationUpVector(yz), an) < 2e-3f);
        CHECK(Dot3(RotationForwardVector(yz), b) > 0.0f);

        Vector3 zx = MakeRotFromZX(a, b);
        CHECK(Dist(RotationForwardVector(zx), an) < 2e-3f);
        CHECK(Dot3(RotationRightVector(zx), b) > 0.0f);
    }
}

TEST(make_rot_from_second_axis_is_minimally_adjusted) {
    Vector3 rot = MakeRotFromZY({ 0, 0, 1 }, { 0, 1, 1 });
    CHECK(Dist(RotationForwardVector(rot), { 0, 0, 1 }) < 1e-4f);
    CHECK(Dist(RotationUpVector(rot), { 0, 1, 0 }) < 1e-4f);
    CHECK(Dist(rot, { 0, 0, 0 }) < 1e-3f);

    Vector3 tilted = MakeRotFromZY({ 0, 0, 1 }, { 1, 1, 0 });
    CHECK(Dist(RotationForwardVector(tilted), { 0, 0, 1 }) < 1e-4f);
    CHECK(Dist(RotationUpVector(tilted), { 0.70710678f, 0.70710678f, 0 }) < 1e-4f);
}

TEST(make_rot_from_all_results_are_orthonormal) {
    test::Rng rng(43);
    for (int i = 0; i < 5000; i++) {
        Vector3 a = { Range(rng, -5, 5), Range(rng, -5, 5), Range(rng, -5, 5) };
        Vector3 b = { Range(rng, -5, 5), Range(rng, -5, 5), Range(rng, -5, 5) };
        const Vector3 results[] = {
            MakeRotFromX(a), MakeRotFromY(a), MakeRotFromZ(a), MakeRotFromXY(a, b), MakeRotFromXZ(a, b),
            MakeRotFromYX(a, b), MakeRotFromYZ(a, b), MakeRotFromZX(a, b), MakeRotFromZY(a, b),
        };
        for (const Vector3& r : results) {
            RotationAxes::Basis basis = { RotationRightVector(r), RotationUpVector(r), RotationForwardVector(r) };
            CHECK(Orthonormal(basis, 1e-4f));
            CHECK(InCanonicalRange(r));
        }
    }
}

TEST(make_rot_from_degenerate_second_axis_falls_back_to_first_axis_alone) {
    Vector3 axes[] = { { 0.3f, -0.4f, 0.8f }, { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, { 5, 5, 5 } };
    for (const Vector3& a : axes) {
        Vector3 zero = { 0, 0, 0 };
        Vector3 parallel = Scaled(a, -3.0f);
        CHECK(Dist(MakeRotFromZY(a, parallel), MakeRotFromZ(a)) < 1e-4f);
        CHECK(Dist(MakeRotFromZY(a, zero), MakeRotFromZ(a)) < 1e-4f);
        CHECK(Dist(MakeRotFromZX(a, parallel), MakeRotFromZ(a)) < 1e-4f);
        CHECK(Dist(MakeRotFromXZ(a, parallel), MakeRotFromX(a)) < 1e-4f);
        CHECK(Dist(MakeRotFromXY(a, zero), MakeRotFromX(a)) < 1e-4f);
        CHECK(Dist(MakeRotFromYX(a, parallel), MakeRotFromY(a)) < 1e-4f);
        CHECK(Dist(MakeRotFromYZ(a, zero), MakeRotFromY(a)) < 1e-4f);
    }
}

TEST(make_rot_from_zero_or_invalid_first_axis_is_zero_rotator) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    Vector3 bad[] = { { 0, 0, 0 }, { nan, 0, 1 }, { 0, inf, 0 }, { 1e-30f, 0, 0 } };
    Vector3 good = { 0, 1, 0 };
    for (const Vector3& a : bad) {
        const Vector3 results[] = {
            MakeRotFromX(a), MakeRotFromY(a), MakeRotFromZ(a), MakeRotFromXY(a, good), MakeRotFromXZ(a, good),
            MakeRotFromYX(a, good), MakeRotFromYZ(a, good), MakeRotFromZX(a, good), MakeRotFromZY(a, good),
        };
        for (const Vector3& r : results) CHECK(Dist(r, { 0, 0, 0 }) == 0.0f);
    }
}

TEST(make_rot_from_invalid_second_axis_still_gives_valid_rotation) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Vector3 a = { 0.2f, 0.9f, -0.3f };
    Vector3 r = MakeRotFromZY(a, { nan, 1, 0 });
    CHECK(std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.z));
    CHECK(Dist(r, MakeRotFromZ(a)) < 1e-4f);
}

TEST(make_rot_from_vertical_single_axis_keeps_yaw_zero) {
    Vector3 up = MakeRotFromZ({ 0, 1, 0 });
    CHECK(Dist(up, { 90, 0, 0 }) < 1e-3f);
    Vector3 down = MakeRotFromZ({ 0, -1, 0 });
    CHECK(Dist(down, { -90, 0, 0 }) < 1e-3f);
    CHECK(Dist(MakeRotFromY({ 0, 1, 0 }), { 0, 0, 0 }) < 1e-4f);
    CHECK(Dist(MakeRotFromX({ -1, 0, 0 }), { 0, 0, 0 }) < 1e-4f);
}

TEST(make_rot_from_nearly_vertical_forward_is_stable) {
    test::Rng rng(51);
    for (int i = 0; i < 3000; i++) {
        Vector3 dir = { Range(rng, -1e-3f, 1e-3f), 1.0f, Range(rng, -1e-3f, 1e-3f) };
        Vector3 r = MakeRotFromZ(dir);
        Vector3 n = dir;
        RotationAxes::Normalize3(n, 1e-6f);
        CHECK(Dist(RotationForwardVector(r), n) < 1e-3f);
    }
}

TEST(shortest_path_helpers_do_not_wrap_automatically) {
    Vector3 raw = { 270.0f, -400.0f, 720.0f };
    Matrix a = ComposeRotationDegrees(raw);
    Matrix b = ComposeRotationDegrees(WrapRotationDegrees(raw));
    CHECK(MatrixDistance(a, b) < 1e-4f);
    CHECK(WrapAngleDegrees(270.0f) == -90.0f);
    CHECK(std::fabs(AngleDeltaDegrees(170.0f, -170.0f) - 20.0f) < 1e-4f);
}

TEST(convention_positive_pitch_looks_up) {
    CHECK(Dist(RotationForwardVector({ 90, 0, 0 }), { 0, 1, 0 }) < 1e-5f);
    CHECK(Dist(RotationForwardVector({ -90, 0, 0 }), { 0, -1, 0 }) < 1e-5f);
    Vector3 f = RotationForwardVector({ 10, 0, 0 });
    CHECK(f.y > 0.17f && f.y < 0.18f);
}

TEST(convention_positive_yaw_turns_right) {
    Vector3 right = RotationRightVector({ 0, 0, 0 });
    for (float yaw : { 5.0f, 30.0f, 90.0f, 170.0f }) {
        Vector3 f = RotationForwardVector({ 0, yaw, 0 });
        Vector3 expected = { right.x * std::sin(yaw * DEG2RAD), 0.0f, std::cos(yaw * DEG2RAD) };
        CHECK(Dist(f, expected) < 1e-5f);
    }
    CHECK(Dist(RotationForwardVector({ 0, 90, 0 }), RotationRightVector({ 0, 0, 0 })) < 1e-5f);
    CHECK(Dist(RotationForwardVector({ 0, -90, 0 }), Scaled(RotationRightVector({ 0, 0, 0 }), -1.0f)) < 1e-5f);
}

TEST(convention_positive_roll_is_clockwise) {
    Vector3 right = RotationRightVector({ 0, 0, 0 });
    CHECK(Dist(RotationUpVector({ 0, 0, 90 }), right) < 1e-5f);
    CHECK(Dist(RotationRightVector({ 0, 0, 90 }), Scaled(RotationUpVector({ 0, 0, 0 }), -1.0f)) < 1e-5f);
    CHECK(Dist(RotationForwardVector({ 0, 0, 90 }), { 0, 0, 1 }) < 1e-6f);
}

TEST(convention_yaw_then_pitch_then_roll_order) {
    Vector3 f = RotationForwardVector({ 30, 90, 45 });
    Vector3 yawOnly = RotationForwardVector({ 0, 90, 0 });
    CHECK(Dist(Scaled(yawOnly, std::cos(30 * DEG2RAD)), { f.x, 0, f.z }) < 1e-5f);
    CHECK(f.y > 0.49f && f.y < 0.51f);
}

TEST(look_at_matches_convention) {
    CHECK(Dist(LookAtRotationDegrees({ 0, 0, 0 }, { 0, 0, 1 }), { 0, 0, 0 }) < 1e-4f);
    CHECK(Dist(LookAtRotationDegrees({ 0, 0, 0 }, { 0, 5, 0 }), { 90, 0, 0 }) < 1e-3f);
    CHECK(Dist(LookAtRotationDegrees({ 0, 0, 0 }, { 0, -5, 0 }), { -90, 0, 0 }) < 1e-3f);
    Vector3 right = RotationRightVector({ 0, 0, 0 });
    Vector3 toRight = Scaled(right, 3.0f);
    CHECK(Dist(LookAtRotationDegrees({ 0, 0, 0 }, toRight), { 0, 90, 0 }) < 1e-3f);
    CHECK(Dist(LookAtRotationDegrees({ 0, 0, 0 }, Scaled(toRight, -1.0f)), { 0, -90, 0 }) < 1e-3f);
    test::Rng rng(61);
    for (int i = 0; i < 5000; i++) {
        Vector3 from = { Range(rng, -50, 50), Range(rng, -50, 50), Range(rng, -50, 50) };
        Vector3 to = { Range(rng, -50, 50), Range(rng, -50, 50), Range(rng, -50, 50) };
        Vector3 d = { to.x - from.x, to.y - from.y, to.z - from.z };
        Vector3 n = d;
        if (!RotationAxes::Normalize3(n, 1e-2f)) continue;
        Vector3 look = LookAtRotationDegrees(from, to);
        CHECK(Dist(RotationForwardVector(look), n) < 1e-4f);
        CHECK(std::fabs(look.z) < 1e-6f);
    }
}

TEST(rotate_around_world_axis_matches_matrix_product) {
    test::Rng rng(62);
    const Vector3 axes[] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 0.3f, -0.5f, 0.8f } };
    for (int i = 0; i < 4000; i++) {
        Vector3 rot = RandomRotation(rng);
        Vector3 axis = axes[i % 4];
        float delta = Range(rng, -180.0f, 180.0f);
        Vector3 result = RotateRotationAroundAxisDegrees(rot, axis, delta);
        Matrix expected = MatrixMultiply(ComposeRotationDegrees(rot), MatrixRotate(axis, delta * DEG2RAD));
        CHECK(MatrixDistance(ComposeRotationDegrees(result), expected) < 5e-4f);
        CHECK(InCanonicalRange(result));
        Vector3 back = RotateRotationAroundAxisDegrees(result, axis, -delta);
        CHECK(SameOrientation(back, rot, 2e-3f));
    }
}

TEST(rotation_from_matrix_round_trips_compose) {
    test::Rng rng(63);
    for (int i = 0; i < 5000; i++) {
        Vector3 rot = RandomRotation(rng);
        Vector3 back = RotationFromMatrixDegrees(ComposeRotationDegrees(rot));
        CHECK(SameOrientation(rot, back, 2e-4f));
        CHECK(InCanonicalRange(back));
    }
}

TEST_MAIN()
