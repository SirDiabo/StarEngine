#pragma once
#include <string>

namespace AssetImporter {

std::string Cook(const std::string& srcPath, const std::string& assetType, const std::string& extension,
                  const std::string& targetFolder);

std::string Reimport(const std::string& starAssetPath);

}