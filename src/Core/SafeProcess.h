#pragma once
#include <string>
#include <vector>
#include <functional>

namespace SafeProcess {

bool IsShellSafe(const std::string& s);

std::vector<std::string> TokenizeCommand(const std::string& cmd);

std::string QuoteArgForWindows(const std::string& arg);
std::string BuildWindowsCommandLine(const std::vector<std::string>& argv);

std::string FindExecutableInPath(const std::string& name);

int Run(const std::vector<std::string>& argv, std::string& log,
        const std::function<void(const std::string&)>& onChunk = nullptr);

int RunCaptureStdout(const std::vector<std::string>& argv, std::string& out);

} // namespace SafeProcess
