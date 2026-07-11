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
};

struct ReplicatedEventCall {
    uint32_t netId = 0;
    std::string eventName;
    std::vector<std::pair<std::string, std::string>> params;
    bool reliable = false;
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
