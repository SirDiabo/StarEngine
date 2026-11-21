#pragma once
#include "CookedModel.h"
#include "raylib.h"
#include <string>
#include <vector>

namespace CookedModelRaylib {

bool CookSourceFile(const std::string& diskPath, std::vector<uint8_t>& out, std::string& error);
bool BuildModel(const CookedModel::Data& data, Model& out, std::string& error);
ModelAnimation* BuildAnimations(const CookedModel::Data& data, int& count);

}
