#include "WorldSpawner.h"
#include "World.h"
#include "Actor.h"
#include "Components/MeshComponent.h"
#include "Components/CollisionComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Modules/Stargraph/StargraphBehavior.h"
#include "Modules/Stargraph/Stargraph.h"
#include <filesystem>

namespace fs = std::filesystem;

void WorldSpawner::SpawnActorFromInstance(const ActorInstance& inst) {
    Actor* actor = World::Get().SpawnActor(inst.className, inst.name);
    actor->position = {inst.posX, inst.posY, inst.posZ};
    actor->rotation = {inst.rotX, inst.rotY, inst.rotZ};
    actor->scale    = {inst.scaleX, inst.scaleY, inst.scaleZ};

    if (!inst.graphAssetPath.empty() && fs::exists(inst.graphAssetPath)) {
        auto* behavior = new StargraphBehavior(Stargraph{});
        behavior->GetGraph().LoadFromFile(inst.graphAssetPath);
        actor->SetGraphBehavior(behavior);

        for (const ComponentDecl& decl : behavior->GetGraph().GetComponents()) {
            Component* comp = nullptr;
            if (decl.componentType == "MeshComponent") {
                auto* mesh = new MeshComponent();
                mesh->SetMeshPath(decl.meshAssetPath);
                mesh->SetMaterialPath(decl.materialAssetPath);
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
            } else {
                comp = new SceneComponent();
            }

            comp->position = {decl.posX, decl.posY, decl.posZ};
            comp->rotation = {decl.rotX, decl.rotY, decl.rotZ};
            comp->scale    = {decl.scaleX, decl.scaleY, decl.scaleZ};
            actor->AddComponent(comp);
        }
    }

    if (!inst.meshAssetPath.empty()) {
        auto* meshComp = new MeshComponent();
        meshComp->SetMeshPath(inst.meshAssetPath);
        meshComp->SetMaterialPath(inst.materialAssetPath);
        actor->AddComponent(meshComp);
    }
}

void WorldSpawner::SpawnMapActors(const StarMap& map) {
    for (const ActorInstance& inst : map.GetInstances()) {
        SpawnActorFromInstance(inst);
    }
}
