#include "AudioStreamFeed.h"
#include <algorithm>
#include <chrono>

StreamFeed::StreamFeed(const std::string& path, const WavInfo& wavInfo)
    : info(wavInfo), ring((size_t)(kCapacity * 2), 0.0f), scratch((size_t)(kChunk * 2), 0.0f) {
    file.open(path, std::ios::binary);
    opened = file.is_open();
}

uint64_t StreamFeed::Available() {
    uint64_t w = writeIdx.load(std::memory_order_acquire);
    uint64_t r = readIdx.load(std::memory_order_relaxed);
    if (w < r) {
        readIdx.store(w, std::memory_order_release);
        return 0;
    }
    return w - r;
}

void StreamFeed::Peek(uint64_t offset, float& left, float& right) const {
    uint64_t index = (readIdx.load(std::memory_order_relaxed) + offset) & kMask;
    left = ring[(size_t)(index * 2)];
    right = ring[(size_t)(index * 2 + 1)];
}

void StreamFeed::Advance(uint64_t frames) {
    readIdx.fetch_add(frames, std::memory_order_release);
}

bool StreamFeed::Exhausted() {
    return endOfFile.load(std::memory_order_acquire) && Available() == 0 && seekRequest.load(std::memory_order_acquire) < 0;
}

void StreamFeed::RequestSeek(uint64_t frame) {
    seekRequest.store((int64_t)frame, std::memory_order_release);
    endOfFile.store(false, std::memory_order_release);
}

uint64_t StreamFeed::PositionFrames() const {
    uint64_t w = writeIdx.load(std::memory_order_acquire);
    uint64_t r = readIdx.load(std::memory_order_acquire);
    uint64_t buffered = w > r ? w - r : 0;
    uint64_t pos = filePosition.load(std::memory_order_acquire);
    if (info.frameCount == 0) return 0;
    if (looping.load(std::memory_order_relaxed)) {
        uint64_t back = buffered % info.frameCount;
        return (pos + info.frameCount - back) % info.frameCount;
    }
    return pos > buffered ? pos - buffered : 0;
}

bool StreamFeed::Refill() {
    if (!opened) return false;

    int64_t seek = seekRequest.exchange(-1, std::memory_order_acq_rel);
    if (seek >= 0) {
        filePosition.store(std::min<uint64_t>((uint64_t)seek, info.frameCount), std::memory_order_release);
        writeIdx.store(readIdx.load(std::memory_order_acquire), std::memory_order_release);
        endOfFile.store(false, std::memory_order_release);
    }
    if (endOfFile.load(std::memory_order_acquire)) return false;

    uint64_t w = writeIdx.load(std::memory_order_relaxed);
    uint64_t r = readIdx.load(std::memory_order_acquire);
    uint64_t used = w > r ? w - r : 0;
    uint64_t freeFrames = kCapacity > used ? kCapacity - used : 0;
    if (freeFrames < kChunk) return false;

    uint64_t position = filePosition.load(std::memory_order_relaxed);
    uint64_t got = WavFile::ReadFrames(file, info, position, kChunk, scratch.data(), 2);
    for (uint64_t i = 0; i < got; i++) {
        uint64_t index = (w + i) & kMask;
        ring[(size_t)(index * 2)] = scratch[(size_t)(i * 2)];
        ring[(size_t)(index * 2 + 1)] = scratch[(size_t)(i * 2 + 1)];
    }
    position += got;
    filePosition.store(position, std::memory_order_release);
    writeIdx.store(w + got, std::memory_order_release);

    if (got == 0 || position >= info.frameCount) {
        if (looping.load(std::memory_order_relaxed) && info.frameCount > 0) filePosition.store(0, std::memory_order_release);
        else endOfFile.store(true, std::memory_order_release);
    }
    return got > 0;
}

StreamService& StreamService::Get() {
    static StreamService instance;
    return instance;
}

void StreamService::Add(const std::shared_ptr<StreamFeed>& feed) {
    std::lock_guard<std::mutex> guard(lock);
    feeds.push_back(feed);
    if (!worker.joinable()) {
        stopping = false;
        worker = std::thread([this]() { Run(); });
    }
    wake.notify_one();
}

void StreamService::Shutdown() {
    {
        std::lock_guard<std::mutex> guard(lock);
        stopping = true;
        wake.notify_all();
    }
    if (worker.joinable()) worker.join();
    std::lock_guard<std::mutex> guard(lock);
    feeds.clear();
}

void StreamService::Run() {
    for (;;) {
        std::vector<std::shared_ptr<StreamFeed>> active;
        {
            std::unique_lock<std::mutex> guard(lock);
            if (stopping) return;
            feeds.erase(std::remove_if(feeds.begin(), feeds.end(), [](const std::weak_ptr<StreamFeed>& w) { return w.expired(); }), feeds.end());
            for (auto& weak : feeds) {
                if (auto feed = weak.lock()) active.push_back(feed);
            }
        }
        for (auto& feed : active) {
            for (int pass = 0; pass < 4; pass++) {
                if (!feed->Refill()) break;
            }
        }
        std::unique_lock<std::mutex> guard(lock);
        if (stopping) return;
        wake.wait_for(guard, std::chrono::milliseconds(4));
    }
}
