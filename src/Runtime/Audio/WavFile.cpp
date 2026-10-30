#include "WavFile.h"
#include <algorithm>
#include <cstring>

namespace {

uint32_t ReadU32(const unsigned char* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t ReadU16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

bool IsSupportedFormat(const WavInfo& info) {
    if (info.channels < 1 || info.sampleRate <= 0) return false;
    if (info.formatTag == 1) return info.bitsPerSample == 8 || info.bitsPerSample == 16 || info.bitsPerSample == 24 || info.bitsPerSample == 32;
    if (info.formatTag == 3) return info.bitsPerSample == 32 || info.bitsPerSample == 64;
    return false;
}

float DecodeSample(const unsigned char* p, const WavInfo& info) {
    if (info.formatTag == 3) {
        if (info.bitsPerSample == 32) {
            float v;
            std::memcpy(&v, p, 4);
            return v;
        }
        double v;
        std::memcpy(&v, p, 8);
        return (float)v;
    }
    switch (info.bitsPerSample) {
        case 8: return ((int)p[0] - 128) / 128.0f;
        case 16: return (int16_t)ReadU16(p) / 32768.0f;
        case 24: {
            int32_t v = (int32_t)(((uint32_t)p[0] << 8) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 24));
            return (float)(v >> 8) / 8388608.0f;
        }
        default: return (int32_t)ReadU32(p) / 2147483648.0f;
    }
}

}

namespace WavFile {

bool ReadInfo(const std::string& path, WavInfo& info, std::string& error) {
    info = WavInfo();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) { error = "could not open file"; return false; }

    unsigned char header[12];
    file.read((char*)header, 12);
    if (file.gcount() != 12 || std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) {
        error = "not a RIFF/WAVE file";
        return false;
    }

    bool haveFmt = false;
    bool haveData = false;
    while (file) {
        unsigned char chunk[8];
        file.read((char*)chunk, 8);
        if (file.gcount() != 8) break;
        uint32_t size = ReadU32(chunk + 4);
        std::streampos bodyStart = file.tellg();

        if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16) {
            std::vector<unsigned char> body(std::min<uint32_t>(size, 64));
            file.read((char*)body.data(), (std::streamsize)body.size());
            info.formatTag = ReadU16(body.data());
            info.channels = ReadU16(body.data() + 2);
            info.sampleRate = (int)ReadU32(body.data() + 4);
            info.bitsPerSample = ReadU16(body.data() + 14);
            if (info.formatTag == 0xFFFE && body.size() >= 26) info.formatTag = ReadU16(body.data() + 24);
            haveFmt = true;
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            info.dataOffset = (uint64_t)bodyStart;
            info.dataBytes = size;
            haveData = true;
        } else if (std::memcmp(chunk, "smpl", 4) == 0 && size >= 36) {
            std::vector<unsigned char> body(std::min<uint32_t>(size, 128));
            file.read((char*)body.data(), (std::streamsize)body.size());
            uint32_t note = ReadU32(body.data() + 12);
            if (note <= 127) info.rootNote = (int)note;
            uint32_t loopCount = ReadU32(body.data() + 28);
            if (loopCount > 0 && body.size() >= 60) {
                info.hasLoop = true;
                info.loopStart = ReadU32(body.data() + 44);
                info.loopEnd = ReadU32(body.data() + 48);
            }
        }

        file.clear();
        file.seekg(bodyStart);
        uint64_t skip = (uint64_t)size + (size & 1);
        file.seekg((std::streamoff)skip, std::ios::cur);
        if (haveFmt && haveData && !file) break;
    }

    if (!haveFmt || !haveData) { error = "missing fmt or data chunk"; return false; }
    if (!IsSupportedFormat(info)) {
        error = "unsupported WAV encoding (supported: 8/16/24/32-bit PCM and 32/64-bit float)";
        return false;
    }

    uint64_t bytesPerFrame = (uint64_t)info.channels * (uint64_t)(info.bitsPerSample / 8);
    info.frameCount = bytesPerFrame ? info.dataBytes / bytesPerFrame : 0;

    std::ifstream sizeProbe(path, std::ios::binary | std::ios::ate);
    uint64_t fileSize = sizeProbe ? (uint64_t)sizeProbe.tellg() : 0;
    if (fileSize > 0 && info.dataOffset + info.dataBytes > fileSize) {
        uint64_t available = fileSize > info.dataOffset ? fileSize - info.dataOffset : 0;
        info.dataBytes = available;
        info.frameCount = bytesPerFrame ? available / bytesPerFrame : 0;
    }
    if (info.hasLoop && (info.loopEnd <= info.loopStart || info.loopEnd >= info.frameCount)) info.hasLoop = false;
    return info.frameCount > 0;
}

uint64_t ReadFrames(std::ifstream& file, const WavInfo& info, uint64_t startFrame, uint64_t count, float* out, int outChannels) {
    if (startFrame >= info.frameCount || count == 0) return 0;
    count = std::min<uint64_t>(count, info.frameCount - startFrame);

    int bytesPerSample = info.bitsPerSample / 8;
    uint64_t bytesPerFrame = (uint64_t)info.channels * bytesPerSample;
    std::vector<unsigned char> raw((size_t)(count * bytesPerFrame));
    file.clear();
    file.seekg((std::streamoff)(info.dataOffset + startFrame * bytesPerFrame));
    file.read((char*)raw.data(), (std::streamsize)raw.size());
    uint64_t framesRead = (uint64_t)file.gcount() / bytesPerFrame;

    for (uint64_t f = 0; f < framesRead; f++) {
        const unsigned char* frame = raw.data() + f * bytesPerFrame;
        for (int c = 0; c < outChannels; c++) {
            int srcChannel = std::min(c, info.channels - 1);
            out[f * outChannels + c] = DecodeSample(frame + srcChannel * bytesPerSample, info);
        }
    }
    return framesRead;
}

std::shared_ptr<SampleData> LoadAll(const std::string& path, std::string& error) {
    WavInfo info;
    if (!ReadInfo(path, info, error)) return nullptr;

    auto data = std::make_shared<SampleData>();
    data->channels = info.channels >= 2 ? 2 : 1;
    data->sampleRate = info.sampleRate;
    data->hasLoop = info.hasLoop;
    data->loopStart = info.loopStart;
    data->loopEnd = info.loopEnd;
    data->rootNote = info.rootNote;
    data->frames.resize((size_t)(info.frameCount * data->channels));

    std::ifstream file(path, std::ios::binary);
    uint64_t read = 0;
    const uint64_t chunk = 65536;
    while (read < info.frameCount) {
        uint64_t got = ReadFrames(file, info, read, chunk, data->frames.data() + read * data->channels, data->channels);
        if (got == 0) break;
        read += got;
    }
    data->frameCount = read;
    data->frames.resize((size_t)(read * data->channels));
    if (data->hasLoop && data->loopEnd >= data->frameCount) data->hasLoop = false;
    return data;
}

}
