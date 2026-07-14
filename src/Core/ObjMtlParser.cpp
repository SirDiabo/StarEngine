#include "ObjMtlParser.h"
#include <algorithm>
#include <sstream>

bool ObjMtlParser::IsObjFile(const std::filesystem::path& p) { return p.extension() == ".obj"; }
bool ObjMtlParser::IsMtlFile(const std::filesystem::path& p) { return p.extension() == ".mtl"; }

std::vector<std::string> ObjMtlParser::ParseMtllibReferences(const std::vector<uint8_t>& objBytes) {
    std::vector<std::string> result;
    std::string text(objBytes.begin(), objBytes.end());
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("mtllib", 0) != 0) continue;
        std::istringstream ls(line.substr(6));
        std::string token;
        while (ls >> token) result.push_back(token);
    }
    return result;
}

std::vector<std::string> ObjMtlParser::ParseMtlTextureReferences(const std::vector<uint8_t>& mtlBytes) {
    static const std::vector<std::string> mapKeys = {
        "map_Kd", "map_Ka", "map_Ks", "map_Ns", "map_d", "map_Bump", "bump", "norm", "disp", "decal",
    };

    std::vector<std::string> result;
    std::string text(mtlBytes.begin(), mtlBytes.end());
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        std::string key;
        ls >> key;

        bool isMap = false;
        for (const auto& k : mapKeys) {
            if (key == k) { isMap = true; break; }
        }
        if (!isMap) continue;

        std::string token, last;
        while (ls >> token) last = token;
        if (!last.empty()) result.push_back(last);
    }
    return result;
}

std::vector<ObjMtlParser::MtlMaterial> ObjMtlParser::ParseMtlMaterials(const std::vector<uint8_t>& mtlBytes) {
    static const std::vector<std::string> mapKeys = {
        "map_Kd", "map_Ka", "map_Ks", "map_Ns", "map_d", "map_Bump", "bump", "norm", "disp", "decal",
    };

    std::vector<MtlMaterial> result;
    std::string text(mtlBytes.begin(), mtlBytes.end());
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        std::string key;
        ls >> key;

        if (key == "newmtl") {
            MtlMaterial mat;
            std::getline(ls, mat.name);
            size_t start = mat.name.find_first_not_of(" \t");
            mat.name = (start == std::string::npos) ? "" : mat.name.substr(start);
            result.push_back(mat);
            continue;
        }
        if (result.empty()) continue;
        MtlMaterial& current = result.back();

        if (key == "Kd") {
            float r = 1, g = 1, b = 1;
            ls >> r >> g >> b;
            current.colorR = (unsigned char)(std::clamp(r, 0.0f, 1.0f) * 255.0f);
            current.colorG = (unsigned char)(std::clamp(g, 0.0f, 1.0f) * 255.0f);
            current.colorB = (unsigned char)(std::clamp(b, 0.0f, 1.0f) * 255.0f);
            continue;
        }
        if (key == "d") {
            float a = 1;
            ls >> a;
            current.colorA = (unsigned char)(std::clamp(a, 0.0f, 1.0f) * 255.0f);
            continue;
        }

        if (current.texturePath.empty()) {
            bool isMap = false;
            for (const auto& k : mapKeys) if (key == k) { isMap = true; break; }
            if (isMap) {
                std::string token, last;
                while (ls >> token) last = token;
                if (!last.empty()) current.texturePath = last;
            }
        }
    }
    return result;
}
