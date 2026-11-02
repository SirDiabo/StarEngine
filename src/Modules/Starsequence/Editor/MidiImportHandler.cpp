#include "MidiImportHandler.h"
#include "MidiConvert.h"
#include "MidiFile.h"
#include "Modules/Starsequence/SequenceFormat.h"
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace Starsequence {

namespace {

constexpr uintmax_t kMaxMidiFileSize = 64ull * 1024ull * 1024ull;

ImportOptionDef MakeEnum(const std::string& id, const std::string& label, const std::string& group,
                         std::vector<std::string> choices, const std::string& defaultValue) {
    ImportOptionDef def;
    def.id = id;
    def.label = label;
    def.group = group;
    def.kind = ImportOptionKind::Enum;
    def.choices = std::move(choices);
    def.defaultValue = defaultValue;
    return def;
}

ImportOptionDef MakeBool(const std::string& id, const std::string& label, const std::string& group, bool defaultValue) {
    ImportOptionDef def;
    def.id = id;
    def.label = label;
    def.group = group;
    def.kind = ImportOptionKind::Bool;
    def.defaultValue = defaultValue ? "true" : "false";
    return def;
}

Midi::ConvertOptions ReadOptions(const ImportContext& context) {
    Midi::ConvertOptions o;
    std::string layers = context.GetString("layers", "Track + Channel");
    o.layerMode = layers == "Track" ? Midi::LayerMode::Track : (layers == "Single Layer" ? Midi::LayerMode::Single : Midi::LayerMode::TrackChannel);
    std::string instruments = context.GetString("instruments", "Channel + Program");
    o.instrumentMode = instruments == "Channel" ? Midi::InstrumentMode::Channel : (instruments == "Single Instrument" ? Midi::InstrumentMode::Single : Midi::InstrumentMode::ChannelProgram);
    o.importTempo = context.GetBool("importTempo", true);
    o.fixedBpm = context.GetFloat("fixedBpm", 120.0f);
    o.importMeter = context.GetBool("importMeter", true);
    o.importMarkers = context.GetBool("importMarkers", true);
    o.importVolume = context.GetBool("importVolume", true);
    o.importExpression = context.GetBool("importExpression", true);
    o.importPan = context.GetBool("importPan", true);
    o.importSustain = context.GetBool("importSustain", true);
    o.importBend = context.GetBool("importBend", true);
    o.importEffects = context.GetBool("importEffects", true);
    o.convertTo480 = context.GetString("resolution", "Keep File Resolution") == "Convert to 480";
    std::string quantize = context.GetString("quantize", "Off");
    o.quantizeDenominator = quantize == "1/4" ? 4 : quantize == "1/8" ? 8 : quantize == "1/16" ? 16 : quantize == "1/32" ? 32 : quantize == "1/64" ? 64 : 0;
    return o;
}

}

std::vector<ImportOptionDef> MidiImportOptions() {
    std::vector<ImportOptionDef> options;
    options.push_back(MakeEnum("layers", "Layers", "Structure", { "Track + Channel", "Track", "Single Layer" }, "Track + Channel"));
    options.push_back(MakeEnum("instruments", "Instruments", "Structure", { "Channel + Program", "Channel", "Single Instrument" }, "Channel + Program"));

    options.push_back(MakeBool("importTempo", "Import Tempo", "Tempo and Meter", true));
    ImportOptionDef bpm;
    bpm.id = "fixedBpm";
    bpm.label = "Fixed BPM";
    bpm.group = "Tempo and Meter";
    bpm.kind = ImportOptionKind::Float;
    bpm.defaultValue = "120";
    bpm.minValue = 1.0f;
    bpm.maxValue = 999.0f;
    bpm.showIfId = "importTempo";
    bpm.showIfValue = "false";
    options.push_back(bpm);
    options.push_back(MakeBool("importMeter", "Import Time Signatures", "Tempo and Meter", true));
    options.push_back(MakeBool("importMarkers", "Import Markers", "Tempo and Meter", true));

    options.push_back(MakeBool("importVolume", "Volume (CC7)", "Controllers", true));
    options.push_back(MakeBool("importExpression", "Expression (CC11)", "Controllers", true));
    options.push_back(MakeBool("importPan", "Pan (CC10)", "Controllers", true));
    options.push_back(MakeBool("importSustain", "Sustain Pedal (CC64)", "Controllers", true));
    options.push_back(MakeBool("importBend", "Pitch Bend", "Controllers", true));
    options.push_back(MakeBool("importEffects", "Reverb and Chorus Sends", "Controllers", true));

    options.push_back(MakeEnum("resolution", "Resolution", "Timing", { "Keep File Resolution", "Convert to 480" }, "Keep File Resolution"));
    options.push_back(MakeEnum("quantize", "Quantize Note Starts", "Timing", { "Off", "1/4", "1/8", "1/16", "1/32", "1/64" }, "Off"));
    return options;
}

ImportResult RunMidiImport(const ImportContext& context) {
    ImportResult result;
    result.summary = "Import failed";

    std::error_code ec;
    uintmax_t size = fs::file_size(context.sourcePath, ec);
    if (ec) {
        result.Error("Could not read the file size of " + context.sourcePath);
        return result;
    }
    if (size > kMaxMidiFileSize) {
        result.Error("The MIDI file is larger than 64 MB and was not imported");
        return result;
    }

    std::ifstream in(context.sourcePath, std::ios::binary);
    if (!in) {
        result.Error("Could not open " + context.sourcePath);
        return result;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    Midi::File file;
    std::vector<Midi::Diagnostic> diagnostics;
    bool parsed = Midi::ParseMidi(bytes, file, diagnostics);
    for (const Midi::Diagnostic& d : diagnostics) {
        if (d.error) result.Error(d.text);
        else result.Warn(d.text);
    }
    if (!parsed) return result;

    Sequence sequence;
    std::vector<Midi::ConvertMessage> messages;
    Midi::ConvertStats stats;
    bool converted = Midi::ConvertToSequence(file, ReadOptions(context), context.baseName, sequence, messages, stats);
    for (const Midi::ConvertMessage& m : messages) {
        if (m.error) result.Error(m.text);
        else result.Warn(m.text);
    }
    if (!converted) return result;

    std::string path = ImportHandlerRegistry::UniquePath(context.targetFolder, context.baseName, ".starseq");
    std::string error;
    if (!SaveSequenceFile(path, sequence, error)) {
        result.Error("Could not write " + path + ": " + error);
        return result;
    }

    result.success = true;
    result.createdAssets.push_back(path);
    result.summary = "Imported " + fs::path(path).stem().string() + ": " + std::to_string(stats.notes) + " notes, " +
        std::to_string(stats.layers) + (stats.layers == 1 ? " layer, " : " layers, ") +
        std::to_string(stats.instruments) + (stats.instruments == 1 ? " instrument" : " instruments") + " (assign samples to hear it)";
    return result;
}

void RegisterMidiImportHandlers() {
    ImportHandler handler;
    handler.extension = ".mid";
    handler.moduleName = "Starsequence";
    handler.label = "MIDI";
    handler.options = MidiImportOptions();
    handler.import = RunMidiImport;
    ImportHandlerRegistry::Get().Register(handler);
    handler.extension = ".midi";
    ImportHandlerRegistry::Get().Register(handler);
}

}
