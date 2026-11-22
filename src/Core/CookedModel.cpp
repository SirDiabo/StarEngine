#include "CookedModel.h"
#include <algorithm>
#include <cstring>
#include <exception>
#include <limits>

namespace CookedModel {

namespace {

constexpr char kMagic[8] = {'S', 'T', 'A', 'R', 'M', 'D', 'L', '1'};

enum MeshFlags : uint32_t {
    HasTexcoords = 1u << 0,
    HasTexcoords2 = 1u << 1,
    HasNormals = 1u << 2,
    HasTangents = 1u << 3,
    HasColors = 1u << 4,
    HasIndices = 1u << 5,
    HasSkin = 1u << 6,
    KnownFlags = (1u << 7) - 1
};

class Writer {
public:
    explicit Writer(std::vector<uint8_t>& target) : out(target) {}

    void U8(uint8_t v) { out.push_back(v); }
    void U16(uint16_t v) { U8((uint8_t)v); U8((uint8_t)(v >> 8)); }
    void U32(uint32_t v) { for (int i = 0; i < 4; i++) U8((uint8_t)(v >> (8 * i))); }
    void I32(int32_t v) { U32((uint32_t)v); }
    void F32(float v) { uint32_t bits; std::memcpy(&bits, &v, 4); U32(bits); }
    void Bytes(const void* data, size_t size) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        out.insert(out.end(), p, p + size);
    }
    void Floats(const std::vector<float>& values) { for (float v : values) F32(v); }

private:
    std::vector<uint8_t>& out;
};

class Reader {
public:
    Reader(const uint8_t* bytes, size_t length) : data(bytes), size(length) {}

    size_t Remaining() const { return size - pos; }
    bool Ok() const { return !failed; }

    bool Need(uint64_t count) {
        if (failed || count > Remaining()) {
            failed = true;
            return false;
        }
        return true;
    }

    uint8_t U8() { return Need(1) ? data[pos++] : 0; }
    uint16_t U16() {
        if (!Need(2)) return 0;
        uint16_t v = (uint16_t)(data[pos] | (data[pos + 1] << 8));
        pos += 2;
        return v;
    }
    uint32_t U32() {
        if (!Need(4)) return 0;
        uint32_t v = (uint32_t)data[pos] | ((uint32_t)data[pos + 1] << 8) | ((uint32_t)data[pos + 2] << 16) | ((uint32_t)data[pos + 3] << 24);
        pos += 4;
        return v;
    }
    int32_t I32() { return (int32_t)U32(); }
    float F32() {
        uint32_t bits = U32();
        float v;
        std::memcpy(&v, &bits, 4);
        return v;
    }
    bool Bytes(void* dest, size_t count) {
        if (!Need(count)) return false;
        if (count > 0) std::memcpy(dest, data + pos, count);
        pos += count;
        return true;
    }
    bool Floats(std::vector<float>& dest, uint64_t count) {
        if (!Need(count * 4)) return false;
        dest.resize((size_t)count);
        for (size_t i = 0; i < dest.size(); i++) dest[i] = F32();
        return Ok();
    }

private:
    const uint8_t* data;
    size_t size;
    size_t pos = 0;
    bool failed = false;
};

void WriteTransform(Writer& w, const Transform& t) {
    for (float v : t.translation) w.F32(v);
    for (float v : t.rotation) w.F32(v);
    for (float v : t.scale) w.F32(v);
}

bool ReadTransform(Reader& r, Transform& t) {
    for (float& v : t.translation) v = r.F32();
    for (float& v : t.rotation) v = r.F32();
    for (float& v : t.scale) v = r.F32();
    return r.Ok();
}

bool SetError(std::string& error, const char* message) {
    error = message;
    return false;
}

bool ValidMeshArrays(const Mesh& m, std::string& error) {
    const uint64_t vc = m.vertexCount;
    if (m.vertices.size() != vc * 3) return SetError(error, "mesh vertex array has the wrong size");
    if (!m.texcoords.empty() && m.texcoords.size() != vc * 2) return SetError(error, "mesh texcoord array has the wrong size");
    if (!m.texcoords2.empty() && m.texcoords2.size() != vc * 2) return SetError(error, "mesh texcoord2 array has the wrong size");
    if (!m.normals.empty() && m.normals.size() != vc * 3) return SetError(error, "mesh normal array has the wrong size");
    if (!m.tangents.empty() && m.tangents.size() != vc * 4) return SetError(error, "mesh tangent array has the wrong size");
    if (!m.colors.empty() && m.colors.size() != vc * 4) return SetError(error, "mesh color array has the wrong size");
    if (!m.indices.empty() && m.indices.size() != (uint64_t)m.triangleCount * 3) return SetError(error, "mesh index array has the wrong size");
    const bool skin = !m.boneIndices.empty() || !m.boneWeights.empty();
    if (skin && (m.boneIndices.size() != vc * 4 || m.boneWeights.size() != vc * 4)) return SetError(error, "mesh skin arrays have the wrong size");
    return true;
}

}

SkinningBuffers MakeSkinningBuffers(const Mesh& mesh) {
    SkinningBuffers buffers;
    if (mesh.boneIndices.empty() || mesh.boneWeights.empty() || mesh.vertices.empty()) return buffers;
    buffers.positions = mesh.vertices;
    buffers.normals.assign(mesh.vertices.size(), 0.0f);
    const size_t shared = mesh.normals.size() < buffers.normals.size() ? mesh.normals.size() : buffers.normals.size();
    for (size_t i = 0; i < shared; i++) buffers.normals[i] = mesh.normals[i];
    return buffers;
}

bool LooksCooked(const uint8_t* data, size_t size) {
    return data != nullptr && size >= sizeof(kMagic) && std::memcmp(data, kMagic, sizeof(kMagic)) == 0;
}

static bool SerializeImpl(const Data& data, std::vector<uint8_t>& out, std::string& error) try {
    out.clear();
    if (data.meshes.size() > kMaxMeshes || data.materials.size() > kMaxMaterials || data.textures.size() > kMaxTextures ||
        data.bones.size() > kMaxBones || data.animations.size() > kMaxAnimations) return SetError(error, "model has too many parts");
    if (data.bindPose.size() != data.bones.size()) return SetError(error, "bind pose does not match the bone count");
    if (data.hasModel && data.meshes.empty()) return SetError(error, "model has no meshes");

    Writer w(out);
    w.Bytes(kMagic, sizeof(kMagic));
    w.U32(data.hasModel ? 1u : 0u);
    w.U32((uint32_t)data.meshes.size());
    w.U32((uint32_t)data.materials.size());
    w.U32((uint32_t)data.textures.size());
    w.U32((uint32_t)data.bones.size());
    w.U32((uint32_t)data.animations.size());

    for (const Mesh& m : data.meshes) {
        if (m.vertexCount > kMaxVerticesPerMesh || m.triangleCount > kMaxTrianglesPerMesh) return SetError(error, "mesh is too large");
        if (!ValidMeshArrays(m, error)) return false;
        if (m.boneCount > data.bones.size()) return SetError(error, "mesh bone count exceeds the skeleton");
        if (!m.boneIndices.empty() && m.boneCount == 0) return SetError(error, "skinned mesh has no bones");
        uint32_t flags = 0;
        if (!m.texcoords.empty()) flags |= HasTexcoords;
        if (!m.texcoords2.empty()) flags |= HasTexcoords2;
        if (!m.normals.empty()) flags |= HasNormals;
        if (!m.tangents.empty()) flags |= HasTangents;
        if (!m.colors.empty()) flags |= HasColors;
        if (!m.indices.empty()) flags |= HasIndices;
        if (!m.boneIndices.empty()) flags |= HasSkin;

        w.I32(m.materialIndex);
        w.U32(m.vertexCount);
        w.U32(m.triangleCount);
        w.U32(flags);
        w.U32(m.boneCount);
        w.Floats(m.vertices);
        w.Floats(m.texcoords);
        w.Floats(m.texcoords2);
        w.Floats(m.normals);
        w.Floats(m.tangents);
        if (!m.colors.empty()) w.Bytes(m.colors.data(), m.colors.size());
        for (uint16_t index : m.indices) w.U16(index);
        if (!m.boneIndices.empty()) {
            w.Bytes(m.boneIndices.data(), m.boneIndices.size());
            w.Floats(m.boneWeights);
        }
    }

    for (const Material& mat : data.materials) {
        w.Bytes(mat.color, 4);
        w.I32(mat.texture);
    }

    for (const std::vector<uint8_t>& texture : data.textures) {
        if (texture.size() > std::numeric_limits<uint32_t>::max()) return SetError(error, "texture is too large");
        w.U32((uint32_t)texture.size());
        if (!texture.empty()) w.Bytes(texture.data(), texture.size());
    }

    for (size_t i = 0; i < data.bones.size(); i++) {
        w.Bytes(data.bones[i].name, kBoneNameBytes);
        w.I32(data.bones[i].parent);
        WriteTransform(w, data.bindPose[i]);
    }

    for (const Animation& a : data.animations) {
        if ((uint64_t)a.boneCount * a.keyframeCount != a.poses.size() || a.poses.size() > kMaxPoseTransforms) return SetError(error, "animation pose data has the wrong size");
        w.Bytes(a.name, kBoneNameBytes);
        w.U32(a.boneCount);
        w.U32(a.keyframeCount);
        for (const Transform& t : a.poses) WriteTransform(w, t);
    }
    return true;
} catch (const std::exception&) {
    out.clear();
    return SetError(error, "out of memory while writing the model");
}

bool Serialize(const Data& data, std::vector<uint8_t>& out, std::string& error) {
    if (SerializeImpl(data, out, error)) return true;
    out.clear();
    return false;
}

static bool DeserializeImpl(const uint8_t* bytes, size_t size, Data& out, std::string& error) try {
    out = Data();
    if (!LooksCooked(bytes, size)) return SetError(error, "not a cooked model");

    Reader r(bytes + sizeof(kMagic), size - sizeof(kMagic));
    const uint32_t hasModel = r.U32();
    const uint32_t meshCount = r.U32();
    const uint32_t materialCount = r.U32();
    const uint32_t textureCount = r.U32();
    const uint32_t boneCount = r.U32();
    const uint32_t animationCount = r.U32();
    if (!r.Ok()) return SetError(error, "cooked model is truncated");
    if (hasModel > 1) return SetError(error, "cooked model header is invalid");
    if (meshCount > kMaxMeshes || materialCount > kMaxMaterials || textureCount > kMaxTextures ||
        boneCount > kMaxBones || animationCount > kMaxAnimations) return SetError(error, "cooked model has too many parts");
    if (hasModel == 1 && meshCount == 0) return SetError(error, "cooked model has no meshes");
    if (hasModel == 0 && (meshCount != 0 || materialCount != 0)) return SetError(error, "cooked model header is inconsistent");
    out.hasModel = hasModel == 1;

    for (uint32_t i = 0; i < meshCount; i++) {
        Mesh m;
        m.materialIndex = r.I32();
        m.vertexCount = r.U32();
        m.triangleCount = r.U32();
        const uint32_t flags = r.U32();
        m.boneCount = r.U32();
        if (!r.Ok()) return SetError(error, "cooked model is truncated");
        if (m.vertexCount == 0 || m.vertexCount > kMaxVerticesPerMesh || m.triangleCount > kMaxTrianglesPerMesh) return SetError(error, "cooked mesh size is invalid");
        if ((flags & ~(uint32_t)KnownFlags) != 0) return SetError(error, "cooked mesh uses unknown data");
        if (m.materialIndex < 0 || (uint32_t)m.materialIndex >= std::max<uint32_t>(materialCount, 1u)) return SetError(error, "cooked mesh material is out of range");
        if (m.boneCount > kMaxBones) return SetError(error, "cooked mesh bone count is invalid");

        const uint64_t vc = m.vertexCount;
        if (!r.Floats(m.vertices, vc * 3)) return SetError(error, "cooked model is truncated");
        if ((flags & HasTexcoords) && !r.Floats(m.texcoords, vc * 2)) return SetError(error, "cooked model is truncated");
        if ((flags & HasTexcoords2) && !r.Floats(m.texcoords2, vc * 2)) return SetError(error, "cooked model is truncated");
        if ((flags & HasNormals) && !r.Floats(m.normals, vc * 3)) return SetError(error, "cooked model is truncated");
        if ((flags & HasTangents) && !r.Floats(m.tangents, vc * 4)) return SetError(error, "cooked model is truncated");
        if (flags & HasColors) {
            m.colors.resize((size_t)(vc * 4));
            if (!r.Bytes(m.colors.data(), m.colors.size())) return SetError(error, "cooked model is truncated");
        }
        if (flags & HasIndices) {
            const uint64_t indexCount = (uint64_t)m.triangleCount * 3;
            if (indexCount == 0 || !r.Need(indexCount * 2)) return SetError(error, "cooked model is truncated");
            m.indices.resize((size_t)indexCount);
            for (uint16_t& index : m.indices) {
                index = r.U16();
                if (index >= m.vertexCount) return SetError(error, "cooked mesh index is out of range");
            }
        }
        if (flags & HasSkin) {
            if (m.boneCount == 0 || m.boneCount > boneCount) return SetError(error, "cooked mesh skin does not match the skeleton");
            m.boneIndices.resize((size_t)(vc * 4));
            if (!r.Bytes(m.boneIndices.data(), m.boneIndices.size())) return SetError(error, "cooked model is truncated");
            for (uint8_t index : m.boneIndices) {
                if (index >= m.boneCount) return SetError(error, "cooked mesh bone index is out of range");
            }
            if (!r.Floats(m.boneWeights, vc * 4)) return SetError(error, "cooked model is truncated");
        } else if (m.boneCount > boneCount) {
            return SetError(error, "cooked mesh skin does not match the skeleton");
        }
        out.meshes.push_back(std::move(m));
    }

    for (uint32_t i = 0; i < materialCount; i++) {
        Material mat;
        if (!r.Bytes(mat.color, 4)) return SetError(error, "cooked model is truncated");
        mat.texture = r.I32();
        if (!r.Ok()) return SetError(error, "cooked model is truncated");
        if (mat.texture < -1 || mat.texture >= (int32_t)textureCount) return SetError(error, "cooked material texture is out of range");
        out.materials.push_back(mat);
    }

    for (uint32_t i = 0; i < textureCount; i++) {
        const uint32_t length = r.U32();
        if (!r.Ok() || !r.Need(length)) return SetError(error, "cooked model is truncated");
        std::vector<uint8_t> texture(length);
        if (length > 0 && !r.Bytes(texture.data(), length)) return SetError(error, "cooked model is truncated");
        out.textures.push_back(std::move(texture));
    }

    for (uint32_t i = 0; i < boneCount; i++) {
        Bone bone;
        if (!r.Bytes(bone.name, kBoneNameBytes)) return SetError(error, "cooked model is truncated");
        bone.name[kBoneNameBytes - 1] = '\0';
        bone.parent = r.I32();
        Transform t;
        if (!ReadTransform(r, t)) return SetError(error, "cooked model is truncated");
        if (bone.parent < -1 || bone.parent >= (int32_t)boneCount) return SetError(error, "cooked bone parent is out of range");
        out.bones.push_back(bone);
        out.bindPose.push_back(t);
    }

    for (uint32_t i = 0; i < animationCount; i++) {
        Animation a;
        if (!r.Bytes(a.name, kBoneNameBytes)) return SetError(error, "cooked model is truncated");
        a.name[kBoneNameBytes - 1] = '\0';
        a.boneCount = r.U32();
        a.keyframeCount = r.U32();
        if (!r.Ok()) return SetError(error, "cooked model is truncated");
        const uint64_t poseCount = (uint64_t)a.boneCount * a.keyframeCount;
        if (a.boneCount > kMaxBones || poseCount > kMaxPoseTransforms) return SetError(error, "cooked animation is too large");
        if (!r.Need(poseCount * 40)) return SetError(error, "cooked model is truncated");
        a.poses.resize((size_t)poseCount);
        for (Transform& t : a.poses) {
            if (!ReadTransform(r, t)) return SetError(error, "cooked model is truncated");
        }
        out.animations.push_back(std::move(a));
    }

    if (r.Remaining() != 0) return SetError(error, "cooked model has trailing data");
    return true;
} catch (const std::exception&) {
    out = Data();
    return SetError(error, "out of memory while reading the model");
}

bool Deserialize(const uint8_t* bytes, size_t size, Data& out, std::string& error) {
    if (DeserializeImpl(bytes, size, out, error)) return true;
    out = Data();
    return false;
}

}
