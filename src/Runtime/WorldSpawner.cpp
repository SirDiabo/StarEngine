#include "WorldSpawner.h"
#include "World.h"
#include "Actor.h"
#include "Components/SceneComponent.h"
#include "ActorAsset.h"
#include "ActorBehavior.h"
#include "ActorClassCache.h"
#include "Runtime/ComponentRegistry.h"
#include "Components/MeshComponent.h"
#include "raylib.h"
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
    Actor* actor = World::Get().SpawnActorDeferred(inst.className, inst.name);
    actor->position = {inst.posX, inst.posY, inst.posZ};
    actor->rotation = {inst.rotX, inst.rotY, inst.rotZ};
    actor->scale    = {inst.scaleX, inst.scaleY, inst.scaleZ};

    if (!inst.classAssetPath.empty()) {
        actor->SetActorClass(inst.classAssetPath);
        if (const ActorAssetData* classData = ActorClassCache::Get().GetOrLoad(inst.classAssetPath)) {
            if (ActorBehavior* behavior = ActorAsset::CreateBehavior(*classData)) {
                actor->SetBehavior(behavior);
            } else if (!classData->programmingInterface.empty()) {
                TraceLog(LOG_WARNING, "WorldSpawner: programming interface \"%s\" is unavailable for \"%s\"; actor spawned without logic",
                         classData->programmingInterface.c_str(), inst.classAssetPath.c_str());
            }

            const std::vector<ComponentDecl>& decls = classData->components;
            std::vector<Component*> built;
            built.reserve(decls.size());
            std::unordered_map<std::string, Component*> componentsByName;

            for (const ComponentDecl& decl : decls) {
                Component* comp = BuildComponentFromDecl(decl);

                comp->position = {decl.posX, decl.posY, decl.posZ};
                comp->rotation = {decl.rotX, decl.rotY, decl.rotZ};
                comp->scale    = {decl.scaleX, decl.scaleY, decl.scaleZ};
                comp->SetName(decl.name);
                comp->savableFields = decl.savableFields;

                built.push_back(comp);
                componentsByName[decl.name] = comp;
            }

            for (size_t i = 0; i < decls.size(); i++) {
                Component* parent = nullptr;
                if (!decls[i].parentName.empty()) {
                    auto it = componentsByName.find(decls[i].parentName);
                    if (it != componentsByName.end()) parent = it->second;
                }
                actor->AddComponent(built[i], parent);
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

    World::Get().FinishSpawningActor(actor);
    return actor;
}

void WorldSpawner::SpawnMapActors(const StarMap& map) {
    World::Get().SetCurrentLevelName(map.mapName);
    for (const ActorInstance& inst : map.GetInstances()) {
        Actor* actor = SpawnActorFromInstance(inst);
        actor->SetNetId(World::Get().AllocateNetId());
    }
}
