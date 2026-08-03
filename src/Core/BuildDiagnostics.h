#pragma once
#include <string>

namespace BuildDiagnostics {

std::string CheckCMakeAvailable();

std::string DiagnoseConfigureFailure(const std::string& log);

}
