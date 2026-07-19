#pragma once
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace StarPointer {

template <typename T>
T* FromHex(const std::string& value) {
    if (value.empty()) return nullptr;

    const char* begin = value.c_str();
    if (value.size() >= 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        begin += 2;
    }

    // Reject anything that is not a pure hex digit run.
    if (*begin == '\0') return nullptr;
    for (const char* p = begin; *p; ++p) {
        bool hex = (*p >= '0' && *p <= '9') ||
                   (*p >= 'a' && *p <= 'f') ||
                   (*p >= 'A' && *p <= 'F');
        if (!hex) return nullptr;
    }

    errno = 0;
    char* end = nullptr;
    unsigned long long addr = std::strtoull(begin, &end, 16);
    if (errno != 0 || end == begin || *end != '\0') return nullptr;

    return reinterpret_cast<T*>(static_cast<uintptr_t>(addr));
}

} // namespace StarPointer
