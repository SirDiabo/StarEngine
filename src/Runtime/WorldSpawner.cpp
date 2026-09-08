#include "WorldSpawner.h"
#include "World.h"
#include "Actor.h"
#include "Components/SceneComponent.h"
#include "Modules/Stargraph/StargraphBehavior.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Modules/Stargraph/StargraphAssetCache.h"
#include "Modules/Stargraph/ComponentRegistry.h"
#include "Components/MeshComponent.h"
#include <unordered_map>
#include <utility>
#include <vector>

static Component* BuildComponentFromDecl(const ComponentDecl& decl) {
    const ComponentTypeInfo* typeInfo = ComponentRegistry::Get().Find(decl.componentType);
    Component* comp = typeInfo ? typeInfo->factory() : nullptr;
    if (!comp) comp = new SceneComponent();

    if (typeInfo) {
        for (const ComponentFieldDesc& f : typeInfo->fields) {
            if (f.apply) f.apply(*comp, decl);
        }
        if (typeInfo->applyArrays) typeInfo->applyArrays(*comp, decl);
    }
    return comp;
}

Actor* WorldSpawner::SpawnActorFromInstance(const ActorInstance& inst) {
    Actor* actor = World::Get().SpawnActor(inst.className, inst.name);
    actor->position = {inst.posX, inst.posY, inst.posZ};
    actor->rotation = {inst.rotX, inst.rotY, inst.rotZ};
    actor->scale    = {inst.scaleX, inst.scaleY, inst.scaleZ};

    if (!inst.graphAssetPath.empty()) {
        actor->SetActorClass(inst.graphAssetPath);
        if (const Stargraph* cachedGraph = StargraphAssetCache::Get().GetOrLoad(inst.graphAssetPath)) {
            auto* behavior = new StargraphBehavior(*cachedGraph);
            actor->SetGraphBehavior(behavior);

            std::unordered_map<std::string, Component*> componentsByName;
            std::vector<std::pair<Component*, std::string>> pendingParents;

            for (const ComponentDecl& decl : behavior->GetGraph().GetComponents()) {
                Component* comp = BuildComponentFromDecl(decl);

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
    } else if (!inst.actorKind.empty()) {
        actor->SetActorClass(inst.actorKind);
    }

    if (!inst.meshAssetPath.empty()) {
        auto* meshComp = new MeshComponent();
        meshComp->SetMeshPath(inst.meshAssetPath);
        meshComp->SetMaterialPaths(inst.materialAssetPaths);
        meshComp->castsShadow = inst.castsShadow;
        actor->AddComponent(meshComp);
    }

    if (inst.hasComponent && !inst.component.componentType.empty()) {
        Component* comp = BuildComponentFromDecl(inst.component);
        comp->SetName(inst.component.componentType);
        actor->AddComponent(comp);
    }

    return actor;
}

void WorldSpawner::SpawnMapActors(const StarMap& map) {
    for (const ActorInstance& inst : map.GetInstances()) {
        Actor* actor = SpawnActorFromInstance(inst);
        actor->SetNetId(World::Get().AllocateNetId());
    }
}