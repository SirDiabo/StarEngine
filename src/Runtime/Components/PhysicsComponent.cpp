#include "PhysicsComponent.h"
#include "Runtime/Actor.h"
#include "Runtime/ComponentEvents.h"
#include "Core/Profiler.h"
#include "Runtime/ComponentRegistry.h"
#include "Core/SafeParse.h"
#include "raymath.h"
#include <cmath>
#include <cstdio>
#include <string>

namespace {

Vector3 Plus(const Vector3& a, const Vector3& b) { return Vector3{ a.x + b.x, a.y + b.y, a.z + b.z }; }
Vector3 Minus(const Vector3& a, const Vector3& b) { return Vector3{ a.x - b.x, a.y - b.y, a.z - b.z }; }
Vector3 Times(const Vector3& a, float s) { return Vector3{ a.x * s, a.y * s, a.z * s }; }
float Dot3(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vector3 ClipToPlanes(Vector3 v, const Vector3* planes, int count) {
    for (int i = 0; i < count; i++) {
        float d = Dot3(v, planes[i]);
        if (d < 0.0f) v = Minus(v, Times(planes[i], d));
    }
    for (int i = 0; i < count; i++) {
        if (Dot3(v, planes[i]) < -1e-3f) {
            if (count >= 2) {
                Vector3 crease = Vector3Normalize(Vector3CrossProduct(planes[0], planes[1]));
                return Times(crease, Dot3(v, crease));
            }
            return Vector3{ 0.0f, 0.0f, 0.0f };
        }
    }
    return v;
}

}

void PhysicsComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddBool("Simulate Physics", &simulatePhysics);
    outProperties.AddBool("Use Gravity", &useGravity);
    outProperties.AddFloat("Gravity", &gravity);
    outProperties.AddFloat("Gravity Scale", &gravityScale);
    outProperties.AddFloat("Mass", &mass);
    outProperties.AddFloat("Linear Drag", &linearDrag);
    outProperties.AddFloat("Ground Friction", &groundFriction);
    outProperties.AddFloat("Max Fall Speed", &maxFallSpeed);
    outProperties.AddFloat("Max Slope Angle", &maxSlopeAngle);
    outProperties.AddFloat("Ground Snap Distance", &groundSnapDistance);
    outProperties.AddFloat("Skin Width", &skinWidth);
    outProperties.AddFloat("Ground Snap Smoothing", &groundSnapSmoothing);
    outProperties.AddFloat("Bounciness", &bounciness);
    outProperties.AddVector3("Velocity", &velocity.x);
    outProperties.AddBool("Grounded", &grounded, true);
}

void PhysicsComponent::SetVelocity(const Vector3& newVelocity) {
    velocity = newVelocity;
}

void PhysicsComponent::AddImpulse(const Vector3& impulse) {
    float m = mass > 0.0001f ? mass : 0.0001f;
    velocity = Plus(velocity, Times(impulse, 1.0f / m));
}

Vector3 PhysicsComponent::Depenetrate(CollisionTrace::Shape& shape, const CollisionTrace::QueryParams& params, float dtForDepenetrate) {
    Vector3 moved{ 0.0f, 0.0f, 0.0f };
    float smoothT = 1.0f - powf(1.0f - groundSnapSmoothing, dtForDepenetrate * 60.0f);
    for (int iter = 0; iter < 4; iter++) {
        int count = CollisionTrace::OverlapShapeAll(shape, params, scratchHits);
        if (count == 0) break;
        const CollisionTrace::HitResult* deepest = &scratchHits[0];
        for (const CollisionTrace::HitResult& h : scratchHits) {
            if (h.penetration > deepest->penetration) deepest = &h;
        }
        if (deepest->penetration <= 1e-3f) break;
        Vector3 n = deepest->normal;
        float pushAmount = deepest->penetration <= skinWidth * 2.0f
            ? deepest->penetration * smoothT + 0.01f
            : deepest->penetration + 0.01f;
        Vector3 push = Times(n, pushAmount);
        shape = CollisionTrace::ShapeTranslated(shape, push);
        moved = Plus(moved, push);
        float vn = Dot3(velocity, n);
        if (vn < 0.0f) velocity = Minus(velocity, Times(n, vn));
    }
    return moved;
}

Vector3 PhysicsComponent::MoveAndSlide(CollisionTrace::Shape& shape, const Vector3& delta, const CollisionTrace::QueryParams& params) {
    Vector3 applied{ 0.0f, 0.0f, 0.0f };
    Vector3 remaining = delta;
    Vector3 planes[3];
    int planeCount = 0;

    for (int bump = 0; bump < 4; bump++) {
        float len = Vector3Length(remaining);
        if (len < 1e-4f) break;

        CollisionTrace::HitResult hit;
        if (!CollisionTrace::SweepShape(shape, remaining, params, hit)) {
            shape = CollisionTrace::ShapeTranslated(shape, remaining);
            applied = Plus(applied, remaining);
            break;
        }

        if (!hit.startPenetrating) {
            float moveDist = hit.distance - skinWidth;
            if (moveDist < 0.0f) moveDist = 0.0f;
            Vector3 step = Times(remaining, moveDist / len);
            shape = CollisionTrace::ShapeTranslated(shape, step);
            applied = Plus(applied, step);
            remaining = Minus(remaining, step);
        }

        Vector3 n = hit.normal;
        float vn = Dot3(velocity, n);
        if (bounciness > 0.0f && vn < -50.0f) {
            velocity = Minus(velocity, Times(n, vn * (1.0f + bounciness)));
            remaining = Vector3{ 0.0f, 0.0f, 0.0f };
            break;
        }

        if (planeCount < 3) planes[planeCount++] = n;
        velocity = ClipToPlanes(velocity, planes, planeCount);
        remaining = ClipToPlanes(remaining, planes, planeCount);
    }
    return applied;
}

Vector3 PhysicsComponent::UpdateGround(CollisionTrace::Shape& shape, const CollisionTrace::QueryParams& params, bool wasGrounded, float dtForGround) {
    grounded = false;
    groundNormal = Vector3{ 0.0f, 1.0f, 0.0f };
    Vector3 applied{ 0.0f, 0.0f, 0.0f };

    float checkDist = wasGrounded ? groundSnapDistance + skinWidth * 2.0f : skinWidth * 2.0f;

    CollisionTrace::HitResult hit;
    if (!CollisionTrace::SweepShape(shape, Vector3{ 0.0f, -checkDist, 0.0f }, params, hit)) return applied;

    float cosSlope = cosf(maxSlopeAngle * DEG2RAD);
    if (hit.normal.y < cosSlope) return applied;
    if (velocity.y > 1.0f && Dot3(velocity, hit.normal) > 1.0f) return applied;

    grounded = true;
    groundNormal = hit.normal;
    if (wasGrounded && !hit.startPenetrating) {
        float drop = hit.distance - skinWidth;
        if (drop > 0.0f) {
            float smoothT = 1.0f - powf(1.0f - groundSnapSmoothing, dtForGround * 60.0f);
            float applyDrop = drop <= skinWidth * 2.0f ? drop * smoothT : drop;
            applied = Vector3{ 0.0f, -applyDrop, 0.0f };
            shape = CollisionTrace::ShapeTranslated(shape, applied);
        }
    }
    float vn = Dot3(velocity, hit.normal);
    if (vn < 0.0f) velocity = Minus(velocity, Times(hit.normal, vn));
    return applied;
}

void PhysicsComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);

    Actor* actor = GetOwningActor();
    if (!actor || !simulatePhysics) return;

    float dt = deltaTime > 0.05f ? 0.05f : deltaTime;
    if (dt <= 0.0f) return;

    if (useGravity && !(grounded && velocity.y <= 0.0f)) {
        velocity.y -= gravity * gravityScale * dt;
    }
    if (grounded && velocity.y < 0.0f) velocity.y = 0.0f;
    if (linearDrag > 0.0f) velocity = Times(velocity, 1.0f / (1.0f + linearDrag * dt));
    if (grounded && groundFriction > 0.0f) {
        float f = 1.0f - groundFriction * dt;
        if (f < 0.0f) f = 0.0f;
        velocity.x *= f;
        velocity.z *= f;
    }
    if (maxFallSpeed > 0.0f && velocity.y < -maxFallSpeed) velocity.y = -maxFallSpeed;

    CollisionComponent* body = actor->GetCollisionComponent();
    CollisionTrace::Shape shape;
    if (!body || !CollisionTrace::GetComponentShape(actor, body, shape)) {
        actor->position = Plus(actor->position, Times(velocity, dt));
        grounded = false;
        return;
    }

    CollisionTrace::QueryParams params;
    params.ignoreActor = actor;
    params.layer = body->collisionLayer;
    params.mask = body->collisionMask;
    CollisionTrace::ScopedBatch batch;
    const bool wasGrounded = grounded;
    const float preMoveFallSpeed = velocity.y < 0.0f ? -velocity.y : 0.0f;
    const Vector3 preMoveVelocity = velocity;

    Vector3 total;
    {
        PROFILE_SCOPE("Depenetrate");
        total = Depenetrate(shape, params, dt);
    }
    {
        PROFILE_SCOPE("Move And Slide");
        total = Plus(total, MoveAndSlide(shape, Times(velocity, dt), params));
    }
    {
        PROFILE_SCOPE("Update Ground");
        total = Plus(total, UpdateGround(shape, params, wasGrounded, dt));
    }

    actor->position = Plus(actor->position, total);

    if (!wasGrounded && grounded) {
        velocity = Vector3{ preMoveVelocity.x, 0.0f, preMoveVelocity.z };
        char fallSpeedBuf[32];
        snprintf(fallSpeedBuf, sizeof(fallSpeedBuf), "%g", preMoveFallSpeed);
        ComponentEvents::Raise(this, "Landed", { { "Fall Speed", fallSpeedBuf } });
    }
}

namespace {

ComponentFieldDesc FloatField(const std::string& key, const std::string& label, float def, float PhysicsComponent::* member) {
    return ComponentField::Flt(key, label, def, [key, def, member](Component& c, const ComponentDecl& d) {
        static_cast<PhysicsComponent&>(c).*member = SafeParse::ToFloat(d.Get(key, std::to_string(def)));
    }, [member](const Component& c) { return ComponentField::FloatStr(static_cast<const PhysicsComponent&>(c).*member); });
}

ComponentFieldDesc BoolField(const std::string& key, const std::string& label, bool def, bool PhysicsComponent::* member) {
    return ComponentField::Bln(key, label, def, [key, def, member](Component& c, const ComponentDecl& d) {
        static_cast<PhysicsComponent&>(c).*member = d.Get(key, def ? "true" : "false") == "true";
    }, [member](const Component& c) { return ComponentField::BoolStr(static_cast<const PhysicsComponent&>(c).*member); });
}

ComponentEventRegistrar registrar_PhysicsComponentEvents({
    "PhysicsComponent", "Landed", { { "Fall Speed", EventValueType::Float } }
});

ComponentRegistrar registrar_PhysicsComponent({
    "PhysicsComponent",
    []() -> Component* { return new PhysicsComponent(); },
    {
        BoolField("SimulatePhysics", "Simulate Physics", true, &PhysicsComponent::simulatePhysics),
        BoolField("UseGravity", "Use Gravity", true, &PhysicsComponent::useGravity),
        FloatField("Gravity", "Gravity", 980.0f, &PhysicsComponent::gravity),
        FloatField("GravityScale", "Gravity Scale", 1.0f, &PhysicsComponent::gravityScale),
        FloatField("Mass", "Mass", 1.0f, &PhysicsComponent::mass),
        FloatField("LinearDrag", "Linear Drag", 0.0f, &PhysicsComponent::linearDrag),
        FloatField("GroundFriction", "Ground Friction", 0.0f, &PhysicsComponent::groundFriction),
        FloatField("MaxFallSpeed", "Max Fall Speed", 0.0f, &PhysicsComponent::maxFallSpeed),
        FloatField("MaxSlopeAngle", "Max Slope Angle", 50.0f, &PhysicsComponent::maxSlopeAngle),
        FloatField("GroundSnapDistance", "Ground Snap Distance", 20.0f, &PhysicsComponent::groundSnapDistance),
        FloatField("SkinWidth", "Skin Width", 0.5f, &PhysicsComponent::skinWidth),
        FloatField("GroundSnapSmoothing", "Ground Snap Smoothing", 0.4f, &PhysicsComponent::groundSnapSmoothing),
        FloatField("Bounciness", "Bounciness", 0.0f, &PhysicsComponent::bounciness),
    }
});

}
