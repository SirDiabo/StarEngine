#include "NetIO.h"
#include <sstream>

namespace NetIO {

std::string EscapeValue(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string UnescapeValue(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '\\' && i + 1 < in.size()) {
            char next = in[i + 1];
            if (next == 'n') { out += '\n'; ++i; continue; }
            if (next == 'r') { out += '\r'; ++i; continue; }
            if (next == '\\') { out += '\\'; ++i; continue; }
        }
        out += in[i];
    }
    return out;
}

void AppendLine(std::string& out, const std::string& key, const std::string& value) {
    out += key;
    out += '=';
    out += EscapeValue(value);
    out += '\n';
}

std::unordered_map<std::string, std::string> ParseLines(const std::string& text) {
    std::unordered_map<std::string, std::string> result;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t newline = text.find('\n', pos);
        std::string line = (newline == std::string::npos) ? text.substr(pos) : text.substr(pos, newline - pos);
        pos = (newline == std::string::npos) ? text.size() : newline + 1;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = UnescapeValue(line.substr(eq + 1));
        result[key] = value;
    }
    return result;
}

void AppendParams(std::string& out, const std::vector<std::pair<std::string, std::string>>& params) {
    for (const auto& [name, value] : params) {
        AppendLine(out, "param:" + name, value);
    }
}

std::vector<std::pair<std::string, std::string>> ExtractParams(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> result;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t newline = text.find('\n', pos);
        std::string line = (newline == std::string::npos) ? text.substr(pos) : text.substr(pos, newline - pos);
        pos = (newline == std::string::npos) ? text.size() : newline + 1;

        if (line.rfind("param:", 0) != 0) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string name = line.substr(6, eq - 6);
        std::string value = UnescapeValue(line.substr(eq + 1));
        result.emplace_back(name, value);
    }
    return result;
}

}
