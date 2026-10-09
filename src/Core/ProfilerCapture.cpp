#include "ProfilerCapture.h"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace ProfilerCapture {

namespace {

constexpr char kMagic[8] = { 'S', 'T', 'A', 'R', 'P', 'R', 'O', 'F' };
constexpr uint8_t kRecordName = 1;
constexpr uint8_t kRecordFrame = 2;
constexpr uint8_t kRecordMarker = 3;
constexpr uint8_t kRecordEnd = 4;
constexpr uint32_t kMaxRecordBytes = 8u * 1024u * 1024u;
constexpr size_t kReadChunkBytes = 16u * 1024u * 1024u;
constexpr int kReadChunksPerPoll = 2;
constexpr uint64_t kGpuHoldFrames = 12;
constexpr double kFlushIntervalSeconds = 0.25;

void PutVarint(std::string& buffer, uint64_t value) {
    while (value >= 0x80) {
        buffer.push_back((char)((value & 0x7F) | 0x80));
        value >>= 7;
    }
    buffer.push_back((char)value);
}

void PutU32(std::string& buffer, uint32_t value) {
    for (int i = 0; i < 4; i++) buffer.push_back((char)((value >> (8 * i)) & 0xFF));
}

void PutU64(std::string& buffer, uint64_t value) {
    for (int i = 0; i < 8; i++) buffer.push_back((char)((value >> (8 * i)) & 0xFF));
}

void PutString(std::string& buffer, const std::string& text) {
    PutVarint(buffer, text.size());
    buffer.append(text);
}

struct Cursor {
    const uint8_t* p;
    const uint8_t* end;

    bool Has(size_t n) const { return (size_t)(end - p) >= n; }

    bool Byte(uint8_t& out) {
        if (!Has(1)) return false;
        out = *p++;
        return true;
    }

    bool U32(uint32_t& out) {
        if (!Has(4)) return false;
        out = 0;
        for (int i = 0; i < 4; i++) out |= (uint32_t)p[i] << (8 * i);
        p += 4;
        return true;
    }

    bool U64(uint64_t& out) {
        if (!Has(8)) return false;
        out = 0;
        for (int i = 0; i < 8; i++) out |= (uint64_t)p[i] << (8 * i);
        p += 8;
        return true;
    }

    bool Varint(uint64_t& out) {
        out = 0;
        int shift = 0;
        while (p < end) {
            uint8_t b = *p++;
            out |= (uint64_t)(b & 0x7F) << shift;
            if (!(b & 0x80)) return true;
            shift += 7;
            if (shift > 63) return false;
        }
        return false;
    }

    bool String(std::string& out) {
        uint64_t length;
        if (!Varint(length) || !Has((size_t)length) || length > kMaxRecordBytes) return false;
        out.assign((const char*)p, (size_t)length);
        p += length;
        return true;
    }
};

struct WriterState {
    std::ofstream file;
    bool active = false;
    std::string path;
    std::string error;
    uint64_t bytes = 0;
    uint64_t maxBytes = kDefaultMaxBytes;
    uint64_t lastWritten = 0;
    std::unordered_set<uint32_t> namesWritten;
    int64_t lastFlushNs = 0;
};

WriterState g_writer;

uint64_t UnixMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void WriteRaw(const std::string& data) {
    g_writer.file.write(data.data(), (std::streamsize)data.size());
    g_writer.bytes += data.size();
    if (!g_writer.file) {
        g_writer.error = "write failed (disk full or file locked)";
        g_writer.active = false;
        Profiler::SetCaptureRecording(false);
    }
}

void WriteRecord(uint8_t type, const std::string& payload) {
    std::string record;
    record.reserve(payload.size() + 5);
    record.push_back((char)type);
    PutU32(record, (uint32_t)payload.size());
    record.append(payload);
    WriteRaw(record);
}

void WriteHeader(const Options& options) {
    std::string header;
    header.append(kMagic, 8);
    PutU32(header, kFormatVersion);
    PutU32(header, options.gpuTiming ? 1u : 0u);
    PutU64(header, UnixMs());
    PutString(header, options.appName);
    PutString(header, options.renderer);
    WriteRaw(header);
}

void WriteFrame(const Profiler::Frame& frame) {
    for (const Profiler::Node& node : frame.nodes) {
        if (g_writer.namesWritten.insert(node.nameId).second) {
            std::string payload;
            PutVarint(payload, node.nameId);
            PutString(payload, Profiler::NameOf(node.nameId));
            WriteRecord(kRecordName, payload);
        }
    }

    std::string payload;
    payload.reserve(frame.nodes.size() * 12 + 48);
    PutVarint(payload, frame.index);
    PutVarint(payload, (uint64_t)frame.wallNs);
    PutVarint(payload, (uint64_t)frame.workNs);
    PutVarint(payload, (uint64_t)frame.waitNs);
    PutVarint(payload, (uint64_t)frame.gpuSpanNs);
    payload.push_back(frame.gpuResolved ? 1 : 0);
    PutVarint(payload, frame.nodes.size());
    for (const Profiler::Node& node : frame.nodes) {
        PutVarint(payload, (uint64_t)(node.parent + 1));
        PutVarint(payload, node.nameId);
        uint8_t laneFlags = (uint8_t)((node.lane == Profiler::Lane::GPU ? 1 : 0) | ((node.flags & Profiler::FlagWait) ? 2 : 0));
        payload.push_back((char)laneFlags);
        PutVarint(payload, node.calls);
        PutVarint(payload, (uint64_t)node.totalNs);
        PutVarint(payload, (uint64_t)node.selfNs);
    }
    WriteRecord(kRecordFrame, payload);
}

void CloseFile() {
    if (g_writer.file.is_open()) {
        g_writer.file.flush();
        g_writer.file.close();
    }
    g_writer.active = false;
    Profiler::SetCaptureRecording(false);
}

bool OpenFile(const Options& options) {
    std::error_code ec;
    std::filesystem::path parent = std::filesystem::path(options.path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);

    g_writer.file.open(options.path, std::ios::binary | std::ios::trunc);
    if (!g_writer.file) {
        g_writer.error = "cannot open '" + options.path + "' for writing";
        return false;
    }
    g_writer.error.clear();
    g_writer.path = options.path;
    g_writer.bytes = 0;
    g_writer.maxBytes = options.maxBytes ? options.maxBytes : kDefaultMaxBytes;
    g_writer.namesWritten.clear();
    g_writer.lastFlushNs = Profiler::NowNs();
    g_writer.active = true;
    WriteHeader(options);
    return g_writer.active;
}

}

std::string TimestampedName(const std::string& prefix) {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &local);
    return prefix + "-" + buf + ".starprof";
}

bool Start(const Options& options) {
    if (g_writer.active) Stop();
    if (!OpenFile(options)) return false;
    g_writer.lastWritten = Profiler::CurrentFrameIndex();
    Profiler::SetCaptureRecording(true);
    return true;
}

void Stop() {
    if (!g_writer.file.is_open()) return;
    if (g_writer.active) WriteRecord(kRecordEnd, std::string());
    CloseFile();
}

void Flush() {
    if (g_writer.file.is_open()) g_writer.file.flush();
}

bool IsActive() { return g_writer.active; }
const std::string& ActivePath() { return g_writer.path; }
uint64_t BytesWritten() { return g_writer.bytes; }
const std::string& LastError() { return g_writer.error; }

void Marker(const std::string& text) {
    if (!g_writer.active) return;
    std::string payload;
    PutVarint(payload, Profiler::CurrentFrameIndex());
    PutString(payload, text);
    WriteRecord(kRecordMarker, payload);
}

void Pump() {
    if (!g_writer.active) return;

    const size_t count = Profiler::FrameCount();
    if (count > 0) {
        const uint64_t newest = Profiler::FrameAt(0)->index;
        std::vector<const Profiler::Frame*> pending;
        for (size_t age = 0; age < count; age++) {
            const Profiler::Frame* frame = Profiler::FrameAt(age);
            if (frame->index <= g_writer.lastWritten) break;
            pending.push_back(frame);
        }
        for (auto it = pending.rbegin(); it != pending.rend(); ++it) {
            const Profiler::Frame& frame = **it;
            const bool ready = !Profiler::IsGpuAvailable() || frame.gpuResolved || newest - frame.index > kGpuHoldFrames;
            if (!ready) break;
            WriteFrame(frame);
            g_writer.lastWritten = frame.index;
            if (!g_writer.active) return;
        }
    }

    if (g_writer.bytes >= g_writer.maxBytes) {
        g_writer.error = "size limit reached, capture stopped";
        Stop();
        return;
    }

    const int64_t now = Profiler::NowNs();
    if ((double)(now - g_writer.lastFlushNs) / 1e9 >= kFlushIntervalSeconds) {
        g_writer.file.flush();
        g_writer.lastFlushNs = now;
    }
}

bool SaveHistory(const Options& options, size_t* framesWritten) {
    const bool wasActive = g_writer.active;
    if (wasActive) return false;
    if (!OpenFile(options)) return false;
    size_t written = 0;
    const size_t count = Profiler::FrameCount();
    for (size_t age = count; age-- > 0;) {
        WriteFrame(*Profiler::FrameAt(age));
        written++;
        if (!g_writer.active) break;
    }
    const bool ok = g_writer.active;
    if (ok) WriteRecord(kRecordEnd, std::string());
    CloseFile();
    if (framesWritten) *framesWritten = written;
    return ok;
}

void Reader::ResetState() {
    wasReset = false;
    headerParsed = false;
    ended = false;
    corrupt = false;
    moreAvailable = false;
    offset = 0;
    framesRead = 0;
    header = Header();
    localIds.clear();
    markers.clear();
}

bool Reader::Open(const std::string& newPath) {
    Close();
    std::error_code ec;
    if (!std::filesystem::exists(newPath, ec)) return false;
    path = newPath;
    open = true;
    ResetState();
    return true;
}

void Reader::Close() {
    open = false;
    path.clear();
    ResetState();
}

size_t Reader::Poll(std::vector<Profiler::Frame>& outFrames) {
    if (!open || corrupt) return 0;
    moreAvailable = false;
    wasReset = false;

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return 0;
    const uint64_t size = (uint64_t)in.tellg();
    if (size < offset) {
        ResetState();
        wasReset = true;
    }

    size_t produced = 0;
    for (int chunk = 0; chunk < kReadChunksPerPoll; chunk++) {
        if (size <= offset) break;
        const size_t want = (size_t)std::min<uint64_t>(size - offset, kReadChunkBytes);
        std::vector<uint8_t> buffer(want);
        in.seekg((std::streamoff)offset);
        in.read((char*)buffer.data(), (std::streamsize)want);
        const size_t got = (size_t)in.gcount();
        if (got == 0) break;
        buffer.resize(got);

        Cursor cursor{ buffer.data(), buffer.data() + buffer.size() };
        const uint8_t* consumed = cursor.p;

        if (!headerParsed) {
            Cursor probe = cursor;
            if (!probe.Has(8)) break;
            if (std::string((const char*)probe.p, 8) != std::string(kMagic, 8)) { corrupt = true; break; }
            probe.p += 8;
            uint32_t flags = 0;
            Header parsed;
            if (!probe.U32(parsed.version) || !probe.U32(flags) || !probe.U64(parsed.startUnixMs) ||
                !probe.String(parsed.appName) || !probe.String(parsed.renderer)) {
                break;
            }
            if (parsed.version == 0 || parsed.version > kFormatVersion) { corrupt = true; break; }
            parsed.gpuTiming = (flags & 1u) != 0;
            header = parsed;
            headerParsed = true;
            cursor = probe;
            consumed = cursor.p;
        }

        while (true) {
            Cursor record = cursor;
            uint8_t type;
            uint32_t length;
            if (!record.Byte(type) || !record.U32(length)) break;
            if (length > kMaxRecordBytes) { corrupt = true; break; }
            if (!record.Has(length)) break;
            Cursor payload{ record.p, record.p + length };
            bool valid = true;

            if (type == kRecordName) {
                uint64_t id;
                std::string name;
                valid = payload.Varint(id) && payload.String(name) && id < (1u << 28);
                if (valid) {
                    if (localIds.size() <= id) localIds.resize((size_t)id + 1, 0);
                    localIds[(size_t)id] = Profiler::Intern(name);
                }
            } else if (type == kRecordFrame) {
                Profiler::Frame frame;
                uint64_t index, wall, work, wait, gpuSpan, nodeCount;
                uint8_t gpuResolved;
                valid = payload.Varint(index) && payload.Varint(wall) && payload.Varint(work) && payload.Varint(wait) &&
                        payload.Varint(gpuSpan) && payload.Byte(gpuResolved) && payload.Varint(nodeCount) &&
                        nodeCount <= length;
                if (valid) {
                    frame.index = index;
                    frame.wallNs = (int64_t)wall;
                    frame.workNs = (int64_t)work;
                    frame.waitNs = (int64_t)wait;
                    frame.gpuSpanNs = (int64_t)gpuSpan;
                    frame.gpuResolved = gpuResolved != 0;
                    frame.nodes.reserve((size_t)nodeCount);
                    for (uint64_t i = 0; i < nodeCount && valid; i++) {
                        uint64_t parent, nameId, calls, total, self;
                        uint8_t laneFlags;
                        valid = payload.Varint(parent) && payload.Varint(nameId) && payload.Byte(laneFlags) &&
                                payload.Varint(calls) && payload.Varint(total) && payload.Varint(self) && parent <= i;
                        if (!valid) break;
                        Profiler::Node node;
                        node.parent = (int32_t)parent - 1;
                        node.nameId = (nameId < localIds.size() && localIds[(size_t)nameId] != 0) ? localIds[(size_t)nameId] : Profiler::Intern("?");
                        node.lane = (laneFlags & 1) ? Profiler::Lane::GPU : Profiler::Lane::CPU;
                        node.flags = (laneFlags & 2) ? Profiler::FlagWait : Profiler::FlagNone;
                        node.calls = (uint32_t)calls;
                        node.totalNs = (int64_t)total;
                        node.selfNs = (int64_t)self;
                        const uint64_t parentKey = node.parent >= 0 ? frame.nodes[(size_t)node.parent].key : Profiler::kRootKey;
                        node.key = Profiler::ChildKey(parentKey, node.nameId, node.lane);
                        frame.nodes.push_back(node);
                    }
                    if (valid) {
                        uint64_t calls = 0;
                        for (const Profiler::Node& node : frame.nodes) calls += node.calls;
                        frame.totalCalls = calls;
                        outFrames.push_back(std::move(frame));
                        framesRead++;
                        produced++;
                    }
                }
            } else if (type == kRecordMarker) {
                uint64_t frameIndex;
                std::string text;
                valid = payload.Varint(frameIndex) && payload.String(text);
                if (valid) markers.emplace_back(frameIndex, text);
            } else if (type == kRecordEnd) {
                ended = true;
            }

            if (!valid) { corrupt = true; break; }
            cursor.p = record.p + length;
            consumed = cursor.p;
        }

        offset += (uint64_t)(consumed - buffer.data());
        if (corrupt) break;
        if (consumed == buffer.data()) break;
    }

    moreAvailable = !corrupt && size > offset && produced > 0;
    return produced;
}

}
