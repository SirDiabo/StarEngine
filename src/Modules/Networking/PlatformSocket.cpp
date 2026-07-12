#include "PlatformSocket.h"
#include <cstring>
#include <atomic>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SockLen = int;
    static intptr_t InvalidFd() { return (intptr_t)INVALID_SOCKET; }
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <csignal>
    using SockLen = socklen_t;
    static intptr_t InvalidFd() { return -1; }
#endif

namespace {

static std::atomic<int> g_initCount{0};

int CloseFd(intptr_t fd) {
#ifdef _WIN32
    return closesocket((SOCKET)fd);
#else
    return close((int)fd);
#endif
}

int WaitReadable(intptr_t fd, int timeoutMs) {
#ifdef _WIN32
    fd_set set;
    FD_ZERO(&set);
    FD_SET((SOCKET)fd, &set);
    TIMEVAL tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    int r = select(0, &set, nullptr, nullptr, timeoutMs >= 0 ? &tv : nullptr);
    return r;
#else
    struct pollfd pfd{};
    pfd.fd = (int)fd;
    pfd.events = POLLIN;
    int r = poll(&pfd, 1, timeoutMs);
    if (r > 0 && (pfd.revents & (POLLERR | POLLHUP)) && !(pfd.revents & POLLIN)) return -1;
    return r;
#endif
}

bool RecvExact(intptr_t fd, char* buf, size_t len, int timeoutMs) {
    size_t received = 0;
    while (received < len) {
        int waitResult = WaitReadable(fd, timeoutMs);
        if (waitResult <= 0) return false; // timeout or error
#ifdef _WIN32
        int n = recv((SOCKET)fd, buf + received, (int)(len - received), 0);
#else
        ssize_t n = recv((int)fd, buf + received, len - received, 0);
#endif
        if (n <= 0) return false;
        received += (size_t)n;
    }
    return true;
}

bool SendExact(intptr_t fd, const char* buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int n = send((SOCKET)fd, buf + sent, (int)(len - sent), 0);
#else
        ssize_t n = send((int)fd, buf + sent, len - sent, 0);
#endif
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

} // namespace

void PlatformSocket::GlobalInit() {
#ifdef _WIN32
    if (g_initCount.fetch_add(1) == 0) {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    }
#else

    if (g_initCount.fetch_add(1) == 0) {
        signal(SIGPIPE, SIG_IGN);
    }
#endif
}

void PlatformSocket::GlobalShutdown() {
#ifdef _WIN32
    if (g_initCount.fetch_sub(1) == 1) {
        WSACleanup();
    }
#else
    g_initCount.fetch_sub(1);
#endif
}

PlatformSocket::PlatformSocket(intptr_t existingFd, std::string peer) : fd(existingFd), peerAddress(std::move(peer)) {}

PlatformSocket::~PlatformSocket() {
    Close();
}

PlatformSocket::PlatformSocket(PlatformSocket&& other) noexcept {
    fd = other.fd;
    peerAddress = std::move(other.peerAddress);
    other.fd = InvalidFd();
}

PlatformSocket& PlatformSocket::operator=(PlatformSocket&& other) noexcept {
    if (this != &other) {
        Close();
        fd = other.fd;
        peerAddress = std::move(other.peerAddress);
        other.fd = InvalidFd();
    }
    return *this;
}

bool PlatformSocket::IsValid() const {
    return fd != InvalidFd();
}

void PlatformSocket::Close() {
    if (IsValid()) {
        CloseFd(fd);
        fd = InvalidFd();
    }
}

bool PlatformSocket::Listen(uint16_t port, std::string& outError) {
#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
    if ((intptr_t)s == InvalidFd()) {
        outError = "Failed to create socket";
        return false;
    }

    int yes = 1;
    setsockopt((decltype(s))s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind((decltype(s))s, (sockaddr*)&addr, sizeof(addr)) != 0) {
        outError = "Failed to bind port " + std::to_string(port) + " (already in use?)";
        CloseFd((intptr_t)s);
        return false;
    }
    if (listen((decltype(s))s, 16) != 0) {
        outError = "Failed to listen on port " + std::to_string(port);
        CloseFd((intptr_t)s);
        return false;
    }

    fd = (intptr_t)s;
    return true;
}

PlatformSocket PlatformSocket::Accept() {
    if (!IsValid()) return PlatformSocket(InvalidFd());

    int waitResult = WaitReadable(fd, 500);
    if (waitResult <= 0) return PlatformSocket(InvalidFd());

    sockaddr_in clientAddr{};
    SockLen addrLen = sizeof(clientAddr);
#ifdef _WIN32
    SOCKET client = accept((SOCKET)fd, (sockaddr*)&clientAddr, &addrLen);
#else
    int client = accept((int)fd, (sockaddr*)&clientAddr, &addrLen);
#endif
    if ((intptr_t)client == InvalidFd()) return PlatformSocket(InvalidFd());

    char ipBuf[64] = {};
#ifdef _WIN32
    InetNtopA(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));
#else
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));
#endif
    std::string peer = std::string(ipBuf) + ":" + std::to_string(ntohs(clientAddr.sin_port));
    return PlatformSocket((intptr_t)client, peer);
}

bool PlatformSocket::Connect(const std::string& host, uint16_t port, int timeoutMs, std::string& outError) {
#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
    if ((intptr_t)s == InvalidFd()) {
        outError = "Failed to create socket";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        outError = "Invalid host address: " + host;
        CloseFd((intptr_t)s);
        return false;
    }

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

    int result = connect((decltype(s))s, (sockaddr*)&addr, sizeof(addr));
    bool connected = (result == 0);
    if (!connected) {
#ifdef _WIN32
        bool inProgress = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
        bool inProgress = (errno == EINPROGRESS);
#endif
        if (!inProgress) {
            outError = "Failed to connect to " + host + ":" + std::to_string(port);
            CloseFd((intptr_t)s);
            return false;
        }

        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET((decltype(s))s, &writeSet);
#ifdef _WIN32
        TIMEVAL tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
#else
        struct timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
#endif
        int selectResult = select((int)s + 1, nullptr, &writeSet, nullptr, &tv);
        if (selectResult <= 0) {
            outError = "Timed out connecting to " + host + ":" + std::to_string(port);
            CloseFd((intptr_t)s);
            return false;
        }
        int soError = 0;
        SockLen soErrorLen = sizeof(soError);
        getsockopt((decltype(s))s, SOL_SOCKET, SO_ERROR, (char*)&soError, &soErrorLen);
        if (soError != 0) {
            outError = "Connection refused by " + host + ":" + std::to_string(port);
            CloseFd((intptr_t)s);
            return false;
        }
    }

#ifdef _WIN32
    u_long blockMode = 0;
    ioctlsocket(s, FIONBIO, &blockMode);
#else
    fcntl(s, F_SETFL, flags);
#endif

    fd = (intptr_t)s;
    peerAddress = host + ":" + std::to_string(port);
    return true;
}

bool PlatformSocket::WriteFrame(const std::string& payload) {
    if (!IsValid()) return false;
    uint32_t len = (uint32_t)payload.size();
    uint8_t header[4] = {
        (uint8_t)((len >> 24) & 0xFF), (uint8_t)((len >> 16) & 0xFF),
        (uint8_t)((len >> 8) & 0xFF),  (uint8_t)(len & 0xFF)
    };
    if (!SendExact(fd, (const char*)header, sizeof(header))) return false;
    if (len == 0) return true;
    return SendExact(fd, payload.data(), payload.size());
}

bool PlatformSocket::ReadFrame(std::string& outPayload, int timeoutMs) {
    if (!IsValid()) return false;
    uint8_t header[4];
    if (!RecvExact(fd, (char*)header, sizeof(header), timeoutMs)) return false;
    uint32_t len = (uint32_t(header[0]) << 24) | (uint32_t(header[1]) << 16) |
                   (uint32_t(header[2]) << 8) | uint32_t(header[3]);
                   
    if (len > 1024 * 1024) return false;
    outPayload.resize(len);
    if (len == 0) return true;
    
    return RecvExact(fd, outPayload.data(), len, timeoutMs < 0 ? -1 : (timeoutMs + 5000));
}
