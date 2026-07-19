#include "ReplicationManager.h"
#include "NetworkBackend.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Modules/Stargraph/StargraphBehavior.h"
#include "Modules/Stargraph/Stargraph.h"
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
        auto* sb = dynamic_cast<StargraphBehavior*>(actor->GetGraphBehavior());
        if (!sb) continue;

        for (Variable& var : sb->GetGraph().GetVariables()) {
            if (var.name != update.variableName) continue;

            if (isHost && var.replication == ReplicationMode::ServerOnly) break;

            var.value = update.value;
            {
                std::lock_guard<std::mutex> lock(lastSyncedValueMutex);
                lastSyncedValue[SnapshotKey(update.netId, update.variableName)] = update.value;
            }

            if (isHost && var.replication == ReplicationMode::Replicated) {
                backend->SendPropertyUpdate(update, true);
            }
            break;
        }
    }

    for (const ReplicatedEventCall& call : events) {
        Actor* actor = Actor::FindByNetId(call.netId);
        if (!actor) continue;

        if (isHost) {
            auto* sb = dynamic_cast<StargraphBehavior*>(actor->GetGraphBehavior());
            Node* eventNode = sb ? sb->GetGraph().FindCustomEventByName(call.eventName) : nullptr;
            if (eventNode && eventNode->eventReplication == ReplicationMode::ServerOnly) continue;
            if (eventNode && eventNode->eventReplication == ReplicationMode::Replicated) {
                backend->SendEventCall(call, true);
            }
        }

        if (GraphBehavior* behavior = actor->GetGraphBehavior()) {
            behavior->OnCustomEvent(actor, call.eventName, call.params);
        }
    }
}

void ReplicationManager::PostTick() {
    NetworkBackend* backend = NetworkBackendRegistry::Get().GetActive();
    if (!backend) return;

    bool hasAuthority = backend->HasAuthority();

    for (const auto& actorPtr : World::Get().GetActors()) {
        Actor* actor = actorPtr.get();
        if (actor->GetNetId() == 0) continue;

        auto* sb = dynamic_cast<StargraphBehavior*>(actor->GetGraphBehavior());
        if (!sb) continue;

        for (Variable& var : sb->GetGraph().GetVariables()) {
            if (var.replication == ReplicationMode::Local) continue;
            if (var.replication == ReplicationMode::ServerOnly && !hasAuthority) continue;

            std::string key = SnapshotKey(actor->GetNetId(), var.name);
            bool changed = false;
            {
                std::lock_guard<std::mutex> lock(lastSyncedValueMutex);
                auto it = lastSyncedValue.find(key);
                if (it != lastSyncedValue.end() && it->second == var.value) continue;
                lastSyncedValue[key] = var.value;
                changed = true;
            }
            if (!changed) continue;

            ReplicatedPropertyUpdate update;
            update.netId = actor->GetNetId();
            update.variableName = var.name;
            update.value = var.value;
            backend->SendPropertyUpdate(update, false);
        }
    }
}
