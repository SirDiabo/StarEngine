#pragma once
#include "Sf2File.h"
#include <string>
#include <vector>

namespace Starsequence {
namespace Sf2 {

struct MatchResult {
    int preset = -1;
    int score = 0;
};

std::string NormalizeName(const std::string& name);
std::string BaseInstrumentName(const std::string& instrumentName);
MatchResult MatchInstrument(const File& file, const std::string& instrumentName);
std::vector<MatchResult> RankPresets(const File& file, const std::string& instrumentName);

}
}
