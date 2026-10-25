#include "ReplicationManager.h"
#include "NetworkBackend.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Runtime/ActorBehavior.h"
#include <mutex>

namespace {

std::unordered_map<std::string, std::string> lastSyncedValue;
std::mutex lastSyncedValueMutex;

std::string SnapshotKey(uint32_t netId, const std::string& varName) {
    return std::to_string(netId) + ":" + varName;
}

} // namespace

void ReplicationManager::Reset() {
    std::lock_guard<std::mutex> lock(lastSyncedValueMutex);
    lastSyncedValue.clear();
}

void ReplicationManager::PreTick() {
    NetworkBackend* backend = NetworkBackendRegistry::Get().GetActive();
    if (!backend) return;

    std::vector<ReplicatedPropertyUpdate> props;
    std::vector<ReplicatedEventCall> events;
    backend->PollIncoming(props, events);

    bool isHost = backend->IsHost();

    for (const ReplicatedPropertyUpdate& update : props) {
        Actor* actor = Actor::FindByNetId(update.netId);
        if (!actor) continue;
        ActorBehavior* behavior = actor->GetBehavior();
        if (!behavior) continue;

        ReplicationMode mode = ReplicationMode::Local;
        if (!behavior->FindReplicatedVariable(update.variableName, mode)) continue;
        if (isHost && mode == ReplicationMode::ServerOnly) continue;

        behavior->SetReplicatedVariable(update.variableName, update.value);
        {
            std::lock_guard<std::mutex> lock(lastSyncedValueMutex);
            lastSyncedValue[SnapshotKey(update.netId, update.variableName)] = update.value;
        }

        if (isHost && mode == ReplicationMode::Replicated) {
            backend->SendPropertyUpdate(update, true);
        }
    }

    for (const ReplicatedEventCall& call : events) {
        Actor* actor = Actor::FindByNetId(call.netId);
        if (!actor) continue;
        ActorBehavior* behavior = actor->GetBehavior();

        if (isHost) {
            ReplicationMode eventMode = ReplicationMode::Local;
            bool known = behavior && behavior->FindEventReplication(call.eventName, eventMode);
            if (known && eventMode == ReplicationMode::ServerOnly) continue;
            if (known && eventMode == ReplicationMode::Replicated) {
                backend->SendEventCall(call, true);
            }
        }

        if (behavior) behavior->OnCustomEvent(actor, call.eventName, call.params);
    }
}

void ReplicationManager::PostTick() {
    NetworkBackend* backend = NetworkBackendRegistry::Get().GetActive();
    if (!backend) return;

    bool hasAuthority = backend->HasAuthority();

    uint32_t currentNetId = 0;
    ReplicatedVariableVisitor visitor = [&](const std::string& name, const std::string& value, ReplicationMode mode) {
        if (mode == ReplicationMode::Local) return;
        if (mode == ReplicationMode::ServerOnly && !hasAuthority) return;

        std::string key = SnapshotKey(currentNetId, name);
        {
            std::lock_guard<std::mutex> lock(lastSyncedValueMutex);
            auto it = lastSyncedValue.find(key);
            if (it != lastSyncedValue.end() && it->second == value) return;
            lastSyncedValue[key] = value;
        }

        ReplicatedPropertyUpdate update;
        update.netId = currentNetId;
        update.variableName = name;
        update.value = value;
        backend->SendPropertyUpdate(update, false);
    };

    for (const auto& actorPtr : World::Get().GetActors()) {
        Actor* actor = actorPtr.get();
        if (actor->GetNetId() == 0) continue;

        ActorBehavior* behavior = actor->GetBehavior();
        if (!behavior) continue;

        currentNetId = actor->GetNetId();
        behavior->VisitReplicatedVariables(visitor);
    }
}
