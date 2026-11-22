#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace CookedModel {

constexpr uint32_t kMaxMeshes = 4096;
constexpr uint32_t kMaxMaterials = 4096;
constexpr uint32_t kMaxTextures = 4096;
constexpr uint32_t kMaxBones = 256;
constexpr uint32_t kMaxAnimations = 1024;
constexpr uint32_t kMaxVerticesPerMesh = 16u * 1024u * 1024u;
constexpr uint32_t kMaxTrianglesPerMesh = 16u * 1024u * 1024u;
constexpr uint64_t kMaxPoseTransforms = 64ull * 1024ull * 1024ull;
constexpr size_t kBoneNameBytes = 32;

struct Transform {
    float translation[3] = {0, 0, 0};
    float rotation[4] = {0, 0, 0, 1};
    float scale[3] = {1, 1, 1};
};

struct Bone {
    char name[kBoneNameBytes] = {};
    int32_t parent = -1;
};

struct Mesh {
    int32_t materialIndex = 0;
    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;
    std::vector<float> vertices;
    std::vector<float> texcoords;
    std::vector<float> texcoords2;
    std::vector<float> normals;
    std::vector<float> tangents;
    std::vector<uint8_t> colors;
    std::vector<uint16_t> indices;
    uint32_t boneCount = 0;
    std::vector<uint8_t> boneIndices;
    std::vector<float> boneWeights;
};

struct Material {
    uint8_t color[4] = {255, 255, 255, 255};
    int32_t texture = -1;
};

struct Animation {
    char name[kBoneNameBytes] = {};
    uint32_t boneCount = 0;
    uint32_t keyframeCount = 0;
    std::vector<Transform> poses;
};

struct Data {
    bool hasModel = false;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<std::vector<uint8_t>> textures;
    std::vector<Bone> bones;
    std::vector<Transform> bindPose;
    std::vector<Animation> animations;
};

struct SkinningBuffers {
    std::vector<float> positions;
    std::vector<float> normals;
};

SkinningBuffers MakeSkinningBuffers(const Mesh& mesh);

bool LooksCooked(const uint8_t* data, size_t size);
bool Serialize(const Data& data, std::vector<uint8_t>& out, std::string& error);
bool Deserialize(const uint8_t* data, size_t size, Data& out, std::string& error);

}
