#include "StargraphModule.h"
#include "NodeRegistry.h"
#include "ExecutionContext.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Runtime/Components/CameraComponent.h"
#include "Runtime/Components/MeshComponent.h"
#include "Runtime/Components/CollisionComponent.h"
#include "Runtime/Components/AudioComponent.h"
#include "Runtime/Components/InputComponent.h"
#include "Stargraph.h"
#include "Core/AssetTypeRegistry.h"
#include "Nodes_Events.h"
#include "Nodes_Flow.h"
#include "Nodes_Math.h"
#include "Nodes_Actor.h"
#include "Nodes_Components.h"
#include "Nodes_Variables.h"
#include "Nodes_Save.h"
#include "Nodes_Utility.h"
#include "Nodes_Audio.h"
#include "Nodes_Timers.h"
#include "Nodes_Collision.h"
#include "Core/ObjectKindRegistry.h"

#ifndef STARENGINE_GAME_BUILD
#include "Core/EditorAssetHandlerRegistry.h"
#include "Editor/StargraphEditorTab.h"
#endif

StargraphModule::StargraphModule() {
    name = "Stargraph";
}

bool StargraphModule::Initialize() {
    RegisterEventNodes();
    RegisterFlowNodes();
    RegisterMathNodes();
    RegisterActorNodes();
    RegisterComponentNodes();
    RegisterVariableNodes();
    RegisterSaveNodes();
    RegisterUtilityNodes();
    RegisterAudioNodes();
    RegisterTimerNodes();

    RegisterCameraComponentNodes();
    RegisterMeshComponentNodes();
    RegisterCollisionComponentNodes();
    RegisterCollisionNodes();
    RegisterAudioComponentNodes();
    RegisterInputComponentNodes();

    RegisterObjectKinds();
    RegisterAssetType();
    RegisterEditorTabs();
    return true;
}

void StargraphModule::Shutdown() {}
void StargraphModule::Update(float deltaTime) {}

void StargraphModule::RegisterAssetType() {
    AssetTypeInfo info;
    info.typeName = "Actor";
    info.extension = ".staractor";
    info.category = "Gameplay";
    info.moduleName = "Stargraph";
    info.createAsset = [](const std::string& fullPath, const std::string& assetName) {
        Stargraph graph;
        graph.assetName = assetName;
        graph.parentClass = "Actor";
        return graph.SaveToFile(fullPath);
    };
    info.renameAsset = [](const std::string& fullPath, const std::string& newAssetName) {
        Stargraph graph;
        if (!graph.LoadFromFile(fullPath)) return false;
        graph.assetName = newAssetName;
        return graph.SaveToFile(fullPath);
    };
    AssetTypeRegistry::Get().RegisterType(info);

    AssetTypeInfo sound;
    sound.typeName = "Sound";
    sound.extension = ".starsound";
    sound.category = "Audio";
    sound.moduleName = "Stargraph";
    AssetTypeRegistry::Get().RegisterType(sound);
}

void StargraphModule::RegisterObjectKinds() {
    ObjectKindRegistry& kinds = ObjectKindRegistry::Get();
    kinds.RegisterKind("Actor", "");
    kinds.RegisterKind("Component", "");
    kinds.RegisterKind("SceneComponent", "Component");
    kinds.RegisterKind("CameraComponent", "SceneComponent");
    kinds.RegisterKind("AudioComponent", "SceneComponent");
    kinds.RegisterKind("MeshComponent", "SceneComponent");
    kinds.RegisterKind("CollisionComponent", "SceneComponent");
    kinds.RegisterKind("InputComponent", "SceneComponent");
}

void StargraphModule::RegisterEditorTabs() {
#ifndef STARENGINE_GAME_BUILD
    EditorAssetHandlerInfo handler;
    handler.moduleName = "Stargraph";
    handler.createTab = [](const std::string& fullPath, const EditorServices& services) -> EditorTab* {
        auto* tab = new StargraphEditorTab(fullPath);
        tab->SetSharedLogPanel(services.sharedLogPanel);
        tab->SetViewportPanel(services.sharedViewportPanel);
        return tab;
    };

    handler.extension = ".staractor";
    EditorAssetHandlerRegistry::Get().RegisterHandler(handler);

    handler.extension = ".stargraph";
    EditorAssetHandlerRegistry::Get().RegisterHandler(handler);
#endif
}