#pragma once
#include <string>
#include <unordered_map>

class LayoutSerializer {
public:
    static bool Save(const std::string& filePath,
                     const std::unordered_map<std::string, std::string>& data);

    static bool Load(const std::string& filePath,
                     std::unordered_map<std::string, std::string>& data);
};