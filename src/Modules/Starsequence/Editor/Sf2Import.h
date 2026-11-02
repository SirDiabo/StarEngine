#pragma once
#include "Sf2File.h"
#include "Modules/Starsequence/SequenceData.h"
#include <map>
#include <string>
#include <vector>

namespace Starsequence {
namespace Sf2 {

class SampleWriter {
public:
    virtual ~SampleWriter() = default;
    virtual std::string Write(const std::string& name, const std::vector<int16_t>& pcm, int sampleRate) = 0;
};

struct Message {
    bool error = false;
    std::string text;
};

bool BuildSampleSet(const File& file, int presetIndex, SampleWriter& writer, std::map<int, std::string>& sampleCache,
                    SampleSet& out, std::vector<Message>& messages);

std::vector<uint8_t> EncodeWav16(const std::vector<int16_t>& pcm, int sampleRate);
std::string SanitizeFileName(const std::string& name, const std::string& fallback);

}
}
