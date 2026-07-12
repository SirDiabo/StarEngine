#pragma once
#include <string>
#include <cstdint>

class PlatformSocket {
public:
    PlatformSocket() = default;
    ~PlatformSocket();

    PlatformSocket(const PlatformSocket&) = delete;
    PlatformSocket& operator=(const PlatformSocket&) = delete;
    PlatformSocket(PlatformSocket&& other) noexcept;
    PlatformSocket& operator=(PlatformSocket&& other) noexcept;

    static void GlobalInit();
    static void GlobalShutdown();

    bool Listen(uint16_t port, std::string& outError);

    PlatformSocket Accept();

    bool Connect(const std::string& host, uint16_t port, int timeoutMs, std::string& outError);

    bool WriteFrame(const std::string& payload);
    bool ReadFrame(std::string& outPayload, int timeoutMs);

    void Close();
    bool IsValid() const;

    std::string PeerAddressString() const { return peerAddress; }

private:
    intptr_t fd = -1;
    std::string peerAddress;

    explicit PlatformSocket(intptr_t existingFd, std::string peer = {});
};
