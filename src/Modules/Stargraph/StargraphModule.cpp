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
#include "Runtime/Components/PhysicsComponent.h"
#include "Stargraph.h"
#include "Core/AssetTypeRegistry.h"
#include "Nodes_Events.h"
#include "Nodes_Functions.h"
#include "Nodes_Flow.h"
#include "Nodes_Math.h"
#include "Nodes_Actor.h"
#include "Nodes_Components.h"
#include "Nodes_Variables.h"
#include "Nodes_Save.h"
#include "Nodes_Utility.h"
#include "Nodes_OS.h"
#include "Nodes_Audio.h"
#include "Nodes_Timers.h"
#include "Nodes_Collision.h"
#include "Nodes_Widgets.h"
#include "ObjectEvents.h"
#include "TimerManager.h"
#include "StargraphInterface.h"
#include "Core/ObjectKindRegistry.h"

#ifndef STARENGINE_GAME_BUILD
#endif

StargraphModule::StargraphModule() {
    name = "Stargraph";
}

bool StargraphModule::Initialize() {
    RegisterEventNodes();
    RegisterFunctionNodes();
    RegisterFlowNodes();
    RegisterMathNodes();
    RegisterArrayNodes();
    RegisterActorNodes();
    RegisterComponentNodes();
    RegisterVariableNodes();
    RegisterSaveNodes();
    RegisterUtilityNodes();
    RegisterOSNodes();
    RegisterAudioNodes();
    RegisterTimerNodes();

    RegisterCameraComponentNodes();
    RegisterMeshComponentNodes();
    RegisterCollisionComponentNodes();
    RegisterPhysicsComponentNodes();
    RegisterCollisionNodes();
    RegisterAudioComponentNodes();
    RegisterInputComponentNodes();
    RegisterWidgetNodes();
    ObjectEvents::RegisterCoreComponentEvents();

    RegisterStargraphInterface();
    World::Get().AddPreTickHook("Timers", [](float deltaTime) { TimerManager::Get().Tick(deltaTime); });
    return true;
}

void StargraphModule::Shutdown() {
    World::Get().RemovePreTickHook("Timers");
}

void StargraphModule::Update(float deltaTime) {}
