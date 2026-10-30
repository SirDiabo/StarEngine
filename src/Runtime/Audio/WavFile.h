#pragma once
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

struct SampleData {
    std::vector<float> frames;
    int channels = 1;
    int sampleRate = 44100;
    uint64_t frameCount = 0;
    bool hasLoop = false;
    uint32_t loopStart = 0;
    uint32_t loopEnd = 0;
    int rootNote = -1;
};

struct WavInfo {
    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    int formatTag = 0;
    uint64_t dataOffset = 0;
    uint64_t dataBytes = 0;
    uint64_t frameCount = 0;
    bool hasLoop = false;
    uint32_t loopStart = 0;
    uint32_t loopEnd = 0;
    int rootNote = -1;

    double DurationSeconds() const { return sampleRate > 0 ? (double)frameCount / (double)sampleRate : 0.0; }
};

namespace WavFile {

bool ReadInfo(const std::string& path, WavInfo& info, std::string& error);
uint64_t ReadFrames(std::ifstream& file, const WavInfo& info, uint64_t startFrame, uint64_t count, float* out, int outChannels);
std::shared_ptr<SampleData> LoadAll(const std::string& path, std::string& error);

}
