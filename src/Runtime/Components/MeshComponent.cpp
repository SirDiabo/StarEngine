#include "MeshComponent.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Core/StarAsset.h"
#include "Runtime/AssetCache.h"
#include <cmath>
#include <cstdlib>
#include "Modules/Stargraph/ObjectPinRef.h"
#include "Modules/Stargraph/ComponentRegistry.h"
#include "CollisionFilterFields.h"

void RegisterMeshComponentNodes() {
    NodeTemplate setStaticMesh;
    setStaticMesh.type = "Set_Static_Mesh";
    setStaticMesh.category = "Mesh";
    setStaticMesh.moduleName = "Stargraph";
    setStaticMesh.hideFromContextMenu = true;
    setStaticMesh.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "MeshComponent"});
        node->inputs.push_back({"Static Mesh", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setStaticMesh.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "MeshComponent", ctx);
            }
            if (p.name == "Static Mesh") path = p.defaultValue;
        }
        if (!target) return;
        if (MeshComponent* mc = dynamic_cast<MeshComponent*>(target)) {
            mc->SetMeshPath(path);
        }
    };
    NodeRegistry::Get().RegisterNode(setStaticMesh);

    NodeTemplate setMaterial;
    setMaterial.type = "Set_Material";
    setMaterial.category = "Mesh";
    setMaterial.moduleName = "Stargraph";
    setMaterial.hideFromContextMenu = true;
    setMaterial.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "MeshComponent"});
        node->inputs.push_back({"Material", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setMaterial.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "MeshComponent", ctx);
            }
            if (p.name == "Material") path = p.defaultValue;
        }
        if (!target) return;
        if (MeshComponent* mc = dynamic_cast<MeshComponent*>(target)) {
            mc->SetMaterialPath(0, path);
        }
    };
    NodeRegistry::Get().RegisterNode(setMaterial);

    NodeTemplate playAnim;
    playAnim.type = "Play_Animation";
    playAnim.category = "Mesh";
    playAnim.moduleName = "Stargraph";
    playAnim.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "MeshComponent"});
        node->inputs.push_back({"Anim", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Looping", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    playAnim.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        std::string animPath;
        bool looping = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "MeshComponent", ctx);
            }
            if (p.name == "Anim") animPath = p.defaultValue;
            if (p.name == "Looping") looping = p.defaultValue.AsBool();
        }
        if (!target) return;
        if (MeshComponent* mc = dynamic_cast<MeshComponent*>(target)) {
            mc->PlayAnimation(animPath, looping);
        }
    };
    NodeRegistry::Get().RegisterNode(playAnim);

    NodeTemplate stopAnim;
    stopAnim.type = "Stop_Animation";
    stopAnim.category = "Mesh";
    stopAnim.moduleName = "Stargraph";
    stopAnim.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "MeshComponent"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    stopAnim.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "MeshComponent", ctx);
            }
        }
        if (!target) return;
        if (MeshComponent* mc = dynamic_cast<MeshComponent*>(target)) {
            mc->StopAnimation();
        }
    };
    NodeRegistry::Get().RegisterNode(stopAnim);

    NodeTemplate setAnimRate;
    setAnimRate.type = "Set_Anim_Play_Rate";
    setAnimRate.category = "Mesh";
    setAnimRate.moduleName = "Stargraph";
    setAnimRate.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "MeshComponent"});
        node->inputs.push_back({"Rate", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setAnimRate.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        float rate = 1.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "MeshComponent", ctx);
            }
            if (p.name == "Rate") rate = p.defaultValue.AsFloat();
        }
        if (!target) return;
        if (MeshComponent* mc = dynamic_cast<MeshComponent*>(target)) {
            mc->SetAnimPlayRate(rate);
        }
    };
    NodeRegistry::Get().RegisterNode(setAnimRate);
}

void MeshComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddString("Static Mesh", &meshPath);
    outProperties.AddBool("Casts Shadow", &castsShadow);
    outProperties.AddString("Current Anim", &currentAnimPath);
    outProperties.AddBool("Anim Playing", &animPlaying, true);
    outProperties.AddBool("Anim Looping", &animLooping);
    outProperties.AddFloat("Anim Play Rate", &animPlayRate);
}

MeshComponent::~MeshComponent() {
    ReleaseAnimModelInstance();
}

void MeshComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);

    if (!animPlaying || currentAnimPath.empty()) return;

    Model* model = GetCachedModel();
    ModelAnimation* anim = GetCurrentAnimation();
    if (!model || !anim || cachedAnimFrameCount <= 0 || cachedAnimFps <= 0.0f) return;

    animTime += deltaTime * animPlayRate;

    float duration = (float)cachedAnimFrameCount / cachedAnimFps;
    if (duration <= 0.0f) return;

    if (animTime >= duration) {
        if (animLooping) animTime = fmodf(animTime, duration);
        else { animTime = duration; animPlaying = false; }
    } else if (animTime < 0.0f) {
        if (animLooping) {
            animTime = fmodf(animTime, duration);
            if (animTime < 0.0f) animTime += duration;
        } else { animTime = 0.0f; animPlaying = false; }
    }

    float frame = animTime * cachedAnimFps;
    UpdateModelAnimation(*model, *anim, frame);
}

void MeshComponent::SetMeshPath(const std::string& path) {
    if (meshPath == path) return;
    meshPath = path;
    InvalidateCachedModel();
}

void MeshComponent::SetMaterialPaths(const std::vector<std::string>& paths) {
    materialPaths = paths;
    InvalidateCachedModel();
}

void MeshComponent::SetMaterialPath(int slotIndex, const std::string& path) {
    if (slotIndex < 0) return;
    if ((size_t)slotIndex >= materialPaths.size()) materialPaths.resize(slotIndex + 1);
    materialPaths[slotIndex] = path;
    InvalidateCachedModel();
}

void MeshComponent::PlayAnimation(const std::string& animPath, bool loop) {
    currentAnimPath = animPath;
    animTime = 0.0f;
    animLooping = loop;
    animPlaying = true;
    EnsureAnimModelInstance();
}

void MeshComponent::StopAnimation() {
    animPlaying = false;
}

void MeshComponent::SetAnimPlayRate(float rate) {
    animPlayRate = rate;
}

ModelAnimation* MeshComponent::GetCurrentAnimation() {
    if (currentAnimPath != cachedAnimMetaPath) {
        cachedAnimFps = 30.0f;
        cachedAnimFrameCount = 0;
        cachedAnimMetaPath.clear();
        if (!currentAnimPath.empty()) {
            StarAsset asset;
            if (StarAsset::LoadFromFile(currentAnimPath, asset) && asset.assetType == "Animation") {
                cachedAnimFps = asset.animFramesPerSecond > 0.0f ? asset.animFramesPerSecond : 30.0f;
                cachedAnimFrameCount = asset.animFrameCount;
                cachedAnimMetaPath = currentAnimPath;
            }
        }
    }
    if (currentAnimPath.empty()) return nullptr;
    return AssetCache::Get().GetOrLoadAnimation(currentAnimPath);
}

Model* MeshComponent::GetCachedModel() {
    if (hasAnimModelInstance && meshPath != animModelInstancePath) ReleaseAnimModelInstance();
    if (!hasAnimModelInstance && (animPlaying || !currentAnimPath.empty())) EnsureAnimModelInstance();
    if (hasAnimModelInstance) return &animModelInstance;

    if (meshPath == cachedModelPath && cachedModel) return cachedModel;

    cachedModel = nullptr;
    cachedModelPath.clear();
    if (meshPath.empty()) return nullptr;

    StarAsset asset;
    if (StarAsset::LoadFromFile(meshPath, asset) && asset.assetType == "StaticMesh") {
        cachedModel = AssetCache::Get().GetOrLoadModel(asset.ResolveSourcePath());
        cachedModelPath = meshPath;
    }
    return cachedModel;
}

void MeshComponent::EnsureAnimModelInstance() {
    if (hasAnimModelInstance && animModelInstancePath == meshPath) return;
    ReleaseAnimModelInstance();
    if (meshPath.empty()) return;

    StarAsset asset;
    if (!StarAsset::LoadFromFile(meshPath, asset) || asset.assetType != "StaticMesh") return;

    Model model = AssetCache::Get().LoadPrivateModel(asset.ResolveSourcePath());
    if (model.meshCount == 0) return;

    animModelInstance = model;
    animModelInstancePath = meshPath;
    hasAnimModelInstance = true;
    AssetCache::Get().RegisterModelInstance(&animModelInstance, asset.ResolveSourcePath());
}

void MeshComponent::ReleaseAnimModelInstance() {
    if (!hasAnimModelInstance) return;
    AssetCache::Get().ForgetModelInstance(&animModelInstance);
    UnloadModel(animModelInstance);
    animModelInstance = Model{};
    animModelInstancePath.clear();
    hasAnimModelInstance = false;
}

void MeshComponent::InvalidateCachedModel() {
    cachedModel = nullptr;
    cachedModelPath.clear();
    ReleaseAnimModelInstance();
}

namespace {
ComponentRegistrar registrar_MeshComponent({
    "MeshComponent",
    []() -> Component* { return new MeshComponent(); },
    {
        ComponentField::Asset("Mesh", "Static Mesh", ".starasset",
            [](Component& c, const ComponentDecl& d) { static_cast<MeshComponent&>(c).SetMeshPath(d.Get("Mesh")); },
            [](const Component& c) { return static_cast<const MeshComponent&>(c).meshPath; }),
        ComponentField::Bln("MeshCastsShadow", "Casts Shadow", true,
            [](Component& c, const ComponentDecl& d) { static_cast<MeshComponent&>(c).castsShadow = d.Get("MeshCastsShadow", "true") == "true"; }, [](const Component& c) { return ComponentField::BoolStr(static_cast<const MeshComponent&>(c).castsShadow); }),
        ComponentField::Enm("MeshCollision", "Collision", {"Mesh", "None"}, "Mesh",
            [](Component& c, const ComponentDecl& d) {
                static_cast<MeshComponent&>(c).collisionMode = d.Get("MeshCollision", "Mesh") == "None"
                    ? MeshComponent::CollisionNone : MeshComponent::CollisionMesh;
            },
            [](const Component& c) -> std::string {
                return static_cast<const MeshComponent&>(c).collisionMode == MeshComponent::CollisionNone ? "None" : "Mesh";
            }),
        CollisionFilterFields::Layer<MeshComponent>([](const ComponentDecl& d) { return d.Get("MeshCollision", "Mesh") != "None"; }),
        CollisionFilterFields::Mask<MeshComponent>([](const ComponentDecl& d) { return d.Get("MeshCollision", "Mesh") != "None"; }),
        ComponentField::Asset("DefaultAnim", "Default Anim", ".staranim",
            [](Component& c, const ComponentDecl& d) {
                auto& mc = static_cast<MeshComponent&>(c);
                std::string animPath = d.Get("DefaultAnim");
                bool autoPlay = d.Get("DefaultAnimAutoPlay", "false") == "true";
                bool looping = d.Get("DefaultAnimLooping", "false") == "true";
                if (autoPlay && !animPath.empty() && !ComponentRegistry::IsEditorApply()) mc.PlayAnimation(animPath, looping);
                else mc.currentAnimPath = animPath;
            },
            [](const Component& c) { return static_cast<const MeshComponent&>(c).currentAnimPath; }),
        ComponentField::Bln("DefaultAnimLooping", "Default Anim Looping", false,
            [](Component& c, const ComponentDecl& d) { static_cast<MeshComponent&>(c).animLooping = d.Get("DefaultAnimLooping", "false") == "true"; }, [](const Component& c) { return ComponentField::BoolStr(static_cast<const MeshComponent&>(c).animLooping); }),
        ComponentField::Bln("DefaultAnimAutoPlay", "Default Anim Auto Play", false,
            [](Component& c, const ComponentDecl& d) { static_cast<MeshComponent&>(c).defaultAnimAutoPlay = d.Get("DefaultAnimAutoPlay", "false") == "true"; },
            [](const Component& c) { return ComponentField::BoolStr(static_cast<const MeshComponent&>(c).defaultAnimAutoPlay); }),
    },
    [](Component& c, const ComponentDecl& d) {
        auto it = d.arrayFields.find("Material");
        if (it != d.arrayFields.end()) static_cast<MeshComponent&>(c).SetMaterialPaths(it->second);
    },
    [](const Component& c, ComponentDecl& d) { d.arrayFields["Material"] = static_cast<const MeshComponent&>(c).materialPaths; },
    { "Material" }
});
}
