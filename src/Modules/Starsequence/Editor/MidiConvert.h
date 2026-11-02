#pragma once
#include "MidiFile.h"
#include "Modules/Starsequence/SequenceData.h"
#include <string>
#include <vector>

namespace Starsequence {
namespace Midi {

enum class LayerMode { TrackChannel, Track, Single };
enum class InstrumentMode { ChannelProgram, Channel, Single };

struct ConvertOptions {
    LayerMode layerMode = LayerMode::TrackChannel;
    InstrumentMode instrumentMode = InstrumentMode::ChannelProgram;
    bool importTempo = true;
    float fixedBpm = 120.0f;
    bool importMeter = true;
    bool importMarkers = true;
    bool importVolume = true;
    bool importExpression = true;
    bool importPan = true;
    bool importSustain = true;
    bool importBend = true;
    bool importEffects = true;
    bool convertTo480 = false;
    int quantizeDenominator = 0;
};

struct ConvertMessage {
    bool error = false;
    std::string text;
};

struct ConvertStats {
    int notes = 0;
    int layers = 0;
    int instruments = 0;
    int controllers = 0;
};

const char* GeneralMidiName(int program);

bool ConvertToSequence(const File& file, const ConvertOptions& options, const std::string& name,
                       Sequence& out, std::vector<ConvertMessage>& messages, ConvertStats& stats);

}
}
