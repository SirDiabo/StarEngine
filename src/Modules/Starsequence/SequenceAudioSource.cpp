#include "SequenceAudioSource.h"
#include "SequenceFormat.h"
#include "Core/AudioBusRegistry.h"
#include "Core/CookedFile.h"
#include "Core/StarAsset.h"
#include "Runtime/AudioCore.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace Starsequence {

bool LoadStarsoundSample(const std::string& assetPath, LoadedSample& out) {
    StarAsset asset;
    if (!StarAsset::LoadFromFile(assetPath, asset) || asset.assetType != "Sound") return false;
    std::string resolved = CookedFile::Open(asset.ResolveSourcePath());
    if (resolved.empty() || !std::filesystem::exists(resolved)) return false;
    out.data = AudioCore::Get().GetSampleData(resolved);
    out.assetGain = std::clamp(asset.soundVolume, 0.0f, 4.0f);
    return out.data != nullptr;
}

void DescribeSequenceParameters(const Sequence& s, std::vector<AudioParamInfo>& out) {
    if (!s.markers.empty()) {
        AudioParamInfo p;
        p.path = "Playback/Section";
        p.group = "Playback";
        p.type = AudioParamType::Enum;
        for (const Marker& m : s.markers) p.options.push_back(m.name);
        out.push_back(p);
    }
    {
        AudioParamInfo p;
        p.path = "Playback/Quantize";
        p.group = "Playback";
        p.type = AudioParamType::Enum;
        p.options = { "Immediate", "Beat", "Bar" };
        p.defaultValue = 2.0f;
        out.push_back(p);
    }
    {
        AudioParamInfo p;
        p.path = "Playback/LayerFade";
        p.group = "Playback";
        p.type = AudioParamType::Float;
        p.minValue = 0.0f;
        p.maxValue = 10.0f;
        p.defaultValue = s.layerFade;
        out.push_back(p);
    }
    for (const Layer& layer : s.layers) {
        std::string group = "Layers/" + layer.name;
        AudioParamInfo mute;
        mute.path = group + "/Mute";
        mute.group = group;
        mute.type = AudioParamType::Bool;
        mute.defaultValue = layer.muted ? 1.0f : 0.0f;
        out.push_back(mute);

        AudioParamInfo solo;
        solo.path = group + "/Solo";
        solo.group = group;
        solo.type = AudioParamType::Bool;
        out.push_back(solo);

        AudioParamInfo vol;
        vol.path = group + "/Volume";
        vol.group = group;
        vol.type = AudioParamType::Float;
        vol.minValue = 0.0f;
        vol.maxValue = 2.0f;
        vol.defaultValue = layer.volume;
        out.push_back(vol);

        AudioParamInfo pan;
        pan.path = group + "/Pan";
        pan.group = group;
        pan.type = AudioParamType::Float;
        pan.minValue = -1.0f;
        pan.maxValue = 1.0f;
        pan.defaultValue = layer.pan;
        out.push_back(pan);
    }
    for (const Instrument& inst : s.instruments) {
        std::string group = "Instruments/" + inst.name;
        AudioParamInfo vol;
        vol.path = group + "/Volume";
        vol.group = group;
        vol.type = AudioParamType::Float;
        vol.minValue = 0.0f;
        vol.maxValue = 2.0f;
        vol.defaultValue = 1.0f;
        out.push_back(vol);

        if (inst.sets.size() > 1) {
            AudioParamInfo set;
            set.path = group + "/Set";
            set.group = group;
            set.type = AudioParamType::Enum;
            for (const SampleSet& ss : inst.sets) set.options.push_back(ss.name);
            out.push_back(set);
        }
    }
}

SequenceAudioSource::SequenceAudioSource(const std::string& path, SampleLoader sampleLoader)
    : assetPath(path), loader(std::move(sampleLoader)) {
    ParseResult parsed;
    valid = LoadSequenceFile(path, parsed);
    if (!valid) {
        for (const ParseDiagnostic& d : parsed.diagnostics) {
            if (d.error) TraceLog(LOG_ERROR, "Starsequence: %s line %d: %s", path.c_str(), d.line, d.message.c_str());
        }
        return;
    }
    sequence = parsed.sequence;
    control = std::make_shared<SequenceControl>((int)sequence.layers.size(), (int)sequence.instruments.size());
    control->layerFade.store(sequence.layerFade);
    for (size_t l = 0; l < sequence.layers.size(); l++) {
        control->layerVolume[l].store(sequence.layers[l].volume);
        control->layerPan[l].store(sequence.layers[l].pan);
        control->layerMute[l].store(sequence.layers[l].muted);
    }
}

SequenceAudioSource::~SequenceAudioSource() {
    if (control) control->stopRequested.store(true);
}

bool SequenceAudioSource::Prepare() {
    if (prepared) return compiled != nullptr;
    prepared = true;
    if (!valid) return false;
    CompileReport report;
    std::shared_ptr<RtSequence> rt = CompileSequence(sequence, loader, report);
    for (const std::string& w : report.warnings) TraceLog(LOG_WARNING, "Starsequence: %s: %s", assetPath.c_str(), w.c_str());
    compiled = rt;
    return compiled != nullptr;
}

void SequenceAudioSource::CopyControl(const SequenceControl& from, SequenceControl& to) const {
    to.pitch.store(from.pitch.load());
    to.tempo.store(from.tempo.load());
    to.pan.store(from.pan.load());
    to.layerFade.store(from.layerFade.load());
    to.looping.store(from.looping.load());
    to.quantize.store(from.quantize.load());
    for (int l = 0; l < from.layers; l++) {
        to.layerVolume[(size_t)l].store(from.layerVolume[(size_t)l].load());
        to.layerPan[(size_t)l].store(from.layerPan[(size_t)l].load());
        to.layerMute[(size_t)l].store(from.layerMute[(size_t)l].load());
        to.layerSolo[(size_t)l].store(from.layerSolo[(size_t)l].load());
    }
    for (int i = 0; i < from.instruments; i++) {
        to.instrumentVolume[(size_t)i].store(from.instrumentVolume[(size_t)i].load());
        to.instrumentSet[(size_t)i].store(from.instrumentSet[(size_t)i].load());
    }
}

void SequenceAudioSource::Play() {
    if (!Prepare()) return;
    if (started && control) {
        control->stopRequested.store(true);
        auto fresh = std::make_shared<SequenceControl>((int)sequence.layers.size(), (int)sequence.instruments.size());
        CopyControl(*control, *fresh);
        control = fresh;
    }
    started = true;
    renderer = std::make_shared<SequenceRenderer>(compiled, control);
    renderer->gain.store(volume * sequence.volume);
    renderer->bus.store(bus);
    AudioMixer::Get().AddRenderer(renderer);
}

void SequenceAudioSource::Stop() {
    if (control) control->stopRequested.store(true);
}

void SequenceAudioSource::Pause() {
    if (control) control->paused.store(true);
}

void SequenceAudioSource::Resume() {
    if (control) control->paused.store(false);
}

bool SequenceAudioSource::IsPlaying() const {
    return started && control && !control->finished.load() && !control->stopRequested.load() && !control->paused.load();
}

bool SequenceAudioSource::IsPaused() const {
    return started && control && !control->finished.load() && !control->stopRequested.load() && control->paused.load();
}

void SequenceAudioSource::SetVolume(float value) {
    volume = std::max(0.0f, value);
    if (renderer) renderer->gain.store(volume * sequence.volume);
}

void SequenceAudioSource::SetPitch(float value) {
    if (control) control->pitch.store(std::max(0.0f, value));
}

void SequenceAudioSource::SetLooping(bool value) {
    if (control) control->looping.store(value);
}

void SequenceAudioSource::SetBus(int value) {
    bus = value;
    if (renderer) renderer->bus.store(bus);
}

void SequenceAudioSource::SetPan(float value) {
    if (control) control->pan.store(std::clamp(value, -1.0f, 1.0f));
}

void SequenceAudioSource::SetTempo(float value) {
    if (control) control->tempo.store(std::max(0.0001f, value));
}

std::string SequenceAudioSource::DefaultBusName() const {
    return AudioBusRegistry::Get().EffectiveDefaultBus(sequence.bus);
}

void SequenceAudioSource::Seek(double seconds) {
    if (!control) return;
    control->seekTick.store((int64_t)std::llround(sequence.SecondsToTick(std::max(0.0, seconds))));
}

double SequenceAudioSource::GetPosition() const {
    if (!control) return 0.0;
    return sequence.TickToSeconds(control->positionTick.load());
}

double SequenceAudioSource::GetDuration() const {
    return valid ? sequence.DurationSeconds() : 0.0;
}

void SequenceAudioSource::GetParameters(std::vector<AudioParamInfo>& out) const {
    if (valid) DescribeSequenceParameters(sequence, out);
}

bool SequenceAudioSource::SetParameter(const std::string& path, const AudioParamValue& value) {
    if (!valid || !control) return false;

    auto indexFromValue = [&](const std::vector<std::string>& names) -> int {
        if (!value.text.empty()) {
            for (size_t i = 0; i < names.size(); i++) if (names[i] == value.text) return (int)i;
            return -1;
        }
        int index = (int)std::lround(value.number);
        return index >= 0 && (size_t)index < names.size() ? index : -1;
    };

    if (path == "Playback/Section") {
        std::vector<std::string> names;
        for (const Marker& m : sequence.markers) names.push_back(m.name);
        int index = indexFromValue(names);
        if (index < 0) return false;
        control->jumpMarker.store(index);
        return true;
    }
    if (path == "Playback/Quantize") {
        int index = indexFromValue({ "Immediate", "Beat", "Bar" });
        if (index < 0) return false;
        control->quantize.store(index);
        return true;
    }
    if (path == "Playback/LayerFade") {
        control->layerFade.store(std::clamp(value.number, 0.0f, 10.0f));
        return true;
    }

    auto splitGroup = [&](const std::string& prefix, std::string& name, std::string& leaf) -> bool {
        if (path.rfind(prefix, 0) != 0) return false;
        size_t slash = path.rfind('/');
        if (slash == std::string::npos || slash <= prefix.size() - 1) return false;
        name = path.substr(prefix.size(), slash - prefix.size());
        leaf = path.substr(slash + 1);
        return !name.empty();
    };

    std::string name, leaf;
    if (splitGroup("Layers/", name, leaf)) {
        int layer = sequence.FindLayer(name);
        if (layer < 0) return false;
        if (leaf == "Mute") { control->layerMute[(size_t)layer].store(value.number >= 0.5f); return true; }
        if (leaf == "Solo") { control->layerSolo[(size_t)layer].store(value.number >= 0.5f); return true; }
        if (leaf == "Volume") { control->layerVolume[(size_t)layer].store(std::clamp(value.number, 0.0f, 2.0f)); return true; }
        if (leaf == "Pan") { control->layerPan[(size_t)layer].store(std::clamp(value.number, -1.0f, 1.0f)); return true; }
        return false;
    }
    if (splitGroup("Instruments/", name, leaf)) {
        int inst = sequence.FindInstrument(name);
        if (inst < 0) return false;
        if (leaf == "Volume") { control->instrumentVolume[(size_t)inst].store(std::clamp(value.number, 0.0f, 2.0f)); return true; }
        if (leaf == "Set") {
            std::vector<std::string> names;
            for (const SampleSet& s : sequence.instruments[(size_t)inst].sets) names.push_back(s.name);
            int index = indexFromValue(names);
            if (index < 0) return false;
            control->instrumentSet[(size_t)inst].store(index);
            return true;
        }
    }
    return false;
}

}
