#pragma once
#include "Profiler.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ProfilerCapture {

constexpr uint32_t kFormatVersion = 1;
constexpr uint64_t kDefaultMaxBytes = 512ull * 1024ull * 1024ull;

struct Header {
    uint32_t version = 0;
    bool gpuTiming = false;
    uint64_t startUnixMs = 0;
    std::string appName;
    std::string renderer;
};

struct Options {
    std::string path;
    std::string appName;
    std::string renderer;
    bool gpuTiming = false;
    uint64_t maxBytes = kDefaultMaxBytes;
};

bool Start(const Options& options);
void Stop();
bool IsActive();
void Pump();
void Flush();
void Marker(const std::string& text);
const std::string& ActivePath();
uint64_t BytesWritten();
const std::string& LastError();
bool SaveHistory(const Options& options, size_t* framesWritten = nullptr);
std::string TimestampedName(const std::string& prefix);

class Reader {
public:
    bool Open(const std::string& path);
    void Close();
    size_t Poll(std::vector<Profiler::Frame>& outFrames);

    bool IsOpen() const { return open; }
    bool HasHeader() const { return headerParsed; }
    bool Ended() const { return ended; }
    bool Corrupt() const { return corrupt; }
    bool MoreAvailable() const { return moreAvailable; }
    bool WasReset() const { return wasReset; }
    uint64_t Offset() const { return offset; }
    uint64_t FramesRead() const { return framesRead; }
    const Header& GetHeader() const { return header; }
    const std::string& Path() const { return path; }
    const std::vector<std::pair<uint64_t, std::string>>& Markers() const { return markers; }

private:
    void ResetState();

    bool open = false;
    bool headerParsed = false;
    bool ended = false;
    bool corrupt = false;
    bool moreAvailable = false;
    bool wasReset = false;
    std::string path;
    uint64_t offset = 0;
    uint64_t framesRead = 0;
    Header header;
    std::vector<uint32_t> localIds;
    std::vector<std::pair<uint64_t, std::string>> markers;
};

}
