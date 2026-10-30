#include "SoundAudioSource.h"
#include "Runtime/AudioCore.h"
#include "Core/StarAsset.h"
#include "Core/CookedFile.h"
#include "raylib.h"
#include <algorithm>
#include <filesystem>

const char* SoundLoadModeName(SoundLoadMode mode) {
    switch (mode) {
        case SoundLoadMode::Memory: return "Memory";
        case SoundLoadMode::Stream: return "Stream";
        default: return "Auto";
    }
}

SoundLoadMode ParseSoundLoadMode(const std::string& text) {
    if (text == "Memory") return SoundLoadMode::Memory;
    if (text == "Stream") return SoundLoadMode::Stream;
    return SoundLoadMode::Auto;
}

SoundAudioSource::SoundAudioSource(const std::string& path) : assetPath(path) {}

SoundAudioSource::~SoundAudioSource() {
    if (voice) voice->stopRequested.store(true);
}

bool SoundAudioSource::Prepare() {
    if (prepared) return true;
    if (failed) return false;

    StarAsset asset;
    if (assetPath.empty() || !StarAsset::LoadFromFile(assetPath, asset) || asset.assetType != "Sound") {
        failed = true;
        return false;
    }

    resolvedPath = CookedFile::Open(asset.ResolveSourcePath());
    if (resolvedPath.empty() || !std::filesystem::exists(resolvedPath)) {
        TraceLog(LOG_ERROR, "Audio: sound file for '%s' was not found", assetPath.c_str());
        failed = true;
        return false;
    }

    std::string error;
    if (!WavFile::ReadInfo(resolvedPath, info, error)) {
        TraceLog(LOG_ERROR, "Audio: could not read '%s': %s", assetPath.c_str(), error.c_str());
        failed = true;
        return false;
    }

    SoundLoadMode mode = ParseSoundLoadMode(asset.soundLoadMode);
    streaming = mode == SoundLoadMode::Stream || (mode == SoundLoadMode::Auto && info.DurationSeconds() > kAutoStreamSeconds);

    if (!streaming) {
        sample = AudioCore::Get().GetSampleData(resolvedPath);
        if (!sample) {
            failed = true;
            return false;
        }
    }

    prepared = true;
    return true;
}

void SoundAudioSource::Play() {
    if (!Prepare()) return;
    if (voice) voice->stopRequested.store(true);

    VoiceStart start;
    start.volume = volume;
    start.pitch = pitch;
    start.pan = pan;
    start.looping = looping;
    start.bus = bus;

    if (streaming) {
        auto feed = std::make_shared<StreamFeed>(resolvedPath, info);
        voice = AudioMixer::Get().PlayStream(feed, start);
    } else {
        voice = AudioMixer::Get().PlayMemory(sample, start);
    }
}

void SoundAudioSource::Stop() {
    if (voice) voice->stopRequested.store(true);
}

void SoundAudioSource::Pause() {
    if (voice) voice->paused.store(true);
}

void SoundAudioSource::Resume() {
    if (voice) voice->paused.store(false);
}

bool SoundAudioSource::IsPlaying() const {
    return voice && !voice->finished.load() && !voice->stopRequested.load() && !voice->paused.load();
}

bool SoundAudioSource::IsPaused() const {
    return voice && !voice->finished.load() && !voice->stopRequested.load() && voice->paused.load();
}

void SoundAudioSource::SetVolume(float value) {
    volume = std::max(0.0f, value);
    if (voice) voice->volume.store(volume);
}

void SoundAudioSource::SetPitch(float value) {
    pitch = std::max(0.0f, value);
    if (voice) voice->pitch.store(pitch);
}

void SoundAudioSource::SetLooping(bool value) {
    looping = value;
    if (voice) voice->looping.store(looping);
}

void SoundAudioSource::SetBus(int value) {
    bus = value;
    if (voice) voice->bus.store(bus);
}

void SoundAudioSource::SetPan(float value) {
    pan = std::clamp(value, -1.0f, 1.0f);
    if (voice) voice->pan.store(pan);
}

void SoundAudioSource::Seek(double seconds) {
    if (!voice || !prepared) return;
    double clamped = std::max(0.0, seconds);
    voice->seekFrame.store((int64_t)(clamped * (double)info.sampleRate));
}

double SoundAudioSource::GetPosition() const {
    if (!voice || !prepared || info.sampleRate <= 0) return 0.0;
    return (double)voice->positionFrames.load() / (double)info.sampleRate;
}

double SoundAudioSource::GetDuration() const {
    return prepared ? info.DurationSeconds() : 0.0;
}

namespace {

AudioSourceRegistrar starsoundRegistrar({
    ".starsound",
    "Sound",
    "",
    [](const std::string& assetPath) -> std::unique_ptr<AudioSource> { return std::make_unique<SoundAudioSource>(assetPath); },
    nullptr
});

}
