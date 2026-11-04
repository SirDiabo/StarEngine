#pragma once
#include <string>
#include <vector>

struct AudioEffectParamDef {
    std::string id;
    std::string label;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    bool integer = false;
    std::vector<std::string> choices;
    std::string unit;
};

struct AudioEffectTypeInfo {
    std::string type;
    std::string displayName;
    std::vector<AudioEffectParamDef> params;
};

namespace ReverbParam { enum { Mix, RoomSize, Damping, PreDelay, Width, Count }; }
namespace ChorusParam { enum { Mix, Rate, Depth, Delay, Feedback, Count }; }
namespace DelayParam { enum { Mix, Time, Feedback, Tone, PingPong, Count }; }
namespace FilterParam { enum { Mix, Mode, Cutoff, Resonance, Count }; }
namespace EqParam { enum { Mix, LowGain, LowFreq, MidGain, MidFreq, MidQ, HighGain, HighFreq, Count }; }
namespace CompressorParam { enum { Mix, Threshold, Ratio, Attack, Release, Makeup, Knee, Count }; }
namespace DistortionParam { enum { Mix, Drive, Tone, BitDepth, Output, Count }; }

namespace AudioEffectTypes {

constexpr int kMaxParams = 8;

const std::vector<AudioEffectTypeInfo>& All();
const AudioEffectTypeInfo* Find(const std::string& type);
int ParamIndex(const AudioEffectTypeInfo& info, const std::string& paramId);
float ClampParam(const AudioEffectParamDef& def, float value);

}
