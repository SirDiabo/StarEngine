#include "EnvVar.h"
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

std::string GetEnvironmentValue(const char* name) {
#ifdef _WIN32
    DWORD needed = GetEnvironmentVariableA(name, nullptr, 0);
    if (needed == 0) return {};
    std::string value(needed, '\0');
    DWORD written = GetEnvironmentVariableA(name, &value[0], needed);
    value.resize(written);
    return value;
#else
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
#endif
}
