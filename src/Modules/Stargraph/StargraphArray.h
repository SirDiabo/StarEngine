#pragma once
#include "Stargraph.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace StarArray {

inline std::string Encode(const std::vector<std::string>& items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) out += ",";
        out += '"';
        for (char c : items[i]) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:   out += c; break;
            }
        }
        out += '"';
    }
    out += "]";
    return out;
}

inline std::vector<std::string> Decode(const std::string& text) {
    std::vector<std::string> items;
    const size_t n = text.size();
    size_t i = 0;

    auto skipSpace = [&]() {
        while (i < n && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r')) i++;
    };

    skipSpace();
    if (i >= n || text[i] != '[') return items;
    i++;

    while (true) {
        skipSpace();
        if (i >= n) return {};
        if (text[i] == ']') return items;
        if (text[i] == ',') { i++; continue; }
        if (text[i] != '"') return {};
        i++;

        std::string item;
        while (i < n && text[i] != '"') {
            if (text[i] == '\\' && i + 1 < n) {
                char escaped = text[++i];
                if (escaped == 'n') item += '\n';
                else if (escaped == 'r') item += '\r';
                else if (escaped == 't') item += '\t';
                else item += escaped;
            } else {
                item += text[i];
            }
            i++;
        }
        if (i >= n) return {};
        i++;
        items.push_back(std::move(item));
    }
}

inline std::string DefaultElementText(PinType type) {
    switch (type) {
        case PinType::Boolean: return "false";
        case PinType::Integer: return "0";
        case PinType::Float:   return "0.00";
        case PinType::Vec3:    return "0.00,0.00,0.00";
        case PinType::Rotator: return "0.00,0.00,0.00";
        default:               return "";
    }
}

inline bool ElementsEqual(PinType type, const std::string& a, const std::string& b) {
    switch (type) {
        case PinType::Float:
            return std::fabs(std::atof(a.c_str()) - std::atof(b.c_str())) < 0.0001;
        case PinType::Integer:
            return std::atoi(a.c_str()) == std::atoi(b.c_str());
        case PinType::Boolean: {
            bool av = (a == "true" || a == "1");
            bool bv = (b == "true" || b == "1");
            return av == bv;
        }
        case PinType::Vec3:
        case PinType::Rotator: {
            float ax = 0, ay = 0, az = 0, bx = 0, by = 0, bz = 0;
            std::sscanf(a.c_str(), "%f,%f,%f", &ax, &ay, &az);
            std::sscanf(b.c_str(), "%f,%f,%f", &bx, &by, &bz);
            return std::fabs(ax - bx) < 0.0001f && std::fabs(ay - by) < 0.0001f && std::fabs(az - bz) < 0.0001f;
        }
        default:
            return a == b;
    }
}

inline int IndexOf(PinType type, const std::vector<std::string>& items, const std::string& item) {
    for (size_t i = 0; i < items.size(); i++) {
        if (ElementsEqual(type, items[i], item)) return (int)i;
    }
    return -1;
}

}
