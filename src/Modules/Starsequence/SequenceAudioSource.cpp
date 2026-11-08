#include "SequenceAudioSource.h"
#include "SequenceFormat.h"
#include "Core/AudioBusRegistry.h"
#include "Core/AudioEffectRegistry.h"
#include "Core/CookedFile.h"
#include "Core/StarAsset.h"
#include "Runtime/AudioCore.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>

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
    InitControl();
}

SequenceAudioSource::SequenceAudioSource(const Sequence& source, SampleLoader sampleLoader)
    : loader(std::move(sampleLoader)), sequence(source) {
    sequence.Normalize();
    valid = true;
    InitControl();
}

void SequenceAudioSource::InitControl() {
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

void SequenceAudioSource::RebuildEffects() {
    if (!effectNamesReady) {
        effectNamesReady = true;
        sequenceEffects = sequence.effects;
        for (const Instrument& inst : sequence.instruments) instrumentEffects.push_back(inst.effects);
        for (const Layer& layer : sequence.layers) layerEffects.push_back(layer.effects);
    }

    const int layerCount = (int)sequence.layers.size();
    const int instrumentCount = (int)sequence.instruments.size();
    AudioCore& core = AudioCore::Get();
    auto next = std::make_shared<SequenceEffects>();
    next->instrumentChains.resize((size_t)layerCount * (size_t)std::max(1, instrumentCount));
    next->layerChains.resize((size_t)layerCount);

    for (int l = 0; l < layerCount; l++) {
        std::vector<bool> used((size_t)std::max(1, instrumentCount), false);
        for (const Note& n : sequence.layers[(size_t)l].notes) {
            if (n.instrument >= 0 && n.instrument < instrumentCount) used[(size_t)n.instrument] = true;
        }
        for (int i = 0; i < instrumentCount; i++) {
            if (!used[(size_t)i] || instrumentEffects[(size_t)i].empty()) continue;
            const size_t slot = (size_t)l * (size_t)instrumentCount + (size_t)i;
            std::shared_ptr<const AudioEffectChain> reuse = effects && slot < effects->instrumentChains.size() ? effects->instrumentChains[slot] : nullptr;
            next->instrumentChains[slot] = core.BuildEffectChain(instrumentEffects[(size_t)i], reuse);
        }
        std::shared_ptr<const AudioEffectChain> reuse = effects && (size_t)l < effects->layerChains.size() ? effects->layerChains[(size_t)l] : nullptr;
        next->layerChains[(size_t)l] = core.BuildEffectChain(layerEffects[(size_t)l], reuse);
    }
    next->sequenceChain = core.BuildEffectChain(sequenceEffects, effects ? effects->sequenceChain : nullptr);
    next->Prepare(layerCount, instrumentCount);

    if (effects) {
        for (const auto& chain : effects->instrumentChains) core.RetireEffectChain(chain);
        for (const auto& chain : effects->layerChains) core.RetireEffectChain(chain);
        core.RetireEffectChain(effects->sequenceChain);
    }
    effects = next;
    if (renderer) renderer->SetEffects(effects);
}

bool SequenceAudioSource::HandleEffectOp(const std::string& path, const AudioParamValue& value) {
    if (!valid || path.rfind("Effects/", 0) != 0) return false;
    if (!effectNamesReady) RebuildEffects();

    const std::string scope = path.substr(8);
    std::vector<std::string>* names = nullptr;
    int layerIndex = -1, instrumentIndex = -1;
    if (scope == "Sequence") {
        names = &sequenceEffects;
    } else if (scope.rfind("Layer/", 0) == 0) {
        const std::string name = scope.substr(6);
        for (size_t i = 0; i < sequence.layers.size(); i++) if (sequence.layers[i].name == name) layerIndex = (int)i;
        if (layerIndex >= 0) names = &layerEffects[(size_t)layerIndex];
    } else if (scope.rfind("Instrument/", 0) == 0) {
        const std::string name = scope.substr(11);
        for (size_t i = 0; i < sequence.instruments.size(); i++) if (sequence.instruments[i].name == name) instrumentIndex = (int)i;
        if (instrumentIndex >= 0) names = &instrumentEffects[(size_t)instrumentIndex];
    }
    if (!names) return false;

    const size_t colon = value.text.find(':');
    const std::string op = value.text.substr(0, colon);
    const std::string rest = colon == std::string::npos ? std::string() : value.text.substr(colon + 1);

    if (op == "push") {
        if (!AudioEffectRegistry::Get().Find(rest)) return false;
        names->push_back(rest);
    } else if (op == "pop") {
        if (names->empty()) return false;
        if (rest.empty()) {
            names->pop_back();
        } else {
            auto found = std::find(names->rbegin(), names->rend(), rest);
            if (found == names->rend()) return false;
            names->erase(std::next(found).base());
        }
    } else if (op == "clear") {
        names->clear();
    } else if (op == "param") {
        const size_t split = rest.find(':');
        if (split == std::string::npos || !effects) return false;
        const std::string effectName = rest.substr(0, split);
        const std::string paramId = rest.substr(split + 1);
        bool applied = false;
        if (scope == "Sequence") {
            applied = effects->sequenceChain && AudioCore::SetChainEffectParameter(*effects->sequenceChain, effectName, paramId, value.number);
        } else if (layerIndex >= 0) {
            const auto& chain = effects->layerChains[(size_t)layerIndex];
            applied = chain && AudioCore::SetChainEffectParameter(*chain, effectName, paramId, value.number);
        } else {
            const int instrumentCount = (int)sequence.instruments.size();
            for (size_t l = 0; l < sequence.layers.size(); l++) {
                const size_t slot = l * (size_t)instrumentCount + (size_t)instrumentIndex;
                if (slot < effects->instrumentChains.size() && effects->instrumentChains[slot]) {
                    applied = AudioCore::SetChainEffectParameter(*effects->instrumentChains[slot], effectName, paramId, value.number) || applied;
                }
            }
        }
        return applied;
    } else {
        return false;
    }
    RebuildEffects();
    return true;
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
    renderer->gain.store(volume * sequence.volume * spatialGain);
    renderer->bus.store(bus);
    RebuildEffects();
    control->pan.store(std::clamp(userPan + spatialPan, -1.0f, 1.0f));
    AudioMixer::Get().AddRenderer(renderer, AudioEffectChain::Merge(effectChain, spatialChain));
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
    if (renderer) renderer->gain.store(volume * sequence.volume * spatialGain);
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
    userPan = std::clamp(value, -1.0f, 1.0f);
    if (control) control->pan.store(std::clamp(userPan + spatialPan, -1.0f, 1.0f));
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

double SequenceAudioSource::GetPositionTick() const {
    return control ? control->positionTick.load() : 0.0;
}

double SequenceAudioSource::GetDuration() const {
    return valid ? sequence.DurationSeconds() : 0.0;
}

void SequenceAudioSource::GetParameters(std::vector<AudioParamInfo>& out) const {
    if (valid) DescribeSequenceParameters(sequence, out);
}

void SequenceAudioSource::SetEffectChain(const std::shared_ptr<const AudioEffectChain>& chain) {
    effectChain = chain;
    if (renderer) AudioMixer::Get().SetRendererChain(renderer, AudioEffectChain::Merge(effectChain, spatialChain));
}

void SequenceAudioSource::SetSpatial(float gain, float panValue) {
    spatialGain = std::max(0.0f, gain);
    spatialPan = std::clamp(panValue, -1.0f, 1.0f);
    if (renderer) renderer->gain.store(volume * sequence.volume * spatialGain);
    if (control) control->pan.store(std::clamp(userPan + spatialPan, -1.0f, 1.0f));
}

void SequenceAudioSource::SetSpatialChain(const std::shared_ptr<const AudioEffectChain>& chain) {
    spatialChain = chain;
    if (renderer) AudioMixer::Get().SetRendererChain(renderer, AudioEffectChain::Merge(effectChain, spatialChain));
}

bool SequenceAudioSource::GetParameter(const std::string& path, AudioParamValue& out) const {
    if (path == "Playback/CurrentMarker") {
        out = AudioParamValue();
        int index = control ? control->currentMarker.load(std::memory_order_relaxed) : -1;
        if (index >= 0 && (size_t)index < sequence.markers.size()) out.text = sequence.markers[(size_t)index].name;
        return true;
    }
    return false;
}

bool SequenceAudioSource::SetParameter(const std::string& path, const AudioParamValue& value) {
    if (path.rfind("Effects/", 0) == 0) return HandleEffectOp(path, value);
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
