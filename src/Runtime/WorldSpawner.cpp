#include "WorldSpawner.h"
#include "World.h"
#include "Actor.h"
#include "Components/MeshComponent.h"
#include "Components/CollisionComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Modules/Stargraph/StargraphBehavior.h"
#include "Modules/Stargraph/Stargraph.h"
#include <unordered_map>
#include <utility>
#include <vector>

Actor* WorldSpawner::SpawnActorFromInstance(const ActorInstance& inst) {
    Actor* actor = World::Get().SpawnActor(inst.className, inst.name);
    actor->position = {inst.posX, inst.posY, inst.posZ};
    actor->rotation = {inst.rotX, inst.rotY, inst.rotZ};
    actor->scale    = {inst.scaleX, inst.scaleY, inst.scaleZ};

    if (!inst.graphAssetPath.empty()) {
        Stargraph loadedGraph;
        if (loadedGraph.LoadFromFile(inst.graphAssetPath)) {
            auto* behavior = new StargraphBehavior(loadedGraph);
            actor->SetGraphBehavior(behavior);

            std::unordered_map<std::string, Component*> componentsByName;
            std::vector<std::pair<Component*, std::string>> pendingParents;

            for (const ComponentDecl& decl : behavior->GetGraph().GetComponents()) {
                Component* comp = nullptr;
                if (decl.componentType == "MeshComponent") {
                    auto* mesh = new MeshComponent();
                    mesh->SetMeshPath(decl.meshAssetPath);
                    mesh->SetMaterialPaths(decl.materialAssetPaths);
                    comp = mesh;
                } else if (decl.componentType == "CollisionComponent") {
                    auto* collision = new CollisionComponent();
                    collision->shapeType = decl.collisionShape == "Sphere" ? CollisionComponent::Sphere
                                         : decl.collisionShape == "Capsule" ? CollisionComponent::Capsule
                                         : CollisionComponent::Box;
                    collision->radius = decl.collisionRadius;
                    collision->height = decl.collisionHeight;
                    comp = collision;
                } else if (decl.componentType == "AudioComponent") {
                    auto* audio = new AudioComponent();
                    audio->soundPath = decl.soundAssetPath;
                    audio->autoPlay = decl.autoPlay;
                    audio->looping = decl.looping;
                    audio->volume = decl.volume;
                    comp = audio;
                } else if (decl.componentType == "CameraComponent") {
                    auto* camera = new CameraComponent();
                    camera->fov = decl.cameraFov;
                    camera->orthographic = decl.cameraOrthographic;
                    comp = camera;
                } else {
                    comp = new SceneComponent();
                }

                comp->position = {decl.posX, decl.posY, decl.posZ};
                comp->rotation = {decl.rotX, decl.rotY, decl.rotZ};
                comp->scale    = {decl.scaleX, decl.scaleY, decl.scaleZ};
                comp->SetName(decl.name);
                comp->savableFields = decl.savableFields;
                actor->AddComponent(comp);

                componentsByName[decl.name] = comp;
                if (!decl.parentName.empty()) pendingParents.push_back({comp, decl.parentName});
            }

            for (auto& [comp, parentName] : pendingParents) {
                auto it = componentsByName.find(parentName);
                if (it != componentsByName.end() && it->second != comp) comp->parent = it->second;
            }
        }
    }

    if (!inst.meshAssetPath.empty()) {
        auto* meshComp = new MeshComponent();
        meshComp->SetMeshPath(inst.meshAssetPath);
        meshComp->SetMaterialPaths(inst.materialAssetPaths);
        actor->AddComponent(meshComp);
    }

    return actor;
}

void WorldSpawner::SpawnMapActors(const StarMap& map) {
    for (const ActorInstance& inst : map.GetInstances()) {
        Actor* actor = SpawnActorFromInstance(inst);
        actor->SetNetId(World::Get().AllocateNetId());
    }
}