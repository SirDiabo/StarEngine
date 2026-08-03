#include "BuildDiagnostics.h"
#include "SafeProcess.h"

namespace BuildDiagnostics {

std::string CheckCMakeAvailable() {
    std::string log;
    int exitCode = SafeProcess::Run({"cmake", "--version"}, log);
    if (exitCode == 0) return "";

#ifdef _WIN32
    return "cmake was not found. Install it from https://cmake.org/download and make sure "
           "it is on your PATH, then try again.";
#else
    return "cmake was not found. Install it with your package manager (for example "
           "\"sudo apt install cmake\" on Debian/Ubuntu, \"sudo dnf install cmake\" on Fedora, "
           "or \"sudo pacman -S cmake\" on Arch), then try again.";
#endif
}

std::string DiagnoseConfigureFailure(const std::string& log) {
    if (log.find("Could NOT find X11") != std::string::npos) {
        return "Missing X11 development headers. Install them with "
               "\"sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev\" "
               "on Debian/Ubuntu, or the equivalent -devel packages on other distros, then try again.";
    }
    if (log.find("Could NOT find OpenGL") != std::string::npos) {
        return "Missing OpenGL development headers. Install them with "
               "\"sudo apt install libgl1-mesa-dev\" on Debian/Ubuntu, or the equivalent "
               "-devel packages on other distros, then try again.";
    }
    if (log.find("No CMAKE_C_COMPILER could be found") != std::string::npos ||
        log.find("No CMAKE_CXX_COMPILER could be found") != std::string::npos) {
        return "No C/C++ compiler was found. Install one with "
               "\"sudo apt install build-essential\" on Debian/Ubuntu, \"sudo dnf groupinstall "
               "'Development Tools'\" on Fedora, or Visual Studio's C++ workload on Windows, "
               "then try again.";
    }
    return "";
}

}
