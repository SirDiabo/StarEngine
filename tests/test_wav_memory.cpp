#include "TestHelper.h"
#include "TestFiles.h"
#include "Core/MemoryStream.h"
#include "Runtime/Audio/WavFile.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

using test::TempDir;

void Put16(std::vector<uint8_t>& out, uint16_t v) { out.push_back((uint8_t)v); out.push_back((uint8_t)(v >> 8)); }
void Put32(std::vector<uint8_t>& out, uint32_t v) { for (int i = 0; i < 4; i++) out.push_back((uint8_t)(v >> (8 * i))); }
void PutTag(std::vector<uint8_t>& out, const char* tag) { out.insert(out.end(), tag, tag + 4); }

std::vector<uint8_t> MakeWav(int channels, int sampleRate, int frames, bool withLoop) {
    std::vector<uint8_t> data;
    for (int f = 0; f < frames; f++) {
        for (int c = 0; c < channels; c++) Put16(data, (uint16_t)(int16_t)((f * 37 + c * 1000) % 20000 - 10000));
    }
    std::vector<uint8_t> out;
    PutTag(out, "RIFF");
    Put32(out, 0);
    PutTag(out, "WAVE");
    PutTag(out, "fmt ");
    Put32(out, 16);
    Put16(out, 1);
    Put16(out, (uint16_t)channels);
    Put32(out, (uint32_t)sampleRate);
    Put32(out, (uint32_t)(sampleRate * channels * 2));
    Put16(out, (uint16_t)(channels * 2));
    Put16(out, 16);
    if (withLoop) {
        PutTag(out, "smpl");
        Put32(out, 60);
        for (int i = 0; i < 7; i++) Put32(out, 0);
        Put32(out, 1);
        Put32(out, 0);
        Put32(out, 0);
        Put32(out, 0);
        Put32(out, 10);
        Put32(out, (uint32_t)(frames / 2));
        Put32(out, 0);
        Put32(out, 0);
    }
    PutTag(out, "data");
    Put32(out, (uint32_t)data.size());
    out.insert(out.end(), data.begin(), data.end());
    const uint32_t riffSize = (uint32_t)out.size() - 8;
    for (int i = 0; i < 4; i++) out[4 + (size_t)i] = (uint8_t)(riffSize >> (8 * i));
    return out;
}

}

TEST(wav_info_from_memory_matches_the_file_reader) {
    TempDir dir;
    for (int channels : { 1, 2 }) {
        for (bool loop : { false, true }) {
            std::vector<uint8_t> wav = MakeWav(channels, 22050, 1000, loop);
            const std::string path = dir.Str("a.wav");
            REQUIRE(test::WriteFile(path, wav));

            WavInfo fromFile, fromMemory;
            std::string errorA, errorB;
            REQUIRE(WavFile::ReadInfo(path, fromFile, errorA));
            REQUIRE(WavFile::ReadInfoFromMemory(wav.data(), wav.size(), fromMemory, errorB));
            CHECK(fromFile.channels == fromMemory.channels);
            CHECK(fromFile.sampleRate == fromMemory.sampleRate);
            CHECK(fromFile.bitsPerSample == fromMemory.bitsPerSample);
            CHECK(fromFile.frameCount == fromMemory.frameCount);
            CHECK(fromFile.dataOffset == fromMemory.dataOffset);
            CHECK(fromFile.hasLoop == fromMemory.hasLoop);
            CHECK(fromFile.loopStart == fromMemory.loopStart);
            CHECK(fromFile.loopEnd == fromMemory.loopEnd);
            CHECK(fromMemory.hasLoop == loop);
            CHECK(fromMemory.frameCount == 1000);
            CHECK(fromMemory.channels == channels);
        }
    }
}

TEST(wav_samples_from_memory_match_the_file_loader) {
    TempDir dir;
    std::vector<uint8_t> wav = MakeWav(2, 44100, 777, true);
    const std::string path = dir.Str("b.wav");
    REQUIRE(test::WriteFile(path, wav));

    std::string errorA, errorB;
    std::shared_ptr<SampleData> fromFile = WavFile::LoadAll(path, errorA);
    std::shared_ptr<SampleData> fromMemory = WavFile::LoadAllFromMemory(wav.data(), wav.size(), errorB);
    REQUIRE(fromFile != nullptr);
    REQUIRE(fromMemory != nullptr);
    CHECK(fromFile->frameCount == fromMemory->frameCount);
    CHECK(fromFile->sampleRate == fromMemory->sampleRate);
    CHECK(fromFile->hasLoop == fromMemory->hasLoop);
    CHECK(fromFile->loopStart == fromMemory->loopStart);
    CHECK(fromFile->loopEnd == fromMemory->loopEnd);
    CHECK(fromFile->channels == fromMemory->channels);
    REQUIRE(fromFile->frames.size() == fromMemory->frames.size());
    CHECK(!fromFile->frames.empty());
    CHECK(fromFile->frames == fromMemory->frames);
}

TEST(wav_frames_can_be_streamed_from_a_memory_stream) {
    std::vector<uint8_t> wav = MakeWav(1, 8000, 600, false);
    WavInfo info;
    std::string error;
    REQUIRE(WavFile::ReadInfoFromMemory(wav.data(), wav.size(), info, error));

    auto shared = std::make_shared<std::vector<uint8_t>>(wav);
    MemoryStream stream(shared);

    std::vector<float> all(600 * 2, 0.0f);
    const uint64_t gotAll = WavFile::ReadFrames(stream, info, 0, 600, all.data(), 2);
    CHECK(gotAll == 600);

    std::vector<float> slice(100 * 2, 0.0f);
    const uint64_t gotSlice = WavFile::ReadFrames(stream, info, 250, 100, slice.data(), 2);
    CHECK(gotSlice == 100);
    CHECK(std::memcmp(slice.data(), all.data() + 250 * 2, sizeof(float) * 200) == 0);

    std::vector<float> tail(100 * 2, 0.0f);
    const uint64_t gotTail = WavFile::ReadFrames(stream, info, 550, 100, tail.data(), 2);
    CHECK(gotTail == 50);
}

TEST(wav_memory_reader_rejects_garbage_and_truncation) {
    WavInfo info;
    std::string error;
    CHECK(!WavFile::ReadInfoFromMemory(nullptr, 0, info, error));
    std::vector<uint8_t> empty;
    CHECK(!WavFile::ReadInfoFromMemory(empty.data(), 0, info, error));

    test::Rng rng(12);
    for (int round = 0; round < 200; round++) {
        std::vector<uint8_t> noise = rng.Bytes(rng.Below(200));
        WavFile::ReadInfoFromMemory(noise.data(), noise.size(), info, error);
        std::string loadError;
        WavFile::LoadAllFromMemory(noise.data(), noise.size(), loadError);
    }

    std::vector<uint8_t> wav = MakeWav(2, 22050, 300, true);
    for (size_t length = 0; length < wav.size(); length += 3) {
        WavInfo cutInfo;
        std::string cutError;
        const bool ok = WavFile::ReadInfoFromMemory(wav.data(), length, cutInfo, cutError);
        if (ok) CHECK(cutInfo.frameCount <= 300);
        WavFile::LoadAllFromMemory(wav.data(), length, cutError);
    }
}

TEST(memory_stream_supports_seeking_and_reading) {
    auto bytes = std::make_shared<std::vector<uint8_t>>();
    for (int i = 0; i < 100; i++) bytes->push_back((uint8_t)i);
    MemoryStream stream(bytes);

    char buffer[10];
    stream.read(buffer, 10);
    CHECK(stream.gcount() == 10);
    CHECK((uint8_t)buffer[9] == 9);
    CHECK(stream.tellg() == 10);

    stream.seekg(50);
    stream.read(buffer, 5);
    CHECK((uint8_t)buffer[0] == 50);

    stream.seekg(-4, std::ios::end);
    stream.read(buffer, 10);
    CHECK(stream.gcount() == 4);
    CHECK(stream.eof());

    stream.clear();
    stream.seekg(0);
    stream.seekg(5, std::ios::cur);
    CHECK(stream.tellg() == 5);

    stream.seekg(1000);
    CHECK(stream.fail());
    stream.clear();
    stream.seekg(-1, std::ios::beg);
    CHECK(stream.fail());

    MemoryStream empty(std::make_shared<std::vector<uint8_t>>());
    empty.read(buffer, 1);
    CHECK(empty.gcount() == 0);
    MemoryStream missing(nullptr);
    missing.read(buffer, 1);
    CHECK(missing.gcount() == 0);
}

TEST_MAIN()
