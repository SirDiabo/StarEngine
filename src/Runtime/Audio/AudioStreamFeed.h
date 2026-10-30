#pragma once
#include "WavFile.h"
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class StreamFeed {
public:
    static constexpr uint64_t kCapacity = 1u << 16;
    static constexpr uint64_t kMask = kCapacity - 1;
    static constexpr uint64_t kChunk = 8192;

    StreamFeed(const std::string& path, const WavInfo& wavInfo);

    bool IsOpen() const { return opened; }
    const WavInfo& Info() const { return info; }

    uint64_t Available();
    void Peek(uint64_t offset, float& left, float& right) const;
    void Advance(uint64_t frames);
    bool Exhausted();
    bool AtEnd() const { return endOfFile.load(std::memory_order_acquire); }

    void RequestSeek(uint64_t frame);
    void SetLooping(bool value) { looping.store(value, std::memory_order_relaxed); }
    uint64_t PositionFrames() const;

    bool Refill();

private:
    WavInfo info;
    std::ifstream file;
    bool opened = false;

    std::vector<float> ring;
    std::vector<float> scratch;
    std::atomic<uint64_t> writeIdx{0};
    std::atomic<uint64_t> readIdx{0};
    std::atomic<int64_t> seekRequest{-1};
    std::atomic<bool> looping{false};
    std::atomic<bool> endOfFile{false};
    std::atomic<uint64_t> filePosition{0};
};

class StreamService {
public:
    static StreamService& Get();

    void Add(const std::shared_ptr<StreamFeed>& feed);
    void Shutdown();

private:
    void Run();

    std::mutex lock;
    std::condition_variable wake;
    std::vector<std::weak_ptr<StreamFeed>> feeds;
    std::thread worker;
    bool stopping = false;
};
