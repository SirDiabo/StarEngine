#include "AssetImporter.h"
#include "Core/Project.h"
#include "Core/AssetRegistry.h"
#include "Core/StarAsset.h"
#include "Core/StarMaterial.h"
#include "Core/SourceVendoring.h"
#include "Core/ObjMtlParser.h"
#include "ImportDetailsPanel.h"
#include "Runtime/AssetCache.h"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <cstdio>

namespace fs = std::filesystem;

namespace {

std::vector<uint8_t> ReadFileBytes(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

fs::path UniqueTargetPath(const fs::path& folder, const std::string& baseName, const std::string& ext) {
    fs::path target = folder / (baseName + ext);
    int suffix = 1;
    while (fs::exists(target)) target = folder / (baseName + "_" + std::to_string(suffix++) + ext);
    return target;
}

StarAsset BuildMeshAsset(const fs::path& absoluteSrc, const std::filesystem::path& folder,
                         const std::filesystem::path& contentRoot,
                         const std::vector<MaterialSlot>& previousSlots) {
    std::error_code ec;
    StarAsset asset;
    asset.assetType = "StaticMesh";
    asset.sourcePath = fs::absolute(absoluteSrc, ec).string();
    asset.vendoredSourcePath = SourceVendoring::Vendor(contentRoot.string(), asset.sourcePath, absoluteSrc.stem().string());
    asset.importScaleX = asset.importScaleY = asset.importScaleZ = 100.0f;

    auto findPreviousSlot = [&](const std::string& usemtlName) -> const MaterialSlot* {
        for (const MaterialSlot& old : previousSlots) {
            if (old.usemtlName == usemtlName) return &old;
        }
        return nullptr;
    };

    if (ObjMtlParser::IsObjFile(absoluteSrc)) {
        std::vector<uint8_t> objBytes = ReadFileBytes(absoluteSrc);
        for (const std::string& mtlName : ObjMtlParser::ParseMtllibReferences(objBytes)) {
            fs::path mtlPath = absoluteSrc.parent_path() / mtlName;
            if (!fs::exists(mtlPath, ec)) continue;

            std::vector<uint8_t> mtlBytes = ReadFileBytes(mtlPath);
            for (const ObjMtlParser::MtlMaterial& mat : ObjMtlParser::ParseMtlMaterials(mtlBytes)) {
                MaterialSlot slot;
                slot.usemtlName = mat.name;

                const MaterialSlot* existing = findPreviousSlot(mat.name);
                if (existing && !existing->materialPath.empty() &&
                    fs::exists(contentRoot / existing->materialPath, ec)) {
                    slot.materialPath = existing->materialPath;
                    asset.materialSlots.push_back(slot);
                    continue;
                }

                StarMaterial starMat;
                starMat.baseColor = Color{mat.colorR, mat.colorG, mat.colorB, mat.colorA};

                if (!mat.texturePath.empty()) {
                    fs::path texPath = mtlPath.parent_path() / mat.texturePath;
                    if (fs::exists(texPath, ec)) {
                        fs::path texTarget = UniqueTargetPath(folder, fs::path(mat.texturePath).stem().string(), ".startex");
                        StarAsset texAsset;
                        texAsset.assetType = "Texture";
                        texAsset.sourcePath = fs::absolute(texPath, ec).string();
                        texAsset.vendoredSourcePath =
                            SourceVendoring::Vendor(contentRoot.string(), texAsset.sourcePath, texTarget.stem().string());
                        if (texAsset.SaveToFile(texTarget.string())) {
                            AssetRegistry::Get().RegisterAsset(texTarget.string(), texAsset.sourcePath, texTarget.string());
                            starMat.textureAssetPath = fs::relative(texTarget, contentRoot, ec).generic_string();
                        }
                    }
                }

                fs::path matTarget = UniqueTargetPath(folder, mat.name.empty() ? fs::path(absoluteSrc).stem().string() : mat.name, ".starmat");
                if (starMat.SaveToFile(matTarget.string())) {
                    slot.materialPath = fs::relative(matTarget, contentRoot, ec).generic_string();
                }

                asset.materialSlots.push_back(slot);
            }
        }
    } else {
        Model rawModel = LoadModel(absoluteSrc.string().c_str());
        if (rawModel.materialCount > 0) {
            for (int i = 0; i < rawModel.materialCount; i++) {
                Material& srcMat = rawModel.materials[i];
                MaterialSlot slot;
                slot.usemtlName = "Material_" + std::to_string(i);

                const MaterialSlot* existing = findPreviousSlot(slot.usemtlName);
                if (existing && !existing->materialPath.empty() &&
                    fs::exists(contentRoot / existing->materialPath, ec)) {
                    slot.materialPath = existing->materialPath;
                    asset.materialSlots.push_back(slot);
                    continue;
                }

                StarMaterial starMat;
                Color mapColor = srcMat.maps[MATERIAL_MAP_ALBEDO].color;
                starMat.baseColor = mapColor;

                Texture2D srcTex = srcMat.maps[MATERIAL_MAP_ALBEDO].texture;
                if (srcTex.id != 0) {
                    Image img = LoadImageFromTexture(srcTex);
                    if (img.data != nullptr) {
                        fs::path pngTarget = UniqueTargetPath(folder, absoluteSrc.stem().string() + "_" + slot.usemtlName, ".png");
                        if (ExportImage(img, pngTarget.string().c_str())) {
                            fs::path texTarget = UniqueTargetPath(folder, pngTarget.stem().string(), ".startex");
                            StarAsset texAsset;
                            texAsset.assetType = "Texture";
                            texAsset.sourcePath = fs::absolute(pngTarget, ec).string();
                            texAsset.vendoredSourcePath =
                                SourceVendoring::Vendor(contentRoot.string(), texAsset.sourcePath, texTarget.stem().string());
                            if (texAsset.SaveToFile(texTarget.string())) {
                                AssetRegistry::Get().RegisterAsset(texTarget.string(), texAsset.sourcePath, texTarget.string());
                                starMat.textureAssetPath = fs::relative(texTarget, contentRoot, ec).generic_string();
                            }
                        }
                        UnloadImage(img);
                    }
                }

                fs::path matTarget = UniqueTargetPath(folder, absoluteSrc.stem().string() + "_" + slot.usemtlName, ".starmat");
                if (starMat.SaveToFile(matTarget.string())) {
                    slot.materialPath = fs::relative(matTarget, contentRoot, ec).generic_string();
                }

                asset.materialSlots.push_back(slot);
            }
        }
        UnloadModel(rawModel);
    }

    return asset;
}

}

namespace AssetImporter {

std::string Cook(const std::string& srcPath, const std::string& assetType, const std::string& extension,
                  const std::string& targetFolder) {
    fs::path src(srcPath);
    std::error_code ec;
    fs::path absoluteSrc = fs::absolute(src, ec);
    if (ec) absoluteSrc = src;

    std::string assetName = absoluteSrc.stem().string();
    fs::path contentRoot = fs::path(Project::Get().GetRootPath()) / "Content";
    fs::path folder = targetFolder.empty() ? contentRoot : fs::path(targetFolder);
    fs::path target = UniqueTargetPath(folder, assetName, extension);

    StarAsset asset;
    if (assetType == "StaticMesh") {
        asset = BuildMeshAsset(absoluteSrc, folder, contentRoot, {});
    } else {
        asset.assetType = assetType;
        asset.sourcePath = absoluteSrc.string();
        asset.vendoredSourcePath = SourceVendoring::Vendor(contentRoot.string(), asset.sourcePath, target.stem().string());
    }

    if (asset.SaveToFile(target.string())) {
        AssetRegistry::Get().RegisterAsset(target.string(), asset.sourcePath, target.string());
        ImportDetailsPanel::Get().Open(target.string(), assetType);
        return target.string();
    }
    return "";
}

std::string Reimport(const std::string& starAssetPath) {
    StarAsset asset;
    if (!StarAsset::LoadFromFile(starAssetPath, asset)) return "";
    if (asset.sourcePath.empty()) return "";

    fs::path contentRoot = fs::path(Project::Get().GetRootPath()) / "Content";
    fs::path folder = fs::path(starAssetPath).parent_path();

    fs::path src(asset.sourcePath);
    std::error_code ec;
    if (!fs::exists(src, ec)) {
        fs::path vendored = contentRoot / asset.vendoredSourcePath;
        if (asset.vendoredSourcePath.empty() || !fs::exists(vendored, ec)) return "";
        src = vendored;
    }

    StarAsset rebuilt;
    if (asset.assetType == "StaticMesh") {
        rebuilt = BuildMeshAsset(fs::absolute(src, ec), folder, contentRoot, asset.materialSlots);
    } else {
        rebuilt.assetType = asset.assetType;
        rebuilt.sourcePath = fs::absolute(src, ec).string();
        rebuilt.vendoredSourcePath =
            SourceVendoring::Vendor(contentRoot.string(), rebuilt.sourcePath, fs::path(starAssetPath).stem().string());
    }

    // Preserve user-edited import transform and any material reassignments by name.
    rebuilt.importPosX = asset.importPosX;
    rebuilt.importPosY = asset.importPosY;
    rebuilt.importPosZ = asset.importPosZ;
    rebuilt.importRotX = asset.importRotX;
    rebuilt.importRotY = asset.importRotY;
    rebuilt.importRotZ = asset.importRotZ;
    rebuilt.importScaleX = asset.importScaleX;
    rebuilt.importScaleY = asset.importScaleY;
    rebuilt.importScaleZ = asset.importScaleZ;

    if (!rebuilt.materialSlots.empty() && !asset.materialSlots.empty()) {
        for (size_t i = 0; i < rebuilt.materialSlots.size(); i++) {
            for (const MaterialSlot& old : asset.materialSlots) {
                if (rebuilt.materialSlots[i].usemtlName == old.usemtlName) {
                    rebuilt.materialSlots[i].materialPath = old.materialPath;
                    break;
                }
            }
        }
    } else {
        rebuilt.materialSlots = asset.materialSlots;
    }

    if (rebuilt.SaveToFile(starAssetPath)) {
        if (rebuilt.assetType == "StaticMesh") {
            AssetCache::Get().ReloadModel(rebuilt.ResolveSourcePath());
        } else if (rebuilt.assetType == "Texture") {
            AssetCache::Get().ReloadTexture(starAssetPath);
        }
        return starAssetPath;
    }
    return "";
}

}