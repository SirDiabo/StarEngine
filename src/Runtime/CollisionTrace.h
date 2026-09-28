#pragma once
#include "raylib.h"
#include <vector>

class Actor;
class Component;
class CollisionComponent;

namespace CollisionTrace {

    struct Shape {
        enum Type { Sphere, Capsule, Box };
        Type type = Sphere;
        Vector3 center{ 0, 0, 0 };
        Vector3 a{ 0, 0, 0 };
        Vector3 b{ 0, 0, 0 };
        float radius = 0.0f;
        Vector3 axis[3]{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
        Vector3 half{ 0, 0, 0 };
    };

    struct QueryParams {
        Actor* ignoreActor = nullptr;
        bool meshes = true;
        bool shapes = true;
    };

    struct HitResult {
        Actor* actor = nullptr;
        Component* component = nullptr;
        Vector3 point{ 0, 0, 0 };
        Vector3 normal{ 0, 1, 0 };
        float distance = 0.0f;
        float time = 0.0f;
        float penetration = 0.0f;
        bool startPenetrating = false;
    };

    Shape MakeSphere(const Vector3& center, float radius);
    Shape MakeCapsule(const Vector3& a, const Vector3& b, float radius);
    Shape MakeBox(const Vector3& center, const Vector3& rotationDegrees, const Vector3& halfExtents);
    bool GetComponentShape(const Actor* actor, const CollisionComponent* component, Shape& outShape);
    bool MakeComponentShape(int shapeType, float radius, float height, const Vector3& boxExtents,
                             const Vector3& pos, const Vector3& rot, const Vector3& scl, Shape& outShape);
    void DrawDebugShape(const Shape& shape, Color color);

    bool TraceAgainstMeshes(const Vector3& start, const Vector3& end, Actor* exclude,
                             Actor*& outActor, Vector3& outLocation, Vector3& outNormal, float& outDistance);

    bool Raycast(const Vector3& start, const Vector3& end, const QueryParams& params, HitResult& outHit);

    bool OverlapShape(const Shape& shape, const QueryParams& params, HitResult& outHit);
    int OverlapShapeAll(const Shape& shape, const QueryParams& params, std::vector<HitResult>& outHits);
    bool OverlapSphere(const Vector3& center, float radius, const QueryParams& params, HitResult& outHit);
    bool OverlapCapsule(const Vector3& a, const Vector3& b, float radius, const QueryParams& params, HitResult& outHit);
    bool OverlapBox(const Vector3& center, const Vector3& rotationDegrees, const Vector3& halfExtents,
                     const QueryParams& params, HitResult& outHit);

    bool SweepShape(const Shape& shape, const Vector3& delta, const QueryParams& params, HitResult& outHit);
    bool SweepSphere(const Vector3& center, float radius, const Vector3& delta, const QueryParams& params, HitResult& outHit);
    bool SweepCapsule(const Vector3& a, const Vector3& b, float radius, const Vector3& delta,
                       const QueryParams& params, HitResult& outHit);
    bool SweepBox(const Vector3& center, const Vector3& rotationDegrees, const Vector3& halfExtents, const Vector3& delta,
                   const QueryParams& params, HitResult& outHit);

    void BeginBatch();
    void EndBatch();

    class ScopedBatch {
    public:
        ScopedBatch() { BeginBatch(); }
        ~ScopedBatch() { EndBatch(); }
        ScopedBatch(const ScopedBatch&) = delete;
        ScopedBatch& operator=(const ScopedBatch&) = delete;
    };

    void DrawDebugShapes();
}
