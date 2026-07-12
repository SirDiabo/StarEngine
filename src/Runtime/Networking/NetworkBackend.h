#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// A single replicated variable changing value
struct ReplicatedPropertyUpdate {
    uint32_t netId = 0;
    std::string variableName;
    std::string value;

    uint64_t sourceConnectionId = 0;
};

struct ReplicatedEventCall {
    uint32_t netId = 0;
    std::string eventName;
    std::vector<std::pair<std::string, std::string>> params;
    bool reliable = false;

    uint64_t sourceConnectionId = 0;
};

struct ReplicatedActorSpawn {
    uint32_t netId = 0;
    std::string className;
    std::string graphPath;
    float posX = 0, posY = 0, posZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0;
    float scaleX = 1, scaleY = 1, scaleZ = 1;
};

class NetworkBackend {
public:
    virtual ~NetworkBackend() = default;

    virtual bool IsHost() const = 0;

    // if host, is authorative
    virtual bool HasAuthority() const { return IsHost(); }

    // isRelay is true when the host is forwarding something that just arrived from a client
    virtual void SendPropertyUpdate(const ReplicatedPropertyUpdate& update, bool isRelay) = 0;
    virtual void SendEventCall(const ReplicatedEventCall& call, bool isRelay) = 0;

    virtual void PollIncoming(std::vector<ReplicatedPropertyUpdate>& outProps,
                               std::vector<ReplicatedEventCall>& outEvents) = 0;

    virtual uint32_t AllocateNetId() { return 0; }
    virtual void SendActorSpawn(const ReplicatedActorSpawn& spawn) {}
    virtual void PollIncomingSpawns(std::vector<ReplicatedActorSpawn>& outSpawns) {}
};

class NetworkBackendRegistry {
public:
    static NetworkBackendRegistry& Get();

    void Register(NetworkBackend* backend);
    void Unregister(NetworkBackend* backend);
    NetworkBackend* GetActive() const { return active; }

    // No backend registered means singleplayer
    bool HasAuthority() const { return !active || active->HasAuthority(); }
    bool IsHost() const { return !active || active->IsHost(); }

private:
    NetworkBackend* active = nullptr;
};
