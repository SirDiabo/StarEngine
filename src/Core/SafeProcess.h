#pragma once
#include <string>
#include <vector>
#include <functional>

namespace SafeProcess {

bool IsShellSafe(const std::string& s);

std::vector<std::string> TokenizeCommand(const std::string& cmd);

int Run(const std::vector<std::string>& argv, std::string& log,
        const std::function<void(const std::string&)>& onChunk = nullptr);

} // namespace SafeProcess
