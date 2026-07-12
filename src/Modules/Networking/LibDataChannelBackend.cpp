#include "LibDataChannelBackend.h"
#include "NetIO.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <algorithm>

LibDataChannelBackend::LibDataChannelBackend() {
    PlatformSocket::GlobalInit();
}

LibDataChannelBackend::~LibDataChannelBackend() {
    Shutdown();
    PlatformSocket::GlobalShutdown();
}

rtc::Configuration LibDataChannelBackend::MakeIceConfig() const {
    rtc::Configuration config;
    
    config.iceServers.emplace_back("stun:stun.l.google.com:19302");
    if (!turnServerUrl.empty()) {
        config.iceServers.emplace_back(turnServerUrl);
    }
    return config;
}

namespace {
bool ConstantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}
} // namespace

// Host
std::string LibDataChannelBackend::StartHost(uint16_t port, const std::string& password,
                                              int maxPlayersIn, const std::string& turnServer) {
    if (hosting.load() || connected.load()) return "Already hosting or joined a session";

    expectedPassword = password;
    maxPlayers = maxPlayersIn > 0 ? maxPlayersIn : 8;
    turnServerUrl = turnServer;

    std::string error;
    if (!listenSocket.Listen(port, error)) {
        return error;
    }

    hosting = true;
    shuttingDown = false;
    acceptThread = std::thread([this]() { HostAcceptLoop(); });
    return "";
}

void LibDataChannelBackend::HostAcceptLoop() {
    while (!shuttingDown.load()) {
        PlatformSocket client = listenSocket.Accept();
        if (!client.IsValid()) continue;

        {
            std::lock_guard<std::mutex> lock(connectionsMutex);
            if ((int)connections.size() >= maxPlayers) {
                PlatformSocket rejectSock = std::move(client);
                std::string frame;
                NetIO::AppendLine(frame, "type", "reject");
                NetIO::AppendLine(frame, "reason", "Server is full");
                rejectSock.WriteFrame(frame);
                continue;
            }
        }

        const int kMaxInFlightHandshakes = std::max(16, maxPlayers * 4);
        if (inFlightHandshakes.load() >= kMaxInFlightHandshakes) {
            continue;
        }
        inFlightHandshakes.fetch_add(1);

        handshakeThreads.emplace_back([this, s = std::move(client)]() mutable {
            try {
                HostHandshakeClient(std::move(s));
            } catch (const std::exception& e) {
                fprintf(stderr, "[Networking] Handshake with a connecting client failed: %s\n", e.what());
            } catch (...) {
                fprintf(stderr, "[Networking] Handshake with a connecting client failed (unknown exception)\n");
            }
            inFlightHandshakes.fetch_sub(1);
        });
    }
}

void LibDataChannelBackend::HostHandshakeClient(PlatformSocket sock) {
    std::string helloFrame;
    if (!sock.ReadFrame(helloFrame, 5000)) return;

    auto hello = NetIO::ParseLines(helloFrame);
    if (hello["type"] != "hello") return;
    if (!ConstantTimeEquals(hello["password"], expectedPassword)) {
        std::string frame;
        NetIO::AppendLine(frame, "type", "reject");
        NetIO::AppendLine(frame, "reason", "Incorrect password");
        sock.WriteFrame(frame);
        return;
    }

    uint64_t connId = nextConnectionId.fetch_add(1);
    auto conn = CreateConnection(connId);
    conn->peerAddress = sock.PeerAddressString();
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        connections[connId] = conn;
    }

    conn->eventsChannel = conn->pc->createDataChannel("events", rtc::DataChannelInit{});
    rtc::DataChannelInit propsInit;
    propsInit.reliability.unordered = true;
    propsInit.reliability.maxRetransmits = 0;
    conn->propsChannel = conn->pc->createDataChannel("props", propsInit);
    WireChannel(conn, conn->eventsChannel, true);
    WireChannel(conn, conn->propsChannel, false);

    WaitForGatheringComplete(conn->pc, 10000);

    auto localDesc = conn->pc->localDescription();
    if (!localDesc) {
        RemoveConnection(connId, "Failed to generate offer");
        return;
    }

    std::string offerFrame;
    NetIO::AppendLine(offerFrame, "type", "offer");
    NetIO::AppendLine(offerFrame, "connid", std::to_string(connId));
    NetIO::AppendLine(offerFrame, "sdp", std::string(*localDesc));
    if (!sock.WriteFrame(offerFrame)) {
        RemoveConnection(connId, "Client disconnected during handshake");
        return;
    }

    std::string answerFrame;
    if (!sock.ReadFrame(answerFrame, 15000)) {
        RemoveConnection(connId, "Client did not answer in time");
        return;
    }
    auto answer = NetIO::ParseLines(answerFrame);
    if (answer["type"] != "answer") {
        RemoveConnection(connId, "Client sent an unexpected handshake message");
        return;
    }

    conn->pc->setRemoteDescription(rtc::Description(answer["sdp"], rtc::Description::Type::Answer));
    
    sock.Close();
}

// Join
std::string LibDataChannelBackend::StartJoin(const std::string& address, const std::string& password,
                                              const std::string& turnServer) {
    if (hosting.load() || connected.load()) return "Already hosting or joined a session";

    size_t colon = address.find(':');
    if (colon == std::string::npos) return "Address must be in \"ip:port\" form";
    std::string host = address.substr(0, colon);
    int portValue = 0;
    try {
        portValue = std::stoi(address.substr(colon + 1));
    } catch (...) {
        return "Invalid port in address";
    }
    if (portValue <= 0 || portValue > 65535) return "Port out of range";

    turnServerUrl = turnServer;
    shuttingDown = false;
    joinThread = std::thread([this, host, port = (uint16_t)portValue, password]() {
        try {
            ClientJoinThread(host, port, password);
        } catch (const std::exception& e) {
            PushLifecycle(LifecycleType::JoinFailed, 0, std::string("Internal error: ") + e.what());
        } catch (...) {
            PushLifecycle(LifecycleType::JoinFailed, 0, "Internal error during join");
        }
    });
    return "";
}

void LibDataChannelBackend::ClientJoinThread(std::string host, uint16_t port, std::string password) {
    PlatformSocket sock;
    std::string connectError;
    if (!sock.Connect(host, port, 8000, connectError)) {
        PushLifecycle(LifecycleType::JoinFailed, 0, connectError);
        return;
    }

    std::string helloFrame;
    NetIO::AppendLine(helloFrame, "type", "hello");
    NetIO::AppendLine(helloFrame, "password", password);
    if (!sock.WriteFrame(helloFrame)) {
        PushLifecycle(LifecycleType::JoinFailed, 0, "Lost connection while saying hello");
        return;
    }

    std::string responseFrame;
    
    if (!sock.ReadFrame(responseFrame, 15000)) {
        PushLifecycle(LifecycleType::JoinFailed, 0, "Host did not respond");
        return;
    }
    auto response = NetIO::ParseLines(responseFrame);
    if (response["type"] == "reject") {
        PushLifecycle(LifecycleType::JoinFailed, 0, response["reason"]);
        return;
    }
    if (response["type"] != "offer") {
        PushLifecycle(LifecycleType::JoinFailed, 0, "Host sent an unexpected handshake message");
        return;
    }

    uint64_t connId = 0;
    try {
        connId = std::stoull(response["connid"]);
    } catch (...) {
        connId = nextConnectionId.fetch_add(1);
    }

    auto conn = CreateConnection(connId);
    conn->peerAddress = host + ":" + std::to_string(port);
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        connections[connId] = conn;
    }

    conn->pc->onDataChannel([this, conn](std::shared_ptr<rtc::DataChannel> ch) {
        bool isEvents = ch->label() == "events";
        if (isEvents) conn->eventsChannel = ch; else conn->propsChannel = ch;
        WireChannel(conn, ch, isEvents);
    });

    conn->pc->setRemoteDescription(rtc::Description(response["sdp"], rtc::Description::Type::Offer));
    
    WaitForGatheringComplete(conn->pc, 10000);

    auto localDesc = conn->pc->localDescription();
    if (!localDesc) {
        RemoveConnection(connId, "Failed to generate answer");
        PushLifecycle(LifecycleType::JoinFailed, 0, "Failed to generate an answer");
        return;
    }

    std::string answerFrame;
    NetIO::AppendLine(answerFrame, "type", "answer");
    NetIO::AppendLine(answerFrame, "sdp", std::string(*localDesc));
    sock.WriteFrame(answerFrame);
    sock.Close();
}

// Shared connection
std::shared_ptr<LibDataChannelBackend::Connection> LibDataChannelBackend::CreateConnection(uint64_t id) {
    auto conn = std::make_shared<Connection>();
    conn->id = id;
    conn->pc = std::make_shared<rtc::PeerConnection>(MakeIceConfig());
    WireCommonCallbacks(conn);
    return conn;
}

void LibDataChannelBackend::WireCommonCallbacks(const std::shared_ptr<Connection>& conn) {
    conn->pc->onStateChange([this, conn](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Failed || state == rtc::PeerConnection::State::Closed) {
            RemoveConnection(conn->id, "Connection lost");
        }
    });
}

bool LibDataChannelBackend::WaitForGatheringComplete(const std::shared_ptr<rtc::PeerConnection>& pc, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (pc->gatheringState() != rtc::PeerConnection::GatheringState::Complete) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return true;
}

void LibDataChannelBackend::WireChannel(const std::shared_ptr<Connection>& conn,
                                         const std::shared_ptr<rtc::DataChannel>& ch, bool isEventsChannel) {
    ch->onOpen([this, conn, isEventsChannel]() { OnChannelOpen(conn, isEventsChannel); });
    ch->onMessage([this, conn, isEventsChannel](rtc::message_variant data) {
        if (!std::holds_alternative<std::string>(data)) return;
        OnChannelMessage(conn, std::get<std::string>(data), isEventsChannel);
    });
    ch->onClosed([this, conn]() { RemoveConnection(conn->id, "Peer closed the connection"); });
}

void LibDataChannelBackend::OnChannelOpen(const std::shared_ptr<Connection>& conn, bool isEventsChannel) {
    if (isEventsChannel) conn->eventsOpen = true; else conn->propsOpen = true;
    if (!conn->eventsOpen.load() || !conn->propsOpen.load()) return;
    if (conn->announcedReady.exchange(true)) return;

    if (hosting.load()) {
        PushLifecycle(LifecycleType::PlayerConnected, conn->id, conn->peerAddress);
    } else {
        connected = true;
        PushLifecycle(LifecycleType::ConnectedToHost, conn->id, conn->peerAddress);
    }
}

void LibDataChannelBackend::OnChannelMessage(const std::shared_ptr<Connection>& conn, const std::string& text,
                                              bool isEventsChannel) {
    auto fields = NetIO::ParseLines(text);
    const std::string& msgType = fields["msg"];

    if (msgType == "prop") {
        ReplicatedPropertyUpdate update;
        try { update.netId = (uint32_t)std::stoul(fields["netid"]); } catch (...) { return; }
        update.variableName = fields["var"];
        update.value = fields["value"];
        update.sourceConnectionId = conn->id;
        std::lock_guard<std::mutex> lock(inboxMutex);
        inboxProps.push_back(std::move(update));
    } else if (msgType == "event") {
        ReplicatedEventCall call;
        try { call.netId = (uint32_t)std::stoul(fields["netid"]); } catch (...) { return; }
        call.eventName = fields["name"];
        call.reliable = fields["reliable"] == "1";
        call.params = NetIO::ExtractParams(text);
        call.sourceConnectionId = conn->id;
        std::lock_guard<std::mutex> lock(inboxMutex);
        inboxEvents.push_back(std::move(call));
    } else if (msgType == "spawn") {
        
        if (hosting.load()) return;
        ReplicatedActorSpawn spawn;
        try { spawn.netId = (uint32_t)std::stoul(fields["netid"]); } catch (...) { return; }
        spawn.className = fields["class"];
        spawn.graphPath = fields["graph"];
        try {
            spawn.posX = std::stof(fields["px"]); spawn.posY = std::stof(fields["py"]); spawn.posZ = std::stof(fields["pz"]);
            spawn.rotX = std::stof(fields["rx"]); spawn.rotY = std::stof(fields["ry"]); spawn.rotZ = std::stof(fields["rz"]);
            spawn.scaleX = std::stof(fields["sx"]); spawn.scaleY = std::stof(fields["sy"]); spawn.scaleZ = std::stof(fields["sz"]);
        } catch (...) { /* leave transform fields at their defaults */ }
        std::lock_guard<std::mutex> lock(inboxMutex);
        inboxSpawns.push_back(std::move(spawn));
    }
}

void LibDataChannelBackend::RemoveConnection(uint64_t id, const std::string& reason) {
    std::shared_ptr<Connection> removed;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        auto it = connections.find(id);
        if (it == connections.end()) return;
        removed = it->second;
        connections.erase(it);
    }
    if (removed->announcedReady.load()) {
        PushLifecycle(hosting.load() ? LifecycleType::PlayerDisconnected : LifecycleType::HostStopped, id, reason);
    }
    
    if (removed->eventsChannel) removed->eventsChannel->resetCallbacks();
    if (removed->propsChannel) removed->propsChannel->resetCallbacks();
    if (removed->pc) {
        removed->pc->resetCallbacks();
        removed->pc->close();
    }
    if (!hosting.load()) connected = false;
}

void LibDataChannelBackend::PushLifecycle(LifecycleType type, uint64_t connId, const std::string& detail) {
    std::lock_guard<std::mutex> lock(lifecycleMutex);
    lifecycleQueue.push_back({type, connId, detail});
}

void LibDataChannelBackend::PumpLifecycleEvents(std::vector<LifecycleEvent>& out) {
    std::lock_guard<std::mutex> lock(lifecycleMutex);
    out.insert(out.end(), lifecycleQueue.begin(), lifecycleQueue.end());
    lifecycleQueue.clear();
}

// Shutdown
void LibDataChannelBackend::Shutdown() {
    shuttingDown = true;
    listenSocket.Close();

    if (acceptThread.joinable()) acceptThread.join();
    for (auto& t : handshakeThreads) {
        if (t.joinable()) t.join();
    }
    handshakeThreads.clear();
    if (joinThread.joinable()) joinThread.join();

    {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        for (auto& [id, conn] : connections) {
            if (conn->eventsChannel) conn->eventsChannel->resetCallbacks();
            if (conn->propsChannel) conn->propsChannel->resetCallbacks();
            if (conn->pc) {
                conn->pc->resetCallbacks();
                conn->pc->close();
            }
        }
        connections.clear();
    }

    hosting = false;
    connected = false;
}

int LibDataChannelBackend::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    return (int)connections.size();
}

// NetworkBackend interface
void LibDataChannelBackend::BroadcastToChannel(bool eventsChannel, const std::string& payload, uint64_t excludeConnectionId) {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    for (auto& [id, conn] : connections) {
        if (excludeConnectionId != 0 && id == excludeConnectionId) continue;
        auto& ch = eventsChannel ? conn->eventsChannel : conn->propsChannel;
        if (ch && ch->isOpen()) ch->send(payload);
    }
}

void LibDataChannelBackend::SendPropertyUpdate(const ReplicatedPropertyUpdate& update, bool isRelay) {
    std::string payload;
    NetIO::AppendLine(payload, "msg", "prop");
    NetIO::AppendLine(payload, "netid", std::to_string(update.netId));
    NetIO::AppendLine(payload, "var", update.variableName);
    NetIO::AppendLine(payload, "value", update.value);
    BroadcastToChannel(false, payload, isRelay ? update.sourceConnectionId : 0);
}

void LibDataChannelBackend::SendEventCall(const ReplicatedEventCall& call, bool isRelay) {
    std::string payload;
    NetIO::AppendLine(payload, "msg", "event");
    NetIO::AppendLine(payload, "netid", std::to_string(call.netId));
    NetIO::AppendLine(payload, "name", call.eventName);
    NetIO::AppendLine(payload, "reliable", call.reliable ? "1" : "0");
    NetIO::AppendParams(payload, call.params);
    
    BroadcastToChannel(true, payload, isRelay ? call.sourceConnectionId : 0);
}

void LibDataChannelBackend::PollIncoming(std::vector<ReplicatedPropertyUpdate>& outProps,
                                          std::vector<ReplicatedEventCall>& outEvents) {
    std::lock_guard<std::mutex> lock(inboxMutex);
    outProps.insert(outProps.end(), inboxProps.begin(), inboxProps.end());
    outEvents.insert(outEvents.end(), inboxEvents.begin(), inboxEvents.end());
    inboxProps.clear();
    inboxEvents.clear();
}

uint32_t LibDataChannelBackend::AllocateNetId() {
    return nextDynamicNetId.fetch_add(1);
}

void LibDataChannelBackend::SendActorSpawn(const ReplicatedActorSpawn& spawn) {
    if (!hosting.load()) return;
    std::string payload;
    NetIO::AppendLine(payload, "msg", "spawn");
    NetIO::AppendLine(payload, "netid", std::to_string(spawn.netId));
    NetIO::AppendLine(payload, "class", spawn.className);
    NetIO::AppendLine(payload, "graph", spawn.graphPath);
    NetIO::AppendLine(payload, "px", std::to_string(spawn.posX));
    NetIO::AppendLine(payload, "py", std::to_string(spawn.posY));
    NetIO::AppendLine(payload, "pz", std::to_string(spawn.posZ));
    NetIO::AppendLine(payload, "rx", std::to_string(spawn.rotX));
    NetIO::AppendLine(payload, "ry", std::to_string(spawn.rotY));
    NetIO::AppendLine(payload, "rz", std::to_string(spawn.rotZ));
    NetIO::AppendLine(payload, "sx", std::to_string(spawn.scaleX));
    NetIO::AppendLine(payload, "sy", std::to_string(spawn.scaleY));
    NetIO::AppendLine(payload, "sz", std::to_string(spawn.scaleZ));
    BroadcastToChannel(true, payload, 0);
}

void LibDataChannelBackend::PollIncomingSpawns(std::vector<ReplicatedActorSpawn>& outSpawns) {
    std::lock_guard<std::mutex> lock(inboxMutex);
    outSpawns.insert(outSpawns.end(), inboxSpawns.begin(), inboxSpawns.end());
    inboxSpawns.clear();
}
