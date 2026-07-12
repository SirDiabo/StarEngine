#pragma once
#include "Runtime/Networking/NetworkBackend.h"

#include <rtc/rtc.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "PlatformSocket.h"

class LibDataChannelBackend : public NetworkBackend {
public:
    LibDataChannelBackend();
    ~LibDataChannelBackend() override;

    std::string StartHost(uint16_t port, const std::string& password, int maxPlayers,
                           const std::string& turnServerUrl);

    std::string StartJoin(const std::string& address, const std::string& password,
                           const std::string& turnServerUrl);

    void Shutdown();

    int GetConnectionCount() const;

    enum class LifecycleType { PlayerConnected, PlayerDisconnected, ConnectedToHost, JoinFailed, HostStopped };
    struct LifecycleEvent {
        LifecycleType type;
        uint64_t connectionId = 0;
        std::string detail;
    };
    
    void PumpLifecycleEvents(std::vector<LifecycleEvent>& out);

    bool IsHost() const override { return hosting; }
    void SendPropertyUpdate(const ReplicatedPropertyUpdate& update, bool isRelay) override;
    void SendEventCall(const ReplicatedEventCall& call, bool isRelay) override;
    void PollIncoming(std::vector<ReplicatedPropertyUpdate>& outProps,
                       std::vector<ReplicatedEventCall>& outEvents) override;
    uint32_t AllocateNetId() override;
    void SendActorSpawn(const ReplicatedActorSpawn& spawn) override;
    void PollIncomingSpawns(std::vector<ReplicatedActorSpawn>& outSpawns) override;

private:
    struct Connection {
        uint64_t id = 0;
        std::shared_ptr<rtc::PeerConnection> pc;
        std::shared_ptr<rtc::DataChannel> eventsChannel;
        std::shared_ptr<rtc::DataChannel> propsChannel;
        std::atomic<bool> eventsOpen{false};
        std::atomic<bool> propsOpen{false};
        std::atomic<bool> announcedReady{false};
        std::string peerAddress;
    };

    rtc::Configuration MakeIceConfig() const;
    std::shared_ptr<Connection> CreateConnection(uint64_t id);
    void WireCommonCallbacks(const std::shared_ptr<Connection>& conn);
    void WireChannel(const std::shared_ptr<Connection>& conn, const std::shared_ptr<rtc::DataChannel>& ch, bool isEventsChannel);
    void OnChannelOpen(const std::shared_ptr<Connection>& conn, bool isEventsChannel);
    void OnChannelMessage(const std::shared_ptr<Connection>& conn, const std::string& text, bool isEventsChannel);
    void RemoveConnection(uint64_t id, const std::string& reason);
    bool WaitForGatheringComplete(const std::shared_ptr<rtc::PeerConnection>& pc, int timeoutMs);

    void HostAcceptLoop();
    void HostHandshakeClient(PlatformSocket sock);
    void ClientJoinThread(std::string host, uint16_t port, std::string password);

    void BroadcastToChannel(bool eventsChannel, const std::string& payload, uint64_t excludeConnectionId);
    void PushLifecycle(LifecycleType type, uint64_t connId, const std::string& detail);

    std::string turnServerUrl;
    std::string expectedPassword;
    int maxPlayers = 8;
    std::atomic<bool> hosting{false};
    std::atomic<bool> connected{false};

    std::atomic<uint64_t> nextConnectionId{1};
    std::atomic<uint32_t> nextDynamicNetId{0x40000000u};

    mutable std::mutex connectionsMutex;
    std::unordered_map<uint64_t, std::shared_ptr<Connection>> connections;

    PlatformSocket listenSocket;
    std::atomic<bool> shuttingDown{false};
    std::atomic<int> inFlightHandshakes{0};
    std::thread acceptThread;
    std::vector<std::thread> handshakeThreads;
    std::thread joinThread;

    std::mutex inboxMutex;
    std::vector<ReplicatedPropertyUpdate> inboxProps;
    std::vector<ReplicatedEventCall> inboxEvents;
    std::vector<ReplicatedActorSpawn> inboxSpawns;

    std::mutex lifecycleMutex;
    std::vector<LifecycleEvent> lifecycleQueue;
};
