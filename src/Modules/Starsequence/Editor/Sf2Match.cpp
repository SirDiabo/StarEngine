#include "Sf2Match.h"
#include "MidiConvert.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace Starsequence {
namespace Sf2 {

namespace {

constexpr int kMatchThreshold = 30;

std::set<std::string> Tokens(const std::string& name) {
    std::set<std::string> tokens;
    std::string current;
    for (unsigned char c : name) {
        if (std::isalnum(c)) current.push_back((char)std::tolower(c));
        else if (!current.empty()) { tokens.insert(current); current.clear(); }
    }
    if (!current.empty()) tokens.insert(current);
    return tokens;
}

double Jaccard(const std::set<std::string>& a, const std::set<std::string>& b) {
    if (a.empty() || b.empty()) return 0.0;
    size_t common = 0;
    for (const std::string& t : a) if (b.count(t)) common++;
    size_t all = a.size() + b.size() - common;
    return all == 0 ? 0.0 : (double)common / (double)all;
}

int GeneralMidiProgram(const std::string& normalized) {
    for (int p = 0; p < 128; p++) {
        if (NormalizeName(Midi::GeneralMidiName(p)) == normalized) return p;
    }
    return -1;
}

bool LooksLikeDrums(const std::set<std::string>& tokens) {
    return tokens.count("drum") || tokens.count("drums") || tokens.count("percussion") || tokens.count("kit");
}

std::vector<MatchResult> RankFor(const File& file, const std::string& name) {
    std::vector<MatchResult> ranked;
    const std::string base = NormalizeName(name);
    const std::set<std::string> baseTokens = Tokens(name);
    const int program = GeneralMidiProgram(base);
    const bool drums = program < 0 && LooksLikeDrums(baseTokens);

    for (int i = 0; i < file.PresetCount(); i++) {
        const PresetHeader& preset = file.presets[(size_t)i];
        const std::string presetNorm = NormalizeName(preset.name);
        const bool presetDrums = preset.bank == 128;
        int score = 0;

        if (!presetNorm.empty() && presetNorm == base) {
            score = 100;
        } else {
            if (program >= 0 && preset.program == program && !presetDrums) score = preset.bank == 0 ? 90 : 70;
            if (drums && presetDrums) score = std::max(score, preset.program == 0 ? 95 : 85);
            double j = Jaccard(baseTokens, Tokens(preset.name));
            int overlap = (int)(60.0 * j);
            if (!presetNorm.empty() && !base.empty()) {
                bool contains = presetNorm.find(base) != std::string::npos ||
                                (presetNorm.size() >= 4 && base.find(presetNorm) != std::string::npos);
                if (contains) overlap += 25;
            }
            score = std::max(score, overlap);
            if (presetDrums && !drums) score /= 4;
            if (drums && !presetDrums) score /= 4;
        }
        ranked.push_back({ i, score });
    }
    std::stable_sort(ranked.begin(), ranked.end(), [&](const MatchResult& a, const MatchResult& b) {
        if (a.score != b.score) return a.score > b.score;
        const PresetHeader& pa = file.presets[(size_t)a.preset];
        const PresetHeader& pb = file.presets[(size_t)b.preset];
        if (pa.bank != pb.bank) return pa.bank < pb.bank;
        if (pa.program != pb.program) return pa.program < pb.program;
        return a.preset < b.preset;
    });
    return ranked;
}

}

std::string NormalizeName(const std::string& name) {
    std::string result;
    for (unsigned char c : name) {
        if (std::isalnum(c)) result.push_back((char)std::tolower(c));
    }
    return result;
}

std::string BaseInstrumentName(const std::string& instrumentName) {
    std::string name = instrumentName;
    size_t suffix = name.rfind(" (Ch ");
    if (suffix != std::string::npos && !name.empty() && name.back() == ')') name = name.substr(0, suffix);
    return name;
}

std::vector<MatchResult> RankPresets(const File& file, const std::string& instrumentName) {
    return RankFor(file, BaseInstrumentName(instrumentName));
}

MatchResult MatchInstrument(const File& file, const std::string& instrumentName) {
    std::string base = BaseInstrumentName(instrumentName);
    std::vector<MatchResult> ranked = RankFor(file, base);
    if (ranked.empty() || ranked.front().score < kMatchThreshold) {
        size_t space = base.find_last_of(' ');
        if (space != std::string::npos && space + 1 < base.size()) {
            bool numeric = std::all_of(base.begin() + (std::ptrdiff_t)space + 1, base.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
            if (numeric) {
                std::vector<MatchResult> retry = RankFor(file, base.substr(0, space));
                if (!retry.empty() && (ranked.empty() || retry.front().score > ranked.front().score)) ranked = retry;
            }
        }
    }
    if (ranked.empty() || ranked.front().score < kMatchThreshold) return MatchResult();
    return ranked.front();
}

}
}
