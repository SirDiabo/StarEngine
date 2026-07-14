#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ObjMtlParser {

bool IsObjFile(const std::filesystem::path& p);
bool IsMtlFile(const std::filesystem::path& p);

std::vector<std::string> ParseMtllibReferences(const std::vector<uint8_t>& objBytes);

std::vector<std::string> ParseMtlTextureReferences(const std::vector<uint8_t>& mtlBytes);

struct MtlMaterial {
    std::string name;
    unsigned char colorR = 255, colorG = 255, colorB = 255, colorA = 255;
    std::string texturePath;
};

std::vector<MtlMaterial> ParseMtlMaterials(const std::vector<uint8_t>& mtlBytes);

}
