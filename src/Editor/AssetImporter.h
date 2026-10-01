#pragma once
#include <string>
#include <vector>

namespace AssetImporter {

struct MeshImportPreview {
    bool valid = false;
    int materialCount = 0;
    std::vector<std::string> materialNames;
    bool hasSkeleton = false;
    unsigned int skeletonBoneCount = 0;
    std::vector<std::string> animationNames;
};

MeshImportPreview InspectMeshSource(const std::string& srcPath);

std::string Cook(const std::string& srcPath, const std::string& assetType, const std::string& extension,
                  const std::string& targetFolder);

std::string CookAnimationsOnly(const std::string& srcPath, const std::string& targetFolder);

std::string Reimport(const std::string& starAssetPath);

std::string ReimportFromFile(const std::string& starAssetPath, const std::string& newSourcePath);

bool NeedsReimportFromFile(const std::string& starAssetPath);

}
