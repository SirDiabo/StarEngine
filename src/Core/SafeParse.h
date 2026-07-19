#pragma once
#include <charconv>
#include <string>

namespace SafeParse {

inline int ToInt(const std::string& s, int defaultValue = 0) {
    if (s.empty()) return defaultValue;
    int value = defaultValue;
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    if (result.ec != std::errc() || result.ptr != s.data() + s.size()) return defaultValue;
    return value;
}

inline float ToFloat(const std::string& s, float defaultValue = 0.0f) {
    if (s.empty()) return defaultValue;
    // from_chars does not handle hex/inf/nan; plain decimal is what we expect.
    float value = defaultValue;
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    if (result.ec != std::errc() || result.ptr != s.data() + s.size()) return defaultValue;
    return value;
}

} // namespace
