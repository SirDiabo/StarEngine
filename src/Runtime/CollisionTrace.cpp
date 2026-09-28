#include "CollisionTrace.h"
#include "CollisionPrivate.h"
#include "Actor.h"
#include "World.h"
#include "AssetCache.h"
#include "DebugDraw.h"
#include "Components/MeshComponent.h"
#include "Components/CollisionComponent.h"
#include "Core/StarAsset.h"
#include "raymath.h"
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>

using namespace CollisionPrivate;

std::shared_ptr<const ModelBVH> CollisionPrivate::BuildModelBVH(const Model& model) {
    auto out = std::make_shared<ModelBVH>();
    for (int i = 0; i < model.meshCount; i++) {
        const Mesh& m = model.meshes[i];
        if (m.boneWeights != nullptr || m.boneIndices != nullptr) out->skinned = true;
    }
    if (out->skinned) return out;

    for (int mi = 0; mi < model.meshCount; mi++) {
        const Mesh& m = model.meshes[mi];
        if (m.vertices == nullptr || m.triangleCount <= 0 || m.vertexCount <= 0) continue;

        std::vector<Tri> tris;
        tris.reserve((size_t)m.triangleCount);
        for (int t = 0; t < m.triangleCount; t++) {
            unsigned int i0, i1, i2;
            if (m.indices != nullptr) {
                i0 = m.indices[t * 3 + 0];
                i1 = m.indices[t * 3 + 1];
                i2 = m.indices[t * 3 + 2];
            } else {
                i0 = (unsigned int)(t * 3 + 0);
                i1 = (unsigned int)(t * 3 + 1);
                i2 = (unsigned int)(t * 3 + 2);
            }
            unsigned int vc = (unsigned int)m.vertexCount;
            if (i0 >= vc || i1 >= vc || i2 >= vc) continue;
            const float* v = m.vertices;
            Tri tri{ { v[i0 * 3], v[i0 * 3 + 1], v[i0 * 3 + 2] },
                     { v[i1 * 3], v[i1 * 3 + 1], v[i1 * 3 + 2] },
                     { v[i2 * 3], v[i2 * 3 + 1], v[i2 * 3 + 2] } };
            if (LenSq(Cross(Sub(tri.b, tri.a), Sub(tri.c, tri.a))) < 1e-24f) continue;
            tris.push_back(tri);
        }
        if (tris.empty()) continue;

        std::vector<AABB> boxes(tris.size());
        for (size_t i = 0; i < tris.size(); i++) {
            AABB b = EmptyAABB();
            Grow(b, tris[i].a);
            Grow(b, tris[i].b);
            Grow(b, tris[i].c);
            boxes[i] = b;
        }

        MeshBVH mb;
        std::vector<uint32_t> order;
        BuildBVH(mb.tree, boxes, order, 4);
        mb.tris.resize(tris.size());
        for (size_t i = 0; i < order.size(); i++) mb.tris[i] = tris[order[i]];

        Grow(out->bounds, AABB{ mb.tree.nodes[0].mn, mb.tree.nodes[0].mx });
        out->meshes.push_back(std::move(mb));
    }
    return out;
}

namespace {

using CollisionTrace::HitResult;
using CollisionTrace::QueryParams;

Matrix ComposeWorld(const Vector3& pos, const Vector3& rot, const Vector3& scl) {
    Matrix scaleM = MatrixScale(scl.x, scl.y, scl.z);
    Matrix rotM = MatrixRotateXYZ({ DEG2RAD * rot.x, DEG2RAD * rot.y, DEG2RAD * rot.z });
    Matrix transM = MatrixTranslate(pos.x, pos.y, pos.z);
    return MatrixMultiply(MatrixMultiply(scaleM, rotM), transM);
}

constexpr int kKeyCapacity = 80;

struct Record {
    Component* comp = nullptr;
    Actor* actor = nullptr;
    bool isMesh = false;
    uint32_t stamp = 0;
    float key[kKeyCapacity];
    int keyCount = -1;
    Matrix world{};
    Matrix inv{};
    AABB box{};
    Shape shape{};
    std::string meshPath;
    unsigned bvhGeneration = ~0u;
    std::shared_ptr<const ModelBVH> model;
};

int BuildKey(const Actor* actor, const Component* comp, float* k) {
    int n = 0;
    auto push = [&](const Vector3& p, const Vector3& r, const Vector3& s) {
        k[n++] = p.x; k[n++] = p.y; k[n++] = p.z;
        k[n++] = r.x; k[n++] = r.y; k[n++] = r.z;
        k[n++] = s.x; k[n++] = s.y; k[n++] = s.z;
    };
    push(actor->position, actor->rotation, actor->scale);
    int depth = 0;
    for (const Component* c = comp; c; c = c->parent) {
        if (depth == 7) return -1;
        push(c->position, c->rotation, c->scale);
        depth++;
    }
    return n;
}

std::shared_ptr<const ModelBVH> LoadModelBVH(const std::string& meshAssetPath) {
    if (meshAssetPath.empty()) return nullptr;
    StarAsset asset;
    if (!StarAsset::LoadFromFile(meshAssetPath, asset) || asset.assetType != "StaticMesh") return nullptr;
    return AssetCache::Get().GetOrBuildModelBVH(asset.ResolveSourcePath());
}

bool Accept(const Record& r, const QueryParams& qp) {
    if (r.actor == qp.ignoreActor && qp.ignoreActor != nullptr) return false;
    return r.isMesh ? qp.meshes : qp.shapes;
}

struct Scene {
    std::unordered_map<const Component*, Record> records;
    std::vector<Record*> active;
    BVH broad;
    std::vector<uint32_t> broadOrder;
    bool broadValid = false;
    int batchDepth = 0;
    bool frozen = false;
    uint32_t stamp = 0;
    std::vector<Record*> scratch;

    static Scene& Get() {
        static Scene s;
        return s;
    }

    void Refresh();
    void BuildBroad();
    void EnsureFresh();
    void Gather(const AABB& q, const QueryParams& qp, std::vector<Record*>& out);
};

void Scene::Refresh() {
    ++stamp;
    active.clear();
    size_t seen = 0;
    const unsigned generation = AssetCache::Get().GetCollisionGeneration();

    for (const auto& actorPtr : World::Get().GetActors()) {
        Actor* actor = actorPtr.get();
        if (!actor) continue;
        for (Component* comp : actor->GetComponents()) {
            if (!comp) continue;
            MeshComponent* mc = dynamic_cast<MeshComponent*>(comp);
            CollisionComponent* cc = mc ? nullptr : dynamic_cast<CollisionComponent*>(comp);
            if (!mc && !cc) continue;
            if (mc && mc->collisionMode == MeshComponent::CollisionNone) continue;

            Record& rec = records[comp];
            if (rec.comp != comp) {
                rec = Record();
                rec.comp = comp;
                rec.isMesh = (mc != nullptr);
            }
            rec.actor = actor;
            rec.stamp = stamp;
            ++seen;

            if (mc) {
                if (rec.meshPath != mc->meshPath || rec.bvhGeneration != generation) {
                    rec.meshPath = mc->meshPath;
                    rec.bvhGeneration = generation;
                    rec.model = LoadModelBVH(mc->meshPath);
                    rec.keyCount = -1;
                }
                if (!rec.model || rec.model->skinned || rec.model->meshes.empty()) continue;

                float k[kKeyCapacity];
                int kc = BuildKey(actor, comp, k);
                bool dirty = kc < 0 || kc != rec.keyCount || std::memcmp(k, rec.key, sizeof(float) * (size_t)kc) != 0;
                if (dirty) {
                    Vector3 pos, rot, scl;
                    actor->GetComponentWorldTransform(comp, pos, rot, scl);
                    if (fabsf(scl.x) < 1e-6f || fabsf(scl.y) < 1e-6f || fabsf(scl.z) < 1e-6f) {
                        rec.keyCount = -1;
                        continue;
                    }
                    rec.world = ComposeWorld(pos, rot, scl);
                    rec.inv = MatrixInvert(rec.world);
                    rec.box = TransformAABB(rec.model->bounds, rec.world);
                    if (kc >= 0) {
                        std::memcpy(rec.key, k, sizeof(float) * (size_t)kc);
                        rec.keyCount = kc;
                    } else {
                        rec.keyCount = -1;
                    }
                }
                active.push_back(&rec);
            } else {
                float k[kKeyCapacity];
                int kc = BuildKey(actor, comp, k);
                if (kc >= 0) {
                    k[kc++] = (float)cc->shapeType;
                    k[kc++] = cc->radius;
                    k[kc++] = cc->height;
                    k[kc++] = cc->boxExtents.x;
                    k[kc++] = cc->boxExtents.y;
                    k[kc++] = cc->boxExtents.z;
                }
                bool dirty = kc < 0 || kc != rec.keyCount || std::memcmp(k, rec.key, sizeof(float) * (size_t)kc) != 0;
                if (dirty) {
                    Shape s;
                    if (!CollisionTrace::GetComponentShape(actor, cc, s)) continue;
                    rec.shape = s;
                    rec.box = ShapeAABB(s);
                    if (kc >= 0) {
                        std::memcpy(rec.key, k, sizeof(float) * (size_t)kc);
                        rec.keyCount = kc;
                    } else {
                        rec.keyCount = -1;
                    }
                }
                active.push_back(&rec);
            }
        }
    }

    if (records.size() != seen) {
        for (auto it = records.begin(); it != records.end();) {
            if (it->second.stamp != stamp) it = records.erase(it);
            else ++it;
        }
    }
}

void Scene::BuildBroad() {
    std::vector<AABB> boxes;
    boxes.reserve(active.size());
    for (Record* r : active) boxes.push_back(r->box);
    if (active.size() > 8) {
        BuildBVH(broad, boxes, broadOrder, 4);
        broadValid = true;
    } else {
        broad.nodes.clear();
        broadOrder.clear();
        broadValid = false;
    }
}

void Scene::EnsureFresh() {
    if (batchDepth > 0) {
        if (!frozen) {
            Refresh();
            BuildBroad();
            frozen = true;
        }
        return;
    }
    Refresh();
    broadValid = false;
}

void Scene::Gather(const AABB& q, const QueryParams& qp, std::vector<Record*>& out) {
    out.clear();
    if (broadValid) {
        TraverseBVHBox(broad, q, [&](uint32_t first, uint32_t count) {
            for (uint32_t i = first; i < first + count; i++) {
                Record* r = active[broadOrder[i]];
                if (Accept(*r, qp) && Overlaps(r->box, q)) out.push_back(r);
            }
            return false;
        });
    } else {
        for (Record* r : active) {
            if (Accept(*r, qp) && Overlaps(r->box, q)) out.push_back(r);
        }
    }
}

bool TestRecord(const Record& r, const Shape& s, const AABB& sBox, Contact& best) {
    if (!r.isMesh) return ShapeShape(s, r.shape, best);

    AABB local = TransformAABB(sBox, r.inv);
    bool any = false;
    Contact c;
    for (const MeshBVH& m : r.model->meshes) {
        TraverseBVHBox(m.tree, local, [&](uint32_t first, uint32_t count) {
            for (uint32_t i = first; i < first + count; i++) {
                const Tri& lt = m.tris[i];
                Tri wt{ TransformPoint(r.world, lt.a), TransformPoint(r.world, lt.b), TransformPoint(r.world, lt.c) };
                if (ShapeTriangle(s, wt, c) && (!any || c.depth > best.depth)) {
                    best = c;
                    any = true;
                }
            }
            return false;
        });
    }
    return any;
}

void FillFromContact(HitResult& out, const Record& r, const Contact& c) {
    out.actor = r.actor;
    out.component = r.comp;
    out.point = c.point;
    out.normal = c.normal;
    out.penetration = c.depth;
}

}

CollisionTrace::Shape CollisionTrace::MakeSphere(const Vector3& center, float radius) {
    Shape s;
    s.type = Shape::Sphere;
    s.center = center;
    s.a = center;
    s.b = center;
    s.radius = radius;
    return s;
}

CollisionTrace::Shape CollisionTrace::MakeCapsule(const Vector3& a, const Vector3& b, float radius) {
    Shape s;
    s.type = Shape::Capsule;
    s.a = a;
    s.b = b;
    s.center = Mul(Add(a, b), 0.5f);
    s.radius = radius;
    return s;
}

CollisionTrace::Shape CollisionTrace::MakeBox(const Vector3& center, const Vector3& rotationDegrees, const Vector3& halfExtents) {
    Matrix m = MatrixRotateXYZ({ DEG2RAD * rotationDegrees.x, DEG2RAD * rotationDegrees.y, DEG2RAD * rotationDegrees.z });
    Shape s;
    s.type = Shape::Box;
    s.center = center;
    s.a = center;
    s.b = center;
    s.axis[0] = { m.m0, m.m1, m.m2 };
    s.axis[1] = { m.m4, m.m5, m.m6 };
    s.axis[2] = { m.m8, m.m9, m.m10 };
    s.half = { fabsf(halfExtents.x), fabsf(halfExtents.y), fabsf(halfExtents.z) };
    return s;
}

bool CollisionTrace::GetComponentShape(const Actor* actor, const CollisionComponent* cc, Shape& outShape) {
    if (!actor || !cc) return false;
    Vector3 pos, rot, scl;
    actor->GetComponentWorldTransform(const_cast<CollisionComponent*>(cc), pos, rot, scl);
    float ax = fabsf(scl.x);
    float ay = fabsf(scl.y);
    float az = fabsf(scl.z);
    switch (cc->shapeType) {
        case CollisionComponent::Sphere:
            outShape = MakeSphere(pos, fabsf(cc->radius) * fmaxf(ax, fmaxf(ay, az)));
            return true;
        case CollisionComponent::Capsule: {
            Matrix m = MatrixRotateXYZ({ DEG2RAD * rot.x, DEG2RAD * rot.y, DEG2RAD * rot.z });
            Vector3 up{ m.m4, m.m5, m.m6 };
            float halfLen = fabsf(cc->height) * 0.5f * ay;
            outShape = MakeCapsule(Sub(pos, Mul(up, halfLen)), Add(pos, Mul(up, halfLen)),
                                   fabsf(cc->radius) * fmaxf(ax, az));
            return true;
        }
        default:
            outShape = MakeBox(pos, rot, { fabsf(cc->boxExtents.x) * ax * 0.5f, fabsf(cc->boxExtents.y) * ay * 0.5f,
                                           fabsf(cc->boxExtents.z) * az * 0.5f });
            return true;
    }
}

void CollisionTrace::BeginBatch() {
    Scene& sc = Scene::Get();
    if (++sc.batchDepth == 1) sc.frozen = false;
}

void CollisionTrace::EndBatch() {
    Scene& sc = Scene::Get();
    if (sc.batchDepth > 0 && --sc.batchDepth == 0) {
        sc.frozen = false;
        sc.broadValid = false;
    }
}

bool CollisionTrace::Raycast(const Vector3& start, const Vector3& end, const QueryParams& params, HitResult& outHit) {
    Vector3 diff = Sub(end, start);
    float dist = Len(diff);
    if (dist < 0.001f) return false;
    Vector3 dir = Mul(diff, 1.0f / dist);
    Vector3 inv = SafeInv3(dir);

    Scene& sc = Scene::Get();
    sc.EnsureFresh();

    float bestT = dist;
    Record* bestRec = nullptr;
    size_t bestMesh = 0;
    uint32_t bestTri = 0;
    Vector3 shapeNormal{ 0, 1, 0 };

    auto testRec = [&](Record* r) {
        if (!Accept(*r, params)) return;
        float tEnter;
        if (!RaySlab(r->box.mn, r->box.mx, start, inv, bestT, tEnter)) return;

        if (r->isMesh) {
            Vector3 lo = TransformPoint(r->inv, start);
            Vector3 raw = TransformDir(r->inv, dir);
            float L = Len(raw);
            if (L < 1e-12f) return;
            Vector3 ld = Mul(raw, 1.0f / L);
            Vector3 linv = SafeInv3(ld);
            float localMax = bestT * L;
            for (size_t mi = 0; mi < r->model->meshes.size(); mi++) {
                uint32_t tri;
                if (RayMesh(r->model->meshes[mi], lo, ld, linv, localMax, tri)) {
                    bestT = localMax / L;
                    bestRec = r;
                    bestMesh = mi;
                    bestTri = tri;
                }
            }
        } else {
            float t;
            Vector3 n;
            if (RayShape(r->shape, start, dir, bestT, t, n)) {
                bestT = t;
                bestRec = r;
                shapeNormal = n;
            }
        }
    };

    if (sc.broadValid) {
        TraverseBVHRay(sc.broad, start, inv, bestT, [&](uint32_t first, uint32_t count) {
            for (uint32_t i = first; i < first + count; i++) testRec(sc.active[sc.broadOrder[i]]);
        });
    } else {
        for (Record* r : sc.active) testRec(r);
    }

    if (!bestRec) return false;

    outHit = HitResult();
    outHit.actor = bestRec->actor;
    outHit.component = bestRec->comp;
    outHit.distance = bestT;
    outHit.time = bestT / dist;
    outHit.point = Add(start, Mul(dir, bestT));
    if (bestRec->isMesh) {
        const Tri& lt = bestRec->model->meshes[bestMesh].tris[bestTri];
        Tri wt{ TransformPoint(bestRec->world, lt.a), TransformPoint(bestRec->world, lt.b), TransformPoint(bestRec->world, lt.c) };
        Vector3 n = TriNormal(wt);
        if (Dot(n, dir) > 0.0f) n = Neg(n);
        outHit.normal = n;
    } else {
        outHit.normal = shapeNormal;
    }
    return true;
}

bool CollisionTrace::TraceAgainstMeshes(const Vector3& start, const Vector3& end, Actor* exclude,
                                         Actor*& outActor, Vector3& outLocation, Vector3& outNormal, float& outDistance) {
    QueryParams params;
    params.ignoreActor = exclude;
    params.shapes = false;
    HitResult hit;
    if (!Raycast(start, end, params, hit)) return false;
    outActor = hit.actor;
    outLocation = hit.point;
    outNormal = hit.normal;
    outDistance = hit.distance;
    return true;
}

bool CollisionTrace::OverlapShape(const Shape& shape, const QueryParams& params, HitResult& outHit) {
    Scene& sc = Scene::Get();
    sc.EnsureFresh();
    AABB sb = ShapeAABB(shape);
    sc.Gather(sb, params, sc.scratch);

    bool any = false;
    Contact best;
    const Record* bestRec = nullptr;
    for (Record* r : sc.scratch) {
        Contact c;
        if (TestRecord(*r, shape, sb, c) && (!any || c.depth > best.depth)) {
            best = c;
            bestRec = r;
            any = true;
        }
    }
    if (!any) return false;
    outHit = HitResult();
    FillFromContact(outHit, *bestRec, best);
    outHit.startPenetrating = true;
    return true;
}

int CollisionTrace::OverlapShapeAll(const Shape& shape, const QueryParams& params, std::vector<HitResult>& outHits) {
    outHits.clear();
    Scene& sc = Scene::Get();
    sc.EnsureFresh();
    AABB sb = ShapeAABB(shape);
    sc.Gather(sb, params, sc.scratch);

    for (Record* r : sc.scratch) {
        Contact c;
        if (!TestRecord(*r, shape, sb, c)) continue;
        HitResult h;
        FillFromContact(h, *r, c);
        h.startPenetrating = true;
        outHits.push_back(h);
    }
    return (int)outHits.size();
}

bool CollisionTrace::OverlapSphere(const Vector3& center, float radius, const QueryParams& params, HitResult& outHit) {
    return OverlapShape(MakeSphere(center, radius), params, outHit);
}

bool CollisionTrace::OverlapCapsule(const Vector3& a, const Vector3& b, float radius, const QueryParams& params, HitResult& outHit) {
    return OverlapShape(MakeCapsule(a, b, radius), params, outHit);
}

bool CollisionTrace::OverlapBox(const Vector3& center, const Vector3& rotationDegrees, const Vector3& halfExtents,
                                 const QueryParams& params, HitResult& outHit) {
    return OverlapShape(MakeBox(center, rotationDegrees, halfExtents), params, outHit);
}

bool CollisionTrace::SweepShape(const Shape& shape, const Vector3& delta, const QueryParams& params, HitResult& outHit) {
    Scene& sc = Scene::Get();
    sc.EnsureFresh();

    AABB swept = ShapeAABB(shape);
    Grow(swept, ShapeAABB(TranslateShape(shape, delta)));
    sc.Gather(swept, params, sc.scratch);
    const std::vector<Record*>& cand = sc.scratch;

    auto overlapAt = [&](float t, Contact& outC, const Record*& outR) -> bool {
        Shape st = TranslateShape(shape, Mul(delta, t));
        AABB sb = ShapeAABB(st);
        bool any = false;
        for (Record* r : cand) {
            Contact c;
            if (TestRecord(*r, st, sb, c) && (!any || c.depth > outC.depth)) {
                outC = c;
                outR = r;
                any = true;
            }
        }
        return any;
    };

    Contact c;
    const Record* rec = nullptr;
    float len = Len(delta);

    if (overlapAt(0.0f, c, rec)) {
        outHit = HitResult();
        FillFromContact(outHit, *rec, c);
        outHit.startPenetrating = true;
        return true;
    }
    if (len < 1e-6f) return false;

    float step = fmaxf(ShapeMinExtent(shape) * 0.5f, 1e-3f);
    int n = (int)ceilf(len / step);
    if (n < 1) n = 1;
    if (n > 128) n = 128;

    int hitIndex = -1;
    for (int i = 1; i <= n; i++) {
        if (overlapAt((float)i / (float)n, c, rec)) {
            hitIndex = i;
            break;
        }
    }
    if (hitIndex < 0) return false;

    float lo = (float)(hitIndex - 1) / (float)n;
    float hi = (float)hitIndex / (float)n;
    for (int k = 0; k < 10; k++) {
        float mid = (lo + hi) * 0.5f;
        Contact tc;
        const Record* tr = nullptr;
        if (overlapAt(mid, tc, tr)) hi = mid; else lo = mid;
    }

    Contact hc;
    const Record* hr = nullptr;
    if (!overlapAt(hi, hc, hr)) return false;

    outHit = HitResult();
    FillFromContact(outHit, *hr, hc);
    outHit.time = lo;
    outHit.distance = lo * len;
    outHit.startPenetrating = false;
    return true;
}

bool CollisionTrace::SweepSphere(const Vector3& center, float radius, const Vector3& delta, const QueryParams& params, HitResult& outHit) {
    return SweepShape(MakeSphere(center, radius), delta, params, outHit);
}

bool CollisionTrace::SweepCapsule(const Vector3& a, const Vector3& b, float radius, const Vector3& delta,
                                   const QueryParams& params, HitResult& outHit) {
    return SweepShape(MakeCapsule(a, b, radius), delta, params, outHit);
}

bool CollisionTrace::SweepBox(const Vector3& center, const Vector3& rotationDegrees, const Vector3& halfExtents, const Vector3& delta,
                               const QueryParams& params, HitResult& outHit) {
    return SweepShape(MakeBox(center, rotationDegrees, halfExtents), delta, params, outHit);
}

void CollisionTrace::DrawDebugShapes() {
    if (!World::Get().IsPlaying()) return;
    const Color color = { 90, 200, 255, 230 };
    const DebugDraw::View view = DebugDraw::CaptureView();
    const float width = DebugDraw::collisionShapeLineWidth;
    for (const auto& actorPtr : World::Get().GetActors()) {
        Actor* actor = actorPtr.get();
        if (!actor) continue;
        for (Component* comp : actor->GetComponents()) {
            CollisionComponent* cc = dynamic_cast<CollisionComponent*>(comp);
            if (!cc) continue;
            Shape s;
            if (!GetComponentShape(actor, cc, s)) continue;
            switch (s.type) {
                case Shape::Sphere:
                    DebugDraw::Sphere(view, s.center, s.radius, width, color);
                    break;
                case Shape::Capsule:
                    DebugDraw::Capsule(view, s.a, s.b, s.radius, width, color);
                    break;
                default:
                    DebugDraw::OrientedBox(view, s.center, s.axis, s.half, width, color);
                    break;
            }
        }
    }
}
