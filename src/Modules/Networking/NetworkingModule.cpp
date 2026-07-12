#include "NetworkingModule.h"
#include "Nodes_Networking.h"
#include "Runtime/Networking/NetworkBackend.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/GraphBehavior.h"
#include "Runtime/WorldSpawner.h"
#include "Runtime/StarMap.h"

NetworkingModule* NetworkingModule::instance = nullptr;

NetworkingModule::NetworkingModule() {
    name = "Networking";
    instance = this;
}

NetworkingModule::~NetworkingModule() {
    if (instance == this) instance = nullptr;
}

bool NetworkingModule::Initialize() {
    RegisterNetworkingNodes();
    return true;
}

void NetworkingModule::Shutdown() {
    EndSession();
}

void NetworkingModule::Update(float deltaTime) {
    ApplyLifecycleEvents();
    ApplyPendingSpawns();
}

void NetworkingModule::BeginSession() {
    if (sessionActive) return;
    NetworkBackendRegistry::Get().Register(&backend);
    sessionActive = true;
}

void NetworkingModule::EndSession() {
    if (!sessionActive) {
        backend.Shutdown();
        return;
    }
    NetworkBackendRegistry::Get().Unregister(&backend);
    backend.Shutdown();
    sessionActive = false;
}

void NetworkingModule::ApplyLifecycleEvents() {
    std::vector<LibDataChannelBackend::LifecycleEvent> events;
    backend.PumpLifecycleEvents(events);
    if (events.empty()) return;

    for (const auto& evt : events) {
        std::string eventName;
        std::vector<std::pair<std::string, std::string>> params;
        params.emplace_back("ConnectionId", std::to_string(evt.connectionId));

        switch (evt.type) {
            case LibDataChannelBackend::LifecycleType::PlayerConnected:
                eventName = "OnPlayerConnected";
                break;
            case LibDataChannelBackend::LifecycleType::PlayerDisconnected:
                eventName = "OnPlayerDisconnected";
                break;
            case LibDataChannelBackend::LifecycleType::ConnectedToHost:
                eventName = "OnConnectedToHost";
                break;
            case LibDataChannelBackend::LifecycleType::JoinFailed:
                eventName = "OnJoinFailed";
                params.emplace_back("Reason", evt.detail);
                break;
            case LibDataChannelBackend::LifecycleType::HostStopped:
                eventName = "OnDisconnectedFromHost";
                params.emplace_back("Reason", evt.detail);
                break;
        }

        for (const auto& actorPtr : World::Get().GetActors()) {
            if (GraphBehavior* gb = actorPtr->GetGraphBehavior()) {
                gb->OnCustomEvent(actorPtr.get(), eventName, params);
            }
        }
    }
}

void NetworkingModule::ApplyPendingSpawns() {
    std::vector<ReplicatedActorSpawn> spawns;
    backend.PollIncomingSpawns(spawns);
    for (const auto& spawn : spawns) {
        
        if (Actor::FindByNetId(spawn.netId)) continue;

        const std::string& path = spawn.graphPath;
        bool looksSafe = path.empty() ||
            (path.find("..") == std::string::npos &&
             !(path.size() >= 1 && (path[0] == '/' || path[0] == '\\')) &&
             !(path.size() >= 2 && path[1] == ':'));
        if (!looksSafe) continue;

        ActorInstance inst;
        inst.className = spawn.className;
        inst.graphAssetPath = spawn.graphPath;
        inst.posX = spawn.posX; inst.posY = spawn.posY; inst.posZ = spawn.posZ;
        inst.rotX = spawn.rotX; inst.rotY = spawn.rotY; inst.rotZ = spawn.rotZ;
        inst.scaleX = spawn.scaleX; inst.scaleY = spawn.scaleY; inst.scaleZ = spawn.scaleZ;

        Actor* actor = WorldSpawner::SpawnActorFromInstance(inst);
        if (actor) actor->SetNetId(spawn.netId);
    }
}
