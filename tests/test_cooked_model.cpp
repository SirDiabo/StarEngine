#include "TestHelper.h"
#include "Core/CookedModel.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

using namespace CookedModel;

std::vector<float> Floats(size_t count, unsigned seed) {
    test::Rng rng(seed);
    std::vector<float> out(count);
    for (float& f : out) f = (float)(rng.Below(20000)) / 100.0f - 100.0f;
    return out;
}

Mesh MakeMesh(uint32_t vertexCount, uint32_t triangleCount, int material, bool skinned, bool indexed, unsigned seed, uint32_t skeletonBones) {
    Mesh m;
    m.materialIndex = material;
    m.vertexCount = vertexCount;
    m.triangleCount = triangleCount;
    m.vertices = Floats(vertexCount * 3ull, seed);
    m.texcoords = Floats(vertexCount * 2ull, seed + 1);
    m.normals = Floats(vertexCount * 3ull, seed + 2);
    if (seed % 2 == 0) m.tangents = Floats(vertexCount * 4ull, seed + 3);
    if (seed % 3 == 0) m.texcoords2 = Floats(vertexCount * 2ull, seed + 4);
    if (seed % 2 == 1) {
        test::Rng rng(seed + 5);
        m.colors = rng.Bytes(vertexCount * 4ull);
    }
    if (indexed) {
        for (uint32_t i = 0; i < triangleCount * 3; i++) m.indices.push_back((uint16_t)(i % vertexCount));
    }
    m.boneCount = skeletonBones;
    if (skinned) {
        for (uint32_t i = 0; i < vertexCount * 4; i++) m.boneIndices.push_back((uint8_t)(i % skeletonBones));
        m.boneWeights = Floats(vertexCount * 4ull, seed + 6);
    }
    return m;
}

Data MakeData() {
    Data d;
    d.hasModel = true;
    d.meshes.push_back(MakeMesh(8, 4, 0, true, true, 2, 3));
    d.meshes.push_back(MakeMesh(5, 1, 1, false, false, 3, 3));
    d.meshes.push_back(MakeMesh(12, 6, 1, false, true, 4, 0));
    Material a;
    a.color[0] = 10; a.color[1] = 20; a.color[2] = 30; a.color[3] = 255;
    a.texture = 0;
    Material b;
    b.texture = -1;
    d.materials = { a, b };
    d.textures = { { 0x89, 'P', 'N', 'G', 1, 2, 3 } };
    for (uint32_t i = 0; i < 3; i++) {
        Bone bone;
        std::snprintf(bone.name, sizeof(bone.name), "bone_%u", i);
        bone.parent = (int)i - 1;
        d.bones.push_back(bone);
        Transform t;
        t.translation[0] = (float)i;
        t.rotation[3] = 1.0f;
        t.scale[1] = 2.0f;
        d.bindPose.push_back(t);
    }
    Animation anim;
    std::snprintf(anim.name, sizeof(anim.name), "Walk");
    anim.boneCount = 3;
    anim.keyframeCount = 4;
    for (uint32_t i = 0; i < 12; i++) {
        Transform t;
        t.translation[2] = (float)i * 0.5f;
        anim.poses.push_back(t);
    }
    d.animations.push_back(anim);
    return d;
}

void CheckEqual(const Data& a, const Data& b) {
    CHECK(a.hasModel == b.hasModel);
    REQUIRE(a.meshes.size() == b.meshes.size());
    for (size_t i = 0; i < a.meshes.size(); i++) {
        const Mesh& x = a.meshes[i];
        const Mesh& y = b.meshes[i];
        CHECK(x.materialIndex == y.materialIndex);
        CHECK(x.vertexCount == y.vertexCount);
        CHECK(x.triangleCount == y.triangleCount);
        CHECK(x.vertices == y.vertices);
        CHECK(x.texcoords == y.texcoords);
        CHECK(x.texcoords2 == y.texcoords2);
        CHECK(x.normals == y.normals);
        CHECK(x.tangents == y.tangents);
        CHECK(x.colors == y.colors);
        CHECK(x.indices == y.indices);
        CHECK(x.boneCount == y.boneCount);
        CHECK(x.boneIndices == y.boneIndices);
        CHECK(x.boneWeights == y.boneWeights);
    }
    REQUIRE(a.materials.size() == b.materials.size());
    for (size_t i = 0; i < a.materials.size(); i++) {
        CHECK(std::memcmp(a.materials[i].color, b.materials[i].color, 4) == 0);
        CHECK(a.materials[i].texture == b.materials[i].texture);
    }
    CHECK(a.textures == b.textures);
    REQUIRE(a.bones.size() == b.bones.size());
    for (size_t i = 0; i < a.bones.size(); i++) {
        CHECK(std::strcmp(a.bones[i].name, b.bones[i].name) == 0);
        CHECK(a.bones[i].parent == b.bones[i].parent);
        CHECK(std::memcmp(&a.bindPose[i], &b.bindPose[i], sizeof(Transform)) == 0);
    }
    REQUIRE(a.animations.size() == b.animations.size());
    for (size_t i = 0; i < a.animations.size(); i++) {
        CHECK(std::strcmp(a.animations[i].name, b.animations[i].name) == 0);
        CHECK(a.animations[i].boneCount == b.animations[i].boneCount);
        CHECK(a.animations[i].keyframeCount == b.animations[i].keyframeCount);
        REQUIRE(a.animations[i].poses.size() == b.animations[i].poses.size());
        CHECK(std::memcmp(a.animations[i].poses.data(), b.animations[i].poses.data(), a.animations[i].poses.size() * sizeof(Transform)) == 0);
    }
}

void Patch32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (int i = 0; i < 4; i++) bytes[offset + (size_t)i] = (uint8_t)(value >> (8 * i));
}

constexpr size_t kHasModel = 8;
constexpr size_t kMeshCount = 12;
constexpr size_t kMaterialCount = 16;
constexpr size_t kTextureCount = 20;
constexpr size_t kBoneCount = 24;
constexpr size_t kAnimationCount = 28;
constexpr size_t kFirstMesh = 32;

}

TEST(cooked_model_round_trips_a_full_model) {
    Data original = MakeData();
    std::vector<uint8_t> bytes;
    std::string error;
    REQUIRE(Serialize(original, bytes, error));
    CHECK(LooksCooked(bytes.data(), bytes.size()));

    Data decoded;
    REQUIRE(Deserialize(bytes.data(), bytes.size(), decoded, error));
    CheckEqual(original, decoded);

    std::vector<uint8_t> again;
    REQUIRE(Serialize(decoded, again, error));
    CHECK_BYTES(again, bytes);
}

TEST(cooked_model_round_trips_animation_only_files) {
    Data d;
    Animation anim;
    std::snprintf(anim.name, sizeof(anim.name), "Idle");
    anim.boneCount = 2;
    anim.keyframeCount = 3;
    anim.poses.resize(6);
    d.animations.push_back(anim);

    std::vector<uint8_t> bytes;
    std::string error;
    REQUIRE(Serialize(d, bytes, error));
    Data decoded;
    REQUIRE(Deserialize(bytes.data(), bytes.size(), decoded, error));
    CHECK(!decoded.hasModel);
    CHECK(decoded.meshes.empty());
    CheckEqual(d, decoded);
}

TEST(cooked_model_names_are_always_terminated) {
    Data d = MakeData();
    std::memset(d.bones[0].name, 'n', sizeof(d.bones[0].name));
    std::memset(d.animations[0].name, 'a', sizeof(d.animations[0].name));
    std::vector<uint8_t> bytes;
    std::string error;
    REQUIRE(Serialize(d, bytes, error));
    Data decoded;
    REQUIRE(Deserialize(bytes.data(), bytes.size(), decoded, error));
    CHECK(decoded.bones[0].name[kBoneNameBytes - 1] == '\0');
    CHECK(decoded.animations[0].name[kBoneNameBytes - 1] == '\0');
    CHECK(std::strlen(decoded.bones[0].name) == kBoneNameBytes - 1);
}

TEST(cooked_model_rejects_every_truncation_and_trailing_data) {
    Data original = MakeData();
    std::vector<uint8_t> bytes;
    std::string error;
    REQUIRE(Serialize(original, bytes, error));

    for (size_t length = 0; length < bytes.size(); length++) {
        Data decoded;
        CHECK_MSG(!Deserialize(bytes.data(), length, decoded, error), "length " + std::to_string(length));
        CHECK(decoded.meshes.empty());
    }

    std::vector<uint8_t> extended = bytes;
    extended.push_back(0);
    Data decoded;
    CHECK(!Deserialize(extended.data(), extended.size(), decoded, error));

    std::vector<uint8_t> wrongMagic = bytes;
    wrongMagic[7] = '2';
    CHECK(!LooksCooked(wrongMagic.data(), wrongMagic.size()));
    CHECK(!Deserialize(wrongMagic.data(), wrongMagic.size(), decoded, error));
    CHECK(!LooksCooked(nullptr, 100));
    CHECK(!LooksCooked(bytes.data(), 4));
}

TEST(cooked_model_rejects_hostile_headers) {
    Data original = MakeData();
    std::vector<uint8_t> good;
    std::string error;
    REQUIRE(Serialize(original, good, error));

    struct Patch { const char* name; size_t offset; uint32_t value; };
    const Patch patches[] = {
        { "hasModel 2", kHasModel, 2 },
        { "mesh count huge", kMeshCount, 0xFFFFFFFFu },
        { "mesh count over limit", kMeshCount, kMaxMeshes + 1 },
        { "material count huge", kMaterialCount, 0xFFFFFFFFu },
        { "texture count huge", kTextureCount, 0xFFFFFFFFu },
        { "bone count over limit", kBoneCount, kMaxBones + 1 },
        { "animation count huge", kAnimationCount, 0xFFFFFFFFu },
        { "mesh count zero with model", kMeshCount, 0 },
        { "material index out of range", kFirstMesh, 9 },
        { "negative material index", kFirstMesh, 0xFFFFFFFFu },
        { "zero vertices", kFirstMesh + 4, 0 },
        { "huge vertices", kFirstMesh + 4, 0xFFFFFFFFu },
        { "huge triangles", kFirstMesh + 8, 0xFFFFFFFFu },
        { "unknown mesh flags", kFirstMesh + 12, 0x80 },
        { "mesh bone count over limit", kFirstMesh + 16, 300 },
    };
    for (const Patch& p : patches) {
        std::vector<uint8_t> bad = good;
        Patch32(bad, p.offset, p.value);
        Data decoded;
        CHECK_MSG(!Deserialize(bad.data(), bad.size(), decoded, error), p.name);
    }

    Data animationOnly;
    Animation anim;
    anim.boneCount = 1;
    anim.keyframeCount = 1;
    anim.poses.resize(1);
    animationOnly.animations.push_back(anim);
    std::vector<uint8_t> animBlob;
    REQUIRE(Serialize(animationOnly, animBlob, error));
    const size_t animBones = kFirstMesh + kBoneNameBytes;
    const size_t animFrames = animBones + 4;
    const Patch animPatches[] = {
        { "too many keyframes", animFrames, 0x7FFFFFFFu },
        { "poses over cap", animFrames, 300000 },
        { "bone count over limit", animBones, kMaxBones + 1 },
    };
    for (const Patch& p : animPatches) {
        std::vector<uint8_t> bad = animBlob;
        if (std::string(p.name) == "poses over cap") Patch32(bad, animBones, 256);
        Patch32(bad, p.offset, p.value);
        Data decoded;
        CHECK_MSG(!Deserialize(bad.data(), bad.size(), decoded, error), p.name);
    }

    std::vector<uint8_t> hugeButUnderCap = animBlob;
    Patch32(hugeButUnderCap, animBones, 256);
    Patch32(hugeButUnderCap, animFrames, 200000);
    Data decoded;
    CHECK(!Deserialize(hugeButUnderCap.data(), hugeButUnderCap.size(), decoded, error));
}

TEST(cooked_model_rejects_inconsistent_content_on_read) {
    std::string error;
    auto roundTripFails = [&](Data d, const char* name) {
        std::vector<uint8_t> bytes;
        if (!Serialize(d, bytes, error)) return;
        Data decoded;
        CHECK_MSG(!Deserialize(bytes.data(), bytes.size(), decoded, error), name);
    };

    { Data d = MakeData(); d.meshes[0].indices[0] = 8; roundTripFails(d, "index equals vertex count"); }
    { Data d = MakeData(); d.meshes[0].indices.back() = 60000; roundTripFails(d, "index far out of range"); }
    { Data d = MakeData(); d.meshes[0].boneIndices[3] = 3; roundTripFails(d, "bone index equals bone count"); }
    { Data d = MakeData(); d.meshes[0].materialIndex = 2; roundTripFails(d, "material index out of range"); }
    { Data d = MakeData(); d.materials[0].texture = 1; roundTripFails(d, "texture index out of range"); }
    { Data d = MakeData(); d.materials[0].texture = -2; roundTripFails(d, "texture index below -1"); }
    { Data d = MakeData(); d.bones[1].parent = 3; roundTripFails(d, "bone parent out of range"); }
    { Data d = MakeData(); d.bones[1].parent = -2; roundTripFails(d, "bone parent below -1"); }
    { Data d = MakeData(); d.meshes[1].boneCount = 4; roundTripFails(d, "mesh bone count above skeleton"); }
}

TEST(cooked_model_serialize_rejects_bad_data) {
    std::vector<uint8_t> bytes = { 1 };
    std::string error;

    { Data d = MakeData(); d.meshes[0].vertices.pop_back(); CHECK(!Serialize(d, bytes, error)); CHECK(bytes.empty()); }
    { Data d = MakeData(); d.meshes[0].texcoords.pop_back(); CHECK(!Serialize(d, bytes, error)); }
    { Data d = MakeData(); d.meshes[0].normals.push_back(1.0f); CHECK(!Serialize(d, bytes, error)); }
    { Data d = MakeData(); d.meshes[0].colors.assign(3, 0); CHECK(!Serialize(d, bytes, error)); }
    { Data d = MakeData(); d.meshes[0].indices.pop_back(); CHECK(!Serialize(d, bytes, error)); }
    { Data d = MakeData(); d.meshes[0].boneWeights.pop_back(); CHECK(!Serialize(d, bytes, error)); }
    { Data d = MakeData(); d.meshes[0].boneCount = 0; CHECK(!Serialize(d, bytes, error)); }
    { Data d = MakeData(); d.meshes[0].boneCount = 4; CHECK(!Serialize(d, bytes, error)); }
    { Data d = MakeData(); d.bindPose.pop_back(); CHECK(!Serialize(d, bytes, error)); }
    { Data d = MakeData(); d.meshes.clear(); CHECK(!Serialize(d, bytes, error)); }
    { Data d = MakeData(); d.animations[0].poses.pop_back(); CHECK(!Serialize(d, bytes, error)); }
    { Data d = MakeData(); d.meshes[0].vertexCount = kMaxVerticesPerMesh + 1; CHECK(!Serialize(d, bytes, error)); }
    { Data d = MakeData(); d.meshes.resize(kMaxMeshes + 1, d.meshes[0]); CHECK(!Serialize(d, bytes, error)); }
    CHECK(Serialize(MakeData(), bytes, error));
}

TEST(cooked_model_survives_random_damage) {
    Data original = MakeData();
    std::vector<uint8_t> bytes;
    std::string error;
    REQUIRE(Serialize(original, bytes, error));

    test::Rng rng(8675309);
    for (int round = 0; round < 3000; round++) {
        std::vector<uint8_t> damaged = bytes;
        const int edits = 1 + (int)rng.Below(6);
        for (int e = 0; e < edits; e++) damaged[rng.Below((uint32_t)damaged.size())] = (uint8_t)rng.Next();
        if (rng.Below(6) == 0) damaged.resize(rng.Below((uint32_t)damaged.size()));
        Data decoded;
        if (Deserialize(damaged.data(), damaged.size(), decoded, error)) {
            for (const Mesh& m : decoded.meshes) {
                CHECK(m.vertices.size() == (size_t)m.vertexCount * 3);
                for (uint16_t index : m.indices) CHECK(index < m.vertexCount);
            }
        }
    }
}

TEST_MAIN()
