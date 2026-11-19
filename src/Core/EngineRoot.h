#pragma once
#include <string>
#include <vector>

namespace EngineRoot {

bool IsInsideAppBundle(const std::string& exePath);

std::vector<std::string> ExternalPakCandidates(const std::string& exePath);

std::string Resolve();

std::string BuildDirectory();

}
