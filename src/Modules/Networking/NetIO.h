#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

namespace NetIO {

std::string EscapeValue(const std::string& in);
std::string UnescapeValue(const std::string& in);

void AppendLine(std::string& out, const std::string& key, const std::string& value);

std::unordered_map<std::string, std::string> ParseLines(const std::string& text);

void AppendParams(std::string& out, const std::vector<std::pair<std::string, std::string>>& params);
std::vector<std::pair<std::string, std::string>> ExtractParams(const std::string& text);

}