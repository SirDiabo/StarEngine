#include "StargraphModule.h"
#include "NodeRegistry.h"
#include "ExecutionContext.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Runtime/Components/CameraComponent.h"
#include "Runtime/Components/MeshComponent.h"
#include "Runtime/Components/CollisionComponent.h"
#include "Runtime/Components/AudioComponent.h"
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
    RegisterAudioComponentNodes();
    
    RegisterAssetType();
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
    AssetTypeRegistry::Get().RegisterType(info);

    AssetTypeInfo sound;
    sound.typeName = "Sound";
    sound.extension = ".starsound";
    sound.category = "Audio";
    sound.moduleName = "Stargraph";
    AssetTypeRegistry::Get().RegisterType(sound);
}