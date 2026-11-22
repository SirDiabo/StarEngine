#include "CookedModelRaylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace {

CookedModel::Transform ToCooked(const Transform& t) {
    CookedModel::Transform out;
    out.translation[0] = t.translation.x;
    out.translation[1] = t.translation.y;
    out.translation[2] = t.translation.z;
    out.rotation[0] = t.rotation.x;
    out.rotation[1] = t.rotation.y;
    out.rotation[2] = t.rotation.z;
    out.rotation[3] = t.rotation.w;
    out.scale[0] = t.scale.x;
    out.scale[1] = t.scale.y;
    out.scale[2] = t.scale.z;
    return out;
}

Transform FromCooked(const CookedModel::Transform& t) {
    Transform out;
    out.translation = {t.translation[0], t.translation[1], t.translation[2]};
    out.rotation = {t.rotation[0], t.rotation[1], t.rotation[2], t.rotation[3]};
    out.scale = {t.scale[0], t.scale[1], t.scale[2]};
    return out;
}

template <typename T>
void CopyOut(const T* source, size_t count, std::vector<T>& dest) {
    if (source == nullptr || count == 0) return;
    dest.assign(source, source + count);
}

template <typename T>
T* AllocCopy(const std::vector<T>& source) {
    if (source.empty()) return nullptr;
    T* memory = static_cast<T*>(MemAlloc((unsigned int)(source.size() * sizeof(T))));
    if (memory != nullptr) std::memcpy(memory, source.data(), source.size() * sizeof(T));
    return memory;
}

bool CaptureTexture(const Texture2D& texture, std::unordered_map<unsigned int, int>& known,
                    std::vector<std::vector<uint8_t>>& textures, int& index) {
    index = -1;
    if (texture.id == 0 || texture.id == rlGetTextureIdDefault()) return true;

    auto found = known.find(texture.id);
    if (found != known.end()) {
        index = found->second;
        return true;
    }

    Image image = LoadImageFromTexture(texture);
    if (image.data == nullptr) return false;
    int fileSize = 0;
    unsigned char* encoded = ExportImageToMemory(image, ".png", &fileSize);
    UnloadImage(image);
    if (encoded == nullptr || fileSize <= 0) {
        if (encoded != nullptr) MemFree(encoded);
        return false;
    }

    textures.emplace_back(encoded, encoded + fileSize);
    MemFree(encoded);
    index = (int)textures.size() - 1;
    known.emplace(texture.id, index);
    return true;
}

}

bool CookedModelRaylib::CookSourceFile(const std::string& diskPath, std::vector<uint8_t>& out, std::string& error) {
    out.clear();
    CookedModel::Data data;

    Model model = LoadModel(diskPath.c_str());
    const bool haveModel = model.meshCount > 0 && model.meshes != nullptr;
    bool ok = true;

    if (haveModel) {
        data.hasModel = true;

        for (int i = 0; i < model.meshCount && ok; i++) {
            const Mesh& source = model.meshes[i];
            if (source.vertexCount <= 0 || source.vertices == nullptr) {
                error = "a mesh has no vertices";
                ok = false;
                break;
            }
            CookedModel::Mesh mesh;
            mesh.materialIndex = model.meshMaterial != nullptr ? model.meshMaterial[i] : 0;
            mesh.vertexCount = (uint32_t)source.vertexCount;
            mesh.triangleCount = (uint32_t)(source.triangleCount > 0 ? source.triangleCount : 0);
            const size_t vc = (size_t)source.vertexCount;
            CopyOut(source.vertices, vc * 3, mesh.vertices);
            CopyOut(source.texcoords, vc * 2, mesh.texcoords);
            CopyOut(source.texcoords2, vc * 2, mesh.texcoords2);
            CopyOut(source.normals, vc * 3, mesh.normals);
            CopyOut(source.tangents, vc * 4, mesh.tangents);
            CopyOut(source.colors, vc * 4, mesh.colors);
            if (source.indices != nullptr) {
                if (vc > 65535) {
                    error = "a mesh uses more vertices than 16-bit indices can address";
                    ok = false;
                    break;
                }
                CopyOut(source.indices, (size_t)mesh.triangleCount * 3, mesh.indices);
            }
            mesh.boneCount = (uint32_t)(source.boneCount > 0 ? source.boneCount : 0);
            if (source.boneIndices != nullptr && source.boneWeights != nullptr) {
                CopyOut(source.boneIndices, vc * 4, mesh.boneIndices);
                CopyOut(source.boneWeights, vc * 4, mesh.boneWeights);
            }
            data.meshes.push_back(std::move(mesh));
        }

        std::unordered_map<unsigned int, int> knownTextures;
        for (int i = 0; i < model.materialCount && ok; i++) {
            CookedModel::Material material;
            if (model.materials != nullptr && model.materials[i].maps != nullptr) {
                const MaterialMap& diffuse = model.materials[i].maps[MATERIAL_MAP_DIFFUSE];
                material.color[0] = diffuse.color.r;
                material.color[1] = diffuse.color.g;
                material.color[2] = diffuse.color.b;
                material.color[3] = diffuse.color.a;
                int textureIndex = -1;
                if (!CaptureTexture(diffuse.texture, knownTextures, data.textures, textureIndex)) {
                    error = "could not read back a material texture";
                    ok = false;
                    break;
                }
                material.texture = textureIndex;
            }
            data.materials.push_back(material);
        }
        for (const CookedModel::Mesh& mesh : data.meshes) {
            if (mesh.materialIndex < 0 || (size_t)mesh.materialIndex >= std::max<size_t>(data.materials.size(), 1)) {
                error = "a mesh points at a missing material";
                ok = false;
                break;
            }
        }

        for (unsigned int i = 0; i < model.skeleton.boneCount && ok; i++) {
            CookedModel::Bone bone;
            std::memcpy(bone.name, model.skeleton.bones[i].name, CookedModel::kBoneNameBytes);
            bone.name[CookedModel::kBoneNameBytes - 1] = '\0';
            bone.parent = model.skeleton.bones[i].parent;
            data.bones.push_back(bone);
            data.bindPose.push_back(model.skeleton.bindPose != nullptr ? ToCooked(model.skeleton.bindPose[i]) : CookedModel::Transform());
        }
    }
    UnloadModel(model);
    if (!ok) return false;

    int animationCount = 0;
    ModelAnimation* animations = LoadModelAnimations(diskPath.c_str(), &animationCount);
    if (animations != nullptr) {
        for (int a = 0; a < animationCount; a++) {
            CookedModel::Animation animation;
            std::memcpy(animation.name, animations[a].name, CookedModel::kBoneNameBytes);
            animation.name[CookedModel::kBoneNameBytes - 1] = '\0';
            animation.boneCount = animations[a].boneCount;
            animation.keyframeCount = animations[a].keyframeCount > 0 ? (uint32_t)animations[a].keyframeCount : 0;
            for (uint32_t f = 0; f < animation.keyframeCount; f++) {
                for (uint32_t b = 0; b < animation.boneCount; b++) {
                    animation.poses.push_back(ToCooked(animations[a].keyframePoses[f][b]));
                }
            }
            data.animations.push_back(std::move(animation));
        }
        UnloadModelAnimations(animations, animationCount);
    }

    if (!data.hasModel && data.animations.empty()) {
        error = "the file has no meshes or animations raylib can read";
        return false;
    }
    return CookedModel::Serialize(data, out, error);
}

bool CookedModelRaylib::BuildModel(const CookedModel::Data& data, Model& out, std::string& error) {
    out = Model();
    if (!data.hasModel || data.meshes.empty()) {
        error = "the file contains no model";
        return false;
    }

    Model model = {};
    model.transform = MatrixIdentity();
    model.meshCount = (int)data.meshes.size();
    model.materialCount = (int)(data.materials.empty() ? 1 : data.materials.size());
    model.meshes = static_cast<Mesh*>(MemAlloc((unsigned int)(sizeof(Mesh) * data.meshes.size())));
    model.materials = static_cast<Material*>(MemAlloc((unsigned int)(sizeof(Material) * (size_t)model.materialCount)));
    model.meshMaterial = static_cast<int*>(MemAlloc((unsigned int)(sizeof(int) * data.meshes.size())));
    if (model.meshes == nullptr || model.materials == nullptr || model.meshMaterial == nullptr) {
        error = "out of memory";
        return false;
    }

    for (size_t i = 0; i < data.meshes.size(); i++) {
        const CookedModel::Mesh& source = data.meshes[i];
        Mesh mesh = {};
        mesh.vertexCount = (int)source.vertexCount;
        mesh.triangleCount = (int)source.triangleCount;
        mesh.vertices = AllocCopy(source.vertices);
        mesh.texcoords = AllocCopy(source.texcoords);
        mesh.texcoords2 = AllocCopy(source.texcoords2);
        mesh.normals = AllocCopy(source.normals);
        mesh.tangents = AllocCopy(source.tangents);
        mesh.colors = AllocCopy(source.colors);
        mesh.indices = AllocCopy(source.indices);
        mesh.boneCount = (int)source.boneCount;
        mesh.boneIndices = AllocCopy(source.boneIndices);
        mesh.boneWeights = AllocCopy(source.boneWeights);
        const CookedModel::SkinningBuffers skinning = CookedModel::MakeSkinningBuffers(source);
        mesh.animVertices = AllocCopy(skinning.positions);
        mesh.animNormals = AllocCopy(skinning.normals);
        UploadMesh(&mesh, false);
        model.meshes[i] = mesh;
        model.meshMaterial[i] = source.materialIndex;
    }

    std::vector<Texture2D> textures;
    for (const std::vector<uint8_t>& png : data.textures) {
        Texture2D texture = {};
        Image image = LoadImageFromMemory(".png", png.data(), (int)png.size());
        if (image.data != nullptr) {
            texture = LoadTextureFromImage(image);
            UnloadImage(image);
        }
        textures.push_back(texture);
    }

    for (int i = 0; i < model.materialCount; i++) {
        model.materials[i] = LoadMaterialDefault();
        if ((size_t)i < data.materials.size()) {
            const CookedModel::Material& source = data.materials[(size_t)i];
            MaterialMap& diffuse = model.materials[i].maps[MATERIAL_MAP_DIFFUSE];
            diffuse.color = {source.color[0], source.color[1], source.color[2], source.color[3]};
            if (source.texture >= 0 && (size_t)source.texture < textures.size() && textures[(size_t)source.texture].id != 0) {
                diffuse.texture = textures[(size_t)source.texture];
            }
        }
    }

    if (!data.bones.empty()) {
        const size_t boneCount = data.bones.size();
        model.skeleton.boneCount = (unsigned int)boneCount;
        model.skeleton.bones = static_cast<BoneInfo*>(MemAlloc((unsigned int)(sizeof(BoneInfo) * boneCount)));
        model.skeleton.bindPose = static_cast<Transform*>(MemAlloc((unsigned int)(sizeof(Transform) * boneCount)));
        model.currentPose = static_cast<Transform*>(MemAlloc((unsigned int)(sizeof(Transform) * boneCount)));
        model.boneMatrices = static_cast<Matrix*>(MemAlloc((unsigned int)(sizeof(Matrix) * boneCount)));
        if (model.skeleton.bones == nullptr || model.skeleton.bindPose == nullptr || model.currentPose == nullptr || model.boneMatrices == nullptr) {
            error = "out of memory";
            return false;
        }
        for (size_t i = 0; i < boneCount; i++) {
            std::memcpy(model.skeleton.bones[i].name, data.bones[i].name, CookedModel::kBoneNameBytes);
            model.skeleton.bones[i].parent = data.bones[i].parent;
            model.skeleton.bindPose[i] = FromCooked(data.bindPose[i]);
            model.currentPose[i] = model.skeleton.bindPose[i];
            model.boneMatrices[i] = MatrixIdentity();
        }
    }

    out = model;
    return true;
}

ModelAnimation* CookedModelRaylib::BuildAnimations(const CookedModel::Data& data, int& count) {
    count = 0;
    if (data.animations.empty()) return nullptr;

    ModelAnimation* animations = static_cast<ModelAnimation*>(MemAlloc((unsigned int)(sizeof(ModelAnimation) * data.animations.size())));
    if (animations == nullptr) return nullptr;

    for (size_t a = 0; a < data.animations.size(); a++) {
        const CookedModel::Animation& source = data.animations[a];
        ModelAnimation& animation = animations[a];
        std::memset(&animation, 0, sizeof(animation));
        std::memcpy(animation.name, source.name, CookedModel::kBoneNameBytes);
        animation.boneCount = source.boneCount;
        animation.keyframeCount = (int)source.keyframeCount;
        if (source.keyframeCount == 0) continue;

        animation.keyframePoses = static_cast<ModelAnimPose*>(MemAlloc((unsigned int)(sizeof(ModelAnimPose) * source.keyframeCount)));
        for (uint32_t f = 0; f < source.keyframeCount && animation.keyframePoses != nullptr; f++) {
            animation.keyframePoses[f] = static_cast<Transform*>(MemAlloc((unsigned int)(sizeof(Transform) * source.boneCount)));
            if (animation.keyframePoses[f] == nullptr) continue;
            for (uint32_t b = 0; b < source.boneCount; b++) {
                animation.keyframePoses[f][b] = FromCooked(source.poses[(size_t)f * source.boneCount + b]);
            }
        }
    }
    count = (int)data.animations.size();
    return animations;
}
